
#ifndef TEST_NETWORK_H

#include "..\src\platform.h"

struct packet
{
    u32 protocol;
    u32 seq;
    u32 ack;
    u32 ack_bit;
    char msg[256];
};

struct client
{
    u32 queue_sent;
    u32 queue_received;
};


#define TEST_NETWORK_H
#endif
