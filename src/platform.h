#ifndef PORTABLE_PLATFORM_H

#ifdef _WIN32
#include <windows.h>
#elif defined __linux__
#else
#error Unhandled OS
#endif

#include <stdint.h>

#define ArrayCount(a) ((sizeof(a)) / (a[0]))

typedef uint32_t u32;
typedef int32_t i32;

#define PORTABLE_PLATFORM_H
#endif
