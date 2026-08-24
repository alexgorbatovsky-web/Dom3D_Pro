#include "comms.h"
#ifdef COMMS_WINDOWS
HWND cWinMain_GetParent();
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
int cMacMain_ShowLog(const comms::cLog::Mode::Enum Mode, const comms::cStr *Text);
#endif // COMMS_MACOS

//#define IMMEDIATE_LOGGING

#ifdef COMMS_3DCOAT
extern bool ExitState;
#else // FRAME
bool ExitState = false;
#endif // COMMS_3DCOAT

namespace comms {

static cStr s_LogForDialog;

cStr s_AutoSaveFilePn;
static bool s_Visible = false;
bool cLog::SkipLogging = false;

// cLog::IsVisible
bool cLog::IsVisible() {
	return s_Visible;
}

// cLog::EnableAutoSaveToFile
void cLog::EnableAutoSaveToFile(const char *FilePn) {
	if (s_AutoSaveFilePn.IsEmpty() && (FilePn != nullptr)) {
		cIO::RemoveFile(FilePn); // Remove the old log file
	}
	s_AutoSaveFilePn = FilePn;
}

#ifdef COMMS_WINDOWS

#pragma comment (lib, "comctl32.lib")

static void *s_hIcon = nullptr; // EnableControls
static cLog::Mode::Enum s_Mode = cLog::Mode::Message; // EnableControls

// cLog::SetIcon
void cLog::SetIcon(void *hIcon) {
	s_hIcon = hIcon;
}

//-----------------------------------------------------------------------------
// GenDlgTemplate
//-----------------------------------------------------------------------------
static void GenDlgTemplate(cWin32::DlgTemplate &Dlg) {
	Dlg.Free();
	Dlg.Create(cMain_Title.ToCharPtr(), DS_CENTER | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_POPUP | WS_CAPTION | WS_SYSMENU | //DS_SYSMODAL
		WS_THICKFRAME | DS_FIXEDSYS | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_CONTROLPARENT | WS_EX_APPWINDOW, 0, 0, 380, 260, "Tahoma", 8);
	Dlg.AddEditBox("", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | WS_HSCROLL | WS_VSCROLL | ES_READONLY | WS_TABSTOP, 0, 0, 0, 10, 10, cLog::ID::Log);
	Dlg.AddControl("msctls_statusbar32", "", WS_VISIBLE, 0, 0, 0, 0, 0, cLog::ID::StatusBar);
	Dlg.AddButton("&Debug", WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 10, 10, cLog::ID::Debug);
	Dlg.AddButton("&Ignore", WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 10, 10, cLog::ID::Ignore);
	Dlg.AddButton("Ignore &Always", WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 10, 10, cLog::ID::IgnoreAlways);
	Dlg.AddButton("&Copy", WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 10, 10, cLog::ID::Copy);
	Dlg.AddButton("E&xit", WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 10, 10, cLog::ID::Exit);
	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_BAR_CLASSES;
	InitCommonControlsEx(&icc);
} // GenDlgTemplate

//-----------------------------------------------------------------------------
// EnableControls
//-----------------------------------------------------------------------------
static void EnableControls(HWND hDlg) {
	// Change controls visibility according to the mode
	EnableWindow(GetDlgItem(hDlg, cLog::ID::Debug), cLog::Mode::Assert == s_Mode);
	EnableWindow(GetDlgItem(hDlg, cLog::ID::Ignore), cLog::Mode::Assert == s_Mode || cLog::Mode::Warning == s_Mode);
	EnableWindow(GetDlgItem(hDlg, cLog::ID::IgnoreAlways), cLog::Mode::Assert == s_Mode);
	EnableWindow(GetDlgItem(hDlg, cLog::ID::Copy), TRUE);
	EnableWindow(GetDlgItem(hDlg, cLog::ID::Exit), cLog::Mode::IsModal(s_Mode));
	
	// Set title according to required mode
	cStr Title(cMain_Title);
	if(cLog::Mode::Message == s_Mode) {
		Title << " Log";
	} else if(cLog::Mode::Warning == s_Mode) {
		Title << " Warning";
	} else if(cLog::Mode::Error == s_Mode) {
		Title << " Error";
	} else if(cLog::Mode::Assert == s_Mode) {
		Title << " Assert";
	}
	SetWindowText(hDlg, Title);
	
	// Disable close button if mode is modal, and enable otherwise
	EnableMenuItem(GetSystemMenu(hDlg, FALSE), SC_CLOSE, MF_BYCOMMAND | (cLog::Mode::IsModal(s_Mode) ? MF_GRAYED : MF_ENABLED));

	// Set icon if any
	if(s_hIcon != nullptr) {
		SendMessage(hDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)s_hIcon);
		SendMessage(hDlg, WM_SETICON, (WPARAM)ICON_SMALL, (LPARAM)s_hIcon);
	}
} // EnableControls

//-----------------------------------------------------------------------------
// AdjustLayout
//-----------------------------------------------------------------------------
static void AdjustLayout(HWND hDlg) {
	const int cxButton = 83;
	const int cyButton = 24;
	const int ButtonSpace = 4;
	const int DlgSpace = 4;
	const int StatusBarSpace = 2;
	// Dlg Size:
	RECT rc;
	GetClientRect(hDlg, &rc);
	const int cxDlg = rc.right - rc.left;
	const int cyDlg = rc.bottom - rc.top;
	// Status Bar Size:
	GetWindowRect(GetDlgItem(hDlg, cLog::ID::StatusBar), &rc);
	const int cyStatusBar = rc.bottom - rc.top;
	// Layout:
	MoveWindow(GetDlgItem(hDlg, cLog::ID::Log), DlgSpace, DlgSpace, cxDlg - 2 * DlgSpace,
		cyDlg - 2 * DlgSpace - cyStatusBar - cyButton - StatusBarSpace, TRUE);

	int yPos = cyDlg - cyStatusBar - cyButton - StatusBarSpace;
	int xPos = DlgSpace + 4 * cxButton + 4 * ButtonSpace;
	MoveWindow(GetDlgItem(hDlg, cLog::ID::Exit), xPos, yPos, cxButton, cyButton, TRUE);
	xPos -= ButtonSpace + cxButton;
	MoveWindow(GetDlgItem(hDlg, cLog::ID::Copy), xPos, yPos, cxButton, cyButton, TRUE);
	xPos -= ButtonSpace + cxButton;
	MoveWindow(GetDlgItem(hDlg, cLog::ID::IgnoreAlways), xPos, yPos, cxButton, cyButton, TRUE);
	xPos -= ButtonSpace + cxButton;
	MoveWindow(GetDlgItem(hDlg, cLog::ID::Ignore), xPos, yPos, cxButton, cyButton, TRUE);
	xPos -= ButtonSpace + cxButton;
	MoveWindow(GetDlgItem(hDlg, cLog::ID::Debug), xPos, yPos, cxButton, cyButton, TRUE);
	InvalidateRect(GetDlgItem(hDlg, cLog::ID::StatusBar), nullptr, TRUE);

	InvalidateRect(GetDlgItem(hDlg, cLog::ID::Debug), nullptr, TRUE);
	InvalidateRect(GetDlgItem(hDlg, cLog::ID::Ignore), nullptr, TRUE);
	InvalidateRect(GetDlgItem(hDlg, cLog::ID::IgnoreAlways), nullptr, TRUE);
	InvalidateRect(GetDlgItem(hDlg, cLog::ID::Copy), nullptr, TRUE);
	InvalidateRect(GetDlgItem(hDlg, cLog::ID::Exit), nullptr, TRUE);
} // AdjustLayout

//---------------------------------------------------------------------------------
// DlgProc
//---------------------------------------------------------------------------------
static LRESULT CALLBACK DlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam) {
	const int EditBoxMaxCapacity = 20000; // Maximum capacity of edit box is 32768 chars
	// But it seems like even less...
	cStr S;

	switch(Msg) {
		case WM_INITDIALOG:
			SendMessage(GetDlgItem(hDlg, cLog::ID::Log), WM_SETFONT, (WPARAM)GetStockObject(SYSTEM_FIXED_FONT), 0);
			EnableControls(hDlg);
			AdjustLayout(hDlg);
			HWND hEdit;
			hEdit = GetDlgItem(hDlg, cLog::ID::Log);
			if(hEdit != nullptr) {
				// Set text to edit box
				S = s_LogForDialog;
				if(S.Length() > EditBoxMaxCapacity) {
					S.Remove(0, S.Length() - EditBoxMaxCapacity);
				}
				SendMessage(hEdit, EM_SETSEL, (WPARAM)0, (LPARAM)0xffffff);
				SendMessage(hEdit, EM_REPLACESEL, 0, (LPARAM)S.ToCharPtr());
				SendMessage(hEdit, EM_SETSEL, (WPARAM)0xffffff, (LPARAM)0xffffff);
				
				// Scroll to bottom
				SendMessage(hEdit, EM_LINESCROLL, (WPARAM)0, (LPARAM)0xffffff);

				SetFocus(hEdit);
			}
			break;
		case WM_CTLCOLORSTATIC:
			SetBkColor((HDC)wParam, RGB(0, 0, 0));
			SetTextColor((HDC)wParam, RGB(255, 255, 255));
			return (LRESULT)GetStockObject(BLACK_BRUSH);
		case WM_SIZING:
			cWin32::OnSizing(hDlg, wParam, lParam, 478, 350);
			break;
		case WM_SIZE:
			AdjustLayout(hDlg);
			SendMessage(GetDlgItem(hDlg, cLog::ID::StatusBar), Msg, wParam, lParam);
			return TRUE;
		case WM_COMMAND:
			WORD id;
			id = LOWORD(wParam);
			if(cLog::ID::Copy == id) {
				cIO::CopyToClipboard(s_LogForDialog);
			} else if(cLog::ID::Debug == id || cLog::ID::Ignore == id || cLog::ID::IgnoreAlways == id || cLog::ID::Exit == id) {
				EndDialog(hDlg, id);
			}
			break;
		case WM_CLOSE:
			EndDialog(hDlg, 0);
			break;
	}
	return FALSE;
} // DlgProc

// ShowDlg
static int ShowDlg(const cLog::Mode::Enum Mode) {
	HWND hParent = cWinMain_GetParent();
	if(hParent != nullptr) { // If there is parent, we should activate it in back of log dlg before show log
		SetForegroundWindow(hParent);
	}
	cWin32::DlgTemplate Dlg;
	GenDlgTemplate(Dlg);
	s_Mode = Mode;
	s_Visible = true;
	int ID = (int)DialogBoxIndirect(GetModuleHandle(nullptr), Dlg.ToDlgTemplatePtr(), hParent, (DLGPROC)DlgProc);
	s_Visible = false;
	if(cLog::ID::Exit == ID && cLog::Mode::IsAutoExit(s_Mode)) {
		exit(0);
	}
	return ID;
}

#endif // COMMS_WINDOWS

//multithreaded saving
cStr s_LogForFile;
bool ThreadIsRun=false;
static void SavingThread(){
	do{
        if(!ExitState)cLog::Flush();
		SleepMs(500);
	}while(true);
}
// cLog::Flush
void cLog::Flush() {
	static cMutex* Mutex = cMutex::GetInstance();
	Mutex->logMutex.lock();
    if(s_LogForFile.Length()>0){
        cIO::SaveFile(s_AutoSaveFilePn, s_LogForFile.ToCharPtr(), s_LogForFile.Length(), true, false);
        s_LogForFile.Clear();
    }
	Mutex->logMutex.unlock();
}

//---------------------------------------------------------------------------------
// cLog::AddString
//---------------------------------------------------------------------------------
void cLog::AddString(const char *Format, va_list Args, const bool EchoToTerminal, const bool Time) {
	cStr S = cStr::Formatv(Format, Args);
	S.Replace("\r", "");
	S.Replace("\n", cStr::EndLn);
	if(!S.EndsWith(cStr::EndLn)) {
		S.Append(cStr::EndLn);
	}
    if(Time) {
        if(0 == Time0) {
            ResetTime();
        }
        const int T1 = (int)(cTimer::AcquireChronoMs() - Time0);
        int ms = T1 % 1000;
        int se = (T1 / 1000);
        int mi = se / 60;
        se %= 60;
        const cStr T = cStr::Format("%.02d:%.02d.%.03d ", mi, se, ms);
        S.Insert(0, T);
    }
	if (!s_AutoSaveFilePn.IsEmpty()) {
		if (!ThreadIsRun){
			ThreadIsRun = true;
			new std::thread(SavingThread);
		}
		static cMutex* Mutex = cMutex::GetInstance();
		Mutex->logMutex.lock();
		s_LogForFile.Append(S);
		Mutex->logMutex.unlock();
#ifdef COMMS_3DCOAT
		s_LogForDialog = S; // No mutex around because function "AddString" is always called within locked "logMutex2"
#else
		s_LogForDialog.Append(S);
#endif 
	}
	else { // When multithreaded saving is disabled keep full log
		s_LogForDialog.Append(S);
	}
	if (EchoToTerminal) {
		::std::cout << S.ToCharPtr();
	}
} // cLog::AddString

qword cLog::Time0 = 0;

// cLog::ResetTime
void cLog::ResetTime() {
    Time0 = cTimer::AcquireChronoMs();
}

// cLog::Message
void cLog::TimeMessage(const char *Format, ...) {
	static cMutex* Mutex = cMutex::GetInstance();
	Mutex->logMutex2.lock();
	va_list Args;
	va_start(Args, Format);
    AddString(Format, Args, false, true);
	va_end(Args);
	Mutex->logMutex2.unlock();
#ifdef IMMEDIATE_LOGGING
	Flush();
#endif
}
void cLog::Message(const char *Format, ...) {
	if (SkipLogging) return;
	static cMutex* Mutex = cMutex::GetInstance();
	Mutex->logMutex2.lock();
	va_list Args;
	va_start(Args, Format);
	AddString(Format, Args);
	va_end(Args);
	Mutex->logMutex2.unlock();
#ifdef IMMEDIATE_LOGGING
	Flush();
#endif
}

void cLog::TerminalMessage(const char* Format, ...) {
	static cMutex* Mutex = cMutex::GetInstance();
	Mutex->logMutex2.lock();
	va_list Args;
	va_start(Args, Format);
	AddString(Format, Args, true);
	va_end(Args);
	Mutex->logMutex2.unlock();
}

// cLog::Warning
void cLog::Warning(const char *Format, ...) {
	static cMutex* Mutex = cMutex::GetInstance();
	Mutex->logMutex2.lock();
	va_list Args;
	va_start(Args, Format);
	AddString(Format, Args);
	va_end(Args);
	Mutex->logMutex2.unlock();
#ifdef IMMEDIATE_LOGGING
	Flush();
#endif
#ifndef COMMS_3DCOAT
    // Calling "ShowDialog" after "logMutex2.unlock()" because "cMacMain_ShowLog(...)" calls
    // "SetSystemPause(true)" which calls "cLog::Message(...)" that locks already locked "logMutex2".
    ShowDialog(Mode::Warning);
#endif // !COMMS_3DCOAT
}

// cLog::Error
void cLog::Error(const char *Format, ...) {
	static cMutex* Mutex = cMutex::GetInstance();
	Mutex->logMutex2.lock();
	va_list Args;
	va_start(Args, Format);
	AddString(Format, Args);
	va_end(Args);
	Mutex->logMutex2.unlock();
#ifdef IMMEDIATE_LOGGING
	Flush();
#endif
    // Calling "ShowDialog" after "logMutex2.unlock()" because "cMacMain_ShowLog(...)" calls
    // "SetSystemPause(true)" which calls "cLog::Message(...)" that locks already locked "logMutex2".
    ShowDialog(Mode::Error);
}

// cLog::Assert
int cLog::Assert(const char *Format, ...) {
	static cMutex* Mutex = cMutex::GetInstance();
	Mutex->logMutex2.lock();
	va_list Args;
	va_start(Args, Format);
	AddString(Format, Args);
	va_end(Args);
	Mutex->logMutex2.unlock();
    // Calling "ShowDialog" after "logMutex2.unlock()" because "cMacMain_ShowLog(...)" calls
    // "SetSystemPause(true)" which calls "cLog::Message(...)" that locks already locked "logMutex2".
    int r = ShowDialog(Mode::Assert);
	return r;
}

// cLog::ShowDialog
int cLog::ShowDialog(const cLog::Mode::Enum Mode) {
	cSplash::DestroyNow();

#ifdef COMMS_MACOS
	return cMacMain_ShowLog(Mode, &s_LogForDialog);
#endif // COMMS_MACOS
	
#ifdef COMMS_WINDOWS
	return ShowDlg(Mode);
#endif // COMMS_WINDOWS

	return 0;
}

} // comms
