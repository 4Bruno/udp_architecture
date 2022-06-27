#include "../src/network_udp.h"
#include "../src/logger.h"
#include <string.h>
#include "../src/atomic.h"

#define SOCKET_RETURN_ON_ERROR(fcall) if ((fcall) == SOCKET_ERROR)\
                              {\
                                  logn("Socket error: \n" #fcall "\n%s", GetLastSocketErrorMessage());\
                                  return 1;\
                              }

static volatile int keep_alive = 1;

struct packet
{
    i32 id;
    char msg[256];
};

int
HandleRecv(socket_handle handle)
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
                packet_data.id, packet_data.msg); 

        recv_status = fromLength;
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

    logn("Got console ctrl handler code %lu", dwCtrlType);

    return signal_handled;
}

void
InitializeTerminateSignalHandler()
{
    SetConsoleCtrlHandler(HandleCtlC, true);
}

int
main()
{
    InitializeTerminateSignalHandler();
#else
int
main()
{
#endif

    if (!InitializeSockets())
    {
        logn("Error initializing sockets library. %s", GetLastSocketErrorMessage());
    }

    socket_handle handle = 0;
    int port = 30000;
#if 1
    u32 ip_addr = IP_ADDR( 127, 0 , 0 , 1);
#else
    u32 ip_addr = IP_ADDR( 3, 71, 72, 238);
#endif
    sockaddr_in addr = CreateSocketAddress( ip_addr, port);

    SOCKET_RETURN_ON_ERROR(CreateSocketUdp(&handle));
    SOCKET_RETURN_ON_ERROR(BindSocket(handle, port));
    SOCKET_RETURN_ON_ERROR(SetSocketNonBlocking(handle));
    
    struct packet packet;
    i32 packet_stack = 0;

    logn("Start sending msg to server");
    while ( keep_alive )
    {
        packet.id = packet_stack++;

        sprintf(packet.msg, "Hello, this is msg number %i", packet.id); 

        if (SendPackage(handle,addr, (void *)&packet, sizeof(packet)) == SOCKET_ERROR)
        {
            logn("Error sending package %i. %s", packet.id, GetLastSocketErrorMessage());
            keep_alive = 0;
        }

        HandleRecv(handle);

        Sleep(1000);
    }

    sprintf(packet.msg, "Bye");
    packet.id = -1;
    if (SendPackage(handle,addr, (void *)&packet, sizeof(packet)) == SOCKET_ERROR)
    {
        logn("Error sending package %i. %s", packet.id, GetLastSocketErrorMessage());
    }

    CloseSocket(handle);

    ShutdownSockets();

    return 0;
}
