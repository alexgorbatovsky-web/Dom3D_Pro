#pragma once

namespace comms {

//*****************************************************************************
// CMoveTool
//*****************************************************************************
class CMoveTool : public cWidget {
public:
	CMoveTool();
	CMoveTool(cVec3 *Translate);
	CMoveTool(cAngles *Rotate, cVec3 *Translate, const cSpace::Enum Space);
	
	void Bind(cVec3 *Translate); // World space only
	void Bind(cAngles *Rotate, cVec3 *Translate, const cSpace::Enum Space);
	
	void SetSpace(const cSpace::Enum Space); // Only when it is binded with quaternion

	bool OnButtonDown(const int Code);
	bool OnButtonUp(const int Code);
	bool OnMouseMove();
	void OnRender();
	bool IsHot(const bool Handled);

	void SetParent(const cMat4 &Parent);
	void SetParent(const cMat4 &Parent, const cMat4 &ParentInverse);
	
	void SetUniversalMode(const bool UniversalMode) {
		m_UniversalMode = UniversalMode;
	}
	bool CalcFrame();
private:
	void Init();
	
	struct Mode {
		enum Enum {
			None, AlongX, AlongY, AlongZ, Center
		};
	};
	Mode::Enum m_Mode;
	cSpace::Enum m_Space, m_DesiredSpace;

	// Size:
	float m_AxisLen; // In pixels.
	float m_MaxSelDistance; // Fraction of arrow len.

	// Axis Cull:
	float m_MaxVisibleDot;
	
	cAngles *m_Rotate;
	cVec3 *m_Translate;

	// Move Tool Frame (in World space):	
	struct tFrame {
		cVec3 CurPos;
		cSeg Axis[3];
		float MaxSelDistance;
		bool AxisIsVisible[3];

		cVec3 PickedPoint;
		cVec3 StartPos;
		cPlane Center;
		cSeg StartAxis[3];
	} m_Frame;

	cMat4 m_Parent, m_ParentInverse;
	bool m_UniversalMode;
};

//*****************************************************************************
// RotateTool
//*****************************************************************************
class CRotateTool : public cWidget {
public:
	CRotateTool();
	CRotateTool(cAngles *Rotate, cVec3 *Translate, const cSpace::Enum Space);
	void Bind(cAngles *Rotate, cVec3 *Translate, const cSpace::Enum Space);
	void SetSpace(const cSpace::Enum Space);
	
	bool OnButtonDown(const int Code);
	bool OnButtonUp(const int Code);
	bool OnMouseMove();
	void OnRender();
	bool IsHot(const bool Handled);

	void SetParent(const cMat4 &Parent);
	void SetParent(const cMat4 &Parent, const cMat4 &ParentInverse);

	void SetUniversalMode(const bool UniversalMode) {
		m_UniversalMode = UniversalMode;
	}
	bool CalcFrame();
private:
	void Init();
	
	// Size:
	float m_OuterRadius; // In pixels.
	float m_InnerRadius; // Fraction of outer radius.
	float m_MaxSelDistance; // Fraction of outer radius.
	
	cAngles *m_Rotate;
	cVec3 *m_Translate;

	cSpace::Enum m_Space, m_DesiredSpace;
	
	struct Mode {
		enum Enum {
			None, Free, AroundX, AroundY, AroundZ, AroundView
		};
	};
	Mode::Enum m_Mode;
	
	// Rotate Tool Frame (in World space):	
	struct tFrame {
		cVec3 Pos;
		// 0 - X, 1 - Y, 2 - Z, 3 - View
		cVec3 AxisNormal[4];
		float OuterRadius;
		float InnerRadius;
		float MaxSelDistance;

		cVec3 StartNormal;
		cAngles StartRotation;
		cVec3 CurNormal;
		cVec3 RotationAxis;
		float Angle;
	} m_Frame;

	cMat4 m_Parent, m_ParentInverse;
	bool m_UniversalMode;
};

//*****************************************************************************
// ScaleTool
//*****************************************************************************
class CScaleTool : public cWidget {
public:
	CScaleTool();
	CScaleTool(cVec3 *Scale, cAngles *Rotate, cVec3 *Translate);
	void Bind(cVec3 *Scale, cAngles *Rotate, cVec3 *Translate);
	bool OnButtonDown(const int Code);
	bool OnButtonUp(const int Code);
	bool OnMouseMove();
	void OnRender();
	bool IsHot(const bool Handled);

	void SetParent(const cMat4 &Parent);
	
	void SetUniversalMode(const bool UniversalMode) {
		m_UniversalMode = UniversalMode;
	}
	bool CalcFrame();
private:
	void Init();
	
	// Size:
	float m_LineLen; // In pixels.
	float m_MaxSelDistance; // Fraction of line len.

	// Axis Cull:
	float m_MaxVisibleDot;
	
	cVec3 *m_Scale;
	cAngles *m_Rotate;
	cVec3 *m_Translate;
	
	struct Mode {
		enum Enum {
			None, AlongX, AlongY, AlongZ, Uniform
		};
	};
	Mode::Enum m_Mode;
	
	// Scale Tool Frame (in World space):	
	struct tFrame {
		// 0 - X, 1 - Y, 2 - Z, 3 - Central
		cVec3 AxisNormal[4];
		cVec3 HandlePos[4];
		float LineLen;
		float MaxSelDistance;
		cSeg PickRay;
		bool AxisIsVisible[4];

		cVec3 AbsScale;
		cVec3 RelScale;
		cVec3 PickedPoint;
	} m_Frame;

	cMat4 m_Parent;
	bool m_UniversalMode;
};

} // comms
