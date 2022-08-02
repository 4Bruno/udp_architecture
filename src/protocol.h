#ifndef UDP_PROTOCOL_H
#define UDP_PROTOCOL_H

#include "platform.h"

#define PROTOCOL_ID 0b1000

enum client_status
{
    client_status_none,
    client_status_trying_auth,
    client_status_auth,
    client_status_in_game
};

struct packet_header
{
    u16 protocol;
    u16 messages;
    u32 seq; // you can get down to u16, the seq will circle every ~1.5h
    u32 ack;
    u32 ack_bit;
};

struct packet
{
    // 4 + 4 + 4 + 4 = 16 overhead
    packet_header header;

    // UDP payload should be restricted by:
    // 576-60-8=508 bytes
    // where 576 is MTU of udp packages to not to be fragmented
    // 60 max ip payload
    // 8 udp header ( 2 bytes each => src port, dst port, length, check sum)
    // 508 - internal header (16) = 492

    // lets round down to 32 bit boundaries from 492
    // 32 * 7 = 480
#define UDP_DATAGRAM_PAYLOAD_MAX_SIZE 508
#define PACKET_PAYLOAD_SIZE UDP_DATAGRAM_PAYLOAD_MAX_SIZE - sizeof(packet_header)
    char data[PACKET_PAYLOAD_SIZE];
};

struct message_header
{
    u8 len;
    // 8 bit is a signal if critical message (must be ack)
    u8 message_type;
    u8 id;
    // 256 (max index) * 96 b/msg * (1 / 1024 * kb/b) = 23 kb
    // if use 16 bits you can send messages of up to 6 Mb
    u8 order; 
};

// the size of message is important
// we maintain a queue of messages for every client
// which is used in the payload of the packet sent every n-frames
// small size has fragmentation and high cost of header
// big size will prevent scaling as the queue is static:
// queue of 32 * sizeof(message) * every client, it adds up
// example:
//    size message ((96 + 32 = 128)
// *  32 queue size
// ----------------
//    4096 b == 4kb
// *  10 000 clients
// ----------------
//    39 Megabytes
struct message
{
    message_header header;
#define MAX_MSG_PAYLOAD_SIZE PACKET_PAYLOAD_SIZE - sizeof(message_header)
    // this causes size to be 128
    // at most you can only fit 3 full size packages
    u8 data[96];
};

struct queue_message
{
    message messages[32];
    i32 msg_sent_in_package_bit_index[32];
    u32 begin;
    u32 next;
    i32 last_id;
};


enum package_type
{
    package_type_buffer = 1,
    package_type_auth = 2
    // should only go up to 127, we use last bit as signal for
    // critical type
};

struct udp_auth
{
    char user[16];
    char pwd[16];
};


struct udp_buffer
{
    u32 size;
};


#endif
