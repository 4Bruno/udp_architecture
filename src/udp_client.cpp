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

struct package_queue
{
#define PACKAGES_PER_SECOND 32 
    int head;
    struct package_info packages[PACKAGES_PER_SECOND];
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
    int i, j;
    
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

    u32 len = strlen(ip_s);
    for (char * c = (char *)ip_s; *c != 0; ++c)
    {
        u32 range_ip = strtoul(c, &c, 10);
        //logn("yields: %u after: %s",range_ip,c);
        //printBits(sizeof(range_ip), &range_ip);
        //printBits(sizeof(range_ip), &inv_max_ip);

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
            ip_addr = IP_ADDR( 127, 0 , 0 , 1);
        }
        else
        {
            char * start_c = (char *)ip_s;
            u32 shift = 24;
            u32 range_ip = 0;
            while ((start_c < (ip_s + len)) && (range_ip = strtoul(start_c, &start_c, 10)))
            {
                ip_addr = ip_addr | (range_ip << shift);
                shift -= 8;
                start_c += 1; // skip .
            }
        }
    }
    else
    {
        logn("Error on _dupenv_s() using locahost instead");
        ip_addr = IP_ADDR( 127, 0 , 0 , 1);
    }

    return ip_addr;
}


void
CreatePackages(struct queue_message * queue, i32 packet_type, void * data, u32 size)
{
    i32 order = 0;
    i32 id = (queue->last_id++ & ((1 << 8) - 1));

    while (size > 0)
    {
        u32 used_size = min(size, sizeof(queue->messages[0].data));

        Assert(
                ((queue->next + 1) & (ArrayCount(queue->messages) - 1)) !=
                (queue->begin & (ArrayCount(queue->messages) - 1))
              );


        struct message * pck = 
            queue->messages + (queue->next++ & (ArrayCount(queue->messages) - 1));

        pck->header.id = id;
        pck->header.order = order;
        pck->header.message_type = packet_type;
        pck->header.len = used_size;

        memcpy(pck->data, data, used_size);

        size -= used_size;
        order += 1;
    }
}

int
main(int argc, char * argv[])
{
    InitializeTerminateSignalHandler();

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
    if (_putenv_s("aws_ip","3.70.5.224") != 0)
    {
        logn("Couldn't set env variable for ip addr");
    }
    u32 ip_addr = GetEnvIPAddr();
#endif

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
#else
    u32 packet_seq = UINT_MAX - 640;
    u32 packet_seq_bit = ~0; // signal pcks as received, corner case initialization
    u32 remote_seq = UINT_MAX - 350;
    u32 remote_seq_bit = ~0;
#endif

    package_queue packages_received_ring = {};
    packages_received_ring.head = 0;

    real_time perf_freq = GetClockResolution();

    u32 packages_per_second = PACKAGES_PER_SECOND;
    r32 expected_ms_per_package = (1.0f / (r32)packages_per_second) * 1000.0f;
    // http://www.geisswerks.com/ryan/FAQS/timing.html
    // Sleep will do granular scheduling up to 1ms
    timeBeginPeriod(1);

    logn("Start sending msg to server (rate speed %i packages per second)",PACKAGES_PER_SECOND);

    client_status my_status_with_server = client_status_none;

    queue_message queue_msg_to_send;
    memset(&queue_msg_to_send, 0, sizeof(queue_message));

    while ( keep_alive )
    {
        real_time starting_time, ending_time, delta_ms;

        starting_time = GetRealTime();

        i32 any_pkc_sent = 0;

        u32 bit_to_check = (1 << ((packet_seq + 1) & (32 - 1)));
        i32 is_packet_ack = (packet_seq_bit & bit_to_check) == bit_to_check;

        if (!is_packet_ack)
        {
            logn("Package was lost! %u", (packet_seq - 31));
            // TODO:
            // is this a critical package? - issue 
        }


        {
            int recv_status = -1;

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
                    logn("Error recvfrom(). %s", GetLastSocketErrorMessage());
                }
            }
            else if ( bytes == 0 )
            {
                logn("No more data. Closing.");
                recv_status = 0;
            }
            else
            {
                unsigned int from_address = 
                    ntohl( from.sin_addr.s_addr );

                unsigned int from_port = 
                    ntohs( from.sin_port );

                u32 recv_packet_seq     = recv_datagram.header.seq;
                u32 recv_packet_ack     = recv_datagram.header.ack;
                u32 recv_packet_ack_bit = recv_datagram.header.ack_bit;

                Assert( (packet_seq == recv_packet_ack) || IsSeqGreaterThan(packet_seq, recv_packet_ack));

                struct message * messages[8];
                u32 msg_count = recv_datagram.header.messages;
                // datagram with 492b max payload can have at most
                // 7.6 messages given that each msg has 32b header + 32b data
                Assert(recv_datagram.header.messages <= sizeof(messages));

                u32 begin_data_offset = 0;
                for (int msg_index = 0;
                        msg_index < msg_count;
                        ++msg_index)
                {
                    struct message ** msg = messages + msg_index;
                    *msg = (struct message *)recv_datagram.data + begin_data_offset;
                    begin_data_offset += (sizeof((*msg)->header) + (*msg)->header.len);
                }
#if 0
                logn("[%i.%i.%i.%i] Message (%i) received %s", 
                        (from_address >> 24),
                        (from_address >> 16)  & 0xFF0000,
                        (from_address >> 8)   & 0xFF00,
                        (from_address >> 0)   & 0xFF,
                        recv_datagram.seq, recv_datagram.msg); 
#endif
                recv_status = fromLength;
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

                    u32 sync_remote_seq_bit = remote_seq_bit;
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
                }

                packet_seq_bit = (recv_packet_ack_bit & bit_mask);
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
                sprintf(login_data.user,"%s","anonymous");
                sprintf(login_data.pwd,"%s","1234");

                my_status_with_server = client_status_trying_auth;

                CreatePackages(&queue_msg_to_send, package_type_auth, (void *)&login_data, sizeof(udp_auth));

            } break;
            default:
            {
            } break;
        }


        // set current seq as not received
        packet_seq += 1;
        packet_seq_bit = (packet_seq_bit & ~(1 << (packet_seq & 31)));

        struct packet packet;
        packet.header.seq       = packet_seq;
        packet.header.ack       = remote_seq;
        packet.header.ack_bit   = remote_seq_bit;
        packet.header.protocol  = PROTOCOL_ID;
        packet.header.messages  = 0;

        u32 current_size = 0;
        queue_message * queue = &queue_msg_to_send;
        for (int i = queue->begin; 
                 i != queue->next; 
                 i = (++i & (ArrayCount(queue->messages) - 1)))
        {
            struct message * msg = queue->messages + i;
            u32 size = msg ->header.len + sizeof(message_header);
            memcpy(packet.data + current_size, msg, size);

            current_size += size;
            queue->begin += 1;
            packet.header.messages += 1;
            if (current_size >= sizeof(packet.data))
            {
                break;
            }
        }

        if (SendPackage(handle,server_addr, (void *)&packet, sizeof(packet)) == SOCKET_ERROR)
        {
            logn("Error sending package %i. %s", packet.header.seq , GetLastSocketErrorMessage());
            keep_alive = 0;
        }

        //Assert(packet.seq != 34);

        // I increase array block asap 
        // there is lag between the last packages so pck 62/63 shows a not received
        // because is already pointing to the next block
        //printf("Sent: %10.10u\t",packet_seq - 1);printBits(sizeof(u32),&packet_seq_bit);
        //printf("Recv: %10.10u\t",remote_seq);printBits(sizeof(u32),&remote_seq_bit);

        // sleep expected time
        delta_time time_frame_elapsed = 
            GetTimeDiff(GetRealTime(), starting_time, perf_freq);
        r32 remaining_ms = expected_ms_per_package - time_frame_elapsed;

        //logn("Time frame remaining %f\n",remaining_ms);

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
    }

    timeEndPeriod(1);

    CloseSocket(handle);

    ShutdownSockets();

    return 0;
}
