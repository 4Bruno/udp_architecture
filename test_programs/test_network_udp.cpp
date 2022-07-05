#include "../src/network_udp.h"
#include "../src/logger.h"
#include <string.h>
#include "../src/atomic.h"
#include "test_network.h"

#pragma comment( lib, "Winmm.lib" )
#define QUAD_TO_MS(Q) Q.QuadPart * (1.0f / 1000.0f)

#define SOCKET_RETURN_ON_ERROR(fcall) if ((fcall) == SOCKET_ERROR)\
                              {\
                                  logn("Socket error: \n" #fcall "\n%s", GetLastSocketErrorMessage());\
                                  return 1;\
                              }

static volatile int keep_alive = 1;

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
HandleRecv(socket_handle handle, u32 * remote_seq_number)
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

struct package_info
{
    SYSTEMTIME system_time;
    u32 id;
};

struct packet_queue
{
#define PACKAGES_PER_SECOND 4
    struct package_info packages[PACKAGES_PER_SECOND];
};

inline LARGE_INTEGER
Win32QueryPerformance()
{
    LARGE_INTEGER Performance;
    QueryPerformanceCounter(&Performance);
    return Performance;
}
/* Returns us microseconds 10^-6 */
inline LARGE_INTEGER
Win32QueryPerformanceDiff(LARGE_INTEGER TimeEnd,LARGE_INTEGER TimeStart,LARGE_INTEGER PerformanceFrequency)
{
    TimeEnd.QuadPart = TimeEnd.QuadPart - TimeStart.QuadPart;
    TimeEnd.QuadPart *= 1000000;
    TimeEnd.QuadPart /= PerformanceFrequency.QuadPart;
    return TimeEnd;
}



void
MessageToServer(socket_handle handle,sockaddr_in addr, u32 * packet_seq, u32 * remote_seq_number)
{
    struct packet packet;
    packet.seq = *packet_seq++;
    packet.ack = *remote_seq_number;

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
#if 0
    u32 ip_addr = IP_ADDR( 127, 0 , 0 , 1);
    u32 ip_addr = IP_ADDR( 89, 187, 72, 238);
#else
    if (_putenv_s("aws_ip","3.70.5.224") != 0)
    {
        logn("Couldn't set env variable for ip addr");
    }
    u32 ip_addr = GetEnvIPAddr();
#endif

    sockaddr_in addr = CreateSocketAddress( ip_addr, port);

    SOCKET_RETURN_ON_ERROR(CreateSocketUdp(&handle));
    SOCKET_RETURN_ON_ERROR(BindSocket(handle, port));
    SOCKET_RETURN_ON_ERROR(SetSocketNonBlocking(handle));

    BOOL opt_val = false;
    int opt_len = sizeof(opt_val);
    setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt_val, opt_len);
    
    u32 packet_seq = 0;
    u32 remote_seq_number = 0;
    u32 packet_received_queue[32] = {}; 

    LARGE_INTEGER perf_freq;
    QueryPerformanceFrequency(&perf_freq); 
    u32 packages_per_second = 2;
    r32 expected_ms_per_package = (1.0f / (r32)packages_per_second) * 1000.0f;
    // http://www.geisswerks.com/ryan/FAQS/timing.html
    // Sleep will do granular scheduling up to 1ms
    timeBeginPeriod(1);

    logn("Start sending msg to server");
    while ( keep_alive )
    {
        LARGE_INTEGER starting_time, ending_time, delta_ms;

        starting_time = Win32QueryPerformance();

        logn("Sending msg");
        MessageToServer(handle, addr, &packet_seq, &remote_seq_number);
        HandleRecv(handle, &remote_seq_number);
        
        // sleep expected time
        LARGE_INTEGER time_frame_elapsed = 
            Win32QueryPerformanceDiff(Win32QueryPerformance(), starting_time, perf_freq);
        r32 remaining_ms = expected_ms_per_package - QUAD_TO_MS(time_frame_elapsed);

        //logn("Time frame remaining %f\n",remaining_ms);

        if (remaining_ms > 1.0f)
        {
            time_frame_elapsed = Win32QueryPerformanceDiff(Win32QueryPerformance(), starting_time, perf_freq);
            remaining_ms = expected_ms_per_package - QUAD_TO_MS(time_frame_elapsed);
            while (remaining_ms > 1.0f)
            {
                Sleep((DWORD)remaining_ms);
                time_frame_elapsed = Win32QueryPerformanceDiff(Win32QueryPerformance(), starting_time, perf_freq);
                remaining_ms = expected_ms_per_package - QUAD_TO_MS(time_frame_elapsed);
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
