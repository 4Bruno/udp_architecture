#ifndef PLATFORM_ATOMIC_H


#ifdef _WIN32
#include <windows.h>
#include <winnt.h>
#include "platform.h"
inline int AtomicLockAndExchange(volatile i32 * dest, i32 value)
{
    return InterlockedExchange((volatile long *)dest, value);
}
#elif define __linux__
inline int AtomicLockAndExchange(volatile i32 * dest, i32 value)
{
    return __sync_lock_test_and_set ((volatile long *)dest, value);
}
#else
#error Unsupported OS
#endif



#define PLATFORM_ATOMIC_H
#endif
