#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
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

    int result = fcntl( fd, F_SETFL, O_NONBLOCK, nonBlocking );

    resut = (result != -1);

    return result;
}

CREATE_SOCKET_UDP(CreateSocketUdp)
{
    *handle = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );

    int result = ( (*handle) == INVALID_SOCKET );

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
    result = (result == SOCKET_ERROR);

    return result;
}

GET_LAST_SOCKET_ERROR(GetLastSocketError)
{
    return 0;
}

