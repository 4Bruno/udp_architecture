/*
 * Watch out:
 * Sequences defines rows as 'y' and columns as 'x'
 * which might be confusing at times, depending on your background
 */
#include "console_sequences.h"

//#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <fcntl.h>
#include <termios.h>


typedef uint32_t u32;

static struct termios terminal_session_old;
static struct termios terminal_session_current;

#define CONSOLE_BUFFER_HEIGHT 24
#define CONSOLE_BUFFER_WIDTH 80
static char console_back_buffer[CONSOLE_BUFFER_HEIGHT][CONSOLE_BUFFER_WIDTH + 1];
static char console_front_buffer[CONSOLE_BUFFER_HEIGHT][CONSOLE_BUFFER_WIDTH + 1];

void
ConsoleDraw(int at_line,const char * format, ...)
{
    va_list list;
    va_start(list, format);

    vsnprintf(console_back_buffer[at_line],CONSOLE_BUFFER_WIDTH + 1, format, list);

    va_end(list);
}

/* prevents from writing \0 at the end of string */
void
ConsoleAppendAt(int at_line, int at_start, const char * format, ...)
{
    if (at_start > CONSOLE_BUFFER_WIDTH) return;
    va_list list;
    va_start(list, format);

    int err = vsnprintf((console_back_buffer[at_line] + at_start),CONSOLE_BUFFER_WIDTH + 1 - at_start, format, list);
    if (err > 0)
    {
        console_back_buffer[at_line][at_start + err] = ' ';
    }
    //memset(console_back_buffer[at_line],'-',at_start);

    va_end(list);
}

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

void
ConsoleClearBuffers()
{
    memset(console_back_buffer,' ',CONSOLE_BUFFER_HEIGHT * (CONSOLE_BUFFER_WIDTH + 1));
    for (int i = 0;
            i < CONSOLE_BUFFER_HEIGHT;
            ++i)
    {
        console_back_buffer[i][CONSOLE_BUFFER_WIDTH] = '\0';
    }

    memset(console_front_buffer,' ',CONSOLE_BUFFER_HEIGHT * (CONSOLE_BUFFER_WIDTH + 1));
    for (int i = 0;
            i < CONSOLE_BUFFER_HEIGHT;
            ++i)
    {
        console_front_buffer[i][CONSOLE_BUFFER_WIDTH] = '\0';
    }
}

void
ConsoleSwapBuffer()
{
    printf("\x1b[H");  // Move cursor to home position
    for (int y = 0; 
             y < CONSOLE_BUFFER_HEIGHT; 
             ++y) 
    {
        int mem_cmp = memcmp(console_front_buffer[y], console_back_buffer[y], CONSOLE_BUFFER_WIDTH);
        if ( mem_cmp != 0 ) 
        {
            printf("\x1b[%d;1H%s", y + 1, console_back_buffer[y]);
            memcpy(console_front_buffer[y], console_back_buffer[y], CONSOLE_BUFFER_WIDTH);
        }
    }
    fflush(stdout);
}

void initTermios(int echo) 
{
  tcgetattr(STDIN_FILENO, &terminal_session_old);
  terminal_session_current = terminal_session_old;
  /* disable buffered i/o */
  terminal_session_current.c_lflag &= ~ICANON; 
  if (echo) {
      terminal_session_current.c_lflag |= ECHO; 
  } else {
      terminal_session_current.c_lflag &= ~ECHO; 
  }
  tcsetattr(STDIN_FILENO, TCSANOW, &terminal_session_current); 
}

void resetTermios(void) 
{
  tcsetattr(STDIN_FILENO, TCSANOW, &terminal_session_old);
}

char getch() 
{
  char ch;
  //ch = getchar();
  int result = read(STDIN_FILENO,&ch,1);
  if (result <= 0)
      ch = EOF;
  return ch;
}

void set_non_blocking_mode() {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

// Function to reset stdin to blocking mode
void set_blocking_mode() {
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
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
    wch = getch();
    if (wch == '\x1b')
    {
        wch = getch();
        valid = (wch == '[');
    }

    return valid;
}

wchar_t
GetFunctionKey()
{
    int wch;
    wch = getch();
    {
        if (wch == 0 || wch == 0xE0)
        {
            wch = getch();
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
    while ( (wch = getch()) && wch != stop_if)
    {
        buffer[size++] = wch;
        if (wch == 0 || wch == 0xE0)
        {
            wch = getch();
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
