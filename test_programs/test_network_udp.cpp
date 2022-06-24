#include "../src/network_udp.h"
#include "../src/logger.h"


int
main()
{

    if (!InitializeSockets())
    {
        logn("Error initializing sockets library. %s", GetLastSocketErrorMessage());
    }

    socket_handle handle = 0;
    int port = 30000;
    u32 ip_addr = IP_ADDR( 127, 0 , 0 , 1);

    if (CreateSocketUdp(&handle) != 0)
    {
        logn("Error creating socket. %s", GetLastSocketErrorMessage());
    }

    if (BindSocket(handle, port) != 0)
    {
        logn("Error setting socket as non blocking. %s", GetLastSocketErrorMessage());
    }

    if (SetSocketNonBlocking(handle) != 0)
    {
        logn("Error setting socket as non blocking. %s", GetLastSocketErrorMessage());
    }

    sockaddr_in addr = CreateSocketAddress( ip_addr, port);

    const char * msg = "Hello";

    SendPackage(handle,addr, (void *)msg, strlen(msg));
    
    while ( 1 )
    {
        unsigned char packet_data[256];

        unsigned int max_packet_size = sizeof( packet_data );

        sockaddr_in from;
        socklen_t fromLength = sizeof( from );

        int bytes = recvfrom( handle, 
                              (char*)packet_data, max_packet_size, 
                              0, 
                              (sockaddr*)&from, &fromLength );

        if ( bytes <= 0 )
            break;

        unsigned int from_address = 
            ntohl( from.sin_addr.s_addr );

        unsigned int from_port = 
            ntohs( from.sin_port );

        // process received packet
        logn("[%i.%i.%i.%i] Message received %.*s", (from_address >> 24),
                                                    (from_address >> 16) & 0xFF0000,
                                                    (from_address >> 8) & 0xFF00,
                                                    from_address & 0xFF,
                                                    fromLength, packet_data); 
    }

    ShutdownSockets();

}
