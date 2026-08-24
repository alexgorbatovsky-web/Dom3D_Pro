#include "comms.h"

#if defined COMMS_3DCOAT && defined COMMS_WINDOWS
#include "../3D-Coat/smart_assert.h"
#endif // COMMS_3DCOAT && COMMS_WINDOWS


// for a stable work scripts when calls `cAssert()`
bool CoatAssertDebugInsteadOfException = true;
comms::cStr CoatLastException = "";


namespace comms {

#ifdef COMMS_ASSERT


void cAssertBreak( const char* Exp, const char* Msg, const char* File, int Line ) {
    CoatLastException =
        (cStr)Exp + "\n" + Msg + "\nFile '" + File + "', line " + Line;
    if ( CoatAssertDebugInsteadOfException ) {
#ifdef COMMS_WINDOWS
        __debugbreak();
#endif // COMMS_WINDOWS
#if defined COMMS_MACOS || defined COMMS_LINUX
        raise( SIGINT );
#endif // COMMS_MACOS || COMMS_LINUX
    }
    else {
        throw ::std::runtime_error( CoatLastException.ToCharPtr() );
    }
}


//-------------------------------------------------------------------------------------------------
// cAssertDlg
//-------------------------------------------------------------------------------------------------
bool cAssertDlg(const char *Exp, const char *Msg, const char *File, int Line, bool &IgnoreAlways) {
    cStr SourceFileName = cStr(File).GetFileName();
    cStr Buffer;
    Buffer += cStr(50, '-');
	Buffer += cStr::EndLn + "Assertion failed!" + cStr::EndLn;
    Buffer += cStr::EndLn + "File: ";
	Buffer += SourceFileName;
	Buffer += cStr::EndLn + "Line: ";
	Buffer += Line;
	Buffer += cStr::EndLn + "Expression: ";
	Buffer += Exp;
	Buffer += cStr::EndLn + "Message: ";
	Buffer += Msg != nullptr ? Msg : "";
	Buffer += cStr::EndLn + cStr::EndLn;
#if defined COMMS_3DCOAT && defined COMMS_WINDOWS
	SmartAssert(File,Line,Exp,Msg);
	return false;
#endif // COMMS_3DCOAT && COMMS_WINDOWS
	const int id = cLog::Assert(Buffer.ToCharPtr(), nullptr);
	switch(id) {
		case cLog::ID::IgnoreAlways:
			IgnoreAlways = true;
		case cLog::ID::Ignore:
			return false;
		case cLog::ID::Debug:
		default:
			return true;
	}
} // cAssertDlg

#endif // COMMS_ASSERT

} // comms
