#ifndef PORTABLE_LOGGER_H

#include "platform.h"
#include <stdio.h>

#define logn( format, ...) printf( format "\n", ## __VA_ARGS__ )

#define PORTABLE_LOGGER_H
#endif

