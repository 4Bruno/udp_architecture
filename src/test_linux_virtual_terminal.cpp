#include <stdio.h>
#include "linux_virtual_terminal.cpp"
#include "console_sequences.cpp"
#include "logger.h"

static struct console con;

int
main()
{
    con = CreateVirtualSeqConsole();

    if (!con.vt_enabled)
    {
        logn("Couldn't initialize console virtual seq");
        return - 1;
    }

    ConsoleAlternateBuffer();
    ConsoleClear();
    SetScrollMargin(&con, 4, 2);
    MoveCursorAbs(1,1);
    ConsoleSetTextFormat(2, Background_Black, Foreground_Green);

    set_non_blocking_mode();
    initTermios(0);

    char c;
    bool continue_loop = true;
    int counter = 0;

    while (continue_loop)
    {
        c = getch();
        if (c == 'q')
            continue_loop = false;
        else if (c == '\n')
        {
            coord cursorP = ConsoleGetCursorP();
            MoveCursorAbs(cursorP.X + 1, 1);
            printf("%i", cursorP.X);
        }
        else if (c != EOF)
        {
            MoveCursorAbs(counter + 1,1);
            printf("Client %c",c);
            fflush(STDIN_FILENO);
            counter = (counter+1) % 4;
        }

        sleep(50);
    }

    if (con.vt_enabled)
    {
        ConsoleExitAlternateBuffer();
        resetTermios();
    }

}
