#include <windows.h>
#include "console_sequences.h"

#include <conio.h>

coord
Win32GetConsoleSize()
{
    CONSOLE_SCREEN_BUFFER_INFO ScreenBufferInfo;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &ScreenBufferInfo);

    coord Size;
    Size.X = ScreenBufferInfo.srWindow.Right - ScreenBufferInfo.srWindow.Left + 1;
    Size.Y = ScreenBufferInfo.srWindow.Bottom - ScreenBufferInfo.srWindow.Top + 1;

    return Size;
}

CREATE_VIRTUAL_SEQ_CONSOLE(CreateVirtualSeqConsole)
{
    con->vt_enabled= false;

    con->margin_bottom = 0;
    con->margin_top = 0;
    con->count_max_palette_index = 0;

    HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);

    if (handle_out != INVALID_HANDLE_VALUE &&
        handle_in  != INVALID_HANDLE_VALUE)
    {
        DWORD dwMode = 0;
        if (GetConsoleMode(handle_out, &dwMode))
        {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            if (SetConsoleMode(handle_out, dwMode))
            {
                con->vt_enabled = true;
            }
        }
#if 0
        if (GetConsoleMode(con->handle_in, &dwMode))
        {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
            if (SetConsoleMode(con->handle_in, dwMode))
            {
                con->vt_enabled = con->vt_enabled & true;
            }
        }
#endif
    }

    if (con->vt_enabled)
    {
        con->size = Win32GetConsoleSize();
    }
}

INIT_TERMIOS(InitTermios) {/*stub */}

RESET_TERMIOS(ResetTermios){/*stub */}

STDIN_SET_NON_BLOCKING(StdinSetNonBlocking)
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdin, &mode);
    mode &= ~ENABLE_LINE_INPUT; // Disable line input
    mode &= ~ENABLE_ECHO_INPUT; // Disable echo input
    SetConsoleMode(hStdin, mode);
}

// Function to reset stdin to blocking mode
STDIN_SET_BLOCKING(StdinSetBlocking)
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdin, &mode);
    mode &= ENABLE_LINE_INPUT; // Disable line input
    mode &= ~ENABLE_ECHO_INPUT; // Disable echo input
    SetConsoleMode(hStdin, mode);
}

GET_CHAR(GetChar)
{
    char v = EOF;
    if (_kbhit()) {
        v = _getch();
    }

    return v;
}


GET_TERMINAL_SIZE(GetTerminalSize)
{
    coord rect;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    i32 api_result = GetConsoleScreenBufferInfo(hStdOut, &csbi);
    if (api_result) {
        rect.Y = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        rect.X = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        rect.Y = 24;  // Default fallback
        rect.X = 80;  // Default fallback
    }

    return rect;
}

