/*
 * Watch out:
 * Sequences defines rows as 'y' and columns as 'x'
 * which might be confusing at times, depending on your background
 */
#include "console_sequences.h"
//#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#define OFFSET_BACK(c,y) (con->back_buffer + (y * (con->buffer_size.X + 1)))
#define OFFSET_BACK_X(c,y,x) (con->back_buffer + (y * (con->buffer_size.X + 1)) + x)


void
ConsoleDraw(console * con, int at_line,const char * format, ...)
{
    va_list list;
    va_start(list, format);

    vsnprintf(OFFSET_BACK(con,at_line),con->buffer_size.X + 1, format, list);

    va_end(list);
}

/* prevents from writing \0 at the end of string */
void
ConsoleAppendAt(console * con, int at_line, int at_start, const char * format, ...)
{
    if (at_start > con->buffer_size.X) return;
    if (at_line > con->buffer_size.Y) return;
    if (at_start < 0) return;
    if (at_line < 0) return;

    va_list list;
    va_start(list, format);

    char * buffer = OFFSET_BACK_X(con,at_line, at_start);
    int err = vsnprintf( buffer ,con->buffer_size.X + 1 - at_start, format, list);
    if (err > 0)
    {
        *(buffer + err ) = ' ';
    }

    va_end(list);
}

#if 0
void
ConsoleAppend(int at_line,const char * format, ...)
{
    int ci = 0;
    for (ci = 0;
            ci < CONSOLE_BUFFER_WIDTH;
            ++ci)
    {
        if (console_back_buffer[at_line][ci] == '\0')
            break;
    }

    if (ci >= CONSOLE_BUFFER_WIDTH) return;

    va_list list;
    va_start(list, format);

    vsnprintf((console_back_buffer[at_line] + ci),CONSOLE_BUFFER_WIDTH + 1 - ci, format, list);

    va_end(list);
}
#endif

void
ConsoleClearBuffers(console * con)
{
    for (i32 i = 0; i < 2; ++i)
    {
        char * buffer = con->buffers[i];
        memset(buffer,' ',con->buffer_size.Y * (con->buffer_size.X + 1));
        u32 offset_x = con->buffer_size.X;
        for (int y = 0;
                y < con->buffer_size.Y;
                ++y)
        {
            u32 offset_y = (con->buffer_size.X + 1) * y;
            *(buffer + offset_y + offset_x) = '\0';
        }
    }
}

void
ConsoleSwapBuffer(console * con)
{
    // Cursor to home
    printf("\x1b[H");

    for (int y = 0; 
             y < con->buffer_size.Y; 
             ++y) 
    {

        u32 offset_y = (con->buffer_size.X + 1) * y;
        char* front_buffer = con->front_buffer + offset_y;
        char* back_buffer = con->back_buffer + offset_y;
        int mem_cmp = memcmp(front_buffer, back_buffer,  con->buffer_size.X);
        if ( mem_cmp != 0 ) 
        {
            printf("\x1b[%d;1H%s", y + 1, back_buffer);
            memcpy(front_buffer, back_buffer, con->buffer_size.X);
        }
    }
    fflush(stdout);
}

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
    int wch;
    wch = GetChar();
    if (wch == '\x1b')
    {
        wch = GetChar();
        valid = (wch == '[');
    }

    return valid;
}

wchar_t
GetFunctionKey()
{
    int wch;
    wch = GetChar();
    {
        if (wch == 0 || wch == 0xE0)
        {
            wch = GetChar();
        }
    }

    return wch;
}

int
ParseResponseInt(char stop_if)
{
    char buffer[10];
    int wch;
    int size = 0;
    while ( (wch = GetChar()) && wch != stop_if)
    {
        buffer[size++] = wch;
        if (wch == 0 || wch == 0xE0)
        {
            wch = GetChar();
        }
    }

    buffer[size] = 0;

    int val = atoi(buffer);

    return val;

}

coord
ConsoleGetCursorP()
{
    struct coord coord = {};

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
        int f = va_arg(list, int);
        _itoa_s(f,buffer + offset_buffer, sizeof(buffer) - offset_buffer, 10);
        int size  = 0;

        do {
            size += 1;
            f = (int)((float)f / 10.0f);
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
    MoveCursorAbs(line,1);
    // 0 - from cursor to end
    // 1 - from begin to cursor
    // 2 - entire
    printf(CSI "2K");
    //printf(CSI "%iM", line);
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


void
ConsoleScrollUp(int x = 1)
{
    printf(CSI "%iS", x);
}


console
CreateConsole(memory_arena * Arena,i32 Width = 80, i32 Height = 24)
{

    console con = {};

    CreateVirtualSeqConsole(&con);
    
    if (con.vt_enabled)
    {
        ConsoleAlternateBuffer();
        ConsoleClear();
        SetScrollMargin(&con, 4, 2);
        MoveCursorAbs(1,1);
        ConsoleSetTextFormat(2, Background_Black, Foreground_Green);

        u32 WidthPlusOne = Width + 1;
        u32 req_size = WidthPlusOne * Height * sizeof(u8);

        u32 AvailableMemory = Arena->max_size - Arena->size;
        Assert(AvailableMemory > (req_size * 2));

        con.buffers[0] = (char *)PushSize(Arena, req_size);
        con.buffers[1] = (char *)PushSize(Arena, req_size);

        con.buffer_size.X = Width;
        con.buffer_size.Y = Height;

        con.margin_top = 0;
        con.margin_bottom = 0;

        con.current_line = 0;
        con.max_lines = con.size.Y;

        ConsoleClearBuffers(&con);
    }

    return con;
}

void
DestroyConsole(console * con)
{
    if (con->vt_enabled)
    {
        ConsoleExitAlternateBuffer();
        ResetTermios();
    }
}

void
ConsoleIncrCL(console * con, b32 ClearIfBottom = false)
{
    con->current_line += 1;
    u32 limit = con->buffer_size.Y - (con->margin_top + con->margin_bottom);
    if (con->current_line >= limit)
    {
        con->current_line = con->margin_top + 1;
        if (ClearIfBottom)
        {
            for (u32 i = con->margin_top; i <= limit;++i)
            {
                ConsoleClearLine(i);
            }
        }
    }
}
