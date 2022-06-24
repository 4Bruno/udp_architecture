
#ifndef PORTABLE_NETWORK_UDP_H

#include "platform.h"

#ifdef _WIN32

#include <winsock2.h>
typedef SOCKET socket_handle ;
typedef int socklen_t;

#elif defined __linux__
#include <sys/socket.h>
#include <netinet/in.h>
typedef int socket_handle ;

#else

#error Not supported

#endif

#define IP_ADDR(a,b,d,c) ((a << 24) |\
                          (b << 16) |\
                          (c << 8)  |\
                          d)

bool InitializeSockets();
void ShutdownSockets();

#define SET_SOCKET_NON_BLOCKING(name) int name(socket_handle handle)
typedef SET_SOCKET_NON_BLOCKING(set_socket_non_blocking);
SET_SOCKET_NON_BLOCKING(SetSocketNonBlocking);

#define CREATE_SOCKET_UDP(name) int name(socket_handle * handle)
typedef CREATE_SOCKET_UDP(create_socket_udp);
CREATE_SOCKET_UDP(CreateSocketUdp);


#define BIND_SOCKET(name) int name(socket_handle handle, int port)
typedef BIND_SOCKET(bind_socket);
BIND_SOCKET(BindSocket);

#define GET_LAST_SOCKET_ERROR_MESSAGE(name) const char * name()
typedef GET_LAST_SOCKET_ERROR_MESSAGE(get_last_socket_error_message);
GET_LAST_SOCKET_ERROR_MESSAGE(GetLastSocketErrorMessage);

#define SEND_PACKAGE(name) int name(socket_handle handle, sockaddr_in address, void * data, int size)
typedef SEND_PACKAGE(send_package);
SEND_PACKAGE(SendPackage);

#define CREATE_SOCKET_ADDRESS(name) sockaddr_in name(u32 ip_address, int port)
typedef CREATE_SOCKET_ADDRESS(create_socket_address);
CREATE_SOCKET_ADDRESS(CreateSocketAddress);

#define PORTABLE_NETWORK_UDP_H
#endif
