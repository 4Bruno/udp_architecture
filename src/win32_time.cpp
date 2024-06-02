#include "platform.h"
    
#pragma comment( lib, "Winmm.lib" )

GET_REAL_TIME(GetRealTime)
{
    real_time time;

    QueryPerformanceCounter(&time);

    return time;
}

GET_CLOCK_RESOLUTION(GetClockResolution)
{
    real_time perf_freq;
    QueryPerformanceFrequency(&perf_freq); 
    return perf_freq;
}

/* Returns us milliseconds  */
GET_TIME_DIFF(GetTimeDiff)
{
    real_time time;

    time.QuadPart = TimeEnd.QuadPart - TimeStart.QuadPart;
    time.QuadPart *= 1000000; // microseconds 10^-6
    time.QuadPart /= ClockFreq.QuadPart;

    delta_time time_diff = (r32)((r64)time.QuadPart * (1.0 / 1000.0));

    return time_diff;
}

MS_SLEEP(msleep)
{
    Sleep(msec);
    return 0;
}

HIGH_DEFINITION_TIME_BEGIN(HighDefinitionTimeBegin) 
{
    // http://www.geisswerks.com/ryan/FAQS/timing.html
    // Sleep will do granular scheduling up to 1ms
    timeBeginPeriod(1);
};
HIGH_DEFINITION_TIME_END(HighDefinitionTimeEnd) 
{
    timeEndPeriod(1);
};
