#include "comms.h"

#ifdef COMMS_MACOS
void cMacMain_ShowSplash(const comms::cImage *Image, const int CornerRadius);
void cMacMain_CloseSplash();
#define __OPENSCRIPTING__
#include <Carbon/Carbon.h>
#endif // COMMS_MACOS

namespace comms {

// cSplash_Data
class cSplash_Data {
public:
	cSplash_Data() {
		m_Loaded = false;
		m_Valid = false;
	}
	bool Load() {
		if(!m_Loaded) {
			m_Loaded = true;
			m_Valid = SearchFiles() && LoadImages();
		}
		return m_Valid;
	}
	cImage Background, Digits[10], Dot, Version;
	void Free() {
		if(m_Loaded) {
			m_Loaded = false;
			m_Valid = false;
			Background.Free();
			int i;
			for(i = 0; i < 10; i++) {
				Digits[i].Free();
			}
			Dot.Free();
			Version.Free();
			m_Background.Clear();
			for(i = 0; i < 10; i++) {
				m_Digits[i].Clear();
			}
			m_Dot.Clear();
			m_Version.Clear();
			m_Dot.Clear();
		}
	}
private:
	bool m_Loaded, m_Valid;
	
	cStr m_Background, m_Digits[10], m_Dot, m_Version;
	bool SearchFiles() {
		const cStr PathToSplash = "data/Splash";
		cList<cStr> AllFiles, AllBackgrounds;
		cIO::SearchFiles(PathToSplash, &AllFiles);
		if(AllFiles.IsEmpty()) {
			return false;
		}
		int i;
		cStr P;
		for(i = 0; i < AllFiles.Count(); i++) {
			const cStr &r = AllFiles[i];
			P = r;
			P.RemoveFilePath();
			P.RemoveFileExtension();
			if(1 == P.Length() && P[0] >= '0' && P[0] <= '9') {
				m_Digits[P[0] - '0'] = r;
			} else if(cStr::EqualsNoCase(P, "Dot")) {
				m_Dot = r;
			} else if(cStr::EqualsNoCase(P, "Version")) {
				m_Version = r;
			} else if(P.StartsWith("Background", true)) {
				AllBackgrounds.Add(r);
			}
		}
		if(!AllBackgrounds.IsEmpty()) {
			int64 T = (int64)cTimer::AcquireClockSeconds();
			i = T % AllBackgrounds.Count();
			m_Background = AllBackgrounds[i];
		}
		return true;
	}

	bool LoadImages() {
		if(m_Background.IsEmpty() || !cIO::LoadImage(m_Background, &Background) || !Background.ToFormat(cFormat::Rgb8)) {
			return false;
		}
		int i;
		for(i = 0; i < 10; i++) {
			const cStr &r = m_Digits[i];
			if(r.IsEmpty() || !cIO::LoadImage(r, &Digits[i]) || !Digits[i].ToFormat(cFormat::Rgb8)) {
				return false;
			}
		}
		if(m_Dot.IsEmpty() || !cIO::LoadImage(m_Dot, &Dot) || !Dot.ToFormat(cFormat::Rgb8)) {
			return false;
		}
		if(m_Version.IsEmpty() || !cIO::LoadImage(m_Version, &Version) || !Version.ToFormat(cFormat::Rgb8)) {
			return false;
		}
		return true;
	}
};
static cSplash_Data s_Data;

// cSplash_Image
class cSplash_Image {
public:
	cSplash_Image() {
		m_Created = false;
		Result = nullptr;
	}
	void Create(const cSplash::ShowArgs &Args) {
		if(!m_Created) {
			m_Created = true;
			ValidateVersion(Args.Version);
			MergeImages(Args.OffsetX, Args.OffsetY);
		}
	}
	void Clear() {
		if(m_Created) {
			m_Created = false;
			Result = nullptr;
			m_Version.Clear();
		}
	}
	cImage *Result;
private:
	bool m_Created;
	
	cStr m_Version;
	void ValidateVersion(const cStr &Version) {
		int i;
		for(i = 0; i < Version.Length(); i++) {
			const char c = Version[i];
			if((c >= '0' && c <= '9') || ('.' == c)) {
				m_Version.Append(c);
			}
		}
	}
	void MergeImages(const int OffsetX, const int OffsetY) {
		Result = &s_Data.Background;
		bool Forward = true;
		int CurX = OffsetX;
		if(CurX < 0) {
			CurX += Result->GetWidth();
			Forward = false;
		}
		const int Y = (OffsetY < 0) ? (Result->GetHeight() + OffsetY) : OffsetY;
		int i;
		const cImage *CurImg = nullptr;
		if(Forward) {
			Result->MergeRgb8(s_Data.Version, CurX, Y);
			CurX += s_Data.Version.GetWidth();
			for(i = 0; i < m_Version.Length(); i++) {
				const char c = m_Version[i];
				if(c >= '0' && c <= '9') {
					CurImg = &s_Data.Digits[c - '0'];
				} else {
					cAssert('.' == c);
					CurImg = &s_Data.Dot;
				}
				Result->MergeRgba8(*CurImg, CurX, Y, true);
				CurX += CurImg->GetWidth();
			}
		} else {
			for(i = m_Version.Length() - 1; i >= 0; i--) {
				const char c = m_Version[i];
				if(c >= '0' && c <= '9') {
					CurImg = &s_Data.Digits[c - '0'];
				} else {
					cAssert('.' == c);
					CurImg = &s_Data.Dot;
				}
				CurX -= CurImg->GetWidth();
				Result->MergeRgba8(*CurImg, CurX, Y, true);
			}
			CurX -= s_Data.Version.GetWidth();
			Result->MergeRgba8(s_Data.Version, CurX, Y, true);
		}
	}
};
static cSplash_Image s_Image;

// cSplash_Window
class cSplash_Window {
public:
    float DelaySec;
    int CornerRadius;
	cSplash_Window() {
		m_Created = false;
        DelaySec = 0.0f;
        CornerRadius = 0;
		Ctor();
	}
	void Create() {
		if(!m_Created) {
			m_Created = true;
			CreateAndShow();
		}
	}
	void Close() {
		if(m_Created) {
			m_Created = false;
			Destroy();
		}
	}
	void CloseLater() {
		if(m_Created) {
#ifdef COMMS_WINDOWS
			if(-1 == m_TimerID) {
				m_TimerID = 1;
				int Ms = (int)cMath::Round(cMath::MsPerSec * DelaySec);
				SetTimer(m_hWnd, m_TimerID, Ms, nullptr);
			}
#endif // COMMS_WINDOWS
#ifdef COMMS_MACOS
            if(-1 == m_DispatchTime) {
                int64_t Nsec = (int64_t)(DelaySec * NSEC_PER_SEC);
                m_DispatchTime = dispatch_time(DISPATCH_TIME_NOW, Nsec);
                dispatch_after(m_DispatchTime, dispatch_get_main_queue(), ^(void) {
                    OnTimeout();
                });
            }
#endif // COMMS_MACOS
		}
	}
private:
	bool m_Created;

#ifdef COMMS_LINUX
	void Ctor() {}
	void CreateAndShow() {}
	void Destroy() {}
#endif // COMMS_LINUX

#ifdef COMMS_MACOS
    dispatch_time_t m_DispatchTime;
	void Ctor() {
        m_DispatchTime = -1;
    }
	void CreateAndShow() {
        s_Image.Result->Flip();
        cMacMain_ShowSplash(s_Image.Result, CornerRadius);
    }
	void Destroy() {
        cMacMain_CloseSplash();
    }
    static void OnTimeout();
#endif // COMMS_MACOS

#ifdef COMMS_WINDOWS
	void Ctor() {
		m_hWnd = nullptr;
		m_TimerID = -1;
	}
	HWND m_hWnd;
	static LRESULT WINAPI WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
	void Destroy() {
		if(m_hWnd != nullptr) {
			if(m_TimerID != -1) {
				KillTimer(m_hWnd, m_TimerID);
				m_TimerID = -1;
			}
			DestroyWindow(m_hWnd);
			m_hWnd = nullptr;
			m_Buffer.Free();
			memset(&m_BMI, 0, sizeof(m_BMI));
		}
	}
	void CreateAndShow() {
		if(m_hWnd != nullptr) {
			return;
		}
		CreateBMI();

		WNDCLASS wc;
		memset(&wc, 0, sizeof(wc));
		wc.hInstance = GetModuleHandle(nullptr);
		wc.lpszClassName = "cSplash_Window";
		wc.lpfnWndProc = WndProc;
		RegisterClass(&wc);
		const int W = s_Image.Result->GetWidth();
		const int H = s_Image.Result->GetHeight();
        m_hWnd = CreateWindowEx(0, "cSplash_Window", "", WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
            0, 0, W, H, nullptr, nullptr, wc.hInstance, nullptr);
		HRGN hRgn = CreateRoundRectRgn(0, 0, W, H, CornerRadius, CornerRadius);
		SetWindowRgn(m_hWnd, hRgn, TRUE);
		cWin32::CenterWindow(m_hWnd, nullptr);
		ShowWindow(m_hWnd, SW_SHOWNORMAL);
	}
	void CreateBMI() {
		s_Image.Result->SwapChannels(0, 2);
		DWORD ResultBytesPerLine = 3 * s_Image.Result->GetWidth();
		DWORD BytesPerLine = ResultBytesPerLine;
		int M = BytesPerLine % 4;
		if(M != 0) {
			BytesPerLine += 4 - M;
		}
		memset(&m_BMI, 0, sizeof(m_BMI));
		m_BMI.bmiHeader.biSize = sizeof(m_BMI.bmiHeader);
		m_BMI.bmiHeader.biWidth = s_Image.Result->GetWidth();
		m_BMI.bmiHeader.biHeight = s_Image.Result->GetHeight();
		m_BMI.bmiHeader.biPlanes = 1;
		m_BMI.bmiHeader.biBitCount = 24;
		m_BMI.bmiHeader.biCompression = BI_RGB;
		m_BMI.bmiHeader.biSizeImage = BytesPerLine * m_BMI.bmiHeader.biHeight;
		
		m_Buffer.SetCount(m_BMI.bmiHeader.biSizeImage);
		int y;
		byte *Into = m_Buffer.ToPtr();
		const byte *From = s_Image.Result->GetPixels();
		for(y = 0; y < s_Image.Result->GetHeight(); y++) {
			memcpy(Into, From, ResultBytesPerLine);
			Into += BytesPerLine;
			From += ResultBytesPerLine;
		}
	}
	void DrawBMI(HDC hDC) const {
		SetDIBitsToDevice(hDC, 0, 0, m_BMI.bmiHeader.biWidth, m_BMI.bmiHeader.biHeight, 0, 0, 0, m_BMI.bmiHeader.biHeight, m_Buffer.ToPtr(), &m_BMI, DIB_RGB_COLORS);
	}
	BITMAPINFO m_BMI;
	cList<BYTE> m_Buffer;
	int m_TimerID;
#endif // COMMS_WINDOWS
};
static cSplash_Window s_Window;

#ifdef COMMS_MACOS
void cSplash_Window::OnTimeout() {
    s_Window.Close();
    s_Window.m_DispatchTime = -1;
}
#endif // COMMS_MACOS

#ifdef COMMS_WINDOWS
LRESULT WINAPI cSplash_Window::WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	if(WM_ERASEBKGND == Msg) {
		HDC hDC = (HDC)wParam;
		s_Window.DrawBMI(hDC);
		return TRUE;
	} else if(WM_TIMER == Msg && s_Window.m_TimerID == wParam) {
		s_Window.Close();
	}
	return DefWindowProc(hWnd, Msg, wParam, lParam);
}
#endif // COMMS_WINDOWS

// cSplash::Show
void cSplash::Show(const ShowArgs &Args) {
	if(s_Data.Load()) {
		s_Image.Create(Args);
        s_Window.CornerRadius = Args.CornerRadius;
		s_Window.Create();
        s_Window.DelaySec = Args.DelaySec;
		s_Image.Clear();
	}
	s_Data.Free();
}

// cSplash::DestroyNow
void cSplash::DestroyNow() {
	s_Window.Close();
}

// cSplash::DestroyLater
void cSplash::DestroyLater() {
	s_Window.CloseLater();
}

} // comms
