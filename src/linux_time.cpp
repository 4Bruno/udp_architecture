#include "platform.h"
#include <errno.h>
    
GET_REAL_TIME(GetRealTime)
{
    real_time time;

    clock_gettime(CLOCK_REALTIME,&time);

    return time;
}


GET_CLOCK_RESOLUTION(GetClockResolution)
{
    real_time perf_freq;
    clock_getres(CLOCK_REALTIME,&perf_freq); 
    return perf_freq;
}

/* Returns milliseconds */
GET_TIME_DIFF(GetTimeDiff)
{
  delta_time dt = 
    (TimeEnd.tv_sec - TimeStart.tv_sec) * 1000.0 
    + 
    (
      (TimeEnd.tv_nsec - TimeStart.tv_nsec) / 
      1000000.0
    );

  return dt;
}

MS_SLEEP(msleep)
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

HIGH_DEFINITION_TIME_BEGIN(HighDefinitionTimeBegin) {};
HIGH_DEFINITION_TIME_END(HighDefinitionTimeEnd) {};
