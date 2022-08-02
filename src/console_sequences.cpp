
/*
 * Watch out:
 * Sequences defines rows as 'y' and columns as 'x'
 * which might be confusing at times, depending on your background
 */
#include "console_sequences.h"

//#include <windows.h>
#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

typedef uint32_t u32;



void
AdvCursor(struct pos pos)
{
    if (pos.up)
    {
        printf(CSI "%iA", pos.up);
    }
    if (pos.down)
    {
        printf(CSI "%iB", pos.down);
    }
    if (pos.right)
    {
        printf(CSI "%iC", pos.right);
    }
    if (pos.left)
    {
        printf(CSI "%iD", pos.left);
    }
}

void
MoveCursorAbs(int x, int y)
{
    printf(CSI "%i;%iH", x, y);
}

void
ConsoleClear()
{
    // Clear screen, tab stops, set, stop at columns 16, 32
    printf(CSI "2J"); // Clear screen
    printf(CSI "3g"); // clear all tab stops

}

bool
ValidateEscResponse()
{
    bool valid = false;
    wchar_t wch;
    wch = _getwch();
    if (wch == '\x1b')
    {
        wch = _getwch();
        valid = (wch == '[');
    }

    return valid;
}

wchar_t
GetFunctionKey()
{
    wchar_t wch;
    wch = _getwch();
    {
        if (wch == 0 || wch == 0xE0)
        {
            wch = _getwch();
        }
    }

    return wch;
}

int
ParseResponseInt(char stop_if)
{
    char buffer[10];
    wchar_t wch;
    int size = 0;
    while ( (wch = _getwch()) && wch != stop_if)
    {
        buffer[size++] = wch;
        if (wch == 0 || wch == 0xE0)
        {
            wch = _getwch();
        }
    }

    buffer[size] = 0;

    int val = atoi(buffer);

    return val;

}

coord
ConsoleGetCursorP()
{
    struct coord coord;

    printf(CSI "6n");

    if (ValidateEscResponse())
    {
        coord.X = ParseResponseInt(';');
        coord.Y = ParseResponseInt('R');
    }

    return coord;
}

int
ConsoleSetNextPalette(console * con, u32 r, u32 g, u32 b)
{
    int index = -1;

    if ( (con->count_max_palette_index + 1) < 256)
    {
        index = con->count_max_palette_index++;
        printf(OSC "4;%i;rgb:%u/%u/%u" OSC_END,
                index, r & 0xFF, g & 0xFF, b & 0xFF);
    }

    return index;
}

void
ConsoleSetForegroundColor(u32 r, u32 g, u32 b)
{
    printf(CSI "%u;2;%u;%u;%um",
            Foreground_Extended,
            r & 0xFF,
            g & 0xFF,
            b & 0xFF);
}

void
ConsoleSetBackgroundColorIndex(int i)
{
    if (i >= 0)
    {
        printf(CSI "%u;5;%im",
                Background_Extended,
                i & 0xFF);
    }
}

void
ConsoleSetBackgroundColor(u32 r, u32 g, u32 b)
{
    printf(CSI "%u;2;%u;%u;%um",
            Background_Extended,
            r & 0xFF,
            g & 0xFF,
            b & 0xFF);
}

void
ConsoleSetTextFormat(int count_formats, ...)
{
    // This command is special in that the <n> position 
    // below can accept between 0 and 16 parameters 
    // separated by semicolons.

    // When no parameters are specified, 
    // it is treated the same as a single 0 parameter.


    va_list list;
    va_start(list, count_formats);

    // longest number to string 100 - 3 char per number
    // max number of formats        - 16 numbers
    // max delimiters between format- 15 char delimiter
    //                              - 0 term char
    char buffer[16 * 3 + 15 + 1];

    int offset_buffer = 0;
    for(int i = 0; i < count_formats; ++i)
    {
        console_text_formats f = va_arg(list, console_text_formats);
        _itoa_s(f,buffer + offset_buffer, sizeof(buffer) - offset_buffer, 10);
        int size  = 0;

        do {
            size += 1;
            f = (console_text_formats)((float)f / 10.0f);
        } while (f > 0);

        offset_buffer += size;

        buffer[offset_buffer] = ';';
        offset_buffer += 1;
    }

    buffer[--offset_buffer] = 'm';
    buffer[offset_buffer + 1] = 0;

    va_end(list);

    printf(CSI "%s", buffer);
}

int
GetConsoleNumLines(console * con)
{
    int lines_count = con->size.Y - (con->margin_top + con->margin_bottom - 1);

    return lines_count;
}

void
ConsoleSaveCursor()
{
    printf(CSI "s");
}
void
ConsoleRestoreCursor()
{
    printf(CSI "u");
}

void
SetScrollMargin(console * con, int margin_top, int margin_bottom)
{
    con->margin_bottom = margin_bottom;
    con->margin_top = margin_top;
    printf(CSI "%d;%dr",margin_top, con->size.Y - margin_bottom);

    // modify log line output to account for margins
    con->max_lines = (con->size.Y - margin_top - margin_bottom);
    con->current_line = (con->current_line % con->max_lines);
}

void
SetTabStopAt(int x, int y)
{
    MoveCursorAbs(x, y);
    printf(ESC "H"); // set a tab stop
}

void
ConsoleClearLine(int line)
{
    printf(CSI "%iK", line);
}

void
ConsoleAlternateBuffer()
{
    printf(CSI "?1049h");
}

void
ConsoleExitAlternateBuffer()
{
    printf(CSI "?1049l");
}

void
ConsoleSetWidth132()
{
    printf(CSI "?3h");
}

void
ConsoleSetWidth80()
{
    printf(CSI "?3l");
}

