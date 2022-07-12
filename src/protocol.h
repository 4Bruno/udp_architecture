#ifndef UDP_PROTOCOL_H
#define UDP_PROTOCOL_H

#define PROTOCOL_ID 0b1000

enum client_status
{
    client_status_none,
    client_status_trying_auth,
    client_status_auth,
    client_status_in_game
};


struct udp_auth
{
    char user[16];
    char pwd[16];
};



#endif
