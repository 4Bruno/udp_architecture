#include "network_udp.h"
#include "logger.h"
#include <string.h>
#include "atomic.h"
#include "test_network.h"

#define SOCKET_RETURN_ON_ERROR(fcall) if ((fcall) == SOCKET_ERROR)\
                              {\
                                  logn("Socket error: \n" #fcall "\n%s", GetLastSocketErrorMessage());\
                                  return 1;\
                              }

static volatile int keep_alive = 1;

int
main()
{
    if (!InitializeSockets())
    {
        logn("Error initializing sockets library. %s", GetLastSocketErrorMessage());
    }

    socket_handle handle = 0;
    int port = 30000;

    SOCKET_RETURN_ON_ERROR(CreateSocketUdp(&handle));

    // https://docs.microsoft.com/en-us/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse
    BOOL opt_val = TRUE;
    int opt_len = sizeof(opt_val);
    setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt_val, opt_len);
    
    SOCKET_RETURN_ON_ERROR(BindSocket(handle, port));
    SOCKET_RETURN_ON_ERROR(SetSocketNonBlocking(handle));

    logn("Starting server");
    while ( keep_alive )
    {
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
            if (socket_errno != EWOULDBLOCK && socket_errno != WSAECONNRESET)
            {
                logn("Error recvfrom(). %s", GetLastSocketErrorMessage());
                return 1;
            }
            Sleep(1000);
        }
        else if ( bytes == 0 )
        {
            logn("No more data. Closing.");
            break;
        }
        else
        {

            unsigned int from_address = 
                ntohl( from.sin_addr.s_addr );

            unsigned int from_port = 
                ntohs( from.sin_port );

            // process received packet
            logn("[%i.%i.%i.%i] Message (%u) received %s", 
                                                             (from_address >> 24),
                                                             (from_address >> 16)  & 0xFF0000,
                                                             (from_address >> 8)   & 0xFF00,
                                                             (from_address >> 0)   & 0xFF,
                                                             packet_data.seq, packet_data.msg); 

            struct packet ack;
            ack.seq = packet_data.seq;
            ack.ack = packet_data.seq;
            sprintf(ack.msg, "I got our msg %u", packet_data.seq);
            if (SendPackage(handle,from, (void *)&ack, sizeof(ack)) == SOCKET_ERROR)
            {
                logn("Error sending ack package %u. %s", ack.seq, GetLastSocketErrorMessage());
                keep_alive = 0;
            }
        }
    }

    CloseSocket(handle);

    ShutdownSockets();

    return 0;
}
