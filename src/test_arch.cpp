#include <windows.h>
#include <stdio.h>


bool EnableVTMode()
{
    // Set output mode to handle virtual terminal sequences
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
    {
        return false;
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode))
    {
        return false;
    }
    return true;
}

int main()
{
    
    //EnableVTMode();
    //
    //_chdir();
    SetCurrentDirectory(".\\build\\debug\\");

    const char * server_exe_name = "test_server_udp_package_loss_handler.exe";
    const char * client_exe_name = "test_network_udp_package_loss_handler.exe";

    STARTUPINFO startup_info = {};
    startup_info.cb = sizeof(startup_info);

    PROCESS_INFORMATION server_pi;

    startup_info.dwFillAttribute = FOREGROUND_BLUE | BACKGROUND_BLUE;
    if (!CreateProcessA(server_exe_name, NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &startup_info,&server_pi))
    {
        printf("Couldnt' initialize server\n");
        return 1;
    }

    Sleep(1000);

    const int total_clients = 3;
    PROCESS_INFORMATION client_pi[total_clients];
    for (int client_index = 0; client_index < total_clients; ++client_index)
    {
        startup_info.dwFillAttribute = FOREGROUND_RED | BACKGROUND_RED;
        if (!CreateProcessA(client_exe_name, NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &startup_info,client_pi + client_index))
        {
            printf("Couldnt' initialize client\n");
            return 1;
        }
    }


#if 1
    DWORD handle_signaled = WaitForSingleObject(server_pi.hProcess, INFINITE);
#else
    HANDLE event_handlers[1] = { server_pi.hProcess };
    BOOL server_is_alive = TRUE;
    while (server_is_alive)
    {
        DWORD handle_signaled = WaitForMultipleObjects(2, event_handlers, FALSE, INFINITE);
        server_is_alive = (handle_signaled != WAIT_OBJECT_0);
        if (!server_is_alive)
        {
            TerminateProcess(client_pi.hProcess, 200);
        }
    }
#endif

    for (int client_index = 0; client_index < total_clients; ++client_index)
    {
        TerminateProcess(client_pi[client_index].hProcess, 200);
        CloseHandle(client_pi[client_index].hProcess);
        CloseHandle(client_pi[client_index].hThread);
    }
    CloseHandle(server_pi.hProcess);
    CloseHandle(server_pi.hThread);

}
