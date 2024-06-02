#ifndef PORTABLE_PLATFORM_H

/* ---------------- WINDOWS ---------------- */
#ifdef _WIN32
#define OS_WINDOWS
#include <windows.h>
#define API _declspec( dllexport )

// Time
#define real_time LARGE_INTEGER
#define clock_resolution LARGE_INTEGER
#define delta_time r32 
#define ZeroTime(rt) rt.QuadPart = 0


// IO
#define OpenFile(fd, file, mode) fopen_s(&fd, file, mode)
#include <direct.h>
#define mkdir(d) _mkdir(d)

/* ---------------- LINUX ---------------- */
#elif defined __linux__
#define OS_LINUX
#include <stdlib.h>
#define API __attribute__((visibility("default")))
#define TRUE 1
#define FALSE 0
#define INVALID_HANDLE_VALUE 0

// Time
#include <time.h>
#define real_time timespec 
#define clock_resolution timespec
#define delta_time r32
#define ZeroTime(rt) memset(&rt, 0, sizeof(rt));

// IO
#define OpenFile(fd, file, mode) fd = fopen(file, mode)
#include <sys/stat.h>
// TODO: this is unsafe for server!!!!! full access folder
#define mkdir(d) mkdir(d, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
// socket windows diff definitions
#define WSAECONNRESET ECONNRESET
#include <unistd.h>
#define Sleep(n) msleep(n)
#define sleep(n) msleep(n)

#define sprintf_s snprintf
#define vsprintf_s vsnprintf
#define _getwch() getwc(STDIN_FILENO)
#define _itoa_s(format, buffer, max_size, base) snprintf( (buffer), max_size, "%d", format)
    

#else
#error Unhandled OS

#endif

#include <stdio.h>
#include <stdint.h>
#include <limits.h>

#define Assert(a) if ( (a) == 0 )\
                  {\
                     int x = *(volatile int *)0;\
                     if (x == 3) { x = 2; };\
                  }

#define ArrayCount(a) ((sizeof(a)) / (sizeof(a[0])))

typedef unsigned char u8;

typedef int BOOL;

typedef uint32_t u32;
typedef int32_t i32;
typedef int32_t b32;

typedef uint16_t u16;
typedef int16_t i16;

typedef float r32;
typedef double r64;


#define GET_REAL_TIME(name) real_time name()
typedef GET_REAL_TIME(get_real_time);
API GET_REAL_TIME(GetRealTime);

#define GET_CLOCK_RESOLUTION(name) real_time name()
typedef GET_CLOCK_RESOLUTION(get_clock_resolution);
API GET_CLOCK_RESOLUTION(GetClockResolution);

#define GET_TIME_DIFF(name) delta_time name(real_time TimeEnd,real_time TimeStart,real_time ClockFreq)
typedef GET_TIME_DIFF(get_time_diff);
API GET_TIME_DIFF(GetTimeDiff);

#define HIGH_DEFINITION_TIME_BEGIN(name) void name()
typedef HIGH_DEFINITION_TIME_BEGIN(high_definition_time_begin);
API HIGH_DEFINITION_TIME_BEGIN(HighDefinitionTimeBegin);

#define HIGH_DEFINITION_TIME_END(name) void name()
typedef HIGH_DEFINITION_TIME_END(high_definition_time_end);
API HIGH_DEFINITION_TIME_END(HighDefinitionTimeEnd);

#define MS_SLEEP(name) int name(u32 msec)
typedef MS_SLEEP(ms_sleep);
API MS_SLEEP(msleep);

#define Kilobytes(x) 1024 * x
#define Megabytes(x) 1024 * Kilobytes(x)
#define Gigabytes(x) 1024 * Megabytes(x)

#define PushArray(arena, size, s) (s *)PushSize_(arena, sizeof(s) * size)
#define PushStruct(arena, s) (s *)PushSize_(arena, sizeof(s))
#define PushSize(arena, s) (void*)PushSize_(arena, s)
struct memory_arena
{
    void * base;
    u32 size;
    u32 max_size;
};

inline void *
PushSize_(memory_arena * arena, i32 size)
{
    Assert( (arena->size + size) <= arena->max_size );
    void * base = (u8 *)arena->base + arena->size;
    arena->size += size;

    return base;
}

#define STDIN_SET_NON_BLOCKING(name)  void name()
typedef STDIN_SET_NON_BLOCKING(stdin_set_non_blocking);
API STDIN_SET_NON_BLOCKING(StdinSetNonBlocking);

#define STDIN_SET_BLOCKING(name)  void name()
typedef STDIN_SET_BLOCKING(stdin_set_blocking);
API STDIN_SET_BLOCKING(StdinSetBlocking);

#define GET_CHAR(name)  char name()
typedef GET_CHAR(get_char);
API GET_CHAR(GetChar);

/* ---------------- WINDOWS ---------------- */
#ifdef _WIN32
#define STALL(ms) Sleep(ms)

/* ---------------- LINUX ---------------- */
#elif defined __linux__
#define STALL(ms) usleep((r32)ms * 1000.0f)

#else
#error Unhandled OS

#endif

#define PORTABLE_PLATFORM_H
#endif
