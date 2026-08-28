#pragma once

//-----------------------------------------------------------------------------
// cThread
//-----------------------------------------------------------------------------
namespace cThread {
	
typedef void (*ThreadProc)(void *);

#ifdef COMMS_WINDOWS
typedef HANDLE ThreadHandle;
#else // !COMMS_WINDOWS
#include <pthread.h>
typedef pthread_t ThreadHandle;
#endif // COMMS_WINDOWS

int CpuCount();
void AddDeadThread(ThreadHandle h);
void RemoveDeadThreads();
void RemoveDeadThread(ThreadHandle h);
ThreadHandle CreateThread(ThreadProc Proc, void *Param, const bool Critical = false, const int Index = 0);
void DeleteThread(ThreadHandle *Thread);
void WaitAndDeleteThread(ThreadHandle *Thread);
void CancelAndDeleteThread(ThreadHandle *Thread);

}; // cThread
