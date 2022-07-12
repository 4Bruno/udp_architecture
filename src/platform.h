#ifndef PORTABLE_PLATFORM_H

#ifdef _WIN32
#include <windows.h>
#define real_time LARGE_INTEGER
#define clock_resolution LARGE_INTEGER
#define delta_time LONGLONG
#define API _declspec( dllexport )

#elif defined __linux__
#define real_time timespec 
#define clock_resolution timespec
#define delta_time LONGLONG

#else
#error Unhandled OS

#endif

#include <stdint.h>

#define Assert(a) if ( (a) == 0 )\
                  {\
                     int x = *(volatile int *)0;\
                  }

#define ArrayCount(a) ((sizeof(a)) / (sizeof(a[0])))

typedef unsigned char u8;

typedef uint32_t u32;
typedef int32_t i32;

typedef uint16_t u16;
typedef int16_t i16;

typedef float r32;
typedef double r64;


API real_time
GetRealTime();

API real_time
GetClockResolution();

API delta_time
GetTimeDiff(real_time TimeEnd,real_time TimeStart,real_time ClockFreq);


#define PORTABLE_PLATFORM_H
#endif
