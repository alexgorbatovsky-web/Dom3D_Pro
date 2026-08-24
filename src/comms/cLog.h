#pragma once

//*****************************************************************************
// cLog
//*****************************************************************************
class cLog {
	static qword Time0;
public:
	static bool SkipLogging;
    static void Flush();
	static void Message(const char *Format, ...);
	static void TerminalMessage(const char* Format, ...); // Only Linux and macOS
	static void ResetTime();
	static void TimeMessage(const char *Format, ...);
	static void Warning(const char *Format, ...); // [Ignore] [Exit] (auto exit)
	static void Error(const char *Format, ...); // [Exit] (no auto exit)
	static int Assert(const char *Format, ...); // [Debug] [Ignore] [Ignore Always] [Exit] (auto exit)

	static void Show() {
		ShowDialog(Mode::Message);
	}

	static void EnableAutoSaveToFile(const char *FilePn = "Log.txt");

	static bool IsVisible();
	
	struct Mode {
		enum Enum {
			Message = 0,
			Warning = 1,
			Error = 2,
			Assert = 3
		};
		static bool IsModal(const Enum Mode) {
			return Mode != Message;
		}
		static bool IsAutoExit(const Enum Mode) {
			return Warning == Mode || Assert == Mode;
		}
	};
	
	struct ID {
		enum Values {
			Debug = 101,
			Ignore = 102,
			IgnoreAlways = 103,
			Copy = 104,
			Exit = 105,
			Log = 106,
			StatusBar = 107
		};
	};

#ifdef COMMS_WINDOWS
	static void SetIcon(void *hIcon);
	friend bool cAssertDlg(const char *, const char *, const char *, int, bool &);
#endif // COMMS_WINDOWS

private:
	static void AddString(const char *Format, va_list Args, const bool EchoToTerminal = false, const bool Time = false);
	static int ShowDialog(const Mode::Enum);
};
