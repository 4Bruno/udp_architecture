#ifndef PORTABLE_LOGGER_H

#include "platform.h"
#include <stdio.h>

#define logn( format, ...) printf( "\r" format "\n", ## __VA_ARGS__ )
//#define logn( format, ...) 

#define PORTABLE_LOGGER_H
#endif

