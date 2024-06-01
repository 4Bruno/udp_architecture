#pragma comment( lib, "wsock32.lib" )
#include "network_udp.h"
#include "logger.h"


static char g_socket_library_formatted_internal_error_buffer[1024] = {};
static const char * g_socket_library_formatted_internal_error_buffer_issue = "Error formatting error message";

bool InitializeSockets()
{
    WSADATA WsaData;
    return WSAStartup( MAKEWORD(2,2), 
            &WsaData ) 
        == NO_ERROR;
    return true;
}

void ShutdownSockets()
{
    WSACleanup();
}

SET_SOCKET_NON_BLOCKING(SetSocketNonBlocking)
{
    DWORD nonBlocking = 1;

    int result = ioctlsocket( handle, FIONBIO, &nonBlocking );

    return result;
}

CREATE_SOCKET_UDP(CreateSocketUdp)
{
    *handle = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );

    int result = ( (*handle) == INVALID_SOCKET ) ? SOCKET_ERROR : 1;

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
    return result;
}

GET_LAST_SOCKET_ERROR_MESSAGE(GetLastSocketErrorMessage)
{
    char * err_msg = g_socket_library_formatted_internal_error_buffer;
    DWORD err_no = GetLastError();
    LPTSTR Error = 0;

    //logn("System error code %lu",err_no);

    int no_chars = FormatMessage(  FORMAT_MESSAGE_FROM_SYSTEM,
                NULL,
                err_no,
                0,
                err_msg,
                sizeof(g_socket_library_formatted_internal_error_buffer),
                NULL);

    if(no_chars == 0)
    {
        err_msg = (char *)g_socket_library_formatted_internal_error_buffer_issue;
    }
    else
    {
        // ignore \r\n
        err_msg[no_chars - 2] = 0;
    }

    return (const char *)err_msg;
}

CREATE_SOCKET_ADDRESS(CreateSocketAddress)
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl( ip_address );
    addr.sin_port = htons( port );

    return addr;
}

CLOSE_SOCKET(CloseSocket)
{
    closesocket(handle);
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

    return sent_bytes;
}
