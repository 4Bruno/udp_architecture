#ifndef PLATFORM_ATOMIC_H


#include "platform.h"

#ifdef _WIN32
#include <winnt.h>
inline int AtomicLockAndExchange(volatile i32 * dest, i32 value)
{
    return InterlockedExchange((volatile long *)dest, value);
}
#elif defined __linux__
inline int AtomicLockAndExchange(volatile i32 * dest, i32 value)
{
    return __sync_lock_test_and_set ((volatile long *)dest, value);
}
#else
#error Unsupported OS
#endif



#define PLATFORM_ATOMIC_H
#endif
