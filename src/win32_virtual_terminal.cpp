#include <windows.h>
#include "console_sequences.h"

coord
Win32GetConsoleSize(struct console * con)
{
    CONSOLE_SCREEN_BUFFER_INFO ScreenBufferInfo;
    GetConsoleScreenBufferInfo(con->handle_out, &ScreenBufferInfo);

    coord Size;
    Size.X = ScreenBufferInfo.srWindow.Right - ScreenBufferInfo.srWindow.Left + 1;
    Size.Y = ScreenBufferInfo.srWindow.Bottom - ScreenBufferInfo.srWindow.Top + 1;

    return Size;
}

CREATE_VIRTUAL_SEQ_CONSOLE(CreateVirtualSeqConsole)
{
    struct console con;
    con.vt_enabled= false;
    con.margin_bottom = 0;
    con.margin_top = 0;
    con.count_max_palette_index = 0;
    con.handle_out = INVALID_HANDLE_VALUE;

    // Set output mode to handle virtual terminal sequences
    con.handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
    con.handle_in = GetStdHandle(STD_INPUT_HANDLE);

    if (con.handle_out != INVALID_HANDLE_VALUE &&
            con.handle_in != INVALID_HANDLE_VALUE)
    {
        DWORD dwMode = 0;
        if (GetConsoleMode(con.handle_out, &dwMode))
        {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            if (SetConsoleMode(con.handle_out, dwMode))
            {
                con.vt_enabled = true;
            }
        }
#if 0
        if (GetConsoleMode(con.handle_in, &dwMode))
        {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
            if (SetConsoleMode(con.handle_in, dwMode))
            {
                con.vt_enabled = con.vt_enabled & true;
            }
        }
#endif
    }

    if (con.vt_enabled)
    {
        con.size = Win32GetConsoleSize(&con);
        con.current_line = 0;
        con.max_lines = con.size.Y;
    }

    return con;
}
