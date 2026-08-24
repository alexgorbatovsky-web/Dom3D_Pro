#pragma once

//-----------------------------------------------------------------------------
// cFrustum
//-----------------------------------------------------------------------------
class cFrustum {
public:
	cFrustum() {
		Clear();
	}
	void Clear() {
		memset(m_Points, 0, sizeof(m_Points));
		memset(m_Planes, 0, sizeof(m_Planes));
	}

	const cVec3 * GetPoints() const {
		return m_Points;
	}
	cVec3 * GetPoints() {
		return m_Points;
	}
    void TransformPoints(const cMat4 &T) {
        cVec3::TransformCoordinate(m_Points, 8, T);
    }
	
	void CalcPlanes();
	const cPlane * GetPlanes() const {
		return m_Planes;
	}
	cPlane * GetPlanes() {
		return m_Planes;
	}

	bool CullPoint(const cVec3 &P) const;
	bool CullBounds(const cBounds &B, const cMat4 &T) const;

	void Scale(const cVec2 &S);
	
	// Segments
	static const int Segments[24];
	// b, n
	// r, n
	// t, n
	// l, n

	// b, f
	// r, f
	// t, f
	// l, f

	// b, l
	// b, r
	// t, r
	// t, l

	// Triangles
	static const int Triangles[36];
	// n
	// f
	// b
	// t
	// l
	// r

	static const cFrustum Zero;

private:
	cVec3 m_Points[8];
	// l, b, n
	// r, b, n
	// r, t, n
	// l, t, n

	// l, b, f
	// r, b, f
	// r, t, f
	// l, t, f

	cPlane m_Planes[6];
	// n
	// f
	// b
	// t
	// l
	// r
}; // cFrustum

//-----------------------------------------------------------------------------
// Viewer
//-----------------------------------------------------------------------------
class cViewer : public cWidget {
public:
	struct CtorArgs {
		CtorArgs() {
			SetDefaults();
		}
		void SetDefaults();
		
		cVec3 Pos;
		cAngles Orient;
		float Fov;
		float Znear;
		float Zfar;
		bool IsInfinite;
		float OrthoZrange;
	};
	
	cViewer(const CtorArgs & = CtorArgs());

	struct State {
		void Clear() {
			Pos.SetZero();
			Orient.SetZero();
			Viewport.SetZero();
			
			IsInfinite = false;
			Fov = 0.0f;
			Znear = 0.0f;
			Zfar = 0.0f;
			
			Aspect = 0.0f;

			WithCustomClipPlane = false;
			OrthoProj = false;
			OrthoScale = 1.0f;
			OrthoZrange = 0.0f;
		}

		State() {
			Clear();
		}

		cVec3 Pos;
		cQuat Orient;
		cRect Viewport;
		
		bool IsInfinite;
		float Fov;
		float Znear;
		float Zfar;
		
		float Aspect; // If "0.0f" (default), aspect is based on "Viewport"
		
		bool WithCustomClipPlane;
		cPlane CustomClipPlane;

		bool OrthoProj;
		float OrthoScale;
		float OrthoZrange;
	};

protected:
	State m_CurState;
	cList<State> m_StatesStack;
	
	// Cached values:
	cVec3 m_Forward;
	cVec3 m_Right;
	cVec3 m_Up;
	float m_Aspect;
	float m_DX;
	float m_DY;
	
	// World <-> View
	cMat4 m_World2View;
	cMat4 m_View2World;
	
	// View <-> Proj
	cMat4 m_View2Proj;
	cMat4 m_Proj2View;
	
	// Proj <-> Screen
	cMat4 m_Proj2Screen;
	cMat4 m_Screen2Proj;
	
	// World < View > Proj
	cMat4 m_World2Proj;
	cMat4 m_Proj2World;
	
	// World < View, Proj > Screen
	cMat4 m_World2Screen;
	cMat4 m_Screen2World;
	
	// View < Proj > Screen
	cMat4 m_View2Screen;
	cMat4 m_Screen2View;
	
	cPlane m_Planes[6];
	
	void Update();
public:
	void PushState();
	void PopState();

	void SetLookAtViewProjection(const cVec3 &LookFrom, const cVec3 &LookAt, const float FovY, const cRect &Viewport, const float Znear = 0.1f, const float Zfar = 2000.0f);
	
	const cVec3 & GetPos() const {
		return m_CurState.Pos;
	}
	void SetPos(const cVec3 &Pos) {
		m_CurState.Pos = Pos;
		Update();
	}
	void MovePos(const cVec3 &Delta) {
		m_CurState.Pos += Delta;
		Update();
	}
	
	const cQuat & GetOrient() const {
		return m_CurState.Orient;
	}
	void SetOrient(const cQuat &Orient) {
		m_CurState.Orient = Orient;
		Update();
	}

	const cRect & GetViewport() const {
		return m_CurState.Viewport;
	}

	comms::cVec2 GetDXY();
	void SetDXY(const float DX, const float DY);

	void SetViewport(const cRect &Viewport) {
		m_CurState.Viewport = Viewport;
		Update();
	}
	float GetAspect() const {
		return m_Aspect;
	}
	void SetAspect(const float Aspect) {
		// Set non zero to override and ignore "Viewport" aspect. Set zero to default.
		m_CurState.Aspect = Aspect;
		Update();
	}
	
	float GetFov() const {
		return m_CurState.Fov;
	}
	void SetFov(const float Fov) {
		m_CurState.Fov = Fov;
		Update();
	}
	
	float GetZnear() const {
		return m_CurState.Znear;
	}
	float GetZfar() const {
		return m_CurState.Zfar;
	}
	void SetZnearFar(const float Znear, const float Zfar) {
		m_CurState.Znear = Znear;
		m_CurState.Zfar = Zfar;
		Update();
	}

	float GetOrthoZrange() const {
		return m_CurState.OrthoZrange;
	}
	void SetOrthoZrange(const float OrthoZrange) {
		m_CurState.OrthoZrange = OrthoZrange;
		Update();
	}

	bool GetOrthoProj() const {
		return m_CurState.OrthoProj;
	}

	const cVec3 & GetForward() const {
		return m_Forward;
	}
	const cVec3 & GetRight() const {
		return m_Right;
	}
	const cVec3 & GetUp() const {
		return m_Up;
	}
	bool IsInfinite() const {
		return m_CurState.IsInfinite;
	}
	void SetInfinite(const bool IsInfinite) {
		m_CurState.IsInfinite = IsInfinite;
		Update();
	}

	void SetState(const State &S) {
		m_CurState = S;
		Update();
	}

	const State & GetState() const {
		return m_CurState;
	}
	State & GetState() {
		return m_CurState;
	}

	// World <-> View
	const cMat4 & GetViewMatrix() const {
		return m_World2View;
	}
	const cMat4 & GetViewMatrixInverse() const {
		return m_View2World;
	}

	// View <-> Proj
	const cMat4 & GetProjectionMatrix() const {
		return m_View2Proj;
	}
	const cMat4 & GetProjectionMatrixInverse() const {
		return m_Proj2View;
	}

	// Proj <-> Screen
	const cMat4 & GetScreenMatrix() const {
		return m_Proj2Screen;
	}
	const cMat4 & GetScreenMatrixInverse() const {
		return m_Screen2Proj;
	}

	// World < View > Proj
	const cMat4 & GetViewProjectionMatrix() const {
		return m_World2Proj;
	}
	const cMat4 & GetViewProjectionMatrixInverse() const {
		return m_Proj2World;
	}

	// World < View, Proj > Screen
	const cMat4 & GetViewProjectionScreenMatrix() const {
		return m_World2Screen;
	}
	const cMat4 & GetViewProjectionScreenMatrixInverse() const {
		return m_Screen2World;
	}

	// View < Proj > Screen
	const cMat4 & GetProjectionScreenMatrix() const {
		return m_View2Screen;
	}
	const cMat4 & GetProjectionScreenMatrixInverse() const {
		return m_Screen2View;
	}
	
	const cSeg GetPickRay(const cVec2 &ScreenCoord) const;
	bool WorldToClosestScreenPoint(const cVec3 &WorldPos, cVec2 *ClosestScreenPos, cVec2 *ZnearScreenPos = nullptr) const;
	
	bool CullPoint(const cVec3 &p) const;
	
	void CalcFrustum(cFrustum *Frustum, const float Zfar) const;
	void CalcFrustum(cFrustum *Frustum, const float Znear, const float Zfar) const;
}; // cViewer

//-----------------------------------------------------------------------------
// FreeViewer
//-----------------------------------------------------------------------------
class cFreeViewer : public cViewer {
public:
	struct OrthoView {
		enum Enum {
			Current = 0,
			PosX = 1,
			PosY = 2,
			PosZ = 3,
			NegX = 4,
			NegY = 5,
			NegZ = 6,
			PosY1 = 7,
			NegY1 = 8
		};
	};

	void SetOrthoProj(const OrthoView::Enum View = OrthoView::Current);
	bool GetOrthoProj() const {
		return m_OrthoProj;
	}
	void SetPerspProj();
	void SetView(const OrthoView::Enum View); // Keeps current proj mode (i.e. changes only orient)

	void OverrideCenter(const cVec3 &Center);
	cVec3 & GetCenter() {
		return m_Center;
	}

	void SetEnableInput(const bool EnableInput) {
		m_EnableInput = EnableInput;
	}
	bool GetEnableInput() const {
		return m_EnableInput;
	}

	void SetDrawTarget(const bool DrawTarget) {
		m_DrawTarget = DrawTarget;
	}
	bool GetDrawTarget() const {
		return m_DrawTarget;
	}

	void Transform(const cMat4 &);
	void OnRender();
	void Update();
	void UpdateFromState();
private:
	bool m_EnableInput;
	bool m_DrawTarget;

	cVec3 m_DefCenter;
	cAngles m_DefOrient;
	float m_DefToEye;
	float m_DefOrthoScale;
	
	cVec3 m_Center;
	float m_ToEye;
	float m_OrthoScale;

	bool m_OrthoProj;

	bool m_OverriddenCenter;
	cVec2 m_CenterShift; // Only if "m_OverriddenCenter == true"

	struct Mode {
		enum Enum {
			None, Track, Tumble, Dolly, Aim
		};
	};
	Mode::Enum m_Mode;
	bool m_DrawTargetUntilAlt;

	float m_TrackStep;
	float m_TumbleDeltaAngle;
	float m_DollyStep;
	float m_RollDeltaAngle;
	float m_AimSpeed;
	float m_ZoomStep;
	float m_MaxZoom;
public:
	bool IsNavigating() const {
		return m_Mode != Mode::None;
	}
	float GetTrackStep() const {
		return m_TrackStep;
	}
	void SetTrackStep(const float TrackStep) {
		m_TrackStep = TrackStep;
	}

	float GetTumbleDeltaAngle() const {
		return m_TumbleDeltaAngle;
	}
	void SetTumbleDeltaAngle(const float TumbleDeltaAngle) {
		m_TumbleDeltaAngle = TumbleDeltaAngle;
	}

	float GetDollyStep() const {
		return m_DollyStep;
	}
	void SetDollyStep(const float DollyStep) {
		m_DollyStep = DollyStep;
	}

	float GetRollDeltaAngle() const {
		return m_RollDeltaAngle;
	}
	void SetRollDeltaAngle(const float RollDeltaAngle) {
		m_RollDeltaAngle = RollDeltaAngle;
	}

	float GetAimSpeed() const {
		return m_AimSpeed;
	}
	void SetAimSpeed(const float AimSpeed) {
		m_AimSpeed = AimSpeed;
	}

	float GetZoomStep() const {
		return m_ZoomStep;
	}
	void SetZoomStep(const float ZoomStep) {
		m_ZoomStep = ZoomStep;
	}

	float GetMaxZoom() const {
		return m_MaxZoom;
	}
	void SetMaxZoom(const float MaxZoom) {
		m_MaxZoom = MaxZoom;
	}
	float& GetToEye(){
		return m_ToEye;
	}
	float & GetOrthoScale(){
		return m_OrthoScale;
	}
	cVec2 & GetCenterShift(){
		return m_CenterShift;
	}

	struct CtorArgs : cViewer::CtorArgs {
		CtorArgs() {
			SetDefaults();
		}
		void SetDefaults();
		cVec3 DefCenter;
		cAngles DefOrient;
		float DefToEye;
		float DefOrthoScale;
	};

	cFreeViewer(const CtorArgs & = CtorArgs());

	void HandleGesture(const cGestureType::Enum Type, const cVec2 &Translate, const float Rotate, const float Scale);
	void Home();
	void LookAt(const cVec3 &TargetPos, const cVec3 &ForwardDir);
	void Frame(const cBounds &Bounds, const cMat4 &Transform = cMat4::Identity) {
		Frame(Bounds, Transform, GetViewport());
	}
	void Frame(const cBounds &Bounds, const cMat4 &Transform, const cRect &Viewport);
	void Track(const cVec2 &Delta);
	void PlaneTrack(const cVec2 &Delta,const cVec3 &NormDir);
	void Tumble(const cVec2 &Delta);
	void SnapAxis();
	void SnapAxisEx(bool Isometry45dg, bool EquilateralHexagonIsometry, bool IsometryRhombus2x1);
	void SnapAxisEx2(bool CameraInWorldCenter, bool CameraFor2DPaint, bool CameraLock2DCanvas);
	void ScreenTumble(const cVec2 &Delta);
	void Dolly(const cVec2 &Delta);
	void Roll(const cVec2 &Delta);
	void ScreenRoll(const cVec2 &Delta);
	void Aim(const float Delta);

	bool OnButtonDown(const int Code);
	bool OnButtonUp(const int Code);
	bool OnMouseMove();
	bool IsHot(const bool Handled);
	bool OnSetCursor(cInput::Cursor::Enum *Cursor);
}; // cFreeViewer
