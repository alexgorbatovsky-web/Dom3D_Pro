#include "comms.h"

namespace comms {

const cFrustum cFrustum::Zero;

//*****************************************************************************
// Viewer
//****************************************************************************

// cViewer::PushState
void cViewer::PushState() {
	m_StatesStack.Add(m_CurState);
}

// cViewer::PopState
void cViewer::PopState() {
	cAssert(!m_StatesStack.IsEmpty());
	if(!m_StatesStack.IsEmpty()) {
		m_CurState = m_StatesStack.GetLast();
		m_StatesStack.RemoveLast();
		Update();
	}
}

// cViewer_CalcOrientFromForwardDir
static void cViewer_CalcOrientFromForwardDir(cQuat *Orient, const cVec3 &ForwardDir) {
	cVec3 Forward = cVec3::Normalize(ForwardDir);
	cVec3 Right = cVec3::Cross(Forward, cVec3::AxisY);
	if(Right.Length() < 0.1f) {
		Right = cVec3::Cross(Forward, cVec3::AxisX);
	}
	Right.Normalize();
	cVec3 Up = cVec3::Cross(Right, Forward).ToNormal();
	*Orient = cMat3(cMat3::RowsCtor, Right, Up, -Forward).ToQuat();
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------
// cViewer::SetLookAtViewProjection
//---------------------------------------------------------------------------------------------------------------------------------------------------------------
void cViewer::SetLookAtViewProjection(const cVec3 &LookFrom, const cVec3 &LookAt, const float FovY, const cRect &Viewport, const float Znear, const float Zfar) {
	m_CurState.Pos = LookFrom;
	cViewer_CalcOrientFromForwardDir(&m_CurState.Orient, LookAt - LookFrom);

	m_CurState.Fov = FovY;
	m_CurState.Znear = Znear;
	m_CurState.Zfar = Zfar;
	m_CurState.Viewport = Viewport;
	m_CurState.Aspect = 0.0f;
	m_CurState.IsInfinite = false;

	m_CurState.WithCustomClipPlane = false;
	m_CurState.CustomClipPlane.ToVec4().SetZero();

	m_CurState.OrthoProj = false;
	m_CurState.OrthoScale = 1.0f;
	m_CurState.OrthoZrange = 20000.0f;
	
	Update();
} // cViewer::SetLookAtViewProjection

//-----------------------------------------------------------------------------
// cViewer::CtorArgs::SetDefaults
//-----------------------------------------------------------------------------
void cViewer::CtorArgs::SetDefaults() {
	Pos.SetZero();
	Orient.SetZero();
	Fov = 67.5f;
	Znear = 1.0f;
	Zfar = 2000.0f;
	IsInfinite = false;
	OrthoZrange = 5000.0f;
} // cViewer::CtorArgs::SetDefaults

//-----------------------------------------------------------------------------
// cViewer.ctor
//-----------------------------------------------------------------------------
cViewer::cViewer(const CtorArgs &Args) {
	m_CurState.Pos = Args.Pos;
	m_CurState.Orient = Args.Orient.ToQuat();
	m_DX = m_DY = 0.0f;
	
	m_CurState.Fov = Args.Fov;
	m_CurState.Znear = Args.Znear;
	m_CurState.Zfar = Args.Zfar;
	m_CurState.Viewport.Set(0.0f, 0.0f, 640.0f, 480.0f);
	
	m_CurState.IsInfinite = Args.IsInfinite;

	m_CurState.Aspect = 0.0f;

	m_CurState.WithCustomClipPlane = false;
	m_CurState.CustomClipPlane.ToVec4().SetZero();

	m_CurState.OrthoProj = false;
	m_CurState.OrthoScale = 1.0f;
	m_CurState.OrthoZrange = Args.OrthoZrange;
	
	Update();
} // cViewer.ctor

// cViewer::GetDXY
comms::cVec2 cViewer::GetDXY() {
	return comms::cVec2(m_DX, m_DY);
}

// cViewer::SetDXY
void cViewer::SetDXY(const float DX, const float DY) {
	m_DX = DX;
	m_DY = DY;
	Update();
}

//-----------------------------------------------------------------------------
// cViewer::Update
//-----------------------------------------------------------------------------
void cViewer::Update() {
	m_CurState.Orient.ToMat3().ToVectors(&m_Forward, &m_Right, &m_Up);
	
	// World <-> View
	m_World2View.SetIdentity();
	m_World2View.SetCol0(cVec4(m_Right, -cVec3::Dot(m_Right, m_CurState.Pos)));
	m_World2View.SetCol1(cVec4(m_Up, -cVec3::Dot(m_Up, m_CurState.Pos)));
	m_World2View.SetCol2(cVec4(-m_Forward, -cVec3::Dot(-m_Forward, m_CurState.Pos)));
	m_View2World = cMat4(cMat3(cMat3::RowsCtor, m_Right, m_Up, -m_Forward), m_CurState.Pos);
	
	m_Aspect = m_CurState.Aspect != 0.0f ? m_CurState.Aspect : m_CurState.Viewport.GetWidth() / m_CurState.Viewport.GetHeight();
	// View <-> Proj
	if(m_CurState.OrthoProj) {
		m_View2Proj = cMat4::Ortho(m_CurState.Viewport.GetWidth() / m_CurState.OrthoScale, m_CurState.Viewport.GetHeight() / m_CurState.OrthoScale, -m_CurState.OrthoZrange, m_CurState.OrthoZrange);
	} else {
		if(m_CurState.IsInfinite) {
			m_View2Proj = cMat4::PerspectiveInf(m_CurState.Fov, m_Aspect, m_CurState.Znear);
		} else {
			m_View2Proj = cMat4::Perspective(m_CurState.Fov, m_Aspect, m_CurState.Znear, m_CurState.Zfar);
		}
	}

    if(m_DX != 0.0f || m_DY != 0.0f) {
		cMat4 M = cMat4::Translation(m_DX / m_CurState.Viewport.GetWidth(), m_DY / m_CurState.Viewport.GetHeight(), 0.0f);
		m_View2Proj *= M;
	}
	
	// Custom Clip Plane
	cMat4 T, C;
	cVec4 s, u;
	if(m_CurState.WithCustomClipPlane) {
		if(!cMat4::Invert(cMat4::Mul(m_World2View, m_View2Proj), &T)) {
			T.SetIdentity();
		}
		T.Transpose();

		s = m_CurState.CustomClipPlane.ToVec4();
		s.ToVec3().Normalize();
		
		u = cVec4::Transform(s, T);

		u /= cMath::Abs(u.z);
		if(cRenderType::OpenGL == cRender::GetType()) {
			u.w -= 1.0f;
		}
		
		if(u.z < 0.0f) {
			u *= -1.0f;
		}

		C.SetIdentity();
		C.SetCol2(u);

		m_View2Proj *= C;
	}
	
	cMat4::Invert(m_View2Proj, &m_Proj2View);
	
	// Proj -> Screen
	m_Proj2Screen.SetIdentity();
	m_Proj2Screen(0, 0) = m_CurState.Viewport.GetWidth() * 0.5f;
	m_Proj2Screen(1, 1) = m_CurState.Viewport.GetHeight() * 0.5f;
	m_Proj2Screen(3, 0) = m_CurState.Viewport.GetWidth() * 0.5f + m_CurState.Viewport.GetLeft();
	m_Proj2Screen(3, 1) = m_CurState.Viewport.GetHeight() * 0.5f + m_CurState.Viewport.GetBottom();
	
	// Screen -> Proj
	m_Screen2Proj.SetIdentity();
	m_Screen2Proj(0, 0) = 2.0f / m_CurState.Viewport.GetWidth();
	m_Screen2Proj(1, 1) = 2.0f / m_CurState.Viewport.GetHeight();
	m_Screen2Proj(3, 0) = -2.0f * m_CurState.Viewport.GetLeft() / m_CurState.Viewport.GetWidth() - 1.0f;
	m_Screen2Proj(3, 1) = -2.0f * m_CurState.Viewport.GetBottom() / m_CurState.Viewport.GetHeight() - 1.0f;

	// World < View > Proj
	m_World2Proj = cMat4::Mul(m_World2View, m_View2Proj);
	m_Proj2World = cMat4::Mul(m_Proj2View, m_View2World);

	// World < View, Proj > Screen
	m_World2Screen = cMat4::Mul(m_World2Proj, m_Proj2Screen);
	m_Screen2World = cMat4::Mul(m_Screen2Proj, m_Proj2World);

	// View < Proj > Screen
	m_View2Screen = cMat4::Mul(m_View2Proj, m_Proj2Screen);
	m_Screen2View = cMat4::Mul(m_Screen2Proj, m_Proj2View);

	// Building frustum:
	cVec4 d[6];

	d[0] = m_World2Proj.GetCol3() - m_World2Proj.GetCol0(); // Right plane
	d[1] = m_World2Proj.GetCol3() + m_World2Proj.GetCol0(); // Left plane
	d[2] = m_World2Proj.GetCol3() - m_World2Proj.GetCol1(); // Top plane
	d[3] = m_World2Proj.GetCol3() + m_World2Proj.GetCol1(); // Bottom plane
	d[4] = m_World2Proj.GetCol3() - m_World2Proj.GetCol2(); // Far plane
	d[5] = m_World2Proj.GetCol3() + m_World2Proj.GetCol2(); // Near plane

	for(int i = 0; i < 6; i++) {
		const float l = d[i].ToVec3().Length();
		if(l != 0.0f) {
			d[i] /= l;
		}
		m_Planes[i].SetNormal(d[i].ToVec3());
		m_Planes[i].SetOffset(-d[i].w);
	}
} // cViewer::Update

//-----------------------------------------------------------------------------
// cViewer::CullPoint
//-----------------------------------------------------------------------------
bool cViewer::CullPoint(const cVec3 &p) const {
	int i;
	for(i = 0; i < 6; i++) {
		if(m_Planes[i].Distance(p) < 0.0f) {
			return true;
		}
	}
	return false;
} // cViewer::CullPoint

// cFrustum::CalcPlanes
void cFrustum::CalcPlanes() {
	int i, i0, t0, t1, t2;
	for(i = 0; i < 6; i++) {
		i0 = 6 * i;
		t0 = Triangles[i0];
		t1 = Triangles[i0 + 1];
		t2 = Triangles[i0 + 2];
		const cVec3 &T0 = m_Points[t0];
		const cVec3 &T1 = m_Points[t1];
		const cVec3 &T2 = m_Points[t2];
		cPlane &P = m_Planes[i];
		P.SetFromPoints(T0, T1, T2);
	}
}

// cFrustum::CullPoint
bool cFrustum::CullPoint(const cVec3 &p) const {
	int i;
	for(i = 0; i < 6; i++) {
		if(m_Planes[i].Distance(p) > 0.0f) {
			return true;
		}
	}
	return false;
}

// cFrustum::Segments
const int cFrustum::Segments[24] = {
	0, 1, // b, n
	1, 2, // r, n
	2, 3, // t, n
	3, 0, // l, n
	
	4, 5, // b, f
	5, 6, // r, f
	6, 7, // t, f
	7, 4, // l, f
	
	0, 4, // b, l
	1, 5, // b, r
	2, 6, // t, r
	3, 7 // t, l
};

// cFrustum::Triangles
const int cFrustum::Triangles[36] = {
	// n
	0, 1, 2,
	0, 2, 3,

	// f
	4, 7, 5,
	5, 7, 6,
	
	// b
	0, 4, 1,
	1, 4, 5,

	// t
	3, 2, 7,
	2, 6, 7,

	// l
	3, 7, 4,
	3, 4, 0,

	// r
	2, 5, 6,
	2, 1, 5
};

//-----------------------------------------------------------------------------
// cFrustum::CullBounds
//-----------------------------------------------------------------------------
bool cFrustum::CullBounds(const cBounds &B, const cMat4 &T) const {
	cFrustum F;
	cVec3 *P = F.GetPoints();
	B.ToPoints(P);
	cVec3::TransformCoordinate(P, 8, T);

	// If all vertices of the bounds are on positive side of any plane of this frustum they are invisible
	int i, j;
	float D[6][8];
	bool q;
	for(i = 0; i < 6; i++) {
		const cPlane &M = m_Planes[i];
		q = true;
		for(j = 0; j < 8; j++) {
			const cVec3 &p = P[j];
			float &d = D[i][j];
			d = M.Distance(p);
			q = d > 0.0f && q;
		}
		if(q) {
			return true;
		}
	}
	
	// If at least one vertex of the bounds is within this frustum they are visible
	for(j = 0; j < 8; j++) {
		for(i = 0; i < 6; i++) {
			if(D[i][j] > 0.0f) {
				break;
			}
		}
		if(6 == i) {
			return false;
		}
	}

	// If at least one vertex of this frustum is within the bounds they are visible
	F.CalcPlanes();
	float O[6][8];
	for(i = 0; i < 6; i++) {
		const cPlane &M = F.m_Planes[i];
		for(j = 0; j < 8; j++) {
			const cVec3 &p = m_Points[j];
			O[i][j] = M.Distance(p);
		}
	}
	for(j = 0; j < 8; j++) {
		for(i = 0; i < 6; i++) {
			if(O[i][j] > 0.0f) {
				break;
			}
		}
		if(6 == i) {
			return false;
		}
	}

	// If at least one segment of the bounds intersects this frustum they are visible
	int i0, i1, l;
	bool b[6];
	cVec3 X[6], c;
	float fr;
	for(i = 0; i < 24; i += 2) {
		i0 = Segments[i];
		i1 = Segments[i + 1];
		for(j = 0; j < 6; j++) {
			b[j] = false;
			const float d0 = D[j][i0];
			const float d1 = D[j][i1];
			if((d0 > 0.0f && d1 > 0.0f) || (d0 < 0.0f && d1 < 0.0f)) {
				continue;
			}
			fr = d0 / (d0 - d1);
			if(fr >= 0.0f && fr <= 1.0f) {
				const cVec3 &S0 = P[i0];
				const cVec3 &S1 = P[i1];
				X[j] = cVec3::Lerp(S0, S1, fr);
				b[j] = true;
			}
		}
		for(j = 0; j < 6; j++) {
			if(!b[j]) {
				continue;
			}
			for(l = 0; l < 6; l++) {
				if(j == l || !b[l]) {
					continue;
				}
				c = cVec3::Lerp05(X[j], X[l]);
				if(!CullPoint(c)) {
					return false;
				}
			}
		}
	}
	
	// If at least one segment of this frustum intersects the bounds they are visible
	for(i = 0; i < 24; i += 2) {
		i0 = Segments[i];
		i1 = Segments[i + 1];
		for(j = 0; j < 6; j++) {
			b[j] = false;
			const float d0 = O[j][i0];
			const float d1 = O[j][i1];
			if((d0 > 0.0f && d1 > 0.0f) || (d0 < 0.0f && d1 < 0.0f)) {
				continue;
			}
			fr = d0 / (d0 - d1);
			if(fr >= 0.0f && fr <= 1.0f) {
				const cVec3 &S0 = m_Points[i0];
				const cVec3 &S1 = m_Points[i1];
				X[j] = cVec3::Lerp(S0, S1, fr);
				b[j] = true;
			}
		}
		for(j = 0; j < 6; j++) {
			if(!b[j]) {
				continue;
			}
			for(l = 0; l < 6; l++) {
				if(j == l || !b[l]) {
					continue;
				}
				c = cVec3::Lerp05(X[j], X[l]);
				if(!F.CullPoint(c)) {
					return false;
				}
			}
		}
	}
	
	return true;
} // cFrustum::CullBounds

// cFrustum::Scale
void cFrustum::Scale(const cVec2 &S) {
	// l, b, n
	// r, b, n
	// r, t, n
	// l, t, n

	// l, b, f
	// r, b, f
	// r, t, f
	// l, t, f
	
	cVec3 X = m_Points[1] - m_Points[0];
	cVec3 Y = m_Points[3] - m_Points[0];
	cVec2 l = (cVec2::One - S) / 2.0f;
	m_Points[0] += X * l.x + Y * l.y;
	m_Points[1] += -X * l.x + Y * l.y;
	m_Points[2] += -X * l.x - Y * l.y;
	m_Points[3] += X * l.x - Y * l.y;

	X = m_Points[5] - m_Points[4];
	Y = m_Points[7] - m_Points[4];
	m_Points[4] += X * l.x + Y * l.y;
	m_Points[5] += -X * l.x + Y * l.y;
	m_Points[6] += -X * l.x - Y * l.y;
	m_Points[7] += X * l.x - Y * l.y;

	CalcPlanes();
}

// cViewer::GetPickRay
const cSeg cViewer::GetPickRay(const cVec2 &ScreenCoord) const {
	cVec3 Fm(ScreenCoord, 0.0f);
	Fm.TransformCoordinate(m_Screen2World);
	if(m_CurState.OrthoProj) {
		return cSeg(cSeg::RayCtor, Fm, GetForward());
	} else {
		return cSeg(cSeg::RayCtor, Fm, Fm - m_CurState.Pos);
	}
}

// cViewer::WorldToClosestScreenPoint
bool cViewer::WorldToClosestScreenPoint(const cVec3 &WorldPos, cVec2 *ClosestScreenPos, cVec2 *ZnearScreenPos) const {
	cFrustum F;
	CalcFrustum(&F, m_CurState.Znear, m_CurState.Zfar);
	bool IsWithinFrustum = true;
	cVec3 p = WorldPos, n;
	float d;
	cPlane Z;
	int i;
	d = F.GetPlanes()[0].Distance(p); // Near
	if(d > 0.0f) {
		p = F.GetPlanes()[0].ProjectPoint(p);
		IsWithinFrustum = false;
	}
	Z = F.GetPlanes()[0];
	Z.MoveToPoint(p);
	if(ZnearScreenPos != nullptr) {
		*ZnearScreenPos = cVec3::TransformCoordinate(p, m_World2Screen).ToVec2();
	}
	for(i = 2; i <= 5; i++) { // Bottom, top, left, right
		const cPlane &P = F.GetPlanes()[i];
		d = P.Distance(p);
		if(d > 0.0f) {
			n = Z.ProjectVector(P.GetNormal());
			n.Normalize();
			P.RayIntersection(p, n, nullptr, &p);
			IsWithinFrustum = false;
		}
	}
	p.TransformCoordinate(m_World2Screen);
	if(ClosestScreenPos != nullptr) {
		*ClosestScreenPos = p.ToVec2();
	}
	return IsWithinFrustum;
}

//-----------------------------------------------------------------------------
// cViewer::CalcFrustum
//-----------------------------------------------------------------------------
void cViewer::CalcFrustum(cFrustum *Frustum, const float Zfar) const {
	Frustum->Clear();
	
	float t = m_CurState.Znear * cMath::Tan(cMath::Rad(m_CurState.Fov) * 0.5f);
	float b = -t;
	float r = m_Aspect * t;
	float l = -r;
	float n = m_CurState.Znear;
	float f = Zfar;
	float fn = f / n;
	float lFar = fn * l;
	float rFar = fn * r;
	float bFar = fn * b;
	float tFar = fn * t;
	Frustum->GetPoints()[0] = cVec3::TransformCoordinate(cVec3(l, b, -n), m_View2World);
	Frustum->GetPoints()[1] = cVec3::TransformCoordinate(cVec3(r, b, -n), m_View2World);
	Frustum->GetPoints()[2] = cVec3::TransformCoordinate(cVec3(r, t, -n), m_View2World);
	Frustum->GetPoints()[3] = cVec3::TransformCoordinate(cVec3(l, t, -n), m_View2World);
	
	Frustum->GetPoints()[4] = cVec3::TransformCoordinate(cVec3(lFar, bFar, -f), m_View2World);
	Frustum->GetPoints()[5]= cVec3::TransformCoordinate(cVec3(rFar, bFar, -f), m_View2World);
	Frustum->GetPoints()[6] = cVec3::TransformCoordinate(cVec3(rFar, tFar, -f), m_View2World);
	Frustum->GetPoints()[7] = cVec3::TransformCoordinate(cVec3(lFar, tFar, -f), m_View2World);

	Frustum->CalcPlanes();
} // cViewer::CalcFrustum

//------------------------------------------------------------------------------------
// cViewer::CalcFrustum
//------------------------------------------------------------------------------------
void cViewer::CalcFrustum(cFrustum *Frustum, const float Znear, const float Zfar) const {
	Frustum->Clear();

	float t = Znear * cMath::Tan(cMath::Rad(m_CurState.Fov) * 0.5f);
	float b = -t;
	float r = m_Aspect * t;
	float l = -r;
	float n = Znear;
	float f = Zfar;
	float fn = f / n;
	float lFar = fn * l;
	float rFar = fn * r;
	float bFar = fn * b;
	float tFar = fn * t;
	Frustum->GetPoints()[0] = cVec3::TransformCoordinate(cVec3(l, b, -n), m_View2World);
	Frustum->GetPoints()[1] = cVec3::TransformCoordinate(cVec3(r, b, -n), m_View2World);
	Frustum->GetPoints()[2] = cVec3::TransformCoordinate(cVec3(r, t, -n), m_View2World);
	Frustum->GetPoints()[3] = cVec3::TransformCoordinate(cVec3(l, t, -n), m_View2World);
	
	Frustum->GetPoints()[4] = cVec3::TransformCoordinate(cVec3(lFar, bFar, -f), m_View2World);
	Frustum->GetPoints()[5]= cVec3::TransformCoordinate(cVec3(rFar, bFar, -f), m_View2World);
	Frustum->GetPoints()[6] = cVec3::TransformCoordinate(cVec3(rFar, tFar, -f), m_View2World);
	Frustum->GetPoints()[7] = cVec3::TransformCoordinate(cVec3(lFar, tFar, -f), m_View2World);

	Frustum->CalcPlanes();
} // cViewer::CalcFrustum

//*****************************************************************************
// FreeViewer
//*****************************************************************************

//-----------------------------------------------------------------------------
// cFreeViewer::CtorArgs::SetDefaults
//-----------------------------------------------------------------------------
void cFreeViewer::CtorArgs::SetDefaults() {
	((cViewer::CtorArgs *)this)->SetDefaults();
	DefCenter.SetZero();
	DefToEye = 20.0f;
	DefOrient.Set(-28.0f, 45.0f, 0.0f);
	DefOrthoScale = 20.0f;
} // cFreeViewer::CtorArgs::SetDefaults

//---------------------------------------------------------------------------------------------------------
// cFreeViewer.ctor
//---------------------------------------------------------------------------------------------------------
cFreeViewer::cFreeViewer(const CtorArgs &Args) : cViewer() {
	m_DefCenter = Args.DefCenter;
	m_DefOrient = Args.DefOrient;
	m_DefToEye = Args.DefToEye;
	m_CurState.Znear = Args.Znear;
	m_CurState.Zfar = Args.Zfar;
	m_DefOrthoScale = Args.DefOrthoScale;
	Home();

	m_Mode = Mode::None;
	m_OrthoProj = false;
	m_OrthoScale = m_DefOrthoScale;

	m_TrackStep = 0.001f;
	m_TumbleDeltaAngle = 0.3f;
	m_DollyStep = 0.2f;
	m_RollDeltaAngle = 0.3f;
	m_AimSpeed = 1.2f;
	m_ZoomStep = 0.002f;
	m_MaxZoom = 1000.0f;

	m_OverriddenCenter = false;
	m_CenterShift.SetZero();

	m_EnableInput = true;
	m_DrawTarget = true;
} // cFreeViewer.ctor

// Uncomment this for "Maya" style ortho view
//#define FREE_VIEWER_MAYA_ORTHO_STYLE

// cFreeViewer::SetView
void cFreeViewer::SetView(const cFreeViewer::OrthoView::Enum View) {
	if(View != OrthoView::Current) {
		m_CenterShift.SetZero();
		if(OrthoView::PosZ == View) {
			m_CurState.Orient = cMat3::FromVectors(cVec3::AxisNegZ, cVec3::AxisX, cVec3::AxisY).ToQuat();
		} else if(OrthoView::NegZ == View) {
			m_CurState.Orient = cMat3::FromVectors(cVec3::AxisZ, cVec3::AxisNegX, cVec3::AxisY).ToQuat();
		} else if(OrthoView::NegX == View) {
			m_CurState.Orient = cMat3::FromVectors(cVec3::AxisX, cVec3::AxisZ, cVec3::AxisY).ToQuat();
		} else if(OrthoView::PosX == View) {
			m_CurState.Orient = cMat3::FromVectors(cVec3::AxisNegX, cVec3::AxisNegZ, cVec3::AxisY).ToQuat();
		} else if(OrthoView::PosY == View) {
			m_CurState.Orient = cMat3::FromVectors(cVec3::AxisNegY, cVec3::AxisZ, cVec3::AxisX).ToQuat();
		} else if(OrthoView::NegY == View) {
			m_CurState.Orient = cMat3::FromVectors(cVec3::AxisY, cVec3::AxisZ, cVec3::AxisNegX).ToQuat();
		} else if (OrthoView::PosY1 == View) {
			m_CurState.Orient = cMat3::FromVectors(cVec3::AxisNegY, cVec3::AxisX, cVec3::AxisNegZ).ToQuat();
		} else if (OrthoView::NegY1 == View) {
			m_CurState.Orient = cMat3::FromVectors(cVec3::AxisY, cVec3::AxisX, cVec3::AxisZ).ToQuat();
		}
		else{
			cAssert(0);
		}
	}
	Update();
}

// cFreeViewer::SetOrthoProj
void cFreeViewer::SetOrthoProj(const cFreeViewer::OrthoView::Enum View) {
#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
	if(OrthoView::Current == View) {
		return;
	}
	m_OverriddenCenter = false;
	m_CenterShift.SetZero();
#endif // FREE_VIEWER_MAYA_ORTHO_STYLE
	m_OrthoProj = true;
	if(View != OrthoView::Current) {
#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
		m_Center.SetZero();
#endif // FREE_VIEWER_MAYA_ORTHO_STYLE
		m_OrthoScale = m_DefOrthoScale;
		SetView(View);
	}
	Update();
}

// cFreeViewer::SetPerspProj
void cFreeViewer::SetPerspProj() {
	m_OrthoProj = false;
	Update();
}

// cFreeViewer::UpdateFromState
void cFreeViewer::UpdateFromState() {
	const cVec3 Forward = m_CurState.Orient.ToMat3().ToForward();
	m_ToEye = m_DefToEye;
	m_Center = m_CurState.Pos + m_ToEye * Forward;
}

// cFreeViewer::Update
void cFreeViewer::Update() {
	const cVec3 Forward = m_CurState.Orient.ToMat3().ToForward();
	m_CurState.Pos = m_Center;
	if(!m_OrthoProj)m_CurState.Pos-=Forward * m_ToEye;
	if(m_OverriddenCenter) {
		cVec3 Right, Up;
		m_CurState.Orient.ToMat3().ToVectors(nullptr, &Right, &Up);
		m_CurState.Pos += m_CenterShift.x * Right + m_CenterShift.y * Up;
	}
	m_CurState.OrthoProj = m_OrthoProj;
	m_CurState.OrthoScale = m_OrthoScale;

	cViewer::Update();
} // cFreeViewer::Update

// cFreeViewer::OverrideCenter
void cFreeViewer::OverrideCenter(const cVec3 &Center) {
#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
	if(m_OrthoProj) {
		return;
	}
#endif

	cVec3 Forward, Right, Up;
	m_CurState.Orient.ToMat3().ToVectors(&Forward, &Right, &Up);
	cVec3 p = m_Center - Forward * m_ToEye;
	if(m_OverriddenCenter) {
		p += m_CenterShift.x * Right + m_CenterShift.y * Up;
	}

	m_Center = Center;

	cPlane P;
	P.SetFromPointAndNormal(p, Forward);
	cVec3 c = P.ProjectPoint(m_Center);
	cVec3 d = p - c;

	m_CenterShift.Set(cVec3::Dot(d, Right), cVec3::Dot(d, Up));
	cVec3 l = m_Center - c;
	m_ToEye = l.Length() * cMath::Sign(cVec3::Dot(l, Forward));
	
	m_OverriddenCenter = true;

	Update();
}

// cFreeViewer::HandleGesture
void cFreeViewer::HandleGesture(const cGestureType::Enum Type, const cVec2 &Translate, const float Rotate, const float Scale) {
	static float DollySpeed = 150.0f, RollSpeed = -3.0f, TrackSpeed = 1.5f;
	if(cGestureType::Translate1 == Type) {
		Tumble(Translate);
	} else if(cGestureType::Translate2 == Type) {
		Track(TrackSpeed * Translate);
		Dolly(cVec2(DollySpeed * (Scale - 1.0f), 0.0f));
		Roll(cVec2(RollSpeed * Rotate, 0.0f));
	} else if(cGestureType::Scale == Type) {
		Track(TrackSpeed * Translate);
		Dolly(cVec2(DollySpeed * (Scale - 1.0f), 0.0f));
	} else if(cGestureType::Rotate == Type) {
		Track(TrackSpeed * Translate);
		Roll(cVec2(RollSpeed * Rotate, 0.0f));
	}
}

// cFreeViewer::Home
void cFreeViewer::Home() {
	SetPerspProj();
	
	m_Center = m_DefCenter;

	m_CurState.Orient = m_DefOrient.ToQuat();
	m_ToEye = m_DefToEye;
	m_DrawTargetUntilAlt = false;
	m_OverriddenCenter = false;
	m_CenterShift.SetZero();
	Update();
}

// cFreeViewer::LookAt
void cFreeViewer::LookAt(const cVec3 &TargetPos, const cVec3 &ForwardDir) {
	m_Center = TargetPos;
	cViewer_CalcOrientFromForwardDir(&m_CurState.Orient, ForwardDir);
	
	m_DrawTargetUntilAlt = false;
	m_OverriddenCenter = false;
	m_CenterShift.SetZero();
	Update();
}

// cFreeViewer::Track
void cFreeViewer::Track(const cVec2 &Delta) {
	float Fr = 0.0f, Fu = 0.0f;
	if(m_OrthoProj) {
		Fr = Delta.x / m_OrthoScale;
		Fu = Delta.y / m_OrthoScale;
		if(m_OverriddenCenter) {
			m_CenterShift.x -= Fr;
			m_CenterShift.y -= Fu;
		} else {
			m_Center -= Fr * GetRight();
			m_Center -= Fu * GetUp();
		}
	} else {
		Fr = Delta.x * m_TrackStep * (10.0f + cMath::Abs(m_ToEye));
		Fu = Delta.y * m_TrackStep * (10.0f + cMath::Abs(m_ToEye));
		if(m_OverriddenCenter) {
			m_CenterShift.x -= Fr;
			m_CenterShift.y -= Fu;
		} else {
			m_Center -= Fr * GetRight();
			m_Center -= Fu * GetUp();
		}
	}
	Update();
}

// cFreeViewer::PlaneTrack
void cFreeViewer::PlaneTrack(const cVec2 &Delta,const cVec3 &NormDir){
	float Fr = 0.0f, Fu = 0.0f;
	cVec3 R=cVec3::Cross(NormDir,GetRight());
	R=cVec3::Cross(R,NormDir);
	R.Normalize();
	cVec3 U=cVec3::Cross(NormDir,GetUp());
	U=cVec3::Cross(NormDir,R);
	U.Normalize();
	if(m_OrthoProj) {
		Fr = Delta.x / m_OrthoScale;
		Fu = Delta.y / m_OrthoScale;
		if(m_OverriddenCenter) {
			m_CenterShift.x -= Fr;
			m_CenterShift.y -= Fu;
		} else {
			m_Center -= Fr * R;
			m_Center -= Fu * U;
		}
	} else {
		Fr = Delta.x * m_TrackStep * (10.0f + cMath::Abs(m_ToEye));
		Fu = Delta.y * m_TrackStep * (10.0f + cMath::Abs(m_ToEye));
		if(m_OverriddenCenter) {
			m_CenterShift.x -= Fr;
			m_CenterShift.y -= Fu;
		} else {
			m_Center -= Fr * R;
			m_Center -= Fu * U;
		}
	}
	Update();
}

// cFreeViewer_SnapVec
static void cFreeViewer_SnapVec(cVec3 &u){
	float X = cMath::Abs(u.x);
	float Y = cMath::Abs(u.y);
	float Z = cMath::Abs(u.z);
	if (X > Y && X > Z) {
		u.y = u.z = 0.0f;
	}
	if (Y > X && Y > Z) {
		u.x = u.z = 0.0f;
	}
	if (Z > X && Z > Y) {
		u.x = u.y = 0.0f;
	}
	u.Normalize();
}

// cFreeViewer::SnapAxisEx2
void cFreeViewer::SnapAxisEx2(bool CameraInWorldCenter, bool CameraFor2DPaint, bool CameraLock2DCanvas) {

	if (CameraInWorldCenter) {
		m_Center = cVec3(0, 0, 0);
		m_CenterShift = cVec2(0, 0);
		m_ToEye = 0;

		Update();
	}

	if (CameraFor2DPaint || CameraLock2DCanvas) {

		cMat3 M = m_CurState.Orient.ToMat3();

		cVec3 Forward = cVec3(0, -1, 0);
		cVec3 Right = cVec3::Cross(Forward, M.Row1()/*cVec3::AxisY*/);
		if (Right.Length() < 0.1f || CameraLock2DCanvas) {
			Right = cVec3::Cross(Forward, cVec3(0,0,-1));
		}
		Right.Normalize();
		cVec3 Up = cVec3::Cross(Right, Forward).ToNormal();
		m_CurState.Orient = cMat3(cMat3::RowsCtor, Right, Up, -Forward).ToQuat();


		m_DrawTargetUntilAlt = false;
		m_OverriddenCenter = false;
		m_CenterShift.SetZero();
		Update();


	}
}

// cFreeViewer::SnapAxisEx
void cFreeViewer::SnapAxisEx(bool Isometry45dg, bool EquilateralHexagonIsometry, bool IsometryRhombus2x1){
	cMat3 M = m_CurState.Orient.ToMat3();
	int i;
	cVec3 u;
	for(i = 0; i < 3; i++) {
		u = M.GetRow(i);
		if (!Isometry45dg && !EquilateralHexagonIsometry && !IsometryRhombus2x1){
			cFreeViewer_SnapVec(u);
		}
		else {
			cVec3 vn = u;
			vn.Normalize();
			float vDist = 2.0f;
			if (EquilateralHexagonIsometry){
				for (int iX = -1; iX <= 1; iX++) for (float iY = -2.0; iY <= 3; iY += 2.0) for (int iZ = -1; iZ <= 1.0; iZ++) {
					cVec3 tv = cVec3((float)iX, iY, (float)iZ);
					tv.Normalize();
					float tDist = tv.distance(vn);
					if (tDist < vDist){
						vDist = tDist;
						u = tv;
					}
				}
			}

			if (IsometryRhombus2x1){
				for (int iX = -1; iX <= 1; iX++) for (float iY = -2.45f; iY <= 3; iY += 2.45f) for (int iZ = -1; iZ <= 1; iZ++){
					cVec3 tv = cVec3((float)iX, iY, (float)iZ);
					tv.Normalize();
					float tDist = tv.distance(vn);
					if (tDist < vDist){
						vDist = tDist;
						u = tv;
					}
				}
			}

			if (Isometry45dg){
				for (int iX = -1; iX <= 1; iX++) for (float iY = -1.0; iY <= 1; iY += 1.0) for (int iZ = -1; iZ <= 1; iZ++){
					cVec3 tv = cVec3((float)iX, iY, (float)iZ);
					tv.Normalize();
					float tDist = tv.distance(vn);
					if (tDist < vDist){
						vDist = tDist;
						u = tv;
					}
				}
			}
		}
		M.SetRow(i, u);
	}
	m_CurState.Orient = M.ToQuat();
	Update();
}

// cFreeViewer::SnapAxis
void cFreeViewer::SnapAxis() {
	cMat3 M = m_CurState.Orient.ToMat3();
	int i;
	cVec3 u;
	for(i = 0; i < 3; i++) {
		u = M.GetRow(i);
		cFreeViewer_SnapVec(u);
		M.SetRow(i, u);
	}
	m_CurState.Orient = M.ToQuat();
	Update();
}

// cFreeViewer::Tumble
void cFreeViewer::Tumble(const cVec2 &Delta) {
#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
	if(m_OrthoProj) {
		return;
	}
#endif // FREE_VIEWER_MAYA_ORTHO_STYLE
	m_CurState.Orient = cMat3::RotationX(Delta.y * m_TumbleDeltaAngle).ToQuat() * m_CurState.Orient;
	m_CurState.Orient = m_CurState.Orient * cMat3::RotationY(-Delta.x * m_TumbleDeltaAngle).ToQuat();
	Update();
}

// cFreeViewer::ScreenTumble
void cFreeViewer::ScreenTumble(const cVec2 &Delta){
#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
	if(m_OrthoProj) {
		return;
	}
#endif // FREE_VIEWER_MAYA_ORTHO_STYLE
	m_CurState.Orient = m_CurState.Orient * (cMat3::Rotation(GetRight(),Delta.y * m_TumbleDeltaAngle).ToQuat());
	m_CurState.Orient = m_CurState.Orient * (cMat3::Rotation(GetUp(), -Delta.x * m_TumbleDeltaAngle).ToQuat());
	Update();
}

// cFreeViewer::Dolly
void cFreeViewer::Dolly(const cVec2 &Delta) {
	if(m_OrthoProj) {
		m_OrthoScale += m_ZoomStep * (Delta.x - Delta.y) * m_OrthoScale;
		if(m_OrthoScale > m_MaxZoom) {
			m_OrthoScale = m_MaxZoom;
		}
	} else {
		const float MinDistance = 0.0f;
		m_ToEye -= m_DollyStep * (Delta.x - Delta.y);
		if(!m_OverriddenCenter && m_ToEye < MinDistance) {
			const float r = MinDistance - m_ToEye;
			m_ToEye = MinDistance;
			m_Center += r * GetForward();
		}
	}
	Update();
}

// cFreeViewer::Roll
void cFreeViewer::Roll(const cVec2 &Delta) {
#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
	if(m_OrthoProj) {
		return;
	}
#endif // FREE_VIEWER_MAYA_ORTHO_STYLE
	m_CurState.Orient = cMat3::RotationZ(m_RollDeltaAngle * (Delta.x + Delta.y)).ToQuat() * m_CurState.Orient;
	Update();
}

// cFreeViewer::ScreenRoll
void cFreeViewer::ScreenRoll(const cVec2 &Delta){
#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
	if(m_OrthoProj) {
		return;
	}
#endif // FREE_VIEWER_MAYA_ORTHO_STYLE
	m_CurState.Orient = m_CurState.Orient * (cMat3::Rotation(GetForward(), m_RollDeltaAngle * (Delta.x + Delta.y)).ToQuat());
	Update();
}

// cFreeViewer::Aim
void cFreeViewer::Aim(const float Delta) {
	if(m_OrthoProj) {
		return;
	}
	if(m_OverriddenCenter) {
		return;
	}
	const float MinDistance = 0.0f;
	m_DrawTargetUntilAlt = true;
	const float PrevToEye = m_ToEye;
	m_ToEye = cMath::Clamp(m_ToEye + m_AimSpeed * Delta, MinDistance, cMath::FloatMaxValue);
	const float d = m_ToEye - PrevToEye;
	m_Center += d * GetForward();
	Update();
}

//-----------------------------------------------------------------------------
// cFreeViewer::OnButtonDown
//-----------------------------------------------------------------------------
bool cFreeViewer::OnButtonDown(const int Code) {
	if(!cWidgets::IsCaptured(this)) {
		if(!m_EnableInput) {
			return false;
		}

		cAssert(Mode::None == m_Mode);

		if(Code == cInput::NumPad2) {
#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
			SetOrthoProj(OrthoView::PosZ);
#else
			SetView(OrthoView::PosZ);
#endif // FREE_VIEWER_MAYA_ORTHO_STYLE
			return true;
		}
		if(Code == cInput::NumPad8) {
#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
			SetOrthoProj(OrthoView::NegZ);
#else
			SetView(OrthoView::NegZ);
#endif // FREE_VIEWER_MAYA_ORTHO_STYLE
			return true;
		}
		if(Code == cInput::NumPad4) {
#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
			SetOrthoProj(OrthoView::NegX);
#else
			SetView(OrthoView::NegX);
#endif // FREE_VIEWER_MAYA_ORTHO_STYLE
			return true;
		}
		if(Code == cInput::NumPad6) {
#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
			SetOrthoProj(OrthoView::PosX);
#else
			SetView(OrthoView::PosX);
#endif // FREE_VIEWER_MAYA_ORTHO_STYLE
			return true;
		}
		if(Code == cInput::NumPad7) {
#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
			SetOrthoProj(OrthoView::PosY);
#else
			SetView(OrthoView::PosY);
#endif // FREE_VIEWER_MAYA_ORTHO_STYLE
			return true;
		}
		if(Code == cInput::NumPad1) {
#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
			SetOrthoProj(OrthoView::NegY);
#else
			SetView(OrthoView::NegY);
#endif // FREE_VIEWER_MAYA_ORTHO_STYLE
			return true;
		}

#ifdef FREE_VIEWER_MAYA_ORTHO_STYLE
		if(Code == cInput::NumPad5) {
			Home();
			return true;
		}
#else // !FREE_VIEWER_MAYA_ORTHO_STYLE
		if(Code == cInput::NumPad5) {
			if(GetOrthoProj()) {
				SetPerspProj();
			} else {
				SetOrthoProj();
			}
			return true;
		}
#endif // FREE_VIEWER_MAYA_ORTHO_STYLE
		
		if(Code == cInput::Home) {
			Home();
			return true;
		}
#ifdef COMMS_3DCOAT
		if(!cInput::IsDown(cInput::Alt) && Code != cInput::MiddleButton) {
#else
		if(!cInput::IsDown(cInput::Alt)) {
#endif // COMMS_3DCOAT
			return false;
		}
	}

	Mode::Enum Match = Mode::None;

	if(Mode::Aim == m_Mode || Mode::None == m_Mode) {
		if(Code == cInput::WheelUp || Code == cInput::WheelDown) {
			Match = Mode::Aim;
			Aim(cInput::GetWheelDelta());
		} else if(Code == cInput::LeftButton) {
			Match = Mode::Tumble;
		} else if(Code == cInput::RightButton) {
			Match = Mode::Dolly;
		} else if(Code == cInput::MiddleButton) {
			Match = Mode::Track;
		}
	}

	if(Mode::None == m_Mode) {
		if(Mode::None == Match) {
			return false;
		}
		
		m_Mode = Match;
		cWidgets::SetCapture();
		return true;
	}

	if(Match != Mode::None) {
		m_Mode = Match;
	}
	
	return true;
} // cFreeViewer::OnButtonDown

//-----------------------------------------------------------------------------
// cFreeViewer::OnMouseMove
//-----------------------------------------------------------------------------
bool cFreeViewer::OnMouseMove() {
	if(!cWidgets::IsCaptured(this)) {
		return false;
	}
	
	switch(m_Mode) {
		case Mode::Tumble:
			if(cInput::IsDown(cInput::RightButton)) {
				Track(cInput::GetMouseDelta());
			} else if(cInput::IsDown(cInput::MiddleButton)) {
				Roll(cInput::GetMouseDelta());
			} else {
				Tumble(cInput::GetMouseDelta());
			}
			break;
		case Mode::Dolly:
			if(cInput::IsDown(cInput::LeftButton)) {
				Track(cInput::GetMouseDelta());
			} else if(cInput::IsDown(cInput::MiddleButton)) {
				Roll(cInput::GetMouseDelta());
			} else {
				Dolly(cInput::GetMouseDelta());
			}
			break;
		case Mode::Track:
			Track(cInput::GetMouseDelta());
			break;
		case Mode::Aim:
			break;
		default:
			cAssertM(0, "Illegal Mode");
			break;
	}
	return true;
} // cFreeViewer::OnMouseMove

//-----------------------------------------------------------------------------
// cFreeViewer::OnButtonUp
//-----------------------------------------------------------------------------
bool cFreeViewer::OnButtonUp(const int Code) {
	if(!cWidgets::IsCaptured(this)) {
		return false;
	}
	
	switch(m_Mode) {
		case Mode::Tumble:
			if(Code == cInput::LeftButton) {
				if(cInput::IsDown(cInput::Alt)) {
					m_Mode = Mode::Aim;
				} else {
					m_Mode = Mode::None;
				}
			}
			break;
		case Mode::Dolly:
			if(Code == cInput::RightButton) {
				if(cInput::IsDown(cInput::Alt)) {
					m_Mode = Mode::Aim;
				} else {
					m_Mode = Mode::None;
				}
			}
			break;
		case Mode::Track:
			if(Code == cInput::MiddleButton) {
				if(cInput::IsDown(cInput::Alt)) {
					m_Mode = Mode::Aim;
				} else {
					m_Mode = Mode::None;
				}
			}
			break;
		case Mode::Aim:
			if(Code == cInput::Alt) {
				m_Mode = Mode::None;
			}
			break;
		default:
			cAssertM(0, "Illegal Mode");
			break;
	}
	if(Mode::None == m_Mode) {
		cWidgets::ReleaseCapture();
	}
	return true;
} // cFreeViewer::OnButtonUp

//-----------------------------------------------------------------------------
// cFreeViewer::OnRender
//-----------------------------------------------------------------------------
void cFreeViewer::OnRender() {	
	if(!m_DrawTarget) {
		return;
	}

	if(m_Mode != Mode::None && m_DrawTargetUntilAlt) {
		m_DrawTargetUntilAlt = false;
	}
	if(m_DrawTargetUntilAlt && !cInput::IsDown(cInput::Alt)) {
		m_DrawTargetUntilAlt = false;
	}
	if(m_Mode == Mode::None && !m_DrawTargetUntilAlt) {
		return;
	}

	const float l = 1.0;
	
	const cVec3 Cross[] = {
		cVec3(m_Center.x - l, m_Center.y, m_Center.z), cVec3(m_Center.x + l, m_Center.y, m_Center.z), // X
		cVec3(m_Center.x, m_Center.y - l, m_Center.z), cVec3(m_Center.x, m_Center.y + l, m_Center.z), // Y
		cVec3(m_Center.x, m_Center.y, m_Center.z - l), cVec3(m_Center.x, m_Center.y, m_Center.z + l) // Z
	};
	
	for(int i = 0; i < 3; i++) {
		cRender::DrawLine(Cross[2 * i + 0], Cross[2 * i + 1], cColor::Yellow);
	}
} // cFreeViewer::OnRender

//-----------------------------------------------------------------------------
// cFreeViewer::OnSetCursor
//-----------------------------------------------------------------------------
bool cFreeViewer::OnSetCursor(cInput::Cursor::Enum *Cursor) {
	if(Mode::Track == m_Mode) {
		*Cursor = cInput::Cursor::SizeAll;
		return true;
	}
	return false;
} // cFreeViewer::OnSetCursor

// cFreeViewer::IsHot
bool cFreeViewer::IsHot(const bool Handled) {
	if(!Handled) {
		return m_Mode != Mode::None;
	}
	return false;
}

// cFreeViewer::Transform
void cFreeViewer::Transform(const cMat4 &T) {
	const cQuat q = T.ToQuat();
	const cVec3 t = T.GetTranslation();
	m_CurState.Orient = q * m_CurState.Orient;
	m_Center += t;
	Update();
}

//---------------------------------------------------------------------------------------------
// cFreeViewer::Frame
//---------------------------------------------------------------------------------------------
void cFreeViewer::Frame(const cBounds &Bounds, const cMat4 &Transform, const cRect &Viewport) {
	if(Bounds.IsEmpty()) {
		return;
	}
	if(Viewport.IsEmpty() || Viewport.GetWidth() < 1.0f || Viewport.GetHeight() < 1.0f) {
		return;
	}

	cQuat O = m_CurState.Orient;
	bool Ortho = m_OrthoProj;
	Home();

	cVec3 P[8];
	Bounds.ToPoints(P);
	int i;
	for(i = 0; i < 8; i++) {
		P[i].TransformCoordinate(Transform);
	}
	
	cSphere Q = cSphere::FromPoints(P, 8);
	m_Center = Q.GetCenter();
	m_CurState.Orient = O;

	if(Ortho) {
		m_OrthoProj = true;
		Update();
		cPlane H, V;
		H.SetFromPointAndNormal(m_Center, GetRight());
		V.SetFromPointAndNormal(m_Center, GetUp());
		float h = -cMath::FloatMaxValue, v = -cMath::FloatMaxValue;
		for(i = 0; i < 8; i++) {
			h = cMath::Max(h, cMath::Abs(H.Distance(P[i])));
			v = cMath::Max(v, cMath::Abs(V.Distance(P[i])));
		}
		h = cMath::Max(h, 1.0f);
		v = cMath::Max(v, 1.0f);
		float X = Viewport.GetWidth();
		float Y = Viewport.GetHeight();
		float q = cMath::Min(X / (2.0f * h), Y / (2.0f * v));
		m_OrthoScale = q;
		Update();
	} else { // Persp
		Update();

		cFrustum F;
		CalcFrustum(&F, 1000.0f);
		cVec2 v = Viewport.GetSize() / m_CurState.Viewport.GetSize();
		F.Scale(v);

		cPlane L[4] = {
			F.GetPlanes()[2], // b
			F.GetPlanes()[3], // t
			F.GetPlanes()[4], // l
			F.GetPlanes()[5]  // r
		};
		int l;
		for(l = 0; l < 4; l++) {
			for(i = 0; i < 8; i++) {
				if(L[l].Distance(P[i]) < 0.0f) {
					L[l].MoveToPoint(P[i]);
				}
			}
		}
		bool r;
		float d = -cMath::FloatMaxValue;
		cVec3 t(0.0f);
		for(l = 0; l < 4; l++) {
			r = L[l].RayIntersection(m_Center, GetForward(), nullptr, &t);
			cAssert(r);
			d = cMath::Max(d, cVec3::Distance(t, m_Center));
		}
		m_ToEye = d;
		Update();
		
		m_Center.TransformCoordinate(m_World2Screen);
		m_Center.TransformCoordinate(m_Screen2World);
		Update();
	}
} // cFreeViewer::Frame

} // comms
