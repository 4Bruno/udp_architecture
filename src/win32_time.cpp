#include "platform.h"
    
inline real_time
GetRealTime()
{
    real_time time;

    QueryPerformanceCounter(&time);

    return time;
}

inline real_time
GetClockResolution()
{
    real_time perf_freq;
    QueryPerformanceFrequency(&perf_freq); 
    return perf_freq;
}

/* Returns us milliseconds  */
inline delta_time
GetTimeDiff(real_time TimeEnd,real_time TimeStart,real_time ClockFreq)
{
    real_time time;

    time.QuadPart = TimeEnd.QuadPart - TimeStart.QuadPart;
    time.QuadPart *= 1000000; // microseconds 10^-6
    time.QuadPart /= ClockFreq.QuadPart;

    delta_time time_diff = (r32)((r64)time.QuadPart * (1.0 / 1000.0));

    return time_diff;
}
