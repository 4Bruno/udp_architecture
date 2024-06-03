#include "multithread.h"
    
inline API COMPARE_AND_EX_IF_MATCHES(CompareAndExchangeIfMatches)
{
    u32 OriginalValue = 
        InterlockedCompareExchange(
            (LONG volatile *)This,
            UpdateTo,
            IfMatchesThis);

    b32 Result = (IfMatchesThis == OriginalValue);

    return Result;
}

b32
DoWorkOnQueue(thread_work_queue * Queue)
{
    b32 GoToSleep = false;

    u32 CurrentRead = Queue->CurrentRead;
    u32 NextReadEntry = (CurrentRead + 1) % ArrayCount(Queue->Entries);

    if (CurrentRead != Queue->CurrentWrite)
    {
        if (CompareAndExchangeIfMatches(&Queue->CurrentRead, NextReadEntry, CurrentRead))
        {
            thread_work_queue_entry Entry = Queue->Entries[CurrentRead];
            Entry.Handler(Queue, Entry.Data);
            InterlockedIncrement((LONG volatile *)&Queue->ThingsDone);
        }
    }
    else
    {
        GoToSleep = true;
    }


    return GoToSleep;
}

THREAD_COMPLETE_QUEUE(ThreadCompleteQueue)
{
    while (Queue->ThingsToDo != Queue->ThingsDone)
    {
        DoWorkOnQueue(Queue);
    }
    Queue->ThingsToDo = 0;
    Queue->ThingsDone = 0;
}

THREAD_ADD_WORK_TO_QUEUE(ThreadAddWorkToQueue)
{
    u32 NextWriteEntry = (Queue->CurrentWrite + 1) % ArrayCount(Queue->Entries);
    Assert(NextWriteEntry != Queue->CurrentRead);

    thread_work_queue_entry * Entry = (Queue->Entries + Queue->CurrentWrite);
    Entry->Handler = Handler;
    Entry->Data = Data;

    MemoryBarrier();
    Queue->CurrentWrite = NextWriteEntry;
    ++Queue->ThingsToDo;

    ReleaseSemaphore(Queue->Semaphore, 1, 0);
}

DWORD WINAPI 
WorkQueueThreadLoop(void * Parameter)
{
    thread_work_queue * Queue = (thread_work_queue *)Parameter;
    for (;;)
    {
        if (DoWorkOnQueue(Queue))
        {
            WaitForSingleObjectEx(Queue->Semaphore, INFINITE, FALSE);
        }
    }
}

void
CleanWorkQueue(thread_work_queue * Queue)
{
    if (Queue->Semaphore)
    {
        CloseHandle(Queue->Semaphore);
        Queue->Semaphore = 0;
    }
    Queue->CurrentWrite = 0;
    Queue->CurrentRead = 0;

    Queue->ThingsDone = 0;
    Queue->ThingsToDo = 0;
}

void
CreateWorkQueue(thread_work_queue * Queue, u32 CountThreads)
{
    SECURITY_ATTRIBUTES * NullSecAttrib = 0;
    const char * NullName               = 0;
    DWORD NullReservedFlags             = 0;
    i32 SizeStack                     = Megabytes(1);
    DWORD RunInmediate                  = 0;
    DWORD ThreadID;

    CleanWorkQueue(Queue);
    void * ThreadParam = (void *)Queue;

    DWORD DesiredAccess = SEMAPHORE_ALL_ACCESS; 
    HANDLE Semaphore = CreateSemaphoreExA( NullSecAttrib, 0, CountThreads, NullName, NullReservedFlags, DesiredAccess);
    Queue->Semaphore = Semaphore;

    for (u32 ThreadIndex = 0;
                ThreadIndex < CountThreads;
                ++ThreadIndex)
    {
        HANDLE T = CreateThread(NullSecAttrib, SizeStack, 
                                WorkQueueThreadLoop, ThreadParam, RunInmediate,
                                &ThreadID);
        Assert(T);
        // Close right away. 2 things need to happen to clean up thread:
        // 1) handle is closed
        // 2) thread function reaches end
        CloseHandle(T);
    }
}


CREATE_MUTEX(CreateMutex)
{
    HANDLE h = CreateMutex( 
            NULL,              // default security attributes
            FALSE,             // initially not owned
            NULL);             // unnamed mutex
 
    if (h == NULL) 
    {
        printf("CreateMutex error: %lu\n", GetLastError());
        return 0;
    }

    return h;
}


THREAD_WAIT_FOR_SINGLE_OBJECT(ThreadWaitForSingleObject)
{
    i32 WaitResult = WaitForSingleObject( Handle,INFINITE);

    return WaitResult;
}

THREAD_RELEASE_MUTEX(ThreadReleaseMutex)
{
    i32 Result = ReleaseMutex(Handle);

    return Result;
}

API THREAD_IS_LOCK_IN_PLACE(ThreadIsLockInPlace)
{
    b32 result = (WaitSignalResult == WAIT_OBJECT_0);
    return result;
}
