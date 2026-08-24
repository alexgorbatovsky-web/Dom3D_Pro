#pragma once

//*****************************************************************************
// cFilter
//*****************************************************************************
struct cFilter {
	enum Enum {
		Nearest			= 0,
		Linear			= 1,
		Bilinear		= 2,
		Trilinear		= 3,
		Bilinear_Aniso	= 4,
		Trilinear_Aniso	= 5
	};

	static bool HasMipMaps(const Enum Filter) {
		return Filter >= Bilinear;
	}

	static bool HasAniso(const Enum Filter) {
		return Filter >= Bilinear_Aniso;
	}
	static const char * ToString(const Enum Filter);
};

//*****************************************************************************// cAddressMode
//*****************************************************************************
struct cAddressMode {
	enum Enum {
		Wrap	= 0,
		Clamp	= 1,
		Mirror	= 2
	};
	static const char * ToString(const Enum AddressMode);
};

//*****************************************************************************
// cTopology
//*****************************************************************************
struct cTopology {
	enum Enum {
		PointList = 0,
		LineList = 1,
		LineStrip = 2,
		TriangleList = 3,
		TriangleStrip = 4,
		TrianglePatches = 5,
		QuadPatches = 6,
		Count = 7
	};

	static int PrimitiveCount(const Enum Topology, const int IndexCount) {
		switch(Topology) {
			case PointList:
				return IndexCount;
			case LineList:
				return IndexCount / 2;
			case LineStrip:
				return IndexCount - 1;
			case TriangleList:
				return IndexCount / 3;
			case TriangleStrip:
				return IndexCount - 2;
			case TrianglePatches:
				return IndexCount / 3;
			case QuadPatches:
				return IndexCount / 4;
			default:
				return 0;
		}
	}
};

//*****************************************************************************
// cBlendFactor
//*****************************************************************************
struct cBlendFactor {
	enum Enum {
		Zero = 0,					// (0, 0, 0, 0)
		One = 1,					// (1, 1, 1, 1)
		SrcColor = 2,				// (Rs, Gs, Bs, As)
		OneMinusSrcColor = 3,		// (1 - Rs, 1 - Gs, 1 - Bs, 1 - As)
		SrcAlpha = 4,				// (As, As, As, As)
		OneMinusSrcAlpha = 5,		// (1 - As, 1 - As, 1 - As, 1 - As)
		DstAlpha = 6,				// (Ad, Ad, Ad, Ad)
		OneMinusDstAlpha = 7,		// (1 - Ad, 1 - Ad, 1 - Ad, 1 - Ad)
		DstColor = 8,				// (Rd, Gd, Bd, Ad)
		OneMinusDstColor = 9,		// (1 - Rd, 1 - Gd, 1 - Bd, 1 - Ad)
		SrcAlphaSat = 10			// (f, f, f, 1); f = Min(As, 1 - Ad)
	};
}; // cBlendFactor

//*****************************************************************************
// cBlendMode
//*****************************************************************************
struct cBlendMode {
	enum Enum {
		Add = 0,
		Subtract = 1,
		ReverseSubtract = 2,
		Min = 3,
		Max = 4
	};
}; // cBlendMode

//*****************************************************************************
// cBlendMask
//*****************************************************************************
struct cBlendMask {
	enum Enum {
		Red = 1,
		Green = 2,
		Blue = 4,
		Alpha = 8,
		RedGreenBlue = Red | Green | Blue,
		RedGreenBlueAlpha = Red | Green | Blue | Alpha,
		None = 0
	};
}; // cBlendMask

//*****************************************************************************
// cDepthFunc
//*****************************************************************************
struct cDepthFunc {
	enum Enum {
		Never = 0,
		Less = 1,
		Equal = 2,
		LessEqual = 3,
		Greater = 4,
		NotEqual = 5,
		GreaterEqual = 6,
		Always = 7
	};
}; // cDepthFunc

//*****************************************************************************
// cCullMode
//*****************************************************************************
struct cCullMode {
	enum Enum {
		None = 0,
		Clockwise = 1,
		CounterClockwise = 2
	};
}; // cCullMode

//*****************************************************************************
// cRenderType
//*****************************************************************************

struct cRenderType {
	enum Enum {
		None = 0,
		OpenGL = 1,
		DirectX = 2
	};
}; // cRenderType

// cFont
struct cFont {
	int CellAscent;
	int CellDescent;
	int LineSpacing;
	struct Glyph {
		word Char;
		cRect TexCoord;
		cVec2i Origin;
		int SizeX;
		int SizeY;
		cVec2i CellInc;
		Glyph() {
			Char = 0;
			TexCoord.SetZero();
			Origin.Set(0);
			SizeX = 0;
			SizeY = 0;
			CellInc.Set(0);
		}
		static int Compare(const Glyph *l, const Glyph *r) {
			if(l->Char > r->Char) {
				return 1;
			} else if(l->Char < r->Char) {
				return -1;
			} else {
				return 0;
			}
		}
	};
	cList<Glyph> Glyphs;
	// Border (in pixels) around each glyph's black box on texture made by font generator.
	// It's needed for elimination of influence of neighboring glyphs when scale is applied to drawing string.
	// Border value is taking into account during tight rect calculations.
	int GlyphBorder;
	cFont() {
		CellAscent = 0;
		CellDescent = 0;
		LineSpacing = 0;
		GlyphBorder = 0;
	}
	bool Read(const cFile &From);
	void Write(cFile *To) const;
};

/***************************************************************************
// cRender_Font
******************************************************************************/
struct cRender_FONT {
	comms::cStr Name;
	cFont Format;
	int TextureID;
};
typedef struct cRender_FONT cRender_FONT;
//*****************************************************************************
// cRender
//*****************************************************************************
class cRender {
public:
	class Stub;

	static Stub * GetInstance();
	static bool _NeedUpdate;
	static void NeedUpdate() { _NeedUpdate = true; }
	static const cRenderType::Enum GetType();
	static bool GetFlipRenderTarget();
	static int GetMRTCount(); // >= 2 - hardware supports MRT, otherwise it doesn't
	static bool SupportsFormat(const cFormat::Enum Format);
	static bool ScreenShot(cImage *To);
    static int PixelsPerPoint;
	
	// When "true" device supports 2D textures with dimensions that are not powers of two at least with these conditions:
	// 1. texture addressing mode is "cAddressMode::Clamp"
	// 2. mipmapping is not in use
	// 3. texture format is not DXT
	static bool SupportsNonPowerOfTwo();
	
	// May be "true" only for DirectX renderer, and is "true" only one frame after device reset
	// This should be used to clear render targets for which previous content is critical (motion blur, water physics, etc.)
	static bool GetDeviceRecentReset();
	
	static bool Init(const int MaxSamples = 0); // MaxSamples = [0, 32], "0" - to disable multisampling, "-1" - to use max level
	static void Free();
	static void ResetStates();

	// Sets view, projection matrices, and viewport
	// After that updates current shader autoconstants
	static void SetViewer(cViewer *);
	static cViewer * GetViewer();
	
	static void SetWorldMatrix(const cMat4 &);
	static const cMat4 & GetWorldMatrix();
	static const cMat4 & GetWorldMatrixInverse();	// Are calculated and cached
	static const cMat3 & GetNormalMatrix();			// only on demand
	static void PushWorldMatrix();
	static void PopWorldMatrix();

	static void SetTextureMatrix(const cMat4 &, const int Index = 0);
	static const cMat4 & GetTextureMatrix(const int Index = 0);
	
	//-------------------------------------------------------------------------
	// ClipRect
	//-------------------------------------------------------------------------
	static void SetClipRect(const cRect &); // Set "cRect::Empty" to disable clipping
	static const cRect & GetClipRect();
	static cList<comms::cRender_FONT>& GetRenderFonts();
	static void PushClipRect();
	static void PopClipRect();
	
	static const cStr& GetVendor();
	static const cStr& GetVersion();
	static const cStr& GetRenderer();

	//*************************************************************************
	// SamplerState
	//*************************************************************************
	static int GetSamplerStateID(const cFilter::Enum Filter, const cAddressMode::Enum str) {
		return GetSamplerStateID(Filter, str, str, str);
	}
	static int GetSamplerStateID(const cFilter::Enum Filter, const cAddressMode::Enum s, const cAddressMode::Enum t, const cAddressMode::Enum r);
	static void SetSamplerState(const int TextureID, const int SamplerStateID);
	
	//*************************************************************************
	// Texture
	//*************************************************************************
	static int GetTextureID(const char *FilePn, const int SamplerStateID = -1); // "SamplerStateID = -1" means,
	static int AddTexture(const cImage &Image, const int SamplerStateID = -1); // that it will be specified later as
	static int AddTexture(const char *FilePn, const int SamplerStateID = -1); // argument of "cRender::SetTexture()"
	static bool GetExpandTexturesToPowerOfTwo();
	static void SetExpandTexturesToPowerOfTwo(const bool Value);
	static void UpdateTexture(const int TextureID, const cImage &Image, const cList<cRect> &Rects);
	static void UpdateTexture(const int TextureID, const cList<cImage *> &SubImages, const cList<cRect> &Rects);
	static void UpdateTexture(const int TextureID, const cImage& Image);
	static void ReloadTextures();
	static bool ReloadTexture(const int TextureID);
	static void ReloadTexture(const char *FilePn);
	static void UnloadTextures();
	static void FreeTexture(const int TextureID);
	static const cStr * GetTextureFilePn(const int TextureID);
	static int GetTextureWidth(const int TextureID);
	static int GetTextureHeight(const int TextureID);
	static const cRect GetTextureRect(const int TextureID) {
		return cRect(cRect::SetCtor, 0.0f, 0.0f, (float)GetTextureWidth(TextureID), (float)GetTextureHeight(TextureID));
	}
	static int GetTextureDepth(const int TextureID);
	static const cDimension::Enum GetTextureDimension(const int TextureID) {
		return cDimension::ToDimension(GetTextureWidth(TextureID), GetTextureHeight(TextureID), GetTextureDepth(TextureID));
	}
	static const cFormat::Enum GetTextureFormat(const int TextureID);
	static int GetTextureMipMapCount(const int TextureID); // >= 1
	static bool GetTextureRenderTargetUsage(const int TextureID);

	//*************************************************************************
	// Render target
	//*************************************************************************
	static int AddRenderTarget(const int Width, const int Height, const cFormat::Enum Format, const int SamplerStateID = -1, const bool CubeMap = false, const int Samples = 1);
	static int AddRenderDepth(const int RenderTargetID);
	static void SetRenderTargetSize(const int RenderTargetID, const int Width, const int Height);
	static void PushRenderTarget(const int ColorRenderTargetID, const int DepthRenderTargetID = -1, const int Face = 0, const bool SaveAspect = false) {
        PushRenderTarget(&ColorRenderTargetID, 1, DepthRenderTargetID, &Face, SaveAspect);
	}
	static void PushRenderTarget(const int *ColorRenderTargetIDs, const int ColorRenderTargetCount, const int DepthRenderTargetID = -1, const int *Faces = nullptr, const bool SaveAspect = false);
	static void PopRenderTarget();
	static bool SaveRenderTarget(const int RenderTargetID, cImage *To);
    struct SaveRenderTargetArgs {
        // In
        byte *Buffer; // "nullptr" to fill the "Out" fields below or a pointer to already allocated buffer to read pixels from the render target
        size_t BufferSize; // Only when "Buffer != nullptr"
		// Rect
		int X, Y, Width, Height; // "X" and "Y" are used when "Width > 0 && Height > 0"
        // Out
        cFormat::Enum Format; // These fields are filled inside "SaveRenderTargetRaw" even when "nullptr == Buffer"
        size_t RequiredSize; // How many memory we should allocate for "Buffer"
        SaveRenderTargetArgs() {
            Clear();
        }
        void Clear() {
            memset(this, 0, sizeof(SaveRenderTargetArgs));
        }
    };
    // macOS 2.2-2.4/Windows 1.5/Linux 1.0 times faster than "SaveRenderTarget"
    static bool SaveRenderTargetRaw(const int RenderTargetID, SaveRenderTargetArgs *Args);
	static void RequestSaveRenderTargetAsync(const int RenderTargetID);
	static bool GetSaveRenderTargetAsync(const int RenderTargetID, cImage *To);
	static void FreeSaveRenderTargetAsync(const int RenderTargetID);
	static bool IsSaveRenderTargetAsyncReady(const int RenderTargetID);
    static bool RequestSaveRenderTargetAsyncRaw(const int RenderTargetID, SaveRenderTargetArgs *Args);
    static bool GetSaveRenderTargetAsyncRaw(const int RenderTargetID, SaveRenderTargetArgs *Args);
    static void FreeSaveRenderTargetAsyncRaw(const int RenderTargetID);
    static bool IsSaveRenderTargetAsyncReadyRaw(const int RenderTargetID);
	
	//		With "SaveAspect" it is possible to achieve different effects. For example, if you want to
	// render a 3d scene into the render target for post processing, then you should save aspect.
	// This guarantees the same scene look independently of the render target aspect.
	//		"PushRenderTarget" besides pushes current viewer state and sets its viewport to the size
	// of the new render target, also it pushes clip rect and disables clipping.
	// "PopRenderTarget" pops viewer state to restores previous viewport, and pops clip rect. So you should
	// avoid overlapping of "(Push/Pop)RenderTarget()" with "(Push/Pop)ClipRect" and "GetViewer()->(Push/Pop)State()".
	//		"SetRenderTargetSize" doesn't change viewport, even if it was overridden, so
	// it should be called before "SetRenderTarget".
	//		You can access render target dimensions through the "g_ViewportInvWidth" and
	// "g_ViewportInvHeight" shader auto constants.
	
	//*************************************************************************
	// Shader
	//*************************************************************************
	static int GetShaderID(const char *ShaderName, const int VertexFormatID, const char *Extra = nullptr, cStr *OptionalLog = nullptr);
	static int AddShader(const char *ShaderName, const int VertexFormatID, const char *Extra = nullptr);
	static void SetShader(const int ShaderID);
	static void FreeShader(const int ShaderID);
	static void ReloadShaders();
	static void ReplaceInShaders(const char *What, const char *With);
	
	static int GetShaderConstID(const int ShaderID, const char *ConstName, int* OptionalPtrToIDForReload = nullptr);
	static void SetShaderConst(const int ConstID, const int Value);
	static void SetShaderConst(const int ConstID, const float Value);
	static void SetShaderConst(const int ConstID, const cVec2 &Value);
	static void SetShaderConst(const int ConstID, const cVec3 &Value);
	static void SetShaderConst(const int ConstID, const cVec4 &Value);
	static void SetShaderConst(const int ConstID, const cColor &Value);
	static void SetShaderConst(const int ConstID, const cRect &Value);
	static void SetShaderConst(const int ConstID, const cMat3 &Value);
	static void SetShaderConst(const int ConstID, const cMat4 &Value);
	static void SetShaderConst(const int ConstID, const float *Array, const int Count);
	static void SetShaderConst(const int ConstID, const cVec2 *Array, const int Count);
	static void SetShaderConst(const int ConstID, const cVec3 *Array, const int Count);
	static void SetShaderConst(const int ConstID, const cVec4 *Array, const int Count);
	static void SetShaderConst(const int ConstID, const cColor *Array, const int Count);
	/**
	\brief "OptionalPtrToIDForReload" updates shader's sampler/const ID after "ReloadShaders"
	\code{.cpp}
		static int ID = cRender::GetShaderID("name", cVertex::PositionTextured::FormatID);
		static int Sampler0 = cRender::GetSamplerID(ID, "s_Sampler0", &Sampler0);
		static int Sampler1 = cRender::GetSamplerID(ID, "s_Sampler1", &Sampler1);
		static int Coeff0 = cRender::GetShaderConstID(ID, "c_Coeff0", &Coeff0);
		static int Coeff1 = cRender::GetShaderConstID(ID, "c_Coeff1", &Coeff1);
	\endcode
	*/
	static int GetSamplerID(const int ShaderID, const char *SamplerName, int *OptionalPtrToIDForReload = nullptr);
	static void SetTexture(const int SamplerID, const int TextureID, const int SamplerStateID = -1);
	
	// Bindless texture
	static bool IsSupported_BindlessTexture();
	
	static int AddVertexFormat(const cVertex::Format &Format);
	static int GetVertexSize(const int VertexFormatID);
	
	static int AddVertexBuffer(const void *Data, const size_t Size);
	static int AddIndexBuffer(const void *Data, const int IndexCount, const int IndexSize = 4);

	static void FreeVertexBuffer(const int VertexBufferID);
	static void FreeIndexBuffer(const int IndexBufferID);

	static void SetVertexBuffer(const int VertexBufferID);
	static void SetIndexBuffer(const int IndexBufferID);
	
	static void DrawArrays(const cTopology::Enum Topology);
	static void DrawArrays(const cTopology::Enum Topology, const void *VertexData, const int VertexCount);
	static void DrawIndexed(const cTopology::Enum Topology);
	static void DrawIndexed(const cTopology::Enum Topology, const void *VertexData, const int VertexCount, const void *IndexData, const int IndexCount, const int IndexSize = 4);
	
	struct AutoConst {
		enum Enum {
			ViewportWidth = 0,				// "g_ViewportWidth"				float
			ViewportHeight = 1,				// "g_ViewportHeight"				float
			ViewportInvWidth = 2,			// "g_ViewportInvWidth"				float
			ViewportInvHeight = 3,			// "g_ViewportInvHeight"			float
			WorldMatrix = 4,				// "g_WorldMatrix"					cMat4
			WorldMatrixInverse = 5,			// "g_WorldMatrixInverse"			cMat4
			ViewerPos = 6,					// "g_ViewerPos"					cVec3
			WorldViewMatrix = 7,			// "g_WorldViewMatrix"				cMat4
			ViewProjectionMatrix = 8,		// "g_ViewProjectionMatrix"			cMat4
			WorldViewProjectionMatrix = 9,	// "g_WorldViewProjectionMatrix"	cMat4
			ProjectionMatrix = 10,			// "g_ProjectionMatrix"				cMat4
			ScreenMatrix = 11,				// "g_ScreenMatrix"					cMat4
			ScreenMatrixInverse = 12,		// "g_ScreenMatrixInverse"			cMat4
			// Transpose of the inverse of the upper leftmost 3x3 of "g_WorldMatrix"
			NormalMatrix = 13,				// "g_NormalMatrix"					cMat3
			NormalViewMatrix = 14,			// "g_NormalViewMatrix"				cMat3
			TimeSec = 15,					// "g_TimeSec"						float
			FrameTimeSec = 16,				// "g_FrameTimeSec"					float
			TextureMatrix0 = 17,			// "g_TextureMatrix0"				cMat4
			TextureMatrix1 = 18,			// "g_TextureMatrix1"				cMat4
			TextureMatrix2 = 19,			// "g_TextureMatrix2"				cMat4
			TextureMatrix3 = 20				// "g_TextureMatrix3"				cMat4
		};
		
		static const char * ToString(const int);
		static int FromString(const char *);
	};

	//-------------------------------------------------------------------------
	// Blend
	//-------------------------------------------------------------------------
	static int GetBlendStateID(const cBlendFactor::Enum SrcFactor, const cBlendFactor::Enum DstFactor, const cBlendMode::Enum Mode = cBlendMode::Add, const dword Mask = cBlendMask::RedGreenBlueAlpha);
	static void SetBlendState(const int BlendStateID); // Pass "-1" to disable blend and set the mask to "All"
	static void PushBlendState();
	static void PopBlendState();

	//-------------------------------------------------------------------------
	// Depth
	//-------------------------------------------------------------------------
	static int GetDepthStateID(const bool TestEnabled, const bool WriteEnabled, const cDepthFunc::Enum Func = cDepthFunc::LessEqual);
	static void SetDepthState(const int DepthStateID); // "-1" to disable depth test / write
	static void PushDepthState();
	static void PopDepthState();
	
	//-------------------------------------------------------------------------
	// Rasterizer
	//-------------------------------------------------------------------------
	static void SetCullMode(const cCullMode::Enum CullMode);
	static void PushCullMode();
	static void PopCullMode();

	static void SetWireframe(const bool Enabled);
	static bool GetWireframe();
	
	static void Clear(const bool ClearColor, const bool ClearDepth, const cColor &Color = cColor(0.63f, 0.63f, 0.63f, 1.0f), const float Depth = 1.0f);
	
	// Calls to "cRender" can work outside "BeginFrame/EndFrame" pair, but
	// render targets fill for DirectX should be between "BeginFrame/EndFrame"
	static void BeginFrame();
	static void EndFrame();

	static void Finish();

	static int GetFontID(const char *FontName);
	static const cFont * GetFont(const int FontID);
	static int GetFontTextureID(const int FontID);
	static void EnsurePrintable(const int FontID, cStr *Text);
	
	//-------------------------------------------------------------------------
	// StringAlign
	//-------------------------------------------------------------------------
	struct StringAlign {
		enum Flags {
			// Horizontal string alignment flags
			Left = 0, Center = 1, Right = 2,
			// Vertical string alignment flags
			Top = 0, Middle = 4, BaseLine = 8, Bottom = 16,

			// Predefined list of possible reference point locations
			TopLeft			= Top | Left,		TopCenter		= Top | Center,			TopRight		= Top | Right,
			MiddleLeft		= Middle | Left,	MiddleCenter	= Middle | Center,		MiddleRight		= Middle | Right,
			BaseLineLeft	= BaseLine | Left,	BaseLineCenter	= BaseLine | Center,	BaseLineRight	= BaseLine | Right,
			BottomLeft		= Bottom | Left,	BottomCenter	= Bottom | Center,		BottomRight		= Bottom | Right,
			
			// Auxiliary flags
			CalcRect = 32, // "DrawString" only measures string and returns bounding rect without drawing
			TightRect = 64 // Bounding rectangle evaluation technique
			
			// With "TightRect" flag bounding rect for string will be based on actual font glyphs
			// rather than font cells.
			// Without flag "TightRect" function "DrawString" will calculate string bounding rectangle
			// in the same way as Win32 API function "GetTextExtentPoint32".
		};
	};

	//---------------------------------------------------------------------------------------------
	// DrawString
	//---------------------------------------------------------------------------------------------
	static const cRect DrawString(const int FontID, const char *Str, const float X, const float Y);
	static const cRect DrawString(const int FontID, const char *Str, const float X, const float Y, const int Align);
	static const cRect DrawString(const int FontID, const char *Str, const float X, const float Y, const cColor &Color);
	static const cRect DrawString(const int FontID, const char *Str, const float X, const float Y, const int Align, const cColor &Color);
	static const cRect DrawString(const int FontID, const char *Str, const cVec2 &Pos);
	static const cRect DrawString(const int FontID, const char *Str, const cVec2 &Pos, const int Align);
	static const cRect DrawString(const int FontID, const char *Str, const cVec2 &Pos, const cColor &Color);
	static const cRect DrawString(const int FontID, const char *Str, const cVec2 &Pos, const int Align, const cColor &Color);
	static const cRect DrawString(const int FontID, const char *Str, const cVec3 &Pos);
	static const cRect DrawString(const int FontID, const char *Str, const cVec3 &Pos, const int Align);
	static const cRect DrawString(const int FontID, const char *Str, const cVec3 &Pos, const cColor &Color);
	static const cRect DrawString(const int FontID, const char *Str, const cVec3 &Pos, const int Align, const cColor &Color);
	static const cRect DrawString(const int FontID, const char *Str, const float X, const float Y, const int Align, const cColor *OverrideColor);
	static const cRect DrawString(const int FontID, const cList<word> &UniChars, const float X, const float Y, const int Align, const cColor *OverrideColor);
	
	//---------------------------------------------------------------------------------------------
	// MeasureString...
	//---------------------------------------------------------------------------------------------
	static const cRect MeasureStringTightRect(const int FontID, const char *Str);
	static const cRect MeasureStringCells(const int FontID, const char *Str); // The same as Win32 API function "GetTextExtentPoint32"
	
	static void DrawLine(const int X0, const int Y0, const int X1, const int Y1, const cColor &Color = cColor::Black);
	static void DrawLine(const float X0, const float Y0, const float X1, const float Y1, const cColor &Color = cColor::Black);
	static void DrawLine(const cVec2 &P0, const cVec2 &P1, const cColor &Color = cColor::Black);
	static void DrawLine(const cVec3 &P0, const cVec3 &P1, const cColor &Color);
	static void DrawNormals(const cList<cVec3>& Centers, const cList<cVec3>& Normals, const cColor& Color, const float Length);
	
	//-------------------------------------------------------------------------
	// DrawRectangle
	//-------------------------------------------------------------------------
	static void DrawRectangle(const int X, const int Y, const int Width, const int Height, const cColor &Color = cColor::Black);
	static void DrawRectangle(const float X, const float Y, const float Width, const float Height, const cColor &Color = cColor::Black);
	static void DrawRectangle(const cRect &rc, const cColor &Color = cColor::Black);

	// DrawBounds
	static void DrawBounds(const cBounds &bs, const cColor &Color = cColor::Black);

	//-------------------------------------------------------------------------
	// DrawRectangle(s)Dot
	//-------------------------------------------------------------------------
	static void DrawRectangleDot(const cRect &Rc, const cColor &Color);
	static void DrawRectanglesDot(const cRect *Rects, const int Count, const cColor &Color);
	
	//-------------------------------------------------------------------------
	// FillRectangle(s)Solid
	//-------------------------------------------------------------------------
	static void FillRectangleSolid(const int X, const int Y, const int Width, const int Height, const cColor &Color);
	static void FillRectangleSolid(const float X, const float Y, const float Width, const float Height, const cColor &Color);
	static void FillRectangleSolid(const cRect &Rc, const cColor &Color);
	static void FillRectanglesSolid(const cRect *Rects, const int Count, const cColor &Color, const int CustomShader = -1);

	//-------------------------------------------------------------------------
	// FillRectangleDither
	//-------------------------------------------------------------------------
	static void FillRectangleDither(const cRect &Rc, const cVec2 &Orig, const cColor &Color0, const cColor &Color1);

	//--------------------------------------------------------------------------
	// FillRectangleTextured
	//--------------------------------------------------------------------------
	struct FillRectangleTexturedArgs {
        int DepthState;
		float Z;
		float Alpha; // If "Alpha = -1.0f" the texture's alpha will be used, otherwise alpha will be overridden
		bool AlphaChannelAsColor;
		int OverrideSamplerStateID;
		int CubeFace; // "-1" - unfolded cube, [0..5] = { PosX, NegX, PosY, NegY, PosZ, NegZ }
		FillRectangleTexturedArgs() {
			Clear();
		}
		void Clear() {
            DepthState = -1;
			Z = 0.0f;
			Alpha = 1.0f;
			AlphaChannelAsColor = false;
			OverrideSamplerStateID = -1;
			CubeFace = -1;
		}
	};
	static void FillRectangleTextured(const cRect &Rc, const int TextureID, const FillRectangleTexturedArgs &Args);
	static void FillRectangleTextured(const cRect &Rc, const int TextureID) {
		FillRectangleTexturedArgs fr;
		FillRectangleTextured(Rc, TextureID, fr);
	}

	//-------------------------------------------------------------------------
	// Linear gradient style
	//-------------------------------------------------------------------------
	struct LinearGradient {
		enum Enum {
			Horizontal, Vertical, ForwardDiagonal, BackwardDiagonal
		};
	};

	//-------------------------------------------------------------------------
	// FillRectangle(s)LinearGradient
	//-------------------------------------------------------------------------
	static void FillRectangleLinearGradient(const int X, const int Y, const int Width, const int Height, const cColor &Color0, const cColor &Color1, const LinearGradient::Enum Mode);
	static void FillRectangleLinearGradient(const float X, const float Y, const float Width, const float Height, const cColor &Color0, const cColor &Color1, const LinearGradient::Enum Mode);
	static void FillRectangleLinearGradient(const cRect &Rc, const cColor &Color0, const cColor &Color1, const LinearGradient::Enum Mode);
	static void FillRectanglesLinearGradient(const cRect *Rects, const int Count, const cColor &Color0, const cColor &Color1, const LinearGradient::Enum Mode);
	
	//-------------------------------------------------------------------------
	// Radial gradient style
	//-------------------------------------------------------------------------
	struct RadialGradient {
		enum Flags {
			// Flags for segment ends style
			RoundFrom = 0, CaveFrom = 1, SharpFrom = 2, FlatFrom = 4,
			RoundTo = 0, CaveTo = 8, SharpTo = 16, FlatTo = 32,

			// Predefined list of possible styles of ends
			RoundRound = RoundFrom | RoundTo,
			RoundCave = RoundFrom | CaveTo,
			RoundSharp = RoundFrom | SharpTo,
			RoundFlat = RoundFrom | FlatTo,
			
			CaveRound = CaveFrom | RoundTo,
			CaveCave = CaveFrom | CaveTo,
			CaveSharp = CaveFrom | SharpTo,
			CaveFlat = CaveFrom | FlatTo,
			
			SharpRound = SharpFrom | RoundTo,
			SharpCave = SharpFrom | CaveTo,
			SharpSharp = SharpFrom | SharpTo,
			SharpFlat = SharpFrom | FlatTo,
			
			FlatRound = FlatFrom | RoundTo,
			FlatCave = FlatFrom | CaveTo,
			FlatSharp = FlatFrom | SharpTo,
			FlatFlat = FlatFrom | FlatTo,
			
			// Draw only half to the specified segment side
			HalfCcwSharp = 64, HalfCcwFlat = 128,
			HalfCwSharp = 256, HalfCwFlat = 512
		};
	};

	//-------------------------------------------------------------------------
	// Fill(Segment, Rectangle)RadialGradient
	//-------------------------------------------------------------------------
	// Since "Style" determines the shape, it goes before colors in arguments list.
	
	// As segments with arbitrary orientation were too slow because point projection function on
	// line segment with arbitrary orientation in pixel shader, some limitations have been introduced:
	// 1. segments should be horizontal or vertical, but rotations, of course, can be done through world matrix;
	// 2. "From" should be less than "To" (i.e. "From" is minimum, "To" is maximum).
	// Such efforts have increased performance of main menu for "Fragile" theme from 240 to 372 FPS.
	
	static void FillSegmentRadialGradient(const cVec2 &From, const cVec2 &To, const float SegRadius, const float GradRadius, const float Power, const int Style, const cColor &CenterColor, const cColor &BoundColor);
	static void FillSegmentRadialGradient(const cRect &BoundingRect, const float GradRadius, const float Power, const int Style, const cColor &CenterColor, const cColor &BoundColor); // If "GradRadius" is zero, it will be the same as radius calculated based on bounding rect
	static void FillRectangleRadialGradient(const cRect &BoundingRect, const float SegRadius, const float GradRadius, const float Power, const cColor &CenterColor, const cColor &BoundColor);

	//---------------------------------------------------------------------------------------------
	// FillCircleRadialGradient
	//---------------------------------------------------------------------------------------------
	static void FillCircleRadialGradient(const int X, const int Y, const int Radius, const int GradRadius, const int Power, const cColor &CenterColor, const cColor &BoundColor);
	static void FillCircleRadialGradient(const float X, const float Y, const float Radius, const float GradRadius, const float Power, const cColor &CenterColor, const cColor &BoundColor);
	static void FillCircleRadialGradient(const cVec2 &Pos, const float Radius, const float GradRadius, const float Power, const cColor &CenterColor, const cColor &BoundColor);

	static void DrawNoPreview(const cRect &Bounds, const cColor &Color);

	static void DrawCaveCaveCorner(const cRect &BoundingQuad, const float GradRadius, const float Power, const cColor &CenterColor, const cColor &BoundColor);
	
	struct IconState {
		enum Enum {
			Normal, Disabled, Hot
		};
	};
	static void DrawIcon(const cRect &Rc, const int ToolBarID, const int Index, const IconState::Enum State, const float Alpha, const int IconWidth, const int IconHeight);
	static void DrawCircle(const cVec2 &Pos, const float Radius, const float StartAngle, const float SweepAngle, const cColor &Color, const int SubDivs = 12);

	//-----------------------------------------------------------------------------------------------
	// DrawBillboardQuad(s)
	//-----------------------------------------------------------------------------------------------
	static void DrawBillboardQuadsSolid(const cVec3 *Centers, const int Count, const float Side, const cColor &Color);
	static void DrawBillboardQuadSolid(const cVec3 &Center, const float Side, const cColor &Color) {
		DrawBillboardQuadsSolid(&Center, 1, Side, Color);
	}

	//-------------------------------------------------------------------------
	// Helpers
	//-------------------------------------------------------------------------
	// Current vertex format should be "cVertex::PositionColored"
	static void FillCirclesColoredCurShader(const cVec2 *Pos, const float *Radius, const int Count, const cColor &Color, const float StartAngle = 0.0f, const float SweepAngle = 360.0f, const int SubDivs = 12);
    
	// Current vertex format should be "cVertex::PositionTextured"
    struct FillRectangleTexturedCurShaderArgs {
        bool Flip;
        float Z;
        float t0;
        float t1;
        FillRectangleTexturedCurShaderArgs() {
            Clear();
        }
        void Clear() {
            Flip = false;
            Z = 0.0f;
            t0 = 0.0f;
            t1 = 1.0f;
        }
    };
	static void FillRectangleTexturedCurShader(const cRect &Rc, const FillRectangleTexturedCurShaderArgs &Args);
    static void FillRectangleTexturedCurShader(const cRect &Rc) {
        const FillRectangleTexturedCurShaderArgs Args;
        FillRectangleTexturedCurShader(Rc, Args);
    }
	// Current vertex format should be "cVertex::PositionNormal"
	static void FillRectangleCubeCurShader(const cRect &Rc, const float Z, const cFrustum &Frustum);
	
	//-------------------------------------------------------------------------
	// FillCircleSolid
	//-------------------------------------------------------------------------
	static void FillCircleSolid(const int X, const int Y, const int Radius, const cColor &Color, const int StartAngle = 0, const int SweepAngle = 360, const int SubDivs = 12);
	static void FillCircleSolid(const float X, const float Y, const float Radius, const cColor &Color, const float StartAngle = 0.0f, const float SweepAngle = 360.0f, const int SubDivs = 12);
	static void FillCirclesSolid(const cVec2 *Pos, const float *Radius, const int Count, const cColor &Color, const float StartAngle = 0.0f, const float SweepAngle = 360.0f, const int SubDivs = 12);
	static void FillCirclesSolid(const cVec2 *Pos, const float Radius, const int Count, const cColor &Color, const float StartAngle = 0.0f, const float SweepAngle = 360.0f, const int SubDivs = 12) {
		static cList<float> Radiuses;
		Radiuses.SetCount(Count, Radius);
		FillCirclesSolid(Pos, Radiuses.ToPtr(), Count, Color, StartAngle, SweepAngle, SubDivs);
	}
	static void FillCircleSolid(const cVec2 &Pos, const float Radius, const cColor &Color, const float StartAngle = 0.0f, const float SweepAngle = 360.0f, const int SubDivs = 12) {
		FillCirclesSolid(&Pos, &Radius, 1, Color, StartAngle, SweepAngle, SubDivs);
	}

	static void DrawCircle(const cVec3 &Center, const float Radius, const cColor &Color, const cVec3 *OverrideAxis = nullptr);
	static void DrawCircleZ(const cVec3 &Center, const float Radius, const cColor &Color) {
		DrawCircle(Center, Radius, Color, &cVec3::AxisZ);
	}
	static void FillCircleSolid(const cVec3 &Center, const float Radius, const cColor &Color);

	static void FillCirclesCurShader(const cVec3 *Pos, const float *Radius, const int Count, const cVec3 &Axis, const int SubDivs = 12);
	
	static void DrawFacingCircle(const cVec3 &Center, const float Radius, const cVec3 &Axis, const cColor &Color, const int SubDivs = 12);
	static void DrawConeSolid(const cVec3 &Apex, const cVec3 &Axis, const float Radius, const float Length, const cColor &Color, const int Sides = 20);
	static void DrawConeWire(const cVec3 &Apex, const cVec3 &Axis, const float Radius, const float Length, const cColor &Color, const int Sides = 20);
	static void DrawSector(const cVec3 &Center, const float Radius, const cVec3 &Fm, const cVec3 &To, const cColor &BorderColor, const cColor &FillColor, const float dAngle = 5.0f);
	static void DrawCubeSolid(const cVec3 &Center, const float Side, const cColor &Color, const cQuat &q = cQuat::Identity);
	static void DrawCubeColored(const float Size);
	static void DrawCubeWire(const cVec3 &Center, const float Side, const cColor &Color);
	static void DrawCubesCurShader(const cVec3 *Centers, const float *Sides, const int Count); // cVertex::PositionOnly
	static void DrawSphereWire(const cVec3 &Center, const float Radius, const cColor &Color, const int Sections = 20, const int Slices = 20, const cVec3 &Axis = cVec3::AxisY);
	static void DrawCrossHair(const cVec3 &Center, const float Radius, const cColor &Color);
	
	struct GridArgs {
		// Size:
		float Size;				// Units, >= 1.0f
		float UpAxisLength;		// Units, >= cMath::Epsilon
		float GridLinesEvery;	// Units, >= cMath::Epsilon
		int Subdivisions;		// Count, >= 1
		// Appearance:
		bool ShowAxes;
		bool ShowGridLines;
		bool ShowSubdivisionLines;
		// Custom View Projection Matrix
		bool OverrideViewProjectionMatrix;
		cMat4 ViewProjectionMatrix;

		GridArgs() { SetDefaults(); }
		void SetDefaults();
		void Validate();
	};
	static void DrawGrid(GridArgs = GridArgs());

	static void DrawCamera(const cColor &Color); // Set world matrix for positioning

	static void DrawFrustumWire(const cFrustum &Frustum, const cColor &Color);
	static void DrawFrustumWireCurShader(const cFrustum &Frustum); // cVertex::PositionOnly
	static void DrawFrustumSolidCurShader(const cFrustum &Frustum); // cVertex::PositionOnly
	
	static void PrecompileShaders(const int Platform); // 0 - PC, 1 - Xbox 360

	class Stub {
	public:
		virtual ~Stub() {}
		virtual const cRenderType::Enum GetType() = 0;
		virtual bool Init(const int MaxSamples) = 0;
		virtual void Free() = 0;
		virtual void SetViewport(const cRect &Viewport) = 0;
		virtual void SetClipRect(const cRect &ClipRect) = 0;
		
		virtual const cStr& GetVendor() const = 0;
		virtual const cStr& GetVersion() const = 0;
		virtual const cStr& GetRenderer() const = 0;
		virtual int GetMRTCount() = 0;
		virtual bool SupportsFormat(const cFormat::Enum Format) = 0;
		virtual bool SupportsNonPowerOfTwo() = 0;
		virtual bool ScreenShot(cImage *To) = 0;
		
		virtual bool GetDeviceRecentReset() = 0;

		//*********************************************************************
		// Textures
		//*********************************************************************
		virtual int GetTextureID(const char *FilePn) = 0;
		virtual void ReloadTextures() = 0;
		virtual bool ReloadTexture(const int TextureID) = 0;
		virtual void ReloadTexture(const char *FilePn) = 0;
		virtual void UnloadTextures() = 0;
		virtual int AddTexture(const char *FilePn) = 0;
		virtual int AddTexture(const cImage &Image) = 0;
		virtual void UpdateTexture(const int TextureID, const cList<cImage *> &SubImages, const cList<cRect> &Rects) = 0;
		virtual void UpdateTexture(const int TextureID, const cImage& Image) = 0;
		virtual void FreeTexture(const int TextureID) = 0;
		virtual const cStr * GetTextureFilePn(const int TextureID) = 0;
		virtual int GetTextureWidth(const int TextureID) = 0;
		virtual int GetTextureHeight(const int TextureID) = 0;
		virtual int GetTextureDepth(const int TextureID) = 0;
		virtual const cFormat::Enum GetTextureFormat(const int TextureID) = 0;
		virtual int GetTextureMipMapCount(const int TextureID) = 0;
		virtual bool GetTextureRenderTargetUsage(const int TextureID) = 0;
		
		virtual int GetShaderID(const char *ShaderName, const int VertexFormatID, const char *Extra, cStr *OptionalLog) = 0;
		virtual int AddShader(const char *ShaderName, const int VertexFormatID, const char *Extra, cStr *OptionalLog) = 0;
		virtual int GetShaderConstID(const int ShaderID, const char *ConstName) = 0;
		virtual void SetShader(const int ShaderID) = 0;
		virtual void FreeShader(const int ShaderID) = 0;
		virtual void ReloadShaders() = 0;

		virtual int AddVertexFormat(const cVertex::Format &Format) = 0;
		virtual int GetVertexSize(const int VertexFormatID) = 0;

		virtual int AddVertexBuffer(const void *Data, const size_t Size) = 0;
		virtual int AddIndexBuffer(const void *Data, const int IndexCount, const int IndexSize) = 0;
		virtual void FreeVertexBuffer(const int VertexBufferID) = 0;
		virtual void FreeIndexBuffer(const int IndexBufferID) = 0;

		virtual void SetVertexBuffer(const int VertexBufferID) = 0;
		virtual void SetIndexBuffer(const int IndexBufferID) = 0;
		
		virtual void DrawArrays(const cTopology::Enum Topology) = 0;
		virtual void DrawArrays(const cTopology::Enum Topology, const void *VertexData, const int VertexCount) = 0;
		virtual void DrawIndexed(const cTopology::Enum Topology) = 0;
		virtual void DrawIndexed(const cTopology::Enum Topology, const void *VertexData, const int VertexCount, const void *IndexData, const int IndexCount, const int IndexSize) = 0;

		virtual void SetWireframe(const bool Enabled) = 0;

		//*********************************************************************
		// SamplerState
		//*********************************************************************
		virtual int GetSamplerStateID(const cFilter::Enum Filter, const cAddressMode::Enum s, const cAddressMode::Enum t, const cAddressMode::Enum r) = 0;
		virtual void SetSamplerState(const int TextureID, const int SamplerStateID) = 0;
		
		virtual int GetSamplerID(const int ShaderID, const char *SamplerName) const = 0;
		virtual void SetTexture(const int SamplerID, const int TextureID) = 0;

		// Bindless texture
		virtual bool IsSupported_BindlessTexture() { return false; }
		
		virtual void SetShaderConst(const int ConstID, const int Value) = 0;
		virtual void SetShaderConst(const int ConstID, const float Value) = 0;
		virtual void SetShaderConst(const int ConstID, const cVec2 &Value) = 0;
		virtual void SetShaderConst(const int ConstID, const cVec3 &Value) = 0;
		virtual void SetShaderConst(const int ConstID, const cVec4 &Value) = 0;
		virtual void SetShaderConst(const int ConstID, const cMat3 &Value) = 0;
		virtual void SetShaderConst(const int ConstID, const cMat4 &Value) = 0;
		virtual void SetShaderConst(const int ConstID, const float *Array, const int Count) = 0;
		virtual void SetShaderConst(const int ConstID, const cVec2 *Array, const int Count) = 0;
		virtual void SetShaderConst(const int ConstID, const cVec3 *Array, const int Count) = 0;
		virtual void SetShaderConst(const int ConstID, const cVec4 *Array, const int Count) = 0;

		virtual void SetShaderAutoConstants() = 0;

		//---------------------------------------------------------------------
		// Blend
		//---------------------------------------------------------------------
		virtual int GetBlendStateID(const cBlendFactor::Enum SrcFactor, const cBlendFactor::Enum DstFactor, const cBlendMode::Enum Mode, const dword Mask) = 0;
		virtual void SetBlendState(const int BlendStateID) = 0;

		//---------------------------------------------------------------------
		// Depth
		//---------------------------------------------------------------------
		virtual int GetDepthStateID(const bool TestEnabled, const bool WriteEnabled, const cDepthFunc::Enum Func = cDepthFunc::LessEqual) = 0;
		virtual void SetDepthState(const int DepthStateID) = 0;

		virtual void SetCullMode(const cCullMode::Enum CullMode) = 0;

		virtual void Clear(const bool ClearColor, const bool ClearDepth, const cColor &Color, const float Depth) = 0;
		
		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;

		virtual void Finish() = 0;

		static bool LoadShader(const char* FilePn, cStr* VSText, cStr* GSText, cStr* TCSText, cStr* TESText, cStr* FSText, int* VSLine, int* GSLine, int* TCSLine, int* TESLine, int* FSLine);
		static bool LoadShaderFromCache(const char *FilePn, const char *Extra, cFile *VSBin, cFile *PSBin);

		//*********************************************************************
		// Render target
		//*********************************************************************
		virtual int AddRenderTarget(const int Width, const int Height, const cFormat::Enum Format, const bool CubeMap, const int Samples) = 0;
		virtual int AddRenderDepth(const int RenderTargetID, const cFormat::Enum Format) = 0;
		virtual void SetRenderTargets(const int *ColorRenderTargetIDs, const int ColorRenderTargetCount, const int DepthRenderTargetID, const int *Faces) = 0;
		virtual void SetRenderTargetSize(const int RenderTargetID, const int Width, const int Height) = 0;
        virtual bool SaveRenderTargetRaw(const int RenderTargetID, SaveRenderTargetArgs *Args) = 0;
        virtual bool RequestSaveRenderTargetAsyncRaw(const int RenderTargetID, SaveRenderTargetArgs *Args) = 0;
        virtual bool GetSaveRenderTargetAsyncRaw(const int RenderTargetID, SaveRenderTargetArgs *Args) = 0;
        virtual void FreeSaveRenderTargetAsyncRaw(const int RenderTargetID) = 0;
        virtual bool IsSaveRenderTargetAsyncReadyRaw(const int RenderTargetID) = 0;
	};
};
