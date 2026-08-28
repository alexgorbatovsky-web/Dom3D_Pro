#include "../comms/comms.h"
#include "cTools.h"

namespace comms {

//*****************************************************************************
// CMoveTool
//*****************************************************************************

//-----------------------------------------------------------------------------
// CMoveTool::Init
//-----------------------------------------------------------------------------
void CMoveTool::Init() {
	m_Mode = Mode::None;
	
	// Size:
	m_AxisLen = 150.0f;//82.0
	m_MaxSelDistance = 0.10f;//0.12

	// Axis Cull:
	m_MaxVisibleDot = 0.98f;
	
	m_Rotate = NULL;
	m_Translate = NULL;
	m_Space = m_DesiredSpace = cSpace::World;

	m_Parent.SetIdentity();
	m_ParentInverse.SetIdentity();

	m_UniversalMode = false;
} // CMoveTool::Init

//-----------------------------------------------------------------------------
// CMoveTool.ctor
//-----------------------------------------------------------------------------
CMoveTool::CMoveTool() {
	Init();
} // CMoveTool.ctor

// CMoveTool.ctor : (cVec3 *)
CMoveTool::CMoveTool(cVec3 *Translate) {
	Init();
	Bind(Translate);
}

// CMoveTool.ctor : (cAngles *, cVec3 *, const cSpace::Enum)
CMoveTool::CMoveTool(cAngles *Rotate, cVec3 *Translate, const cSpace::Enum Space) {
	Init();
	Bind(Rotate, Translate, Space);
}

// CMoveTool::Bind : (cVec3 *)
void CMoveTool::Bind(cVec3 *Translate) {
	m_Rotate = NULL;
	m_Translate = Translate;
	m_Space = m_DesiredSpace = cSpace::World;
}

//-----------------------------------------------------------------------------
// CMoveTool::SetSpace
//-----------------------------------------------------------------------------
void CMoveTool::SetSpace(const cSpace::Enum Space) {
	if(NULL == m_Rotate) {
		cAssert(cSpace::World == m_Space);
		cAssert(cSpace::World == m_DesiredSpace);
		return;
	}
	m_DesiredSpace = Space;
	if(Mode::None == m_Mode) {
		m_Space = m_DesiredSpace;
	}
} // CMoveTool::SetSpace

//-------------------------------------------------------------------------------
// CMoveTool::Bind
//-------------------------------------------------------------------------------
void CMoveTool::Bind(cAngles *Rotate, cVec3 *Translate, const cSpace::Enum Space) {
	Bind(Translate);
	m_Rotate = Rotate;
	SetSpace(Space);
} // CMoveTool::Bind

// CMoveTool::SetParent
void CMoveTool::SetParent(const cMat4 &Parent) {
	m_Parent = Parent;
	if(!cMat4::Invert(Parent, &m_ParentInverse)) {
		m_ParentInverse.SetIdentity();
	}
}

// CMoveTool::SetParent
void CMoveTool::SetParent(const cMat4 &Parent, const cMat4 &ParentInverse) {
	m_Parent = Parent;
	m_ParentInverse = ParentInverse;
}

//-----------------------------------------------------------------------------
// CMoveTool::CalcFrame
//-----------------------------------------------------------------------------
bool CMoveTool::CalcFrame() {
	if(NULL == m_Translate) {
		return false;
	}
	// CurPos:
	m_Frame.CurPos = cVec3::TransformCoordinate(*m_Translate, m_Parent);
	
	// AxisLen:
	cVec3 u = cVec3::TransformCoordinate(m_Frame.CurPos, cRender::GetViewer()->GetViewProjectionScreenMatrix());
	u.x += m_AxisLen;
	u.TransformCoordinate(cRender::GetViewer()->GetViewProjectionScreenMatrixInverse());
	u -= m_Frame.CurPos;
	const float AxisLen = u.Length();

	// Axis (0..2):
	cVec3 AxisNormal;
	int iAxis;
	for(iAxis = 0; iAxis < 3; iAxis++) {
		AxisNormal.SetZero();
		AxisNormal[iAxis] = 1.0f;
		if(cSpace::Object == m_Space) {
			cAssert(m_Rotate != NULL);
			AxisNormal.Rotate(m_Rotate->ToQuat());
		}
		AxisNormal.TransformNormal(m_Parent);
		m_Frame.Axis[iAxis].SetFromEnds(m_Frame.CurPos, m_Frame.CurPos + AxisNormal * AxisLen);
		if(m_Frame.Axis[iAxis].IsZero()) {
			return false;
		}
	}

	m_Frame.MaxSelDistance = m_MaxSelDistance * AxisLen;

	// Update segs visibility only in "None" mode:
	cVec3 ToEye;
	if(Mode::None == m_Mode) {
		if(cRender::GetViewer()->GetOrthoProj()) {
			ToEye = -cRender::GetViewer()->GetForward();
		} else {
			ToEye = (cRender::GetViewer()->GetPos() - m_Frame.CurPos).ToNormal();
		}
		for(int iAxis = 0; iAxis < 3; iAxis++) {
			const float d = cMath::Abs(cVec3::Dot(ToEye, m_Frame.Axis[iAxis].GetNormal()));
			m_Frame.AxisIsVisible[iAxis] = d <= m_MaxVisibleDot;
		}
	}
	return true;
} // CMoveTool::CalcFrame

//-----------------------------------------------------------------------------
// CMoveTool::OnRender
//-----------------------------------------------------------------------------
void CMoveTool::OnRender() {
	if(!CalcFrame()) { // Tool is not binded.
		return;
	}
	
	// Colors:
	const cColor TraceColor = cColor::Gray;
	const cColor SelColor = cColor::Yellow;
	const cColor CenterColor = Mode::Center == m_Mode ? SelColor : cColor::White;
	const cColor AxisColor[3] = {
		Mode::AlongX == m_Mode ? SelColor : cColor(1.0f, 0.0f, 0.0f),
		Mode::AlongY == m_Mode ? SelColor : cColor(0.0f, 1.0f, 0.0f),
		Mode::AlongZ == m_Mode ? SelColor : cColor(0.0f, 0.0f, 1.0f)
	};

	// Appearance:
	const float HeadLenFr = 0.15;
	const float CurHeadLen = HeadLenFr * m_Frame.Axis[0].GetLength();
	const float HeadRadius = 0.04f * m_Frame.Axis[0].GetLength();
	const int CenterSubDivs = 32;
	const int ArrowSubDivs = 12;
	const float LabelShift = 0.2f * m_Frame.Axis[0].GetLength();
	
	const bool ShowAxis[3] = {
		m_Frame.AxisIsVisible[0] && m_Mode != Mode::AlongY && m_Mode != Mode::AlongZ,
		m_Frame.AxisIsVisible[1] && m_Mode != Mode::AlongX && m_Mode != Mode::AlongZ,
		m_Frame.AxisIsVisible[2] && m_Mode != Mode::AlongX && m_Mode != Mode::AlongY
	};
	const cVec3 LineEnds[3] = {
		ShowAxis[0] ? m_Frame.Axis[0].GetTo() - CurHeadLen * m_Frame.Axis[0].GetNormal() : cVec3::Zero,
		ShowAxis[1] ? m_Frame.Axis[1].GetTo() - CurHeadLen * m_Frame.Axis[1].GetNormal() : cVec3::Zero,
		ShowAxis[2] ? m_Frame.Axis[2].GetTo() - CurHeadLen * m_Frame.Axis[2].GetNormal() : cVec3::Zero
	};
	
	// Drawing Traces in any move mode (only in move mode StartAxis[0..2] are valid):
	if(m_Mode != Mode::None) {
		const float TraceQuadSide = 0.1f * m_Frame.StartAxis[0].GetLength();
		const float StartHeadLen = HeadLenFr * m_Frame.StartAxis[0].GetLength();
		const cVec3 StartLineEnds[3] = {
			ShowAxis[0] ? m_Frame.StartAxis[0].GetTo() - StartHeadLen * m_Frame.StartAxis[0].GetNormal() : cVec3::Zero,
			ShowAxis[1] ? m_Frame.StartAxis[1].GetTo() - StartHeadLen * m_Frame.StartAxis[1].GetNormal() : cVec3::Zero,
			ShowAxis[2] ? m_Frame.StartAxis[2].GetTo() - StartHeadLen * m_Frame.StartAxis[2].GetNormal() : cVec3::Zero
		};
		
		for(int iAxis = 0; iAxis < 3; iAxis++) {
			if(!ShowAxis[iAxis]) {
				continue;
			}
			// Line Trace:
			cRender::DrawLine(m_Frame.StartPos, StartLineEnds[iAxis], TraceColor);
			// Quad Trace:
			cRender::DrawBillboardQuadSolid(StartLineEnds[iAxis], TraceQuadSide, TraceColor);
		}
		// Center Trace:
		cRender::DrawBillboardQuadSolid(m_Frame.StartPos, TraceQuadSide, TraceColor);
	}

	// Drawing Axes:
	for(int iAxis = 0; iAxis < 3; iAxis++) {
		if(!ShowAxis[iAxis]) {
			continue;
		}
		// Line:
		cRender::DrawLine(m_Frame.CurPos, LineEnds[iAxis], AxisColor[iAxis]);
		// Arrow:
		if(m_Mode != Mode::Center) {
			cRender::DrawConeSolid(m_Frame.Axis[iAxis].GetTo(), m_Frame.Axis[iAxis].GetNormal(), HeadRadius, CurHeadLen, AxisColor[iAxis], ArrowSubDivs);
		}
	}

	// Center:
	cRender::DrawCircle(m_Frame.CurPos, m_Frame.MaxSelDistance * (m_UniversalMode ? 2.0f : 1.0f), CenterColor);

	// Drawing labels in any move mode:
	if(m_Mode != Mode::None) {
		const cVec3 Shift = m_Frame.CurPos - m_Frame.StartPos;
		for(int iAxis = 0; iAxis < 3; iAxis++) {
			if(!ShowAxis[iAxis]) {
				continue;
			}
			cStr Label;
			Label << Shift[iAxis];
			const cVec3 LabelPos = m_Frame.Axis[iAxis].GetTo() + cRender::GetViewer()->GetUp() * LabelShift;
			static int FontID = cRender::GetFontID("Fira Sans Condensed 10");
			cRender::DrawString(FontID, Label, LabelPos, cRender::StringAlign::MiddleCenter, cColor::Yellow);
		}
	}
} // CMoveTool::OnRender

//-----------------------------------------------------------------------------
// CMoveTool::OnButtonDown
//-----------------------------------------------------------------------------
bool CMoveTool::OnButtonDown(const int Code) {
	if(cWidgets::IsCaptured(this)) {
		return true;
	}
	if(Code != cInput::LeftButton) {
		return false;
	}
	if(!CalcFrame()) { // Tool is not binded.
		return false;
	}

	cAssert(Mode::None == m_Mode);
	
	const cSeg PickRay = cRender::GetViewer()->GetPickRay(cInput::GetMousePosition());

	cVec3 PickedPoint[4], q;
	float D[4]; // 0..2 - distance from pick ray to axis seg, 3 - distance from pick ray to center along view plane.
	int iInvisibleAxis = -1;
	for(int iAxis = 0; iAxis < 3; iAxis++) {
		if(!m_Frame.AxisIsVisible[iAxis]) {
			D[iAxis] = cMath::FloatMaxValue;
			iInvisibleAxis = iAxis;
			continue;
		}
		cSeg::ClosestPoints(cSeg::SegRay, m_Frame.Axis[iAxis], PickRay, PickedPoint[iAxis], q);
		D[iAxis] = cVec3::Distance(PickedPoint[iAxis], q);
	}
	const int iMin = cMath::MinIndex(D[0], D[1], D[2]);
	
	// Center plane (normal based on axis visibility):
	const cVec3 CenterNormal = iInvisibleAxis != -1 ? m_Frame.Axis[iInvisibleAxis].GetNormal() : cRender::GetViewer()->GetForward();
	m_Frame.Center.SetFromPointAndNormal(m_Frame.CurPos, CenterNormal);
	if(m_Frame.Center.RayIntersection(PickRay.GetFm(), PickRay.GetNormal(), NULL, &PickedPoint[3])) {
		D[3] = cSeg::Distance(cSeg::Ray, PickRay, m_Frame.CurPos);
	} else {
		D[3] = cMath::FloatMaxValue;
	}

	if((!m_UniversalMode && D[3] < m_Frame.MaxSelDistance) || (m_UniversalMode && D[3] > m_Frame.MaxSelDistance && D[3] < 2.0f * m_Frame.MaxSelDistance)) { // Center has highest priority.
		m_Frame.PickedPoint = PickedPoint[3];
		m_Mode = Mode::Center;
	} else if(D[iMin] < m_Frame.MaxSelDistance) {
		m_Frame.PickedPoint = PickedPoint[iMin];
		m_Mode = (Mode::Enum)(Mode::AlongX + iMin);
	}
	
	if(m_Mode != Mode::None) {
		m_Frame.StartAxis[0] = m_Frame.Axis[0];
		m_Frame.StartAxis[1] = m_Frame.Axis[1];
		m_Frame.StartAxis[2] = m_Frame.Axis[2];
		m_Frame.StartPos = m_Frame.CurPos;
	}
	
	if(Mode::None == m_Mode) {
		return false;
	}
	// Capturing input in any move mode:
	cWidgets::SetCapture();
	return true;
} // CMoveTool::OnButtonDown

//-----------------------------------------------------------------------------
// CMoveTool::OnButtonUp
//-----------------------------------------------------------------------------
bool CMoveTool::OnButtonUp(const int Code) {
	if(!cWidgets::IsCaptured(this)) {
		return false;
	}
	cAssert(m_Mode != Mode::None);
	if(Code != cInput::LeftButton) {
		return true;
	}
	// Releasing capturing from any move mode:
	m_Mode = Mode::None;
	if(m_Space != m_DesiredSpace) {
		m_Space = m_DesiredSpace;
	}
	cWidgets::ReleaseCapture();
	return true;
} // CMoveTool::OnButtonUp

//-----------------------------------------------------------------------------
// CMoveTool::OnMouseMove
//-----------------------------------------------------------------------------
bool CMoveTool::OnMouseMove() {
	if(!cWidgets::IsCaptured(this)) {
		return false;
	}
	cAssert(m_Mode != Mode::None);
	
	const cSeg PickRay = cRender::GetViewer()->GetPickRay(cInput::GetMousePosition());

	cVec3 CurPickedPoint = m_Frame.PickedPoint, q;
	if(Mode::Center == m_Mode) {
		m_Frame.Center.RayIntersection(PickRay.GetFm(), PickRay.GetNormal(), NULL, &CurPickedPoint);
	} else {
		const int iAxis = m_Mode - Mode::AlongX;
		cAssert(iAxis >= 0 && iAxis <= 2);
		cSeg::ClosestPoints(cSeg::LineRay, m_Frame.StartAxis[iAxis], PickRay, CurPickedPoint, q);
	}
	m_Frame.CurPos = cVec3::TransformCoordinate(m_Frame.StartPos + CurPickedPoint - m_Frame.PickedPoint, m_ParentInverse);

	*m_Translate = m_Frame.CurPos;
	return true;
} // CMoveTool::OnMouseMove

// CMoveTool::IsHot
bool CMoveTool::IsHot(const bool Handled) {
	return !Handled && cWidgets::IsCaptured(this);
}

//*****************************************************************************
// CScaleTool
//*****************************************************************************

// CScaleTool::SetParent
void CScaleTool::SetParent(const cMat4 &Parent) {
	m_Parent = Parent;
}

//-----------------------------------------------------------------------------
// CScaleTool::Init
//-----------------------------------------------------------------------------
void CScaleTool::Init() {
	m_Mode = Mode::None;
	
	// Size:
	m_LineLen = 110.0f;
	m_MaxSelDistance = 0.12f;

	// Axis cull:
	m_MaxVisibleDot = 0.98f;

	m_Scale = NULL;
	m_Rotate = NULL;
	m_Translate = NULL;

	m_Parent.SetIdentity();

	m_UniversalMode = false;
} // CScaleTool::Init


// CScaleTool.ctor : ()
CScaleTool::CScaleTool() {
	Init();
}

// CScaleTool.ctor : (cVec3 *, cAngles *, cVec3 *)
CScaleTool::CScaleTool(cVec3 *Scale, cAngles *Rotate, cVec3 *Translate) {
	Init();
	Bind(Scale, Rotate, Translate);
}

//-----------------------------------------------------------------------------
// CScaleTool::Bind
//-----------------------------------------------------------------------------
void CScaleTool::Bind(cVec3 *Scale, cAngles *Rotate, cVec3 *Translate) {
	if(NULL == Scale || NULL == Rotate || NULL == Translate) {
		m_Scale = NULL;
		m_Rotate = NULL;
		m_Translate = NULL;
	} else {
		m_Scale = Scale;
		m_Rotate = Rotate;
		m_Translate = Translate;
	}
} // CScaleTool::Bind

//-----------------------------------------------------------------------------
// CScaleTool::CalcFrame
//-----------------------------------------------------------------------------
bool CScaleTool::CalcFrame() {
	if(NULL == m_Scale || NULL == m_Rotate || NULL == m_Translate) {
		return false;
	}
	
	// HandlePos[3] (Center)
	m_Frame.HandlePos[3] = cVec3::TransformCoordinate(*m_Translate, m_Parent);
	
	// LineLen:
	cVec3 u = cVec3::TransformCoordinate(m_Frame.HandlePos[3], cRender::GetViewer()->GetViewProjectionScreenMatrix());
	u.x += m_LineLen;
	u.TransformCoordinate(cRender::GetViewer()->GetViewProjectionScreenMatrixInverse());
	u -= m_Frame.HandlePos[3];
	m_Frame.LineLen = u.Length();
	
	// AxisNormal && HandlePos[0 - 2]:
	const cMat3 R = m_Rotate->ToMat3();
	int iAxis;
	for(iAxis = 0; iAxis < 3; iAxis++) {
		float Sign = cMath::Sign((*m_Scale)[iAxis]);
		if(Sign == 0.0f) {
			Sign = 1.0f;
		}
		m_Frame.AxisNormal[iAxis] = cVec3::TransformNormal(Sign * R.GetRow(iAxis), m_Parent).ToNormal();
		m_Frame.HandlePos[iAxis] = m_Frame.HandlePos[3] + m_Frame.AxisNormal[iAxis] * m_Frame.LineLen;
	}
	m_Frame.AxisNormal[3] = cRender::GetViewer()->GetRight();

	m_Frame.MaxSelDistance = m_MaxSelDistance * m_Frame.LineLen;
	
	m_Frame.PickRay = cRender::GetViewer()->GetPickRay(cInput::GetMousePosition());

	// Update axes visibility:
	cVec3 ToEye;
	if(cRender::GetViewer()->GetOrthoProj()) {
		ToEye = -cRender::GetViewer()->GetForward();
	} else {
		ToEye = (cRender::GetViewer()->GetPos() - m_Frame.HandlePos[3]).ToNormal();
	}
	for(iAxis = 0; iAxis < 3; iAxis++) {
		const float d = cMath::Abs(cVec3::Dot(ToEye, m_Frame.AxisNormal[iAxis]));
		m_Frame.AxisIsVisible[iAxis] = d <= m_MaxVisibleDot;
		m_Frame.AxisIsVisible[3] = true; // Central handle is always visible
	}
	
	return true;
} // CScaleTool::CalcFrame

//-----------------------------------------------------------------------------
// CScaleTool::OnRender
//-----------------------------------------------------------------------------
void CScaleTool::OnRender() {
	if(NULL == m_Scale || NULL == m_Rotate || NULL == m_Translate) { // Tool is not binded.
		return;
	}

	if(Mode::None == m_Mode) { // Update frame in non - scale mode.
		CalcFrame();
	}

	// Colors:
	const cColor SelColor = cColor::Yellow;
	const cColor TraceColor = cColor::Gray;
	// Appearance:
	const float LineWidth = 2.0f;
	const float TraceQuadSide = 0.096f * m_Frame.LineLen;
	const float HandleRadius = 0.06f * m_Frame.LineLen;
	const float LabelShift = 0.2f * m_Frame.LineLen;
	
	const bool ShowAxis[3] = {
		m_Frame.AxisIsVisible[0] && m_Mode != Mode::AlongY && m_Mode != Mode::AlongZ,
		m_Frame.AxisIsVisible[1] && m_Mode != Mode::AlongX && m_Mode != Mode::AlongZ,
		m_Frame.AxisIsVisible[2] && m_Mode != Mode::AlongX && m_Mode != Mode::AlongY
	};
	const cVec3 CurEnds[3] = {
		Mode::None == m_Mode ? m_Frame.HandlePos[0] : m_Frame.HandlePos[3] + m_Frame.AxisNormal[0] * m_Frame.LineLen * m_Frame.RelScale[0],
		Mode::None == m_Mode ? m_Frame.HandlePos[1] : m_Frame.HandlePos[3] + m_Frame.AxisNormal[1] * m_Frame.LineLen * m_Frame.RelScale[1],
		Mode::None == m_Mode ? m_Frame.HandlePos[2] : m_Frame.HandlePos[3] + m_Frame.AxisNormal[2] * m_Frame.LineLen * m_Frame.RelScale[2],
	};

	// Drawing Traces in any scale mode:
	if(m_Mode != Mode::None) {
		for(int iAxis = 0; iAxis < 3; iAxis++) {
			if(!ShowAxis[iAxis]) {
				continue;
			}
			// Line Trace:
			cRender::DrawLine(m_Frame.HandlePos[3], m_Frame.HandlePos[iAxis], TraceColor);
			// Quad Trace:
			cRender::DrawBillboardQuadSolid(m_Frame.HandlePos[iAxis], TraceQuadSide, TraceColor);
		}
	}
	
	// Drawing Axes:
	const cColor AxisColor[3] = {
		Mode::AlongX == m_Mode ? SelColor : cColor(1.0f, 0.0f, 0.0f),
		Mode::AlongY == m_Mode ? SelColor : cColor(0.0f, 1.0f, 0.0f),
		Mode::AlongZ == m_Mode ? SelColor : cColor(0.0f, 0.0f, 1.0f)
	};
	for(int iAxis = 0; iAxis < 3; iAxis++) {
		if(!ShowAxis[iAxis]) {
			continue;
		}
		// Line:
		const cVec3 End = CurEnds[iAxis];
		//if(!m_UniversalMode) {
			cRender::DrawLine(m_Frame.HandlePos[3], End, cColor::DimGray);
		//}
		// Handle:
		cRender::DrawCubeSolid(End, 2.0f * HandleRadius, AxisColor[iAxis], m_Rotate->ToQuat());
	}

	// Center:
	const cColor CenterColor = Mode::Uniform == m_Mode ? SelColor : cColor::White;
	cRender::DrawCubeSolid(m_Frame.HandlePos[3], 2.0f * HandleRadius, CenterColor, m_Rotate->ToQuat());

	// Drawing labels in any scale mode:
	if(m_Mode != Mode::None) {
		cStr Label;
        cVec3 LabelPos;
		const int iCurAxis = m_Mode - Mode::AlongX;
		cAssert(iCurAxis >= 0 && iCurAxis <= 3);
		if(Mode::Uniform == m_Mode) {
			LabelPos = m_Frame.HandlePos[3] + (cRender::GetViewer()->GetRight() + cRender::GetViewer()->GetUp()) * LabelShift;
		} else {
			LabelPos = CurEnds[iCurAxis] + cRender::GetViewer()->GetUp() * LabelShift;
		}
		Label << m_Frame.RelScale[iCurAxis % 3];
		static int FontID = cRender::GetFontID("Fira Sans Condensed 10");
		cRender::DrawString(FontID, Label, LabelPos, cRender::StringAlign::MiddleCenter, cColor::Yellow);
	}
} // CScaleTool::OnRender

//-----------------------------------------------------------------------------
// CScaleTool::OnButtonDown
//-----------------------------------------------------------------------------
bool CScaleTool::OnButtonDown(const int Code) {
	if(cWidgets::IsCaptured(this)) {
		return true;
	}
	if(Code != cInput::LeftButton) {
		return false;
	}
	if(!CalcFrame()) { // Tool is not binded.
		return false;
	}

	cAssert(Mode::None == m_Mode);
	
	float D[4];	// Distance from handle to pick ray
	for(int iAxis = 0; iAxis < 4; iAxis++) {
		D[iAxis] = m_Frame.AxisIsVisible[iAxis] ? cSeg::Distance(cSeg::Ray, m_Frame.PickRay, m_Frame.HandlePos[iAxis]) : cMath::FloatMaxValue;
	}
	const int iMin = cMath::MinIndex(D[0], D[1], D[2], D[3]);
	if(D[iMin] < m_Frame.MaxSelDistance) {
		const cSeg S(cSeg::RayCtor, m_Frame.HandlePos[3], m_Frame.AxisNormal[iMin]);
		cVec3 q;
		cSeg::ClosestPoints(cSeg::LineRay, S, m_Frame.PickRay, m_Frame.PickedPoint, q);
		
		m_Frame.AbsScale = *m_Scale;
		m_Frame.RelScale.Set(1.0f);
		m_Mode = (Mode::Enum)(Mode::AlongX + iMin);
	}

	if(Mode::None == m_Mode) {
		return false;
	}
	
	// Set capture in any scale mode:
	cWidgets::SetCapture();
	return true;
} // CScaleTool::OnButtonDown

//-----------------------------------------------------------------------------
// CScaleTool::OnButtonUp
//-----------------------------------------------------------------------------
bool CScaleTool::OnButtonUp(const int Code) {
	if(!cWidgets::IsCaptured(this)) {
		return false;
	}
	cAssert(m_Mode != Mode::None);
	if(Code != cInput::LeftButton) {
		return true;
	}
	// Releasing capture from any scale mode:
	m_Mode = Mode::None;
	cWidgets::ReleaseCapture();
	return true;
} // CScaleTool::OnButtonUp

//-----------------------------------------------------------------------------
// CScaleTool::OnMouseMove
//-----------------------------------------------------------------------------
bool CScaleTool::OnMouseMove() {
	if(!cWidgets::IsCaptured(this)) {
		return false;
	}
	cAssert(m_Mode != Mode::None);
	
	const cSeg PickRay = cRender::GetViewer()->GetPickRay(cInput::GetMousePosition());
	
	cVec4 RelScale(1.0f);
	const int iAxis = m_Mode - Mode::AlongX;
	cAssert(iAxis >= 0 && iAxis < 4);
	const cSeg S(cSeg::RayCtor, m_Frame.HandlePos[iAxis], m_Frame.AxisNormal[iAxis]);
	cVec3 p, q;
	cSeg::ClosestPoints(cSeg::LineRay, S, PickRay, p, q);
	const cVec3 Shift = p - m_Frame.PickedPoint;
	const float Sign = cMath::Sign(cVec3::Dot(Shift, m_Frame.AxisNormal[iAxis]));
	RelScale[iAxis] = 1.0f + Sign * Shift.Length() / m_Frame.LineLen;
	
	if(Mode::Uniform == m_Mode) {
		for(int i = 0; i < 3; i++) {
			RelScale[i] = m_Frame.AxisIsVisible[i] ? RelScale.w : RelScale[i];
		}
	}
	m_Frame.RelScale = RelScale.ToVec3();

	*m_Scale = m_Frame.AbsScale * m_Frame.RelScale;
	return true;
} // CScaleTool::OnMouseMove

// CScaleTool::IsHot
bool CScaleTool::IsHot(const bool Handled) {
	return !Handled && cWidgets::IsCaptured(this);
}

//*****************************************************************************
// CRotateTool
//*****************************************************************************

// CRotateTool::SetParent
void CRotateTool::SetParent(const cMat4 &Parent) {
	m_Parent = Parent;
	if(!cMat4::Invert(m_Parent, &m_ParentInverse)) {
		m_ParentInverse.SetIdentity();
	}
}

// CRotateTool::SetParent
void CRotateTool::SetParent(const cMat4 &Parent, const cMat4 &ParentInverse) {
	m_Parent = Parent;
	m_ParentInverse = ParentInverse;
}

//-----------------------------------------------------------------------------
// CRotateTool::Init
//-----------------------------------------------------------------------------
void CRotateTool::Init() {
	// Size:
	m_OuterRadius = 85.0f;
	m_InnerRadius = 0.86f;
	m_MaxSelDistance = 0.08f;
	
	m_Rotate = NULL;
	m_Translate = NULL;
	
	m_Mode = Mode::None;
	m_Space = m_DesiredSpace = cSpace::World;

	m_Parent.SetIdentity();
	m_ParentInverse.SetIdentity();

	m_UniversalMode = false;
} // CRotateTool::Init

// CRotateTool.ctor : ()
CRotateTool::CRotateTool() {
	Init();
}

// CRotateTool.ctor : (cAngles *, ...)
CRotateTool::CRotateTool(cAngles *Rotate, cVec3 *Translate, const cSpace::Enum Space) {
	Init();
	Bind(Rotate, Translate, Space);
}

//-----------------------------------------------------------------------------------
// CRotateTool::Bind
//-----------------------------------------------------------------------------------
void CRotateTool::Bind(cAngles *Rotate, cVec3 *Translate, const cSpace::Enum Space) {
	if(NULL == Rotate || NULL == Translate) {
		m_Rotate = NULL;
		m_Translate = NULL;
	} else {
		m_Rotate = Rotate;
		m_Translate = Translate;
	}
	m_Space = m_DesiredSpace = Space;
} // CRotateTool::Bind

//-----------------------------------------------------------------------------
// CRotateTool::SetSpace
//-----------------------------------------------------------------------------
void CRotateTool::SetSpace(const cSpace::Enum Space) {
	if(m_DesiredSpace != Space) {
		m_DesiredSpace = Space;
	}
	if(Mode::None == m_Mode) {
		m_Space = m_DesiredSpace;
	}
} // CRotateTool::SetSpace

//-----------------------------------------------------------------------------
// CRotateTool::CalcFrame
//-----------------------------------------------------------------------------
bool CRotateTool::CalcFrame() {
	if(NULL == m_Rotate || NULL == m_Translate) {
		return false;
	}
	
	m_Frame.Pos = cVec3::TransformCoordinate(*m_Translate, m_Parent);
	
	// AxisNormal[0..3]:
	if(cSpace::World == m_Space) {
		m_Frame.AxisNormal[0] = m_Parent.GetRow0().ToVec3().ToNormal();
		m_Frame.AxisNormal[1] = m_Parent.GetRow1().ToVec3().ToNormal();
		m_Frame.AxisNormal[2] = m_Parent.GetRow2().ToVec3().ToNormal();
	} else if(cSpace::Object == m_Space) {
		const cMat3 R = m_Rotate->ToMat3();
		for(int iAxis = 0; iAxis < 3; iAxis++) {
			m_Frame.AxisNormal[iAxis] = cVec3::TransformNormal(R.GetRow(iAxis), m_Parent).ToNormal();
		}
	} else {
		cAssertM(0, "Illegal Space");
	}
	m_Frame.AxisNormal[3] = cRender::GetViewer()->GetForward();

	// Size:
	cVec3 u = cVec3::TransformCoordinate(m_Frame.Pos, cRender::GetViewer()->GetViewProjectionScreenMatrix());
	u.x += m_OuterRadius;
	u.TransformCoordinate(cRender::GetViewer()->GetViewProjectionScreenMatrixInverse());
	u -= m_Frame.Pos;
	m_Frame.OuterRadius = u.Length();
	m_Frame.InnerRadius = m_Frame.OuterRadius * m_InnerRadius;
	m_Frame.MaxSelDistance = m_Frame.OuterRadius * m_MaxSelDistance;

	return true;
} // CRotateTool::CalcFrame

//-----------------------------------------------------------------------------
// CRotateTool::OnButtonDown
//-----------------------------------------------------------------------------
bool CRotateTool::OnButtonDown(const int Code) {
	if(cWidgets::IsCaptured(this)) {
		return true;
	}
	if(Code != cInput::LeftButton) {
		return false;
	}
	if(!CalcFrame()) {
		return false;
	}

	const cSeg PickRay = cRender::GetViewer()->GetPickRay(cInput::GetMousePosition());

	const cPlane P(m_Frame.Pos, m_Frame.AxisNormal[3]);
	cVec3 Around;
	const bool f = P.RayIntersection(PickRay.GetFm(), PickRay.GetNormal(), NULL, &Around);
	cAssert(f);
	const float dToCenter = cVec3::Distance(m_Frame.Pos, Around);
	if(cMath::Equals(dToCenter, m_Frame.OuterRadius, m_Frame.MaxSelDistance)) {
		m_Mode = Mode::AroundView;
		m_Frame.StartNormal = (Around - m_Frame.Pos).ToNormal();
		m_Frame.RotationAxis = m_Frame.AxisNormal[3];
	} else {
		cSphere InnerSphere(m_Frame.Pos, m_Frame.InnerRadius);
		float Scale1, Scale2;
		if(InnerSphere.RayIntersection(PickRay.GetFm(), PickRay.GetNormal(), Scale1, Scale2)) {
			const float Scale = cMath::Min(Scale1, Scale2);
			m_Frame.StartNormal = (PickRay.GetFm() + Scale * PickRay.GetNormal() - m_Frame.Pos).ToNormal();
			const float D[3] = {
				cVec3::Dot(m_Frame.StartNormal, m_Frame.AxisNormal[0]),
				cVec3::Dot(m_Frame.StartNormal, m_Frame.AxisNormal[1]),
				cVec3::Dot(m_Frame.StartNormal, m_Frame.AxisNormal[2])
			};
			const int iMin = cMath::MinIndex(cMath::Abs(D[0]), cMath::Abs(D[1]), cMath::Abs(D[2]));
			if(cMath::Abs(D[iMin]) < m_MaxSelDistance) {
				m_Mode = (Mode::Enum)(Mode::AroundX + iMin);
				m_Frame.StartNormal = (m_Frame.StartNormal - m_Frame.AxisNormal[iMin] * D[iMin]).ToNormal();
				cAssert(cMath::IsZero(cVec3::Dot(m_Frame.StartNormal, m_Frame.AxisNormal[iMin])));
				m_Frame.RotationAxis = m_Frame.AxisNormal[iMin];
			} else {
				if(!m_UniversalMode) {
					m_Mode = Mode::Free;
				}
			}
		}
	}

	if(Mode::None == m_Mode) {
		return false;
	}
	
	m_Frame.StartRotation = *m_Rotate;
	//m_Frame.StartRotation = (m_Rotate->ToQuat() * m_Parent.ToQuat()).ToAngles();
	m_Frame.CurNormal = m_Frame.StartNormal;
	m_Frame.Angle = 0.0f;
	
	// Set capturing in any rotate mode.
	cWidgets::SetCapture();

	cWidgets::BringToFront(this);

	return true;
} // CRotateTool::OnButtonDown

//-----------------------------------------------------------------------------
// CRotateTool::OnButtonUp
//-----------------------------------------------------------------------------
bool CRotateTool::OnButtonUp(const int Code) {
	if(!cWidgets::IsCaptured(this)) {
		return false;
	}
	cAssert(m_Mode != Mode::None);
	if(Code != cInput::LeftButton) {
		return true;
	}
	// Releasing capture from any rotate mode:
	m_Mode = Mode::None;
	if(m_Space != m_DesiredSpace) {
		m_Space = m_DesiredSpace;
	}
	cWidgets::ReleaseCapture();
	return true; 
} // CRotateTool::OnButtonUp

//-----------------------------------------------------------------------------
// CRotateTool::OnMouseMove
//-----------------------------------------------------------------------------
bool CRotateTool::OnMouseMove() {
	if(!cWidgets::IsCaptured(this)) {
		return false;
	}
	cAssert(m_Mode != Mode::None);
	
	if(!CalcFrame()) {
		return true;
	}
	
	const cSeg PickRay = cRender::GetViewer()->GetPickRay(cInput::GetMousePosition());

	if(Mode::AroundView == m_Mode) {
		const cPlane P(m_Frame.Pos, m_Frame.AxisNormal[3]);
		cVec3 Around;
		const bool f = P.RayIntersection(PickRay.GetFm(), PickRay.GetNormal(), NULL, &Around);
		cAssert(f);
		m_Frame.CurNormal = (Around - m_Frame.Pos).ToNormal();
	} else {
		cSphere InnerSphere(m_Frame.Pos, m_Frame.InnerRadius);
		float Scale1, Scale2;
		if(InnerSphere.RayIntersection(PickRay.GetFm(), PickRay.GetNormal(), Scale1, Scale2)) {
			const float Scale = cMath::Min(Scale1, Scale2);
			m_Frame.CurNormal = (PickRay.GetFm() + Scale * PickRay.GetNormal() - m_Frame.Pos).ToNormal();
		} else {
			const cSeg::Result r = PickRay.ProjectPoint(cSeg::Ray, m_Frame.Pos);
			m_Frame.CurNormal = (r.Point - m_Frame.Pos).ToNormal();
		}
		if(m_Mode >= Mode::AroundX && m_Mode <= Mode::AroundZ) {
			const int iAxis = m_Mode - Mode::AroundX;
			cAssert(iAxis >= 0 && iAxis <= 2);
			m_Frame.CurNormal = (m_Frame.CurNormal - m_Frame.AxisNormal[iAxis] * cVec3::Dot(m_Frame.CurNormal, m_Frame.AxisNormal[iAxis])).ToNormal();
			cAssert(cMath::IsZero(cVec3::Dot(m_Frame.CurNormal, m_Frame.AxisNormal[iAxis])));
		}
	}

	if(!m_Frame.CurNormal.IsNormalized()) {
		m_Frame.CurNormal = m_Frame.StartNormal;
	}

	const float Angle = cVec3::Angle(m_Frame.StartNormal, m_Frame.CurNormal);
	cVec3 Axis = cVec3::Cross(m_Frame.StartNormal, m_Frame.CurNormal).ToNormal();
	if(m_Mode != Mode::Free) {
		const float Sign = cMath::Sign(cVec3::Dot(m_Frame.RotationAxis, Axis));
		Axis = Sign > 0.0f ? m_Frame.RotationAxis : -m_Frame.RotationAxis;
		m_Frame.Angle = Sign * Angle;
	}
	if(Axis.IsNormalized()) {
		*m_Rotate = (m_Frame.StartRotation.ToQuat() * cMat3::Rotation(cVec3::TransformNormal(Axis, m_ParentInverse).ToNormal(), Angle).ToQuat()).ToAngles();
	}
	return true;
} // CRotateTool::OnMouseMove

//-----------------------------------------------------------------------------
// CRotateTool::OnRender
//-----------------------------------------------------------------------------
void CRotateTool::OnRender() {
	if(!CalcFrame()) {
		return;
	}
	
	// Colors:
	const cColor SelColor = cColor::Yellow;
	const cColor XColor = Mode::AroundX == m_Mode ? SelColor : cColor(1.0f, 0.0f, 0.0f);
	const cColor YColor = Mode::AroundY == m_Mode ? SelColor : cColor(0.0f, 1.0f, 0.0f);
	const cColor ZColor = Mode::AroundZ == m_Mode ? SelColor : cColor(0.0f, 0.0f, 1.0f);
	const cColor OuterColor = Mode::AroundView == m_Mode ? SelColor : cColor::White;
	const cColor InnerColor(cColor::Black);
	const cColor TraceColor(cColor::Gray);
	const cColor TraceSectorFillColor(cColor::Gray, 0.5f);
	const cColor TraceSectorBorderColor = cColor::Gray;
	
	// Appearance:
	const float TraceQuadSide = 0.08f * m_Frame.OuterRadius;
	const int SubDivs = 48;
	const float TraceSectorDelta = 5.0f;
	const float LabelUp = 1.2f * m_Frame.OuterRadius;

	cRender::SetDepthState(-1);
	
	// Label:
	if(Mode::AroundX == m_Mode || Mode::AroundY == m_Mode || Mode::AroundZ == m_Mode) {
		float Angles[3] = { 0.0f, 0.0f, 0.0f };
		const int iAxis = (int)(m_Mode - Mode::AroundX);
		Angles[iAxis] = m_Frame.Angle;
		cStr Label;
		Label << "[" << cStr::ToString(Angles, 3) << "]";
		const cVec3 LabelPos = m_Frame.Pos + cRender::GetViewer()->GetUp() * LabelUp;
		static int FontID = cRender::GetFontID("Fira Sans Condensed 10");
		cRender::DrawString(FontID, Label, LabelPos, cRender::StringAlign::MiddleCenter, cColor::Yellow);
	}
	
	if(Mode::AroundX == m_Mode || Mode::AroundY == m_Mode || Mode::AroundZ == m_Mode || Mode::AroundView == m_Mode) {
		const float Radius = Mode::AroundView == m_Mode ? m_Frame.OuterRadius : m_Frame.InnerRadius;
		const cVec3 p0 = m_Frame.Pos + m_Frame.StartNormal * Radius;
		const cVec3 p1 = m_Frame.Pos + m_Frame.CurNormal * Radius;
		cRender::DrawBillboardQuadSolid(m_Frame.Pos, TraceQuadSide, TraceColor);
		cRender::DrawBillboardQuadSolid(p0, TraceQuadSide, TraceColor);
		cRender::DrawBillboardQuadSolid(p1, TraceQuadSide, TraceColor);

		static int BlendStateID = cRender::GetBlendStateID(cBlendFactor::SrcAlpha, cBlendFactor::OneMinusSrcAlpha);
		cRender::SetBlendState(BlendStateID);

		cRender::DrawSector(m_Frame.Pos, Radius, m_Frame.StartNormal, m_Frame.CurNormal, TraceSectorBorderColor, TraceSectorFillColor, TraceSectorDelta);
	}
	cRender::DrawCircle(m_Frame.Pos, m_Frame.InnerRadius, InnerColor);
	cRender::DrawCircle(m_Frame.Pos, m_Frame.OuterRadius, OuterColor);

	cRender::DrawFacingCircle(m_Frame.Pos, m_Frame.InnerRadius, m_Frame.AxisNormal[0], XColor, SubDivs);
	cRender::DrawFacingCircle(m_Frame.Pos, m_Frame.InnerRadius, m_Frame.AxisNormal[1], YColor, SubDivs);
	cRender::DrawFacingCircle(m_Frame.Pos, m_Frame.InnerRadius, m_Frame.AxisNormal[2], ZColor, SubDivs);
} // CRotateTool::OnRender

// CRotateTool::IsHot
bool CRotateTool::IsHot(const bool Handled) {
	return !Handled && cWidgets::IsCaptured(this);
}

} // comms
