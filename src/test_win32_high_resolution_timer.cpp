#include "../src/logger.h"
#include "../src/atomic.h"
#pragma comment( lib, "Winmm.lib" )

#define QUAD_TO_MS(Q) Q.QuadPart * (1.0f / 1000.0f)

static i32 keep_alive = 1;

#if _WIN32
BOOL WINAPI 
HandleCtlC(DWORD dwCtrlType)
{
    BOOL signal_handled = false;

    if (dwCtrlType == CTRL_C_EVENT ||
        dwCtrlType == CTRL_BREAK_EVENT)
    {
        AtomicLockAndExchange(&keep_alive, 0);
        signal_handled = true;
    }
    else if (dwCtrlType == CTRL_CLOSE_EVENT)
    {
        signal_handled = true;
    }

    return signal_handled;
}
#endif

void
InitializeTerminateSignalHandler()
{
#ifdef _WIN32
    SetConsoleCtrlHandler(HandleCtlC, true);
#else
#endif
}

inline LARGE_INTEGER
Win32QueryPerformance()
{
    LARGE_INTEGER Performance;
    QueryPerformanceCounter(&Performance);
    return Performance;
}
/* Returns us microseconds 10^-6 */
inline LARGE_INTEGER
Win32QueryPerformanceDiff(LARGE_INTEGER TimeEnd,LARGE_INTEGER TimeStart,LARGE_INTEGER PerformanceFrequency)
{
    TimeEnd.QuadPart = TimeEnd.QuadPart - TimeStart.QuadPart;
    TimeEnd.QuadPart *= 1000000;
    TimeEnd.QuadPart /= PerformanceFrequency.QuadPart;
    return TimeEnd;
}

int
main()
{
    InitializeTerminateSignalHandler();

    LARGE_INTEGER perf_freq;

    QueryPerformanceFrequency(&perf_freq); 

    u32 packages_per_second = 4;
    r32 expected_ms_per_package = (1.0f / (r32)packages_per_second) * 1000.0f;

    // http://www.geisswerks.com/ryan/FAQS/timing.html
    // Sleep will do granular scheduling up to 1ms
    timeBeginPeriod(1);

    logn("Start timing function");

    while ( keep_alive )
    {
        LARGE_INTEGER starting_time, ending_time, delta_ms;

        starting_time = Win32QueryPerformance();

        // do work


        // sleep expected time
        LARGE_INTEGER time_frame_elapsed = 
            Win32QueryPerformanceDiff(Win32QueryPerformance(), starting_time, perf_freq);
        r32 remaining_ms = expected_ms_per_package - QUAD_TO_MS(time_frame_elapsed);

        logn("Time frame remaining %f\n",remaining_ms);

        if (remaining_ms > 1.0f)
        {
            time_frame_elapsed = Win32QueryPerformanceDiff(Win32QueryPerformance(), starting_time, perf_freq);
            remaining_ms = expected_ms_per_package - QUAD_TO_MS(time_frame_elapsed);
            while (remaining_ms > 1.0f)
            {
                Sleep((DWORD)remaining_ms);
                time_frame_elapsed = Win32QueryPerformanceDiff(Win32QueryPerformance(), starting_time, perf_freq);
                remaining_ms = expected_ms_per_package - QUAD_TO_MS(time_frame_elapsed);
            }
        }

        logn("Time frame work %f\n",(r32)(time_frame_elapsed.QuadPart * (1.0f / 1000.0f)));
    }

    logn("Ending timed function");

    timeEndPeriod(1);

    return 0;
}
