#include "console_sequences.h"
#include <stdio.h>
#include <sys/ioctl.h>

coord
GetConsoleSize(struct console * con)
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    coord Size;
    Size.X = w.ws_row;
    Size.Y = w.ws_col;

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
    //con.handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
    //con.handle_in = GetStdHandle(STD_INPUT_HANDLE);
    
    con.size = GetConsoleSize(&con);
    con.current_line = 0;
    con.max_lines = con.size.Y;
    con.vt_enabled = true;

    return con;
}
