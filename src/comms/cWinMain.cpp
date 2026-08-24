#include <cassert>

#include "comms.h"
#ifdef COMMS_3DCOAT
#include "navlib_job.h"
class BinStream;
#include "../ClassEngine/TagsList.h"
#define COMMS_3DCONNEXION_NEW
bool UseOld3DConnexionAPI();
bool CheckIfFileExists(const char* path);
void __elog(const char* s);
#else // !3DCoat
bool UseOld3DConnexionAPI() { return true; }
void __elog(const char*) {}
#endif // COMMS_3DCOAT

#ifdef COMMS_3DCOAT
#include "../3D-Coat/BenchmarkState.h"
extern int ScriptDepth;
#ifndef COMMS_WINDOWS

void BenchmarkState::Start(){
}
void BenchmarkState::End(){
}
void BenchmarkState::Report(const char* name,float value){
}
#endif // !COMMS_WINDOWS
#endif // COMMS_3DCOAT

#ifdef COMMS_WINDOWS
#include <ShellScalingApi.h>
#ifndef COMMS_3DCOAT
static const int ScriptDepth = 0;
#endif // !COMMS_3DCOAT
#ifdef COMMS_TABLET
#include <msinkaut.h>
#include <msinkaut_i.c>
#include "../Libs/Tablet/wintab.h"
#define PACKETDATA (PK_X | PK_Y | PK_BUTTONS | PK_NORMAL_PRESSURE | PK_STATUS)
#define PACKETMODE 0
#include "../Libs/Tablet/pktdef.h"
#endif // COMMS_TABLET
static bool cWinMain_Proximity = false;

static comms::cStr cWinMain_CmdLineArgs;
// cWinMain_ParseCmdLineArgs
static void cWinMain_ParseCmdLineArgs(int argc, char** argv) {
	if(argc > 1) {
		int i;
        for(i = 1; i < argc; i++) {
            if(!cWinMain_CmdLineArgs.IsEmpty()) {
                cWinMain_CmdLineArgs += " ";
			}
            cWinMain_CmdLineArgs.Append(argv[i]);
        }
    }
}



namespace comms {

extern HWND cWinMain_NonmodalDialog;
	
// cMain_GetCmdLineArgs
const cStr cMain_GetCmdLineArgs() {
	return cWinMain_CmdLineArgs;
}

} // comms

static HWND cWinMain_hWnd = nullptr;
void cWinMain_SetWindow(HWND Window) {
	cWinMain_hWnd = Window;
}
static bool cWinMain_AllowOnRender = false;
static WINDOWPLACEMENT cWinMain_WindowPlacement = { 0 };

// cWinMain_GetWindow
HWND cWinMain_GetWindow() {
	return cWinMain_hWnd;
}
static HWND cWinMain_OverrideParent = nullptr; // When a file dialog is visible set this to the "File Dialog's Handler" (hDlg). nullptr otherwise.
void cWinMain_SetOverrideParent(HWND Dialog) {
	cWinMain_OverrideParent = Dialog;
}
// cWinMain_GetParent
HWND cWinMain_GetParent() {
	return (nullptr == cWinMain_OverrideParent) ? cWinMain_hWnd : cWinMain_OverrideParent;
}

#ifdef COMMS_3DCOAT
#include "../3D-Coat/OpenAutomate/OpenAutomate.h"
#include <direct.h>
#include "../3D-Coat/fex/FEXModule.h"
#include "../3D-Coat/res/resource.h"
#else // StrokeTest, FServer
#include "../resource.h"
#endif // COMMS_3DCOAT

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
// The '*' in the processor architecture means both 'x86' and 'amd64'. It resolves the error "The application was unable to start correctly (0xc000007b)" which is the result of linking against 32-bit "comctl32.dll" when building 64-bit application.
#ifdef COMMS_XINPUT
#include "../Libs/XInput/XInput.h"
#endif // COMMS_XINPUT
#include "Wininet.h"
#pragma comment (lib, "Wininet.lib")
#pragma comment (lib, "ws2_32.lib")

static bool cWinMain_EraseBkgnd = false;
static comms::cList<int> cWinMain_Codes, cWinMain_CodesRemap;
static bool cWinMain_EnableInputEvents = true;
static int cWinMain_CurCursorIndex = -1;
static HCURSOR cWinMain_hCursors[11] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

#ifdef COMMS_XINPUT
static HINSTANCE g_XInputDll = nullptr;
typedef DWORD (__stdcall *pFnXInputGetState)(DWORD, XINPUT_STATE *);
typedef DWORD (__stdcall *pFnXInputSetState)(DWORD, XINPUT_VIBRATION *);

static pFnXInputGetState g_XInputGetState = nullptr;
static pFnXInputSetState g_XInputSetState = nullptr;
#endif // COMMS_XINPUT

static bool cWinMain_FullScreen = false;

static STICKYKEYS cWinMain_StartupStickyKeys = {
	sizeof(STICKYKEYS), 0
};
static TOGGLEKEYS cWinMain_StartupToggleKeys = {
	sizeof(TOGGLEKEYS), 0
};
static FILTERKEYS cWinMain_StartupFilterKeys = {
	sizeof(FILTERKEYS), 0
};

static WINDOWPLACEMENT cWinMain_WindowPlacementBeforeFullScreenMode;
static HMONITOR cWinMain_MonitorForFullScreenMode = nullptr;

#ifdef COMMS_3DCOAT
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int);
#include <intrin.h>
void cWinMain_OnRender();
void cWinMain_AllowAccessibilityShortcutKeys(bool AllowKeys);
LRESULT WINAPI WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
extern bool OpenAutomateEnabled;
extern bool OpenAutomateHidden;


void ParseArgs(LPSTR args){
	char* s1=strstr(args,"-openautomate");
	if(s1){
		s1+=14;
		while(s1[0]==' ')s1++;
		oaVersion oaV;		
		char tmp[512];
		strcpy(tmp,s1);
		s1=tmp;
		if(tmp[0]=='"'){
			tmp[strlen(tmp)-1]=0;
			s1++;
		}
		comms::cLog::Message("oaInit args:%s",s1);
		if(!oaInit(s1,&oaV)){			
			comms::cLog::Message("oaInit failed");
		}
		OpenAutomateEnabled=true;		
	}
}


static oaNamedOption Options[8];
static oaInt NumOptions = 0;
void InitOptions(){	
	oaNamedOption *Option;

	Option = &Options[NumOptions++];
	oaInitOption(Option);
	Option->Name = "CUDA Usage";
	Option->DataType = OA_TYPE_ENUM;
	Option->Value.Enum = "Enabled";

	Options[NumOptions] = *Option;
	Option = &Options[NumOptions++];
	Option->Value.Enum = "Disabled";
}
void BenchmarkState::Start(){
	oaStartBenchmark();
}
void BenchmarkState::End(){
	oaEndBenchmark();
}
void BenchmarkState::Report(const char* name,float value){
	oaValue ov;
	ov.Float=value;
	oaDisplayFrame(value);
}
void SetOptions(){
	oaNamedOption *Option;
	while((Option = oaGetNextOption()) != nullptr){
	}
}
const char* BenchmarksList[]={
	"Draw with sphere",	//1
	"Carve",			//2
	"Carve with mask",	//3
	""
};
char* BenchmarksListF[128];
int NBenchF=0;
const char* u_cuda[]={"CUDA","CPU"};
const char* u_pen[]={"thin pen(10)","thick pen(40)"};
bool ProcessOpenAutomateCommands(){
	oaCommand Command;	
	while(1){
		oaInitCommand(&Command);
		switch(oaGetNextCommand(&Command))
		{
			/* No more commands, exit program */
		case OA_CMD_EXIT: 
			OpenAutomateEnabled=false;
			OpenAutomateHidden=false;
			comms::cLog::Message("OA_CMD_EXIT");
			//while(true){};						
			return false;

			/* Run as normal */
		case OA_CMD_RUN: 
			OpenAutomateEnabled=false;
			OpenAutomateHidden=true;
			comms::cLog::Message("OA_CMD_RUN");
			//while(true){};						
			return true;

			/* Enumerate all in-game options */
		case OA_CMD_GET_ALL_OPTIONS: 
			for(oaInt i=0; i < NumOptions; ++i){
				oaAddOption(&Options[i]);
			}
			comms::cLog::Message("OA_CMD_OPTIONS");
			break;
			/* Return the option values currently set */
		case OA_CMD_GET_CURRENT_OPTIONS:
			{
				int idx=0;			
				if(BenchmarkState::CUDA_Enabled){
					idx=0;				
				}else{
					idx=1;
				}			
				oaAddOptionValue(Options[idx].Name,Options[idx].DataType,&Options[idx].Value);
			}
			comms::cLog::Message("OA_CMD_GET_CURRENT_OPTIONS");
			break;

			/* Set all in-game options */
		case OA_CMD_SET_OPTIONS: 
			SetOptions();
			comms::cLog::Message("OA_CMD_SET_OPTIONS");
			break;

			/* Enumerate all known benchmarks */
		case OA_CMD_GET_BENCHMARKS: 
			{
				NBenchF=0;				
				void StartBenchmarkSesion();
				StartBenchmarkSesion();
				for(int t=0;t<2;t++){
					for(int id=0;id<3;id++){
						for(int c=0;c<2;c++){
							char cc2[256];
							sprintf(cc2,"%s: %s+%s",BenchmarksList[id],u_cuda[c],u_pen[t]);
							BenchmarksListF[NBenchF]=new char[strlen(cc2)+1];
							strcpy(BenchmarksListF[NBenchF],cc2);
							oaAddBenchmark(BenchmarksListF[NBenchF]);
							comms::cLog::Message("%d: %s",NBenchF,BenchmarksListF[NBenchF]);
							NBenchF++;							
						}
					}
				}
			}
			comms::cLog::Message("OA_CMD_GET_BENCHMARKS");
			break;
			/* Run benchmark */
		case OA_CMD_RUN_BENCHMARK:
			{
				//SetOptions();
				for(int i=0;i<NBenchF;i++){
					if(!strcmp(Command.BenchmarkName,BenchmarksListF[i])){
						extern char TestName[128];						
						strcpy(TestName,BenchmarksListF[i]);
						BenchmarkState::BenchmarkIndex=((i/2)%3)+1;
						BenchmarkState::BenchmarkStage=0;
						int p=i/6;
						BenchmarkState::PenRadius=10.0+((p&1) ? 30.0 : 0);
						BenchmarkState::CUDA_Enabled=!(i&1);
						BenchmarkState::Start();
						comms::cLog::Message("Test: %s, CUDA:%d, R:%f",BenchmarksListF[i],BenchmarkState::CUDA_Enabled,BenchmarkState::PenRadius);
						break;				
					}
				}
			}
			comms::cLog::Message("OA_CMD_RUN_BENCHMARK");
			return true;
		}
	}
	return true;
}
#endif // COMMS_3DCOAT

//-----------------------------------------------------------------------------
// cWinMain_AllowAccessibilityShortcutKeys
//-----------------------------------------------------------------------------
static void cWinMain_AllowAccessibilityShortcutKeys(bool AllowKeys) {
	if(AllowKeys) {
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &cWinMain_StartupStickyKeys, 0);
		SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &cWinMain_StartupToggleKeys, 0);
		SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &cWinMain_StartupFilterKeys, 0);
	} else {
		// Disable sticky / toggle / filter shortcuts but if the accessibility feature is on,
		// then leave the settings alone as its probably being usefully used
		STICKYKEYS skOff = cWinMain_StartupStickyKeys;
		if((skOff.dwFlags & SKF_STICKYKEYSON) == 0) {
			// Disable the hotkey and the confirmation
			skOff.dwFlags &= ~SKF_HOTKEYACTIVE;
			skOff.dwFlags &= ~SKF_CONFIRMHOTKEY;
			SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &skOff, 0);
		}
		
		TOGGLEKEYS tkOff = cWinMain_StartupToggleKeys;
		if((tkOff.dwFlags & TKF_TOGGLEKEYSON) == 0) {
			// Disable the hotkey and the confirmation
			tkOff.dwFlags &= ~TKF_HOTKEYACTIVE;
			tkOff.dwFlags &= ~TKF_CONFIRMHOTKEY;
			
			SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tkOff, 0);
		}
		
		FILTERKEYS fkOff = cWinMain_StartupFilterKeys;
		if((fkOff.dwFlags & FKF_FILTERKEYSON) == 0) {
			// Disable the hotkey and the confirmation
			fkOff.dwFlags &= ~FKF_HOTKEYACTIVE;
			fkOff.dwFlags &= ~FKF_CONFIRMHOTKEY;
			
			SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &fkOff, 0);
		}
	}
} // cWinMain_AllowAccessibilityShortcutKeys

// cWinMain_SetWindowed
static void cWinMain_SetWindowed() {
	cWinMain_FullScreen = false;
	HWND F = GetForegroundWindow();
	
	SetWindowLong(cWinMain_hWnd, GWL_EXSTYLE, 0);
	SetWindowLong(cWinMain_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE);

	SetWindowPlacement(cWinMain_hWnd, &cWinMain_WindowPlacementBeforeFullScreenMode);
	if(F == cWinMain_hWnd) {
		SetWindowPos(cWinMain_hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	} else {
		SetWindowPos(cWinMain_hWnd, F, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
	}
	cWinMain_AllowAccessibilityShortcutKeys(true);
}

// cWinMain_SetFullScreen
static void cWinMain_SetFullScreen() {
	cWinMain_FullScreen = true;
	// Save window placement before going to "Full Screen" mode
	memset(&cWinMain_WindowPlacementBeforeFullScreenMode, 0, sizeof(cWinMain_WindowPlacementBeforeFullScreenMode));
	cWinMain_WindowPlacementBeforeFullScreenMode.length = sizeof(cWinMain_WindowPlacementBeforeFullScreenMode);
	GetWindowPlacement(cWinMain_hWnd, &cWinMain_WindowPlacementBeforeFullScreenMode);
	
	// Do not set the style "WS_POPUP" because the input string and file dialogs will be invisible.
	// Remove the caption from the existing windowed style:
	LONG lStyle = GetWindowLong(cWinMain_hWnd, GWL_STYLE);
	lStyle = lStyle & ~WS_CAPTION;
	SetWindowLong(cWinMain_hWnd, GWL_STYLE, lStyle);

	RECT rc;
	memset(&rc, 0, sizeof(rc));
	rc.right = GetSystemMetrics(SM_CXSCREEN);
	rc.bottom = GetSystemMetrics(SM_CYSCREEN);
	cWinMain_MonitorForFullScreenMode = MonitorFromWindow(cWinMain_hWnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO I;
	memset(&I, 0, sizeof(I));
	I.cbSize = sizeof(I);
	if(GetMonitorInfo(cWinMain_MonitorForFullScreenMode, &I)) {
		rc = I.rcMonitor;
	}
	// Do not pass "SWP_SHOWWINDOW" into "SetWindowPos" because the fullscreen window will not take the whole screen.
	// Instead, hide the window and then show it maximized. The fullscreen window will occupy the whole screen now.
	SetWindowPos(cWinMain_hWnd, HWND_TOPMOST, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_HIDEWINDOW);
	ShowWindow(cWinMain_hWnd, SW_MAXIMIZE);
	cWinMain_AllowAccessibilityShortcutKeys(false);
}



// cMessageBox::YesNo
bool comms::cMessageBox::YesNo(const char *Caption, const char *Text, ...) {
	va_list args;
	va_start(args, Text);
	char temp[8192];
	vsprintf_s(temp, 8192, Text, args);
	va_end(args);
    cPause::SetSystemPause(true);

	wchar_t wc[8192];
	wc[MultiByteToWideChar(CP_UTF8, 0, temp, (int)strlen(temp), wc, 8192)] = 0;
	wchar_t wcc[8192];
	wcc[MultiByteToWideChar(CP_UTF8, 0, Caption, (int)strlen(Caption), wcc, 8192)] = 0;
	
	return MessageBoxW(cWinMain_GetParent(), wc, wcc, MB_YESNO) == IDYES;
}

// cMessageBox::Ok
void comms::cMessageBox::Ok(const char *Caption, const char *Text, ...) {
	va_list args;
	va_start(args, Text);
	char temp[8192];
	vsprintf_s(temp, 8192, Text, args);
	va_end(args);
    cPause::SetSystemPause(true);

	wchar_t wc[8192];
	wc[MultiByteToWideChar(CP_UTF8, 0, temp, (int)strlen(temp), wc, 8192)] = 0;
	wchar_t wcc[8192];
	wcc[MultiByteToWideChar(CP_UTF8, 0, Caption, (int)strlen(Caption), wcc, 8192)] = 0;

    MessageBoxW(cWinMain_GetParent(), wc, wcc, MB_OK);
}

//-----------------------------------------------------------------------------
// cInput::SetCursor
//-----------------------------------------------------------------------------
void comms::cInput::SetCursor(const cInput::Cursor::Enum Cursor) {
	int Index = (int)Cursor;
	if(-1 == Index) {
		Index = 0;
	}
	if(Index != cWinMain_CurCursorIndex) {
		if(cInput::Cursor::None == Index) {
			::ShowCursor(FALSE);
		} else {
			::SetCursor(cWinMain_hCursors[Index]);
			if(cInput::Cursor::None == cWinMain_CurCursorIndex) {
				::ShowCursor(TRUE);
			}
		}
		cWinMain_CurCursorIndex = Index;
	}
} // cInput::SetCursor

// cInput::WarpCursor
void comms::cInput::WarpCursor(const int LocalX, const int LocalY, const bool DownY) {
	POINT p;
	p.x = LocalX;
	p.y = LocalY;
	RECT rc;
	GetClientRect(cWinMain_hWnd, &rc);
	int H = rc.bottom - rc.top;
    if(!DownY) {
		p.y = H - LocalY;
	}
	ClientToScreen(cWinMain_hWnd, &p);
	SetCursorPos(p.x, p.y);
}

// cInput::EnableEvents
bool comms::cInput::EnableEvents() {
	return cWinMain_EnableInputEvents;
}

//-----------------------------------------------------------------------------
// cInput::IsDownAcquire
//-----------------------------------------------------------------------------
bool comms::cInput::IsDownAcquire(const int Code) {
	if(GetForegroundWindow() != cWinMain_hWnd) { // Input only to active window
		return false;
	}
    if(Code == LeftButton) {
        const bool L = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        return L;
    }
	if (Code == RightButton)return GetKeyState(VK_RBUTTON);
	if (Code == MiddleButton)return GetKeyState(VK_MBUTTON);
	if(Code < Esc || Code > Decimal) {
		return false;
	}
	
	int i0 = cWinMain_Codes[Code];
	if(-1 == i0) {
		return false;
	}
	bool r = (GetKeyState(i0) & 0x80) != 0;
	
	return r;
} // cInput::IsDownAcquire

//-----------------------------------------------------------------------------
// cInput::AcquireKeyboard
//-----------------------------------------------------------------------------
bool comms::cInput::AcquireKeyboard(comms::cInput::KeyboardState *S) {
	if(GetForegroundWindow() != cWinMain_hWnd) { // Input only to active window
		return false;
	}
	
	static BYTE State[256];
	int i, i0;

	int ScanCode, r;
	WORD c;

	if(!GetKeyboardState(State)) {
		return false;
	}
	
	for(i = 0; i < 256; i++) {
		i0 = cWinMain_CodesRemap[i];
		if(i0 != -1) {
			S->IsDown[i0] = ((State[i] & 0x80) != 0);
		}
	}
	// Special case: CapsLock
	S->IsDown[cInput::CapsLock] = State[VK_CAPITAL] & 1;

	// Chars
	HKL Layout = GetKeyboardLayout(0);
	for(i = 0; i < cWinMain_Codes.Count(); i++) {
		ScanCode = MapVirtualKeyEx(cWinMain_Codes[i], 0, Layout);
		if(ScanCode != 0) {
			r = ToAsciiEx(cWinMain_Codes[i], ScanCode, State, &c, 0, Layout);
			if(r != 0) {
				S->Chars[i] = (char)c;
			}
		}
	}
	return true;
} // cInput::AcquireKeyboard

// cWinMain_SetCapture
void cWinMain_SetCapture() {
	::SetCapture(cWinMain_hWnd);
}

// cWinMain_ReleaseCapture
void cWinMain_ReleaseCapture() {
	::ReleaseCapture();
}

// cWinMain_ClientToLocal
void cWinMain_ClientToLocal(const int X, const int Y, comms::cVec2 *LocalPos) {
	RECT rc;
	GetClientRect(cWinMain_hWnd, &rc);
	LocalPos->Set((float)X, (float)(rc.bottom - Y));
}

// cWinMain_GlobalToLocal
void cWinMain_GlobalToLocal(const int X, const int Y, comms::cVec2 *LocalPos) {
	POINT p;
	p.x = X;
	p.y = Y;
	ScreenToClient(cWinMain_hWnd, &p);
	cWinMain_ClientToLocal(p.x, p.y, LocalPos);
}

// cWinMain_GetMousePositionAcquire
void cWinMain_GetMousePositionAcquire(comms::cVec2* M) {
	POINT p;
	GetCursorPos(&p);
	cWinMain_GlobalToLocal(p.x, p.y, M);
}

#ifdef COMMS_TABLET

//-----------------------------------------------------------------------------
// cWinMain_TabletBase
//-----------------------------------------------------------------------------
class cWinMain_TabletBase {
public:
	virtual bool Init() = 0;
	virtual void Free() = 0;
	virtual void GetTabletState(float *CurPressure, bool *PenPressed, bool *EraserUsed, int *LastTime) = 0;
	virtual void WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {}
}; // cWinMain_TabletBase

//-----------------------------------------------------------------------------
// cWinMain_TabletState
//-----------------------------------------------------------------------------
class cWinMain_TabletState {
public:
	cWinMain_TabletState() {
		m_CurPressure = 0.0f;
		m_PenPressed = false;
		m_EraserUsed = false;
		m_LastActivityTime = -1;
	}
	void Init() {
	}
	void Free() {
	}
	void GetTabletState(float *CurPressure, bool *PenPressed, bool *EraserUsed, int *LastTime) {
		m_Mutex.lock();
		if (CurPressure != nullptr) {
			*CurPressure = m_CurPressure;
		}
		if (PenPressed != nullptr) {
			*PenPressed = m_PenPressed;
		}
		if (EraserUsed != nullptr) {
			*EraserUsed = m_EraserUsed;
		}
		if (LastTime != nullptr) {
			*LastTime = m_LastActivityTime;
		}
		m_Mutex.unlock();
	}
	void SetPressure(const float Pressure) {
		DWORD t = GetTickCount();
		m_Mutex.lock();
		m_CurPressure = 650.0f * Pressure;
		m_LastActivityTime = t;
		m_Mutex.unlock();
	}
	void SetTabletState(float Pressure, bool PenPressed, bool EraserUsed) {
		DWORD t = GetTickCount();
		m_Mutex.lock();
		m_CurPressure = 650.0f * Pressure;
		m_PenPressed = PenPressed;
		m_EraserUsed = EraserUsed;
		m_LastActivityTime = t;
		m_Mutex.unlock();
	}
	void SetZero() {
		m_Mutex.lock();
		m_CurPressure = 0.0f;
		m_PenPressed = false;
		m_EraserUsed = false;
		m_Mutex.unlock();
	}
private:
	int m_LastActivityTime;
	float m_CurPressure;
	bool m_PenPressed, m_EraserUsed;
	std::mutex m_Mutex;
}; // cWinMain_TabletState

void bypass_on_hang(const char* id, const char* msg, std::function<void()> todo) {
	auto fn = comms::cIO::EnsureAbsolutePath(comms::cStr("data/Temp/") + msg + ".bypass");
	comms::cIO::ReplaceReadPath(&fn);
	FILE* F = nullptr;
	_wfopen_s(&F, comms::wchar_path(fn), comms::wchar_path("rb"));
	if(F) {
		fclose(F);
		if (comms::cMessageBox::YesNo("Something wrong!", msg))return;
	}
	_wfopen_s(&F, comms::wchar_path(fn), comms::wchar_path("wb"));
	if (F) {
		fwrite(&F, 4, 1, F);
		fclose(F);
	}
	todo();
	comms::cIO::RemoveFile(fn);

}
// cWinMain_WinTab
class cWinMain_WinTab : public cWinMain_TabletBase {
public:
	cWinMain_WinTab() {
		m_WasPressed = false;
		m_PrevPos.SetZero();
		m_ShiftPos.SetZero();
		m_InitShiftPos = true;
		m_MaxPressure = 0;
		Null();
		m_Active = TRUE;
		m_LastActivityTime = -1;
		m_RefreshPeriod = 3000;
	}
	bool Init() {
		m_State.Init();
		bypass_on_hang("wintab", "The Wintab32.dll suspended during previous loading. This may be related to the tablet drivers. Please reinstall the tablet drivers. Press Yes to bypass the problem and No to try again.",
			[&] {
				m_WinTabDll = LoadLibraryA("Wintab32.dll");
			});

		if(nullptr == m_WinTabDll) {
			return false;
		}
		FindProcs();
		if((nullptr == m_WTInfoA) || !m_WTInfoA(0, 0, nullptr)) { // No WinTab services
			comms::cLog::Message("No WinTab Service. Reinstall Wacom tablet drivers.");
			return false;
		}
		m_DeviceName.SetLength(128);
		m_WTInfoA(WTI_DEVICES, DVC_NAME, m_DeviceName.ToNonConstCharPtr());
		m_DeviceName.CalcLength();
		if(m_DeviceName.IsEmpty()) {
			return false;
		}
		AXIS N;
		m_WTInfoA(WTI_DEVICES, DVC_NPRESSURE, &N);
		m_MaxPressure = N.axMax;
		LogInfo();
		LOGCONTEXT LC;
		m_WTInfoA(WTI_DEFCONTEXT, 0, &LC);
		DWORD ID = GetCurrentProcessId();
		wsprintf(LC.lcName, "cWinMain_WinTab %d", ID);
		LC.lcOptions |= (CXO_MESSAGES | CXO_SYSTEM);
		LC.lcPktData = PACKETDATA;
		LC.lcPktMode = PACKETMODE;
		LC.lcMoveMask = PACKETDATA;
		LC.lcBtnUpMask = LC.lcBtnDnMask;
		LC.lcOutOrgX = GetSystemMetrics(SM_XVIRTUALSCREEN);
		LC.lcOutOrgY = GetSystemMetrics(SM_YVIRTUALSCREEN);
		LC.lcOutExtX = GetSystemMetrics(SM_CXVIRTUALSCREEN);
		LC.lcOutExtY = -GetSystemMetrics(SM_CYVIRTUALSCREEN);
		m_Context = m_WTOpenA(cWinMain_hWnd, &LC, TRUE);
		return (m_Context != nullptr);
	}
	void Free() {
		m_State.Free();
		if(m_Context != nullptr) {
			m_WTClose(m_Context);
			m_Context = nullptr;
		}
		if(m_WinTabDll != nullptr) {
			FreeLibrary(m_WinTabDll);
			m_WinTabDll = nullptr;
		}
		Null();
	}
	// cWinMain_WinTab.PosFromPacket
	const comms::cVec2 PosFromPacket(const PACKET &P) {
		comms::cVec2 Pos(0.0f);
		cWinMain_GlobalToLocal(P.pkX, P.pkY, &Pos);
		if (m_InitShiftPos) {
			m_InitShiftPos = false;
			comms::cVec2 M;
			cWinMain_GetMousePositionAcquire(&M);
			m_ShiftPos = M - Pos;
		}
		return Pos + m_ShiftPos;
	}
	// cWinMain_WinTab.WndProc
	void WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
		PACKET P;
		if(WT_PROXIMITY == Msg) {
			cWinMain_Proximity = lParam != 0;
		} else if(WT_PACKET == Msg) {
			if(m_Active && m_WTPacket((HCTX)lParam, (UINT)wParam, &P)) {
				bool PenPressed = (P.pkNormalPressure > 0);
				bool EraserUsed = PenPressed && ((P.pkStatus & TPS_INVERT) != 0) && !comms::cSettings::GetInstance()->TreatEraserAsPen;
				float Pressure = comms::cMath::LerperClamp01(0.0f, (float)m_MaxPressure, (float)P.pkNormalPressure);
				m_State.SetTabletState(Pressure, PenPressed, EraserUsed);
				if (!m_WasPressed && PenPressed) {
					m_WasPressed = true;
					comms::cInputEvent_Pen *Pen = new comms::cInputEvent_Pen;
					Pen->Type = comms::cInputEvent_Pen::TYPE::Down;
					Pen->Eraser = EraserUsed;
					Pen->PenPos = PosFromPacket(P);
					m_PrevPos = Pen->PenPos;
					Pen->PenPressure = Pressure;
					comms::cInput::AddEvent(Pen); Pen = nullptr;
				}
				else if (m_WasPressed && !PenPressed) {
					m_WasPressed = false;
					comms::cInputEvent_Pen *Pen = new comms::cInputEvent_Pen;
					Pen->Type = comms::cInputEvent_Pen::TYPE::Up;
					comms::cInput::AddEvent(Pen); Pen = nullptr;
				}
				else if (m_WasPressed && PenPressed) {
					comms::cInputEvent_Pen *Pen = new comms::cInputEvent_Pen;
					Pen->Type = comms::cInputEvent_Pen::TYPE::Move;
					comms::cVec2 CurPos = PosFromPacket(P);
					Pen->PenPos = CurPos;
					Pen->PenDelta = CurPos - m_PrevPos;
					m_PrevPos = CurPos;
					Pen->PenPressure = Pressure;
					comms::cInput::AddEvent(Pen); Pen = nullptr;
				}
				m_LastActivityTime = GetTickCount();
				m_RefreshPeriod = 3000;
			}
		} else if(WM_ACTIVATE == Msg) {
			if(m_Context != nullptr) {
				m_Active = LOWORD(wParam);
				m_WTEnable(m_Context, m_Active);
				if(m_Context && m_Active) {
					m_WTOverlap(m_Context, TRUE);
				}
				if(m_Context && !m_Active) {
					m_State.SetZero();
				}
			}
		}
	}
	void GetTabletState(float *CurPressure, bool *PenPressed, bool *EraserUsed, int *LastTime) {
		m_State.GetTabletState(CurPressure, PenPressed, EraserUsed, LastTime);
		if(m_LastActivityTime!=-1 && (int)GetTickCount()-m_LastActivityTime>m_RefreshPeriod){
			if(m_Context){
				m_LastActivityTime=GetTickCount();
				m_WTEnable(m_Context, false);
				m_WTEnable(m_Context, true);
				m_WTOverlap(m_Context, TRUE);
				m_RefreshPeriod+=2000;
			}
		}
	}
private:
	typedef UINT (WINAPI * WTINFOA)(UINT, UINT, LPVOID);
	typedef HCTX (WINAPI * WTOPENA)(HWND, LPLOGCONTEXTA, BOOL);
	typedef BOOL (WINAPI * WTGETA)(HCTX, LPLOGCONTEXT);
	typedef BOOL (WINAPI * WTSETA)(HCTX, LPLOGCONTEXT);
	typedef BOOL (WINAPI * WTCLOSE)(HCTX);
	typedef BOOL (WINAPI * WTENABLE)(HCTX, BOOL);
	typedef BOOL (WINAPI * WTPACKET)(HCTX, UINT, LPVOID);
	typedef BOOL (WINAPI * WTOVERLAP)(HCTX, BOOL);
	typedef BOOL (WINAPI * WTSAVE)(HCTX, LPVOID);
	typedef BOOL (WINAPI * WTCONFIG)(HCTX, HWND);
	typedef HCTX (WINAPI * WTRESTORE)(HWND, LPVOID, BOOL);
	typedef BOOL (WINAPI * WTEXTSET)(HCTX, UINT, LPVOID);
	typedef BOOL (WINAPI * WTEXTGET)(HCTX, UINT, LPVOID);
	typedef BOOL (WINAPI * WTQUEUESIZESET)(HCTX, int);
	typedef int (WINAPI * WTDATAPEEK)(HCTX, UINT, UINT, int, LPVOID, LPINT);
	typedef int (WINAPI * WTPACKETSGET)(HCTX, int, LPVOID);
	
	cWinMain_TabletState m_State;
	int m_LastActivityTime;
	int m_RefreshPeriod;
	int m_MaxPressure;
	bool m_WasPressed;
	comms::cVec2 m_PrevPos;
	comms::cStr m_DeviceName;

	// User of "Huion Kamvas 24 Pro" (Michal Bukowski) in "WinTab" has shift by 2880 pixels along axis "Y".
	// To address this problem here is an additional shift:
	comms::cVec2 m_ShiftPos; // Contains difference between mouse cursor position and position from "WinTab" packets
	bool m_InitShiftPos;

	void LogInfo() {
		comms::cStr Log;
		Log += comms::cStr(60, '-') + comms::cStr::EndLn;
		Log += "WinTab" + comms::cStr::EndLn;
		Log += "Device Name: " + m_DeviceName + comms::cStr::EndLn;
		Log += "Max Pressure: " + comms::cStr::ToString(m_MaxPressure) + comms::cStr::EndLn;
		Log += comms::cStr(60, '-') + comms::cStr::EndLn;
		comms::cLog::Message(Log);
	}
	HINSTANCE m_WinTabDll;
	WTOPENA m_WTOpenA;
	WTCLOSE m_WTClose;
	WTINFOA m_WTInfoA;
	WTGETA m_WTGetA;
	WTSETA m_WTSetA;
	WTPACKET m_WTPacket;
	WTENABLE m_WTEnable;
	WTOVERLAP m_WTOverlap;
	WTSAVE m_WTSave;
	WTCONFIG m_WTConfig;
	WTRESTORE m_WTRestore;
	WTEXTGET m_WTExtGet;
	WTEXTSET m_WTExtSet;
	WTQUEUESIZESET m_WTQueueSizeSet;
	WTDATAPEEK m_WTDataPeek;
	WTPACKETSGET m_WTPacketsGet;
	HCTX m_Context;
	BOOL m_Active;
	void Null() {
		m_WinTabDll = nullptr;
		m_WTOpenA = nullptr;
		m_WTClose = nullptr;
		m_WTInfoA = nullptr;
		m_WTGetA = nullptr;
		m_WTSetA = nullptr;
		m_WTPacket = nullptr;
		m_WTEnable = nullptr;
		m_WTOverlap = nullptr;
		m_WTSave = nullptr;
		m_WTConfig = nullptr;
		m_WTRestore = nullptr;
		m_WTExtGet = nullptr;
		m_WTExtSet = nullptr;
		m_WTQueueSizeSet = nullptr;
		m_WTDataPeek = nullptr;
		m_WTPacketsGet = nullptr;
		m_Context = nullptr;
	}
	void FindProcs() {
		m_WTOpenA = (WTOPENA)GetProcAddress(m_WinTabDll, "WTOpenA");
		m_WTClose = (WTCLOSE)GetProcAddress(m_WinTabDll, "WTClose");
		m_WTInfoA = (WTINFOA)GetProcAddress(m_WinTabDll, "WTInfoA");
		m_WTGetA = (WTGETA)GetProcAddress(m_WinTabDll, "WTGetA");
		m_WTSetA = (WTSETA)GetProcAddress(m_WinTabDll, "WTSetA");
		m_WTPacket = (WTPACKET)GetProcAddress(m_WinTabDll, "WTPacket");
		m_WTEnable = (WTENABLE)GetProcAddress(m_WinTabDll, "WTEnable");
		m_WTOverlap = (WTOVERLAP)GetProcAddress(m_WinTabDll, "WTOverlap");
		m_WTSave = (WTSAVE)GetProcAddress(m_WinTabDll, "WTSave");
		m_WTConfig = (WTCONFIG)GetProcAddress(m_WinTabDll, "WTConfig");
		m_WTRestore = (WTRESTORE)GetProcAddress(m_WinTabDll, "WTRestore");
		m_WTExtGet = (WTEXTGET)GetProcAddress(m_WinTabDll, "WTExtGet");
		m_WTExtSet = (WTEXTSET)GetProcAddress(m_WinTabDll, "WTExtSet");
		m_WTQueueSizeSet = (WTQUEUESIZESET)GetProcAddress(m_WinTabDll, "WTQueueSizeSet");
		m_WTDataPeek = (WTDATAPEEK)GetProcAddress(m_WinTabDll, "WTDataPeek");
		m_WTPacketsGet = (WTPACKETSGET)GetProcAddress(m_WinTabDll, "WTPacketsGet");
	}
};

//-----------------------------------------------------------------------------
// cWinMain_InkCollector
//-----------------------------------------------------------------------------
// COM interface of Tablet PC Ink API resides within INKOBJ.DLL which is available on Windows XP SP2 and later
// This class has been tested with:
// 1. Tablet PC SDK v1.7 http://www.microsoft.com/en-us/download/details.aspx?id=20039
// 2. WACOM Graphire4 CTE-640 + 4.95-6 Tablet PC Edition Drivers ftp://ftp.wacom-europe.com/pub/WINDOWS/cons4.95-6_int.exe
// 3. Genius G-Pen 450 Pen Tablet ftp://85.21.123.44/GPen.zip
//-----------------------------------------------------------------------------
class cWinMain_InkCollector : public cWinMain_TabletBase, public _IInkCollectorEvents {
public:
	cWinMain_InkCollector() {
		m_InkCollector = nullptr;
		m_ConnectionPoint = nullptr;
		m_AdviseCookie = 0;
		m_Unknown = nullptr;
	}
	bool Init() {
		m_State.Init();
		CoInitialize(nullptr);
		CoCreateInstance(CLSID_InkCollector, nullptr, CLSCTX_ALL, IID_IInkCollector, (void **)&m_InkCollector);
		if(m_InkCollector != nullptr) {
			CoCreateFreeThreadedMarshaler(this, &m_Unknown);
			m_InkCollector->SetEventInterest(ICEI_AllEvents, VARIANT_TRUE);
			IConnectionPointContainer *CPC = nullptr;
			m_InkCollector->QueryInterface(IID_IConnectionPointContainer, (void **)&CPC);
			if(CPC != nullptr) {
				CPC->FindConnectionPoint(DIID__IInkCollectorEvents, &m_ConnectionPoint);
				if(m_ConnectionPoint != nullptr) {
					m_ConnectionPoint->Advise(this, &m_AdviseCookie);
				}
				CPC->Release();
				CPC = nullptr;
			}
#pragma warning(disable:4311)
#pragma warning(disable:4302)
			m_InkCollector->put_hWnd((long)cWinMain_hWnd);
#pragma warning(default:4302)
#pragma warning(default:4311)
			m_InkCollector->SetAllTabletsMode(VARIANT_FALSE);
			m_InkCollector->put_CollectionMode(ICM_InkOnly);
			m_InkCollector->put_MousePointer(IMP_Custom);
			m_InkCollector->put_AutoRedraw(VARIANT_FALSE);
			m_InkCollector->put_DynamicRendering(VARIANT_FALSE);
			m_InkCollector->put_Enabled(VARIANT_TRUE);
		}
		return true;
	}
	void Free() {
		m_State.Free();
		if(m_ConnectionPoint != nullptr) {
			m_ConnectionPoint->Unadvise(m_AdviseCookie);
			m_ConnectionPoint->Release();
			m_ConnectionPoint = nullptr;
		}
		if(m_Unknown != nullptr) {
			m_Unknown->Release();
			m_Unknown = nullptr;
		}
		if(m_InkCollector != nullptr) {
			m_InkCollector->put_Enabled(VARIANT_FALSE);
			m_InkCollector->Release();
			m_InkCollector = nullptr;
		}
		m_AllUsedTablets.Free();
		m_AllUsedTabletNames.Free();
	}
	HRESULT __stdcall QueryInterface(REFIID riid, void **ppvObject) {
		if(nullptr == ppvObject) {
			return E_POINTER;
		}
		if((riid == IID_IUnknown) || (riid == IID_IDispatch) || (riid == DIID__IInkCollectorEvents)) {
			*ppvObject = (IDispatch *)this;
			return S_OK;
		} else if(riid == IID_IMarshal) {
			return m_Unknown->QueryInterface(riid, ppvObject);
		}
		return E_NOINTERFACE;
	}
	ULONG __stdcall AddRef() {
		return 1;
	}
	ULONG __stdcall Release() {
		return 1;
	}
	HRESULT __stdcall GetTypeInfoCount(UINT *pctinfo) {
		return E_NOTIMPL;
	}
	HRESULT __stdcall GetTypeInfo(UINT itinfo, LCID lcid, ITypeInfo **pptinfo) {
		return E_NOTIMPL;
	}
	HRESULT __stdcall GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID* rgdispid) {
		return E_NOTIMPL;
	}
	HRESULT __stdcall Invoke(DISPID dispidMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pdispparams, VARIANT *pvarResult, EXCEPINFO *pexcepinfo, UINT *puArgErr) {
		switch(dispidMember) {
			case DISPID_ICEStroke:
				Stroke((IInkCursor *)pdispparams->rgvarg[2].pdispVal, (IInkStrokeDisp *)pdispparams->rgvarg[1].pdispVal, (VARIANT_BOOL *)pdispparams->rgvarg[0].pboolVal);
				break;
			case DISPID_ICECursorDown:
				CursorDown((IInkCursor *)pdispparams->rgvarg[1].pdispVal, (IInkStrokeDisp *)pdispparams->rgvarg[0].pdispVal);
				break;
			case DISPID_ICENewPackets:
				NewPackets((IInkCursor *)pdispparams->rgvarg[3].pdispVal, (IInkStrokeDisp *)pdispparams->rgvarg[2].pdispVal, pdispparams->rgvarg[1].lVal, pdispparams->rgvarg[0].pvarVal);
				break;
			case DISPID_IPEDblClick:
				DblClick((VARIANT_BOOL *)pdispparams->rgvarg[0].pboolVal);
				break;
			case DISPID_IPEMouseMove:
				MouseMove((InkMouseButton)pdispparams->rgvarg[4].lVal, (InkShiftKeyModifierFlags)pdispparams->rgvarg[3].lVal, pdispparams->rgvarg[2].lVal, pdispparams->rgvarg[1].lVal, (VARIANT_BOOL *)pdispparams->rgvarg[0].pboolVal);
				break;
			case DISPID_IPEMouseDown:
				MouseDown((InkMouseButton)pdispparams->rgvarg[4].lVal, (InkShiftKeyModifierFlags)pdispparams->rgvarg[3].lVal, pdispparams->rgvarg[2].lVal, pdispparams->rgvarg[1].lVal, (VARIANT_BOOL *)pdispparams->rgvarg[0].pboolVal);
				break;
			case DISPID_IPEMouseUp:
				MouseUp((InkMouseButton)pdispparams->rgvarg[4].lVal, (InkShiftKeyModifierFlags)pdispparams->rgvarg[3].lVal, pdispparams->rgvarg[2].lVal, pdispparams->rgvarg[1].lVal, (VARIANT_BOOL *)pdispparams->rgvarg[0].pboolVal);
				break;
			case DISPID_IPEMouseWheel:
				MouseWheel((InkMouseButton)pdispparams->rgvarg[5].lVal, (InkShiftKeyModifierFlags)pdispparams->rgvarg[4].lVal, pdispparams->rgvarg[3].lVal, pdispparams->rgvarg[2].lVal, pdispparams->rgvarg[1].lVal, (VARIANT_BOOL *)pdispparams->rgvarg[0].pboolVal);
				break;
			case DISPID_ICENewInAirPackets:
				NewInAirPackets((IInkCursor *)pdispparams->rgvarg[2].pdispVal, pdispparams->rgvarg[1].lVal, pdispparams->rgvarg[0].pvarVal);
				break;
			case DISPID_ICECursorButtonDown:
				CursorButtonDown((IInkCursor *)pdispparams->rgvarg[1].pdispVal, (IInkCursorButton *)pdispparams->rgvarg[0].pdispVal);
				break;
			case DISPID_ICECursorButtonUp:
				CursorButtonUp((IInkCursor *)pdispparams->rgvarg[1].pdispVal, (IInkCursorButton *)pdispparams->rgvarg[0].pdispVal);
				break;
			case DISPID_ICECursorInRange:
				CursorInRange((IInkCursor *)pdispparams->rgvarg[2].pdispVal, (VARIANT_BOOL)pdispparams->rgvarg[1].iVal, pdispparams->rgvarg[0]);
				break;
			case DISPID_ICECursorOutOfRange:
				CursorOutOfRange((IInkCursor *)pdispparams->rgvarg[0].pdispVal);
				break;
			case DISPID_ICESystemGesture:
				SystemGesture((IInkCursor *)pdispparams->rgvarg[6].pdispVal, (InkSystemGesture)pdispparams->rgvarg[5].lVal, pdispparams->rgvarg[4].lVal, pdispparams->rgvarg[3].lVal, pdispparams->rgvarg[2].lVal, pdispparams->rgvarg[1].bstrVal, pdispparams->rgvarg[0].lVal);
				break;
			case DISPID_ICEGesture:
				Gesture((IInkCursor *)pdispparams->rgvarg[3].pdispVal, (IInkStrokes *)pdispparams->rgvarg[2].pdispVal, pdispparams->rgvarg[1], (VARIANT_BOOL *)pdispparams->rgvarg[0].pboolVal);
				break;
			case DISPID_ICETabletAdded:
				TabletAdded((IInkTablet *)pdispparams->rgvarg[0].pdispVal);
				break;
			case DISPID_ICETabletRemoved:
				TabletRemoved(pdispparams->rgvarg[0].lVal);
				break;
			default:
				break;
		}
		return S_OK;
	}
	void Stroke(IInkCursor *Cursor, IInkStrokeDisp *Stroke, VARIANT_BOOL *Cancel) {
		m_State.SetTabletState(0, false, false);
	}
	void CursorDown(IInkCursor *Cursor, IInkStrokeDisp *Stroke) {
		VARIANT_BOOL B = VARIANT_FALSE;
		Cursor->get_Inverted(&B);
		m_State.SetTabletState(0, true, (VARIANT_TRUE == B));
	}
	void NewPackets(IInkCursor *Cursor, IInkStrokeDisp *Stroke, long PacketCount, VARIANT *PacketData) {
		int i = FindElemIndex_Pressure(Stroke);
		float v = (float)GetSafeArrayValueI4(PacketData, i);
		NormalizePressure(Cursor, &v);
		m_State.SetPressure(v);
	}
	void DblClick(VARIANT_BOOL *Cancel) {
	}
	void MouseMove(InkMouseButton Button, InkShiftKeyModifierFlags Shift, long pX, long pY, VARIANT_BOOL *Cancel) {
	}
	void MouseDown(InkMouseButton Button, InkShiftKeyModifierFlags Shift, long pX, long pY, VARIANT_BOOL *Cancel) {
	}
	void MouseUp(InkMouseButton Button, InkShiftKeyModifierFlags Shift, long pX, long pY, VARIANT_BOOL *Cancel) {
	}
	void MouseWheel(InkMouseButton Button, InkShiftKeyModifierFlags Shift, long Delta, long X, long Y, VARIANT_BOOL *Cancel) {
	}
	void NewInAirPackets(IInkCursor *Cursor, long lPacketCount, VARIANT *PacketData) {
	}
	void CursorButtonDown(IInkCursor *Cursor, IInkCursorButton *Button) {
	}
	void CursorButtonUp(IInkCursor *Cursor, IInkCursorButton *Button) {
	}
	void CursorInRange(IInkCursor *Cursor, VARIANT_BOOL NewCursor, VARIANT ButtonsState) {
	}
	void CursorOutOfRange(IInkCursor *Cursor) {
	}
	void SystemGesture(IInkCursor *Cursor, InkSystemGesture Id, long X, long Y, long Modifier, BSTR Character, long CursorMode) {
	}
	void Gesture(IInkCursor *Cursor, IInkStrokes *Strokes, VARIANT Gestures, VARIANT_BOOL *Cancel) {
	}
	void TabletAdded(IInkTablet *Tablet) {
	}
	void TabletRemoved(long TabletId) {
	}
	void GetTabletState(float *CurPressure, bool *PenPressed, bool *EraserUsed, int *LastTime) {
		m_State.GetTabletState(CurPressure, PenPressed, EraserUsed, LastTime);
	}
private:
	comms::cList<IInkTablet *> m_AllUsedTablets;
	comms::cList<comms::cStr> m_AllUsedTabletNames;
	cWinMain_TabletState m_State;
	IInkCollector *m_InkCollector;
	IConnectionPoint *m_ConnectionPoint;
	DWORD m_AdviseCookie;
	IUnknown *m_Unknown;
	int GetSafeArrayCount(VARIANT *SafeArray) {
		LONG u, l;
		SafeArrayGetUBound(SafeArray->parray, 1, &u);
		SafeArrayGetLBound(SafeArray->parray, 1, &l);
		return (int)(u - l + 1);
	}
	int FindElemIndex(IInkStrokeDisp *Stroke, const wchar_t *Name) {
		int r = -1;
		VARIANT D;
		memset(&D, 0, sizeof(D));
		Stroke->get_PacketDescription(&D);
		if(((VT_ARRAY | VT_BSTR) == D.vt) && (D.parray != nullptr)) {
			BSTR *B = nullptr;
			SafeArrayAccessData(D.parray, (void **)&B);
			int i, c = GetSafeArrayCount(&D);
			for(i = 0; i < c; i++) {
				if(0 == wcscmp(B[i], Name)) {
					r = i;
				}
			}
			SafeArrayUnaccessData(D.parray);
		}
		return r;
	}
	int FindElemIndex_Pressure(IInkStrokeDisp *Stroke) {
		return FindElemIndex(Stroke, STR_GUID_NORMALPRESSURE);
	}
	int GetSafeArrayValueI4(VARIANT *SafeArray, const int Index) {
		int v = 0;
		if((SafeArray != nullptr) && ((VT_ARRAY | VT_I4) == SafeArray->vt) && (SafeArray->parray != nullptr)) {
			int c = GetSafeArrayCount(SafeArray);
			if(Index >= 0 && Index < c) {
				int *I = nullptr;
				SafeArrayAccessData(SafeArray->parray, (void **)&I);
				v = I[Index];
				SafeArrayUnaccessData(SafeArray->parray);
			}
		}
		return v;
	}
	// GetTabletName
	void GetTabletName(IInkTablet *Tablet, comms::cStr *Name) {
		BSTR N;
		Tablet->get_Name(&N);
		int W = lstrlenW(N);
		int L = WideCharToMultiByte(CP_ACP, 0, N, W, nullptr, 0, nullptr, nullptr);
		if(L > 0) {
			Name->SetLength(L);
			WideCharToMultiByte(CP_ACP, 0, N, W, Name->ToNonConstCharPtr(), L, nullptr, nullptr);
		}
		SysFreeString(N);
	}
	// LogDevice
	void LogDevice(IInkTablet *Tablet, int PressureFrom, int PressureTo) {
		comms::cStr Name;
		GetTabletName(Tablet, &Name);
		if(!m_AllUsedTabletNames.Contains(Name)) {
			m_AllUsedTabletNames.Add(Name);
			comms::cStr Log;
			Log += comms::cStr(60, '-') + comms::cStr::EndLn;
			Log += "InkCollector device \"" + Name + "\"" + comms::cStr::EndLn;
			int Levels = PressureTo > PressureFrom ? (PressureTo - PressureFrom + 1) : 0;
			Log += "Pressure from " + comms::cStr::ToString(PressureFrom) + " to " + comms::cStr::ToString(PressureTo) + " (" + comms::cStr::ToString(Levels) + " levels)" + comms::cStr::EndLn;
			Log += comms::cStr(60, '-') + comms::cStr::EndLn;
			comms::cLog::Message(Log);
		}
	}
	void NormalizePressure(IInkCursor *Cursor, float *Pressure) {
		IInkTablet *T = nullptr;
		Cursor->get_Tablet(&T);
		if(T != nullptr) {
			long From = 0, To = 0;
			float R = 0.0f;
			TabletPropertyMetricUnit U = TPMU_Default;
			BSTR B = SysAllocString(STR_GUID_NORMALPRESSURE);
			HRESULT hr = T->GetPropertyMetrics(B, &From, &To, &U, &R);
			if(SUCCEEDED(hr)) {
				if(!m_AllUsedTablets.Contains(T)) {
					m_AllUsedTablets.Add(T);
					LogDevice(T, From, To);
				}
				float l = comms::cMath::LerperClamp01((float)From, (float)To, *Pressure);
				*Pressure = l;
			}
			SysFreeString(B);
			T->Release();
			T = nullptr;
		}
	}
};

typedef UINT(__stdcall* pFnGetDpiForWindow)(HWND);
typedef HRESULT(__stdcall *pFnGetDpiForMonitor)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);
static pFnGetDpiForWindow g_GetDpiForWindow = nullptr; // Minimum Windows 10
static pFnGetDpiForMonitor g_GetDpiForMonitor = nullptr;
int cWinMain_WindowsInkDPI = USER_DEFAULT_SCREEN_DPI;
static void cWinMain_FillWindowsInkDPI() {
	if (g_GetDpiForMonitor != nullptr) {
		HMONITOR M = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
		UINT X = 0, Y = 0;
		if (SUCCEEDED(g_GetDpiForMonitor(M, MDT_EFFECTIVE_DPI, &X, &Y))) {
			cWinMain_WindowsInkDPI = X;
		}
	}
}

static void cWinMain_FillWindowDPI() {
	if(cWinMain_hWnd != nullptr && g_GetDpiForWindow != nullptr) {
		UINT DPI = g_GetDpiForWindow(cWinMain_hWnd); // The DPI of the monitor where the window is located because "DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE" inside "cWinMain_EnableHighDPI".
		if(DPI != 0) {
			comms::cMain_DPI = DPI;
		}
	}
}

//-----------------------------------------------------------------------------
// cWinMain_RealTimeStylus
//-----------------------------------------------------------------------------
#include <rtscom.h>
#include <rtscom_i.c>
// cWinMain_RealTimeStylus_EventHandler
class cWinMain_RealTimeStylus_EventHandler : public IStylusSyncPlugin {
public:
	// ClearCapture
	void ClearCapture() {
		m_CapturedTablet.Clear();
		m_CapturedTouch.Clear();
		m_pState->SetZero();
	}
	cWinMain_RealTimeStylus_EventHandler() {
		m_RefCount = 1;
		m_Marshaler = nullptr;
		m_pState = nullptr;
	}
	virtual ~cWinMain_RealTimeStylus_EventHandler() {
		if (m_Marshaler != nullptr) {
			m_Marshaler->Release();
			m_Marshaler = nullptr;
		}
	}
	static cWinMain_RealTimeStylus_EventHandler * Create(IRealTimeStylus *pRealTimeStylus, cWinMain_TabletState *pState) {
		cWinMain_RealTimeStylus_EventHandler *pEventHandler = new cWinMain_RealTimeStylus_EventHandler();
		HRESULT hr = CoCreateFreeThreadedMarshaler(pEventHandler, &pEventHandler->m_Marshaler);
		cAssert(SUCCEEDED(hr));
		hr = pRealTimeStylus->AddStylusSyncPlugin(0, pEventHandler);
		cAssert(SUCCEEDED(hr));
		pEventHandler->m_pState = pState;
		return pEventHandler;
	}
	//-------------------------------------------------------------------------
	// StylusDown
	//-------------------------------------------------------------------------
	STDMETHOD(StylusDown)(IRealTimeStylus *piRTS, const StylusInfo *pStylusInfo, ULONG cPropCountPerPkt, LONG *pPacket, LONG **ppInOutPkt) {
		IInkTablet *piTablet = nullptr;
		HRESULT hr = piRTS->GetTabletFromTabletContextId(pStylusInfo->tcid, &piTablet);
		cAssert(SUCCEEDED(hr));
		if (piTablet != nullptr) {
			TabletDeviceKind K = GetTabletKind(piTablet);
			if (TDK_Pen == K) {
				float P = 0.0f, F, T;
				int Levels;
				GetTabletPressureRange(piTablet, &F, &T, &Levels);
				cAssert(pPacket != nullptr && cPropCountPerPkt >= 3);
				if (pPacket != nullptr && cPropCountPerPkt >= 3) {
					P = PressureFromLong(F, T, pPacket[2]);
				}
				bool Eraser = (pStylusInfo->bIsInvertedCursor != 0) && !comms::cSettings::GetInstance()->TreatEraserAsPen;
				m_pState->SetTabletState(P, true, Eraser);
				m_CapturedTablet.tcid = pStylusInfo->tcid;
				m_CapturedTablet.PressureRange.Set(F, T);
				m_CapturedTablet.PenPos = PosFromPackets(nullptr, 1, cPropCountPerPkt, pPacket);
				LogNewDevice(piTablet, Levels);
				comms::cInputEvent_Pen *Pen = new comms::cInputEvent_Pen;
				Pen->Type = comms::cInputEvent_Pen::TYPE::Down;
				Pen->Eraser = Eraser;
				Pen->PenPos = m_CapturedTablet.PenPos;
				Pen->PenPressure = P;
				comms::cInput::AddEvent(Pen); Pen = nullptr;
			}
			else if (TDK_Touch == K) {
				if (0 == m_CapturedTouch.tcid) {
					m_CapturedTouch.tcid = pStylusInfo->tcid;
					cAssert(m_CapturedTouch.Touches_cid.IsEmpty() && m_CapturedTouch.Touches_Pos.IsEmpty());
				}
				if (m_CapturedTouch.tcid == pStylusInfo->tcid) {
					cAssert(!m_CapturedTouch.Touches_cid.Contains(pStylusInfo->cid));
					m_CapturedTouch.Touches_cid.Add(pStylusInfo->cid);
					comms::cVec2 Pos(0.0f);
					cAssert(pPacket != nullptr && cPropCountPerPkt >= 2);
					if (pPacket != nullptr && cPropCountPerPkt >= 2) {
						Pos = PosFromPackets(nullptr, 1, cPropCountPerPkt, pPacket);
						comms::cInputEvent_Touch *Touch = new comms::cInputEvent_Touch;
						Touch->Type = comms::cInputEvent_Touch::TYPE::Down;
						Touch->TouchID = pStylusInfo->cid;
						Touch->TouchPos = Pos;
						comms::cInput::AddEvent(Touch); Touch = nullptr;
					}
					m_CapturedTouch.Touches_Pos.Add(Pos);
				}
			}
			piTablet->Release(); piTablet = nullptr;
		}
		return S_OK;
	} // StylusDown
	//-------------------------------------------------------------------------
	// StylusUp
	//-------------------------------------------------------------------------
	STDMETHOD(StylusUp)(IRealTimeStylus *piRTS, const StylusInfo *pStylusInfo, ULONG cPropCountPerPkt, LONG *pPacket, LONG **ppInOutPkt) {
		if (m_CapturedTablet.tcid == pStylusInfo->tcid) {
			m_pState->SetZero();
			m_CapturedTablet.Clear();
			comms::cInputEvent_Pen *Pen = new comms::cInputEvent_Pen;
			Pen->Type = comms::cInputEvent_Pen::TYPE::Up;
			comms::cInput::AddEvent(Pen); Pen = nullptr;
		}
		else if (m_CapturedTouch.tcid == pStylusInfo->tcid) {
			int n = m_CapturedTouch.Touches_cid.IndexOf(pStylusInfo->cid);
			cAssert(n != -1);
			if (n != -1) {
				m_CapturedTouch.Touches_cid.RemoveAt(n);
				m_CapturedTouch.Touches_Pos.RemoveAt(n);
				comms::cInputEvent_Touch *Touch = new comms::cInputEvent_Touch;
				Touch->Type = comms::cInputEvent_Touch::TYPE::Up;
				Touch->TouchID = pStylusInfo->cid;
				comms::cInput::AddEvent(Touch); Touch = nullptr;
			}
			if (m_CapturedTouch.Touches_cid.IsEmpty()) {
				m_CapturedTouch.Clear();
			}
		}
		return S_OK;
	} // StylusUp
	//-------------------------------------------------------------------------
	// Packets
	//-------------------------------------------------------------------------
	STDMETHOD(Packets)(IRealTimeStylus *piRTS, const StylusInfo *pStylusInfo, ULONG cPktCount, ULONG cPktBuffLength, LONG *pPackets, ULONG *pcInOutPkts, LONG **ppInOutPkts) {
		if (m_CapturedTablet.tcid == pStylusInfo->tcid) {
			comms::cList<comms::cVec2> Positions;
			PosFromPackets(&Positions, cPktCount, cPktBuffLength, pPackets);
			ULONG cPropertyCount = cPktBuffLength / cPktCount;
			cAssert(cPropertyCount >= 3);
			comms::cList<float> Pressures;
			if (cPropertyCount >= 3) {
				for (ULONG i = 0; i < cPktBuffLength; i += cPropertyCount) {
					float P = PressureFromLong(m_CapturedTablet.PressureRange[0], m_CapturedTablet.PressureRange[1], pPackets[i + 2]);
					m_pState->SetPressure(P);
					Pressures.Add(P);
				}
			}
			cAssert(Positions.Count() == Pressures.Count());
			if (Positions.Count() == Pressures.Count()) {
				for (int i = 0; i < Positions.Count(); i++) {
					comms::cInputEvent_Pen *Pen = new comms::cInputEvent_Pen;
					Pen->Type = comms::cInputEvent_Pen::TYPE::Move;
					Pen->PenPos = Positions[i];
					Pen->PenDelta = Positions[i] - m_CapturedTablet.PenPos;
					m_CapturedTablet.PenPos = Positions[i];
					Pen->PenPressure = Pressures[i];
					comms::cInput::AddEvent(Pen); Pen = nullptr;
				}
			}
		}
		else if (m_CapturedTouch.tcid == pStylusInfo->tcid) {
			int n = m_CapturedTouch.Touches_cid.IndexOf(pStylusInfo->cid);
			cAssert(n != -1);
			if (n != -1) {
				comms::cList<comms::cVec2> Positions;
				PosFromPackets(&Positions, cPktCount, cPktBuffLength, pPackets);
				for (int i = 0; i < Positions.Count(); i++) {
					comms::cInputEvent_Touch *Touch = new comms::cInputEvent_Touch;
					Touch->Type = comms::cInputEvent_Touch::TYPE::Move;
					Touch->TouchID = pStylusInfo->cid;
					Touch->TouchPos = Positions[i];
					Touch->TouchDelta = Positions[i] - m_CapturedTouch.Touches_Pos[n];
					comms::cInput::AddEvent(Touch); Touch = nullptr;
					m_CapturedTouch.Touches_Pos[n] = Positions[i];
				}
			}
		}
		return S_OK;
	} // Packets
	// DataInterest
	STDMETHOD(DataInterest)(RealTimeStylusDataInterest *pDataInterest) {
		*pDataInterest = (RealTimeStylusDataInterest)(RTSDI_StylusDown | RTSDI_Packets | RTSDI_StylusUp | RTSDI_StylusInRange | RTSDI_StylusOutOfRange);
		return S_OK;
	}
	STDMETHOD(RealTimeStylusEnabled)(IRealTimeStylus *, ULONG, const TABLET_CONTEXT_ID *) { return S_OK; }
	STDMETHOD(RealTimeStylusDisabled)(IRealTimeStylus *, ULONG, const TABLET_CONTEXT_ID *) { return S_OK; }
	STDMETHOD(StylusInRange)(IRealTimeStylus *piRTS, TABLET_CONTEXT_ID tcid, STYLUS_ID) {
		IInkTablet *piTablet = nullptr;
		HRESULT hr = piRTS->GetTabletFromTabletContextId(tcid, &piTablet);
		cAssert(SUCCEEDED(hr));
		if(piTablet != nullptr) {
			TabletDeviceKind K = GetTabletKind(piTablet);
			if(TDK_Pen == K) {
				cWinMain_Proximity = true;
			}
			piTablet->Release(); piTablet = nullptr;
		}
		return S_OK;
	}
	STDMETHOD(StylusOutOfRange)(IRealTimeStylus *piRTS, TABLET_CONTEXT_ID tcid, STYLUS_ID) {
		IInkTablet* piTablet = nullptr;
		HRESULT hr = piRTS->GetTabletFromTabletContextId(tcid, &piTablet);
		cAssert(SUCCEEDED(hr));
		if(piTablet != nullptr) {
			TabletDeviceKind K = GetTabletKind(piTablet);
			if(TDK_Pen == K) {
				cWinMain_Proximity = false;
			}
			piTablet->Release(); piTablet = nullptr;
		}
		return S_OK;
	}
	STDMETHOD(InAirPackets)(IRealTimeStylus *, const StylusInfo *, ULONG, ULONG, LONG *, ULONG *, LONG **) { return S_OK; }
	STDMETHOD(StylusButtonUp)(IRealTimeStylus *, STYLUS_ID, const GUID *, POINT *) { return S_OK; }
	STDMETHOD(StylusButtonDown)(IRealTimeStylus *, STYLUS_ID, const GUID *, POINT *) { return S_OK; }
	STDMETHOD(SystemEvent)(IRealTimeStylus *, TABLET_CONTEXT_ID, STYLUS_ID, SYSTEM_EVENT, SYSTEM_EVENT_DATA) { return S_OK; }
	STDMETHOD(TabletAdded)(IRealTimeStylus *, IInkTablet *) { return S_OK; }
	STDMETHOD(TabletRemoved)(IRealTimeStylus *, LONG) { return S_OK; }
	STDMETHOD(CustomStylusDataAdded)(IRealTimeStylus *, const GUID *, ULONG, const BYTE *) { return S_OK; }
	STDMETHOD(Error)(IRealTimeStylus *, IStylusPlugin *, RealTimeStylusDataInterest, HRESULT, LONG_PTR *) { return S_OK; }
	STDMETHOD(UpdateMapping)(IRealTimeStylus *) { return S_OK; }

	STDMETHOD_(ULONG, AddRef)() {
		return InterlockedIncrement(&m_RefCount);
	}
	STDMETHOD_(ULONG, Release)() {
		ULONG NewRefCount = InterlockedDecrement(&m_RefCount);
		if (0 == NewRefCount) {
			delete this;
		}
		return NewRefCount;
	}
	STDMETHOD(QueryInterface)(REFIID riid, LPVOID *ppvObj) {
		if ((riid == IID_IStylusSyncPlugin) || (riid == IID_IUnknown)) {
			*ppvObj = this;
			AddRef();
			return S_OK;
		}
		else if (riid == IID_IMarshal) {
			cAssert(m_Marshaler != nullptr);
			return m_Marshaler->QueryInterface(riid, ppvObj);
		}
		*ppvObj = nullptr;
		return E_NOINTERFACE;
	}
private:
	cWinMain_TabletState *m_pState;
	LONG m_RefCount;
	IUnknown *m_Marshaler;
	struct CapturedTablet {
		TABLET_CONTEXT_ID tcid;
		comms::cVec2 PressureRange;
		comms::cVec2 PenPos;
		void Clear() {
			tcid = 0;
			PressureRange.SetZero();
			PenPos.SetZero();
		}
		CapturedTablet() {
			Clear();
		}
	};
	CapturedTablet m_CapturedTablet;
	struct CapturedTouch {
		TABLET_CONTEXT_ID tcid;
		comms::cList<CURSOR_ID> Touches_cid;
		comms::cList<comms::cVec2> Touches_Pos;
		void Clear() {
			tcid = 0;
			Touches_cid.Clear();
			Touches_Pos.Clear();
		}
		CapturedTouch() {
			Clear();
		}
	};
	CapturedTouch m_CapturedTouch;
	// PosFromPackets
	static const comms::cVec2 PosFromPackets(comms::cList<comms::cVec2> *Positions, ULONG cPktCount, ULONG cPktBuffLength, LONG *pPackets) {
		RECT rc;
		GetClientRect(cWinMain_hWnd, &rc);
		comms::cVec2 Pos(0.0f);
		if (Positions != nullptr) {
			Positions->Clear();
		}
		ULONG cPropertyCount = cPktBuffLength / cPktCount;
		cAssert(cPropertyCount >= 2);
		if (cPropertyCount >= 2) {
			// Regardless of "Scale" and "Resolution" in "Settings > System > Display" the calls below return 96:
			// "HDC hDC = GetDC(cWinMain_hWnd); int LX = GetDeviceCaps(hDC, LOGPIXELSX); int LY = GetDeviceCaps(hDC, LOGPIXELSY);"
			// "ReleaseDC(cWinMain_hWnd, hDC); hDC = nullptr;"
			for (ULONG i = 0; i < cPktBuffLength; i += cPropertyCount) {
				// Metric unit of coords in the packets are 1/100 of millimeter
				Pos.x = float(pPackets[i]) * float(cWinMain_WindowsInkDPI) / 2540.0f;
				Pos.y = float(rc.bottom) - float(pPackets[i + 1]) * float(cWinMain_WindowsInkDPI) / 2540.0f;
				if (Positions != nullptr) {
					Positions->Add(Pos);
				}
			}
		}
		return Pos;
	}
	// GetTabletName
	static void GetTabletName(IInkTablet *Tablet, comms::cStr *Name) {
		BSTR N = nullptr;
		Tablet->get_Name(&N);
		int W = lstrlenW(N);
		int L = WideCharToMultiByte(CP_ACP, 0, N, W, nullptr, 0, nullptr, nullptr);
		if (L > 0) {
			Name->SetLength(L);
			WideCharToMultiByte(CP_ACP, 0, N, W, Name->ToNonConstCharPtr(), L, nullptr, nullptr);
		}
		SysFreeString(N); N = nullptr;
	}
	// GetTabletPressureRange
	static void GetTabletPressureRange(IInkTablet *Tablet, float *PressureFrom, float *PressureTo, int *PressureLevels) {
		*PressureFrom = 0.0f; *PressureTo = 1.0f;
		*PressureLevels = 0;
		long From = 0, To = 0;
		float R = 0.0f;
		TabletPropertyMetricUnit U = TPMU_Default;
		BSTR B = SysAllocString(STR_GUID_NORMALPRESSURE);
		HRESULT hr = Tablet->GetPropertyMetrics(B, &From, &To, &U, &R);
		if (SUCCEEDED(hr)) {
			*PressureFrom = (float)From; *PressureTo = (float)To;
			*PressureLevels = To - From;
		}
		SysFreeString(B); B = nullptr;
	}
	// GetTabletKind
	static TabletDeviceKind GetTabletKind(IInkTablet *Tablet) {
		TabletDeviceKind K = TDK_Mouse;
		IInkTablet2 *piTablet2 = nullptr;
		Tablet->QueryInterface(IID_IInkTablet2, (void **)&piTablet2);
		if (piTablet2 != nullptr) {
			piTablet2->get_DeviceKind(&K);
			piTablet2->Release(); piTablet2 = nullptr;
		}
		return K;
	}
	// PressureFromLong
	static float PressureFromLong(const float From, const float To, LONG PacketPressure) {
		float l = comms::cMath::LerperClamp01(From, To, (float)PacketPressure);
		return l;
	}
	comms::cList<IInkTablet *> m_AllUsedTablets;
	comms::cList<comms::cStr> m_AllUsedTabletNames;
	//-------------------------------------------------------------------------
	// LogNewDevice
	//-------------------------------------------------------------------------
	void LogNewDevice(IInkTablet *piTablet, int Levels) {
		if (m_AllUsedTablets.Contains(piTablet)) {
			return;
		}
		m_AllUsedTablets.Add(piTablet);
		comms::cStr N;
		GetTabletName(piTablet, &N);
		if (m_AllUsedTabletNames.Contains(N)) {
			return;
		}
		m_AllUsedTabletNames.Add(N);
		comms::cStr Log;
		Log += comms::cStr(60, '-') + comms::cStr::EndLn;
		Log += "Real Time Stylus \"" + N + "\" (" + comms::cStr::ToString(Levels) + " pressure levels)" + comms::cStr::EndLn;
		Log += comms::cStr(60, '-') + comms::cStr::EndLn;
		comms::cLog::Message(Log);
	} // LogNewDevice
};

class cWinMain_RealTimeStylus : public cWinMain_TabletBase {
public:
	cWinMain_RealTimeStylus() {
		m_RealTimeStylus = nullptr;
		m_EventHandler = nullptr;
	}
	bool Init() {
		m_State.Init();
		HRESULT hr = CoInitialize(nullptr);
		cAssert(SUCCEEDED(hr));
		cAssert(nullptr == m_RealTimeStylus);
		hr = CoCreateInstance(CLSID_RealTimeStylus, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_RealTimeStylus));
		cAssert(SUCCEEDED(hr));
		if (m_RealTimeStylus != nullptr) {
			hr = m_RealTimeStylus->put_HWND((HANDLE_PTR)cWinMain_hWnd);
			cAssert(SUCCEEDED(hr));
			IRealTimeStylus3 *RealTimeStylus3 = nullptr;
			hr = m_RealTimeStylus->QueryInterface(&RealTimeStylus3);
			cAssert(SUCCEEDED(hr));
			if (RealTimeStylus3 != nullptr) {
				hr = RealTimeStylus3->put_MultiTouchEnabled(TRUE);
				cAssert(SUCCEEDED(hr));
				RealTimeStylus3->Release(); RealTimeStylus3 = nullptr;
			}
			m_EventHandler = cWinMain_RealTimeStylus_EventHandler::Create(m_RealTimeStylus, &m_State);
			hr = m_RealTimeStylus->put_Enabled(TRUE);
			cAssert(SUCCEEDED(hr));
		}
		return true;
	}
	void Free() {
		m_State.Free();
		if (m_EventHandler != nullptr) {
			m_EventHandler->Release(); m_EventHandler = nullptr;
		}
		if (m_RealTimeStylus != nullptr) {
			m_RealTimeStylus->Release(); m_RealTimeStylus = nullptr;
		}
	}
	void GetTabletState(float *CurPressure, bool *PenPressed, bool *EraserUsed, int *LastTime) {
		m_State.GetTabletState(CurPressure, PenPressed, EraserUsed, LastTime);
	}
private:
	IRealTimeStylus *m_RealTimeStylus;
	IStylusSyncPlugin *m_EventHandler;
	cWinMain_TabletState m_State;
}; // cWinMain_RealTimeStylus

namespace comms {

//-----------------------------------------------------------------------------
// cWinMain_TabletSelector
//-----------------------------------------------------------------------------
class cWinMain_TabletSelector {
public:
	static void ApplyTabletLibrary() {
		static int PrevLibraryType = -1;
		if (PrevLibraryType == cSettings::GetInstance()->TabletLibrary) {
			return;
		}
		cAssert(cSettings::GetInstance()->TabletLibrary >= 0 && cSettings::GetInstance()->TabletLibrary < cTabletLibrary::Count);
		if(cSettings::GetInstance()->TabletLibrary < 0 || cSettings::GetInstance()->TabletLibrary >= cTabletLibrary::Count) {
			return;
		}
		Free();
		PrevLibraryType = cSettings::GetInstance()->TabletLibrary;
		cAssert(nullptr == s_CurLibraryPtr);
		switch (PrevLibraryType) {
		case cTabletLibrary::WinTab:
			s_CurLibraryPtr = new cWinMain_WinTab;
			if (s_CurLibraryPtr->Init()) {
				cSettings::GetInstance()->CurrentTabletLibrary = cTabletLibrary::WinTab;
				break;
			}
			FreeLibrary();
			// No "break" here because in case "WinTab" has failed to init
			// we will use "WindowsInk" library regardless of the settings.
		case cTabletLibrary::WindowsInk:
			s_CurLibraryPtr = new cWinMain_RealTimeStylus;
			if (s_CurLibraryPtr->Init()) {
				cSettings::GetInstance()->CurrentTabletLibrary = cTabletLibrary::WindowsInk;
				break;
			}
			FreeLibrary();
			break;
		default:
			cAssert(0);
		}
	}
	static void Free() {
		FreeLibrary();
	}
	static void GetTabletState(float *CurPressure, bool *PenPressed, bool *EraserUsed, int *LastTime) {
		if(s_CurLibraryPtr != nullptr) {
			s_CurLibraryPtr->GetTabletState(CurPressure, PenPressed, EraserUsed, LastTime);
		}
	}
	static void WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
		if (s_CurLibraryPtr != nullptr) {
			s_CurLibraryPtr->WndProc(hWnd, Msg, wParam, lParam);
		}
	}
private:
	static cWinMain_TabletBase *s_CurLibraryPtr;
	static void FreeLibrary() {
		if (s_CurLibraryPtr != nullptr) {
			s_CurLibraryPtr->Free();
			delete s_CurLibraryPtr;
			s_CurLibraryPtr = nullptr;
		}
	}
}; // cWinMain_TabletSelector
cWinMain_TabletBase *cWinMain_TabletSelector::s_CurLibraryPtr = nullptr;

} // comms

#endif // COMMS_TABLET

// cMain_GetTabletState
void comms::cMain_GetTabletState(float *CurPressure, bool *PenPressed, bool *EraserUsed,int* LastUsedTime) {
	if(CurPressure != nullptr) {
		*CurPressure = 0.0f;
	}
	if(PenPressed != nullptr) {
		*PenPressed = false;
	}
	if(EraserUsed != nullptr) {
		*EraserUsed = false;
	}
	if(LastUsedTime != nullptr) {
		*LastUsedTime = -1;
	}
#ifdef COMMS_TABLET
	cWinMain_TabletSelector::GetTabletState(CurPressure, PenPressed, EraserUsed, LastUsedTime);
#endif // COMMS_TABLET
}

static void cWinMain_FullscreenHandler() {
	if(comms::cSettings::GetInstance()->FullScreen && !cWinMain_FullScreen) {
		cWinMain_SetFullScreen();
	} else if(!comms::cSettings::GetInstance()->FullScreen && cWinMain_FullScreen) {
		cWinMain_SetWindowed();
	}
}

void cWinMain_OnRender() {
	if (!cWinMain_AllowOnRender) {
		return;
	}
	if(nullptr == comms::cRender::GetInstance() || comms::cLog::IsVisible()) {
		return;
	}
	cWinMain_FullscreenHandler();
	RECT rc;
	GetClientRect(cWinMain_hWnd, &rc);
	int Width = rc.right - rc.left;
	int Height = rc.bottom - rc.top;
	if(Width < 1 || Height < 1) {
		return;
	}

	comms::cRect Viewport;
	Viewport.SetBottomLeft(0.0f, 0.0f);
	Viewport.SetTopRight((float)Width, (float)Height);

	comms::cMain_OnRender(Viewport);

#ifdef COMMS_TABLET
	comms::cWinMain_TabletSelector::ApplyTabletLibrary();
#endif // COMMS_TABLET
} // cWinMain_OnRender

namespace comms {

#if defined(COMMS_3DCONNEXION) 
#include "../Libs/3Dconnexion/Windows/Old/si.h"
#include "../Libs/3Dconnexion/Windows/Old/siapp.h"

#ifdef COMMS_64
#pragma comment (lib, "Libs/3Dconnexion/Windows/Old/x64/siapp.lib")
#else // 32
#pragma comment (lib, "Libs/3Dconnexion/Windows/Old/x86/siapp.lib")
#endif // COMMS_64

// cWinMain_3DconnexionOld
class cWinMain_3DconnexionOld {
public:
	cVec3 Translation;
	cVec3 Rotation;
	cVec2i ButtonState;
	std::mutex Mutex;
	
	cWinMain_3DconnexionOld() {
		Translation.SetZero();
		Rotation.SetZero();
		ButtonState.Set(0);
		m_Device = nullptr;
	}
	void Free() {
	}
	void Init();
	void WndProc(UINT Msg, WPARAM wParam, LPARAM lParam);
private:
	bool LibInit();
	SiHdl m_Device;
	cStr m_Name;
};
cWinMain_3DconnexionOld g_3DconnexionOld;

// cWinMain_3DconnexionOld::Init
void cWinMain_3DconnexionOld::Init() {
	
	cStr L;
	if(LibInit()) {
		L = "3Dconnexion Device Name: " + m_Name;
	} else {
		L = "No 3Dconnexion driver";
	}
	cLog::Message(comms::cStr(60, '-') + cStr::EndLn + L + cStr::EndLn + comms::cStr(60, '-'));
}

// cWinMain_3DconnexionOld::LibInit
bool cWinMain_3DconnexionOld::LibInit() {
	SiOpenData OD;
	if(SiInitialize() == SPW_DLL_LOAD_ERROR) {
		return false;
	}
	SiOpenWinInit(&OD, cWinMain_hWnd);
	SiSetUiMode(m_Device, SI_UI_NO_CONTROLS);
	if((m_Device = SiOpen(cMain_Title.ToCharPtr(), SI_ANY_DEVICE, SI_NO_MASK, SI_EVENT, &OD)) == nullptr) {
		SiTerminate();
		return false;
	} else {
		SiDeviceName DN;
		SiGetDeviceName(m_Device, &DN);
		m_Name.Copy(DN.name);
		return true;
	}
}

// cWinMain_3DconnexionOld::WndProc
void cWinMain_3DconnexionOld::WndProc(UINT Msg, WPARAM wParam, LPARAM lParam) {
	if (!UseOld3DConnexionAPI())return;
	if(nullptr == m_Device) {
		return;
	}
	SiSpwEvent Event;
	SiGetEventData Data;
	SiGetEventWinInit(&Data, Msg, wParam, lParam);
	if(SiGetEvent(m_Device, SI_AVERAGE_EVENTS, &Data, &Event) == SI_IS_EVENT) {
		comms::cRender::NeedUpdate();
		g_3DconnexionOld.Mutex.lock();
		if(Event.type == SI_MOTION_EVENT) {
			Translation.x = (float)Event.u.spwData.mData[SI_TX];
			Translation.y = (float)Event.u.spwData.mData[SI_TY];
			Translation.z = (float)Event.u.spwData.mData[SI_TZ];
			Rotation.x = (float)Event.u.spwData.mData[SI_RX];
			Rotation.y = (float)Event.u.spwData.mData[SI_RY];
			Rotation.z = (float)Event.u.spwData.mData[SI_RZ];
		} else if(Event.type == SI_ZERO_EVENT) {
			Translation.SetZero();
			Rotation.SetZero();
		} else if((Event.type == SI_BUTTON_PRESS_EVENT) || (Event.type == SI_BUTTON_RELEASE_EVENT)) {
			// START > All apps > 3Dconnexion > 3Dconnexion Properties > Buttons > LEFT = Application Use, RIGHT = Application Use
			if(V3DK_MENU == Event.u.hwButtonEvent.buttonNumber) {
				ButtonState[0] = ((Event.type == SI_BUTTON_PRESS_EVENT) ? 1 : 0);
			} else if(V3DK_FIT == Event.u.hwButtonEvent.buttonNumber) {
				ButtonState[1] = ((Event.type == SI_BUTTON_PRESS_EVENT) ? 1 : 0);
			}
		} else if(Event.type == SI_DEVICE_CHANGE_EVENT) {
		} else if(Event.type == SI_CMD_EVENT) {
		}
		g_3DconnexionOld.Mutex.unlock();
	}
}
bool cMain_TdxTimeProportional() {
	return false;
}
// cMain_GetTdxState
void cMain_GetTdxState(cVec3 *Translation, cVec3 *Rotation, cVec2i *ButtonState) {
	if(!UseOld3DConnexionAPI()) {
		if (Translation != nullptr) {
			*Translation = cVec3::Zero;
		}
		if (Rotation != nullptr) {
			*Rotation = cVec3::Zero;
		}
		if (ButtonState != nullptr) {
			*ButtonState = cVec2i(0);
		}
	}
	else {
		g_3DconnexionOld.Mutex.lock();
		if (Translation != nullptr) {
			*Translation = g_3DconnexionOld.Translation;
		}
		if (Rotation != nullptr) {
			*Rotation = g_3DconnexionOld.Rotation;
		}
		if (ButtonState != nullptr) {
			*ButtonState = g_3DconnexionOld.ButtonState;
		}
		g_3DconnexionOld.Mutex.unlock();
	}
}
#endif // COMMS_3DCONNEXION

#if !defined(COMMS_3DCONNEXION)
bool cMain_TdxTimeProportional() {
	return false;
}
// cMain_GetTdxState
void cMain_GetTdxState(cVec3 *Translation, cVec3 *Rotation, cVec2i *ButtonState) {
    if(Translation != nullptr) {
        *Translation = cVec3::Zero;
    }
    if(Rotation != nullptr) {
        *Rotation = cVec3::Zero;
    }
    if(ButtonState != nullptr) {
        ButtonState->Set(0);
    }
}
#endif // !COMMS_3DCONNEXION

} // comms

static HINSTANCE g_User32Dll = nullptr;
static HINSTANCE g_ShcoreDll = nullptr;
typedef BOOL (__stdcall *pFnSetGestureConfig)(HWND, DWORD, UINT, PGESTURECONFIG, UINT);
typedef BOOL (__stdcall *pFnCloseGestureInfoHandle)(HGESTUREINFO);
typedef BOOL (__stdcall *pFnGetGestureInfo)(HGESTUREINFO, PGESTUREINFO);
typedef BOOL (__stdcall *pFnSetProcessDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);

static pFnSetGestureConfig g_SetGestureConfig = nullptr;
static pFnCloseGestureInfoHandle g_CloseGestureInfoHandle = nullptr;
static pFnGetGestureInfo g_GetGestureInfo = nullptr;
static pFnSetProcessDpiAwarenessContext g_SetProcessDpiAwarenessContext = nullptr;

// InitGesturesAndDpi
static void InitGesturesAndDpi() {
	if(nullptr == g_User32Dll) {
		g_User32Dll = LoadLibrary("User32.dll");
	}
	if(g_User32Dll != nullptr) {
		g_SetGestureConfig = (pFnSetGestureConfig)GetProcAddress(g_User32Dll, "SetGestureConfig");
		g_CloseGestureInfoHandle = (pFnCloseGestureInfoHandle)GetProcAddress(g_User32Dll, "CloseGestureInfoHandle");
		g_GetGestureInfo = (pFnGetGestureInfo)GetProcAddress(g_User32Dll, "GetGestureInfo");
		#ifdef COMMS_TABLET
		g_GetDpiForWindow = (pFnGetDpiForWindow)GetProcAddress(g_User32Dll, "GetDpiForWindow");
		#endif // COMMS_TABLET
		g_SetProcessDpiAwarenessContext = (pFnSetProcessDpiAwarenessContext)GetProcAddress(g_User32Dll, "SetProcessDpiAwarenessContext");
	}
	if (nullptr == g_ShcoreDll) {
		g_ShcoreDll = LoadLibrary("Shcore.dll");
	}
	if (g_ShcoreDll != nullptr) {
		#ifdef COMMS_TABLET
		g_GetDpiForMonitor = (pFnGetDpiForMonitor)GetProcAddress(g_ShcoreDll, "GetDpiForMonitor");
		#endif // COMMS_TABLET
	}
}

// ConfigureGestures
static void ConfigureGestures() {
	// https://msdn.microsoft.com/en-us/library/windows/desktop/dd353241(v=vs.85).aspx
	DWORD PanWant  = GC_PAN_WITH_SINGLE_FINGER_VERTICALLY | GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY;
	DWORD PanBlock = GC_PAN_WITH_GUTTER | GC_PAN_WITH_INERTIA;
	GESTURECONFIG GC[] = {
		{ GID_PAN, PanWant, PanBlock },
		{ GID_ZOOM, GC_ZOOM, 0 },
		{ GID_ROTATE, GC_ROTATE, 0 }
	};
	int c = sizeof(GC) / sizeof(GC[0]);
	if(g_SetGestureConfig != nullptr) {
		g_SetGestureConfig(cWinMain_hWnd, 0, c, GC, sizeof(GESTURECONFIG));
	}
}

// HandleGesture
static void HandleGesture(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	// https://msdn.microsoft.com/en-us/library/windows/desktop/dd353242(v=vs.85).aspx
	GESTUREINFO GI;
	ZeroMemory(&GI, sizeof(GI));
	GI.cbSize = sizeof(GI);
	static int PrevX = 0, PrevY = 0, PrevDist = 0, PrevAngle = 0;
	static bool IgnoreFirstRotationAngle = false; // Ignoring the first rotation angle to prevent noticeable "jump" at the beginning of rotation gesture
	static bool IgnoreFirstScalingFactor = false; // Ignoring the first scaling factor to prevent noticeable "jump" at the beginning of scale gesture
	int CurX, CurY, DeltaX, DeltaY, CurDist, CurAngle;
	float Scale, Rotate;
	comms::cStr M;
	if((g_GetGestureInfo != nullptr) && g_GetGestureInfo((HGESTUREINFO)lParam, &GI)) {
		switch(GI.dwID) {
			case GID_BEGIN:
				M = "GID_BEGIN";
				PrevX = GI.ptsLocation.x;
				PrevY = GI.ptsLocation.y;
				M += " | PrevX, PrevY = " + comms::cStr::ToString(PrevX) + ", " + comms::cStr::ToString(PrevY);
				PrevDist = PrevAngle = (int)GI.ullArguments;
				M += " | PrevDist = " + comms::cStr::ToString(PrevDist);
				M += " | PrevAngle = " + comms::cStr::ToString(PrevAngle);
				break;
			case GID_END:
				M = "GID_END";
				CurX = GI.ptsLocation.x;
				CurY = GI.ptsLocation.y;
				M += " | X, Y = " + comms::cStr::ToString(CurX) + ", " + comms::cStr::ToString(CurY);
				CurDist = CurAngle = (int)GI.ullArguments;
				M += " | CurDist = " + comms::cStr::ToString(CurDist);
				M += " | CurAngle = " + comms::cStr::ToString(CurAngle);
				break;
			case GID_PAN:
				M = "GID_PAN";
				if(GI.dwFlags & GF_BEGIN) {
					M += " | GF_BEGIN";
					PrevX = GI.ptsLocation.x;
					PrevY = GI.ptsLocation.y;
					M += " | PrevX, PrevY = " + comms::cStr::ToString(PrevX) + ", " + comms::cStr::ToString(PrevY);
					PrevDist = (int)GI.ullArguments;
					M += " | PrevDist = " + comms::cStr::ToString(PrevDist);
					
					comms::cInputEvent_Gesture *E = new comms::cInputEvent_Gesture;
					E->GestureType = (0 == PrevDist) ? comms::cGestureType::Translate1 : comms::cGestureType::Translate2;
					E->GestureState = comms::cInputEvent_Gesture::STATE::Down;
					cWinMain_GlobalToLocal(PrevX, PrevY, &E->GestureCenter);
					comms::cInput::AddEvent(E);
					E = nullptr;
				} else if(GI.dwFlags & GF_END) {
					M += " | GF_END";
					CurX = GI.ptsLocation.x;
					CurY = GI.ptsLocation.y;
					M += " | X, Y = " + comms::cStr::ToString(CurX) + ", " + comms::cStr::ToString(CurY);
					CurDist = (int)GI.ullArguments;
					M += " | CurDist = " + comms::cStr::ToString(CurDist);
					
					comms::cInputEvent_Gesture *E = new comms::cInputEvent_Gesture;
					E->GestureType = (0 == CurDist) ? comms::cGestureType::Translate1 : comms::cGestureType::Translate2;
					E->GestureState = comms::cInputEvent_Gesture::STATE::Up;
					comms::cInput::AddEvent(E);
					E = nullptr;
				} else {
					CurX = GI.ptsLocation.x;
					CurY = GI.ptsLocation.y;
					M += " | X, Y = " + comms::cStr::ToString(CurX) + ", " + comms::cStr::ToString(CurY);
					CurDist = (int)GI.ullArguments;
					M += " | CurDist = " + comms::cStr::ToString(CurDist);

					DeltaX = CurX - PrevX;
					DeltaY = CurY - PrevY;
					Scale = (PrevDist != 0) ? ((float)CurDist / (float)PrevDist) : 1.0f;

					PrevX = CurX;
					PrevY = CurY;
					PrevDist = CurDist;
					
					comms::cInputEvent_Gesture *E = new comms::cInputEvent_Gesture;
					E->GestureType = (0 == CurDist) ? comms::cGestureType::Translate1 : comms::cGestureType::Translate2;
					E->GestureState = comms::cInputEvent_Gesture::STATE::Move;
					cWinMain_GlobalToLocal(CurX, CurY, &E->GestureCenter);
					E->GestureTranslate.Set((float)DeltaX, -(float)DeltaY);
					E->GestureScale = Scale;
					comms::cInput::AddEvent(E);
					E = nullptr;
				}
				break;
			case GID_ZOOM:
				M = "GID_ZOOM";
				if(GI.dwFlags & GF_BEGIN) {
					M += " | GF_BEGIN";
					PrevX = GI.ptsLocation.x;
					PrevY = GI.ptsLocation.y;
					M += " | PrevX, PrevY = " + comms::cStr::ToString(PrevX) + ", " + comms::cStr::ToString(PrevY);
					PrevDist = (int)GI.ullArguments;
					M += " | PrevDist = " + comms::cStr::ToString(PrevDist);
					IgnoreFirstScalingFactor = true;

					comms::cInputEvent_Gesture *E = new comms::cInputEvent_Gesture;
					E->GestureType = comms::cGestureType::Scale;
					E->GestureState = comms::cInputEvent_Gesture::STATE::Down;
					cWinMain_GlobalToLocal(PrevX, PrevY, &E->GestureCenter);
					comms::cInput::AddEvent(E);
					E = nullptr;
				} else if(GI.dwFlags & GF_END) {
					M += " | GF_END";
					CurX = GI.ptsLocation.x;
					CurY = GI.ptsLocation.y;
					M += " | X, Y = " + comms::cStr::ToString(CurX) + ", " + comms::cStr::ToString(CurY);
					CurDist = (int)GI.ullArguments;
					M += " | CurDist = " + comms::cStr::ToString(CurDist);

					comms::cInputEvent_Gesture *E = new comms::cInputEvent_Gesture;
					E->GestureType = comms::cGestureType::Scale;
					E->GestureState = comms::cInputEvent_Gesture::STATE::Up;
					comms::cInput::AddEvent(E);
					E = nullptr;
				} else {
					CurX = GI.ptsLocation.x;
					CurY = GI.ptsLocation.y;
					M += " | X, Y = " + comms::cStr::ToString(CurX) + ", " + comms::cStr::ToString(CurY);
					CurDist = (int)GI.ullArguments;
					M += " | CurDist = " + comms::cStr::ToString(CurDist);

					DeltaX = CurX - PrevX;
					DeltaY = CurY - PrevY;
					Scale = (float)CurDist / (float)PrevDist;
					
					PrevX = CurX;
					PrevY = CurY;
					PrevDist = CurDist;
					
					comms::cInputEvent_Gesture *E = new comms::cInputEvent_Gesture;
					E->GestureType = comms::cGestureType::Scale;
					E->GestureState = comms::cInputEvent_Gesture::STATE::Move;
					cWinMain_GlobalToLocal(CurX, CurY, &E->GestureCenter);
					E->GestureTranslate.Set((float)DeltaX, -(float)DeltaY);
					E->GestureScale = IgnoreFirstScalingFactor ? 1.0f : Scale;
					IgnoreFirstScalingFactor = false;
					comms::cInput::AddEvent(E);
					E = nullptr;
				}
				break;
			case GID_ROTATE:
				M = "GID_ROTATE";
				if(GI.dwFlags & GF_BEGIN) {
					M += " | GF_BEGIN";
					PrevX = GI.ptsLocation.x;
					PrevY = GI.ptsLocation.y;
					M += " | PrevX, PrevY = " + comms::cStr::ToString(PrevX) + ", " + comms::cStr::ToString(PrevY);
					PrevAngle = (int)GI.ullArguments;
					M += " | PrevAngle = " + comms::cStr::ToString(PrevAngle);
					IgnoreFirstRotationAngle = true;

					comms::cInputEvent_Gesture *E = new comms::cInputEvent_Gesture;
					E->GestureType = comms::cGestureType::Rotate;
					E->GestureState = comms::cInputEvent_Gesture::STATE::Down;
					cWinMain_GlobalToLocal(PrevX, PrevY, &E->GestureCenter);
					comms::cInput::AddEvent(E);
					E = nullptr;
				} else if(GI.dwFlags & GF_END) {
					M += " | GF_END";
					CurX = GI.ptsLocation.x;
					CurY = GI.ptsLocation.y;
					M += " | X, Y = " + comms::cStr::ToString(CurX) + ", " + comms::cStr::ToString(CurY);
					CurAngle = (int)GI.ullArguments;
					M += " | CurAngle = " + comms::cStr::ToString(CurAngle);

					comms::cInputEvent_Gesture *E = new comms::cInputEvent_Gesture;
					E->GestureType = comms::cGestureType::Rotate;
					E->GestureState = comms::cInputEvent_Gesture::STATE::Up;
					comms::cInput::AddEvent(E);
					E = nullptr;
				} else {
					CurX = GI.ptsLocation.x;
					CurY = GI.ptsLocation.y;
					M += " | X, Y = " + comms::cStr::ToString(CurX) + ", " + comms::cStr::ToString(CurY);
					CurAngle = (int)GI.ullArguments;
					M += " | CurAngle = " + comms::cStr::ToString(CurAngle);

					DeltaX = CurX - PrevX;
					DeltaY = CurY - PrevY;
					Rotate = comms::cMath::Deg(comms::cMath::AngleDeltaRad((float)GID_ROTATE_ANGLE_FROM_ARGUMENT(CurAngle), (float)GID_ROTATE_ANGLE_FROM_ARGUMENT(PrevAngle)));

					PrevX = CurX;
					PrevY = CurY;
					PrevAngle = CurAngle;
					
					comms::cInputEvent_Gesture *E = new comms::cInputEvent_Gesture;
					E->GestureType = comms::cGestureType::Rotate;
					E->GestureState = comms::cInputEvent_Gesture::STATE::Move;
					cWinMain_GlobalToLocal(CurX, CurY, &E->GestureCenter);
					E->GestureTranslate.Set((float)DeltaX, -(float)DeltaY);
					E->GestureRotate = IgnoreFirstRotationAngle ? 0.0f : Rotate;
					IgnoreFirstRotationAngle = false;
					comms::cInput::AddEvent(E);
					E = nullptr;
				}
				break;
			case GID_TWOFINGERTAP:
				break;
			case GID_PRESSANDTAP:
				break;
			default:
				break;
		}
	}
	if(!M.IsEmpty()) {
		//comms::cLog::Message(M);
	}
	if(g_CloseGestureInfoHandle != nullptr) {
		g_CloseGestureInfoHandle((HGESTUREINFO)lParam);
	}
}

// cWinMain_RestoreWindowState
static void cWinMain_RestoreWindowState() {
	if (nullptr == cWinMain_hWnd) {
		return;
	}
	// Default state
	memset(&cWinMain_WindowPlacement, 0, sizeof(cWinMain_WindowPlacement));
	cWinMain_WindowPlacement.length = sizeof(cWinMain_WindowPlacement);
	GetWindowPlacement(cWinMain_hWnd, &cWinMain_WindowPlacement);
	cWinMain_WindowPlacement.showCmd = SW_SHOWMAXIMIZED;
	// Read the state from optional file
	if (!comms::cMain_WindowStateFile.IsEmpty()) {
#ifdef COMMS_3DCOAT
		TagsList t;
		comms::cStr wp = comms::cMain_WindowStateFile;
		t.ReadTagsFromFile(wp);
		if (t.GetSubTagsCount() >= 10) {
			WINDOWPLACEMENT& w = cWinMain_WindowPlacement;
			*t["flags"] >> (int&)w.flags;
			*t["showCmd"] >> (int&)w.showCmd;
			*t["minx"] >> (int&)w.ptMinPosition.x;
			*t["miny"] >>  (int&)w.ptMinPosition.y;
			*t["maxx"] >> (int&)w.ptMaxPosition.x;
			*t["maxy"] >> (int&)w.ptMaxPosition.y;
			*t["left"] >> (int&)w.rcNormalPosition.left;
			*t["top"] >> (int&)w.rcNormalPosition.top;
			*t["right"] >> (int&)w.rcNormalPosition.right;
			*t["bottom"] >> (int&)w.rcNormalPosition.bottom;
			w.length = sizeof(w);
		}
#endif // COMMS_3DCOAT
	}
	// Apply the state but hide the window for now (we will show the window after "cRender::Init")
	WINDOWPLACEMENT WP = cWinMain_WindowPlacement;
	WP.showCmd = SW_HIDE;
	SetWindowPlacement(cWinMain_hWnd, &WP);
}

// cWinMain_StoreWindowState
void cWinMain_StoreWindowState() {
	if (nullptr == cWinMain_hWnd) {
		return;
	}
	if (comms::cMain_WindowStateFile.IsEmpty()) {
		return;
	}
	WINDOWPLACEMENT WP;
	memset(&WP, 0, sizeof(WP));
	WP.length = sizeof(WP);
	if (!GetWindowPlacement(cWinMain_hWnd, &WP)) {
		return;
	}
	comms::cIO::RemoveFile(comms::cMain_WindowStateFile);
	comms::cStr wf = comms::cMain_WindowStateFile;
#ifdef COMMS_3DCOAT
	TagsList t;
	t << "#flags" << int(WP.flags) << "#showCmd" << int(WP.showCmd);
	t << "#minx" << WP.ptMinPosition.x << "#miny" << WP.ptMinPosition.y;
	t << "#maxx" << WP.ptMaxPosition.x << "#maxy" << WP.ptMaxPosition.y;
	t << "#left" << WP.rcNormalPosition.left;
	t << "#top" << WP.rcNormalPosition.top;
	t << "#right" << WP.rcNormalPosition.right;
	t << "#bottom" << WP.rcNormalPosition.bottom;
	if(AppOpt.StoreWindowState) {
		t.WriteTagsToFile(wf);
	}
#endif // COMMS_3DCOAT
}

//-----------------------------------------------------------------------------
// WndProc
// ----------------------------------------------------------------------------
LRESULT WINAPI WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	HWND hFW = nullptr;
	BOOL Active, Minimized;
	comms::cInput::OldEvent *E = nullptr;
	int X, Y, DPI;
	static int PrevX = -1, PrevY = -1;
	comms::cStr S;

#ifdef COMMS_TABLET
	comms::cWinMain_TabletSelector::WndProc(hWnd, Msg, wParam, lParam);
#endif // COMMS_TABLET
#if defined(COMMS_3DCONNEXION)
	comms::g_3DconnexionOld.WndProc(Msg, wParam, lParam);
#endif // COMMS_3DCONNEXION
	switch(Msg) {
		case WM_GESTURE:
			HandleGesture(hWnd, Msg, wParam, lParam);
			return 0;
		case WM_SYSCOMMAND: // To avoid sealing on Alt key in window w/ system menu
			if(SC_SCREENSAVE == wParam || SC_KEYMENU == wParam) {
				return 0;
			}
			break;
		case WM_SETCURSOR:
			if((1 == ((int)lParam & 0xffff)) && (cWinMain_CurCursorIndex != -1) && (cWinMain_CurCursorIndex != comms::cInput::Cursor::None)) {
				::SetCursor(cWinMain_hCursors[cWinMain_CurCursorIndex]);
				return TRUE;
			}
			break;
		case WM_MOUSEMOVE:
			X = (int)(lParam & 0xffff);
			Y = (int)(lParam >> 16);
			E = new comms::cInput::OldEvent;
			E->Type = comms::cInput::OldEvent::TYPE_MOUSEMOVE;
			cWinMain_ClientToLocal(X, Y, &E->MousePos);
			if(-1 == PrevX || -1 == PrevY) {
				E->MouseDelta.SetZero();
			} else {
				E->MouseDelta.Set((float)(X - PrevX), -(float)(Y - PrevY));
			}
			comms::cInput::AddEvent(E);
			E = nullptr;
#ifndef COMMS_3DCOAT
			if (comms::cInput::Cursor::None == cWinMain_CurCursorIndex) {
				// Center mouse cursor to not move it when it is hidden
				RECT rc;
				GetClientRect(cWinMain_hWnd, &rc);
				PrevX = (rc.right - rc.left) / 2;
				PrevY = (rc.bottom - rc.top) / 2;
				POINT P = { PrevX, PrevY };
				ClientToScreen(cWinMain_hWnd, &P);
				SetCursorPos(P.x, P.y);
			} else {
#endif // !3DCoat
				PrevX = X;
				PrevY = Y;
#ifndef COMMS_3DCOAT
			}
#endif // !3DCoat
			return 0;
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			if((WM_KEYDOWN == Msg || WM_SYSKEYDOWN == Msg) && ((lParam & (1 << 30)) != 0)) {
				break; // Filter auto repeated down messages
			}
			E = new comms::cInput::OldEvent;
			E->Type = comms::cInput::OldEvent::TYPE_BUTTON;
			E->Code = cWinMain_CodesRemap[(int)wParam];
			E->Pressed = ((WM_KEYDOWN == Msg) || (WM_SYSKEYDOWN == Msg));
			comms::cInput::AddEvent(E);
			E = nullptr;
			break;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
			E = new comms::cInput::OldEvent;
			E->Type = comms::cInput::OldEvent::TYPE_BUTTON;
			if(WM_XBUTTONDOWN == Msg || WM_XBUTTONUP == Msg || WM_XBUTTONDBLCLK == Msg) {
				E->Code = comms::cInput::XButton1;
			} else if(WM_LBUTTONDOWN == Msg || WM_LBUTTONUP == Msg || WM_LBUTTONDBLCLK == Msg) {
				E->Code = comms::cInput::LeftButton;
			} else if(WM_RBUTTONDOWN == Msg || WM_RBUTTONUP == Msg || WM_RBUTTONDBLCLK == Msg) {
				E->Code = comms::cInput::RightButton;
			} else {
				E->Code = comms::cInput::MiddleButton;
			}
			E->Pressed = (WM_LBUTTONDOWN == Msg || WM_RBUTTONDOWN == Msg || WM_MBUTTONDOWN == Msg || WM_XBUTTONDOWN == Msg ||
				WM_LBUTTONDBLCLK == Msg || WM_RBUTTONDBLCLK == Msg || WM_MBUTTONDBLCLK == Msg || WM_XBUTTONDBLCLK == Msg);
			E->DoubleClick = (WM_LBUTTONDBLCLK == Msg || WM_RBUTTONDBLCLK == Msg || WM_MBUTTONDBLCLK == Msg || WM_XBUTTONDBLCLK == Msg);
			if(comms::cSettings::GetInstance()->IgnoreDoubleClicksFromPen && cWinMain_Proximity) {
				E->DoubleClick = false;  // This disables double clicks from the tablet
			}
			X = (int)(lParam & 0xffff);
			Y = (int)(lParam >> 16);
			cWinMain_ClientToLocal(X, Y, &E->MousePos);
			comms::cInput::AddEvent(E);
			E = nullptr;
			return 0;
		case WM_MOUSEWHEEL:
			E = new comms::cInput::OldEvent;
			E->Type = comms::cInput::OldEvent::TYPE_BUTTON;
			E->WheelDelta = (float)((short)HIWORD(wParam)) / (float)WHEEL_DELTA;
			E->Code = E->WheelDelta > 0.0f ? comms::cInput::WheelUp : comms::cInput::WheelDown;
			E->Pressed = true;
			X = (int)(lParam & 0xffff);
			Y = (int)(lParam >> 16);
			cWinMain_GlobalToLocal(X, Y, &E->MousePos);
			comms::cInput::AddEvent(E);
			E = nullptr;
			return 0;
		case WM_CREATE:
			return 0;
		case WM_DESTROY:
			cWinMain_StoreWindowState();
			PostQuitMessage(0);
			return 0;
		case WM_SIZING:
			comms::cWin32::OnSizing(hWnd, wParam, lParam, 400, 300);
		case WM_SIZE:
		case WM_MOVE:
			cWinMain_OnRender();
			return 0;
		case WM_ERASEBKGND:
			if(comms::cPause::GetSystemPause()) {
				cWinMain_EraseBkgnd = true;
			}
			return TRUE;
		case WM_ENTERSIZEMOVE: // We should block input events and pause the timer during sizing the window
			comms::cLog::Message("WM_ENTERSIZEMOVE");
			cWinMain_EnableInputEvents = false;
			comms::cPause::SetSystemPause(true);
			cWinMain_OnRender();
			break;
		case WM_EXITSIZEMOVE:
			comms::cLog::Message("WM_EXITSIZEMOVE");
			cWinMain_EnableInputEvents = true;
			comms::cPause::SetSystemPause(false);
			break;
		case WM_ACTIVATEAPP:
			comms::cLog::Message("WM_ACTIVATEAPP");
			break;
		case WM_ACTIVATE:
			comms::cLog::Message("WM_ACTIVATE");
			hFW = GetForegroundWindow();
			Active = hFW == cWinMain_hWnd || (comms::cWinMain_NonmodalDialog != nullptr && hFW == comms::cWinMain_NonmodalDialog);
			Minimized = (BOOL)HIWORD(wParam);
			// This eliminates unwanted input messages which are consequent of loosing / gaining focus
			if(Active) {
				comms::cInput::Acquire();
				comms::cInput::FreeEvents();
			}
			comms::cPause::SetSystemPause(!Active || Minimized);
			comms::cMain_OnActivate();
			break;
		case WM_CLOSE:
			if(!comms::cMain_OnClose()) {
				return 0;
			}
			break;
		case WM_DPICHANGED:
			// Source: https://docs.microsoft.com/en-us/windows/win32/hidpi/wm-dpichanged
			{
			DPI = HIWORD(wParam);
			const float ScaleFactorInSystemSettings = (float)DPI / (float)USER_DEFAULT_SCREEN_DPI;
			return 0;
			}
			break;
		default:
			break;
	}
	return DefWindowProc(hWnd, Msg, wParam, lParam);
} // WndProc

static bool cWinMain_FontInited = false;
static LOGFONT cWinMain_LogFont;
static bool cWinMain_FontChanged;

static void save_font() {
#ifdef COMMS_3DCOAT
	FileWriteBinStream b("data/Temp/font.bin");
	b.Write(&cWinMain_LogFont, sizeof(cWinMain_LogFont));
#endif // COMMS_3DCOAT
}

static void load_font() {
#ifdef COMMS_3DCOAT
	FileReadBinStream b("data/Temp/font.bin");
	if (b.Size() == sizeof(cWinMain_LogFont)) {
		b.Read(&cWinMain_LogFont, sizeof(cWinMain_LogFont));
		cWinMain_FontChanged = true;
	}
#endif // COMMS_3DCOAT
}

// cWinMain_FontInit
static void cWinMain_FontInit() {
	if(cWinMain_FontInited) {
		return;
	}
	cWinMain_FontInited = true;
	
	memset(&cWinMain_LogFont, 0, sizeof(cWinMain_LogFont));
	strcpy_s(cWinMain_LogFont.lfFaceName, "Times New Roman");
	cWinMain_LogFont.lfHeight = -96*4;
	cWinMain_FontChanged = true;
	load_font();
}

namespace comms {

#ifdef COMMS_3DCOAT
void cMain_FontToJson(TagsList& F) {
	F.AddSubTag("FaceName", cWinMain_LogFont.lfFaceName);
	F.AddSubTag("Height", int(cWinMain_LogFont.lfHeight));
	F.AddSubTag("Width", int(cWinMain_LogFont.lfWidth));
	F.AddSubTag("Weight", int(cWinMain_LogFont.lfWeight));
	F.AddSubTag("Italic", int(cWinMain_LogFont.lfItalic));
	F.AddSubTag("Escapement", int(cWinMain_LogFont.lfEscapement));
	F.AddSubTag("Orientation", int(cWinMain_LogFont.lfOrientation));
	F.AddSubTag("Underline", int(cWinMain_LogFont.lfUnderline));
	F.AddSubTag("StrikeOut", int(cWinMain_LogFont.lfStrikeOut));
	F.AddSubTag("CharSet", int(cWinMain_LogFont.lfCharSet));
	F.AddSubTag("OutPrecision", int(cWinMain_LogFont.lfOutPrecision));
	F.AddSubTag("ClipPrecision", int(cWinMain_LogFont.lfClipPrecision));
	F.AddSubTag("Quality", int(cWinMain_LogFont.lfQuality));
	F.AddSubTag("PitchAndFamily", int(cWinMain_LogFont.lfPitchAndFamily));
}

void cMain_FontFromJson(TagsList& F) {
	cWinMain_FontInit();
	auto* q=F.GetSubTag("FaceName");
	if (q) strcpy_s(cWinMain_LogFont.lfFaceName, q->body());
	q = F.GetSubTag("Height");
	if(q) cWinMain_LogFont.lfHeight = q->to_int();
	q = F.GetSubTag("Width");
	if (q) cWinMain_LogFont.lfWidth = q->to_int();
	q = F.GetSubTag("Weight");
	if (q) cWinMain_LogFont.lfWeight = q->to_int();
	q = F.GetSubTag("Italic");
	if (q) cWinMain_LogFont.lfItalic = q->to_int();
	q = F.GetSubTag("Escapement");
	if (q) cWinMain_LogFont.lfEscapement = q->to_int();
	q = F.GetSubTag("Orientation");
	if (q) cWinMain_LogFont.lfOrientation = q->to_int();
	q = F.GetSubTag("Underline");
	if (q) cWinMain_LogFont.lfUnderline = q->to_int();
	q = F.GetSubTag("StrikeOut");
	if (q) cWinMain_LogFont.lfStrikeOut = q->to_int();
	q = F.GetSubTag("CharSet");
	if (q) cWinMain_LogFont.lfCharSet = q->to_int();
	q = F.GetSubTag("OutPrecision");
	if (q) cWinMain_LogFont.lfOutPrecision = q->to_int();
	q = F.GetSubTag("ClipPrecision");
	if (q) cWinMain_LogFont.lfClipPrecision = q->to_int();
	q = F.GetSubTag("Quality");
	if (q) cWinMain_LogFont.lfQuality = q->to_int();
	q = F.GetSubTag("PitchAndFamily");
	if (q) cWinMain_LogFont.lfPitchAndFamily = q->to_int();
	cWinMain_FontChanged = true;
}
#endif // COMMS_3DCOAT

void cMain_GetFont(LOGFONT** F) {
	cWinMain_FontInit();
	*F = &cWinMain_LogFont;
}
// cMain_GetFontChanged
bool cMain_GetFontChanged() {
    cWinMain_FontInit();

    bool r = cWinMain_FontChanged;
    cWinMain_FontChanged = false;
    return r;
}
// cMain_SetFontChanged
void cMain_SetFontChanged() {
	cWinMain_FontInit();
	cWinMain_FontChanged = true;
}



// cMain_GetFontName
const cStr cMain_GetFontName() {
    cWinMain_FontInit();

	return cStr(cWinMain_LogFont.lfFaceName);
}

void cMain_SetFontName(const char* fn) {
	if (strlen(fn) < 32) {
		cWinMain_FontChanged = true;
		strcpy_s(cWinMain_LogFont.lfFaceName, LF_FACESIZE, fn);
		save_font();
	}
}

// cMain_ChooseFont
void cMain_ChooseFont() {
    cWinMain_FontInit();
	
	CHOOSEFONT cf;
	ZeroMemory(&cf, sizeof(cf));
	cf.lStructSize = sizeof(cf);
	cf.hwndOwner = cWinMain_hWnd;
	LOGFONT lf = cWinMain_LogFont;
	cf.lpLogFont = &cWinMain_LogFont;
	cf.rgbColors = 0xFF000000;
	cf.Flags = CF_SCREENFONTS | CF_EFFECTS | CF_INITTOLOGFONTSTRUCT;

	cWinMain_FontChanged = true;
	if (cWinMain_LogFont.lfHeight < 0){
		cWinMain_LogFont.lfHeight = -40;
	}
	else{
		cWinMain_LogFont.lfHeight = 40;
	}

	if(ChooseFont(&cf)) {
		cWinMain_FontChanged = true;
		if (cWinMain_LogFont.lfHeight < 0){
			if (cWinMain_LogFont.lfHeight > -96 * 4){
				cWinMain_LogFont.lfHeight = -96 * 4;
			}
		}else{
			if (cWinMain_LogFont.lfHeight < 96 * 4){
				cWinMain_LogFont.lfHeight = 96 * 4;
			}
		}
		save_font();
	} else {
		cWinMain_LogFont = lf;
	}
}

//-----------------------------------------------------------------------------------------
// cMain_DrawText
//-----------------------------------------------------------------------------------------
void cMain_SetTextSize(const int Size) {
	cWinMain_FontInit();
	cWinMain_LogFont.lfHeight = -Size;
	save_font();
}
int cMain_GetTextSize() {
	cWinMain_FontInit();
	return -cWinMain_LogFont.lfHeight;
}
void cMain_DrawText(const char *Text, comms::cImage *To, int *TextWidth, int *TextHeight) {
    cWinMain_FontInit();
	HDC hDC = nullptr;
	INT n;
	HFONT hFont = nullptr;
	HGDIOBJ hOldFont;
	SIZE S;
	wchar_t wc[4096];
	int L, W, H, BytesPerRow, TotalSize;
	DWORD *pBitmapBits;
	BITMAPINFO bmi;
	HBITMAP hBitmap;
	HGDIOBJ hOldBmp;
	
	// Measuring text
	hDC = CreateCompatibleDC(nullptr);
	n = -MulDiv(12, (INT)(GetDeviceCaps(hDC, LOGPIXELSY) * 1.0f), 72);
	cWinMain_LogFont.lfQuality = ANTIALIASED_QUALITY;
	for(int i=0;i<10;i++){
		hFont = CreateFontIndirect(&cWinMain_LogFont);
		hOldFont = SelectObject(hDC, hFont);
		SetTextColor(hDC, RGB(255, 255, 255));
		SetBkColor(hDC, 0);
		SetTextAlign(hDC, TA_TOP);
		L = MultiByteToWideChar(CP_UTF8, 0, Text, (int)strlen(Text), wc, 4096);
		wc[L] = 0;
		GetTextExtentPoint32W(hDC, wc, L, &S);
		if (S.cx < 4096)break;
		if (S.cx > 0){
			cWinMain_LogFont.lfHeight *= 4000;
			cWinMain_LogFont.lfHeight /= S.cx;
		}
	}
	*TextWidth = S.cx;
	*TextHeight = S.cy;
	// Drawing text
	W = comms::cMath::Max(32, comms::cMath::UpperPowerOfTwo(*TextWidth));
    H = comms::cMath::Max(32, comms::cMath::UpperPowerOfTwo(*TextHeight));
	if (W > 16384) {
		W = 16384;
    }
    if(H > 4096) {
        H = 4096;
    }
	ZeroMemory(&bmi.bmiHeader, sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = W;
	bmi.bmiHeader.biHeight = H;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount = 32;
	hBitmap = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, (VOID **)&pBitmapBits, nullptr, 0);
	SetMapMode(hDC, MM_TEXT);
	hOldBmp = SelectObject(hDC, hBitmap);
	ExtTextOutW(hDC, 0, 0, ETO_OPAQUE, nullptr, wc, L, nullptr);
	// Copy dib
	To->Create(cFormat::Rgba8, W, H, 1, 1);
	BytesPerRow = cFormat::BytesPerPixel(cFormat::Rgba8) * W;
	TotalSize = BytesPerRow * H;
	memcpy(To->GetPixels(), pBitmapBits, TotalSize);
	// Clean up
	SelectObject(hDC, hOldBmp);
	SelectObject(hDC, hOldFont);
	DeleteObject(hBitmap);
	DeleteObject(hFont);
	DeleteDC(hDC);
} // cMain_DrawText

// cMain_SetWindowTitle
void cMain_SetWindowTitle(const char *Title) {
	static cStr CurTitle;
	if(cWinMain_hWnd != nullptr) {
		if(!cStr::Equals(Title, CurTitle)) {
			CurTitle = Title;
			SetWindowTextW(cWinMain_hWnd, comms::wchar_path(CurTitle));
		}
	}
}

// cMain_GetClientWidth
int cMain_GetClientWidth() {
	RECT rc;
	GetClientRect(cWinMain_hWnd, &rc);
	int W = rc.right - rc.left;
	return W;
}

// cMain_GetClientHeight
int cMain_GetClientHeight() {
	RECT rc;
	GetClientRect(cWinMain_hWnd, &rc);
	int H = rc.bottom - rc.top;
	return H;
}

// cMain_Quit
void cMain_Quit() {
	if(cWinMain_hWnd != nullptr) {
		DestroyWindow(cWinMain_hWnd);
		cWinMain_hWnd = nullptr;
	}
	PostQuitMessage(0);
}

//-----------------------------------------------------------------------------
// cWinMain_FileDialog
//-----------------------------------------------------------------------------
class cWinMain_FileDialog {
public:
	bool LoadMode; // To save set "false"

	// In
	comms::cStr Title;
	comms::cList<comms::cStr> Extensions;
	comms::cStr DefaultExtension;	// Can be empty
	comms::cStr InitialFolder;		// Can be empty
	comms::cStr InitialFileBase;	// Can be empty (Used only when "false == LoadMode")
	comms::cStr InitialFileName;	// Can be empty (Used only when "true == LoadMode")

	wchar_path wInitialFolder;
	wchar_path wTitle;
	
	// Out
	bool Success; // Is "false" when canceled
	// These fields are only valid when "true == Success"
	comms::cStr *SingleFilePn;
	comms::cList<comms::cStr> *MultiFilePn;

	cWinMain_FileDialog() {
		SingleFilePn = nullptr;
		MultiFilePn = nullptr;
	}
	~cWinMain_FileDialog() {
	}
	void Init(HWND hParentWnd);
	void Run();

private:
	struct {
		DWORD			lStructSize;
		HWND			hwndOwner;
		HINSTANCE		hInstance;
		LPCWSTR			lpstrFilter;
		LPWSTR			lpstrCustomFilter;
		DWORD			nMaxCustFilter;
		DWORD			nFilterIndex;
		LPWSTR			lpstrFile;
		DWORD			nMaxFile;
		LPWSTR			lpstrFileTitle;
		DWORD			nMaxFileTitle;
		LPCWSTR			lpstrInitialDir;
		LPCWSTR			lpstrTitle;
		DWORD			Flags;
		WORD			nFileOffset;
		WORD			nFileExtension;
		LPCWSTR			lpstrDefExt;
		LPARAM			lCustData;
		LPOFNHOOKPROC	lpfnHook;
		LPCWSTR			lpTemplateName;
		
		// _WIN32_WINNT >= 0x0500
		void			*pvReserved;
		DWORD			dwReserved;
		DWORD			FlagsEx;
	} m_ofn;

	void SetFilter();
	comms::cStr m_Filter;
	std::wstring m_wFilter;
	cList<wchar_t> m_Buffer;

	static UINT_PTR CALLBACK HookProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
	bool FileExist(const char *FilePn);
	void FixExtension(cStr *FilePn);
}; // cWinMain_FileDialog

// cWinMain_FileDialog::FileExist
bool cWinMain_FileDialog::FileExist(const char *FilePn) {
#ifdef COMMS_3DCOAT
	return CheckIfFileExists(FilePn);
#else // FRAME
	struct _stat Buffer;
	int r = _stat(FilePn, &Buffer);
	return r != -1 && (Buffer.st_mode & _S_IFREG);
#endif // COMMS_3DCOAT
}

//-----------------------------------------------------------------------------
// cWinMain_FileDialog::Init
//-----------------------------------------------------------------------------
void cWinMain_FileDialog::Init(HWND hParentWnd) {
	//*************************************************************************
	// Process extensions
	//*************************************************************************
	int i;
	for(i = 0; i < Extensions.Count();) {
		comms::cStr &r = Extensions[i];
		//if(r.IsEmpty()) {
		//	Extensions.RemoveAt(i);
		//	continue;
		//}
		r.MakeLower();
		if('.' == r[0]) {
			r.Remove(0, 1);
		}
		i++;
	}
	
	if(!DefaultExtension.IsEmpty()) {
		DefaultExtension.MakeLower();
		if('.' == DefaultExtension[0]) {
			DefaultExtension.Remove(0, 1);
		}
		i = Extensions.IndexOf(DefaultExtension, comms::cStr::Equals);
		if(i != -1 && i != 0) {
			comms::cMath::Swap(Extensions[0], Extensions[i]);
		}
	}

	//****************************************************************************
	// m_ofn
	//****************************************************************************
	memset(&m_ofn, 0, sizeof(m_ofn));
	m_ofn.lStructSize = sizeof(m_ofn);
	m_ofn.hwndOwner = hParentWnd;
	wTitle = wchar_path(Title);;
	m_ofn.lpstrTitle = wTitle;

	SetFilter();

	int BufferSize = MAX_PATH;
	if(MultiFilePn != nullptr) {
		cAssert(LoadMode);
		BufferSize *= 100;
	}
	m_Buffer.SetCount(BufferSize, 0);
	m_ofn.lpstrFile = m_Buffer.ToPtr();
	m_ofn.nMaxFile = m_Buffer.Count();
	wInitialFolder = wchar_path(InitialFolder);
	m_ofn.lpstrInitialDir = wInitialFolder;
	m_ofn.lpstrTitle = wTitle;
	m_ofn.lpfnHook = (LPOFNHOOKPROC)HookProc;
	m_ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_ENABLESIZING;
	if(MultiFilePn != nullptr) {
		cAssert(LoadMode);
		m_ofn.Flags |= OFN_ALLOWMULTISELECT;
	}
	// Windows Vista will show the new file dialog only if an app does not customize the dialog at all: no callback function (no OFN_ENABLEHOOK flag) and no template.
	// Vista has the new COM interfaces IFileOpenDialog and IFileSaveDialog. They replace the GetOpenFileName() and GetSaveFileName() APIs. Callback function is not present in Vista.
	// Here we query for IFileOpenDialog interface to determine whether this is Vista. If the interface is available we doesn't use the hook to center the dialog.
	IUnknown *IFileOpenDialog_Ptr = nullptr;
	GUID IFileOpenDialog_CLSID = { 0xDC1C5A9C, 0xE88A, 0x4dde, 0xA5, 0xA1, 0x60, 0xF8, 0x2A, 0x20, 0xAE, 0xF7 };
	GUID IFileOpenDialog_IID = { 0xd57c7288, 0xd4ad, 0x4768, 0xbe, 0x02, 0x9d, 0x96, 0x95, 0x32, 0xd9, 0x60 };
	CoCreateInstance(IFileOpenDialog_CLSID, nullptr, CLSCTX_ALL, IFileOpenDialog_IID, (void **)&IFileOpenDialog_Ptr);
	if(nullptr == IFileOpenDialog_Ptr) {
		m_ofn.Flags |= OFN_ENABLEHOOK;
	}
	if(IFileOpenDialog_Ptr != nullptr) {
		IFileOpenDialog_Ptr->Release();
		IFileOpenDialog_Ptr = nullptr;
	}
	if(LoadMode) {
		m_ofn.Flags |= OFN_FILEMUSTEXIST;
	} else {
		m_ofn.Flags |= OFN_OVERWRITEPROMPT;
	}
} // cWinMain_FileDialog::Init

//--------------------------------------------------------------------------------------------------
// cWinMain_FileDialog::HookProc
//--------------------------------------------------------------------------------------------------
UINT_PTR CALLBACK cWinMain_FileDialog::HookProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam) {
	switch(Msg) {
		case WM_INITDIALOG:
			cWin32::CenterWindow(GetParent(hDlg), nullptr);
			break;
		default:
			break;
	}
	return FALSE;
} // cWinMain_FileDialog::HookProc

//-----------------------------------------------------------------------------
// cWinMain_FileDialog::SetFilter
//-----------------------------------------------------------------------------
void cWinMain_FileDialog::SetFilter() {
	m_Filter.Clear();
	cStr Label, Mask;
	int i;
	for(i = 0; i < Extensions.Count(); i++) {
		const cStr &r = Extensions[i];
		if(LoadMode) {
			if(!Label.IsEmpty()) {
				Label += " ";
			}
			Label += r;
			Mask += cStr::Format("*.%s;", r.ToCharPtr());
		} else {
			m_Filter += cStr::Format("%s|*.%s|", r.ToCharPtr(), r.ToCharPtr());
		}
	}
	if(LoadMode) {
		m_Filter = Label + "|" + Mask + "|";
	}
	
	m_Filter.Append('|');
	m_Filter.toWstring(m_wFilter);
	int sz = (int)m_wFilter.size();
	for(int i=0;i<sz;i++) {
		if (m_wFilter[i] == '|')m_wFilter[i] = 0;
	}
	m_Filter.Replace('|', '\0');	
	m_Filter.CalcLength();
	m_ofn.lpstrFilter = m_wFilter.c_str();
	m_ofn.nFilterIndex = 1;
} // FileDialog::SetFilter

//-----------------------------------------------------------------------------
// cWinMain_FileDialog::Run
//-----------------------------------------------------------------------------
void cWinMain_FileDialog::Run() {
	Success = false;
	if(Extensions.IsEmpty()) {
		return;
	}

	// Storing CurDir
	cStr CurDir(MAX_PATH);
	GetCurrentDirectory(MAX_PATH, CurDir.ToNonConstCharPtr());
	CurDir.CalcLength();

	if(SingleFilePn != nullptr) {
		SingleFilePn->Clear();
	}
	if(MultiFilePn != nullptr) {
		MultiFilePn->Clear();
	}
	m_Buffer.Fill(0);
	if(LoadMode) {
		if(!InitialFileName.IsEmpty()) {
			wcscpy_s(m_Buffer.ToPtr(), m_Buffer.Count(), wchar_path(InitialFileName));
		}
	} else {
		if(!InitialFileBase.IsEmpty()) {
			wcscpy_s(m_Buffer.ToPtr(), m_Buffer.Count(), wchar_path(InitialFileBase));
		}
	}
	BOOL Load = LoadMode ? GetOpenFileNameW(reinterpret_cast<OPENFILENAMEW *>(&m_ofn)) : GetSaveFileNameW(reinterpret_cast<OPENFILENAMEW *>(&m_ofn));
	int End, i, j;
	cStr S;
	if(Load) {
		if(SingleFilePn != nullptr) {
			std::wstring ws = m_ofn.lpstrFile;
			cStr fn = ws;
			SingleFilePn->Copy(cIO::EnsureAbsolutePath(fn));
			Success = true;
		}
		if(MultiFilePn != nullptr) {
			cList<wchar_t *> Ptrs;
			for(End = m_Buffer.Count() - 1; End > 0; End--) {
				if(m_Buffer[End - 1] != 0) {
					break;
				}
			}
			for(i = 0, j = 0; i <= End; i++) {
				if(0 == m_Buffer[i]) {
					Ptrs.Add(&m_Buffer[j]);
					j = i + 1;
				}
			}
			if(1 == Ptrs.Count()) { // [0] - Full Path Name
				std::wstring ws(Ptrs[0]);
				cStr s = ws;
				MultiFilePn->Add(cIO::EnsureAbsolutePath(s));
				Success = true;
			} else if(Ptrs.Count() >= 2) { // [0] - Full Path, [1] - First File Name, [2] - Second File Name, ...
				for(i = 1; i < Ptrs.Count(); i++) {
					S = cStr(Ptrs[0]);
					std::wstring ws(Ptrs[i]);
					cStr s = ws;
					S.AppendPath(s);
					MultiFilePn->Add(cIO::EnsureAbsolutePath(S));
				}
				Success = true;
			}
		}
		if(Success && LoadMode) {
			// Check for file existance
			if(SingleFilePn != nullptr) {
				if(!FileExist(SingleFilePn->ToCharPtr())) {
					Success = false;
				}
			}
			if(MultiFilePn != nullptr) {
				for(i = 0; i < MultiFilePn->Count();) {
					if(!FileExist(MultiFilePn->GetAt(i).ToCharPtr())) {
						MultiFilePn->RemoveAt(i);
						continue;
					}
					i++;
				}
				if(MultiFilePn->IsEmpty()) {
					Success = false;
				}
			}
		}
	}
	SetCurrentDirectory(CurDir); // Restoring "CurDir"
	if(Success) {
		if(SingleFilePn != nullptr) {
			FixExtension(SingleFilePn);
		}
		if(MultiFilePn != nullptr) {
			for(i = 0; i < MultiFilePn->Count(); i++) {
				FixExtension(&MultiFilePn->GetAt(i));
			}
		}
	}
} // cWinMain_FileDialog::Run

// cWinMain_FileDialog::FixExtension
void cWinMain_FileDialog::FixExtension(cStr *FilePn) {
	cStr Ex = FilePn->GetFileExtension();
	bool GoodEx = false;
	int i;
	for(i = 0; i < Extensions.Count(); i++) {
		if ((Extensions[i].Contains(Ex, true) && !Ex.IsEmpty()) || (Extensions[i].IsEmpty()/*&& Ex.IsEmpty()*/)) {
			GoodEx = true;
		}
	}
	if(!GoodEx) {
		if(LoadMode) {
			Success = false;
		} else {
			if(m_ofn.nFilterIndex >= 1 && (int)m_ofn.nFilterIndex <= Extensions.Count()) {
				FilePn->Append("." + Extensions[m_ofn.nFilterIndex - 1]);
			} else {
				if(!DefaultExtension.IsEmpty()) {
					FilePn->Append("." + DefaultExtension);
				} else {
					FilePn->Append("." + Extensions[0]);
				}
			}
		}
	}
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// cWinMain_LoadFileDialog
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool cWinMain_LoadFileDialog(const char *Title, const cList<cStr> &Extensions, cStr *SingleFilePn, cList<cStr> *MultiFilePn, const char *PrefKey, const char *InitialFileName) {
	cPause::SetSystemPause(true);
	
	cWinMain_FileDialog *S = new cWinMain_FileDialog;
	S->LoadMode = true;
	S->Title = Title;
	S->Extensions = Extensions;
	S->InitialFileName = InitialFileName;
	S->InitialFolder = cIO::GetFileDialogInitialFolder(PrefKey);
	cAssert((SingleFilePn != nullptr && nullptr == MultiFilePn) || (nullptr == SingleFilePn && MultiFilePn != nullptr));
	S->SingleFilePn = SingleFilePn;
	S->MultiFilePn = MultiFilePn;
	
	S->Init(cWinMain_hWnd);
	S->Run();
	bool r = S->Success;
	if(r) {
		if(SingleFilePn != nullptr) {
			cIO::SetFileDialogInitialFolder(PrefKey, SingleFilePn->GetFilePath());
		}
		if(MultiFilePn != nullptr) {
			cIO::SetFileDialogInitialFolder(PrefKey, MultiFilePn->GetAt(0).GetFilePath());
		}
	}
	delete S;
	S = nullptr;
	cInput::FreeEvents();
	return r;
} // cWinMain_LoadFileDialog

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// cWinMain_SaveFileDialog
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
bool cWinMain_SaveFileDialog(const char *Title, const comms::cList<comms::cStr> &Extensions, comms::cStr *FilePn, const char *PrefKey, const char *DefaultExtension, const char *InitialFileBase) {
	comms::cPause::SetSystemPause(true);
	
	cWinMain_FileDialog *S = new cWinMain_FileDialog;
	S->LoadMode = false;
	S->Title = Title;
	S->Extensions = Extensions;
	S->DefaultExtension = DefaultExtension;
	S->InitialFileBase = InitialFileBase;
	S->InitialFolder = comms::cIO::GetFileDialogInitialFolder(PrefKey);
	S->SingleFilePn = FilePn;
	
	S->Init(cWinMain_hWnd);
	S->Run();
	bool r = S->Success;
	if(r) {
		comms::cIO::SetFileDialogInitialFolder(PrefKey, FilePn->GetFilePath());
	}
	delete S;
	S = nullptr;
	cInput::FreeEvents();
	return r;
} // cWinMain_SaveFileDialog

// cWinMain_SelectFolderDialog
static cStr SelectFolderDialog_InitialFolder;
static INT CALLBACK SelectFolderDialog_CallbackProc(HWND hDlg, UINT Msg, LPARAM lParam, LPARAM pData) {
	if(BFFM_INITIALIZED == Msg) {
		SendMessage(hDlg, BFFM_SETSELECTION, TRUE, (LPARAM)SelectFolderDialog_InitialFolder.ToCharPtr());
	}
	return 0;
}

// Function to show a folder picker dialog that allows typing paths
bool ShowFolderPickerDialog(HWND hwndOwner, std::wstring& folderPath)
{
	bool success = false;
	IFileDialog* pfd = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&pfd));
	if (SUCCEEDED(hr))
	{
		DWORD dwOptions;
		hr = pfd->GetOptions(&dwOptions);
		if (SUCCEEDED(hr))
		{
			hr = pfd->SetOptions(dwOptions | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
			if (SUCCEEDED(hr))
			{
				hr = pfd->Show(hwndOwner);
				if (SUCCEEDED(hr))
				{
					IShellItem* psiResult;
					hr = pfd->GetResult(&psiResult);
					if (SUCCEEDED(hr))
					{
						PWSTR pszFilePath;
						hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
						if (SUCCEEDED(hr))
						{
							folderPath = pszFilePath; // Direct assignment to std::wstring
							CoTaskMemFree(pszFilePath);
							success = true;
						}
						psiResult->Release();
					}
				}
			}
		}
		pfd->Release();
	}
	return success;
}


bool cWinMain_SelectFolderDialog(const char *Title, comms::cStr *SelectedFolder, const char *InitialFolder) {
	cPause::SetSystemPause(true);
	bool r = false;
	SelectFolderDialog_InitialFolder = cIO::EnsureAbsolutePath(InitialFolder);
	std::wstring w;
	SelectedFolder->toWstring(w);
	if(ShowFolderPickerDialog(cWinMain_hWnd, w)) {
		SelectedFolder->Copy(w);
		return true;
	}
	return false;
}

} // comms

// cWinMain_IDrop
class cWinMain_IDrop : public IDropTarget {
public:
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void __RPC_FAR *__RPC_FAR *ppvObject) {
		*ppvObject = this;
		return 0;
	}
	ULONG STDMETHODCALLTYPE AddRef() {
		return 0;
	}
	ULONG STDMETHODCALLTYPE Release() {
		return 0;
	}
	HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
		return 0;
	}
	HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
		return 0;
	}
	HRESULT STDMETHODCALLTYPE DragLeave() {
		return 0;
	}
	HRESULT STDMETHODCALLTYPE Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
		FORMATETC F;
		STGMEDIUM M;
		memset(&F, 0, sizeof(F));
		F.cfFormat = CF_HDROP;
		F.tymed = TYMED_HGLOBAL;
		F.dwAspect = DVASPECT_CONTENT;
		HRESULT h = pDataObj->GetData(&F, &M);
		HDROP Drop;
		comms::cStr S, T;
		comms::cFile File;
		HINTERNET hOpen, hConnect;
		CHAR Head[] = "Accept: */*\r\n\r\n";
		static comms::cList<wchar_t> Buffer(1024);
		DWORD Size;
		int i;
		// Free
		comms::cMain_OnDropImage.Free();
		// Update mouse pos with event
		comms::cInput::OldEvent *E = new comms::cInput::OldEvent;
		E->Type = comms::cInput::OldEvent::TYPE_MOUSEMOVE;
		POINT p;
		GetCursorPos(&p);
		cWinMain_GlobalToLocal(p.x, p.y, &E->MousePos);
		E->MouseDelta.SetZero();
		comms::cInput::AddEvent(E);
		E = nullptr;
		while(comms::cInput::GetEvent()) {
		}
		if(SUCCEEDED(h)) { // Local pathname
			Drop = (HDROP)M.hGlobal;
			wchar_t temp[MAX_PATH];
			memset(temp, 0, sizeof(temp));
			DragQueryFileW(Drop, 0, temp, MAX_PATH);
			std::wstring ws = temp;
			S = ws;
			ReleaseStgMedium(&M);
			// Callback
			comms::cMain_OnDrop(S);
			SetForegroundWindow(cWinMain_hWnd);
		} else { // Web image
			F.cfFormat = CF_TEXT;
			h = pDataObj->GetData(&F, &M);
			if(SUCCEEDED(h)) {
				// Copy text URL
				S.Copy((const char *)GlobalLock(M.hGlobal));
				GlobalUnlock(M.hGlobal);
			}
			if(S.IsEmpty()) {
				static const UINT CF_URL = RegisterClipboardFormat(CFSTR_SHELLURL);
				memset(&F, 0, sizeof(F));
				memset(&M, 0, sizeof(M));
				F.dwAspect = DVASPECT_CONTENT;
				F.lindex = -1;
				F.tymed = TYMED_HGLOBAL;
				F.cfFormat = CF_URL;
				if(pDataObj->GetData(&F, &M) == S_OK) { // Data object contains shell URL
					S.Copy((const char *)GlobalLock(M.hGlobal));
					GlobalUnlock(M.hGlobal);
				}
			}
			if(!S.IsEmpty()) {
				// Cutoff '?' suffix
				i = S.IndexOf('?');
				if(i != -1) {
					S.Remove(i);
				}
				// File extension
				T = S.GetFileExtension();
				if(!T.IsEmpty()) {
					// Search codec
					const comms::cList<comms::cImageCodecInfo> &Codecs = comms::cIO::GetImageCodecs();
					comms::cImageCodec *C = nullptr;
					for(i = 0; i < Codecs.Count(); i++) {
						if(comms::cStr::EqualsNoCase(Codecs[i].FileExtension, T)) {
							C = Codecs[i].Codec;
							break;
						}
					}
					if(C != nullptr) {
						// Download file from Internet
						hOpen = InternetOpen("comms", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
						if(hOpen != nullptr) {
							hConnect = InternetOpenUrl(hOpen, S.ToCharPtr(), Head, lstrlen(Head), INTERNET_FLAG_DONT_CACHE, 0);
							if(hConnect != nullptr) {
								do {
									if(!InternetReadFile(hConnect, Buffer.ToPtr(), Buffer.Count(),  &Size)) {
										break;
									}
									if(0 == Size) {
										break; // EOF
									}
									File.WriteBytes(Buffer.ToPtr(), Size);
									if(!comms::cMain_OnDownloadProgress((int)File.Size())) {
										File.Clear();
										break;
									}
								} while(true);
								InternetCloseHandle(hConnect);
							}
							InternetCloseHandle(hOpen);
						}
						if(File.Size() > 0) {
							// Decode image
							File.SetPos(0);
							if(C->Decode(File, &comms::cMain_OnDropImage)) {
								// Callback
								comms::cMain_OnDrop(nullptr);
								SetForegroundWindow(cWinMain_hWnd);
							}
						}
					} else {
						comms::cMain_OnDropURL(S.ToCharPtr());
					}
				} else {
					comms::cMain_OnDropURL(S.ToCharPtr());
				}
			}
		}
		return 0;
	}
};
cWinMain_IDrop cWinMain_Drop;

// cWinMain_MinimizeIfFullScreenIsInactive
static void cWinMain_MinimizeIfFullScreenIsInactive() {
	if(!cWinMain_FullScreen) {
		return;
	}
	HWND F = GetForegroundWindow();
	if(F == cWinMain_hWnd) {
		return;
	}
	HMONITOR M0 = MonitorFromWindow(cWinMain_hWnd, MONITOR_DEFAULTTONEAREST);
	HMONITOR M1 = MonitorFromWindow(F, MONITOR_DEFAULTTONEAREST);
	if(M0 != M1) {
		return;
	}
	ShowWindow(cWinMain_hWnd, SW_MINIMIZE);
}

// cWinMain_ResizeIfFullScreenMonitorHasChanged
static void cWinMain_ResizeIfFullScreenMonitorHasChanged() {
	if(!cWinMain_FullScreen) {
		return;
	}
	HMONITOR M = MonitorFromWindow(cWinMain_hWnd, MONITOR_DEFAULTTONEAREST);
	if(M == cWinMain_MonitorForFullScreenMode) {
		return;
	}
	cWinMain_MonitorForFullScreenMode = M;
	MONITORINFO I;
	memset(&I, 0, sizeof(I));
	I.cbSize = sizeof(I);
	if(!GetMonitorInfo(cWinMain_MonitorForFullScreenMode, &I)) {
		return;
	}
	RECT rc = I.rcMonitor;
	SetWindowPos(cWinMain_hWnd, HWND_TOPMOST, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_SHOWWINDOW);
}

// cWinMain_OneIterationOfMessageLoop
static void cWinMain_OneIterationOfMessageLoop(MSG *Msg, const bool UsePeekMessage) {
	//__elog("cWinMain_OneIterationOfMessageLoop");
	//char mss[32];
	cWinMain_ResizeIfFullScreenMonitorHasChanged();
	if(comms::cPause::GetSystemPause() && ScriptDepth==0) {
		cWinMain_MinimizeIfFullScreenIsInactive();
		BOOL r = (UsePeekMessage ? PeekMessage(Msg, nullptr, 0, 0, PM_REMOVE) : GetMessage(Msg, nullptr, 0, 0));
		if(r) {
			//sprintf(mss,"msg: %p",Msg);
			//__elog(mss);
			TranslateMessage(Msg);
			DispatchMessage(Msg);
			//__elog("endmsg");
			if(cWinMain_EraseBkgnd) {

				//__elog("render1");
				cWinMain_OnRender();
				cWinMain_EraseBkgnd = false;
				//__elog("render2");
			}
		}
	} else {
		if(PeekMessage(Msg, nullptr, 0, 0, PM_REMOVE)) {
			//sprintf(mss,"msg2: %p",Msg);
			//__elog(mss);
			TranslateMessage(Msg);
			DispatchMessage(Msg);
			//__elog("endmsg2");
		} else {
			//__elog("render3");
			cWinMain_OnRender();
			//__elog("render4");
		}
	}
	//__elog("endof : cWinMain_OneIterationOfMessageLoop");
}

// cWinMain_CreateTempFile
static void cWinMain_CreateTempFile(const char *SrcFilePn, comms::cStr *TempFilePn) {
	comms::cFile F;
	comms::cStr T(MAX_PATH), N(MAX_PATH), S;
	if(GetTempPath(T.Length(), T.ToNonConstCharPtr())) {
		T.CalcLength();
		if(GetTempFileName(T.ToCharPtr(), nullptr, 0, N.ToNonConstCharPtr())) {
			N.CalcLength();
			if(comms::cIO::LoadFile(SrcFilePn, &F, false)) {
				if(comms::cIO::SaveFile(N.ToCharPtr(), F, false, false)) {
					*TempFilePn = N;
				}
			}
		}
	}
}

// cWinMain_LoadCursors
static void cWinMain_LoadCursors() {
	cWinMain_hCursors[0] = LoadCursor(nullptr, IDC_ARROW);
	cWinMain_hCursors[1] = LoadCursor(nullptr, IDC_UPARROW);
	cWinMain_hCursors[2] = LoadCursor(nullptr, IDC_SIZEWE);
	cWinMain_hCursors[3] = LoadCursor(nullptr, IDC_SIZENS);
	cWinMain_hCursors[4] = LoadCursor(nullptr, IDC_SIZENESW);
	cWinMain_hCursors[5] = LoadCursor(nullptr, IDC_SIZENWSE);
	cWinMain_hCursors[6] = LoadCursor(nullptr, IDC_SIZEALL);
	cWinMain_hCursors[7] = LoadCursor(nullptr, IDC_IBEAM);
	cWinMain_hCursors[8] = LoadCursor(nullptr, IDC_CROSS);
	cWinMain_hCursors[9] = LoadCursor(nullptr, IDC_WAIT);
	cWinMain_hCursors[10] = LoadCursor(nullptr, IDC_NO);
	
	comms::cStr N;
	cWinMain_CreateTempFile("Cursors/Arrow.ani", &N);
	HCURSOR h = nullptr;
	if(!N.IsEmpty()) {
		h = LoadCursorFromFile(N.ToCharPtr());
		if(h != nullptr) {
			cWinMain_hCursors[0] = h;
		}
	}
}

// cWinMain_LoadIcon
static HICON cWinMain_LoadIcon() {
	comms::cStr N;
	cWinMain_CreateTempFile("Icons/Icon.ico", &N);
	if(N.IsEmpty()) {
		return nullptr;
	}
	return (HICON)LoadImage(nullptr, N.ToCharPtr(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
}

static void cWinMain_EnableHighDPI() {
	if (g_SetProcessDpiAwarenessContext != nullptr) {
		g_SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
	}
}

struct no_grouping : std::numpunct<char> {
protected:
	virtual string_type do_grouping() const {
		return "\000";
	}
};

//-----------------------------------------------------------------------------
// WinMain
// ----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR Args, int) {
	//comms::cMessageBox::Ok("Debugging", "Started: 1");
	// This call prevents the executable from starting under Windows 7
	// when Project Properties > C/C++ > Code Generation > Multi-threaded DLL (/MD)
	// std::locale::global(std::locale(std::locale("en_US.UTF-8"), new no_grouping()));
	// 
	InitGesturesAndDpi();
	cWinMain_EnableHighDPI();
	
	comms::cMutex::GetInstance();
	cWinMain_CmdLineArgs = Args;
	OleInitialize(nullptr);
#ifdef COMMS_3DCOAT
#ifdef COMMS_64
	FEX_BEGIN64();
#else
	FEX_BEGIN();
#endif
#endif // COMMS_3DCOAT
	
	SetThreadAffinityMask(GetCurrentThread(), 1);

	cWinMain_Codes.Add(VK_ESCAPE);	// Esc
	cWinMain_Codes.Add(VK_F1);		// F1
	cWinMain_Codes.Add(VK_F2);		// F2
	cWinMain_Codes.Add(VK_F3);		// F3
	cWinMain_Codes.Add(VK_F4);		// F4
	cWinMain_Codes.Add(VK_F5);		// F5
	cWinMain_Codes.Add(VK_F6);		// F6
	cWinMain_Codes.Add(VK_F7);		// F7
	cWinMain_Codes.Add(VK_F8);		// F8
	cWinMain_Codes.Add(VK_F9);		// F9
	cWinMain_Codes.Add(VK_F10);		// F10
	cWinMain_Codes.Add(VK_F11);		// F11
	cWinMain_Codes.Add(VK_F12);		// F12
	cWinMain_Codes.Add('0');		// Zero, "0"
	cWinMain_Codes.Add('1');		// One, "1"
	cWinMain_Codes.Add('2');		// Two, "2"
	cWinMain_Codes.Add('3');		// Three, "3"
	cWinMain_Codes.Add('4');		// Four, "4"
	cWinMain_Codes.Add('5');		// Five, "5"
	cWinMain_Codes.Add('6');		// Six, "6"
	cWinMain_Codes.Add('7');		// Seven, "7"
	cWinMain_Codes.Add('8');		// Eight, "8"
	cWinMain_Codes.Add('9');		// Nine, "9"
	cWinMain_Codes.Add(0xbd);		// Minus, "-" (VK_OEM_MINUS)
	cWinMain_Codes.Add(0xbb);		// Equals, "=" (VK_OEM_PLUS)
	cWinMain_Codes.Add(VK_BACK);	// BackSpace
	cWinMain_Codes.Add(VK_TAB);		// Tab
	cWinMain_Codes.Add(VK_CAPITAL);	// CapsLock
	cWinMain_Codes.Add(VK_INSERT);	// Insert
	cWinMain_Codes.Add(VK_DELETE);	// Delete
	cWinMain_Codes.Add(VK_HOME);	// Home
	cWinMain_Codes.Add(VK_END);		// End
	cWinMain_Codes.Add(VK_PRIOR);	// PageUp
	cWinMain_Codes.Add(VK_NEXT);	// PageDown
	cWinMain_Codes.Add(VK_UP);		// Up
	cWinMain_Codes.Add(VK_DOWN);	// Down
	cWinMain_Codes.Add(VK_LEFT);	// Left
	cWinMain_Codes.Add(VK_RIGHT);	// Right
	cWinMain_Codes.Add(0xdc);		// BackSlash, "\\" (VK_OEM_5)
	cWinMain_Codes.Add(VK_RETURN);	// Enter
	cWinMain_Codes.Add(0xdb);		// LeftBracket, "[" (VK_OEM_4)
	cWinMain_Codes.Add(0xdd);		// RightBracket, "]" (VK_OEM_6)
	cWinMain_Codes.Add(0xba);		// SemiColon, ";" (VK_OEM_1)
	cWinMain_Codes.Add(0xde);		// SingleQuote, "\'" (VK_OEM_7)
	cWinMain_Codes.Add(0xbc);		// Comma, "," (VK_OEM_COMMA)
	cWinMain_Codes.Add(0xbe);		// Period, "." (VK_OEM_PERIOD)
	cWinMain_Codes.Add(0xbf);		// Slash, "/" (VK_OEM_2)
	cWinMain_Codes.Add(VK_SHIFT);	// Shift
	cWinMain_Codes.Add(VK_CONTROL);	// Control
	cWinMain_Codes.Add(VK_MENU);	// Alt
	cWinMain_Codes.Add(VK_SPACE);	// Space
	cWinMain_Codes.Add(0xc0);		// Tilda, "~" (VK_OEM_3)
	cWinMain_Codes.Add('A');		// A
	cWinMain_Codes.Add('B');		// B
	cWinMain_Codes.Add('C');		// C
	cWinMain_Codes.Add('D');		// D
	cWinMain_Codes.Add('E');		// E
	cWinMain_Codes.Add('F');		// F
	cWinMain_Codes.Add('G');		// G
	cWinMain_Codes.Add('H');		// H
	cWinMain_Codes.Add('I');		// I
	cWinMain_Codes.Add('J');		// J
	cWinMain_Codes.Add('K');		// K
	cWinMain_Codes.Add('L');		// L
	cWinMain_Codes.Add('M');		// M
	cWinMain_Codes.Add('N');		// N
	cWinMain_Codes.Add('O');		// O
	cWinMain_Codes.Add('P');		// P
	cWinMain_Codes.Add('Q');		// Q
	cWinMain_Codes.Add('R');		// R
	cWinMain_Codes.Add('S');		// S
	cWinMain_Codes.Add('T');		// T
	cWinMain_Codes.Add('U');		// U
	cWinMain_Codes.Add('V');		// V
	cWinMain_Codes.Add('W');		// W
	cWinMain_Codes.Add('X');		// X
	cWinMain_Codes.Add('Y');		// Y
	cWinMain_Codes.Add('Z');		// Z
	cWinMain_Codes.Add(VK_NUMPAD0);	// NumPad0
	cWinMain_Codes.Add(VK_NUMPAD1);	// NumPad1
	cWinMain_Codes.Add(VK_NUMPAD2);	// NumPad2
	cWinMain_Codes.Add(VK_NUMPAD3);	// NumPad3
	cWinMain_Codes.Add(VK_NUMPAD4);	// NumPad4
	cWinMain_Codes.Add(VK_NUMPAD5);	// NumPad5
	cWinMain_Codes.Add(VK_NUMPAD6);	// NumPad6
	cWinMain_Codes.Add(VK_NUMPAD7);	// NumPad7
	cWinMain_Codes.Add(VK_NUMPAD8);	// NumPad8
	cWinMain_Codes.Add(VK_NUMPAD9);	// NumPad9
	cWinMain_Codes.Add(VK_ADD);		// Add
	cWinMain_Codes.Add(VK_SUBTRACT);// Subtract
	cWinMain_Codes.Add(VK_MULTIPLY);// Multiply
	cWinMain_Codes.Add(VK_DIVIDE);	// Divide
	cWinMain_Codes.Add(VK_DECIMAL);	// Decimal
	cWinMain_CodesRemap.SetCount(256, -1);
	int i;
	for(i = 0; i < cWinMain_Codes.Count(); i++) {
		cWinMain_CodesRemap[cWinMain_Codes[i]] = i;
	}
	comms::cMain_OnPreInit();
	cWinMain_LoadCursors();

#ifdef COMMS_XINPUT
	//*************************************************************************
	// XInput
	//*************************************************************************
	comms::cList<comms::cStr> XInputLibs;
	XInputLibs.Add("xinput1_1.dll");
	XInputLibs.Add("xinput1_2.dll");
	XInputLibs.Add("xinput1_3.dll");
	XInputLibs.Add("xinput1_4.dll");
	XInputLibs.Add("xinput9_1_0.dll");

	g_XInputDll = nullptr;
	g_XInputGetState = nullptr;
	g_XInputSetState = nullptr;
	
	for(i = 0; i < XInputLibs.Count(); i++) {
		const comms::cStr &r = XInputLibs[i];
		g_XInputDll = LoadLibrary(r.ToCharPtr());
		if(g_XInputDll != nullptr) {
			g_XInputGetState = (pFnXInputGetState)GetProcAddress(g_XInputDll, "XInputGetState");
			g_XInputSetState = (pFnXInputSetState)GetProcAddress(g_XInputDll, "XInputSetState");
			break;
		}
	} // XInput
#endif // COMMS_XINPUT

	const char* ClassName = "comms::cWinMain";
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC | CS_DBLCLKS, WndProc, 0, 0,
		GetModuleHandle(nullptr), nullptr, LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr,
		ClassName, nullptr };
	wc.hIcon = wc.hIconSm = cWinMain_LoadIcon();
#ifdef COMMS_3DCOAT
	bool CheckIfMedical();
	if(CheckIfMedical()){
		wc.hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_ICON1));
		wc.hIconSm = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_ICON1));
	}else{
		wc.hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_MAINFRAME));
		wc.hIconSm = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_MAINFRAME));
	}
#else // StrokeTest, FServer
	wc.hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_APPICON));
	wc.hIconSm = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_APPICON));
#endif // COMMS_3DCOAT
	RegisterClassEx(&wc);

	const DWORD Style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	RECT rc;
	SetRect(&rc, 0, 0, 800, 600);
	AdjustWindowRect(&rc, Style, FALSE);
	const int Width = rc.right - rc.left;
	const int Height = rc.bottom - rc.top;
	cWinMain_hWnd = CreateWindowEx(0, ClassName, comms::cMain_Title.ToCharPtr(), Style, CW_USEDEFAULT, CW_USEDEFAULT, Width, Height, nullptr, nullptr, wc.hInstance, nullptr);
	#ifdef COMMS_TABLET
	cWinMain_FillWindowsInkDPI();
	#endif // COMMS_TABLET
	cWinMain_RestoreWindowState();
	cWinMain_FillWindowDPI();

	// Save the current sticky / toggle / filter key settings so they can be restored them later
	SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &cWinMain_StartupStickyKeys, 0);
	SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &cWinMain_StartupToggleKeys, 0);
	SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &cWinMain_StartupFilterKeys, 0);
	
	MSG Msg;
	memset(&Msg, 0, sizeof(Msg));

	//comms::cMessageBox::Ok("Debugging", "Started: 2");

	if(comms::cRender::Init()) {
		comms::cMain_OnInit();		

		//comms::cMessageBox::Ok("Debugging", "Started: 3");

#if defined(COMMS_3DCONNEXION)

		if(UseOld3DConnexionAPI())comms::g_3DconnexionOld.Init();
#ifdef COMMS_3DCONNEXION_NEW
		else InitNavlib();
#endif
#endif // COMMS_3DCONNEXION
		
		ConfigureGestures();
	
		RegisterDragDrop(cWinMain_hWnd, &cWinMain_Drop);

		ShowWindow(cWinMain_hWnd, cWinMain_WindowPlacement.showCmd);
		cWinMain_AllowOnRender = true;
		cWinMain_OnRender();
#ifdef COMMS_3DCOAT
		ParseArgs(Args);
		InitOptions();
#endif // COMMS_3DCOAT

		//comms::cMessageBox::Ok("Debugging", "Started: 4");

		while(Msg.message != WM_QUIT && !ExitState) {
#ifdef COMMS_3DCOAT
			if(OpenAutomateEnabled){
				if(BenchmarkState::BenchmarkIndex==0){
					if(!ProcessOpenAutomateCommands())break;
				}				
			}
#endif // COMMS_3DCOAT
			cWinMain_OneIterationOfMessageLoop(&Msg, false);
		}

#if defined(COMMS_3DCONNEXION)
#ifdef COMMS_3DCONNEXION_NEW
		FreeNavlib();
#endif 
		comms::g_3DconnexionOld.Free();
#endif // COMMS_3DCONNEXION
#ifdef COMMS_TABLET
		comms::cWinMain_TabletSelector::Free();
#endif // COMMS_TABLET
		comms::cMain_OnFree();
		comms::cRender::Free();
	}

	comms::cMain_OnPostFree();

	comms::cInput::FreeEvents();

	cWinMain_AllowAccessibilityShortcutKeys(true);

#ifdef COMMS_XINPUT
	// XInput
	if(g_XInputDll != nullptr) {
		FreeLibrary(g_XInputDll);
		g_XInputDll = nullptr;
	}
#endif // COMMS_XINPUT
	
	UnregisterClass(ClassName, wc.hInstance);
	
	return 0;
} // WinMain

#ifdef COMMS_3DCOAT
extern bool IsInExitState;
extern bool IgnoreSystemPause;
#else // COMMS_3DCOAT
bool IsInExitState = false;
bool IgnoreSystemPause = false;
#endif // COMMS_3DCOAT
class BaseWidget;
typedef bool fnCycleEnd(BaseWidget*);
void ProcessRenderCycle(fnCycleEnd* WhenEnd,BaseWidget* W){	
	MSG Msg;
	memset(&Msg, 0, sizeof(Msg));
	while(Msg.message != WM_QUIT && (WhenEnd==0 || WhenEnd(W)==false) && IsInExitState==false) {
		cWinMain_OneIterationOfMessageLoop(&Msg, IgnoreSystemPause);
	}	
	if(IsInExitState) {
		exit(0);
	}
}
#endif // COMMS_WINDOWS
