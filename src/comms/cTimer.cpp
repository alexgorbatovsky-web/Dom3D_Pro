#include "comms.h"

namespace comms {

static double cTimer_ClockTimeSecPreviousQuery = 0.0;
static double cTimer_DoubleTimeSec = 0.0;
static float cTimer_TimeSec = 0.0f;
static float cTimer_TimeMs = 0.0f;
static float cTimer_FrameTimeSec = 0.0f;
static float cTimer_FrameTimeMs = 0.0f;
static double cTimer_DoubleSystemTimeSec = 0.0;
static float cTimer_SystemTimeSec = 0.0f;
static float cTimer_SystemFrameTimeSec = 0.0f;
static float cTimer_TimeScale = 1.0f;

static double cTimer_FpsCycleStartTimeSec = 0.0;
static double cTimer_FpsCyclePrevTimeSec = 0.0;
static int cTimer_FpsCycleFrame = -1;
static float cTimer_FramesPerSecond = 0.0f;

static cStr cTimer_LocalDate;
static cStr cTimer_LocalTime;

static const int cTimer_FpsCycleLength = 100;
static bool cPause_Disabled = false;

//-----------------------------------------------------------------------------
// cTimer::Reset
//-----------------------------------------------------------------------------
void cTimer::Reset() {
	cTimer_ClockTimeSecPreviousQuery = 0.0;
	cTimer_DoubleTimeSec = 0.0;
	cTimer_TimeSec = 0.0f;
	cTimer_TimeMs = 0.0f;
	cTimer_FrameTimeSec = 0.0f;
	cTimer_FrameTimeMs = 0.0f;
	cTimer_DoubleSystemTimeSec = 0.0;
	cTimer_SystemTimeSec = 0.0f;
	cTimer_SystemFrameTimeSec = 0.0f;

	cTimer_FpsCycleStartTimeSec = 0.0;
	cTimer_FpsCycleFrame = -1;
	cTimer_FramesPerSecond = 0.0f;

	cInput::StopVibrations();
} // cTimer::Reset

//-----------------------------------------------------------------------------
// cTimer::Acquire
//-----------------------------------------------------------------------------
void cTimer::Acquire() {
	AcquireDateTime(&cTimer_LocalDate, &cTimer_LocalTime);

	double ClockTimeSec = AcquireClockSeconds();

	if(0.0 == cTimer_ClockTimeSecPreviousQuery) { // First acquire since application start or timer reset
		cTimer_ClockTimeSecPreviousQuery = ClockTimeSec;
	}
	
	// System Frame Time
	if(cPause::GetSystemPause()) {
		cTimer_ClockTimeSecPreviousQuery = ClockTimeSec;
	}
	double ClockFrameDeltaSec = (ClockTimeSec - cTimer_ClockTimeSecPreviousQuery) * (double)cTimer_TimeScale;
	if(ClockFrameDeltaSec < 0.0) {
		ClockFrameDeltaSec = 0.0; // Sometimes in "Release" under Linux this value is negative
	}
	cTimer_SystemFrameTimeSec = float(ClockFrameDeltaSec);
	cTimer_DoubleSystemTimeSec += ClockFrameDeltaSec;
	cTimer_SystemTimeSec = float(cTimer_DoubleSystemTimeSec);

	// App Frame Time
	if(cPause::GetPause()) {
		cTimer_ClockTimeSecPreviousQuery = ClockTimeSec;
	}
	ClockFrameDeltaSec = (ClockTimeSec - cTimer_ClockTimeSecPreviousQuery) * (double)cTimer_TimeScale;
	if(ClockFrameDeltaSec < 0.0) {
		ClockFrameDeltaSec = 0.0; // Sometimes in "Release" under Linux this value is negative
	}
	cTimer_ClockTimeSecPreviousQuery = ClockTimeSec;
	
	cTimer_FrameTimeSec = float(ClockFrameDeltaSec);
	cTimer_FrameTimeMs = float(1000.0 * ClockFrameDeltaSec);
	
	cTimer_DoubleTimeSec += ClockFrameDeltaSec;
	cTimer_TimeSec = float(cTimer_DoubleTimeSec);
	cTimer_TimeMs = float(1000.0 * cTimer_DoubleTimeSec);

        // Frames per second
	if(-1 == cTimer_FpsCycleFrame) { // Starting first cycle since application start or timer reset
		cTimer_FpsCycleStartTimeSec = cTimer_FpsCyclePrevTimeSec = AcquireClockSeconds(); // Using real time
		cTimer_FpsCycleFrame = 0; // Starting frame number 0
	} else if(cPause::GetPause()) { // Disable frames per second measurement during pause
		const double ClockRealTimeSec = AcquireClockSeconds();
		cTimer_FpsCycleStartTimeSec = ClockRealTimeSec - (cTimer_FpsCyclePrevTimeSec - cTimer_FpsCycleStartTimeSec);
		cTimer_FpsCyclePrevTimeSec = ClockRealTimeSec;
	} else {
		cTimer_FpsCycleFrame++;

		const double ClockRealTimeSec = AcquireClockSeconds();
		cTimer_FpsCyclePrevTimeSec = ClockRealTimeSec;
		
		if(cTimer_FpsCycleFrame == cTimer_FpsCycleLength) {
			// Cycle is up
			const double CycleTimeSec = ClockRealTimeSec - cTimer_FpsCycleStartTimeSec;
			cTimer_FramesPerSecond = float(double(cTimer_FpsCycleFrame) / CycleTimeSec);
#ifndef COMMS_3DCOAT
            const cStr T = cMain_Title + " | " + cStr::ToString(cTimer_FramesPerSecond, 0) + " FPS";
			cMain_SetWindowTitle(T);
#endif // !3DCoat

			// Starting consequent cycle 
			cTimer_FpsCycleStartTimeSec = ClockRealTimeSec;
			cTimer_FpsCycleFrame = 0;
		}
	}
} // cTimer::Acquire

// cTimer::GetTimeSec
float cTimer::GetTimeSec() {
	return cTimer_TimeSec;
}

// cTimer::GetTimeMs
float cTimer::GetTimeMs() {
	return cTimer_TimeMs;
}

// cTimer::GetFrameTimeSec
float cTimer::GetFrameTimeSec() {
	return cTimer_FrameTimeSec;
}

// cTimer::GetFrameTimeMs
float cTimer::GetFrameTimeMs() {
	return cTimer_FrameTimeMs;
}

// cTimer::GetSystemTimeSec
float cTimer::GetSystemTimeSec() {
	return cTimer_SystemTimeSec;
}

// cTimer::GetSystemFrameTimeSec
float cTimer::GetSystemFrameTimeSec() {
	return cTimer_SystemFrameTimeSec;
}

// cTimer::GetFramesPerSecond
float cTimer::GetFramesPerSecond() {
	return cTimer_FramesPerSecond;
}

// cTimer::GetLocalDate
const cStr & cTimer::GetLocalDate() {
	return cTimer_LocalDate;
}

// cTimer::GetLocalTime
const cStr & cTimer::GetLocalTime() {
	return cTimer_LocalTime;
}

// cTimer::GetTimeScale
float cTimer::GetTimeScale() {
	return cTimer_TimeScale;
}

// cTimer::SetTimeScale
void cTimer::SetTimeScale(const float TimeScale) {
	cTimer_TimeScale = TimeScale;
}

//-----------------------------------------------------------------------------
// cTimer::AcquireClockSeconds
//-----------------------------------------------------------------------------
double cTimer::AcquireClockSeconds() {
#if defined COMMS_WINDOWS
	LARGE_INTEGER liCounter;
	LARGE_INTEGER liFrequency;
	QueryPerformanceCounter(&liCounter);
	QueryPerformanceFrequency(&liFrequency);
	
	return (double)liCounter.QuadPart / (double)liFrequency.QuadPart;
#endif // COMMS_WINDOWS

#if defined COMMS_MACOS || defined COMMS_LINUX || defined COMMS_IOS || defined COMMS_TIZEN
	timeval t;
	gettimeofday(&t, nullptr);
	return (double)t.tv_sec + 0.000001 * (double)t.tv_usec;
#endif // COMMS_MACOS || COMMS_LINUX || COMMS_IOS || COMMS_TIZEN
} // cTimer::AcquireClockSeconds

// cTimer::AcquireClockTicks
dword cTimer::AcquireClockTicks() {
#ifdef COMMS_WINDOWS
	return GetTickCount();
#else // Linux, OS X
	static timeval t0 = { 0, 0 };
	if(0 == t0.tv_sec) {
		gettimeofday(&t0, nullptr);
	}
	timeval t1;
	gettimeofday(&t1, nullptr);
	timeval t3 = { t1.tv_sec - t0.tv_sec, t1.tv_usec - t0.tv_usec };
	return (dword)((t3.tv_sec * 1000) + (t3.tv_usec / 1000));
#endif // COMMS_WINDOWS
}

//-----------------------------------------------------------------------------
// cTimer.ctor
//-----------------------------------------------------------------------------
cTimer::cTimer(const int MeasuringCyclesCount) {
	m_InsideBeginEnd = false;

	MeanTimeMs = 0.0f;
	MeanTimeSec = 0.0f;

	cAssert(MeasuringCyclesCount >= 1);
	m_MeasuringCyclesCount = cMath::Max(1, MeasuringCyclesCount);
} // cTimer.ctor

//-----------------------------------------------------------------------------
// cTimer::Begin
//-----------------------------------------------------------------------------
void cTimer::Begin() {
	cAssertM(!m_InsideBeginEnd, "\"Begin\" should be called once before \"End\"");
	if(m_InsideBeginEnd) {
		return;
	}
	m_InsideBeginEnd = true;

	m_BeginTime = ::std::chrono::system_clock::now();
} // cTimer::Begin

//-----------------------------------------------------------------------------
// cTimer::End
//-----------------------------------------------------------------------------
bool cTimer::End() {
	cAssertM(m_InsideBeginEnd, "\"End\" should be called once after \"Begin\"");
	if(!m_InsideBeginEnd) {
		return false;
	}
	m_InsideBeginEnd = false;

	::std::chrono::time_point<::std::chrono::system_clock> EndTime = ::std::chrono::system_clock::now();
	::std::chrono::duration<double> PeriodTimeSec = EndTime - m_BeginTime;
	
	m_TimeTableSec.Add(PeriodTimeSec.count());
	if(m_TimeTableSec.Count() < m_MeasuringCyclesCount) {
		return false;
	}
	
	// Calc mean time (sec)
	double Sec = 0.0;
	int i;
	for(i = 0; i < m_TimeTableSec.Count(); i++) {
		Sec += m_TimeTableSec[i];
	}
	Sec /= double(m_TimeTableSec.Count());
	MeanTimeSec = float(Sec);
	
	// Set mean time (ms)
	MeanTimeMs = float(1000.0 * Sec);
	
	m_TimeTableSec.Clear();
	return true;
} // cTimer::End

//-----------------------------------------------------------------------------
// cTimer::AcquireDateTime
//-----------------------------------------------------------------------------
void cTimer::AcquireDateTime(cStr *Date, cStr *Time) {
	static const char DaysOfWeek[7][10] = {
		"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
	};
	static const char MonthsOfYear[12][10] = {
		"January", "February", "March", "April", "May", "June", "July",
		"August", "September", "October", "November", "December"
	};

	int hDay, lDay, DayOfWeek;

#if defined COMMS_WINDOWS
	SYSTEMTIME ST;
	::GetLocalTime(&ST);

	// Time
	Time->Clear();
	Time->Append((char)(ST.wHour / 10 + '0'));
	Time->Append((char)(ST.wHour % 10 + '0'));
	Time->Append(':');
	Time->Append((char)(ST.wMinute / 10 + '0'));
	Time->Append((char)(ST.wMinute % 10 + '0'));
	Time->Append(':');
	Time->Append((char)(ST.wSecond / 10 + '0'));
	Time->Append((char)(ST.wSecond % 10 + '0'));
	
	// Date
	hDay = ST.wDay / 10;
	lDay = ST.wDay % 10;
	DayOfWeek = ST.wDayOfWeek != 0 ? ST.wDayOfWeek - 1 : 6;
	
	*Date = cStr::Format("%s, %d%d %s %d", DaysOfWeek[DayOfWeek], hDay, lDay, MonthsOfYear[ST.wMonth - 1], ST.wYear);
#endif // COMMS_WINDOWS

#if defined COMMS_MACOS || defined COMMS_LINUX || defined COMMS_IOS || defined COMMS_TIZEN
	time_t RawTime;
	struct tm *TimeInfo;
	
	time(&RawTime );
	TimeInfo = localtime(&RawTime);
	
	// Time
	Time->Clear();
	Time->Append((char)(TimeInfo->tm_hour / 10 + '0'));
	Time->Append((char)(TimeInfo->tm_hour % 10 + '0'));
	Time->Append(':');
	Time->Append((char)(TimeInfo->tm_min / 10 + '0'));
	Time->Append((char)(TimeInfo->tm_min % 10 + '0'));
	Time->Append(':');
	Time->Append((char)(TimeInfo->tm_sec / 10 + '0'));
	Time->Append((char)(TimeInfo->tm_sec % 10 + '0'));

	// Date
	hDay = TimeInfo->tm_mday / 10;
	lDay = TimeInfo->tm_mday % 10;
	DayOfWeek = TimeInfo->tm_wday != 0 ? TimeInfo->tm_wday - 1 : 6;

	*Date = cStr::Format("%s, %d%d %s %d", DaysOfWeek[DayOfWeek], hDay, lDay,
		MonthsOfYear[TimeInfo->tm_mon], TimeInfo->tm_year + 1900);
#endif // COMMS_MACOS || COMMS_LINUX || COMMS_IOS || COMMS_TIZEN
} // cTimer::AcquireDateTime
	
static bool cPause_SystemPause = false;
static bool cPause_UserPause = false;
	
// cPause::GetPause
bool cPause::GetPause() {
	if (cPause_Disabled)return false;
	return cPause_SystemPause || cPause_UserPause;
}
	
// cPause::GetUserPause
bool cPause::GetUserPause() {
	return cPause_UserPause;
}
	
// cPause::GetSystemPause
bool cPause::GetSystemPause() {
	if (cPause_Disabled)return false;
	return cPause_SystemPause;
}
	
// cPause::SetSystemPause
void cPause::SetSystemPause(const bool SystemPause) {
	static bool _first_time = true;
	if(SystemPause && _first_time) {
		_first_time = false;
		return;
	}
	_first_time = false;

	cLog::Message("SetSystemPause: %d", int(SystemPause));
	bool WasPaused = GetPause();
	if(cPause_SystemPause && !SystemPause) {
		cTimer::Acquire();
	}
	cPause_SystemPause = SystemPause;
	bool IsPaused = GetPause();
	if(WasPaused != IsPaused) {
		if(IsPaused) {
			cInput::StopVibrations();
		}
	}
}
	
// cPause::SetUserPause
void cPause::SetUserPause(const bool UserPause) {
	bool WasPaused = GetPause();
	cPause_UserPause = UserPause;
	bool IsPaused = GetPause();
	if(WasPaused != IsPaused) {
		if(IsPaused) {
			cInput::StopVibrations();
		}
	}
}

void cPause::ManuallyDisablePause(const bool Disable) {
	cPause_Disabled = Disable;
}
} // comms
