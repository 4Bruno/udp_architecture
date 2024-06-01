
#ifndef PLATFORM_MATH_H
#define PLATFORM_MATH_H

#include "platform.h"

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

inline i32
BetweenIn(i32 val, i32 min, i32 max)
{
    i32 is_between = (val >= min && val <= max);

    return is_between;
}

inline i32
BetweenEx(i32 val, i32 min, i32 max)
{
    i32 is_between = (val > min && val < max);

    return is_between;
}


#endif
