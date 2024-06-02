#ifndef CONSOLE_SEQUENCES_H
#define CONSOLE_SEQUENCES_H

#include "platform.h"

#define ESC "\x1b"
// for comparison eq x == ESC_EQ
#define ESC_EQ '\x1b'
#define CSI "\x1b["
#define OSC "\x1b]"
#define OSC_END ESC "\\"

/* ---------------- WINDOWS ---------------- */
#ifdef _WIN32
#include <windows.h>

/* ---------------- LINUX ---------------- */
#elif defined __linux__

#include <fcntl.h>
#include <termios.h>


/* ---------------- UNKNOWN ---------------- */
#else
#error Unhandled OS

#endif

struct coord
{
    int X,Y;
};


struct pos
{
    int left;
    int up;
    int right;
    int down;
};


enum console_text_formats
{
    restore_before_modification = 0, //	Default	Returns all attributes to the default state prior to modification

    apply_bright_foreground   = 1, //	Bold/Bright	Applies brightness/intensity flag to foreground color
    remove_bright_foreground  = 22, //	No bold/bright	Removes brightness/intensity flag from foreground color

    add_underline     = 4, //	Underline	Adds underline
    remove_underline  = 24, //	No underline	Removes underline

    swap_foreground_background    = 7, //	Negative	Swaps foreground and background colors
    return_foreground_background  =27, //	Positive (No negative)	Returns foreground/background to normal

    Foreground_Black	=30, //	Applies non-bold/bright black to foreground
    Foreground_Red		=31, //	Applies non-bold/bright red to foreground
    Foreground_Green	=32, //	Applies non-bold/bright green to foreground
    Foreground_Yellow	=33, //	Applies non-bold/bright yellow to foreground
    Foreground_Blue		=34, //	Applies non-bold/bright blue to foreground
    Foreground_Magenta	=35, //	Applies non-bold/bright magenta to foreground
    Foreground_Cyan		=36, //	Applies non-bold/bright cyan to foreground
    Foreground_White	=37, //	Applies non-bold/bright white to foreground
    Foreground_Extended	=38, //	Applies extended color value to the foreground (see details below)
    Foreground_Default	=39, //	Applies only the foreground portion of the defaults (see 0, //)

    Background_Black	=40, //	Applies non-bold/bright black to background
    Background_Red		=41, //	Applies non-bold/bright red to background
    Background_Green	=42, //	Applies non-bold/bright green to background
    Background_Yellow	=43, //	Applies non-bold/bright yellow to background
    Background_Blue		=44, //	Applies non-bold/bright blue to background
    Background_Magenta	=45, //	Applies non-bold/bright magenta to background
    Background_Cyan		=46, //	Applies non-bold/bright cyan to background
    Background_White	=47, //	Applies non-bold/bright white to background
    Background_Extended	=48, //	Applies extended color value to the background (see details below)
    Background_Default	=49, //	Applies only the background portion of the defaults (see 0, //)

    Bright_Foreground_Black		=90, //	Applies bold/bright black to foreground
    Bright_Foreground_Red		=91, //	Applies bold/bright red to foreground
    Bright_Foreground_Green		=92, //	Applies bold/bright green to foreground
    Bright_Foreground_Yellow	=93, //	Applies bold/bright yellow to foreground
    Bright_Foreground_Blue		=94, //	Applies bold/bright blue to foreground
    Bright_Foreground_Magenta	=95, //	Applies bold/bright magenta to foreground
    Bright_Foreground_Cyan		=96, //	Applies bold/bright cyan to foreground
    Bright_Foreground_White		=97, //	Applies bold/bright white to foreground
    Bright_Background_Black		=100, //Applies bold/bright black to background
    Bright_Background_Red		=101, //Applies bold/bright red to background
    Bright_Background_Green		=102, //Applies bold/bright green to background
    Bright_Background_Yellow	=103, //Applies bold/bright yellow to background
    Bright_Background_Blue		=104, //Applies bold/bright blue to background
    Bright_Background_Magenta	=105, //Applies bold/bright magenta to background
    Bright_Background_Cyan		=106, //Applies bold/bright cyan to background
    Bright_Background_White		=107, //Applies bold/bright white to background
};


struct console;
#define CREATE_VIRTUAL_SEQ_CONSOLE(name)  void name(console * con)
typedef CREATE_VIRTUAL_SEQ_CONSOLE(create_virtual_seq_console);
API CREATE_VIRTUAL_SEQ_CONSOLE(CreateVirtualSeqConsole);

#define INIT_TERMIOS(name)  void name(int echo)
typedef INIT_TERMIOS(init_termios);
API INIT_TERMIOS(InitTermios);

#define RESET_TERMIOS(name)  void name()
typedef RESET_TERMIOS(reset_termios);
API RESET_TERMIOS(ResetTermios);

#define GET_TERMINAL_SIZE(name)  coord name()
typedef GET_TERMINAL_SIZE(get_terminal_size);
API GET_TERMINAL_SIZE(GetTerminalSize);

struct console
{
    bool vt_enabled;

    int margin_top, margin_bottom;
    int count_max_palette_index;

    coord size;

    u32 current_line;
    int max_lines;

    union
    {
        char * buffers[2];
        struct {
            char * front_buffer;
            char * back_buffer;
        };
    };
    coord buffer_size;
};

#endif
