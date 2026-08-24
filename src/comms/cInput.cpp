#include "comms.h"

#ifdef COMMS_WINDOWS
void cWinMain_GetMousePositionAcquire(comms::cVec2 *M);
#endif // COMMS_WINDOWS

namespace comms {

// cInputEvent.ctor
cInputEvent::cInputEvent() {
	Class = cInputEventClass::Old;
}

// cInputEvent.dtor
cInputEvent::~cInputEvent() {
}

static bool cInput_Inited = false;
static cList<cInputEvent *> cInput_Events;
static cList<cStr> cInput_Strs;
static cList<int> cInput_Chars; // 1 - char (with control or alt becomes syschar), 2 - syschar only
static cList<float> cInput_DownTimeSec;
static cVec2 cInput_MousePos(0.0f), cInput_MousePosAcquire(0.0f);
static cVec2 cInput_MouseDelta(0.0f);
static float cInput_WheelDelta = 0.0f;
static bool cInput_DoubleClick = false;
struct cInput_MouseState_t {
	bool IsDown[5]; // LeftButton, RightButton, MiddleButton, XButton1, XButton2
	void Clear() {
		IsDown[0] = IsDown[1] = IsDown[2] = IsDown[3] = IsDown[4] = false;
	}
	cInput_MouseState_t() {
		Clear();
	}
};

static cInput_MouseState_t cInput_MouseState;
static cInput::KeyboardState cInput_KeyboardState;
static cList<float> cInput_CharsStartRepeatTimeSec;

void cInput::SendMouseUpEvents() {
	const int Count = sizeof(cInput_MouseState.IsDown);
	for(int i = 0; i < Count; i++) {
		if(cInput_MouseState.IsDown[i]) {
			cInput_MouseState.IsDown[i] = false;
			OldEvent *E = new OldEvent;
			E->Type = OldEvent::TYPE_BUTTON;
			E->Code = i + LeftButton;
			E->Pressed = false;
			E->MousePos = cInput_MousePos;
			AddEvent(E);
			E = nullptr;
		}
	}
}

// cInput::GetMousePosition
const cVec2 & cInput::GetMousePosition() {
	return cInput_MousePos;
}

const cVec2 & comms::cInput::GetMousePositionAcquire() {
	cInput_MousePosAcquire = cInput_MousePos;
#ifdef COMMS_WINDOWS
	cWinMain_GetMousePositionAcquire(&cInput_MousePosAcquire);
#endif // COMMS_WINDOWS
	return cInput_MousePosAcquire;
}

// cInput::GetMouseDelta
const cVec2 & cInput::GetMouseDelta() {
	return cInput_MouseDelta;
}

// cInput::GetWheelDelta
float cInput::GetWheelDelta() {
	return cInput_WheelDelta;
}

// cInput::IsDoubleClick
bool cInput::IsDoubleClick() {
	return cInput_DoubleClick;
}

// cInput_GestureRecognizer
class cInput_GestureRecognizer {
public:
	// cInput_GestureRecognizer.GetInstance
	static cInput_GestureRecognizer * GetInstance() {
		static cInput_GestureRecognizer *Ptr = new cInput_GestureRecognizer;
		return Ptr;
	}
	// cInput_GestureRecognizer.ctor
	cInput_GestureRecognizer() {
		Null();
	}
	// cInput_GestureRecognizer.AddEvent
	void AddEvent(const cInputEvent *E) {
		if (E->Class != cInputEventClass::Touch) {
			return;
		}
		cInputEvent_Gesture *G = nullptr;
		cInputEvent_Touch *T = (cInputEvent_Touch *)E;
		if (cInputEvent_Touch::TYPE::Down == T->Type) {
			if ((-1 == m_TouchA) && (-1 == m_TouchB)) {
				m_TouchA = T->TouchID;
				m_PosA = T->TouchPos;
				G = new cInputEvent_Gesture;
				G->GestureType = cGestureType::Translate1;
				G->GestureState = cInputEvent_Gesture::STATE::Down;
				G->GestureCenter = m_PosA;
				cInput::AddEvent(G); G = nullptr;
			}
			else if ((m_TouchA != -1) && (-1 == m_TouchB)) {
				G = new cInputEvent_Gesture;
				G->GestureType = cGestureType::Translate1;
				G->GestureState = cInputEvent_Gesture::STATE::Up;
				cInput::AddEvent(G); G = nullptr;
				m_TouchB = T->TouchID;
				m_PosB = T->TouchPos;
				m_CenterAB = cVec2::Lerp05(m_PosA, m_PosB);
				G = new cInputEvent_Gesture;
				G->GestureType = cGestureType::Translate2;
				G->GestureState = cInputEvent_Gesture::STATE::Down;
				G->GestureCenter = m_CenterAB;
				cInput::AddEvent(G); G = nullptr;
			}
		}
		else if (cInputEvent_Touch::TYPE::Move == T->Type) {
			if ((T->TouchID == m_TouchA) && (-1 == m_TouchB)) {
				G = new cInputEvent_Gesture;
				G->GestureType = cGestureType::Translate1;
				G->GestureState = cInputEvent_Gesture::STATE::Move;
				G->GestureTranslate = T->TouchPos - m_PosA;
				G->GestureCenter = T->TouchPos;
				cInput::AddEvent(G); G = nullptr;
				m_PosA = T->TouchPos;
			}
			else if ((m_TouchA != -1) && (m_TouchB != -1) && ((T->TouchID == m_TouchA) || (T->TouchID == m_TouchB))) {
				const cVec2 PrevVec = m_PosB - m_PosA;
				const float PrevLenSq = PrevVec.LengthSq();
				if (T->TouchID == m_TouchA) {
					m_PosA = T->TouchPos;
				}
				else {
					cAssert(T->TouchID == m_TouchB);
					m_PosB = T->TouchPos;
				}
				cVec2 NewCenterAB = cVec2::Lerp05(m_PosA, m_PosB);
				const cVec2 NewVec = m_PosB - m_PosA;
				const float NewLenSq = NewVec.LengthSq();
				G = new cInputEvent_Gesture;
				G->GestureType = cGestureType::Translate2;
				G->GestureState = cInputEvent_Gesture::STATE::Move;
				G->GestureTranslate = NewCenterAB - m_CenterAB;
				G->GestureCenter = NewCenterAB;
				if ((NewLenSq > 1.0f) && (PrevLenSq > 1.0f)) {
					G->GestureScale = NewLenSq / PrevLenSq;
					G->GestureRotate = cVec2::Angle(PrevVec, NewVec);
				}
				cInput::AddEvent(G); G = nullptr;
				m_CenterAB = NewCenterAB;
			}
		}
		else if (cInputEvent_Touch::TYPE::Up == T->Type) {
			if ((T->TouchID == m_TouchA) && (-1 == m_TouchB)) {
				G = new cInputEvent_Gesture;
				G->GestureType = cGestureType::Translate1;
				G->GestureState = cInputEvent_Gesture::STATE::Up;
				cInput::AddEvent(G); G = nullptr;
				Null();
			}
			else if ((m_TouchA != -1) && (m_TouchB != -1) && ((T->TouchID == m_TouchA) || (T->TouchID == m_TouchB))) {
				G = new cInputEvent_Gesture;
				G->GestureType = cGestureType::Translate2;
				G->GestureState = cInputEvent_Gesture::STATE::Up;
				cInput::AddEvent(G); G = nullptr;
				if (T->TouchID == m_TouchA) {
					m_TouchA = m_TouchB;
					m_PosA = m_PosB;
				}
				NullB();
				G = new cInputEvent_Gesture;
				G->GestureType = cGestureType::Translate1;
				G->GestureState = cInputEvent_Gesture::STATE::Down;
				G->GestureCenter = m_PosA;
				cInput::AddEvent(G); G = nullptr;
			}
		}
	}
private:
	void Null() {
		m_TouchA = -1;
		m_PosA.SetZero();
		NullB();
	}
	void NullB() {
		m_TouchB = -1;
		m_PosB.SetZero();
		m_CenterAB.SetZero();
	}
	int m_TouchA, m_TouchB;
	cVec2 m_PosA, m_PosB, m_CenterAB;
};

void cInputEvent::LogThis() const {
	cStr S;
	if(cInputEventClass::Old == Class) {
		const cInput::OldEvent *O = (const cInput::OldEvent *)this;
		if(cInput::OldEvent::TYPE_BUTTON == O->Type) {
			S = "Button ";
			S += "\"" + cInput::ToString(O->Code) + "\" ";
			S += O->Pressed ? "Pressed" : "Unpressed";
			if(cInput::IsMouseWheelCode(O->Code)) {
				S += " WheelDelta = " + cStr::ToString(O->WheelDelta);
			}
			cLog::Message(S);
		}
	}
}

// cInput::AddEvent
void cInput::AddEvent(cInputEvent *E) {
    Init();

	if(nullptr == E) {
		return;
	}
	OldEvent *O = (cInputEventClass::Old == E->Class) ? (OldEvent *)E : nullptr;

	if((O != nullptr) && (OldEvent::TYPE_BUTTON == O->Type)) {
		if(O->Code < Esc || O->Code >= LastCode) {
			return;
		}
	}
	cInput_GestureRecognizer::GetInstance()->AddEvent(E);
	static cMutex* Mutex = cMutex::GetInstance();
	Mutex->InputEvents.lock();
	cInput_Events.Add(E);
	Mutex->InputEvents.unlock();

	if((O != nullptr) && (OldEvent::TYPE_BUTTON == O->Type)) {
		if (O->Pressed) {
			cInput_DownTimeSec[O->Code] = cTimer::GetTimeSec();
		}
	}
}

//-----------------------------------------------------------------------------
// cInput::GetEvent
//-----------------------------------------------------------------------------
cInputEvent * cInput::GetEvent() {
	cInput_MouseDelta.SetZero();
	cInput_WheelDelta = 0.0f;
	cInput_DoubleClick = false;

	cInputEvent *E = nullptr;
	static cMutex* Mutex = cMutex::GetInstance();
	Mutex->InputEvents.lock();
	if (!cInput_Events.IsEmpty()) {
		E = cInput_Events[0];
		cInput_Events.RemoveAt(0);
	}
	Mutex->InputEvents.unlock();
	if (E != nullptr) {
		OldEvent *Old = (cInputEventClass::Old == E->Class) ? (OldEvent *)E : nullptr;
		if (Old != nullptr) {
			if (OldEvent::TYPE_MOUSEMOVE == Old->Type) {
				cInput_MousePos = Old->MousePos;
				cInput_MouseDelta = Old->MouseDelta;
			}
			else if (OldEvent::TYPE_BUTTON == Old->Type) {
				if (Old->Pressed && IsMouseButtonCode(Old->Code)) {
					cInput_DoubleClick = Old->DoubleClick;
				}
				if (IsMouseButtonCode(Old->Code)) {
					cInput_MouseState.IsDown[Old->Code - comms::cInput::LeftButton] = Old->Pressed;
					cInput_MousePos = Old->MousePos;
				}
				if (WheelUp == Old->Code || WheelDown == Old->Code) {
					cInput_WheelDelta = Old->WheelDelta;
				}
			}
		}
	}
	return E;
} // cInput::GetEvent

// cInput::FreeEvents
void cInput::FreeEvents() {
	static cMutex* Mutex = cMutex::GetInstance();
	Mutex->InputEvents.lock();
	cInput_Events.FreeContents();
	cInput_Events.Clear();
	Mutex->InputEvents.unlock();
	cInput_MouseState.Clear();
}

//-----------------------------------------------------------------------------
// cInput::IsDown
//-----------------------------------------------------------------------------
bool cInput::IsDown(const int Code) {
	Init();
	
	if(IsKeyboardCode(Code)) {
		return cInput_KeyboardState.IsDown[Code];
	}
	if(IsMouseButtonCode(Code)) {
		return cInput_MouseState.IsDown[Code - LeftButton];
	}
	return false;
} // cInput::IsDown

// cInput::GetDownTimeSec
float cInput::GetDownTimeSec(const int Code) {
	Init();

	if(Code >= Esc && Code < LastCode) {
		return cInput_DownTimeSec[Code];
	}
	return 0.0f;
}

//-----------------------------------------------------------------------------
// cInput::Init
//-----------------------------------------------------------------------------
void cInput::Init() {
	if(cInput_Inited) {
		return;
	}
	
	cInput_Strs.Clear();
	//*************************************************************************
	// Keyboard
	//*************************************************************************
	cInput_Strs.Add("Esc");
	cInput_Strs.Add("F1");
	cInput_Strs.Add("F2");
	cInput_Strs.Add("F3");
	cInput_Strs.Add("F4");
	cInput_Strs.Add("F5");
	cInput_Strs.Add("F6");
	cInput_Strs.Add("F7");
	cInput_Strs.Add("F8");
	cInput_Strs.Add("F9");
	cInput_Strs.Add("F10");
	cInput_Strs.Add("F11");
	cInput_Strs.Add("F12");
	cInput_Strs.Add("0");
	cInput_Strs.Add("1");
	cInput_Strs.Add("2");
	cInput_Strs.Add("3");
	cInput_Strs.Add("4");
	cInput_Strs.Add("5");
	cInput_Strs.Add("6");
	cInput_Strs.Add("7");
	cInput_Strs.Add("8");
	cInput_Strs.Add("9");
	cInput_Strs.Add("-"); // Minus
	cInput_Strs.Add("="); // Equals
	cInput_Strs.Add("BackSpace");
	cInput_Strs.Add("Tab");
	cInput_Strs.Add("CapsLock");
	cInput_Strs.Add("Insert");
	cInput_Strs.Add("Delete");
	cInput_Strs.Add("Home");
	cInput_Strs.Add("End");
	cInput_Strs.Add("PageUp");
	cInput_Strs.Add("PageDown");
	cInput_Strs.Add("Up");
	cInput_Strs.Add("Down");
	cInput_Strs.Add("Left");
	cInput_Strs.Add("Right");
	cInput_Strs.Add("\\"); // BackSlash
	cInput_Strs.Add("Enter");
	cInput_Strs.Add("["); // LeftBracket
	cInput_Strs.Add("]"); // RightBracket
	cInput_Strs.Add(";"); // SemiColon
	cInput_Strs.Add("\'"); // SingleQuote
	cInput_Strs.Add(","); // Comma
	cInput_Strs.Add("."); // Period
	cInput_Strs.Add("/"); // Slash
	cInput_Strs.Add("Shift");
	cInput_Strs.Add("Control");
	cInput_Strs.Add("Alt");
	cInput_Strs.Add("Space");
	cInput_Strs.Add("~"); // Tilda
	cInput_Strs.Add("A");
	cInput_Strs.Add("B");
	cInput_Strs.Add("C");
	cInput_Strs.Add("D");
	cInput_Strs.Add("E");
	cInput_Strs.Add("F");
	cInput_Strs.Add("G");
	cInput_Strs.Add("H");
	cInput_Strs.Add("I");
	cInput_Strs.Add("J");
	cInput_Strs.Add("K");
	cInput_Strs.Add("L");
	cInput_Strs.Add("M");
	cInput_Strs.Add("N");
	cInput_Strs.Add("O");
	cInput_Strs.Add("P");
	cInput_Strs.Add("Q");
	cInput_Strs.Add("R");
	cInput_Strs.Add("S");
	cInput_Strs.Add("T");
	cInput_Strs.Add("U");
	cInput_Strs.Add("V");
	cInput_Strs.Add("W");
	cInput_Strs.Add("X");
	cInput_Strs.Add("Y");
	cInput_Strs.Add("Z");
	// NumPad
	cInput_Strs.Add("NumPad0");
	cInput_Strs.Add("NumPad1");
	cInput_Strs.Add("NumPad2");
	cInput_Strs.Add("NumPad3");
	cInput_Strs.Add("NumPad4");
	cInput_Strs.Add("NumPad5");
	cInput_Strs.Add("NumPad6");
	cInput_Strs.Add("NumPad7");
	cInput_Strs.Add("NumPad8");
	cInput_Strs.Add("NumPad9");
	cInput_Strs.Add("Add");
	cInput_Strs.Add("Subtract");
	cInput_Strs.Add("Multiply");
	cInput_Strs.Add("Divide");
	cInput_Strs.Add("Decimal");
	//*************************************************************************
	// Mouse
	//*************************************************************************
	cInput_Strs.Add("LeftButton");
	cInput_Strs.Add("RightButton");
	cInput_Strs.Add("MiddleButton");
	cInput_Strs.Add("XButton1");
	cInput_Strs.Add("XButton2");
	cInput_Strs.Add("WheelUp");
	cInput_Strs.Add("WheelDown");

	cInput_Chars.Clear();
	//*************************************************************************
	// Keyboard
	//*************************************************************************
	cInput_Chars.Add(0); // Esc
	cInput_Chars.Add(0); // F1
	cInput_Chars.Add(0); // F2
	cInput_Chars.Add(0); // F3
	cInput_Chars.Add(0); // F4
	cInput_Chars.Add(0); // F5
	cInput_Chars.Add(0); // F6
	cInput_Chars.Add(0); // F7
	cInput_Chars.Add(0); // F8
	cInput_Chars.Add(0); // F9
	cInput_Chars.Add(0); // F10
	cInput_Chars.Add(0); // F11
	cInput_Chars.Add(0); // F12
	cInput_Chars.Add(1); // '0', ')'
	cInput_Chars.Add(1); // '1', '!'
	cInput_Chars.Add(1); // '2', '@'
	cInput_Chars.Add(1); // '3', '#'
	cInput_Chars.Add(1); // '4', '$'
	cInput_Chars.Add(1); // '5', '%'
	cInput_Chars.Add(1); // '6', '^'
	cInput_Chars.Add(1); // '7', '&'
	cInput_Chars.Add(1); // '8', '*'
	cInput_Chars.Add(1); // '9', '('
	cInput_Chars.Add(1); // '-', '_'
	cInput_Chars.Add(1); // '=', '+'
	cInput_Chars.Add(2); // BackSpace
	cInput_Chars.Add(2); // Tab
	cInput_Chars.Add(0); // CapsLock
	cInput_Chars.Add(0); // Insert
	cInput_Chars.Add(2); // Delete
	cInput_Chars.Add(0); // Home
	cInput_Chars.Add(0); // End
	cInput_Chars.Add(0); // PageUp
	cInput_Chars.Add(0); // PageDown
	cInput_Chars.Add(2); // Up
	cInput_Chars.Add(2); // Down
	cInput_Chars.Add(2); // Left
	cInput_Chars.Add(2); // Right
	cInput_Chars.Add(1); // '\\', '|'
	cInput_Chars.Add(0); // Enter
	cInput_Chars.Add(1); // '[', '{'
	cInput_Chars.Add(1); // ']', '}'
	cInput_Chars.Add(1); // ';', ':'
	cInput_Chars.Add(1); // '\'', '\"'
	cInput_Chars.Add(1); // ',', '<'
	cInput_Chars.Add(1); // '.', '>'
	cInput_Chars.Add(1); // '/', '?'
	cInput_Chars.Add(0); // Shift
	cInput_Chars.Add(0); // Control
	cInput_Chars.Add(0); // Alt
	cInput_Chars.Add(1); // ' '
	cInput_Chars.Add(1); // '`', '~'
	cInput_Chars.Add(1); // 'a', 'A'
	cInput_Chars.Add(1); // 'b', 'B'
	cInput_Chars.Add(1); // 'c', 'C'
	cInput_Chars.Add(1); // 'd', 'D'
	cInput_Chars.Add(1); // 'e', 'E'
	cInput_Chars.Add(1); // 'f', 'F'
	cInput_Chars.Add(1); // 'g', 'G'
	cInput_Chars.Add(1); // 'h', 'H'
	cInput_Chars.Add(1); // 'i', 'I'
	cInput_Chars.Add(1); // 'j', 'J'
	cInput_Chars.Add(1); // 'k', 'K'
	cInput_Chars.Add(1); // 'l', 'L'
	cInput_Chars.Add(1); // 'm', 'M'
	cInput_Chars.Add(1); // 'n', 'N'
	cInput_Chars.Add(1); // 'o', 'O'
	cInput_Chars.Add(1); // 'p', 'P'
	cInput_Chars.Add(1); // 'q', 'Q'
	cInput_Chars.Add(1); // 'r', 'R'
	cInput_Chars.Add(1); // 's', 'S'
	cInput_Chars.Add(1); // 't', 'T'
	cInput_Chars.Add(1); // 'u', 'U'
	cInput_Chars.Add(1); // 'v', 'V'
	cInput_Chars.Add(1); // 'w', 'W'
	cInput_Chars.Add(1); // 'x', 'X'
	cInput_Chars.Add(1); // 'y', 'Y'
	cInput_Chars.Add(1); // 'z', 'Z'
	// NumPad
	cInput_Chars.Add(0);	// NumPad0
	cInput_Chars.Add(0); // NumPad1
	cInput_Chars.Add(0); // NumPad2
	cInput_Chars.Add(0); // NumPad3
	cInput_Chars.Add(0); // NumPad4
	cInput_Chars.Add(0); // NumPad5
	cInput_Chars.Add(0); // NumPad6
	cInput_Chars.Add(0); // NumPad7
	cInput_Chars.Add(0); // NumPad8
	cInput_Chars.Add(0); // NumPad9
	cInput_Chars.Add(0); // Add
	cInput_Chars.Add(0); // Subtract
	cInput_Chars.Add(0); // Multiply
	cInput_Chars.Add(0); // Divide
	cInput_Chars.Add(0); // Decimal
	//*************************************************************************
	// Mouse
	//*************************************************************************
	cInput_Chars.Add(0); // LeftButton
	cInput_Chars.Add(0); // RightButton
	cInput_Chars.Add(0); // MiddleButton
	cInput_Chars.Add(0); // XButton1
	cInput_Chars.Add(0); // XButton2
	cInput_Chars.Add(0); // WheelUp
	cInput_Chars.Add(0); // WheelDown

	cAssert(cInput_Chars.Count() == cInput_Strs.Count());

	cInput_DownTimeSec.SetCount(cInput_Chars.Count(), 0.0f);
	cInput_MouseState.Clear();
	cInput_KeyboardState.Clear();
	cInput_CharsStartRepeatTimeSec.SetCount(cInput_Chars.Count(), 0.0f);
	
	cInput_Inited = true;
} // cInput::Init

// cInput::ToCode
int cInput::ToCode(const char *Str) {
	Init();
	
	return cInput_Strs.IndexOf(Str, cStr::EqualsNoCase);
}

// cInput::ToString
const cStr cInput::ToString(const int Code) {
	Init();
	
	if(!cInput_Strs.IsEmpty() && Code >= Esc && Code < LastCode) {
		return cInput_Strs[Code];
	}
	return cStr("");
}

static float cInput_LeftMotorSpeed = 0.0f;
static float cInput_RightMotorSpeed = 0.0f;

struct cInput_VIBRATION {
	float LeftMotorSpeed;
	float RightMotorSpeed;
	float VibrateTillTimeSec;
};
static cList<cInput_VIBRATION> cInput_Vibrations;

// cInput::AddVibration
void cInput::AddVibration(const float LeftMotorSpeed, const float RightMotorSpeed, const float DurationSec) {
	cInput_VIBRATION V;
	V.LeftMotorSpeed = LeftMotorSpeed;
	V.RightMotorSpeed = RightMotorSpeed;
	V.VibrateTillTimeSec = cTimer::GetTimeSec() + DurationSec;
	cInput_Vibrations.Add(V);
}

// cInput::StopVibrations
void cInput::StopVibrations() {
	cInput_Vibrations.Clear();
	cInput_LeftMotorSpeed = 0.0f;
	cInput_RightMotorSpeed = 0.0f;
#if defined COMMS_WINDOWS && defined COMMS_XINPUT
	cWinMain_ApplyVibration(0.0f, 0.0f);
#endif // COMMS_WINDOWS && COMMS_XINPUT
}

// cInput::UpdateVibrations
void cInput::UpdateVibrations() {
	int i;
	float L = cInput_LeftMotorSpeed, R = cInput_RightMotorSpeed;
	for(i = 0; i < cInput_Vibrations.Count();) {
		const cInput_VIBRATION &V = cInput_Vibrations[i];
		if(V.VibrateTillTimeSec < cTimer::GetTimeSec()) {
			cInput_Vibrations.RemoveAt(i);
			continue;
		}
		L += V.LeftMotorSpeed;
		R += V.RightMotorSpeed;
		i++;
	}
	if(cPause::GetPause()) {
		L = 0.0f;
		R = 0.0f;
	}
#if defined COMMS_WINDOWS && defined COMMS_XINPUT
	cWinMain_ApplyVibration(cMath::Clamp01(L), cMath::Clamp01(R));
#endif // COMMS_WINDOWS && COMMS_XINPUT
	cInput_LeftMotorSpeed = 0.0f;
	cInput_RightMotorSpeed = 0.0f;
}

// cInput::SetVibration
void cInput::SetVibration(const float LeftMotorSpeed, const float RightMotorSpeed) {
	cInput_LeftMotorSpeed = LeftMotorSpeed;
	cInput_RightMotorSpeed = RightMotorSpeed;
}

//-----------------------------------------------------------------------------
// cInput::Acquire
//-----------------------------------------------------------------------------
void cInput::Acquire() {
	Init();

	UpdateVibrations();
	
	static KeyboardState CurKeyboardState;
	CurKeyboardState.Clear();
	if(!AcquireKeyboard(&CurKeyboardState)) {
		CurKeyboardState = cInput_KeyboardState;
	}

	OldEvent *pEvent = nullptr;
	int Code, Index, RepCount, i;
	bool WasDown, IsDown;
	float CurTimeSec = cTimer::GetSystemTimeSec();
	float CountF, CountR, RestCount;
	
	//*************************************************************************
	// Keyboard char events
	//*************************************************************************
	const float CharRepeatDelaySec = 0.4f;
	const float CharRepeatRateSec = 0.04f;
	bool SysChar = CurKeyboardState.IsDown[Control] || CurKeyboardState.IsDown[Alt];

	// Special plug for stop repeating char if "Control" or "Alt" is just released
	if((cInput_KeyboardState.IsDown[Control] && !CurKeyboardState.IsDown[Control]) || // "Control" is just released
		(cInput_KeyboardState.IsDown[Alt] && !CurKeyboardState.IsDown[Alt])) { // "Alt" is just released
			for(Code = Esc; Code <= Decimal; Code++) {
				cInput_CharsStartRepeatTimeSec[Code] = 0.0f;
			}
	}
	int HandledCharCode = -1; // Only one char per time
	for(Code = Esc; Code <= Decimal; Code++) {
		cAssert(-1 == HandledCharCode);
		if(cInput_Chars[Code] != 0) { // This button has char
			if(!cInput_KeyboardState.IsDown[Code] && CurKeyboardState.IsDown[Code]) { // It is pressed just now
				pEvent = new OldEvent;
				pEvent->Type = OldEvent::TYPE_CHAR;
				if(SysChar || 2 == cInput_Chars[Code]) {
					pEvent->Code = Code;
				} else {
					pEvent->Char = CurKeyboardState.Chars[Code];
				}
				AddEvent(pEvent);
				pEvent = nullptr;
				cInput_CharsStartRepeatTimeSec[Code] = CurTimeSec + CharRepeatDelaySec;
				HandledCharCode = Code;
				break;
			}
		}
	}

	for(Code = Esc; Code <= Decimal; Code++ ) {
		if(cInput_Chars[Code] != 0) {
			if((cInput_KeyboardState.IsDown[Code] && !CurKeyboardState.IsDown[Code]) || (HandledCharCode != -1 && HandledCharCode != Code)) { // It is released just now
				cInput_CharsStartRepeatTimeSec[Code] = 0.0f;
			} else if(cInput_CharsStartRepeatTimeSec[Code] != 0.0f && cInput_CharsStartRepeatTimeSec[Code] <= CurTimeSec) {
				CountF = (CurTimeSec - cInput_CharsStartRepeatTimeSec[Code] + CharRepeatRateSec) / CharRepeatRateSec;
				CountR = cMath::Floor(CountF);
				RestCount = CountF - CountR;
				RepCount = (int)CountR;
				cInput_CharsStartRepeatTimeSec[Code] = CurTimeSec + CharRepeatRateSec - RestCount * CharRepeatRateSec;
				
				for(i = 0; i < RepCount; i++) {
					pEvent = new OldEvent;
					pEvent->Type = OldEvent::TYPE_CHAR;
					if(SysChar || 2 == cInput_Chars[Code]) {
						pEvent->Code = Code;
					} else {
						pEvent->Char = CurKeyboardState.Chars[Code];
					}
					AddEvent(pEvent);
					pEvent = nullptr;
					HandledCharCode = Code;
				}
			}
		}
	}

	cInput_KeyboardState = CurKeyboardState;
	
	if(!EnableEvents()) {
		FreeEvents();
	}
} // cInput::Acquire

} // comms

// cLinuxMain_GetEvents
comms::cList<comms::cInputEvent *> & cLinuxMain_GetEvents() {
	return comms::cInput_Events;
}
