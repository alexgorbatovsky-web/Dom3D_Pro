#pragma once


void report(const char* f, int l, const char* tid);
#define __REPORT(tid) report(__FILE__, __LINE__, tid)

// Powered by "comms" cross-platform engine (c) 2005 - 2015 Sergii Kryzhanovskyi
#ifdef _WINDOWS
#define APICALL __declspec(dllexport)
#else
#define APICALL
#endif

#ifndef DOM3D_COMMS_STANDALONE
#define COMMS_3DCOAT
#endif
#include "../comms-Math/comms-Math.h"

//#define COMMS_WINDOWS
//#define COMMS_MACOS
//#define COMMS_LINUX
//#define COMMS_IOS
//#define COMMS_TIZEN

//#define COMMS_DIRECTX
//#define COMMS_OPENGL

//#define COMMS_OPENAL
//#define COMMS_XAUDIO2

//#define COMMS_ZIP
//#define COMMS_JPEG
//#define COMMS_PNG
//#define COMMS_TIFF
//#define COMMS_FBX
//#define COMMS_XINPUT // Only with COMMS_WINDOWS
//#define COMMS_TABLET // Only with COMMS_WINDOWS
//#define COMMS_3DCONNEXION // macOS, Windows, Linux
//#define COMMS_CURL // Only with COMMS_LINUX
//#define COMMS_PVRTC // Only with COMMS_MACOS
//#define COMMS_BIG_ENDIAN

#ifndef _DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif // NDEBUG
#endif // _DEBUG

#ifdef COMMS_WINDOWS
#define COMMS_EXPORT __declspec(dllexport)
#else // Linux, macOS
#define COMMS_EXPORT
#endif // COMMS_WINDOWS

#ifdef COMMS_WINDOWS
#ifndef WINVER
#define WINVER 0x0601 // Windows Vista and above
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Windows Vista and above
#endif
#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN7
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <Wspiapi.h>
#include <windows.h>
#include <CommDlg.h>
#include <stdio.h>
#include <stdexcept>
#include <math.h>
#include <typeinfo>
#include <Share.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <process.h>
#include <Shlobj.h>
#include <zmouse.h>
#include <time.h>
#include <chrono>
typedef int socklen_t;
#ifdef _WIN64
#define COMMS_64
#endif // _WIN64
#ifdef _DEBUG
#define COMMS_ASSERT
#endif // _DEBUG
#define COMMS_ALIASING
#endif // COMMS_WINDOWS

#ifdef COMMS_MACOS
#include <stdexcept>
#include <typeinfo>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <limits>
#include <iostream>
#ifdef __LP64__
#define COMMS_64
#endif // __LP64__
#define COMMS_ALIASING
#endif // COMMS_MACOS

#ifdef COMMS_LINUX
#include <unistd.h>
#include <stdexcept>
#include <math.h>
#include <limits.h>
#include <limits>
#include <iostream>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>
#include <float.h>
#include <gtk/gtk.h>
#ifdef __LP64__
#define COMMS_64
#endif // __LP64__
#define COMMS_ALIASING __attribute__((__may_alias__))
#endif // COMMS_LINUX

#if defined(COMMS_LINUX) || defined(COMMS_MACOS)
#include <netdb.h> // error: unknown type name 'sockaddr_in'
#include <arpa/inet.h> // error: use of undeclared identifier 'inet_addr'
typedef int SOCKET; // error: unknown type name 'SOCKET'
inline void closesocket(const SOCKET S) { // error: use of undeclared identifier 'closesocket'
    close(S);
}
#define MAX_PATH 1024 // error: use of undeclared identifier 'MAX_PATH'
#define wcscpy_s(dest, dest_size, src) wcscpy(dest, src) // error: use of undeclared identifier 'wcscpy_s'
#define sscanf_s sscanf // Error: Use of undeclared identifier 'sscanf_s'
#define strncpy_s(dest, dest_size, src, src_size) strncpy(dest, src, src_size) // Error: Use of undeclared identifier 'strncpy_s'
#define DWORD uint32_t // error: unknown type name 'DWORD'
#define WORD uint16_t // error: unknown type name 'WORD'
#define BYTE uint8_t // error: unknown type name 'BYTE'
#endif // Linux, macOS

#ifdef COMMS_TIZEN
#include <math.h>
#include <limits.h>
#include <limits>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#ifdef __LP64__
#define COMMS_64
#endif // __LP64__
#define COMMS_ALIASING
#endif // COMMS_TIZEN

#ifdef COMMS_IOS
#include <math.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#ifdef __LP64__
#define COMMS_64
#endif // __LP64__
#define COMMS_ALIASING
#endif // COMMS_IOS

class TagsList;

namespace comms {

class cModel;

// cMessageBox
class cMessageBox {
public:
	static bool YesNo(const char *Caption, const char *Text, ...);
	static void Ok(const char *Caption, const char *Text, ...);
};

#undef cAssert
#undef cAssertM
#undef cAssertBreak
#include "cAssert.h"
#include "cHash.h"
#include "cColor.h"
#include "cBitMap.h"

#include "cFile.h"

#include "cSolver.h"
#include "cLine.h"
#include "cTimer.h"
#include "cSegBezier.h"
#include "cExtrapolate.h"
#include "cInterpolate.h"
#include "cInput.h"

#include "cWidget.h"

typedef cList<cStr>::Ctor cStrs;

struct cSpace {
	enum Enum {
		World, Object
	};
};

#include "cViewer.h"
#include "cImage.h"
#include "cVertex.h"
#include "cRender.h"
#include "cRenderDX.h"
#include "cRenderGL.h"
#include "cRawMesh.h"
#include "cIO.h"

#include "cCodecBmp.h"
#include "cCodecDds.h"
#include "cCodecTga.h"
#include "cCodecJpeg.h"
#include "cCodecPng.h"
#include "cCodecTiff.h"
#include "cCodecBin.h"

#include "cLog.h"
#include "cWin32.h"
#include "cSplash.h"
#include "cSettings.h"

// cMain
extern int cMain_DPI; // The DPI of the monitor where the window is located after the window state has been restored
void cMain_OnInitPath(cStr *Folder); // You can modify default file source with custom absolute path
extern cStr cMain_Title; // Default window title
extern const cStr cMain_Platform; // Windows, macOS, Linux
void cMain_SetWindowTitle(const char *Title); /// Updates only main window title
void cMain_SetTitle(const char *Title); /// Stores the title and then updates window title
int cMain_GetClientWidth();
int cMain_GetClientHeight();
void cMain_OnRender(const cRect &Viewport);
void cMain_OnPreInit();
cStr& cMain_GetStartupMessage();
void cMain_OnInit();
void cMain_OnFree();
void cMain_OnPostFree();
bool cMain_OnClose(); // You should return "true" to allow window closing
void cMain_OnActivate();
extern cImage cMain_OnDropImage;
void cMain_OnDrop(const char *FilePn);
void cMain_OnDropURL(const char *FilePn);
bool cMain_OnDownloadProgress(const int Size);
void cMain_Quit();
extern cStr cMain_LoadSceneXmlFilePn;
extern cStr cMain_WindowStateFile;
void cMain_OnCrash();
const cStr cMain_GetCmdLineArgs();
void cMain_GetTabletState(float *CurPressure, bool *PenPressed, bool *EraserUsed, int *LastUsedTime);
void cMain_GetTdxState(cVec3 *Translation, cVec3 *Rotation, cVec2i *ButtonState);
bool cMain_TdxTimeProportional();
// Font
bool cMain_GetFontChanged();
void cMain_SetFontChanged();
#ifdef COMMS_WINDOWS
void cMain_GetFont(LOGFONT** F);
#endif // Windows
const cStr cMain_GetFontName();
void cMain_SetFontName(const char*);
void cMain_ChooseFont();
void cMain_DrawText(const char *Text, cImage *To, int *TextWidth, int *TextHeight); // cFormat::Rgba8
void cMain_SetTextSize(const int Size);
int cMain_GetTextSize();
void cMain_FontToJson(TagsList& F);
void cMain_FontFromJson(TagsList& F);
// Save / Load
struct cRegistry {
    cList<cStr> IntKeys;
    cList<int> IntValues;
};
void cMain_OnSave(cRegistry *Reg);
void cMain_OnLoad(const cRegistry &Reg);
void cMain_UnloadData();
void cMain_ReloadData();

} // comms




namespace std {

inline std::ostream& operator<<(std::ostream& out, const comms::cVec3& v) {
    typedef comms::cVec< float, 3 >  vec_t;
    return out << static_cast< vec_t >(v);
}

} // std
