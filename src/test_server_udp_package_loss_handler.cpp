
#include "network_udp.h"
#include "logger.h"
#include <string.h>
#include "atomic.h"
#include "test_network.h"
#include "MurmurHash3.h"
#include "protocol.h"
#include <direct.h>


static volatile int keep_alive = 1;

struct client
{
    u32 addr;
    client_status status;
    LARGE_INTEGER last_update;
    struct client * next;
    FILE * fd;
    u32 fd_entry_count;
};

struct hash_map
{
    u32 table_size;
    void * table;
    void * bucket_list;
    i32 bucket_count;
    i32 bucket_first_free;
    void * bucket_free_ll;
};

u32
ClientHashKey(u32 addr, struct hash_map * client_map)
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
AddClientLogEntry(struct client * client,log_entry * entry)
{
    client->fd_entry_count += 1;
    entry->update = GetRealTime();
    fseek(client->fd,0, SEEK_END);
    fwrite(entry,sizeof(struct log_entry) - 1,1,client->fd);
}

void
ReadLastClientEntry(struct client * client, log_entry * entry)
{
    fseek(client->fd, 0, SEEK_END);
    u32 pos = ftell(client->fd);
    fseek(client->fd, pos - (sizeof(struct log_entry) + 1), SEEK_SET);
    fread(entry, sizeof(struct log_entry),1,client->fd);
}

void
CreateClientLog(struct client * client)
{

    char addr_to_s[50];
    sprintf(addr_to_s, ".\\clients\\%i", client->addr);

    fopen_s(&client->fd,addr_to_s, "w+D");
    Assert(client->fd);

    log_entry entry;
    sprintf((char *)&entry.msg, "New entry\n");
    AddClientLogEntry(client,&entry);
}


struct client *
Client(u32 addr,struct hash_map * client_map)
{
    struct client ** ptr_client = 0;
    struct client * client = 0;
    Assert(client_map->table_size > 0);

    u32 hashkey = ClientHashKey(addr, client_map);
    ptr_client = (struct client **)client_map->table + hashkey;
    client = *ptr_client;

    while ( client )
    {
        if (client->addr == addr)
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
            client = (struct client *)client_map->bucket_free_ll;
            client_map->bucket_free_ll = client->next;
        }
        else
        {
            client = (struct client *)client_map->bucket_list + client_map->bucket_first_free;
            client_map->bucket_first_free += 1;

            if (client_map->bucket_first_free >= client_map->bucket_count)
            {
                Assert(0); // not implemented, resize hash_table
            }
        }

        client->addr = addr;
        client->next = 0;
        client->status = client_status_none;
        client->last_update = GetRealTime();

        CreateClientLog(client);

        *ptr_client = client;
    }

    return client;
}

void
RemoveClient(u32 addr, struct hash_map * client_map)
{
    u32 hashkey = ClientHashKey(addr,client_map);
    struct client ** parent = (struct client **)client_map->table + hashkey;
    Assert(parent);
    struct client * child = *parent;
    while (child->addr != addr)
    {
        parent = &child->next;
        child = child->next;
    }

    (*parent) = (*parent)->next;

    child->addr = 0;
    child->next = 0;
    if (child->fd)
    {
        fclose(child->fd);
    }

    if ( (struct client *)client_map->bucket_free_ll )
    {
        child->next = ((struct client *)client_map->bucket_free_ll)->next;
    }

    client_map->bucket_free_ll = child;
}

void
RemoveClient(struct client * client, struct hash_map * client_map)
{
    RemoveClient(client->addr, client_map);
}

#define Kilobytes(x) 1024 * x
#define Megabytes(x) 1024 * Kilobytes(x)
#define Gigabytes(x) 1024 * Megabytes(x)

#define PushStruct(arena, s) (s *)PushSize_(arena, sizeof(s))
#define PushSize(arena, s) (void*)PushSize_(arena, s)
struct memory_arena
{
    void * base;
    u32 size;
    u32 max_size;
};

void *
PushSize_(memory_arena * arena, i32 size)
{
    Assert( (arena->size + size) <= arena->max_size );
    arena->size += size;
    void * base = (char *)arena->base + arena->size;

    return base;
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
    i32 size_table = server->client_map.table_size * sizeof(struct client *);
    server->client_map.table = PushSize(&server->permanent_arena, size_table);
    memset(server->client_map.table, 0, size_table);

    r32 occupancy_ratio = 0.75f;
    server->client_map.bucket_count = (i32)(occupancy_ratio * (r32)server->client_map.table_size);
    i32 size_buckets = server->client_map.bucket_count * sizeof(struct client);
    server->client_map.bucket_list = PushSize(&server->permanent_arena, size_buckets);
    memset(server->client_map.bucket_list, 0, size_buckets);

    server->client_map.bucket_first_free = 0;
    
    return server;
}

void
ShutdownServer(struct server * server)
{
    CloseSocket(server->handle);
}

int
main()
{
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

    if (!server)
    {
        logn("Error creating server");
        return 1;
    }

    real_time clock_freq = GetClockResolution();
    _mkdir(".\\clients\\");

    server->seed = 12312312;
    srand(server->seed);

    while ( server->keep_alive )
    {
        struct packet packet_data;
        u32 max_packet_size = sizeof( packet_data );

        sockaddr_in from;
        socklen_t fromLength = sizeof( from );

        int bytes = recvfrom( server->handle, 
                (char *)&packet_data, max_packet_size, 
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
                Sleep(10);
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
            i32 lost_on_purpose = (rand() % 64) == 0;
            if (!lost_on_purpose)
            {
                struct client * client = Client(from_address, &server->client_map);
                // process received packet
#if 0
                logn("[%i.%i.%i.%i] Message (%u) received %s", 
                        (from_address >> 24),
                        (from_address >> 16)  & 0xFF0000,
                        (from_address >> 8)   & 0xFF00,
                        (from_address >> 0)   & 0xFF,
                        packet_data.seq, packet_data.msg); 
#endif

                struct packet ack;
                ack.seq = packet_data.seq;
                ack.ack = packet_data.seq;
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
                            struct udp_auth * login_data = (udp_auth *)packet_data.msg;
                            //sprintf(ack.msg, "Requires authentication. Received %s and %s", login_data->user, login_data->pwd);
                            log_entry entry;
                            sprintf(entry.msg,"user:%s, pwd:%s\n",login_data->user, login_data->pwd);
                            AddClientLogEntry(client,&entry);
                            sprintf(ack.msg, "I am checking your use/pwd");
                        } break;
                    default:
                        {
                        } break;
                }

                //sprintf(ack.msg, "I got our msg %u", packet_data.seq);
                if (SendPackage(server->handle,from, (void *)&ack, sizeof(ack)) == SOCKET_ERROR)
                {
                    logn("Error sending ack package %u. %s", ack.seq, GetLastSocketErrorMessage());
                    server->keep_alive = 0;
                }
            }
        }

        HANDLE hFind = INVALID_HANDLE_VALUE;
        WIN32_FIND_DATAA ffd;
        hFind = FindFirstFile(".\\clients\\*", &ffd);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    u32 addr = atol(ffd.cFileName);
                    struct client * client = Client(addr, &server->client_map);
                    log_entry entry;
                    ReadLastClientEntry(client, &entry);
                    delta_time dt_time = GetTimeDiff(GetRealTime(),entry.update,clock_freq);
                    logn("Time since client send msg %lli", dt_time);
                    if (dt_time > 5000)
                    {
                        logn("Removing client timeout 10s");
                        RemoveClient(from_address, &server->client_map);
                    }
                }
            } while (FindNextFile(hFind, &ffd) != 0);
        }
    }

    ShutdownServer(server);

    ShutdownSockets();

    return 0;
}
