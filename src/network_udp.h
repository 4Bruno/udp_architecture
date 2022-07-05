#ifndef PORTABLE_NETWORK_UDP_H

#include "platform.h"

#ifdef _WIN32
#define _CRT_NO_POSIX_ERROR_CODES
typedef SOCKET socket_handle ;
typedef int socklen_t;
#define socket_errno GetLastError()
#ifndef SOCKET_ERROR
#error Windows missing SOCKET_ERROR definition?
#endif
#ifdef EWOULDBLOCK
#undef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#endif

#elif defined __linux__
#include <sys/socket.h>
#include <sys/errno.h>
#include <netinet/in.h>
typedef int socket_handle ;
#define socket_errno errno
#define SOCKET_ERROR -1

#else

#error Not supported

#endif

#define IP_ADDR(a,b,c,d) ((a << 24) |\
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

#define CLOSE_SOCKET(name) void name(socket_handle handle)
typedef CLOSE_SOCKET(close_socket);
CLOSE_SOCKET(CloseSocket);

#define PORTABLE_NETWORK_UDP_H
#endif
