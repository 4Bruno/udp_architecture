#include "platform.h"
#include <errno.h>
    
real_time
GetRealTime()
{
    real_time time;

    clock_gettime(CLOCK_REALTIME,&time);

    return time;
}

real_time
GetClockResolution()
{
    real_time perf_freq;
    clock_getres(CLOCK_REALTIME,&perf_freq); 
    return perf_freq;
}

/* Returns milliseconds */
delta_time
GetTimeDiff(real_time TimeEnd,real_time TimeStart,real_time ClockFreq)
{
  delta_time dt = 
    (TimeEnd.tv_sec - TimeStart.tv_sec) + 
    (
      (TimeEnd.tv_nsec - TimeStart.tv_nsec) / 
      1000000.0
    );

  return dt;
}

int msleep(u32 msec)
{
  struct timespec ts;
  int res;

  if (msec < 0)
  {
    errno = EINVAL;
    return -1;
  }

  ts.tv_sec = msec / 1000;
  ts.tv_nsec = (msec % 1000) * 1000000;

  do {
    res = nanosleep(&ts, &ts);
  } while (res && errno == EINTR);

  return res;
}
