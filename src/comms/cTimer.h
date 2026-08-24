#pragma once

//*****************************************************************************
// cTimer
//*****************************************************************************
/*
	// Using cTimer for profiling
	static cTimer T(100);
	...
	T.Begin();
	... // Code to profile
	if(T.End()) {
		... // Results are ready through "T.MeanTimeMs" and "T.MeanTimeSec"
	}
*/

class cTimer {
public:
	static void Acquire();
	
	static float GetTimeSec(); // Current application time
	static float GetTimeMs();
	
	static float GetFrameTimeSec(); // Time since previous frame
	static float GetFrameTimeMs();

	// Only for input and system menu
	static float GetSystemTimeSec();
	static float GetSystemFrameTimeSec();
	
	static const cStr & GetLocalDate();
	static const cStr & GetLocalTime();
	
	// Frames per second value is based on real clock time regardless time scale value.
	// If result is 0.0f, there are insufficient frames rendered (calls to "Acquire")
	// to gather average frames per second during previous "cTimer_FpsCycleLength" frames.
	static float GetFramesPerSecond();
	
	// Resets gathered time values and frames per second stats
	static void Reset();

	// Accessing system clock (actually there is no need to call them directly)
	static double AcquireClockSeconds();
	static void AcquireDateTime(cStr *Date, cStr *Time);
	static dword AcquireClockTicks();

	static float GetTimeScale();
	static void SetTimeScale(const float TimeScale);
	
    /**
     \brief Acquires time since epoch
     \details
     Under Windows and macOS acquires time since 1 January 1970
     */
	static qword AcquireChronoMs() {
		return ::std::chrono::duration_cast<::std::chrono::milliseconds>(::std::chrono::system_clock::now().time_since_epoch()).count();
	}

	/**
	\brief Profiling by measuring mean time between "Begin" and "End" calls
	\details
		cTimer T(10);
		for (int j = 0; j < 10; j++) {
			T.Begin();
			for (int i = 0; i < 100; i++) {
				cThread::SleepMs(10);
			}
			if (T.End()) {
				float Sec = T.MeanTimeSec; // ~1 sec
				float Ms = T.MeanTimeMs; // ~1000 ms
			}
		}
	*/
	cTimer(const int MeasuringCyclesCount = 1);
	void Begin();
	bool End(); // Returns "true" when periodical measuring cycles have finished and the fields "MeanTimeMs/Sec" have been updated

	float MeanTimeMs;
	float MeanTimeSec;
	/// Formats the mean time as string "HH:MM:SS" (hours, minutes, seconds)
	void GetMeanTime_HH_MM_SS(cStr* Time) {
		int SS = (int)MeanTimeSec;
		int HH = SS / 3600;
		SS -= HH * 3600;
		int MM = SS / 60;
		SS -= MM * 60;
		*Time = cStr::ToString(HH, 2) + ":" + cStr::ToString(MM, 2) + ":" + cStr::ToString(SS, 2);
	}
private:
	bool m_InsideBeginEnd;
	int m_MeasuringCyclesCount;
	::std::chrono::time_point<::std::chrono::system_clock> m_BeginTime;
	cList<double> m_TimeTableSec;
};

// cPause
class cPause {
public:
	static void SetSystemPause(const bool SystemPause);
	static void SetUserPause(const bool UserPause);
	static void ManuallyDisablePause(const bool Disable = true);
	
	static bool GetPause();
	static bool GetUserPause();
	static bool GetSystemPause();
};
