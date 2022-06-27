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
#if 0
    u32 ip_addr = IP_ADDR( 127, 0 , 0 , 1);
#else
    u32 ip_addr = IP_ADDR( 3, 71, 72, 238);
#endif
    const char * msg = "Hello";
    sockaddr_in addr = CreateSocketAddress( ip_addr, port);

    SOCKET_RETURN_ON_ERROR(CreateSocketUdp(&handle));
    SOCKET_RETURN_ON_ERROR(BindSocket(handle, port));
    SOCKET_RETURN_ON_ERROR(SetSocketNonBlocking(handle));
    
    while ( keep_alive )
    {
      SOCKET_RETURN_ON_ERROR(SendPackage(handle,addr, (void *)msg, strlen(msg)));
      Sleep(1000);
    }

    const char * msg_exit = "Bye";
    SOCKET_RETURN_ON_ERROR(SendPackage(handle,addr, (void *)msg_exit, strlen(msg_exit)));

    CloseSocket(handle);

    ShutdownSockets();

    return 0;
}
