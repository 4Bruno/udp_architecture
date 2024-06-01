/*
 *
 * Raw udp client/server serves the purpose of basic example of how to
 * communicate between without any way of knowing if the package was received
 * The purpose of this code is to handle basic ack communication.
 * In order to avoid tcp implementation (sending ack packages), we need to do:
 *  - keep track of packages send, no need to keep historical data
 *    * bit array
 *    * say at 32 pck / sec
 *    * 32 pck/sec * 60 sec/min * 1 min = 1920 pck / min, rounds up to 2048 
 *    * In 64 bit system, thats 32 size_t array
 *    * Every sizeof(size_t) pcks move head block as in a ring buffer and
 *    * replace contents of old head.
 *  - wait for server pck ack bit seq
 *
 */
#include "network_udp.h"
#include "logger.h"
#include <string.h>
#include "atomic.h"
#include "protocol.h"
#include "console_sequences.cpp"
#include "math.h"

#pragma comment( lib, "Winmm.lib" )
#define QUAD_TO_MS(Q) Q.QuadPart * (1.0f / 1000.0f)

#define SOCKET_RETURN_ON_ERROR(fcall) if ((fcall) == SOCKET_ERROR)\
                              {\
                                  logn("Socket error: \n" #fcall "\n%s", GetLastSocketErrorMessage());\
                                  return 1;\
                              }

static volatile int keep_alive = 1;

struct package_info
{
    real_time time_sent;
    real_time time_received;
    u32 id;
    i32 ack;
};


inline i32
IsSeqGreaterThan(u32 a, u32 b)
{
    const u32 half = UINT_MAX / 2;

    i32 result = 
        ((a > b) && ((a - b) <= half)) ||
        ((a < b) && ((b - a) >  half));

    return result;

}

inline i32
IsSeqGreaterThan(u16 a, u16 b)
{
    const u32 half = USHRT_MAX / 2;

    i32 result = 
        ((a > b) && ((a - b) <= half)) ||
        ((a < b) && ((b - a) >  half));

    return result;
}


#if _WIN32
BOOL WINAPI 
HandleCtlC(DWORD dwCtrlType)
{
    BOOL signal_handled = false;

    if (dwCtrlType == CTRL_C_EVENT ||
        dwCtrlType == CTRL_BREAK_EVENT)
    {
        AtomicLockAndExchange(&keep_alive, 0);
        signal_handled = true;
    }
    else if (dwCtrlType == CTRL_CLOSE_EVENT)
    {
        signal_handled = true;
    }

    return signal_handled;
}

#endif

void
InitializeTerminateSignalHandler()
{
#ifdef _WIN32
    SetConsoleCtrlHandler(HandleCtlC, true);
#else
#endif
}

void 
printBits(size_t const size, void const * const ptr)
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    size_t i, j;
    
    for (i = size-1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            byte = (b[i] >> j) & 1;
            printf("%u", byte);
        }
    }
    puts("");
}

u32
IPToU32(const char * ip_s)
{
    u32 ip_addr = 0;
    u32 lshift = 24;
    u32 range_ip = 0;
    u32 inv_max_ip = (~(u32)255);
    const char * err_msg = "Error parsing ip address. Expected format (nnn.xxx.yyy.zzz). Values between 0-255";

    //u32 len = (u32)strlen(ip_s);
    for (char * c = (char *)ip_s; *c != 0; ++c)
    {
        range_ip = strtoul(c, &c, 10); // will move to next '.' or \0

        if ( (range_ip & inv_max_ip) > 0)
        {
            logn("%s", err_msg);
            return 0;
        }

        if ( *c != '.' && *c != 0)
        {
            logn("%s", err_msg);
            return 0;
        }

        ip_addr = ip_addr | (range_ip << lshift);
        lshift -= 8;

        if (*c == 0) break;
    }

    return ip_addr;
}

u32
GetEnvIPAddr()
{
    // expected env var aws_ip with format x.x.x.x
    u32 ip_addr = 0;
    char ip_s[255];
    size_t len = 0;


    if (getenv_s(&len, &ip_s[0], sizeof(ip_s),"aws_ip") == 0)
    {
        if (len == 0)
        {
            logn("Missing aws_ip env var, using localhost");
            ip_addr = IP_ADDR( 127, 0 , 0 , 1);
        }
        else
        {
            ip_addr = IPToU32(ip_s);
        }
    }
    else
    {
        logn("Error on _dupenv_s() using locahost instead");
        ip_addr = IP_ADDR( 127, 0 , 0 , 1);
    }

    return ip_addr;
}

inline package_type
GetMessageType(message * msg)
{
    package_type result = (package_type)((int)msg->header.message_type & ~(1 << 7));

    return result;
}

i32
IsCriticalMessage(message * msg)
{
    i32 is_critical = 
        (msg->header.message_type & (1 << 7)) == (1 << 7);

    return is_critical;
}

b32
IsAckMessage(const u32 * packet_seq_bit, queue_message * queue, i32 index)
{
    i32 pck_bit_index = queue->msg_sent_in_package_bit_index[index];
    u32 bit_mask = ((u32)1 << pck_bit_index);

    i32 is_ack_msg = (*packet_seq_bit & bit_mask) == bit_mask;

    return is_ack_msg;
}

struct message *
GetNextAvailableMessageInQueue(struct queue_message * queue, const u32 * packet_seq_bit)
{
    struct message * msg = 0;
    b32 MessageIsAck = 0;

    i32 index = (queue->next++ & (ArrayCount(queue->messages) - 1));

    // this may happen if all packages are critical and none is ack
    Assert(queue->next != queue->begin);

    msg = queue->messages + index;
    MessageIsAck = IsAckMessage(packet_seq_bit, queue, index);

    if (IsCriticalMessage(msg) && !MessageIsAck)
    {
        struct message * first_ack_msg = 0;
        u32 index = queue->next;
        do
        {
            first_ack_msg = queue->messages + index;
            if (IsAckMessage(packet_seq_bit, queue, index))
            {
                memcpy(first_ack_msg, msg, sizeof(struct message));
                msg->header.message_type = 0;
                break;
            }
            index = (index + 1) & (ArrayCount(queue->messages) - 1);
        } while (index != queue->begin);

        Assert(index != queue->begin);
    }

    Assert(!IsCriticalMessage(msg) || MessageIsAck);

    return msg;
}

void
CreatePackages(const u32 * packet_seq_bit, struct queue_message * queue, enum package_type packet_type, const void * data, u32 size, b32 is_critical)
{
    i32 order = 0;
    i32 id = (queue->last_id++ & ((1 << 8) - 1));
    u8 critical_mask = ((u8)(is_critical ? 1 : 0) << 7);

    // create a header package so peer knows needs to create buffer
    if (size > ArrayCount(queue->messages[0].data))
    {
        struct message * msg = GetNextAvailableMessageInQueue(queue, packet_seq_bit);
        msg->header.id = id;
        msg->header.order = order;
        msg->header.message_type = package_type_buffer | critical_mask;
        msg->header.len = sizeof(struct udp_buffer);

        udp_buffer buffer_msg = { size };
        memcpy(msg->data, &buffer_msg, sizeof(udp_buffer) );

        order += 1;
    }

    while (size > 0)
    {
        u32 used_size = min(size, sizeof(queue->messages[0].data));

        struct message * msg = GetNextAvailableMessageInQueue(queue, packet_seq_bit);

        msg->header.id = id;
        msg->header.order = order;
        msg->header.message_type = packet_type | critical_mask;
        msg->header.len = used_size;

        memcpy(msg->data, data, used_size);

        size -= used_size;
        order += 1;
    }
}


struct console con;

void
ConsolePrintstatus(const char * format, ...)
{
    va_list list;
    va_start(list, format);

    char out[255];
    vsprintf_s(out, ArrayCount(out), format, list);

    ConsoleClearLine(con.size.Y);
    printf("%.*s", con.size.X - 1 , out);

    va_end(list);

}


void
ConsoleUpdateMetrics(r32 time_frame_elapsed, r32 avg_package_roundtrip)
{
    i32 metrics_line = 3;
    ConsoleSaveCursor();
    MoveCursorAbs(metrics_line, 1);
    ConsoleClearLine(metrics_line);
    printf("fps: %3.1f, roundtrip: %3.1f",1000.0f / time_frame_elapsed,avg_package_roundtrip);
    ConsoleRestoreCursor();
}

void
ConsoleClientStatus(const char * s)
{
    const char * msg = "connection status: ";
    const int max_len = con.size.X;
    const int remaining_space = max_len - 19 /* size msg */;
    int start_col = 1;
    ConsoleSaveCursor();
    MoveCursorAbs(1, start_col);
    ConsoleClearLine(1);
    printf("%s%*.*s",msg,remaining_space,remaining_space,s);
    ConsoleRestoreCursor();
}

struct format_ip
{
    char ip[12 + 3 + 5 + 3 + 1];
};

format_ip
FormatIP(u32 addr, u32 port)
{ 
    struct format_ip format_ip;
    sprintf_s(format_ip.ip, "[%3i.%3i.%3i.%3i|%5.5i]", 
            (addr >> 24), (addr >> 16)  & 0xFF, (addr >> 8)   & 0xFF, (addr >> 0)   & 0xFF,
            port);

    return format_ip;
}

int
main(int argc, char * argv[])
{
    memory_arena Arena;
    Arena.max_size = Megabytes(16);
    Arena.base = malloc(Arena.max_size);
    Arena.size = 0;

    InitializeTerminateSignalHandler();

    console con = CreateConsole(&Arena);
    con.margin_top = 5;
    con.margin_bottom = 1;
    con.current_line = con.margin_top + 1;

    if (!con.vt_enabled)
    {
        printf("Terminal can't handle ANSI escaped commands (virtual terminal). Press any key to close this prompt.");
        char c = GetChar();
        while (c == EOF)
        {
            c = GetChar();
            STALL(50);
        }
        return -1;
    }

    if (!InitializeSockets())
    {
        logn("Error initializing sockets library. %s", GetLastSocketErrorMessage());
    }

    socket_handle handle = 0;
    int port = 30000;

#if 1
    u32 ip_addr = GetEnvIPAddr();

    if (argc > 1)
    {
        ip_addr = IPToU32(argv[1]);
    }

#else
    if (_putenv_s("aws_ip","3.71.25.249") != 0)
    {
        logn("Couldn't set env variable for ip addr");
    }
    u32 ip_addr = GetEnvIPAddr();
#endif

    int start_col = 1;
    int max_len = con.size.X - 11;
    MoveCursorAbs(2, start_col);
    printf("%s %*.*s", "Server IP:", max_len ,max_len, FormatIP(ip_addr, port).ip);

#if 0
    logn("Attemp connection to client [%i.%i.%i.%i|%i]",
            (ip_addr >> 24),
            (ip_addr >> 16)  & 0xFF,
            (ip_addr >> 8)   & 0xFF,
            (ip_addr >> 0)   & 0xFF,
            port);
#endif

    ConsoleClientStatus("Disconnected");

    // server port
    sockaddr_in server_addr = CreateSocketAddress( ip_addr, port);
    SOCKET_RETURN_ON_ERROR(CreateSocketUdp(&handle));
    BOOL opt_val = true;
    int opt_len = sizeof(opt_val);
    setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt_val, opt_len);
    SOCKET_RETURN_ON_ERROR(BindSocket(handle, 0));
    SOCKET_RETURN_ON_ERROR(SetSocketNonBlocking(handle));

#if 1
    u32 packet_seq = UINT_MAX;
    u32 packet_seq_bit = ~0; // signal pcks as received, corner case initialization
    u32 remote_seq = UINT_MAX;
    u32 remote_seq_bit = ~0;
    real_time packet_seq_realtime[32];
    delta_time packet_seq_deltatime[32];
    for (u32 i = 0; i < ArrayCount(packet_seq_realtime); ++i)
    {
        ZeroTime(packet_seq_realtime[i]);
        packet_seq_deltatime[i] = 0.0f;
    }
    u32 packet_seq_critical = 0;
#else
    u32 packet_seq = UINT_MAX - 640;
    u32 packet_seq_bit = ~0; // signal pcks as received, corner case initialization
    u32 remote_seq = UINT_MAX - 350;
    u32 remote_seq_bit = ~0;
#endif

    real_time perf_freq = GetClockResolution();

#define PACKAGES_PER_SECOND 32
#if 0
    u32 packages_per_second = PACKAGES_PER_SECOND;
#else
    u32 packages_per_second = 2;
#endif
    r32 expected_ms_per_package = (1.0f / (r32)packages_per_second) * 1000.0f;
    // http://www.geisswerks.com/ryan/FAQS/timing.html
    // Sleep will do granular scheduling up to 1ms
    timeBeginPeriod(1);

    ConsolePrintstatus("Start sending msg to server (rate speed %i packages per second)",packages_per_second);

    client_status my_status_with_server = client_status_none;

    queue_message queue_msg_to_send;
    memset(&queue_msg_to_send, 0, sizeof(queue_message));
    for (u32 i  = 0; i < ArrayCount(queue_msg_to_send.msg_sent_in_package_bit_index); ++i)
    {
        queue_msg_to_send.msg_sent_in_package_bit_index[i] = 32;
    }

    while ( keep_alive )
    {
        real_time starting_time;

        starting_time = GetRealTime();

        char c = GetChar();

        if (c == 'q')
        {
            keep_alive = false;
        }

        u32 packet_index_to_check = (packet_seq + 1) & (32 - 1);
        u32 packet_bit_index_to_check = (1 << packet_index_to_check);
        i32 is_packet_ack = (packet_seq_bit & packet_bit_index_to_check) == packet_bit_index_to_check;
        i32 is_packet_critical = (packet_seq_critical & packet_bit_index_to_check) == packet_bit_index_to_check;

        if (!is_packet_ack)
        {
            //ConsoleAddMessage("Package was lost! %u (critical?%s)", (packet_seq - 31), is_packet_critical ? "True" : "False");
            if (is_packet_critical)
            {
                queue_message * queue = &queue_msg_to_send;
                struct message * msg = queue->messages + queue->next;
                u32 packet_index = queue->msg_sent_in_package_bit_index[queue->next];

                Assert(BetweenIn(packet_index,0,32));

                if (
                        (packet_index == packet_index_to_check)
                        &&
                        IsCriticalMessage(msg)
                    )
                {
                    // msg at next index needs to be re-sent. simply adv pointer
                    queue->next = ( (++queue->next) & (ArrayCount(queue->messages) - 1));
                }

                int next_index = (queue->next == queue->begin) ? queue->next + 1 : queue->next;
                next_index &= (ArrayCount(queue->messages) - 1);

                // corner case begin == next
                for (u32 i = next_index; 
                         i != queue->begin; 
                         i = ( (++i) & (ArrayCount(queue->messages) - 1)))
                {
                    struct message * msg = queue->messages + i;
                    u32 packet_index = queue->msg_sent_in_package_bit_index[i];
                    if (
                            (packet_index == packet_index_to_check)
                            &&
                            IsCriticalMessage(msg)
                        )
                    {
                        // we need to send it again. swap if necessary and incr next
                        if (queue->next != i)
                        {
                            struct message * next_msg = (queue->messages + queue->next);
                            memcpy(next_msg,msg, sizeof(struct message));
                            memset(msg, 0, sizeof(struct message));
                        }
                        queue->next = ( (++queue->next) & (ArrayCount(queue->messages) - 1));
                    }
                }

            }
        }


        {
            struct packet recv_datagram;
            u32 max_packet_size = sizeof( struct packet);

            sockaddr_in from;
            socklen_t fromLength = sizeof( from );

            int bytes = recvfrom( handle, 
                    (char *)&recv_datagram, max_packet_size, 
                    0, 
                    (sockaddr*)&from, &fromLength );

            if ( bytes == SOCKET_ERROR )
            {
                if (socket_errno != EWOULDBLOCK)
                {
                    coord new_size = GetTerminalSize();
                    if (new_size.Y != con.size.X || new_size.X != con.size.Y)
                    {
                        ConsoleClear();
                        con.size = new_size;
                        SetScrollMargin(&con, con.margin_top, con.margin_bottom);
                    }
                    //ConsolePrintstatus("Error recvfrom(). %s", GetLastSocketErrorMessage());
                    ConsoleAppendAt(&con,10,40,"Error recvfrom(): %s", GetLastSocketErrorMessage());
                    ConsoleAppendAt(&con,11,40,"%s", GetLastSocketErrorMessage());
                    ConsoleClientStatus("Server error");
#if 0
                    keep_alive = false;
#else
                    // do nothing
#endif
                }
            }
            else if ( bytes == 0 )
            {
                logn("No more data. Closing.");
            }
            else
            {

                ConsoleClientStatus("Connected");

#if 0
                unsigned int from_address = 
                    ntohl( from.sin_addr.s_addr );

                unsigned int from_port = 
                    ntohs( from.sin_port );
#endif

                u32 recv_packet_seq     = recv_datagram.header.seq;
                u32 recv_packet_ack     = recv_datagram.header.ack;
                u32 recv_packet_ack_bit = recv_datagram.header.ack_bit;

                Assert( (packet_seq == recv_packet_ack) || IsSeqGreaterThan(packet_seq, recv_packet_ack));

                struct message * messages[8];
                i32 msg_count = recv_datagram.header.messages;
                // datagram with 492b max payload can have at most
                // 7.6 messages given that each msg has 32b header + 32b data
                Assert(recv_datagram.header.messages <= sizeof(messages));

                i32 inc_package_has_any_critical_msg = 0;
                u32 begin_data_offset = 0;
                for (i32 msg_index = 0;
                        msg_index < msg_count;
                        ++msg_index)
                {
                    struct message ** msg = messages + msg_index;
                    *msg = (struct message *)recv_datagram.data + begin_data_offset;
                    inc_package_has_any_critical_msg = 
                        inc_package_has_any_critical_msg || IsCriticalMessage(*msg);

                    // strip critical flag
                    (*msg)->header.message_type = GetMessageType(*msg);

                    begin_data_offset += (sizeof((*msg)->header) + (*msg)->header.len);
                }

#if 0
                if (inc_package_has_any_critical_msg)
                {
                    ConsoleAddMessage("Critical msg from server");
                }
#endif

                // debug lose all critical
                inc_package_has_any_critical_msg = 0;
                if (!inc_package_has_any_critical_msg)
                {
                    {
                        /* SYNC INCOMING PACKAGE SEQ WITH OUR RECORDS */
                        // unless crafted package, recv package should always be higher than our record
                        Assert(IsSeqGreaterThan(recv_packet_seq,remote_seq));

                        // TODO: what if we lose all packages for 1 > s?
                        // should we simply reset to 0 all bits and move on
                        // TEST THIS
                        // int on purpose check abs (to look at no branching method)
                        // https://graphics.stanford.edu/~seander/bithacks.html#IntegerAbs
                        i32 delta_local_remote_seq = abs((i32)(recv_packet_seq - remote_seq));
                        Assert(delta_local_remote_seq < 32);

                        u32 bit_mask = 0;
                        u32 remote_bit_index = (recv_packet_seq & 31);

                        if (delta_local_remote_seq < 32)
                        {
                            u32 local_bit_index = (remote_seq & 31);

                            u32 lo = min(remote_bit_index, local_bit_index);
                            u32 hi = max(remote_bit_index, local_bit_index);
                            u32 max_minus_hi = (31 - hi);

                            bit_mask = ((u32)~0 << (lo + max_minus_hi)) >> max_minus_hi;

                            //     hi        low 
                            //      v         v
                            // 00000111111111110000
                            if (remote_bit_index >= local_bit_index)
                            {
                                //     hi        low 
                                //      v         v
                                // 11111000000000001111
                                bit_mask = ~bit_mask; 
                            }

                            bit_mask = bit_mask ^ ((u32)1 << lo);
                        }

                        remote_seq_bit = (remote_seq_bit & bit_mask) | ((u32)1 << remote_bit_index);
                        remote_seq = recv_packet_seq;
                    }

                    /* UPDATE OUR BIT ARRAY OF PACKAGES SENT CONFIRMED BY PEER */

                    u32 delta_seq_and_ack = (packet_seq - recv_packet_ack);
                    u32 bit_mask = 0;

                    // TODO: peer is ack packages 1 s old, should we issue a sync flag?
                    if (delta_seq_and_ack <= 31)
                    {
                        u32 remote_bit_index = (recv_packet_ack & 31);
                        u32 local_bit_index = (packet_seq & 31);

                        u32 lo = min(remote_bit_index, local_bit_index);
                        u32 hi = max(remote_bit_index, local_bit_index);
                        u32 max_minus_hi = (31 - hi);

                        bit_mask = ((u32)~0 << (lo + max_minus_hi)) >> max_minus_hi;

                        //     hi        low 
                        //      v         v
                        // 00000111111111110000
                        if (local_bit_index >= remote_bit_index)
                        {
                            //     hi        low 
                            //      v         v
                            // 11111000000000001111
                            bit_mask = ~bit_mask; 
                        }

                        bit_mask = bit_mask ^ ((u32)1 << lo);
                        packet_seq_deltatime[remote_bit_index] = GetTimeDiff(GetRealTime(), packet_seq_realtime[remote_bit_index], perf_freq);
                    }

                    packet_seq_bit = (recv_packet_ack_bit & bit_mask);
                }
            }
        }

        switch (my_status_with_server)
        {
            case client_status_in_game:
            {
            } break;
            case client_status_auth:
            {
            } break;
            case client_status_trying_auth:
            {
            } break;
            case client_status_none:
            {
                struct udp_auth login_data;
                sprintf_s(login_data.user,"%s","anonymous");
                sprintf_s(login_data.pwd,"%s","1234");

                my_status_with_server = client_status_trying_auth;

                CreatePackages(&packet_seq_bit,&queue_msg_to_send, package_type_auth, (const void *)&login_data, sizeof(udp_auth), true);

            } break;
            default:
            {
            } break;
        }

        r32 aggr_roundtrips = 0.0f;
        i32 count_pkgs_received = 0;
        for (u32 i = 0; i < ArrayCount(packet_seq_deltatime); ++i)
        {
            u32 mask = ((u32)1 << i);
            if ( (packet_seq_bit & mask) ==  mask )
            {
                aggr_roundtrips += packet_seq_deltatime[i];
                count_pkgs_received += 1;
            }
        }
        r32 avg_roundtrips = aggr_roundtrips / (r32)(max(count_pkgs_received, 1));

        // set current seq as not received
        packet_seq += 1;
        u32 new_package_bit_index = (packet_seq & 31);
        packet_seq_bit = (packet_seq_bit & ~((u32)1 << new_package_bit_index));

        struct packet packet;
        packet.header.seq       = packet_seq;
        packet.header.ack       = remote_seq;
        packet.header.ack_bit   = remote_seq_bit;
        packet.header.protocol  = PROTOCOL_ID;
        packet.header.messages  = 0;

        i32 is_critical = 0;
        u32 current_size = 0;
        queue_message * queue = &queue_msg_to_send;
        for (u32 i = queue->begin; 
                 i != queue->next; 
                 i = ( (++i) & (ArrayCount(queue->messages) - 1)))
        {
            struct message * msg = queue->messages + i;
            u32 msg_size = msg ->header.len + sizeof(message_header);
            u32 size_after_msg = (current_size + msg_size);

            if ( size_after_msg <= sizeof(packet.data) )
            {
                memcpy(packet.data + current_size, msg, msg_size);

                is_critical = is_critical | IsCriticalMessage(msg);

                // keep track in which pck was sent
                queue->msg_sent_in_package_bit_index[i] = new_package_bit_index;

                current_size = size_after_msg;
                queue->begin = (++queue->begin) & (ArrayCount(queue->messages) - 1);
                packet.header.messages += 1;
                if (current_size >= sizeof(packet.data))
                {
                    break;
                }
            }
        }

        packet_seq_critical = (packet_seq_critical & (~((u32)1 << new_package_bit_index)));
        packet_seq_critical = packet_seq_critical | ( (is_critical ? 1 : 0) << new_package_bit_index );
        packet_seq_realtime[new_package_bit_index] = GetRealTime();
        if (SendPackage(handle,server_addr, (void *)&packet, sizeof(packet)) == SOCKET_ERROR)
        {
            //logn("Error sending package %i. %s", packet.header.seq , GetLastSocketErrorMessage());
            //keep_alive = 0;
        }
        else
        {
            con.current_line += 1;
            u32 limit = con.buffer_size.Y - (con.margin_top + con.margin_bottom);
            if (con.current_line >= limit)
            {
                con.current_line = con.margin_top + 1;
                for (u32 i = con.margin_top; i <= limit;++i)
                {
                    ConsoleClearLine(i);
                }
            }
            ConsoleAppendAt(&con, con.current_line,0,"Sending package %i.",  packet.header.seq);
        }

        // sleep expected time
        delta_time time_frame_elapsed = 
            GetTimeDiff(GetRealTime(), starting_time, perf_freq);
        r32 remaining_ms = expected_ms_per_package - time_frame_elapsed;

        if (remaining_ms > 1.0f)
        {
            time_frame_elapsed = GetTimeDiff(GetRealTime(), starting_time, perf_freq);
            remaining_ms = expected_ms_per_package - time_frame_elapsed;
            while (remaining_ms > 1.0f)
            {
                Sleep((DWORD)remaining_ms);
                time_frame_elapsed = GetTimeDiff(GetRealTime(), starting_time, perf_freq);
                remaining_ms = expected_ms_per_package - time_frame_elapsed;
            }
        }

        ConsoleSwapBuffer(&con);
        ConsoleUpdateMetrics(time_frame_elapsed,avg_roundtrips);
    }

    DestroyConsole(&con);

    timeEndPeriod(1);

    CloseSocket(handle);

    ShutdownSockets();

    return 0;
}
