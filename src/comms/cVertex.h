#pragma once

//*****************************************************************************
// cVertexUsage
//*****************************************************************************
struct cVertexUsage {
	enum Enum {
		Custom = 0,
		Position = 1,
		Normal = 2,
		TexCoord = 3,
		Tangent = 4,
		BiTangent = 5,
		Indices = 6,
		Weights = 7
	};
};
	
//*****************************************************************************
// cVertexType
//*****************************************************************************
struct cVertexType {
	enum Enum {
		Float = 0,
		Byte = 1
	};
	static int SizeOf(const cVertexType::Enum Type);
};

//*****************************************************************************
// cVertex
//*****************************************************************************
class cVertex {
public:
	//-------------------------------------------------------------------------
	// Attrib
	//-------------------------------------------------------------------------
	struct Attrib {
		cStr Name;
		cVertexUsage::Enum Usage;
		cVertexType::Enum Type;
		int Dim;

		// ---------------------------------------
		// Usage		Type			Dim
		// ---------------------------------------
		// Custom		Float			1, 2, 3, 4
		//				Byte			4
		// Position		Float			3, 4
		// Normal		Float			3
		// TexCoord		Float			2, 3, 4
		// Tangent		Float			3
		// BiTangent	Float			3
		// Indices		Float			4
		// Weights		Float			4
		// ---------------------------------------
		bool IsValid() const;
	};

	//-------------------------------------------------------------------------
	// Format
	//-------------------------------------------------------------------------
	class Format : public cList<Attrib> {
	public:
		int Add(const char *Name, const cVertexUsage::Enum Usage, const cVertexType::Enum Type, const int Dim);
		bool HasUsage(const cVertexUsage::Enum Usage, int *Offset = nullptr, int *Dim = nullptr) const;
	};
	
	//---------------------------------------------------------------------
	// PositionColored
	//---------------------------------------------------------------------
	struct PositionColored {
		cVec3 Pos;
		dword Color;

		PositionColored() {
		}
		
		PositionColored(const cVec3 &_Pos, const dword _Color) : Pos(_Pos), Color(_Color) {
		}

		static const Format GetFormat() {
			Format F;
			F.Add( "Pos",		cVertexUsage::Position,		cVertexType::Float,		3 ); // POSITION
			F.Add( "Color",		cVertexUsage::Custom,		cVertexType::Byte,		4 ); // TEXCOORD0
			return F;
		}

		static int FormatID;
	}; // PositionColored
	
	//---------------------------------------------------------------------
	// PositionColoredTextured
	//---------------------------------------------------------------------
	struct PositionColoredTextured {
		cVec3 Pos;
		dword Color;
		cVec2 TexCoord;

		static const Format GetFormat() {
			Format F;
			F.Add( "Pos",		cVertexUsage::Position,		cVertexType::Float,		3 ); // POSITION
			F.Add( "Color",		cVertexUsage::Custom,		cVertexType::Byte,		4 ); // TEXCOORD0
			F.Add( "TexCoord",	cVertexUsage::TexCoord,		cVertexType::Float,		2 ); // TEXCOORD1
			return F;
		}

		static int FormatID;
	}; // PositionColoredTextured

	//-------------------------------------------------------------------------
	// PositionColoredTextured2
	//-------------------------------------------------------------------------
	struct PositionColoredTextured2 {
		cVec3 Pos;
		dword Color;
		cVec2 TexCoord0;
		cVec2 TexCoord1;

		static const Format GetFormat() {
			Format F;
			F.Add( "Pos",		cVertexUsage::Position,		cVertexType::Float,		3 ); // POSITION
			F.Add( "Color",		cVertexUsage::Custom,		cVertexType::Byte,		4 ); // TEXCOORD0
			F.Add( "TexCoord0",	cVertexUsage::TexCoord,		cVertexType::Float,		2 ); // TEXCOORD1
			F.Add( "TexCoord1",	cVertexUsage::Custom,		cVertexType::Float,		2 ); // TEXCOORD2
			return F;
		}
		
		static int FormatID;
	}; // PositionColoredTextured2

	//-------------------------------------------------------------------------
	// Particle
	//-------------------------------------------------------------------------
	struct Particle {
		cVec3 Pos;
		dword Color;
		cVec2 TexCoord;
		cVec2 RightUp;
		cVec3 Forward;

		static const Format GetFormat() {
			Format F;
			F.Add( "Pos",		cVertexUsage::Position,		cVertexType::Float,		3 ); // POSITION
			F.Add( "Color",		cVertexUsage::Custom,		cVertexType::Byte,		4 ); // TEXCOORD0
			F.Add( "TexCoord",	cVertexUsage::TexCoord,		cVertexType::Float,		2 ); // TEXCOORD1
			F.Add( "RightUp",	cVertexUsage::Custom,		cVertexType::Float,		2 ); // TEXCOORD2
			F.Add( "Forward",	cVertexUsage::Custom,		cVertexType::Float,		3 ); // TEXCOORD3
			return F;
		}
		
		static int FormatID;
	}; // Particle

	//---------------------------------------------------------------------
	// PositionNormal
	//---------------------------------------------------------------------
	struct PositionNormal {
		cVec3 Pos;
		cVec3 Normal;

		static const Format GetFormat() {
			Format F;
			F.Add( "Pos",		cVertexUsage::Position,		cVertexType::Float,		3 ); // POSITION
			F.Add( "Normal",	cVertexUsage::Normal,		cVertexType::Float,		3 ); // TEXCOORD0
			return F;
		}

		static int FormatID;
	}; // PositionNormal
	
	//---------------------------------------------------------------------
	// PositionNormalColored
	//---------------------------------------------------------------------
	struct PositionNormalColored {
		cVec3 Pos;
		cVec3 Normal;
		dword Color;

		static const Format GetFormat() {
			Format F;
			F.Add( "Pos",		cVertexUsage::Position,		cVertexType::Float,		3 ); // POSITION
			F.Add( "Normal",	cVertexUsage::Normal,		cVertexType::Float,		3 ); // TEXCOORD0
			F.Add( "Color",		cVertexUsage::Custom,		cVertexType::Byte,		4 ); // TEXCOORD1
			return F;
		}

		static int FormatID;
	}; // PositionNormalColored
	
	//---------------------------------------------------------------------
	// PositionNormalTextured
	//---------------------------------------------------------------------
	struct PositionNormalTextured {
		cVec3 Pos;
		cVec3 Normal;
		cVec2 TexCoord;

		static const Format GetFormat() {
			Format F;
			F.Add( "Pos",		cVertexUsage::Position,		cVertexType::Float,		3 ); // POSITION
			F.Add( "Normal",	cVertexUsage::Normal,		cVertexType::Float,		3 ); // TEXCOORD0
			F.Add( "TexCoord",	cVertexUsage::TexCoord,		cVertexType::Float,		2 ); // TEXCOORD1
			return F;
		}

		static int FormatID;
	}; // PositionNormalTextured

	//---------------------------------------------------------------------
	// PositionOnly
	//---------------------------------------------------------------------
	struct PositionOnly {
		cVec3 Pos;

		static const Format GetFormat() {
			Format F;
			F.Add("Pos", cVertexUsage::Position, cVertexType::Float, 3); // POSITION
			return F;
		}

		static int FormatID;
	}; // PositionOnly

	//-------------------------------------------------------------------------
	// PositionRhw
	//-------------------------------------------------------------------------
	struct PositionRhw {
		cVec4 Pos;
		
		static const Format GetFormat() {
			Format F;
			F.Add("Pos", cVertexUsage::Position, cVertexType::Float, 4); // POSITION
			return F;
		}

		static int FormatID;
	}; // PositionRhw
	
	//---------------------------------------------------------------------
	// PositionTextured
	//---------------------------------------------------------------------
	struct PositionTextured {
		cVec3 Pos;
		cVec2 TexCoord;
		
		static const Format GetFormat() {
			Format F;
			F.Add( "Pos",		cVertexUsage::Position,	cVertexType::Float,	3 ); // POSITION
			F.Add( "TexCoord",	cVertexUsage::TexCoord,	cVertexType::Float,	2 ); // TEXCOORD0
			return F;
		}

		static int FormatID;
	}; // PositionTextured
	
	//-------------------------------------------------------------------------
	// Bump
	//-------------------------------------------------------------------------
	struct Bump {
		cVec3 Pos;
		cVec3 Normal;
		cVec2 TexCoord;
		cVec3 Tangent;
		cVec3 BiTangent;
		// 4 * (3 + 3 + 2 + 3 + 3) = 56 (bytes)
		
		static const Format GetFormat() {
			Format F;
			F.Add( "Pos",		cVertexUsage::Position,		cVertexType::Float,	3 ); // POSITION
			F.Add( "Normal",	cVertexUsage::Normal,		cVertexType::Float,	3 ); // TEXCOORD0
			F.Add( "TexCoord",	cVertexUsage::TexCoord,		cVertexType::Float,	2 ); // TEXCOORD1
			F.Add( "Tangent",	cVertexUsage::Tangent,		cVertexType::Float,	3 ); // TEXCOORD2
			F.Add( "BiTangent", cVertexUsage::BiTangent,	cVertexType::Float,	3 ); // TEXCOORD3
			return F;
		}

		static int FormatID;
	}; // Bump
	
	//-------------------------------------------------------------------------
	// Anim
	//-------------------------------------------------------------------------
	struct Anim {
		cVec3 Pos;
		cVec3 Normal;
		cVec2 TexCoord;
		cVec3 Tangent;
		cVec3 BiTangent;
		cVec4 Indices;
		cVec4 Weights;
		// 4 * (3 + 3 + 2 + 3 + 3 + 4 + 4) = 88 (bytes)

		static const Format GetFormat() {
			Format F;
			F.Add( "Pos",		cVertexUsage::Position,		cVertexType::Float, 3 ); // POSITION
			F.Add( "Normal",	cVertexUsage::Normal,		cVertexType::Float, 3 ); // TEXCOORD0
			F.Add( "TexCoord",	cVertexUsage::TexCoord,		cVertexType::Float, 2 ); // TEXCOORD1
			F.Add( "Tangent",	cVertexUsage::Tangent,		cVertexType::Float,	3 ); // TEXCOORD2
			F.Add( "BiTangent",	cVertexUsage::BiTangent,	cVertexType::Float, 3 ); // TEXCOORD3
			F.Add( "Indices",	cVertexUsage::Indices,		cVertexType::Float,	4 ); // TEXCOORD4
			F.Add( "Weights",	cVertexUsage::Weights,		cVertexType::Float, 4 ); // TEXCOORD5
			return F;
		}
        // ToBump
        void ToBump(Bump *B) {
            B->Pos = Pos;
            B->Normal = Normal;
            B->TexCoord = TexCoord;
            B->Tangent = Tangent;
            B->BiTangent = BiTangent;
        }

		static int FormatID;
	}; // Anim

	//-------------------------------------------------------------------------
	// ShadowVolume
	//-------------------------------------------------------------------------
	struct ShadowVolume {
		cVec3 Pos0;
		cVec3 Pos1;
		cVec3 Pos2;
		cVec4 Indices0;
		cVec4 Weights0;
		cVec4 Indices1;
		cVec4 Weights1;
		cVec4 Indices2;
		cVec4 Weights2;
		// 4 * (3 + 3 + 3 + 4 + 4 + 4 + 4 + 4 + 4) = 4 * (9 + 24) = 132 (bytes)
		
		static const Format GetFormat() {
			Format F;
			F.Add( "Pos0",			cVertexUsage::Position,		cVertexType::Float, 3 ); // POSITION
			F.Add( "Pos1",			cVertexUsage::Custom,		cVertexType::Float, 3 ); // TEXCOORD0
			F.Add( "Pos2",			cVertexUsage::Custom,		cVertexType::Float, 3 ); // TEXCOORD1
			F.Add( "Indices0",		cVertexUsage::Custom,		cVertexType::Float, 4 ); // TEXCOORD2
			F.Add( "Weights0",		cVertexUsage::Custom,		cVertexType::Float,	4 ); // TEXCOORD3
			F.Add( "Indices1",		cVertexUsage::Custom,		cVertexType::Float, 4 ); // TEXCOORD4
			F.Add( "Weights1",		cVertexUsage::Custom,		cVertexType::Float,	4 ); // TEXCOORD5
			F.Add( "Indices2",		cVertexUsage::Custom,		cVertexType::Float, 4 ); // TEXCOORD6
			F.Add( "Weights2",		cVertexUsage::Custom,		cVertexType::Float,	4 ); // TEXCOORD7
			return F;
		};

		static int FormatID;
	};
	
	//-------------------------------------------------------------------------
	// PaddingInfo
	//-------------------------------------------------------------------------
	struct PaddingInfo {
		cVec3 Pos0;
		cVec4 Pos1;
		cVec4 Pos2;
		cVec4 Pos3;
		cVec4 Pos4;
		cVec4 Pos5;
		cVec4 Pos6;
		cVec4 Pos7;
		cVec4 Pos8;
		// 4 * (3 + 4 * 9) = 140 (bytes)

		static const Format GetFormat() {
			Format F;
			F.Add("Pos0", cVertexUsage::Position, cVertexType::Float, 3); // POSITION
			F.Add("Pos1", cVertexUsage::Custom, cVertexType::Float, 4); // TEXCOORD0
			F.Add("Pos2", cVertexUsage::Custom, cVertexType::Float, 4); // TEXCOORD1
			F.Add("Pos3", cVertexUsage::Custom, cVertexType::Float, 4); // TEXCOORD2
			F.Add("Pos4", cVertexUsage::Custom, cVertexType::Float, 4); // TEXCOORD3
			F.Add("Pos5", cVertexUsage::Custom, cVertexType::Float, 4); // TEXCOORD4
			F.Add("Pos6", cVertexUsage::Custom, cVertexType::Float, 4); // TEXCOORD5
			F.Add("Pos7", cVertexUsage::Custom, cVertexType::Float, 4); // TEXCOORD6
			F.Add("Pos8", cVertexUsage::Custom, cVertexType::Float, 4); // TEXCOORD7
			return F;
		};

		static int FormatID;
	};

	static void Register();
};
