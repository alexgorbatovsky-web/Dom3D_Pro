#pragma once

#ifdef COMMS_ASSERT

bool cAssertDlg(const char *Exp, const char *Msg, const char *File, int Line, bool &IgnoreAlways);
void cAssertBreak( const char* Exp, const char* Msg, const char* File, int Line );

// cAssert
#define cAssert(Exp) {\
	static bool IgnoreAlways = false;\
	if(!IgnoreAlways && !(Exp)) {\
		if(comms::cAssertDlg(#Exp, nullptr, __FILE__, __LINE__, IgnoreAlways)) {\
            comms::cAssertBreak( #Exp, "", __FILE__, __LINE__ );\
		}\
	}\
}

// cAssertM
#define cAssertM(Exp, Msg) {\
	static bool IgnoreAlways = false;\
	if(!IgnoreAlways && !(Exp)) {\
		if(comms::cAssertDlg(#Exp, Msg, __FILE__, __LINE__, IgnoreAlways)) {\
            comms::cAssertBreak( #Exp, Msg, __FILE__, __LINE__ );\
		}\
	}\
}

#else

#define cAssert(Exp)		((void)0)
#define cAssertM(Exp, Msg)	((void)0)

#endif // COMMS_ASSERT
