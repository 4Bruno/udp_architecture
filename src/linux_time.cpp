#include "platform.h"
    
inline real_time
GetRealTime()
{
    real_time time;

    clock_gettime(CLOCK_REALTIME_HR,&time);

    return time;
}

inline real_time
GetClockResolution()
{
    real_time perf_freq;
    clock_getres(CLOCK_REALTIME_HR,&perf_freq); 
    return perf_freq;
}

/* Returns milliseconds */
inline delta_time
GetTimeDiff(real_time TimeEnd,real_time TimeStart,real_time ClockFreq)
{
    real_time time;

    time.QuadPart = TimeEnd.QuadPart - TimeStart.QuadPart;
    time.QuadPart *= 1000000; // microseconds 10^-6
    time.QuadPart /= ClockFreq.QuadPart;

    delta_time time_diff = time.QuadPart * (1.0f / 1000.0f);

    return time_diff;
}
