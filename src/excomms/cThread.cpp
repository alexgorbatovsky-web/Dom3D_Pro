#include "stdafx.h"

namespace cThread {

cList<ThreadHandle> DeadThreads;
static ::std::mutex dead_m;

void AddDeadThread(ThreadHandle h){
	dead_m.lock();
	DeadThreads.Add(h);
	dead_m.unlock();
	comms::cLog::Message("AddDeadThread: %d", h);
}
void RemoveDeadThread(ThreadHandle h) {
	dead_m.lock();
	for (int k = 0; k < DeadThreads.Count(); k++) {
		if (DeadThreads[k] == h) {
			DeadThreads.RemoveAt(k, 1);
			k--;
		}
	}
	dead_m.unlock();
}
void RemoveDeadThreads() {
	dead_m.lock();
	for (int i = DeadThreads.Count() - 1; i >= 0; i--) {
#ifdef COMMS_WINDOWS
		// The same as "WaitAndDeleteThread" without deadlock because
		// "WaitAndDeleteThread" calls "RemoveDeadThread" which locks the same mutex
		if (DeadThreads[i] != nullptr) {
			WaitForSingleObject(DeadThreads[i], INFINITE);
			CloseHandle(DeadThreads[i]);
			DeadThreads[i] = nullptr;
		}
#endif // Windows
#if defined COMMS_MACOS || defined COMMS_LINUX
        if(DeadThreads[i] != 0) {
            // The same as "WaitAndDeleteThread" without deadlock because
            // "WaitAndDeleteThread" calls "RemoveDeadThread" which locks the same mutex
            pthread_join(DeadThreads[i], nullptr);
            DeadThreads[i] = 0;
        }
#endif // macOS, Linux
	}
	DeadThreads.Clear();
	dead_m.unlock();
}

#ifdef COMMS_WINDOWS

// CpuCount
int CpuCount() {
	static int Cpus = -1;
	if(-1 == Cpus) {
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		Cpus = comms::cMath::Max(1, (int)si.dwNumberOfProcessors);
	}
	return Cpus;
}

// CreateThread
ThreadHandle CreateThread(ThreadProc Proc, void *Param, const bool Critical, const int Index) {
	RemoveDeadThreads();
	ThreadHandle hThread = ::CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)Proc, Param, CREATE_SUSPENDED, nullptr);
	if(Critical) {
		SetThreadPriority(hThread, THREAD_PRIORITY_TIME_CRITICAL);
	}
	ResumeThread(hThread);
	return hThread;
}
void SafeCloseHandle(HANDLE Thread) {
	try {
		CloseHandle(Thread);
	}
	catch (std::exception& e) {
		comms::cLog::Message("%s", e.what());
	}
	catch (...) {
		comms::cLog::Message("UNKNOWN exception");
	}
}
// CancelAndDeleteThread
void CancelAndDeleteThread(ThreadHandle *Thread) {
#ifdef COMMS_WINDOWS
	if(*Thread != nullptr) {
		TerminateThread(*Thread, 0);
		RemoveDeadThread(*Thread);
		CloseHandle(*Thread);
		*Thread = nullptr;
	}
#endif // COMMS_WINDOWS
}

// DeleteThread
void DeleteThread(ThreadHandle *Thread) {
	if(*Thread != nullptr) {
		RemoveDeadThread(*Thread);
		CloseHandle(*Thread);
		*Thread = nullptr;
	}
}

// WaitAndDeleteThread
void WaitAndDeleteThread(ThreadHandle *Thread) {
	if(*Thread != nullptr) {
		WaitForSingleObject(*Thread, INFINITE);
		RemoveDeadThread(*Thread);
		CloseHandle(*Thread);
		*Thread = nullptr;
	}
}

#endif // COMMS_WINDOWS

#if defined COMMS_MACOS || defined COMMS_LINUX || defined COMMS_IOS || defined COMMS_TIZEN

// CpuCount
int CpuCount() {
	static int Cpus = -1;
	if(-1 == Cpus) {
#ifdef COMMS_MACOS
		size_t s = sizeof(Cpus);
		sysctlbyname("hw.logicalcpu", &Cpus, &s, nullptr, 0);
#endif // COMMS_MACOS
#ifdef COMMS_LINUX
		Cpus = sysconf(_SC_NPROCESSORS_ONLN);
#endif // COMMS_LINUX
#if defined COMMS_IOS || defined COMMS_TIZEN
		Cpus = -1;
#endif // COMMS_IOS || COMMS_TIZEN
		Cpus = comms::cMath::Max(1, Cpus);
	}
	return Cpus;
}

// CreateThread
ThreadHandle CreateThread(ThreadProc Proc, void *Param, const bool Critical, const int Index) {
	pthread_t th;
	pthread_create(&th, nullptr, (void *(*)(void *))Proc, Param);
	return th;
}

// For every thread we should call "pthread_join" OR "pthread_detach" because resources are not freed automatically after finishing thread procedure.
// We should NOT call both "pthread_join" and "pthread_detach" because this results in "double free" crash.

// CancelAndDeleteThread
void CancelAndDeleteThread(ThreadHandle *Thread) {
	if(*Thread != 0) {
		RemoveDeadThread(*Thread);
		pthread_cancel(*Thread);
		pthread_detach(*Thread);
		*Thread = 0;
	}
}

// DeleteThread
void DeleteThread(ThreadHandle *Thread) {
	if(*Thread != 0) {
		RemoveDeadThread(*Thread);
		pthread_detach(*Thread); // Doesn't stop the thread. It just specifies the system to free resources of already finished thread OR
		// to free resources in the future after the thread will be finished. It is valid to call "pthread_t th; pthread_create(&th, nullptr, Proc, nullptr); pthread_detach(th);".
		*Thread = 0;
	}
}

// WaitAndDeleteThread
void WaitAndDeleteThread(ThreadHandle *Thread) {
	if(*Thread != 0) {
		RemoveDeadThread(*Thread);
		pthread_join(*Thread, nullptr);
		*Thread = 0;
	}
}

#endif // COMMS_MACOS || COMMS_LINUX || COMMS_IOS || COMMS_TIZEN

} // cThread
