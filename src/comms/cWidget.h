#pragma once

class cWidget;
class cTheme;

//*****************************************************************************
// cWidgets
//*****************************************************************************
class cWidgets {
public:
	enum Constants {
		TOP_LAYER = 32,
		LAYER_SHIFT = 16
	};

	// This is the predefined layout. Of course you can insert layers as you need them,
	// but "Screen" and "TopScreen" should be the last layers in the system.
	struct Layers {
		enum Indexes {
			Scene = 0,
			Tools = 1,
			Viewer = 2,
			Screen = 3,
			TopScreen = 4 // For "ComboBox_DropDown", "ToolTip", etc.
		};
	};
	
	static int AddWidget(cWidget *pWidget, const int Layer);

	static cWidget * GetWidget(const int WidgetID);
	static int GetWidgetID(const cWidget *Widget);
	static int GetWidgetLayer(const cWidget *Widget);
	
	// Remove widget and call its dtor
	static void FreeWidget(const int WidgetID, const bool FreeObject = true);
	static void FreeWidget(const cWidget *Widget, const bool FreeObject = true);
	
	static void FreeLayer(const int Layer); // Frees all widgets in specified layer
	static void FreeAll();

	static void BringToFront(const int WidgetID); // Brings the widget to the front within its layer
	static void BringToFront(const cWidget *Widget);
	
	static const cList<cWidget *> & GetLayer(const int Index);
	static const cList<int> & GetLayerFreeSlots(const int Index);
	static const cList<int> & GetLayerOrder(const int Index);
	
	static void HandleInputEvents();
	static void SetCapture(); // Causes the calling this function widget to take processing of all input events
	static void SetCapture(const int WidgetID);
	static void SetCapture(const cWidget *Widget);
	static void ReleaseCapture(); // till this function is called.
	static bool IsCaptured(); // Returns "true" if any widget captures input
	static bool IsCaptured(const cWidget *Widget); // Returns "true" if the specified widget
	static bool IsCaptured(const int WidgetID); // captures input
	
	static void Render(const int Layer = -1); // -1 - all
private:
	static void FreeDead(); // Frees all widgets which returns "true" on "IsDead" method
	static void ApplyWithinCriticalSectionQueries();
	static cList<cWidget *> s_Layers[TOP_LAYER + 1];
	static cList<int> s_FreeSlots[TOP_LAYER + 1];
	static cList<int> s_Order[TOP_LAYER + 1];
	static int m_idSendTo, m_idCurProcessing;
	static bool s_CriticalSection;
	// If "s_CriticalSection" is "true":
	static cList<int> s_ToFree; // - method "FreeWidget" will only add widget ID to list "s_ToFree";
	static cList<int> s_BringToFront; // - method "BringToFont" will add widget ID to list "s_BringToFront".
	
	static cWidget *s_ToolTipBlock;
	static float s_ToolTipLagSec;
	static cStr s_ToolTipStr;
	static cVec2 s_ToolTipPos;
	static const cTheme *s_ToolTipTheme;
}; // cWidgets

//*****************************************************************************
// cWidget
//*****************************************************************************
class cWidget {
public:
	virtual ~cWidget() {}
	virtual bool OnButtonDown(const int Code) {
		return false;
	}
	virtual bool OnButtonUp(const int Code) {
		return false;
	}
	virtual bool OnMouseMove() {
		return cWidgets::IsCaptured(this);
	}
	virtual bool OnChar(const int Char, const int Code) {
		return cWidgets::IsCaptured(this);
	}
    virtual void OnTouchDown(const int ID, const cVec2 &Pos) {
    }
	virtual void OnTouchMove(const int ID, const cVec2 &Pos, const cVec2 &Delta) {
	}
    virtual void OnTouchUp(const int ID) {
    }
	virtual void OnGestureDown(const cGestureType::Enum Type, const cVec2 &Center) {
	}
	virtual void OnGestureMove(const cVec2 &Center, const cVec2 &Translate, const float Rotate, const float Scale) {
	}
	virtual void OnGestureUp() {
	}
	virtual void OnPenDown(const bool Eraser, const cVec2 &Pos, const float Pressure) {
	}
	virtual void OnPenMove(const cVec2 &Pos, const cVec2 &Delta, const float Pressure) {
	}
	virtual void OnPenUp() {
	}
	virtual void OnRender() {}
	virtual bool IsDead() {
		return false;
	}
	// Handler "IsHot" is introduced for determining current hot widget in the whole system.
	// As side effect it serves for uniform implementation hot feature, since it is called every frame in all widgets.
	// Widget, which returns "true" if "Handled" is "false", becomes current hot widget in the whole system.
	// Such widget can set own cursor through immediately called "OnSetCursor" handler.
	// Even if some widget returns "true" within "IsHot" handler, it will be called
	// in all subsequent widgets, but with "Handled" set to "true".
	// Moreover, it will be called even if some widget is capturing input.
	// In such case first call to "IsHot" is performed in widget with captured input with "Handled" set to "false",
	// and all subsequent calls will be performed with "Handled" set to the result of that first call, excluding
	// widget with captured input. That means, that if some widget captures input it can be not hot.
	// To be or not to be hot :-) is that widget decision.
	virtual bool IsHot(const bool Handled) {
		return false;
	}
	virtual bool OnSetCursor(cInput::Cursor::Enum *Cursor) { // Is called every frame in hot widget.
		return false;
	}
	virtual bool OnSetToolTip(cStr *ToolTip, const cTheme **Theme) { // Is called every frame in hot widget
		return false;
	}
	virtual void OnBringToFront() {}
}; // cWidget
