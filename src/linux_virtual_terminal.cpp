#include "console_sequences.h"
#include <stdio.h>
#include <sys/ioctl.h>


static struct termios terminal_session_old;
static struct termios terminal_session_current;

coord
GetConsoleSize()
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
    con->vt_enabled= false;
    con->margin_bottom = 0;
    con->margin_top = 0;
    con->count_max_palette_index = 0;

    con->size = GetConsoleSize();
    con->max_lines = con->size.Y;
    con->vt_enabled = true;
}

INIT_TERMIOS(InitTermios)
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

RESET_TERMIOS(ResetTermios)
{
  tcsetattr(STDIN_FILENO, TCSANOW, &terminal_session_old);
}

STDIN_SET_NON_BLOCKING(StdinSetNonBlocking)
{
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

// Function to reset stdin to blocking mode
STDIN_SET_BLOCKING(StdinSetBlocking)
{
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
}

GET_CHAR(GetChar)
{
  char ch;
  int result = read(STDIN_FILENO,&ch,1);
  if (result <= 0)
      ch = EOF;
  return ch;
}

GET_TERMINAL_SIZE(GetTerminalSize)
{
    coord rect;
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        rect.Y = ws.ws_row;
        rect.X = ws.ws_col;
    } else {
        perror("ioctl");
        rect.Y = 24;  // Default fallback
        rect.X = 80;  // Default fallback
    }

    return rect;
}
