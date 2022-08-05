#include "network_udp.h"
#include "logger.h"
#include <string.h>
#include "atomic.h"
#include "MurmurHash3.h"
#include "protocol.h"
#include "math.h"
#include "console_sequences.cpp"
#include "math.h"

#define QUAD_TO_MS(Q) Q.QuadPart * (1.0f / 1000.0f)

/* ---------------------------- BEGIN STATIC VARIABLES ----------------------------- */
static volatile int * keep_alive = 0;
static struct console con;
/* ---------------------------- END STATIC VARIABLES ----------------------------- */

struct client_info
{
    u32 addr;
    u32 port;
    sockaddr_in addr_ip;
    client_status status;
    real_time last_update;
    real_time last_message_from_server;
    struct client_info * next;
    FILE * fd;
    u32 fd_entry_count;
    struct client_info ** entry;

    // this are the packages the server sent to client
    u32 server_packet_seq;
    u32 server_packet_seq_bit;
    u32 server_packet_seq_critical;

    // this monitor client packages received
    u32 client_remote_seq;
    u32 client_remote_seq_bit;
    queue_message queue_msg_to_send;
};

struct hash_map
{
    u32 table_size;
    void * table;
    void * bucket_list;
    i32 bucket_count;
    i32 bucket_first_free;
    void * bucket_free_ll;
    void * entries_begin;
    i32 entries_count;
};

u32
ClientHashKey(u32 addr, u32 port, struct hash_map * client_map)
{
    u32 seed = 27398;
    u32 hashkey = 0;

    MurmurHash3_x86_32(&addr,sizeof(u32),seed,&hashkey);

    hashkey = hashkey & (client_map->table_size - 1);
    return hashkey;
}

struct log_entry
{
    real_time update;
    char msg[255];
};

void
AddClientLogEntry(struct client_info * client,log_entry * entry)
{
    client->fd_entry_count += 1;
    entry->update = GetRealTime();
    fseek(client->fd,0, SEEK_END);
    fwrite(entry,sizeof(struct log_entry) - 1,1,client->fd);
}

void
ReadLastClientEntry(struct client_info * client, log_entry * entry)
{
    fseek(client->fd, 0, SEEK_END);
    u32 pos = ftell(client->fd);
    fseek(client->fd, pos - (sizeof(struct log_entry) + 1), SEEK_SET);
    fread(entry, sizeof(struct log_entry),1,client->fd);
}

void
CreateClientLog(struct client_info * client)
{

    char addr_to_s[50];
    sprintf_s(addr_to_s, ArrayCount(addr_to_s), ".\\clients\\%i_%i", client->addr, client->port);

    OpenFile(client->fd,addr_to_s, "w+");
    Assert(client->fd);

    log_entry entry;
    sprintf_s((char *)&entry.msg,ArrayCount(entry.msg), "New entry\n");
    AddClientLogEntry(client,&entry);
}


struct client_info *
Client(u32 addr, u32 port, struct hash_map * client_map)
{
    struct client_info ** ptr_client = 0;
    struct client_info * client = 0;
    Assert(client_map->table_size > 0);

    u32 hashkey = ClientHashKey(addr, port, client_map);
    ptr_client = (struct client_info **)client_map->table + hashkey;
    client = *ptr_client;

    while ( client )
    {
        if (client->addr == addr && client->port == port)
        {
            break;
        }
        ptr_client = &client->next;
        client = *ptr_client;
    }

    if (!client)
    {
        if (client_map->bucket_free_ll)
        {
            client = (struct client_info *)client_map->bucket_free_ll;
            client_map->bucket_free_ll = client->next;
        }
        else
        {
            client = (struct client_info *)client_map->bucket_list + client_map->bucket_first_free;
            client_map->bucket_first_free += 1;

            if (client_map->bucket_first_free >= client_map->bucket_count)
            {
                Assert(0); // not implemented, resize hash_table
            }
        }

        // new client
        client->addr = addr;
        client->port = port;
        client->next = 0;
        client->status = client_status_none;
        client->last_update = GetRealTime();
        client->addr_ip = CreateSocketAddress( addr , port);
        ZeroTime(client->last_message_from_server);
        client->last_message_from_server = GetRealTime();
#if 1
        client->server_packet_seq = UINT_MAX;
        client->server_packet_seq_bit = ~0;
        // none are critical
        client->server_packet_seq_critical = 0;

        client->client_remote_seq = UINT_MAX;
        client->client_remote_seq_bit = ~0;
#else
        client->server_packet_seq = UINT_MAX - 345;
        client->server_packet_seq_bit = ~0;
        client->client_remote_seq = UINT_MAX - 650;
        client->client_remote_seq_bit = ~0;
#endif
        memset(&(client->queue_msg_to_send), 0, sizeof(client->queue_msg_to_send));
        // range should fall between 0-31, 32 signal as not set
        for (u32 i = 0; 
                 i < ArrayCount(client->queue_msg_to_send.msg_sent_in_package_bit_index); 
                 ++i)
        {
            client->queue_msg_to_send.msg_sent_in_package_bit_index[i] = 32;
        }

        Assert(client_map->entries_begin);
        Assert(client_map->bucket_count >= (client_map->entries_count + 1));

        struct client_info ** entry_client = ((struct client_info **)client_map->entries_begin + client_map->entries_count++);
        client->entry = entry_client;
        *entry_client = client;

        CreateClientLog(client);

        *ptr_client = client;

        logn("New client [%i.%i.%i.%i|%i]",
                (addr >> 24),
                (addr >> 16)  & 0xFF,
                (addr >> 8)   & 0xFF,
                (addr >> 0)   & 0xFF,
                port);
    }

    return client;
}

void
RemoveClient(u32 addr, u32 port, struct hash_map * client_map)
{
    u32 hashkey = ClientHashKey(addr,port, client_map);
    struct client_info ** parent = (struct client_info **)client_map->table + hashkey;
    Assert(parent);
    struct client_info * child = *parent;
    while (child->addr != addr || child->port != port)
    {
        parent = &child->next;
        child = child->next;
    }

    (*parent) = (*parent)->next;

    logn("Removing client [%i.%i.%i.%i|%i]",
            (addr >> 24),
            (addr >> 16)  & 0xFF,
            (addr >> 8)   & 0xFF,
            (addr >> 0)   & 0xFF,
            port);

    child->addr = 0;
    child->port = 0;
    child->next = 0;

    Assert(child->entry);
    struct client_info ** entry_to_remove = child->entry;
    struct client_info ** last_entry = (struct client_info **)client_map->entries_begin + client_map->entries_count - 1;

    if ((*entry_to_remove) != (*last_entry))
    {
        struct client_info * last_client  = (*last_entry);
        last_client->entry = entry_to_remove;
        (*entry_to_remove) = last_client;
        *last_entry = 0;
    }

    client_map->entries_count -= 1;

    child->entry = 0;

    if (child->fd)
    {
        fclose(child->fd);
    }

    if ( (struct client_info *)client_map->bucket_free_ll )
    {
        child->next = ((struct client_info *)client_map->bucket_free_ll)->next;
    }

    client_map->bucket_free_ll = child;
}

void
RemoveClient(struct client_info * client, struct hash_map * client_map)
{
    RemoveClient(client->addr, client->port, client_map);
}


struct server
{
    memory_arena permanent_arena;
    memory_arena transient_arena;

    // network
    socket_handle handle;
    i32 port;

    // connections
    hash_map client_map;

    i32 keep_alive;
    u32 seed;
};


struct server *
CreateServer(memory_arena * server_arena, u32 PermanentMemorySize, u32 TransientMemorySize, i32 port)
{
    struct server * server = 0;

    Assert(server_arena);

    u32 total_mem_req = PermanentMemorySize + TransientMemorySize + sizeof(struct server);
    u32 server_capacity = (server_arena->max_size - server_arena->size);

    if (total_mem_req > server_capacity)
    {
        logn("Requested %u memory and %u is available", total_mem_req, server_capacity);
        return 0;
    }

    socket_handle handle;

    if (CreateSocketUdp(&handle) == SOCKET_ERROR)
    {
        logn("Socket error: \n%s\n%s", "CreateSocketUdp", GetLastSocketErrorMessage());
        return 0;
    }

    BOOL opt_val = TRUE;
    int opt_len = sizeof(opt_val);
    // https://docs.microsoft.com/en-us/windows/win32/winsock/using-so-reuseaddr-and-so-exclusiveaddruse
    if (setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt_val, opt_len) == SOCKET_ERROR)
    {
        logn("Socket error: \n%s\n%s", "setsockopt", GetLastSocketErrorMessage());
        return 0;
    }

    if (BindSocket(handle, port) == SOCKET_ERROR)
    {
        logn("Socket error: \n%s\n%s", "BindSocket", GetLastSocketErrorMessage());
        return 0;
    }

    if (SetSocketNonBlocking(handle) == SOCKET_ERROR)
    {
        logn("Socket error: \n%s\n%s", "SetSocketNonBlocking", GetLastSocketErrorMessage());
        return 0;
    }

    server = (struct server *)PushSize(server_arena, total_mem_req);

    server->permanent_arena.base = (char *)server + sizeof(struct server);
    server->permanent_arena.max_size = PermanentMemorySize;
    server->permanent_arena.size = 0;

    server->transient_arena.base = (char *)server->permanent_arena.base + PermanentMemorySize;
    server->transient_arena.max_size = TransientMemorySize;
    server->transient_arena.size = 0;

    server->handle = handle;
    server->port = port;
    server->keep_alive = 1;

    // must be power of 2 (x & (256 - 1)) lookup
    server->client_map.table_size = 256;
    i32 size_table = server->client_map.table_size * sizeof(struct client_info *);
    server->client_map.table = PushSize(&server->permanent_arena, size_table);
    memset(server->client_map.table, 0, size_table);

    r32 occupancy_ratio = 0.75f;
    server->client_map.bucket_count = (i32)(occupancy_ratio * (r32)server->client_map.table_size);
    i32 size_buckets = server->client_map.bucket_count * sizeof(struct client_info);
    server->client_map.bucket_list = PushSize(&server->permanent_arena, size_buckets);
    memset(server->client_map.bucket_list, 0, size_buckets);

    server->client_map.bucket_first_free = 0;

    // this is an array of pointers to entries in use
    server->client_map.entries_begin = PushArray(&server->permanent_arena, server->client_map.bucket_count, struct client_info *);
    
    return server;
}

void
ShutdownServer(struct server * server)
{
    CloseSocket(server->handle);
}

inline i32
IsSeqGreaterThan(u32 a, u32 b)
{
    const u32 half = UINT_MAX / 2;

    i32 result = 
        ((a > b) && ((a - b) <= half)) ||
        ((a < b) && ((b - a) >  half));

    return result;

}

inline i32
IsSeqGreaterThan(u16 a, u16 b)
{
    const u16 half = USHRT_MAX / 2;

    i32 result = 
        ((a > b) && ((a - b) <= half)) ||
        ((a < b) && ((b - a) >  half));

    return result;
}

b32
IsAckMessage(const u32 * packet_seq_bit, queue_message * queue, i32 index)
{
    u32 pck_bit_index = queue->msg_sent_in_package_bit_index[index];
    u32 bit_mask = ((u32)1 << pck_bit_index);

    i32 is_ack_msg = (i32)((*packet_seq_bit & bit_mask) == bit_mask);

    return is_ack_msg;
}

i32
IsCriticalMessage(message * msg)
{
    i32 is_critical = 
        (msg->header.message_type & (1 << 7)) == (1 << 7);

    return is_critical;
}

struct message *
GetNextAvailableMessageInQueue(struct queue_message * queue, const u32 * packet_seq_bit)
{
    struct message * msg = 0;
    b32 MessageIsAck = 0;

    i32 index = (queue->next++ & (ArrayCount(queue->messages) - 1));

    // this may happen if all packages are critical and none is ack
    Assert(queue->next != queue->begin);

    msg = queue->messages + index;
    MessageIsAck = IsAckMessage(packet_seq_bit, queue, index);

    if (IsCriticalMessage(msg) && !MessageIsAck)
    {
        struct message * first_ack_msg = 0;
        u32 index_next = queue->next;
        do
        {
            first_ack_msg = queue->messages + index_next;
            if (IsAckMessage(packet_seq_bit, queue, index_next))
            {
                memcpy(first_ack_msg, msg, sizeof(struct message));
                msg->header.message_type = 0;
                break;
            }
            index_next = (index_next + 1) & (ArrayCount(queue->messages) - 1);
        } while (index_next != queue->begin);

        Assert(index_next != queue->begin);
    }

    Assert(!IsCriticalMessage(msg) || MessageIsAck);

    return msg;
}

void
CreatePackages(const u32 * packet_seq_bit, struct queue_message * queue, enum package_type packet_type, const void * data, u32 size, b32 is_critical)
{
    i32 order = 0;
    i32 id = (queue->last_id++ & ((1 << 8) - 1));
    u8 critical_mask = ((u8)(is_critical ? 1 : 0) << 7);

    // create a header package so peer knows needs to create buffer
    if (size > ArrayCount(queue->messages[0].data))
    {
        struct message * msg = GetNextAvailableMessageInQueue(queue, packet_seq_bit);
        msg->header.id = id;
        msg->header.order = order;
        msg->header.message_type = package_type_buffer | critical_mask;
        msg->header.len = sizeof(struct udp_buffer);

        udp_buffer buffer_msg = { size };
        memcpy(msg->data, &buffer_msg, sizeof(udp_buffer) );

        order += 1;
    }

    while (size > 0)
    {
        u32 used_size = min(size, sizeof(queue->messages[0].data));

        struct message * msg = GetNextAvailableMessageInQueue(queue, packet_seq_bit);

        msg->header.id = id;
        msg->header.order = order;
        msg->header.message_type = packet_type | critical_mask;
        msg->header.len = used_size;

        memcpy(msg->data, data, used_size);

        size -= used_size;
        order += 1;
    }
}

void 
printBits(size_t const size, void const * const ptr)
{
    const unsigned char *b = (const unsigned char*) ptr;
    unsigned char byte;
    size_t i, j;
    
    for (i = size-1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            byte = (b[i] >> j) & 1;
            printf("%u", byte);
        }
    }
    puts("");
}



struct format_ip
{
    char ip[12 + 3 + 5 + 3 + 1];
};

format_ip
FormatIP(u32 addr, u32 port)
{ 
    struct format_ip format_ip;
    sprintf_s(format_ip.ip,ArrayCount(format_ip.ip), "[%3i.%3i.%3i.%3i|%5.5i]", 
            (addr >> 24), (addr >> 16)  & 0xFF, (addr >> 8)   & 0xFF, (addr >> 0)   & 0xFF,
            port);

    return format_ip;
}

#if _WIN32
BOOL WINAPI 
HandleCtlC(DWORD dwCtrlType)
{
    BOOL signal_handled = false;

    if (dwCtrlType == CTRL_C_EVENT ||
        dwCtrlType == CTRL_BREAK_EVENT)
    {
        AtomicLockAndExchange(keep_alive, 0);
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

void
ConsoleAddMessage(const char * format, ...)
{
    if (con.current_line > con.max_lines)
    {
        con.current_line = con.max_lines;
        printf(CSI "1S");
    }

    va_list list;
    va_start(list, format);

    int output_after_top_margin = con.current_line + con.margin_top;

    ConsoleClearLine(output_after_top_margin);
    //MoveCursorAbs(output_after_top_margin, 1);

    char out[255];
    vsprintf_s(out,ArrayCount(out), format, list);

    va_end(list);

    //vprintf(format, list);
    printf("%.*s", con.size.X - 1,out);

    con.current_line += 1;
}

inline package_type
GetMessageType(message * msg)
{
    package_type result = (package_type)((int)msg->header.message_type & ~(1 << 7));

    return result;
}

int
main()
{
    InitializeTerminateSignalHandler();

    con = CreateVirtualSeqConsole();

    if (!con.vt_enabled)
    {
        logn("Couldn't initialize console virtual seq");
        return - 1;
    }

    ConsoleAlternateBuffer();
    ConsoleClear();
    SetScrollMargin(&con, 4, 2);
    MoveCursorAbs(1,1);
    ConsoleSetTextFormat(2, Background_Black, Foreground_Green);

    if (!InitializeSockets())
    {
        logn("Error initializing sockets library. %s", GetLastSocketErrorMessage());
    }

    int port = 30000;

    memory_arena server_arena;
    server_arena.max_size = Megabytes(512);
    server_arena.base = malloc(server_arena.max_size);
    server_arena.size = 0;

    struct server * server = CreateServer(&server_arena, Megabytes(10), Megabytes(50), port);

    // TODO: debug ctrl-c stop server gracefully
    keep_alive = &server->keep_alive;

    if (!server)
    {
        logn("Error creating server");
        return 1;
    }

    real_time clock_freq = GetClockResolution();
    mkdir(".\\clients\\");

    server->seed = 12312312;
    srand(server->seed);

    u32 packages_per_second = 20;
    r32 expected_ms_per_package = (1.0f / (r32)packages_per_second) * 1000.0f;

    while ( server->keep_alive )
    {
        real_time starting_time;
        starting_time = GetRealTime();

        for (int entry_index = 0;
                 entry_index < server->client_map.entries_count;
                 ++entry_index)
        {
            struct client_info ** client_entry = (struct client_info **)server->client_map.entries_begin + entry_index;
            struct client_info * client = (*client_entry);

            u32 packet_index_to_check = ((client->server_packet_seq + 1) & (32 - 1));
            u32 bit_to_check = (1 << packet_index_to_check);
            i32 is_packet_ack = (client->server_packet_seq_bit & bit_to_check) == bit_to_check;
            i32 is_packet_critical = (client->server_packet_seq_critical & bit_to_check) == bit_to_check;

            if (!is_packet_ack)
            {
                if (is_packet_critical)
                {
                    ConsoleAddMessage("%s Package was lost! %u (critical?%s)",
                                        FormatIP(client->addr, client->port).ip ,
                                        (client->server_packet_seq - 31), 
                                        is_packet_critical ? "True" : "False");

                    queue_message * queue = &client->queue_msg_to_send;
                    struct message * msg = queue->messages + queue->next;
                    u32 packet_index = queue->msg_sent_in_package_bit_index[queue->next];

                    Assert(BetweenIn(packet_index,0,32));

                    if (
                            (packet_index == packet_index_to_check)
                            &&
                            IsCriticalMessage(msg)
                       )
                    {
                        // msg at next index needs to be re-sent. simply adv pointer
                        queue->next += 1;
                        queue->next &= (ArrayCount(queue->messages) - 1);
                    }

                    int next_index = (queue->next == queue->begin) ? queue->next + 1 : queue->next;
                    next_index &= (ArrayCount(queue->messages) - 1);

                    // corner case begin == next
                    for (u32 i  = next_index; 
                             i != queue->begin; 
                            /* in loop code */)
                    {
                        msg = queue->messages + i;
                        packet_index = queue->msg_sent_in_package_bit_index[i];
                        if (
                                (packet_index == packet_index_to_check)
                                &&
                                IsCriticalMessage(msg)
                           )
                        {
                            // we need to send it again. swap if necessary and incr next
                            if (queue->next != i)
                            {
                                struct message * next_msg = (queue->messages + queue->next);
                                memcpy(next_msg,msg, sizeof(struct message));
                                memset(msg, 0, sizeof(struct message));
                            }
                            queue->next += 1; 
                            queue->next &= (ArrayCount(queue->messages) - 1);
                        }
                        i += 1;
                        i &= (ArrayCount(queue->messages) - 1);
                    }

                }
            }
        }

        {
            struct packet recv_datagram;
            u32 max_packet_size = sizeof( recv_datagram );

            sockaddr_in from;
            socklen_t fromLength = sizeof( from );

            int bytes = recvfrom( server->handle, 
                    (char *)&recv_datagram, max_packet_size, 
                    0, 
                    (sockaddr*)&from, &fromLength );

            u32 from_address = ntohl( from.sin_addr.s_addr );
            u32 from_port = ntohs( from.sin_port );

            if ( bytes == SOCKET_ERROR )
            {
                i32 err = socket_errno;
                if (err != EWOULDBLOCK && err != WSAECONNRESET)
                {
                    logn("Error recvfrom(). %s", GetLastSocketErrorMessage());
                    return 1;
                }

                if (err == EWOULDBLOCK)
                {
                    Sleep(3);
                }
                else if (err == WSAECONNRESET)
                {
                    // conn reset in win32
                    // handle it via time out
                }

            }
            else if ( bytes == 0 )
            {
                Assert(0);
                logn("No more data. Closing.");
                //break;
            }
            else
            {
                struct client_info * client = Client(from_address, from_port, &server->client_map);

                u32 recv_packet_seq = recv_datagram.header.seq;
                u32 recv_packet_ack     = recv_datagram.header.ack;
                u32 recv_packet_ack_bit = recv_datagram.header.ack_bit;

                Assert( (client->server_packet_seq == recv_packet_ack) || IsSeqGreaterThan(client->server_packet_seq, recv_packet_ack));

                struct message * messages[8];
                i32 msg_count = recv_datagram.header.messages;
                // datagram with 492b max payload can have at most
                // 7.6 messages given that each msg has 32b header + 32b data
                Assert(recv_datagram.header.messages <= sizeof(messages));

                //i32 lost_on_purpose = (rand() % 20) == 0;
                i32 lost_on_purpose = 0;

                u32 begin_data_offset = 0;
                for (i32 msg_index = 0;
                        msg_index < msg_count;
                        ++msg_index)
                {
                    struct message ** msg = messages + msg_index;
                    *msg = (struct message *)recv_datagram.data + begin_data_offset;

                    // strip critical flag
                    (*msg)->header.message_type = GetMessageType(*msg);

                    begin_data_offset += (sizeof((*msg)->header) + (*msg)->header.len);
                }

                if (!lost_on_purpose) 
                {
                    //logn("Lost %u on purpose", recv_datagram.header.seq);
                }

                if (!lost_on_purpose)
                {
                    switch (client->status)
                    {
                        case client_status_in_game:
                            {
                            } break;
                        case client_status_auth:
                            {
                            } break;
                        case client_status_none:
                            {
                                const char reply[] = "Checking credentials";

                                for (int msg_index = 0;
                                        msg_index < msg_count;
                                        ++msg_index)
                                {
                                    struct message * msg = messages[msg_index];
                                    if (msg->header.message_type == package_type_auth)
                                    {
                                        Assert(msg->header.len == sizeof(udp_auth));
                                        struct udp_auth * login_data = (udp_auth *)msg->data;

                                        log_entry entry;
                                        sprintf_s(entry.msg, ArrayCount(entry.msg),"user:%s, pwd:%s\n",login_data->user, login_data->pwd);
                                        AddClientLogEntry(client,&entry);

                                        client->status = client_status_trying_auth;

//#pragma GCC diagnostic ignored "-Wcast-qual"
                                        CreatePackages(&client->server_packet_seq_bit,
                                                       &client->queue_msg_to_send, 
                                                       package_type_auth, 
                                                       (const void *)reply, sizeof(reply), 
                                                       true);

                                        break;
                                    }
                                }
                            } break;
                        case client_status_trying_auth:
                            {
                            };
                        default:
                            {
                            } break;
                    }

                    /* SYNC INCOMING PACKAGE SEQ WITH OUR RECORDS */
                    // unless crafted package, recv package should always be higher than our record
                    Assert(IsSeqGreaterThan(recv_packet_seq,client->client_remote_seq));

                    // TODO: what if we lose all packages for 1 > s?
                    // should we simply reset to 0 all bits and move on
                    // TEST THIS
                    // int on purpose check abs (to look at no branching method)
                    // https://graphics.stanford.edu/~seander/bithacks.html#IntegerAbs
                    i32 delta_local_remote_seq = abs((i32)(recv_packet_seq - client->client_remote_seq));
                    Assert(delta_local_remote_seq < 32);

                    u32 bit_mask = 0;
                    u32 remote_bit_index = (recv_packet_seq & 31);

                    // if delay is beyond 1 s we nullify our bit array
                    if (delta_local_remote_seq < 32)
                    {
                        u32 local_bit_index = (client->client_remote_seq & 31);

                        u32 lo = min(remote_bit_index, local_bit_index);
                        u32 hi = max(remote_bit_index, local_bit_index);
                        u32 max_minus_hi = (31 - hi);

                        bit_mask = ((u32)~0 << (lo + max_minus_hi)) >> max_minus_hi;

                        //     hi        low 
                        //      v         v
                        // 00000111111111110000
                        if (remote_bit_index >= local_bit_index)
                        {
                            //     hi        low 
                            //      v         v
                            // 11111000000000001111
                            bit_mask = ~bit_mask; 
                        }

                        bit_mask = bit_mask ^ ((u32)1 << lo);
                    }

                    client->client_remote_seq_bit = (client->client_remote_seq_bit & bit_mask) | ((u32)1 << remote_bit_index);
                    client->client_remote_seq = recv_packet_seq;
                }

                /* UPDATE OUR BIT ARRAY OF PACKAGES SENT CONFIRMED BY PEER */

                {
                    // unless crafted package, ack package should refer to lower
                    Assert(
                            (client->server_packet_seq == recv_packet_ack) || 
                            IsSeqGreaterThan(client->server_packet_seq,recv_packet_ack)
                            );
                    u32 delta_seq_and_ack = (client->server_packet_seq - recv_packet_ack);
                    u32 bit_mask = 0;

                    if (delta_seq_and_ack < 32)
                    {
                        u32 remote_bit_index = (recv_packet_ack & 31);
                        u32 local_bit_index = (client->server_packet_seq & 31);

                        u32 lo = min(remote_bit_index, local_bit_index);
                        u32 hi = max(remote_bit_index, local_bit_index);
                        u32 max_minus_hi = (31 - hi);

                        bit_mask = ((u32)~0 << (lo + max_minus_hi)) >> max_minus_hi;

                        //     hi        low 
                        //      v         v
                        // 00000111111111110000
                        if (local_bit_index >= remote_bit_index)
                        {
                            //     hi        low 
                            //      v         v
                            // 11111000000000001111
                            bit_mask = ~bit_mask; 
                        }

                        bit_mask = bit_mask ^ ((u32)1 << lo);
                    }

                    client->server_packet_seq_bit = (recv_packet_ack_bit & bit_mask);

                    client->last_update = GetRealTime();
                }
            }
        }

#if 1
        int entries_to_debug_display = max(server->client_map.entries_count, 5);
        for (int entry_index = 0;
                 entry_index < entries_to_debug_display;
                 ++entry_index /* decrement if client removed */)
        {
            struct client_info ** client_entry = (struct client_info **)server->client_map.entries_begin + entry_index;
            int line = (con.margin_top == 0 ? 1 : con.margin_top) + entry_index;
            ConsoleClearLine(line);
            printf("[%i] ptr: %p", entry_index , (void *)*client_entry);
            if (*client_entry)
            {
                printf("; Client %s", FormatIP((*client_entry)->addr, (*client_entry)->port).ip);
            }
        }
#endif

        for (int entry_index = 0;
                 entry_index < server->client_map.entries_count;
                 ++entry_index /* decrement if client removed */)
        {
            struct client_info ** client_entry = (struct client_info **)server->client_map.entries_begin + entry_index;
            struct client_info * client = (*client_entry);

            delta_time dt_time = GetTimeDiff(GetRealTime(),client->last_update,clock_freq);

            //logn("Time since client send msg %f", dt_time);

            if (dt_time > 5000)
            {
                logn("Removing client timeout 10s");
                RemoveClient(client, &server->client_map);
                entry_index -= 1;
            }

            //logn("Diff client last msg (%f - %f) = %f",QUAD_TO_MS(GetRealTime()),QUAD_TO_MS(client->last_message_from_server),GetTimeDiff(GetRealTime(), client->last_message_from_server,clock_freq));

            if (GetTimeDiff(GetRealTime(), client->last_message_from_server,clock_freq) > expected_ms_per_package)
            {

                // signal next seq package as not received
                client->server_packet_seq += 1;
                u32 new_package_bit_index = (client->server_packet_seq & 31);
                client->server_packet_seq_bit = 
                    (client->server_packet_seq_bit & ~(1 << new_package_bit_index));

                struct packet packet;
                packet.header.seq       = client->server_packet_seq;
                packet.header.ack       = client->client_remote_seq;
                packet.header.ack_bit   = client->client_remote_seq_bit;
                packet.header.protocol  = PROTOCOL_ID;
                packet.header.messages  = 0;

                i32 is_critical = 0;
                u32 current_size = 0;
                queue_message * queue = &client->queue_msg_to_send;
                for (u32 i = queue->begin; 
                        i != queue->next; 
                        /* in loop code */)
                {
                    struct message * msg = queue->messages + i;
                    u32 msg_size = msg ->header.len + sizeof(message_header);
                    u32 size_after_msg = (current_size + msg_size);

                    if ( size_after_msg <= sizeof(packet.data) )
                    {
                        memcpy(packet.data + current_size, msg, msg_size);

                        is_critical = is_critical | IsCriticalMessage(msg);
                        queue->msg_sent_in_package_bit_index[i] = new_package_bit_index;

                        current_size = size_after_msg;
                        queue->begin += 1; 
                        queue->begin &= (ArrayCount(queue->messages) - 1);
                        packet.header.messages += 1;
                        if (current_size >= sizeof(packet.data))
                        {
                            break;
                        }
                    }

                    i += 1;
                    i &= (ArrayCount(queue->messages) - 1);
                }

                client->server_packet_seq_critical = (client->server_packet_seq_critical & (~((u32)1 << new_package_bit_index)));
                client->server_packet_seq_critical = 
                    client->server_packet_seq_critical | 
                    ( (is_critical ? 1 : 0) << new_package_bit_index );

                if (SendPackage(server->handle,client->addr_ip, (void *)&packet, sizeof(packet)) == SOCKET_ERROR)
                {
                    logn("Error sending ack package %u. %s", packet.header.seq, GetLastSocketErrorMessage());
                    server->keep_alive = 0;
                }
                
                client->last_message_from_server = GetRealTime();
            }
        }
    }

    if (con.vt_enabled)
    {
        ConsoleExitAlternateBuffer();
    }


    ShutdownServer(server);

    ShutdownSockets();

    return 0;
}
