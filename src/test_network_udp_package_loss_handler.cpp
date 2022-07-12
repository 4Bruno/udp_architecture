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
#include "test_network.h"
#include "protocol.h"

#pragma comment( lib, "Winmm.lib" )
#define QUAD_TO_MS(Q) Q.QuadPart * (1.0f / 1000.0f)

#define SOCKET_RETURN_ON_ERROR(fcall) if ((fcall) == SOCKET_ERROR)\
                              {\
                                  logn("Socket error: \n" #fcall "\n%s", GetLastSocketErrorMessage());\
                                  return 1;\
                              }

static volatile int keep_alive = 1;

struct client_handler
{
#define BITARRAY_SIZE (sizeof(size_t) * 8)
// ((32 * 60 * 1) / BITARRAY_SIZE) = 1920, round up to 2048 to be power of 2 for & modulo bit op
// 1920 / 64 = 30
// 2048 / 64 = 32
// Must be power of 2
#define SENT_PACKAGES_BIT_BLOCKS 32 
    size_t sent_seq_bit[SENT_PACKAGES_BIT_BLOCKS];
    int head_block;
    u32 seq;
    real_time sent_pck_time[SENT_PACKAGES_BIT_BLOCKS * BITARRAY_SIZE];
};

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


int
HandleRecv(socket_handle handle, u32 * remote_seq_number, u32 * remote_ack_bit, package_queue * received_queue)
{
    int recv_status = -1;

    struct packet packet_data;
    u32 max_packet_size = sizeof( packet_data );

    sockaddr_in from;
    socklen_t fromLength = sizeof( from );

    int bytes = recvfrom( handle, 
            (char *)&packet_data, max_packet_size, 
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

        // process received packet
        logn("[%i.%i.%i.%i] Message (%i) received %s", 
                (from_address >> 24),
                (from_address >> 16)  & 0xFF0000,
                (from_address >> 8)   & 0xFF00,
                (from_address >> 0)   & 0xFF,
                packet_data.seq, packet_data.msg); 

        recv_status = fromLength;

        if (IsSeqGreaterThan(*remote_seq_number,packet_data.ack))
        {
            *remote_seq_number = packet_data.ack;
        }

        for (int i = 0; i < 32;++i)
        {
            i32 mask = (1 << i);
            i32 signaled_received = packet_data.ack_bit & mask;
            if (signaled_received)
            {
            }
        }
    }

    return recv_status;
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
MessageToServer(socket_handle handle,sockaddr_in addr, 
                u32 * packet_seq, 
                u32 * remote_seq_number, u32 * remote_ack_bit,
                package_queue * received_queue)
{
    struct packet packet;
    packet.seq = *packet_seq++;
    packet.ack = *remote_seq_number;
    packet.ack_bit = *remote_ack_bit;

    LONGLONG time_sent = received_queue->packages[received_queue->head].time_sent.QuadPart;
    LONGLONG time_received = received_queue->packages[received_queue->head].time_received.QuadPart;
    LONGLONG delta_time_ms = (time_received - time_sent) * (1.0f / 1000.0f);

    if (!received_queue->packages[received_queue->head].ack)
    {
        logn("Package was not acknowledge by server!");
    }

#if 0
    for (i32 i = 0; i < ArrayCount(packet_received_queue);++i)
    {
        u32 bitmask = ((remote_seq_number in packet_received_queue) << i)
            packet.ack_bit = packet.ack_bit | bitmask;
    }
#endif

    sprintf(packet.msg, "Hello, this is msg number %i", packet.seq); 

    if (SendPackage(handle,addr, (void *)&packet, sizeof(packet)) == SOCKET_ERROR)
    {
        logn("Error sending package %i. %s", packet.seq, GetLastSocketErrorMessage());
        keep_alive = 0;
    }
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

real_time *
GetPckTime(struct client_handler * client, u32 ack)
{
    u32 delta_pck_ack = client->seq - ack - 1;
    i32 offset_block = ((client->head_block + 1) * BITARRAY_SIZE) - delta_pck_ack; 
    real_time * time = client->sent_pck_time + offset_block;
    return time;
}

i32
IsPacketAck(struct client_handler * client, u32 ack)
{
    u32 delta_pck_ack = client->seq - ack - 1;
    i32 offset_block = delta_pck_ack / BITARRAY_SIZE;
    i32 offset_block_index = (client->head_block - offset_block) & (SENT_PACKAGES_BIT_BLOCKS - 1);
    i32 offset_bitarray = (ack & (BITARRAY_SIZE - 1));
    size_t mask = ((size_t)1 << offset_bitarray);
    i32 is_ack = (client->sent_seq_bit[offset_block_index] & mask) == mask;

    return is_ack;
}

int
main()
{
    InitializeTerminateSignalHandler();

    if (!InitializeSockets())
    {
        logn("Error initializing sockets library. %s", GetLastSocketErrorMessage());
    }

    socket_handle handle = 0;
    int port = 30000;
#if 1
    u32 ip_addr = IP_ADDR( 127, 0 , 0 , 1);
#else
    if (_putenv_s("aws_ip","3.70.5.224") != 0)
    {
        logn("Couldn't set env variable for ip addr");
    }
    u32 ip_addr = GetEnvIPAddr();
#endif

    // server port
    sockaddr_in addr = CreateSocketAddress( ip_addr, port);
    SOCKET_RETURN_ON_ERROR(CreateSocketUdp(&handle));
    BOOL opt_val = true;
    int opt_len = sizeof(opt_val);
    setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt_val, opt_len);
    SOCKET_RETURN_ON_ERROR(BindSocket(handle, 0));
    SOCKET_RETURN_ON_ERROR(SetSocketNonBlocking(handle));

    u32 packet_seq = 0;
    u32 remote_seq_number = 0;
    u32 remote_ack_bit = 0;
    i32 remote_ack_head = 0;
    package_queue packages_received_ring = {};
    packages_received_ring.head = 0;
    client_handler client;
    client.head_block = 0;
    client.seq = BITARRAY_SIZE; // tells max seq reference in current head
    for (int i = 0; i < ArrayCount(client.sent_seq_bit);++i)
    {
        client.sent_seq_bit[i] = 0;
    }

    // set initial prior block as acknowledge. This is to avoid corner case
    // at initialization
    client.sent_seq_bit[ArrayCount(client.sent_seq_bit) - 1] = ~(size_t)0;

    for (int i = 0; i < ArrayCount(packages_received_ring.packages);++i)
    {
        packages_received_ring.packages[i].ack = 1;
    }

    real_time perf_freq = GetClockResolution();

    u32 packages_per_second = PACKAGES_PER_SECOND;
    r32 expected_ms_per_package = (1.0f / (r32)packages_per_second) * 1000.0f;
    // http://www.geisswerks.com/ryan/FAQS/timing.html
    // Sleep will do granular scheduling up to 1ms
    timeBeginPeriod(1);

    logn("Start sending msg to server (rate speed %i packages per second)",PACKAGES_PER_SECOND);

    client_status my_status_with_server = client_status_none;

    while ( keep_alive )
    {
        real_time starting_time, ending_time, delta_ms;

        starting_time = GetRealTime();

        //MessageToServer(handle, addr, &packet_seq, &remote_seq_number, &remote_ack_bit, &packages_received_ring);

#if 0
        LONGLONG time_sent = received_queue->packages[received_queue->head].time_sent.QuadPart;
        LONGLONG time_received = received_queue->packages[received_queue->head].time_received.QuadPart;
        LONGLONG delta_time_ms = (time_received - time_sent) * (1.0f / 1000.0f);
        if (!received_queue->packages[received_queue->head].ack)
        {
            logn("Package was not acknowledge by server!");
        }
        for (i32 i = 0; i < ArrayCount(packet_received_queue);++i)
        {
            u32 bitmask = ((remote_seq_number in packet_received_queue) << i)
                packet.ack_bit = packet.ack_bit | bitmask;
        }
#endif
        i32 any_pkc_sent = 0;

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
                struct packet packet;
                packet.seq = packet_seq++;
                packet.ack = remote_seq_number;
                packet.ack_bit = remote_ack_bit;
                packet.protocol = PROTOCOL_ID;

                real_time * time = GetPckTime(&client, 0);
                *time = GetRealTime();

                struct udp_auth login_data;
                sprintf(login_data.user,"%s","anonymous");
                sprintf(login_data.pwd,"%s","1234");
                memcpy(packet.msg, &login_data, sizeof(udp_auth));
                if (SendPackage(handle,addr, (void *)&packet, sizeof(packet)) == SOCKET_ERROR)
                {
                    logn("Error sending package %i. %s", packet.seq, GetLastSocketErrorMessage());
                    keep_alive = 0;
                }
                any_pkc_sent = 1;
                my_status_with_server = client_status_trying_auth;
            } break;
            default:
            {
            } break;
        }

        if (!any_pkc_sent)
        {
            struct packet packet;
            packet.seq = packet_seq++;
            packet.ack = remote_seq_number;
            packet.ack_bit = remote_ack_bit;
            packet.protocol = PROTOCOL_ID;

            real_time * time = GetPckTime(&client, packet.seq);
            *time = GetRealTime();

            sprintf(packet.msg,"keek alive");
            if (SendPackage(handle,addr, (void *)&packet, sizeof(packet)) == SOCKET_ERROR)
            {
                logn("Error sending package %i. %s", packet.seq, GetLastSocketErrorMessage());
                keep_alive = 0;
            }
        }


        if (!IsPacketAck(&client, packet_seq - PACKAGES_PER_SECOND))
        {
            logn("Package was lost! %u", (packet_seq - (u32)PACKAGES_PER_SECOND));
            // TODO:
            // is this a critical package? - issue 
        }

        if ( (packet_seq % BITARRAY_SIZE) == 0)
        {
            i32 current_head = client.head_block;
            u32 current_seq = client.seq;
            client.head_block += 1;
            client.head_block = (client.head_block & (SENT_PACKAGES_BIT_BLOCKS - 1));
            client.seq += BITARRAY_SIZE;

#if 0
            // review last 64 packages if handled
            unsigned long index = 0;
            size_t flip_ack_bitarray = ~client.sent_seq_bit[current_head];
            while (_BitScanForward64(&index, flip_ack_bitarray) == 1)
            {
                logn("package never ack %i", current_seq - (u32)BITARRAY_SIZE + (u32)index);
                flip_ack_bitarray = flip_ack_bitarray & (~((size_t)1 << index));
            }
#endif
        }

        //HandleRecv(handle, &remote_seq_number, &remote_ack_bit);
        int recv_status = -1;

        struct packet packet_data;
        u32 max_packet_size = sizeof( packet_data );

        sockaddr_in from;
        socklen_t fromLength = sizeof( from );

        int bytes = recvfrom( handle, 
                (char *)&packet_data, max_packet_size, 
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

            // process received packet
#if 0
            logn("[%i.%i.%i.%i] Message (%i) received %s", 
                    (from_address >> 24),
                    (from_address >> 16)  & 0xFF0000,
                    (from_address >> 8)   & 0xFF00,
                    (from_address >> 0)   & 0xFF,
                    packet_data.seq, packet_data.msg); 
#endif
            recv_status = fromLength;

            if (IsSeqGreaterThan(remote_seq_number,packet_data.ack))
            {
                remote_seq_number = packet_data.ack;
            }

            u32 delta_pck_ack = client.seq - packet_data.ack - 1;
            i32 offset_block = delta_pck_ack / BITARRAY_SIZE;
            i32 offset_block_index = (client.head_block - offset_block) & (SENT_PACKAGES_BIT_BLOCKS - 1);
            i32 offset_bitarray = (packet_data.ack & (BITARRAY_SIZE - 1));
            client.sent_seq_bit[offset_block_index] = client.sent_seq_bit[offset_block_index] | ((size_t)1 << offset_bitarray);
        }

        // I increase array block asap 
        // there is lag between the last packages so pck 62/63 shows a not received
        // because is already pointing to the next block
        printBits(sizeof(size_t),&client.sent_seq_bit[client.head_block]);

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

    struct packet packet;
    sprintf(packet.msg, "Bye");
    packet.seq = -1;
    if (SendPackage(handle,addr, (void *)&packet, sizeof(packet)) == SOCKET_ERROR)
    {
        logn("Error sending package %i. %s", packet.seq , GetLastSocketErrorMessage());
    }

    CloseSocket(handle);

    ShutdownSockets();

    return 0;
}
