
#ifndef PLATFORM_MULTITHREAD_H
#define PLATFORM_MULTITHREAD_H

#include "platform.h"

#define COMPILER_DONOT_REORDER_BARRIER  __faststorefence()
struct thread_work_queue;

#define THREAD_WORK_HANDLER(name) void name(thread_work_queue * Queue, void * Data)
typedef THREAD_WORK_HANDLER(thread_work_handler);
API THREAD_WORK_HANDLER(ThreadWorkHandler);

#define THREAD_ADD_WORK_TO_QUEUE(name) void name(thread_work_queue * Queue, thread_work_handler * Handler, void * Data)
typedef THREAD_ADD_WORK_TO_QUEUE(thread_add_work_to_queue);
API THREAD_ADD_WORK_TO_QUEUE(ThreadAddWorkToQueue);

#define THREAD_COMPLETE_QUEUE(name) void name(thread_work_queue * Queue)
typedef THREAD_COMPLETE_QUEUE(thread_complete_queue);
API THREAD_COMPLETE_QUEUE(ThreadCompleteQueue);

#define CREATE_WORK_QUEUE(name) void name(thread_work_queue * Queue, u32 CountThreads)
typedef CREATE_WORK_QUEUE(create_work_queue);
API CREATE_WORK_QUEUE(CreateWorkQueue);

#define THREAD_WAIT_FOR_SINGLE_OBJECT(name) i32 name(void * Handle)
typedef THREAD_WAIT_FOR_SINGLE_OBJECT(thread_wait_for_single_object);
API THREAD_WAIT_FOR_SINGLE_OBJECT(ThreadWaitForSingleObject);

#define THREAD_IS_LOCK_IN_PLACE(name) b32 name(i32 WaitSignalResult)
typedef THREAD_IS_LOCK_IN_PLACE(thread_is_lock_in_place);
API THREAD_IS_LOCK_IN_PLACE(ThreadIsLockInPlace);

#define THREAD_RELEASE_MUTEX(name) i32 name(void * Handle)
typedef THREAD_RELEASE_MUTEX(thread_release_mutex);
API THREAD_RELEASE_MUTEX(ThreadReleaseMutex);

#define CREATE_MUTEX(name) void * name()
typedef CREATE_MUTEX(create_mutex);
API CREATE_MUTEX(CreateMutex);

#define COMPARE_AND_EX_IF_MATCHES(name) b32 name(volatile u32 * This, u32 UpdateTo, u32 IfMatchesThis)
typedef COMPARE_AND_EX_IF_MATCHES(compare_and_ex_if_matches);
API COMPARE_AND_EX_IF_MATCHES(CompareAndExchangeIfMatches);

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

    volatile b32 KillSignal;
};


#endif
