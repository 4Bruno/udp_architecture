
#ifndef PLATFORM_MULTITHREAD_H
#define PLATFORM_MULTITHREAD_H

#include "platform.h"

#define COMPILER_DONOT_REORDER_BARRIER  __faststorefence()
struct thread_work_queue;

#define THREAD_WORK_HANDLER(name) void name(thread_work_queue * Queue, void * Data)
typedef THREAD_WORK_HANDLER(thread_work_handler);

#define THREAD_ADD_WORK_TO_QUEUE(name) void name(thread_work_queue * Queue, thread_work_handler * Handler, void * Data)
typedef THREAD_ADD_WORK_TO_QUEUE(thread_add_work_to_queue);

#define THREAD_COMPLETE_QUEUE(name) void name(thread_work_queue * Queue)
typedef THREAD_COMPLETE_QUEUE(thread_complete_queue);

struct thread_work_queue_entry
{
    thread_work_handler * Handler;
    void * Data;
};

struct thread_work_queue
{
    void * Semaphore;

    volatile u32 ThingsToDo;
    volatile u32 ThingsDone;

    volatile u32 CurrentRead;
    volatile u32 CurrentWrite;

    thread_work_queue_entry Entries[256];
};


#endif
