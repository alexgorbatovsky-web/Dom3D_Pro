#pragma once

// cInputEventClass
struct cInputEventClass {
	enum Enum {
		Old = 0,
		Touch = 1,
		Pen = 2,
		Gesture = 3
	};
};

//*****************************************************************************
// cInputEvent
//*****************************************************************************
class cInputEvent {
public:
	void LogThis() const;
	cInputEventClass::Enum Class;

	// cInputEvent.ctor
	cInputEvent();
	// cInputEvent.dtor
	~cInputEvent();
}; // cInputEvent

// cInputEvent_Touch
class cInputEvent_Touch : public cInputEvent {
public:
	// cInputEvent_Touch.ctor
	cInputEvent_Touch() {
		Class = cInputEventClass::Touch;
		TouchID = -1;
		Type = TYPE::None;
		TouchPos.SetZero();
		TouchDelta.SetZero();
	}
	struct TYPE {
		enum Enum {
			None = 0,
			Down = 1,
			Up = 2,
			Move = 3
		};
	};

	int TouchID;
	TYPE::Enum Type;
	cVec2 TouchPos;
	cVec2 TouchDelta;
};

// cInputEvent_Pen
class cInputEvent_Pen : public cInputEvent {
public:
	// cInputEvent_Pen.ctor
	cInputEvent_Pen() {
		Class = cInputEventClass::Pen;
		Eraser = false;
		Type = TYPE::None;
		PenPos.SetZero();
		PenDelta.SetZero();
		PenPressure = 0.0f;
	}
	struct TYPE {
		enum Enum {
			None = 0,
			Down = 1,
			Up = 2,
			Move = 3
		};
	};

	bool Eraser;
	TYPE::Enum Type;
	cVec2 PenPos;
	cVec2 PenDelta;
	float PenPressure;
};

// cGestureType
struct cGestureType {
	enum Enum {
		None = 0,
		Translate1 = 1,
		Translate2 = 2,
		Rotate = 3,
		Scale = 4
	};
};

// cInputEvent_Gesture
class cInputEvent_Gesture : public cInputEvent {
public:
	// cInputEvent_Gesture.ctor
	cInputEvent_Gesture() {
		Class = cInputEventClass::Gesture;
		GestureType = cGestureType::None;
		GestureState = STATE::None;
		GestureCenter.SetZero();
		GestureTranslate.SetZero();
		GestureRotate = 0.0f;
		GestureScale = 1.0f;
	}
	// STATE
	struct STATE {
		enum Enum {
			None = 0,
			Down = 1,
			Move = 2,
			Up = 3
		};
	};
	cGestureType::Enum GestureType;
	STATE::Enum GestureState;
	cVec2 GestureCenter;
	cVec2 GestureTranslate;
	float GestureRotate;
	float GestureScale;
};

//*****************************************************************************
// cInput
//*****************************************************************************
class cInput {
public:
	friend class cWidgets;
	
	static void Acquire();
	
	enum Codes {
		// Keyboard
		Esc = 0, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
		Zero,	// "0"
		One,	// "1"
		Two,	// "2"
		Three,	// "3"
		Four,	// "4"
		Five,	// "5"
		Six,	// "6"
		Seven,	// "7"
		Eight,	// "8"
		Nine,	// "9"
		Minus,	// "-"
		Equals,	// "="
		BackSpace, Tab, CapsLock,
		Insert, Delete, Home, End, PageUp, PageDown,
		Up, Down, Left, Right,
		BackSlash, // "\\"
		Enter,
		LeftBracket,	// "["
		RightBracket,	// "]"
		SemiColon,		// ";"
		SingleQuote,	// "\'"
		Comma,			// ","
		Period,			// "."
		Slash,			// "/"
		Shift, Control, Alt, Space,
		Tilda,	// "~"
		A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		// NumPad
		NumPad0, NumPad1, NumPad2, NumPad3, NumPad4, NumPad5, NumPad6, NumPad7, NumPad8, NumPad9,
		Add, Subtract, Multiply, Divide, Decimal,
		// Mouse
		LeftButton, RightButton, MiddleButton, XButton1, XButton2,
		WheelUp, WheelDown, LastCode
	};
	
	struct Cursor {
		enum Enum {
			Default = -1, // No cursor force
			Arrow = 0, UpArrow = 1,
			SizeHor = 2, SizeVert = 3, SizeSlash = 4, SizeBackSlash = 5, SizeAll = 6,
			IBeam = 7, Cross = 8, Wait = 9, Stop = 10, None = 11
		};
	};
	
	class OldEvent : public cInputEvent {
	public:
		enum TYPE {
			TYPE_NONE,
			TYPE_BUTTON,
			TYPE_MOUSEMOVE,
			TYPE_CHAR
        };
		
		TYPE Type;
		int Code;
		bool Pressed;
		bool DoubleClick;
		cVec2 MousePos;
        cVec2 MouseDelta;
		float WheelDelta;
		int Char;
        
        // TYPE_MOUSEMOVE
		//	MousePos
		//	MouseDelta

		// TYPE_BUTTON
		//	Code
		//	Pressed
		//	DoubleClick		"LeftButton", "RightButton", and "MiddleButton"
		//	MousePos		"LeftButton", "RightButton", "MiddleButton", "WheelUp", and "WheelDown"
		//	WheelDelta		"WheelUp" and "WheelDown"

		// TYPE_CHAR
		//	Char or Code
        
		OldEvent() {
			Type = TYPE_NONE;
			Code = -1;
			Pressed = false;
			DoubleClick = false;
			MousePos.SetZero();
            MouseDelta.SetZero();
			WheelDelta = 0.0f;
			Char = -1;
		}
	};
	
	static void AddEvent(cInputEvent *); // Do not release pointer
	static cInputEvent * GetEvent(); // You should release pointer
	static void FreeEvents(); // Free all accumulated events
	
	static int ToCode(const char *Str);
	static const cStr ToString(const int Code);
	static bool IsKeyboardCode(const int Code) {
		return (Code >= Esc) && (Code <= Decimal);
	}
	static bool IsMouseButtonCode(const int Code) { // Should not include "WheelUp" and "WheelDown"
		return (LeftButton == Code) || (RightButton == Code) || (MiddleButton == Code) || (XButton1 == Code) || (XButton2 == Code);
	}
	static bool IsMouseWheelCode(const int Code) {
		return (WheelUp == Code) || (WheelDown == Code);
	}
	static bool IsDown(const int Code);
	static bool IsDownAcquire(const int Code);
    static bool IsDownAcquireLeftButton() {
        return IsDownAcquire(LeftButton);
    }
	
	static float GetDownTimeSec(const int Code); // Pressed time
	
	static const cVec2 & GetMousePosition();
	static const cVec2 & GetMousePositionAcquire();
	static const cVec2 & GetMouseDelta();
    static float GetWheelDelta();
	static void SendMouseUpEvents();
	
	static const cVec2 & GetLeftThumbPos();
	static const cVec2 & GetRightThumbPos();
	static float GetLeftTriggerPos();
	static float GetRightTriggerPos();
	
	static void SetVibration(const float LeftMotorSpeed, const float RightMotorSpeed);
	static void AddVibration(const float LeftMotorSpeed, const float RightMotorSpeed, const float DurationSec);
	static void StopVibrations();
	
	// Double click state. Should be checked during "OnMouseDown" event handling.
	static bool IsDoubleClick();

	static void WarpCursor(const int LocalX, const int LocalY, const bool DownY);
	
	struct KeyboardState {
		cList<bool> IsDown;
		cList<int> Chars;
		
		void Clear() {
			IsDown.SetCount(Decimal - Esc + 1, false);
			Chars.SetCount(Decimal - Esc + 1, -1);
		}
		KeyboardState() {
			Clear();
		}
	};
	static void ForceDefaultCursor() {
		SetCursor(Cursor::Default);
	}
	static bool AcquireKeyboard(KeyboardState*);
private:
	// Should be never called directly. For use only within "cWidgets".
	static void SetCursor(const Cursor::Enum);
	static void SetCapture();
	static void ReleaseCapture();
	static bool EnableEvents();
	
	static void Init();
	static void UpdateVibrations();
};
