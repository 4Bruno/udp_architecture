#include <fcntl.h>
#include <sys/errno.h>
#include <string.h>
#include "platform.h"
#include "network_udp.h"


bool
InitializeSockets()
{
    return true;
}

void 
ShutdownSockets()
{
    //
}

SET_SOCKET_NON_BLOCKING(SetSocketNonBlocking)
{
    int nonBlocking = 1;

    int result = fcntl( handle, F_SETFL, O_NONBLOCK, nonBlocking );

    result = (result == -1);

    return result;
}

CREATE_SOCKET_UDP(CreateSocketUdp)
{
    *handle = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );

    int result = ( (*handle) == -1 );

    return result;
}


BIND_SOCKET(BindSocket)
{
    sockaddr_in address;
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port        = htons( (unsigned short) port );

    int result = bind( handle, 
                       (const sockaddr*) &address, 
                       sizeof(sockaddr_in) );
    result = (result == -1);

    return result;
}

GET_LAST_SOCKET_ERROR_MESSAGE(GetLastSocketErrorMessage)
{
    int err = errno;
    return strerror(err);
}

CREATE_SOCKET_ADDRESS(CreateSocketAddress)
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl( ip_address );
    addr.sin_port = htons( port );

    return addr;
}

SEND_PACKAGE(SendPackage)
{
    int sent_bytes = 
        sendto( handle, 
                (const char*)data, 
                size,
                0, 
                (sockaddr*)&address, 
                sizeof(sockaddr_in) );

    int result = ( sent_bytes != size);

    return result;
}
