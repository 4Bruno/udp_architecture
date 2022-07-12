
#ifndef TEST_NETWORK_H

#include "platform.h"

struct packet
{
    // 4 + 4 + 4 + 4 = 16 overhead
    u32 protocol;
    u32 seq;
    u32 ack;
    u32 ack_bit;
    // UDP payload should be restricted by:
    // 576-60-8=508 bytes
    // where 576 is MTU of udp packages to not to be fragmented
    // 60 max ip payload
    // 8 udp header ( 2 bytes each => src port, dst port, length, check sum)
    // 508 - internal header (16) = 492
    char msg[492];
};


#define TEST_NETWORK_H
#endif
