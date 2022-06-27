
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
main()
{
    if (!InitializeSockets())
    {
        logn("Error initializing sockets library. %s", GetLastSocketErrorMessage());
    }

    socket_handle handle = 0;
    int port = 30000;
    const char * msg = "Hello";

    SOCKET_RETURN_ON_ERROR(CreateSocketUdp(&handle));
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
            if (socket_errno != EWOULDBLOCK)
            {
                logn("Error recvfrom(). %s", GetLastSocketErrorMessage());
                return 1;
            }
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
            logn("[%i.%i.%i.%i] Message (%i) received %.*s", 
                                                             (from_address >> 24),
                                                             (from_address >> 16)  & 0xFF0000,
                                                             (from_address >> 8)   & 0xFF00,
                                                             (from_address >> 0)   & 0xFF,
                                                             fromLength, packet_data.id, packet_data.msg); 
        }
    }

    CloseSocket(handle);

    ShutdownSockets();

    return 0;
}
