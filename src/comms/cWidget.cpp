#include "comms.h"

#ifdef COMMS_WINDOWS
void cWinMain_SetCapture();
void cWinMain_ReleaseCapture();
#endif // COMMS_WINDOWS

namespace comms {

cList<cWidget *> cWidgets::s_Layers[cWidgets::TOP_LAYER + 1];
cList<int> cWidgets::s_FreeSlots[cWidgets::TOP_LAYER + 1];
cList<int> cWidgets::s_Order[cWidgets::TOP_LAYER + 1];
float cWidgets::s_ToolTipLagSec = 0.0f;
cStr cWidgets::s_ToolTipStr;
cVec2 cWidgets::s_ToolTipPos = cVec2::Zero;
cWidget *cWidgets::s_ToolTipBlock = nullptr;
const cTheme *cWidgets::s_ToolTipTheme = nullptr;
bool cWidgets::s_CriticalSection = false;
cList<int> cWidgets::s_ToFree;
cList<int> cWidgets::s_BringToFront;

// cWidgets::GetLayer
const cList<cWidget *> & cWidgets::GetLayer(const int Index) {
	cAssert(Index >= 0 && Index <= TOP_LAYER);

	if(Index < 0 || Index > TOP_LAYER) {
		return s_Layers[0];
	}
	return s_Layers[Index];
}

// cWidgets::GetLayerFreeSlots
const cList<int> & cWidgets::GetLayerFreeSlots(const int Index) {
	cAssert(Index >= 0 && Index <= TOP_LAYER);

	if(Index < 0 || Index > TOP_LAYER) {
		return s_FreeSlots[0];
	}
	return s_FreeSlots[Index];
}

// cWidgets::GetLayerOrder
const cList<int> & cWidgets::GetLayerOrder(const int Index) {
	cAssert(Index >= 0 && Index <= TOP_LAYER);

	if(Index < 0 || Index > TOP_LAYER) {
		return s_Order[0];
	}
	return s_Order[Index];
}

//-----------------------------------------------------------------------------
// cWidgets::AddWidget
//-----------------------------------------------------------------------------
int cWidgets::AddWidget(cWidget *pWidget, const int Layer) {
	// No way to add "nullptr" widget
	cAssert(pWidget != nullptr);
	if(nullptr == pWidget) {
		return -1;
	}

	// Check layer index
	cAssert(Layer >= 0 && Layer <= TOP_LAYER);
	if(Layer < 0 || Layer > TOP_LAYER) {
		return -1;
	}

	int Index = -1;
	
	// Looking for free slots in requested layer 
	if(!s_FreeSlots[Layer].IsEmpty()) { // Layer has empty slots. Using last one.
		Index = s_FreeSlots[Layer].GetLast();
		s_FreeSlots[Layer].RemoveLast();
		s_Layers[Layer][Index] = pWidget;
	}
	
	if(-1 == Index) { // There is no empty slot. Add new
		Index = s_Layers[Layer].Add(pWidget);
	}
	
	s_Order[Layer].Add(Index); // Adding widget to the end of order list
	
	return Index + (Layer << LAYER_SHIFT);
} // cWidgets::AddWidget

//-----------------------------------------------------------------------------
// cWidgets::GetWidget
//-----------------------------------------------------------------------------
cWidget * cWidgets::GetWidget(const int WidgetID) {
	cAssert(WidgetID != -1);
	if(-1 == WidgetID) {
		return nullptr;
	}

	const int Index = WidgetID & ((1 << LAYER_SHIFT) - 1);
	const int Layer = WidgetID >> LAYER_SHIFT;

	// Check layer index
	cAssert(Layer >= 0 && Layer <= TOP_LAYER);
	if(Layer < 0 || Layer > TOP_LAYER) {
		return nullptr;
	}

	// Check widget index in layer
	cAssert(Index >= 0 && Index < s_Layers[Layer].Count());
	if(Index < 0 || Index >= s_Layers[Layer].Count()) {
		return nullptr;
	}

	return s_Layers[Layer][Index];
} // cWidgets::GetWidget

//-----------------------------------------------------------------------------
// cWidgets::GetWidgetID : (const cWidget *)
//-----------------------------------------------------------------------------
int cWidgets::GetWidgetID(const cWidget *Widget) {
	cAssert(Widget != nullptr);
	if(nullptr == Widget) {
		return -1;
	}
	
	int Layer, Index;
	cWidget *W;

	for(Layer = 0; Layer <= TOP_LAYER; Layer++) {
		for(Index = 0; Index < s_Layers[Layer].Count(); Index++) {
			W = s_Layers[Layer][Index];
			if(Widget == W) {
				return Index + (Layer << LAYER_SHIFT);
			}
		}
	}

	return -1;
} // cWidgets::GetWidgetID : (const cWidget *)

// cWidgets::GetWidgetLayer
int cWidgets::GetWidgetLayer(const cWidget *Widget) {
	int ID;
	
	ID = GetWidgetID(Widget);
	if(ID != -1) {
		return ID >> LAYER_SHIFT;
	}
	return -1;
}

// cWidgets::FreeWidget : (const cWidget *)
void cWidgets::FreeWidget(const cWidget *Widget, const bool FreeObject) {
	const int ID = GetWidgetID(Widget);
	if(ID != -1) {
		FreeWidget(ID, FreeObject);
	}
}

//-----------------------------------------------------------------------------
// cWidgets::FreeWidget : (const int)
//-----------------------------------------------------------------------------
void cWidgets::FreeWidget(const int WidgetID, const bool FreeObject) {
	if(s_CriticalSection && FreeObject) {
		s_ToFree.Add(WidgetID);
		return;
	}

	const int Index = WidgetID & ((1 << LAYER_SHIFT) - 1);
	const int Layer = WidgetID >> LAYER_SHIFT;

	// Check layer index
	cAssert(Layer >= 0 && Layer <= TOP_LAYER);
	if(Layer < 0 || Layer > TOP_LAYER) {
		return;
	}

	// Check widget index in layer
	cAssert(Index >= 0 && Index < s_Layers[Layer].Count());
	if(Index < 0 || Index >= s_Layers[Layer].Count()) {
		return;
	}
	
	if(s_Layers[Layer][Index] != nullptr) {
		if(IsCaptured(s_Layers[Layer][Index])) { // Releasing widget with captured input 
			ReleaseCapture();
		}
		// Free widget object
		if(FreeObject) {
			delete s_Layers[Layer][Index];
		}
		s_Layers[Layer][Index] = nullptr;
		// Unregister widget in order list
		int OrderIndex = s_Order[Layer].IndexOf(Index);
		cAssert(OrderIndex != -1); // Existed widget should be registered
		if(OrderIndex != -1) {
			s_Order[Layer].RemoveAt(OrderIndex);
		}
		// Add free slot index
		s_FreeSlots[Layer].Add(Index);
	}
} // cWidgets::FreeWidget

//-----------------------------------------------------------------------------
// cWidgets::FreeLayer
//-----------------------------------------------------------------------------
void cWidgets::FreeLayer(const int Layer) {
	cAssert(Layer >= 0 && Layer <= TOP_LAYER);
	if(Layer < 0 || Layer > TOP_LAYER) {
		return;
	}
	
	for(int Index = 0; Index < s_Layers[Layer].Count(); Index++) {
		if(s_Layers[Layer][Index] != nullptr) {
			FreeWidget(Index + (Layer << LAYER_SHIFT));
		}
	}
	
	s_Order[Layer].Clear();
} // cWidgets::FreeLayer

//-----------------------------------------------------------------------------
// cWidgets::FreeDead
//-----------------------------------------------------------------------------
void cWidgets::FreeDead() {
	for(int Layer = 0; Layer <= TOP_LAYER; Layer++) {
		for(int Index = 0; Index < s_Layers[Layer].Count(); Index++) {
			if(s_Layers[Layer][Index] != nullptr) { // Skip "nullptr" pointers
				if(s_Layers[Layer][Index]->IsDead()) {
					// Free widget object
					delete s_Layers[Layer][Index];
					s_Layers[Layer][Index] = nullptr;
					// Unregister widget in order list
					int OrderIndex = s_Order[Layer].IndexOf(Index);
					cAssert(OrderIndex != -1); // Existed widget should be registered
					if(OrderIndex != -1) {
						s_Order[Layer].RemoveAt(OrderIndex);
					}
					// Add free slot index
					s_FreeSlots[Layer].Add(Index);
				}
			}
		}
	}
} // cWidgets::FreeDead

//-----------------------------------------------------------------------------
// cWidgets::FreeAll
//-----------------------------------------------------------------------------
void cWidgets::FreeAll() {
	for(int Layer = 0; Layer <= TOP_LAYER; Layer++) {
		FreeLayer(Layer);
	}
} // cWidgets::FreeAll

void cWidgets::BringToFront(const cWidget *Widget) {
	int ID = GetWidgetID(Widget);
	if(ID != -1) {
		BringToFront(ID);
	}
}

//-----------------------------------------------------------------------------
// cWidgets::BringToFront : (const int)
//-----------------------------------------------------------------------------
void cWidgets::BringToFront(const int WidgetID) {
	cAssert(WidgetID != -1);
	if(-1 == WidgetID) {
		return;
	}

	if(s_CriticalSection) {
		s_BringToFront.Add(WidgetID);
		return;
	}

	const int Index = WidgetID & ((1 << LAYER_SHIFT) - 1);
	const int Layer = WidgetID >> LAYER_SHIFT;

	// Check layer index
	cAssert(Layer >= 0 && Layer <= TOP_LAYER);
	if(Layer < 0 || Layer > TOP_LAYER) {
		return;
	}

	// Check widget index in layer
	cAssert(Index >= 0 && Index < s_Layers[Layer].Count());
	if(Index < 0 || Index >= s_Layers[Layer].Count()) {
		return;
	}

	// Find widget in order list
	int i, j = -1;
	for(i = 0; i < s_Order[Layer].Count(); i++) {
		if(s_Order[Layer][i] == Index) {
			j = i;
			break;
		}
	}
	if(j != -1) {
		s_Order[Layer].RemoveAt(j);
		s_Order[Layer].Add(Index);
	}
	
	cWidget *P = GetWidget(WidgetID);
	if(P != nullptr) {
		P->OnBringToFront();
	}
} // cWidgets::BringToFront

int cWidgets::m_idSendTo = -1;
int cWidgets::m_idCurProcessing = -1;

//-----------------------------------------------------------------------------
// cWidgets::SetCapture : ()
//-----------------------------------------------------------------------------
void cWidgets::SetCapture() {
	cAssertM(-1 == m_idSendTo, "SetCapture() - Input is already captured");
	if(m_idSendTo != -1) {
		return;
	}
	
	m_idSendTo = m_idCurProcessing;

#ifdef COMMS_WINDOWS
	cWinMain_SetCapture();
#endif // COMMS_WINDOWS
} // cWidgets::SetCapture : ()

// cWidgets::SetCapture: (const cWidget *)
void cWidgets::SetCapture(const cWidget *Widget) {
	SetCapture(GetWidgetID(Widget));
}

//-----------------------------------------------------------------------------
// cWidget::SetCapture : (const int)
//-----------------------------------------------------------------------------
void cWidgets::SetCapture(const int WidgetID) {
	cAssertM(-1 == m_idSendTo, "SetCapture() - Input is already captured");
	if(m_idSendTo != -1) {
		return;
	}
	
	m_idSendTo = WidgetID;
	
#ifdef COMMS_WINDOWS
	cWinMain_SetCapture();
#endif // COMMS_WINDOWS
} // cWidgets::SetCapture : (const int)

//-----------------------------------------------------------------------------
// cWidgets::ReleaseCapture
//-----------------------------------------------------------------------------
void cWidgets::ReleaseCapture() {
	cAssertM(m_idSendTo != -1, "ReleaseCapture() - Input is not captured");
	if(-1 == m_idSendTo) {
		return;
	}
	
	m_idSendTo = -1;
	
#ifdef COMMS_WINDOWS
	cWinMain_ReleaseCapture();
#endif // COMMS_WINDOWS
} // cWidgets::ReleaseCapture

// cWidgets::IsCaptured
bool cWidgets::IsCaptured() {
	return m_idSendTo != -1;
}

// cWidgets::IsCaptured
bool cWidgets::IsCaptured(const cWidget *Widget) {
	if(-1 == m_idSendTo) { // There is not capture at all
		return false;
	}
	return GetWidget(m_idSendTo) == Widget;
}

// cWidgets::IsCaptured
bool cWidgets::IsCaptured(const int WidgetID) {
	if(-1 == m_idSendTo) { // There is no capture
		return false;
	}
	return WidgetID == m_idSendTo;
}

//-----------------------------------------------------------------------------
// cWidgets::HandleInputEvents
//-----------------------------------------------------------------------------
void cWidgets::HandleInputEvents() {
	int Layer, i, Index;
	cWidget *Cur = nullptr, *Hot = nullptr, *SendTo = nullptr;
	bool Skip, Handled, Res;
	cInput::Cursor::Enum Cursor;

	FreeDead(); // No sense to handle dead widgets

	// "IsHot" handler
	Handled = false;
	SendTo = m_idSendTo != -1 ? GetWidget(m_idSendTo) : nullptr;
	if(SendTo != nullptr) { // There is widget with captured input. First we should call its handler.
		Handled = SendTo->IsHot(false);
		if(Handled) { // Widget with captured input wants to be hot.
			Hot = SendTo;
		}
	}
	for(Layer = TOP_LAYER; Layer >= 0; Layer--) {
		for(i = s_Order[Layer].Count() - 1; i >= 0; i--) {
			Index = s_Order[Layer][i];
			Cur = s_Layers[Layer][Index];
			cAssert(Cur != nullptr); // "s_Order" should not index "nullptr" slots
			if(nullptr == Cur) {
				continue;
			}
			if(Cur == SendTo) { // Skip widget with captured input
				continue;
			}
			if(Cur->IsHot(Handled) && !Handled) {
				Handled = true;
				Hot = Cur;
			}
		}
	}
	if(nullptr == Hot || !Hot->OnSetCursor(&Cursor)) { // There is no hot widget or it doesn't requires special cursor over
#ifndef COMMS_3DCOAT
		cInput::SetCursor(cInput::Cursor::Default);
#endif // !COMMS_3DCOAT
	} else {
		cInput::SetCursor(Cursor);
	}

	// Tool tip lag
	cStr ToolTip;
	float PrevLagSec = s_ToolTipLagSec;
	if(SendTo != nullptr) { // Block tool tip for widget with captured input
		s_ToolTipBlock = SendTo;
	}
	if(Hot != s_ToolTipBlock) {
		s_ToolTipBlock = nullptr; // Unblock tool tip for widget if there is another hot widget or no hot widget at all
	}
	if(nullptr == Hot || !Hot->OnSetToolTip(&ToolTip, &s_ToolTipTheme) || s_ToolTipBlock == Hot) {
		s_ToolTipLagSec += cTimer::GetFrameTimeSec();
	} else {
		s_ToolTipLagSec -= cTimer::GetFrameTimeSec();
	}
	s_ToolTipLagSec = cMath::Clamp(s_ToolTipLagSec, 0.0f, 1.0f);

	if(0.0f == s_ToolTipLagSec) {
		if(PrevLagSec != 0.0f) { // Tool tip is about to show
			s_ToolTipPos = cInput::GetMousePosition();
		}
		s_ToolTipStr = ToolTip;
	} else {
		s_ToolTipStr.Clear();
	}

	s_CriticalSection = true;
	
	while(cInputEvent *pInputEvent = cInput::GetEvent()) {
		SendTo = m_idSendTo != -1 ? GetWidget(m_idSendTo) : nullptr;
		cInputEvent_Touch *Touch = (cInputEventClass::Touch == pInputEvent->Class) ? (cInputEvent_Touch *)pInputEvent : nullptr;
		cInputEvent_Pen *Pen = (cInputEventClass::Pen == pInputEvent->Class) ? (cInputEvent_Pen *)pInputEvent : nullptr;
		cInputEvent_Gesture *Gesture = (cInputEventClass::Gesture == pInputEvent->Class) ? (cInputEvent_Gesture *)pInputEvent : nullptr;
		if ((Touch != nullptr) || (Pen != nullptr) || (Gesture != nullptr)) {
			for (Layer = TOP_LAYER; Layer >= 0; Layer--) {
				for (i = s_Order[Layer].Count() - 1; i >= 0; i--) {
					Index = s_Order[Layer][i];
					Cur = s_Layers[Layer][Index];
					cAssert(Cur != nullptr); // "s_Order" should not index "nullptr" slots
					if (nullptr == Cur) {
						continue;
					}
					m_idCurProcessing = Index + (Layer << LAYER_SHIFT);
					if (Touch != nullptr) {
						if (cInputEvent_Touch::TYPE::Down == Touch->Type) {
							Cur->OnTouchDown(Touch->TouchID, Touch->TouchPos);
						}
						else if (cInputEvent_Touch::TYPE::Move == Touch->Type) {
							Cur->OnTouchMove(Touch->TouchID, Touch->TouchPos, Touch->TouchDelta);
						}
						else if (cInputEvent_Touch::TYPE::Up == Touch->Type) {
							Cur->OnTouchUp(Touch->TouchID);
						}
						else {
							cAssert(0);
						}
					}
					else if (Pen != nullptr) {
						if (cInputEvent_Pen::TYPE::Down == Pen->Type) {
							Cur->OnPenDown(Pen->Eraser, Pen->PenPos, Pen->PenPressure);
						}
						else if (cInputEvent_Pen::TYPE::Move == Pen->Type) {
							Cur->OnPenMove(Pen->PenPos, Pen->PenDelta, Pen->PenPressure);
						}
						else if (cInputEvent_Pen::TYPE::Up == Pen->Type) {
							Cur->OnPenUp();
						}
						else {
							cAssert(0);
						}
					}
					else if (Gesture != nullptr) {
						if (cInputEvent_Gesture::STATE::Down == Gesture->GestureState) {
							Cur->OnGestureDown(Gesture->GestureType, Gesture->GestureCenter);
						}
						else if (cInputEvent_Gesture::STATE::Move == Gesture->GestureState) {
							Cur->OnGestureMove(Gesture->GestureCenter, Gesture->GestureTranslate, Gesture->GestureRotate, Gesture->GestureScale);
						}
						else if (cInputEvent_Gesture::STATE::Up == Gesture->GestureState) {
							Cur->OnGestureUp();
						}
						else {
							cAssert(0);
						}
					}
				}
			}
		}
		if (cInputEventClass::Old == pInputEvent->Class) {
			cInput::OldEvent *pEvent = (cInput::OldEvent *)pInputEvent;
			if (cInput::OldEvent::TYPE_BUTTON == pEvent->Type) {
				if (nullptr == SendTo || !(Res = pEvent->Pressed ? SendTo->OnButtonDown(pEvent->Code) : SendTo->OnButtonUp(pEvent->Code))) {
					cAssertM(-1 == m_idSendTo, "Widget should release capture if it refuses to handle event");
					// If there is no widget with captured input, or that widget refuses to handle event
					// (in which case that widget should release capture), we should send to all widgets.
					Skip = false;
					for (Layer = TOP_LAYER; Layer >= 0; Layer--) {
						for (i = s_Order[Layer].Count() - 1; i >= 0; i--) {
							Index = s_Order[Layer][i];
							Cur = s_Layers[Layer][Index];
							cAssert(Cur != nullptr); // "s_Order" should not index "nullptr" slots
							if (nullptr == Cur) {
								continue;
							}
							if (Cur == SendTo) { // Skip widget which had captured input
								continue;
							}
							m_idCurProcessing = Index + (Layer << LAYER_SHIFT);
							if (pEvent->Pressed ? Cur->OnButtonDown(pEvent->Code) : Cur->OnButtonUp(pEvent->Code)) {
								Skip = true;
								break;
							}
						}
						if (Skip) {
							break;
						}
					}
				}
			}
			else if (cInput::OldEvent::TYPE_MOUSEMOVE == pEvent->Type) {
				if (nullptr == SendTo || !SendTo->OnMouseMove()) {
					cAssertM(-1 == m_idSendTo, "Widget should release capture if it refuses to handle event");
					// If there is no widget with captured input, or that widget refuses to handle event
					// (in which case that widget should release capture), we should send to all widgets.
					Skip = false;
					for (Layer = TOP_LAYER; Layer >= 0; Layer--) {
						for (i = s_Order[Layer].Count() - 1; i >= 0; i--) {
							Index = s_Order[Layer][i];
							Cur = s_Layers[Layer][Index];
							cAssert(Cur != nullptr); // "s_Order" should not index "nullptr" slots
							if (nullptr == Cur) {
								continue;
							}
							if (Cur == SendTo) { // Skip widget which had captured input
								continue;
							}
							m_idCurProcessing = Index + (Layer << LAYER_SHIFT);
							if (Cur->OnMouseMove()) {
								Skip = true;
								break;
							}
						}
						if (Skip) {
							break;
						}
					}
				}
			}
			else if (cInput::OldEvent::TYPE_CHAR == pEvent->Type) {
				if (nullptr == SendTo || !SendTo->OnChar(pEvent->Char, pEvent->Code)) {
					cAssertM(-1 == m_idSendTo, "Widget should release capture if it refuses to handle event");
					// If there is no widget with captured input, or that widget refuses to handle event
					// (in which case that widget should release capture), we should send to all widgets.
					Skip = false;
					for (Layer = TOP_LAYER; Layer >= 0; Layer--) {
						for (i = s_Order[Layer].Count() - 1; i >= 0; i--) {
							Index = s_Order[Layer][i];
							Cur = s_Layers[Layer][Index];
							cAssert(Cur != nullptr); // "s_Order" should not index "nullptr" slots
							if (nullptr == Cur) {
								continue;
							}
							if (Cur == SendTo) { // Skip widget which had captured input
								continue;
							}
							m_idCurProcessing = Index + (Layer << LAYER_SHIFT);
							if (Cur->OnChar(pEvent->Char, pEvent->Code)) {
								Skip = true;
								break;
							}
						}
						if (Skip) {
							break;
						}
					}
				}
			}
		}
		delete pInputEvent;
	}

	s_CriticalSection = false;

	ApplyWithinCriticalSectionQueries();
} // cWidgets::HandleInputEvents

//-----------------------------------------------------------------------------
// cWidgets::ApplyWithinCriticalSectionQueries
//-----------------------------------------------------------------------------
void cWidgets::ApplyWithinCriticalSectionQueries() {
	cAssert(!s_CriticalSection);

	int i;

	for(i = 0; i < s_ToFree.Count(); i++) {
		FreeWidget(s_ToFree[i]);
	}

	for(i = 0; i < s_BringToFront.Count(); i++) {
		BringToFront(s_BringToFront[i]);
	}

	s_ToFree.Clear();
	s_BringToFront.Clear();
} // cWidgets::ApplyWithinCriticalSectionQueries

//-----------------------------------------------------------------------------
// cWidgets::Render
//-----------------------------------------------------------------------------
void cWidgets::Render(const int Layer) {
	// It seems, like deleting widget or bringing it to front within "OnRender"
	// handler is stupid, but for sure let's consider such case.
	s_CriticalSection = true;

	for(int l = 0; l <= TOP_LAYER; l++) {
		if(l == Layer || Layer == -1) {
			for(int i = 0; i < s_Order[l].Count(); i++) {
				const int Index = s_Order[l][i];
				cWidget *w = s_Layers[l][Index];
				cAssert(w != nullptr); // "s_Order" should not index "nullptr" slots
				if(nullptr == w) {
					continue;
				}
				w->OnRender();
			}
		}
	}

	s_CriticalSection = false;
	ApplyWithinCriticalSectionQueries();
} // cWidgets::Render

} // comms
