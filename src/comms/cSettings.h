#pragma once

// cTabletLibrary
struct cTabletLibrary {
	enum Enum {
#ifdef COMMS_WINDOWS
#ifdef COMMS_TABLET
		WinTab = 0,
		WindowsInk = 1,
		Count = 2
#else // !COMMS_TABLET
		None = 0,
		Count = 1
#endif // COMMS_TABLET
#else // Linux, macOS
		Default = 0,
		Count = 1
#endif // COMMS_WINDOWS
	};
	// cTabletLibrary::ToString
	static const char * ToString(const Enum TabletLibrary) {
		const char *Strings[Count] = {
#ifdef COMMS_WINDOWS
#ifdef COMMS_TABLET
		"WinTab (Wacom)", "WindowsInk"
#else // !COMMS_TABLET
		"None"
#endif // COMMS_TABLET
#else // Linux, macOS
		"Default"
#endif // COMMS_WINDOWS
		};
		cAssert(TabletLibrary >= 0 && TabletLibrary < Count);
		return Strings[TabletLibrary];
	}
};

class cSettings {
public:
	bool FullScreen;
	void FullScreenToggle() {
		FullScreen = !FullScreen;
	}
	bool IgnoreDoubleClicksFromPen;
	int TabletLibrary; // Desired tablet library type
	int CurrentTabletLibrary; // Actually inited tablet library type
	bool TreatEraserAsPen;
	bool VSync;
	bool GammaCorrection;
	cSettings() {
		FullScreen = false;
		IgnoreDoubleClicksFromPen = false;
		TabletLibrary = 0;
		CurrentTabletLibrary = 0;
		TreatEraserAsPen = false;
		VSync = false;
		GammaCorrection = false;
	}
	static cSettings * GetInstance() {
		static cSettings *Ptr = new cSettings;
		return Ptr;
	}
};
