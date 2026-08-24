#include "comms.h"

namespace comms {

int cRender::PixelsPerPoint = 1;
bool cRender::_NeedUpdate = false;
static cMat4 cRender_WorldMatrix;
static cMat4 cRender_WorldMatrixInverse;
static bool cRender_WorldMatrixInverseIsValid;
static cMat3 cRender_NormalMatrix;
static bool cRender_NormalMatrixIsValid;
static cList<cMat4> cRender_WorldMatricesStack;
static cMat4 cRender_TextureMatrix[4];
static int cRender_CurDepthStateID;
static int cRender_CurBlendStateID;
static cCullMode::Enum cRender_CurCullMode;
static cList<int> cRender_DepthStatesStack;
static cList<int> cRender_BlendStatesStack;
static cList<cCullMode::Enum> cRender_CullModesStack;
static bool cRender_BeginFrame;
static bool cRender_ExpandTexturesToPowerOfTwo = true;

static bool cRender_Wireframe;
bool cRender::GetWireframe() {
	return cRender_Wireframe;
}

struct cRender_RenderTarget {
	cList<int> ColorRenderTargetIDs;
	int DepthRenderTargetID;
	cList<int> Faces;
};
static cList<cRender_RenderTarget> cRender_RenderTargetsStack;

static cRender::Stub *cRender_Instance = nullptr;

static cList<cRender_FONT> cRender_Fonts;

static cViewer *cRender_Viewer = nullptr;
cViewer * cRender::GetViewer() {
	return cRender_Viewer;
}

static cRect cRender_CurClipRect = cRect::Empty;
const cRect & cRender::GetClipRect() {
	return cRender_CurClipRect;
}

 cList<cRender_FONT>& cRender::GetRenderFonts() {
	return cRender_Fonts;
}

static cList<cRect> cRender_ClipRectsStack;

static cList<cStr> cRender_ShadersCache;

static const char * FilterStrings[] = {
	"Nearest",
	"Linear",
	"Bilinear",
	"Trilinear",
	"Bilinear Aniso",
	"Trilinear Aniso"
};

const char * cFilter::ToString(const cFilter::Enum Filter) {
	return FilterStrings[Filter];
}

static const char * AddressModeStrings[] = {
	"Wrap",
	"Clamp",
	"Mirror",
	"Clip"
};

const char * cAddressMode::ToString(const cAddressMode::Enum AddressMode) {
	return AddressModeStrings[AddressMode];
}

cRender::Stub * cRender::GetInstance() {
	return cRender_Instance;
}

// cRender::GetType
const cRenderType::Enum cRender::GetType() {
	if(nullptr == cRender_Instance) {
		return cRenderType::None;
	}
	return cRender_Instance->GetType();
}

// cRender::GetFlipRenderTarget
bool cRender::GetFlipRenderTarget() {
	return cRenderType::DirectX == GetType();
}

//-----------------------------------------------------------------------------
// cRender::ResetStates
//-----------------------------------------------------------------------------
void cRender::ResetStates() {
	cRender_CurDepthStateID = -1;
	cRender_Instance->SetDepthState(-1);
	
	cRender_CurBlendStateID = -1;
	cRender_Instance->SetBlendState(-1);
	
	cRender_CurCullMode = cCullMode::None;
	cRender_Instance->SetCullMode(cCullMode::None);
	
	cRender_WorldMatrix.SetIdentity();
	cRender_WorldMatrixInverse.SetIdentity();
	cRender_WorldMatrixInverseIsValid = true;
	cRender_NormalMatrix.SetIdentity();
	cRender_NormalMatrixIsValid = true;
	
	cRender_TextureMatrix[0].SetIdentity();
	cRender_TextureMatrix[1].SetIdentity();
	cRender_TextureMatrix[2].SetIdentity();
	cRender_TextureMatrix[3].SetIdentity();
	
	cRender_Wireframe = false;
	cRender_Instance->SetWireframe(false);
	
	cRender_CurClipRect = cRect::Empty;
	cRender_Instance->SetClipRect(cRect::Empty);
} // cRender::ResetStates

// cRender_CacheFolder
static const char *cRender_CacheFolder[2] = {
	"Shaders/PC",
    nullptr
};

// cRender_InitShadersCache
static void cRender_InitShadersCache() {
    int PlatformIndex = -1;
#ifdef COMMS_WINDOWS
	PlatformIndex = 0;
#endif
    if(-1 == PlatformIndex) {
        return;
    }
	cStr P = cRender_CacheFolder[PlatformIndex];
	P.AppendPath("Cache.txt");
	cFile F;
	if(!cIO::LoadFile(P, &F, false)) {
		return;
	}
	cStr S;
	while(F.ReadString(&S)) {
		S.Trim(" \t");
		if(S.IsEmpty()) {
			continue;
		}
		cRender_ShadersCache.Add(S);
	}
	// Sort defs
	int i = 1, j;
	cList<cStr> t;
	while(i < cRender_ShadersCache.Count()) {
		cStr &r = cRender_ShadersCache[i];
		r.Replace("nullptr", "");
		r.Split(&t);
		t.Sort();
		r.Clear();
		for(j = 0; j < t.Count(); j++) {
			if(!r.IsEmpty()) {
				r += " ";
			}
			r += t[j];
		}
		i += 3;
	}
	cLog::Message("%d Cached Shaders", cRender_ShadersCache.Count() / 3);
}

//-----------------------------------------------------------------------------
// cRender::Init
//-----------------------------------------------------------------------------
bool cRender::Init(const int MaxSamples) {
	cAssert(nullptr == cRender_Instance);
	if(cRender_Instance != nullptr) {
		return false;
	}
#ifdef COMMS_OPENGL
	cRender_Instance = cRender_CreateGL();
#endif
#ifdef COMMS_DIRECTX
	cRender_Instance = cRender_CreateDX();
#endif
	bool r = false;
	if (cRender_Instance != nullptr) {
		r = cRender_Instance->Init(MaxSamples);
		if (r) {
			cRender_BeginFrame = false;
			ResetStates();
			cLog::Message("Registering vertex formats...");
			cVertex::Register();
			cRender_InitShadersCache();
		}
	}
	return r;
} // cRender::Init

//-----------------------------------------------------------------------------
// cRender::Free
//-----------------------------------------------------------------------------
void cRender::Free() {
	cAssert(cRender_Instance != nullptr);
	if(nullptr == cRender_Instance) {
		return;
	}
	cRender_Instance->Free();
	delete cRender_Instance;
	cRender_Instance = nullptr;

	cRender_DepthStatesStack.Clear();
	cRender_BlendStatesStack.Clear();
	cRender_CullModesStack.Clear();
	cRender_RenderTargetsStack.Clear();
	cRender_ClipRectsStack.Clear();
	cRender_WorldMatricesStack.Clear();
} // cRender::Free

// cRender::PushDepthState
void cRender::PushDepthState() {
	cRender_DepthStatesStack.Add(cRender_CurDepthStateID);
}

// cRender::PopDepthState
void cRender::PopDepthState() {
	cAssert(!cRender_DepthStatesStack.IsEmpty());
	if(!cRender_DepthStatesStack.IsEmpty()) {
		SetDepthState(cRender_DepthStatesStack.GetLast());
		cRender_DepthStatesStack.RemoveLast();
	}
}

// cRender::PushBlendState
void cRender::PushBlendState() {
	cRender_BlendStatesStack.Add(cRender_CurBlendStateID);
}

// cRender::PopBlendState
void cRender::PopBlendState() {
	cAssert(!cRender_BlendStatesStack.IsEmpty());
	if(!cRender_BlendStatesStack.IsEmpty()) {
		SetBlendState(cRender_BlendStatesStack.GetLast());
		cRender_BlendStatesStack.RemoveLast();
	}
}

// cRender::PushCullMode
void cRender::PushCullMode() {
	cRender_CullModesStack.Add(cRender_CurCullMode);
}

// cRender::PopCullMode
void cRender::PopCullMode() {
	cAssert(!cRender_CullModesStack.IsEmpty());
	if(!cRender_CullModesStack.IsEmpty()) {
		SetCullMode(cRender_CullModesStack.GetLast());
		cRender_CullModesStack.RemoveLast();
	}
}

//-----------------------------------------------------------------------------
// cRender::SetViewer
//-----------------------------------------------------------------------------
void cRender::SetViewer(cViewer *Viewer) {
	cRender_Viewer = Viewer;
	
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetViewport(Viewer->GetViewport());
	cRender_Instance->SetShaderAutoConstants();
} // cRender::SetViewer

// cRender::PushClipRect
void cRender::PushClipRect() {
	cRender_ClipRectsStack.Add(cRender_CurClipRect);
}

//-----------------------------------------------------------------------------
// cRender::PopClipRect
//-----------------------------------------------------------------------------
void cRender::PopClipRect() {
	cAssert(!cRender_ClipRectsStack.IsEmpty());
	if(cRender_ClipRectsStack.IsEmpty()) {
		return;
	}
	SetClipRect(cRender_ClipRectsStack.GetLast());
	cRender_ClipRectsStack.RemoveLast();
} // cRender::PopClipRect

//-----------------------------------------------------------------------------
// cRender::SetClipRect
//-----------------------------------------------------------------------------
void cRender::SetClipRect(const cRect &ClipRect) {
	if(cRender_CurClipRect.IsEmpty() && ClipRect.IsEmpty()) {
		return; // Clipping is already disabled
	}
	if(cRect::Equals(cRender_CurClipRect, ClipRect)) {
		return; // The same clipping region is already set
	}
	cRender_CurClipRect = ClipRect;
	
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetClipRect(cRender_CurClipRect);
} // cRender::SetClipRect

//-----------------------------------------------------------------------------
// cRender::SetTextureMatix
//-----------------------------------------------------------------------------
void cRender::SetTextureMatrix(const cMat4 &T, const int Index) {
	cAssert(Index >= 0 && Index < 4);
	if(Index < 0 || Index >= 4) {
		return;
	}

	cRender_TextureMatrix[Index] = T;

	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderAutoConstants();
} // cRender::SetTextureMatrix

//-----------------------------------------------------------------------------
// cRender::GetTextureMatrix
//-----------------------------------------------------------------------------
const cMat4 & cRender::GetTextureMatrix(const int Index) {
	cAssert(Index >= 0 && Index < 4);
	if(Index < 0 || Index >= 4) {
		return cRender_TextureMatrix[0];
	}

	return cRender_TextureMatrix[Index];
} // cRender::GetTextureMatrix

// cRender::PushWorldMatrix
void cRender::PushWorldMatrix() {
	cRender_WorldMatricesStack.Add(cRender_WorldMatrix);
}

// cRender::PopWorldMatrix
void cRender::PopWorldMatrix() {
	cAssert(!cRender_WorldMatricesStack.IsEmpty());
	if(cRender_WorldMatricesStack.IsEmpty()) {
		return;
	}
	SetWorldMatrix(cRender_WorldMatricesStack.GetLast());
	cRender_WorldMatricesStack.RemoveLast();
}

// cRender::SetWorldMatrix
void cRender::SetWorldMatrix(const cMat4 &T) {
	cRender_WorldMatrix = T;
	cRender_WorldMatrixInverseIsValid = false;
	cRender_NormalMatrixIsValid = false;

	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderAutoConstants();
}

// cRender::GetWorldMatrix
const cMat4 & cRender::GetWorldMatrix() {
	return cRender_WorldMatrix;
}

// cRender::GetWorldMatrixInverse
const cMat4 & cRender::GetWorldMatrixInverse() {
	if(!cRender_WorldMatrixInverseIsValid) {
		if(!cMat4::Invert(cRender_WorldMatrix, &cRender_WorldMatrixInverse)) {
			cRender_WorldMatrixInverse.SetIdentity();
		}
		cRender_WorldMatrixInverseIsValid = true;
	}
	return cRender_WorldMatrixInverse;
}

// cRender::GetNormalMatrix
const cMat3 & cRender::GetNormalMatrix() {
	if(!cRender_NormalMatrixIsValid) {
		cRender_NormalMatrix = cRender_WorldMatrix.ToNormalMatrix();
		cRender_NormalMatrixIsValid = true;
	}
	return cRender_NormalMatrix;
}

// cRender::SetWireframe
void cRender::SetWireframe(const bool Enabled) {
	if(Enabled != cRender_Wireframe) {
		cAssert(cRender_Instance != nullptr);
		cRender_Instance->SetWireframe(Enabled);
		cRender_Wireframe = Enabled;
	}
}

// cRender::GetVendor
const cStr& cRender::GetVendor() {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetVendor();
}
// cRender::GetVersion
const cStr& cRender::GetVersion() {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetVersion();
}
// cRender::GetRenderer
const cStr& cRender::GetRenderer() {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetRenderer();
}

// cRender::GetMRTCount
int cRender::GetMRTCount() {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetMRTCount();
}

// cRender::SupportsFormat
bool cRender::SupportsFormat(const cFormat::Enum Format) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->SupportsFormat(Format);
}

// cRender::ScreenShot
bool cRender::ScreenShot(cImage *To) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->ScreenShot(To);
}

// cRender::SupportsNonPowerOfTwo
bool cRender::SupportsNonPowerOfTwo() {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->SupportsNonPowerOfTwo();
}

// cRender::GetDeviceRecentReset
bool cRender::GetDeviceRecentReset() {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetDeviceRecentReset();
}

//*****************************************************************************
// SamplerState
//*****************************************************************************
int cRender::GetSamplerStateID(const cFilter::Enum Filter, const cAddressMode::Enum s, const cAddressMode::Enum t, const cAddressMode::Enum r) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetSamplerStateID(Filter, s, t, r);
}

// cRender_ShaderSampler
struct cRender_ShaderSampler {
	int ShaderID;
	cStr SamplerName;
	int* PtrToID;
};
static cList<cRender_ShaderSampler> cRender_ShadersSamplers;

int cRender::GetSamplerID(const int ShaderID, const char *SamplerName, int *OptionalPtrToIDForReload) {
	cAssert(cRender_Instance != nullptr);
	if (OptionalPtrToIDForReload != nullptr) {
		cRender_ShaderSampler SS;
		SS.ShaderID = ShaderID;
		SS.SamplerName = SamplerName;
		SS.PtrToID = OptionalPtrToIDForReload;
		cRender_ShadersSamplers.Add(SS);
	}
	return cRender_Instance->GetSamplerID(ShaderID, SamplerName);
}

void cRender::SetSamplerState(const int TextureID, const int SamplerStateID) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetSamplerState(TextureID, SamplerStateID);
}

void cRender::SetTexture(const int SamplerID, const int TextureID, const int SamplerStateID) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetSamplerState(TextureID, SamplerStateID);
	cRender_Instance->SetTexture(SamplerID, TextureID);
}

// Bindless texture
bool cRender::IsSupported_BindlessTexture() {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->IsSupported_BindlessTexture();
}

void cRender::SetShaderConst(const int ConstID, const int Value) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, Value);
}

void cRender::SetShaderConst(const int ConstID, const float Value) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, Value);
}

void cRender::SetShaderConst(const int ConstID, const cVec2 &Value) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, Value);
}

void cRender::SetShaderConst(const int ConstID, const cVec3 &Value) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, Value);
}

void cRender::SetShaderConst(const int ConstID, const cVec4 &Value) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, Value);
}

void cRender::SetShaderConst(const int ConstID, const cColor &Value) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, *((const cVec4 *)&Value));
}

void cRender::SetShaderConst(const int ConstID, const cRect &Value) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, cVec4(Value.GetBottomLeft(), Value.GetTopRight()));
}

void cRender::SetShaderConst(const int ConstID, const cMat3 &Value) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, Value);
}

void cRender::SetShaderConst(const int ConstID, const cMat4 &Value) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, Value);
}

void cRender::SetShaderConst(const int ConstID, const float *Array, const int Count) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, Array, Count);
}

void cRender::SetShaderConst(const int ConstID, const cVec2 *Array, const int Count) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, Array, Count);
}

void cRender::SetShaderConst(const int ConstID, const cVec3 *Array, const int Count) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, Array, Count);
}

void cRender::SetShaderConst(const int ConstID, const cVec4 *Array, const int Count) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, Array, Count);
}

void cRender::SetShaderConst(const int ConstID, const cColor *Array, const int Count) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShaderConst(ConstID, (const cVec4 *)Array, Count);
}

//*****************************************************************************
// Textures
//*****************************************************************************
// cRender::GetTextureFilePn
const cStr * cRender::GetTextureFilePn(const int TextureID) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetTextureFilePn(TextureID);
}

// cRender::GetTextureWidth
int cRender::GetTextureWidth(const int TextureID) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetTextureWidth(TextureID);
}

// cRender::GetTextureHeight
int cRender::GetTextureHeight(const int TextureID) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetTextureHeight(TextureID);
}

// cRender::GetTextureDepth
int cRender::GetTextureDepth(const int TextureID) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetTextureDepth(TextureID);
}

// cRender::GetTextureFormat
const cFormat::Enum cRender::GetTextureFormat(const int TextureID) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetTextureFormat(TextureID);
}

// cRender::GetTextureMipMapCount
int cRender::GetTextureMipMapCount(const int TextureID) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetTextureMipMapCount(TextureID);
}

// cRender::GetTextureRenderTargetUsage
bool cRender::GetTextureRenderTargetUsage(const int TextureID) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetTextureRenderTargetUsage(TextureID);
}

// cRender::AddRenderTarget
int cRender::AddRenderTarget(const int Width, const int Height, const cFormat::Enum Format, const int SamplerStateID, const bool CubeMap, const int Samples) {
	cAssert(cRender_Instance != nullptr);
	int ID = cRender_Instance->AddRenderTarget(Width, Height, Format, CubeMap, Samples);
	cRender_Instance->SetSamplerState(ID, SamplerStateID);
	return ID;
}

// cRender::AddRenderDepth
int cRender::AddRenderDepth(const int RenderTargetID) {
	cAssert(cRender_Instance != nullptr);
    cFormat::Enum Format = cFormat::Depth24Stencil8;
#if defined COMMS_IOS || defined COMMS_TIZEN
    Format = cFormat::Depth16;
#endif // COMMS_IOS || COMMS_TIZEN
	return cRender_Instance->AddRenderDepth(RenderTargetID, Format);
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    // cRender::PushRenderTarget
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void cRender::PushRenderTarget(const int *ColorRenderTargetIDs, const int ColorRenderTargetCount, const int DepthRenderTargetID, const int *Faces, const bool SaveAspect) {
	cAssert(cRender_Instance != nullptr);
	cAssert(ColorRenderTargetIDs != nullptr);
	cAssert(ColorRenderTargetCount > 0);
	
	cRender_RenderTarget RT;
	RT.ColorRenderTargetIDs.Copy(ColorRenderTargetIDs, ColorRenderTargetCount);
	RT.DepthRenderTargetID = DepthRenderTargetID;
	if(Faces != nullptr) {
		RT.Faces.Copy(Faces, ColorRenderTargetCount);
	}
	cRender_RenderTargetsStack.Add(RT);

	static int M = -1;
	M = cMath::Max(M, cRender_RenderTargetsStack.Count());
	
	cRender_Instance->SetRenderTargets(ColorRenderTargetIDs, ColorRenderTargetCount, DepthRenderTargetID, Faces);
	
	// For MRT all color render targets should have the same size
	GetViewer()->PushState();
	if(SaveAspect) {
		GetViewer()->SetAspect(GetViewer()->GetAspect());
	}
	cRect rc = GetTextureRect(ColorRenderTargetIDs[0]);
	GetViewer()->SetViewport(rc);
	SetViewer(GetViewer());
	PushClipRect();
	SetClipRect(cRect::Empty);
} // cRender::PushRenderTarget

//-----------------------------------------------------------------------------
// cRender::PopRenderTarget
//-----------------------------------------------------------------------------
void cRender::PopRenderTarget() {
	cAssert(cRender_Instance != nullptr);
	cAssert(!cRender_RenderTargetsStack.IsEmpty());

	if(cRender_RenderTargetsStack.IsEmpty()) {
		return;
	}

	cRender_RenderTarget RT = cRender_RenderTargetsStack.GetLast();
	cRender_RenderTargetsStack.RemoveLast();

	if(cRender_RenderTargetsStack.IsEmpty()) {
		cRender_Instance->SetRenderTargets(nullptr, 0, -1, nullptr); // Restore frame buffer
	} else {
		const cRender_RenderTarget &RT = cRender_RenderTargetsStack.GetLast();
		cRender_Instance->SetRenderTargets(RT.ColorRenderTargetIDs.ToPtr(), RT.ColorRenderTargetIDs.Count(), RT.DepthRenderTargetID, RT.Faces.ToPtr());
	}

	GetViewer()->PopState();
	SetViewer(GetViewer());
	PopClipRect();
} // cRender::PopRenderTarget

// cRender::SetRenderTargetSize
void cRender::SetRenderTargetSize(const int RenderTargetID, const int Width, const int Height) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetRenderTargetSize(RenderTargetID, Width, Height);
}

// cRender::SaveRenderTarget
bool cRender::SaveRenderTarget(const int RenderTargetID, cImage *To) {
	cAssert(cRender_Instance != nullptr);
    SaveRenderTargetArgs Args;
    cRender_Instance->SaveRenderTargetRaw(RenderTargetID, &Args);
    if(Args.RequiredSize > 0) {
        To->Create(Args.Format, Args.Width, Args.Height, 1, 1);
        Args.Buffer = To->GetPixels();
        Args.BufferSize = Args.RequiredSize;
        cAssert(To->GetSize() == Args.BufferSize);
        return cRender_Instance->SaveRenderTargetRaw(RenderTargetID, &Args);
    }
    return false;
}

bool cRender::SaveRenderTargetRaw(const int RenderTargetID, SaveRenderTargetArgs *Args) {
    cAssert(cRender_Instance != nullptr);
    return cRender_Instance->SaveRenderTargetRaw(RenderTargetID, Args);
}

// cRender::RequestSaveRenderTargetAsync
void cRender::RequestSaveRenderTargetAsync(const int RenderTargetID) {
	cAssert(cRender_Instance != nullptr);
    SaveRenderTargetArgs Args;
    cRender_Instance->RequestSaveRenderTargetAsyncRaw(RenderTargetID, &Args);
}

// cRender::GetSaveRenderTargetAsync
bool cRender::GetSaveRenderTargetAsync(const int RenderTargetID, cImage *To) {
	cAssert(cRender_Instance != nullptr);
    SaveRenderTargetArgs Args;
    
    // First call to get sizes
    cRender_Instance->GetSaveRenderTargetAsyncRaw(RenderTargetID, &Args);
    if(Args.RequiredSize > 0) {
        To->Create(Args.Format, Args.Width, Args.Height, 1, 1);
        Args.Buffer = To->GetPixels();
        Args.BufferSize = Args.RequiredSize;
        cAssert(To->GetSize() == Args.BufferSize);
        // Second call mapped to buffer
        return cRender_Instance->GetSaveRenderTargetAsyncRaw(RenderTargetID, &Args);
    }
    return false;
}

void cRender::FreeSaveRenderTargetAsync(const int RenderTargetID) {
    cAssert(cRender_Instance != nullptr);
    cRender_Instance->FreeSaveRenderTargetAsyncRaw(RenderTargetID);
}

//--------------------------------------------------------------------------------------------------
// cRender::IsSaveRenderTargetAsyncReady
//--------------------------------------------------------------------------------------------------
bool cRender::IsSaveRenderTargetAsyncReady(const int RenderTargetID) {
    if (cRender_Instance) {
        return cRender_Instance->IsSaveRenderTargetAsyncReadyRaw(RenderTargetID);
    }
    return false;
}

bool cRender::RequestSaveRenderTargetAsyncRaw(const int RenderTargetID, SaveRenderTargetArgs *Args) {
    cAssert(cRender_Instance != nullptr);
    return cRender_Instance->RequestSaveRenderTargetAsyncRaw(RenderTargetID, Args);
}

bool cRender::GetSaveRenderTargetAsyncRaw(const int RenderTargetID, SaveRenderTargetArgs *Args) {
    cAssert(cRender_Instance != nullptr);
    return cRender_Instance->GetSaveRenderTargetAsyncRaw(RenderTargetID, Args);
}

void cRender::FreeSaveRenderTargetAsyncRaw(const int RenderTargetID) {
    cAssert(cRender_Instance != nullptr);
    cRender_Instance->FreeSaveRenderTargetAsyncRaw(RenderTargetID);
}

bool cRender::IsSaveRenderTargetAsyncReadyRaw(const int RenderTargetID) {
    cAssert(cRender_Instance != nullptr);
    return cRender_Instance->IsSaveRenderTargetAsyncReadyRaw(RenderTargetID);
}

// cRender::GetTextureID
int cRender::GetTextureID(const char *FilePn, const int SamplerStateID) {
	cAssert(cRender_Instance != nullptr);
	int ID = cRender_Instance->GetTextureID(FilePn);
	cRender_Instance->SetSamplerState(ID, SamplerStateID);
	return ID;
}

// cRender::AddTexture : (const char *)
int cRender::AddTexture(const char *FilePn, const int SamplerStateID) {
	cAssert(cRender_Instance != nullptr);
	int ID = cRender_Instance->AddTexture(FilePn);
	cRender_Instance->SetSamplerState(ID, SamplerStateID);
	return ID;
}

// cRender::AddTexture
int cRender::AddTexture(const cImage &Image, const int SamplerStateID) {
	cAssert(cRender_Instance != nullptr);
	int ID = cRender_Instance->AddTexture(Image);
	cRender_Instance->SetSamplerState(ID, SamplerStateID);
	return ID;
}

// cRender::GetExpandTexturesToPowerOfTwo
bool cRender::GetExpandTexturesToPowerOfTwo() {
	return cRender_ExpandTexturesToPowerOfTwo;
}

// SetExpandTexturesToPowerOfTwo
void cRender::SetExpandTexturesToPowerOfTwo(const bool Value) {
	cRender_ExpandTexturesToPowerOfTwo = Value;
}

// cRender::UpdateTexture : (..., const cImage &, ...)
void cRender::UpdateTexture(const int TextureID, const cImage &Image, const cList<cRect> &Rects) {
	cList<cImage *> SubImages;
	int i;
	for(i = 0; i < Rects.Count(); i++) {
		SubImages.Add(new cImage(Image, Rects[i]));
	}
	UpdateTexture(TextureID, SubImages, Rects);
	SubImages.FreeContents();
}

// cRender::UpdateTexture : (..., const cList<cImage *> &, ...)
void cRender::UpdateTexture(const int TextureID, const cList<cImage *> &SubImages, const cList<cRect> &Rects) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->UpdateTexture(TextureID, SubImages, Rects);
}

void cRender::UpdateTexture(const int TextureID, const cImage& Image) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->UpdateTexture(TextureID, Image);
}


// cRender::ReloadTextures
void cRender::ReloadTextures() {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->ReloadTextures();
}

// cRender::ReloadTexture
bool cRender::ReloadTexture(const int TextureID) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->ReloadTexture(TextureID);
}

// cRender::ReloadTexture
void cRender::ReloadTexture(const char *FilePn) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->ReloadTexture(FilePn);
}

// cRender::UnloadTextures
void cRender::UnloadTextures() {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->UnloadTextures();
}

// cRender::FreeTexture
void cRender::FreeTexture(const int TextureID) {
	if(cRender_Instance != nullptr) {
		cRender_Instance->FreeTexture(TextureID);
	}
}

// cRender::GetShaderID
int cRender::GetShaderID(const char *ShaderName, const int VertexFormatID, const char *Extra, cStr *OptionalLog) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetShaderID(ShaderName, VertexFormatID, Extra, OptionalLog);
}

// cRender::AddShader
int cRender::AddShader(const char *ShaderName, const int VertexFormatID, const char *Extra) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->AddShader(ShaderName, VertexFormatID, Extra, nullptr);
}

// cRender::FreeShader
void cRender::FreeShader(const int ShaderID) {
	if(cRender_Instance != nullptr) {
		cRender_Instance->FreeShader(ShaderID);
	}
}

// cRender_ShaderConst
struct cRender_ShaderConst {
	int ShaderID;
	cStr ConstName;
	int* PtrToID;
};
static cList<cRender_ShaderConst> cRender_ShadersConsts;

// cRender::ReloadShaders
void cRender::ReloadShaders() {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->ReloadShaders();
	int i;
	for (i = 0; i < cRender_ShadersSamplers.Count(); i++) {
		cRender_ShaderSampler *SS = &cRender_ShadersSamplers[i];
		int NewID = cRender_Instance->GetSamplerID(SS->ShaderID, SS->SamplerName);
		*SS->PtrToID = NewID;
	}
	for (i = 0; i < cRender_ShadersConsts.Count(); i++) {
		cRender_ShaderConst* SC = &cRender_ShadersConsts[i];
		int NewID = cRender_Instance->GetShaderConstID(SC->ShaderID, SC->ConstName);
		*SC->PtrToID = NewID;
	}
}

struct cRender_ReplaceInShaders_t {
	cStr What;
	cStr With;
	static int Compare(cRender_ReplaceInShaders_t * const *l, cRender_ReplaceInShaders_t * const *r) {
		return cStr::Compare((*l)->What, (*r)->What);
	}
};
cList<cRender_ReplaceInShaders_t *> cRender_ReplaceInShaders;

// cRender::ReplaceInShaders
void cRender::ReplaceInShaders(const char *What, const char *With) {
	cRender_ReplaceInShaders_t *R = new cRender_ReplaceInShaders_t;
	R->What = What;
	R->With = With;
	int i = cRender_ReplaceInShaders.BinarySearch(R, cRender_ReplaceInShaders_t::Compare);
	if(i != -1) {
		delete R;
		R = nullptr;
		cRender_ReplaceInShaders[i]->With = With;
	} else {
		cRender_ReplaceInShaders.Add(R);
		R = nullptr;
		cRender_ReplaceInShaders.Sort(cRender_ReplaceInShaders_t::Compare);
	}
}

// cRender::GetShaderConstID
int cRender::GetShaderConstID(const int ShaderID, const char *ConstName, int* OptionalPtrToIDForReload) {
	cAssert(cRender_Instance != nullptr);
	if (OptionalPtrToIDForReload != nullptr) {
		cRender_ShaderConst SC;
		SC.ShaderID = ShaderID;
		SC.ConstName = ConstName;
		SC.PtrToID = OptionalPtrToIDForReload;
		cRender_ShadersConsts.Add(SC);
	}
	return cRender_Instance->GetShaderConstID(ShaderID, ConstName);
}

//----------------------------------------------------------------------------------------------------------
// cRender::Stub::LoadShaderFromCache
//----------------------------------------------------------------------------------------------------------
bool cRender::Stub::LoadShaderFromCache(const char *FilePn, const char *Extra, cFile *VSBin, cFile *PSBin) {
    int PlatformIndex = -1;
#ifdef COMMS_WINDOWS
	PlatformIndex = 0;
#endif
    if(-1 == PlatformIndex) {
        return false;
    }
	cStr c = cRender_CacheFolder[PlatformIndex];
    cStr S = Extra;
	S.Replace("#define", "");
	S.Replace("\r", "");
	S.Replace("\n", "");
	cList<cStr> t;
	S.Split(&t);
	t.Sort();
	S.Clear();
	int i;
	for(i = 0; i < t.Count(); i++) {
		if(!S.IsEmpty()) {
			S += " ";
		}
		S += t[i];
	}
	cStr n;
	i = 1;
	while(i < cRender_ShadersCache.Count()) {
		const cStr &p = cRender_ShadersCache[i - 1];
		const cStr &d = cRender_ShadersCache[i];
		if(cStr::EqualsPath(FilePn, p) && cStr::Equals(d, S)) {
			n = cRender_ShadersCache[i + 1];
			break;
		}
		i += 3;
	}
	if(n.IsEmpty()) {
		return false;
	}
	c.AppendPath(n);
	c.SetFileExtension("VS");
	if(!cIO::LoadFile(c, VSBin)) {
		return false;
	}
	c.SetFileExtension("PS");
	if(!cIO::LoadFile(c, PSBin)) {
		return false;
	}
	return true;
} // cRender::Stub::LoadShaderFromCache

//--------------------------------------------------------------------------------------------------------
// cRender::Stub::LoadShader
//--------------------------------------------------------------------------------------------------------
bool cRender::Stub::LoadShader(const char* FilePn, cStr* VSText, cStr* GSText, cStr* TCSText, cStr* TESText, cStr* FSText, int* VSLine, int* GSLine, int* TCSLine, int* TESLine, int* FSLine) {
	cFile File;
	cStr Src;
	const char *VSMagic = "// Vertex shader";
	const char* TCSMagic = "// Tessellation Control shader";
	const char* TESMagic = "// Tessellation Evaluation shader";
	const char* GSMagic = "// Geometry shader";
	const char *FSMagic = "// Fragment shader";
	const char *PSMagic = "// Pixel shader";
	int FSMagicLength = cStr::Length(FSMagic);
	int VSIndex, FSIndex, PSIndex, GSIndex, TCSIndex, TESIndex;
	char *VSPtr, *FSPtr, *GSPtr, *TCSPtr, *TESPtr;
	const char *c;
	*VSLine = 1;
	*FSLine = 1;
	*GSLine = 1;
	*TCSLine = 1;
	*TESLine = 1;
#if defined(COMMS_MACOS) || defined(COMMS_LINUX)
    // Under macOS and Linux we count lines from 0
    *VSLine = 0;
    *FSLine = 0;
    *GSLine = 0;
	*TCSLine = 0;
	*TESLine = 0;
#endif // macOS, Linux
	
	if(!cIO::LoadFile(FilePn, &File)) {
		return false;
	}

	cLog::Message(" * Loading shader \"%s\"...", FilePn);
	
	// Copy shaders source to string
	File.SeekEnd(0);
	File.WriteByte('\0');
	Src = (const char *)File.ToPtr();
	
	VSIndex = Src.IndexOf(VSMagic, true);
	FSIndex = Src.IndexOf(FSMagic, true);
	PSIndex = Src.IndexOf(PSMagic, true);
	GSIndex = Src.IndexOf(GSMagic, true);
	TCSIndex = Src.IndexOf(TCSMagic, true);
	TESIndex = Src.IndexOf(TESMagic, true);

	VSPtr = (VSIndex != -1) ? &Src[VSIndex] : nullptr;
	FSPtr = (FSIndex != -1) ? &Src[FSIndex] : nullptr;
	GSPtr = (GSIndex != -1) ? &Src[GSIndex] : nullptr;
	TCSPtr = (TCSIndex != -1) ? &Src[TCSIndex] : nullptr;
	TESPtr = (TESIndex != -1) ? &Src[TESIndex] : nullptr;
	if(-1 == FSIndex) {
		FSPtr = (PSIndex != -1) ? &Src[PSIndex] : nullptr;
		FSMagicLength = cStr::Length(PSMagic);
	}
	
	if(VSPtr != nullptr) {
		*VSPtr = '\0';
		VSPtr += cStr::Length(VSMagic);

		// Seeking end of line:
		while(*VSPtr != '\0' && *VSPtr != '\n') {
			VSPtr++;
		}

		// VS starts from the next line:
		if(*VSPtr != '\0') {
			VSPtr++;
		}

		// Counting lines
		c = Src.ToCharPtr();
		while(c < VSPtr) {
			if('\n' == *c) {
				(*VSLine)++;
			}
			c++;
		}
	}

	if (GSPtr != nullptr) {
		*GSPtr = '\0';
		GSPtr += cStr::Length(GSMagic);

		// Seeking end of line:
		while (*GSPtr != '\0' && *GSPtr != '\n') {
			GSPtr++;
		}

		// GS starts from the next line:
		if (*GSPtr != '\0') {
			GSPtr++;
		}

		// Counting lines
		c = Src.ToCharPtr();
		while (c < GSPtr) {
			if ('\n' == *c) {
				(*GSLine)++;
			}
			c++;
		}
	}

	if (TCSPtr != nullptr) {
		*TCSPtr = '\0';
		TCSPtr += cStr::Length(TCSMagic);

		// Seeking end of line:
		while (*TCSPtr != '\0' && *TCSPtr != '\n') {
			TCSPtr++;
		}

		// TCS starts from the next line:
		if (*TCSPtr != '\0') {
			TCSPtr++;
		}

		// Counting lines
		c = Src.ToCharPtr();
		while (c < TCSPtr) {
			if ('\n' == *c) {
				(*TCSLine)++;
			}
			c++;
		}
	}

	if (TESPtr != nullptr) {
		*TESPtr = '\0';
		TESPtr += cStr::Length(TESMagic);

		// Seeking end of line:
		while (*TESPtr != '\0' && *TESPtr != '\n') {
			TESPtr++;
		}

		// TCS starts from the next line:
		if (*TESPtr != '\0') {
			TESPtr++;
		}

		// Counting lines
		c = Src.ToCharPtr();
		while (c < TESPtr) {
			if ('\n' == *c) {
				(*TCSLine)++;
			}
			c++;
		}
	}

	if(FSPtr != nullptr) {
		*FSPtr = '\0';
		FSPtr += FSMagicLength;

		// Seeking end of line:
		while(*FSPtr != '\0' && *FSPtr != '\n') {
			FSPtr++;
		}

		// FS starts from the next line:
		if(*FSPtr != '\0') {
			FSPtr++;
		}

		// Counting lines
		c = Src.ToCharPtr();
		while(c < FSPtr) {
			if('\n' == *c) {
				(*FSLine)++;
			}
			c++;
		}
	}

	cAssert(VSPtr != nullptr);
	if(nullptr == VSPtr) {
		cLog::Warning("No vertex shader found.");
		return false;
	}

	cAssert(FSPtr != nullptr);
	if(nullptr == FSPtr) {
		cLog::Warning("No fragment / pixel shader found");
		return false;
	}
	
	VSText->Copy(VSPtr);
	GSText->Copy(GSPtr);
	TCSText->Copy(TCSPtr);
	TESText->Copy(TESPtr);
	FSText->Copy(FSPtr);

	int i;
    // Reverse loop (from the end to the beginning) because the list "cRender_ReplaceInShaders"
    // is sorted and "//@insert" go after "#include".
	for(i = cRender_ReplaceInShaders.Count() - 1; i >= 0; i--) {
		const cRender_ReplaceInShaders_t *R = cRender_ReplaceInShaders[i];
		VSText->Replace(R->What, R->With);
		GSText->Replace(R->What, R->With);
		TCSText->Replace(R->What, R->With);
		TESText->Replace(R->What, R->With);
		FSText->Replace(R->What, R->With);
	}

	return true;
} // cRender::Stub::LoadShader

void cRender::SetShader(const int ShaderID) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetShader(ShaderID);
}

int cRender::AddVertexFormat(const cVertex::Format &Format) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->AddVertexFormat(Format);
}

// cRender::GetVertexSize
int cRender::GetVertexSize(const int VertexFormatID) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetVertexSize(VertexFormatID);
}

int cRender::AddVertexBuffer(const void *Data, const size_t Size) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->AddVertexBuffer(Data, Size);
}

int cRender::AddIndexBuffer(const void *Data, const int IndexCount, const int IndexSize) {
	cAssert(cRender_Instance != nullptr);
	cAssert(2 == IndexSize || 4 == IndexSize);
	return cRender_Instance->AddIndexBuffer(Data, IndexCount, IndexSize);
}

void cRender::FreeVertexBuffer(const int VertexBufferID) {
	if(cRender_Instance != nullptr) {
		cRender_Instance->FreeVertexBuffer(VertexBufferID);
	}
}

void cRender::FreeIndexBuffer(const int IndexBufferID) {
	if(cRender_Instance != nullptr) {
		cRender_Instance->FreeIndexBuffer(IndexBufferID);
	}
}

void cRender::SetVertexBuffer(const int VertexBufferID) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetVertexBuffer(VertexBufferID);
}

void cRender::SetIndexBuffer(const int IndexBufferID) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->SetIndexBuffer(IndexBufferID);
}

// cRender::DrawArrays : (..., const int, ...)
void cRender::DrawArrays(const cTopology::Enum Topology) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->DrawArrays(Topology);
}

// cRender::DrawArrays : (..., const void *, ...)
void cRender::DrawArrays(const cTopology::Enum Topology, const void *VertexData, const int VertexCount) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->DrawArrays(Topology, VertexData, VertexCount);
}

// cRender::DrawIndexed : (..., const int, ...)
void cRender::DrawIndexed(const cTopology::Enum Topology) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->DrawIndexed(Topology);
}

// cRender::DrawIndexed : (..., const void *, ...)
void cRender::DrawIndexed(const cTopology::Enum Topology, const void *VertexData, const int VertexCount, const void *IndexData, const int IndexCount, const int IndexSize) {
	cAssert(cRender_Instance != nullptr);
	cAssert(2 == IndexSize || 4 == IndexSize);
	cRender_Instance->DrawIndexed(Topology, VertexData, VertexCount, IndexData, IndexCount, IndexSize);
}

//-----------------------------------------------------------------------------
// AutoConstStrings
//-----------------------------------------------------------------------------
static const char *AutoConstStrings[] = {
	"g_ViewportWidth",				// AutoConst::ViewportWidth
	"g_ViewportHeight",				// AutoConst::ViewportHeight
	"g_ViewportInvWidth",			// AutoConst::ViewportInvWidth
	"g_ViewportInvHeight",			// AutoConst::ViewportInvHeight
	"g_WorldMatrix",				// AutoConst::WorldMatrix
	"g_WorldMatrixInverse",			// AutoConst::WorldMatrixInverse
	"g_ViewerPos",					// AutoConst::ViewerPos
	"g_WorldViewMatrix",			// AutoConst::WorldViewMatrix
	"g_ViewProjectionMatrix",		// AutoConst::ViewProjectionMatrix
	"g_WorldViewProjectionMatrix",	// AutoConst::WorldViewProjectionMatrix
	"g_ProjectionMatrix",			// AutoConst::ProjectionMatrix
	"g_ScreenMatrix",				// AutoConst::ScreenMatrix
	"g_ScreenMatrixInverse",		// AutoConst::ScreenMatrixInverse
	"g_NormalMatrix",				// AutoConst::NormalMatrix
	"g_NormalViewMatrix",			// AutoConst::NormalViewMatrix
	"g_TimeSec",					// AutoConst::TimeSec
	"g_FrameTimeSec",				// AutoConst::FrameTimeSec
	"g_TextureMatrix0",				// AutoConst::TextureMatrix0
	"g_TextureMatrix1",				// AutoConst::TextureMatrix1
	"g_TextureMatrix2",				// AutoConst::TextureMatrix2
	"g_TextureMatrix3"				// AutoConst::TextureMatrix3
}; // AutoConstStrings

//-----------------------------------------------------------------------------
// cRender::AutoConst::ToString
//-----------------------------------------------------------------------------
const char * cRender::AutoConst::ToString(const int Code) {
	const int Count = sizeof(AutoConstStrings) / sizeof(const char *);

	if(Code < 0 || Code >= Count) {
		return nullptr;
	}

	return AutoConstStrings[Code];
} // cRender::AutoConst::ToString

//-----------------------------------------------------------------------------
// cRender::AutoConst::FromString
//-----------------------------------------------------------------------------
int cRender::AutoConst::FromString(const char *String) {
	const int Count = sizeof(AutoConstStrings) / sizeof(const char *);

	for(int i = 0; i < Count; i++) {
		if(cStr::Equals(String, AutoConstStrings[i])) {
			return i;
		}
	}
	
	return -1;
} // cRender::AutoConst::FromString

// cRender::GetDepthStateID
int cRender::GetDepthStateID(const bool TestEnabled, const bool WriteEnabled, const cDepthFunc::Enum Func) {
	cAssert(cRender_Instance != nullptr);
	return cRender_Instance->GetDepthStateID(TestEnabled, WriteEnabled, Func);
}

// cRender::SetDepthState
void cRender::SetDepthState(const int DepthStateID) {
	cAssert(cRender_Instance != nullptr);
	
	if(cRender_CurDepthStateID != DepthStateID) {
		cRender_Instance->SetDepthState(DepthStateID);
		cRender_CurDepthStateID = DepthStateID;
	}
}

// cRender::GetBlendStateID
int cRender::GetBlendStateID(const cBlendFactor::Enum SrcFactor, const cBlendFactor::Enum DstFactor, const cBlendMode::Enum Mode, const dword Mask) {
	cAssert(cRender_Instance != nullptr);

	return cRender_Instance->GetBlendStateID(SrcFactor, DstFactor, Mode, Mask);
}

// cRender::SetBlendState
void cRender::SetBlendState(const int BlendStateID) {
	cAssert(cRender_Instance != nullptr);

	if(cRender_CurBlendStateID != BlendStateID) {
		cRender_Instance->SetBlendState(BlendStateID);
		cRender_CurBlendStateID = BlendStateID;
	}
}

//-----------------------------------------------------------------------------
// cRender::SetCullMode
//-----------------------------------------------------------------------------
void cRender::SetCullMode(const cCullMode::Enum CullMode) {
	cAssert(cRender_Instance != nullptr);

	if(cRender_CurCullMode != CullMode) {
		cRender_Instance->SetCullMode(CullMode);
		cRender_CurCullMode = CullMode;
	}
} // cRender::SetCullMode

void cRender::Clear(const bool ClearColor, const bool ClearDepth, const cColor &Color, const float Depth) {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->Clear(ClearColor, ClearDepth, Color, Depth);
}

// cRender::BeginFrame
void cRender::BeginFrame() {
	cAssert(cRender_Instance != nullptr);
	if(!cRender_BeginFrame) {
		cRender_Instance->BeginFrame();
		cRender_BeginFrame = true;
	}
}

// cRender::EndFrame
void cRender::EndFrame() {
	cAssert(cRender_Instance != nullptr);
	if(cRender_BeginFrame) {
		cRender_Instance->EndFrame();
		cRender_BeginFrame = false;
	}
}

// cRender::Finish
void cRender::Finish() {
	cAssert(cRender_Instance != nullptr);
	cRender_Instance->Finish();
}

//-----------------------------------------------------------------------------
// cFont::Read
//-----------------------------------------------------------------------------
bool cFont::Read(const cFile &From) {
	word Magic;
	if(!From.ReadWord(&Magic)) {
		return false;
	}
	if(Magic != ('c' | 'f' << 8)) {
		return false;
	}
	byte Version;
	if(!From.ReadByte(&Version)) {
		return false;
	}
	if(Version != 0) {
		return false;
	}
	if(!From.ReadInt(&CellAscent)) {
		return false;
	}
	if(!From.ReadInt(&CellDescent)) {
		return false;
	}
	if(!From.ReadInt(&LineSpacing)) {
		return false;
	}
	int Count;
	if(!From.ReadInt(&Count)) {
		return false;
	}
	if(Count < 0 || Count > (0xffff + 1)) {
		return false;
	}
	Glyphs.SetCount(Count);
	int i;
	for(i = 0; i < Glyphs.Count(); i++) {
		Glyph &g = Glyphs[i];
		if(!From.ReadWord(&g.Char)) {
			return false;
		}
		if(!From.ReadRect(&g.TexCoord)) {
			return false;
		}
		if(!From.ReadVec2i(&g.Origin)) {
			return false;
		}
		if(!From.ReadInt(&g.SizeX)) {
			return false;
		}
		if(!From.ReadInt(&g.SizeY)) {
			return false;
		}
		if(!From.ReadVec2i(&g.CellInc)) {
			return false;
		}
	}
	if(!From.ReadInt(&GlyphBorder)) {
		return false;
	}
	return true;
} // cFont::Read

// cFont::Write
void cFont::Write(cFile *To) const {
	word Magic = ('c' | 'f' << 8);
	byte Version = 0;
	To->WriteWord(Magic);
	To->WriteByte(Version);
	To->WriteInt(CellAscent);
	To->WriteInt(CellDescent);
	To->WriteInt(LineSpacing);
	To->WriteInt(Glyphs.Count());
	int i;
	for(i = 0; i < Glyphs.Count(); i++) {
		const Glyph &g = Glyphs[i];
		To->WriteWord(g.Char);
		To->WriteRect(g.TexCoord);
		To->WriteVec2i(g.Origin);
		To->WriteInt(g.SizeX);
		To->WriteInt(g.SizeY);
		To->WriteVec2i(g.CellInc);
	}
	To->WriteInt(GlyphBorder);
}

//-----------------------------------------------------------------------------
// cRender::GetFontID
//-----------------------------------------------------------------------------
int cRender::GetFontID(const char *FontName) {
	cAssert(FontName != nullptr);

	int i;
	cStr Fn, Tn;
	cFile File;
	cRender_FONT F;
	int FontID = -1;
	
	if(cStr::Length(FontName) < 1) {
		return -1; // No name.
	}
	
	// Searching loaded fonts:
	for(i = 0; i < cRender_Fonts.Count(); i++) {
		if(cStr::EqualsNoCase(cRender_Fonts[i].Name.ToCharPtr(), FontName)) {
			return i; // Font is already loaded.
		}
	}
	
	cLog::Message(" - Loading font \"%s\"...", FontName);

	// Font should be in these files:
	Fn = cStr::Format("data/Fonts/%s.cFont", FontName); // Font format.
	Tn = cStr::Format("data/Fonts/%s.png", FontName); // Font texture.

	// Loading font format file:
	if(!cIO::LoadFile(Fn, &File)) {
		return -1;
	}
	
	// Loading font format:
	if(F.Format.Read(File)) {
		cLog::Message("Loaded font format file \"%s\"", Fn.ToCharPtr());
		// Loading font texture file:
		if((F.TextureID = GetTextureID(Tn)) != -1) {
			F.Name = FontName;
			FontID = cRender_Fonts.Add(F);
		}
	} else {
		cLog::Warning("Font format file \"%s\" has wrong size.", Fn.ToCharPtr());
	}
	
	return FontID;
} // cRender::GetFontID

// cRender::GetFont
const cFont * cRender::GetFont(const int FontID) {
	if(FontID < 0 || FontID >= cRender_Fonts.Count()) {
		return nullptr; // Invalid font ID
	}
	return &cRender_Fonts[FontID].Format;
}

// cRender::GetFontTextureID
int cRender::GetFontTextureID(const int FontID) {
	if(FontID < 0 || FontID >= cRender_Fonts.Count()) {
		return -1; // Invalid font ID
	}
	return cRender_Fonts[FontID].TextureID;
}

// cRender::EnsurePrintable
void cRender::EnsurePrintable(const int FontID, cStr *Text) {
	cStr S;
	int i, j;
	const cFont *F = GetFont(FontID);
	cFont::Glyph g;
	if(F != nullptr) {
		for(i = 0; i < Text->Length(); i++) {
			g.Char = (*Text)[i];
			j = F->Glyphs.BinarySearch(g, cFont::Glyph::Compare);
			if(j != -1) {
				S.Append((char)g.Char);
			}
		}
		Text->Copy(S);
	}
}

// cRender::DrawString : const cRect (const int, const char *, const float, const float)
const cRect cRender::DrawString(const int FontID, const char *Str, const float X, const float Y) {
	return DrawString(FontID, Str, X, Y, StringAlign::TopLeft, nullptr);
}

// cRender::DrawString : const cRect (const int, const char *, const float, const float, const int)
const cRect cRender::DrawString(const int FontID, const char *Str, const float X, const float Y, const int Align) {
	return DrawString(FontID, Str, X, Y, Align, nullptr);
}

// cRender::DrawString : const cRect (const int, const char *, const float, const flaot, const cColor &)
const cRect cRender::DrawString(const int FontID, const char *Str, const float X, const float Y, const cColor &Color) {
	return DrawString(FontID, Str, X, Y, StringAlign::TopLeft, &Color);
}

// cRender::DrawString : const cRect (const int, const char *, const float, const float, const int, const cColor &
const cRect cRender::DrawString(const int FontID, const char *Str, const float X, const float Y, const int Align, const cColor &Color) {
	return DrawString(FontID, Str, X, Y, Align, &Color);
}

// cRender::DrawString : const cRect (const int, const char *, const cVec2 &)
const cRect cRender::DrawString(const int FontID, const char *Str, const cVec2 &Pos) {
	return DrawString(FontID, Str, Pos.x, Pos.y, StringAlign::TopLeft, nullptr);
}

// cRender::DrawString : const cRect (const int, const char *, const cVec2 &, const int)
const cRect cRender::DrawString(const int FontID, const char *Str, const cVec2 &Pos, const int Align) {
	return DrawString(FontID, Str, Pos.x, Pos.y, Align, nullptr);
}

// cRender::DrawString : const cRect (const int, const char *, const cVec2 &, const cColor &)
const cRect cRender::DrawString(const int FontID, const char *Str, const cVec2 &Pos, const cColor &Color) {
	return DrawString(FontID, Str, Pos.x, Pos.y, StringAlign::TopLeft, &Color);
}

// cRender::DrawString : const cRect (const int, const char *, const cVec2 &, const int, const cColor &)
const cRect cRender::DrawString(const int FontID, const char *Str, const cVec2 &Pos, const int Align, const cColor &Color) {
	return DrawString(FontID, Str, Pos.x, Pos.y, Align, &Color);
}

// cRender::DrawString : (const int, const char *, const cVec3 &)
const cRect cRender::DrawString(const int FontID, const char *Str, const cVec3 &Pos) {
	return DrawString(FontID, Str, Pos, StringAlign::TopLeft);
}

// cRender::DrawString : (const int, const char *, const cVec3 &, const int)
const cRect cRender::DrawString(const int FontID, const char *Str, const cVec3 &Pos, const int Align) {
	cVec3 p = cVec3::TransformCoordinate(Pos, GetWorldMatrix());
	if(GetViewer()->CullPoint(p)) {
		return cRect::Zero;
	}
	const cVec3 ScreenPos = cVec3::TransformCoordinate(p, GetViewer()->GetViewProjectionScreenMatrix());
	return DrawString(FontID, Str, cMath::Round(ScreenPos.x), cMath::Round(ScreenPos.y), Align, nullptr);
}

// cRender::DrawString : const cRect (const int, const char *, const cVec3 &, const cColor &)
const cRect cRender::DrawString(const int FontID, const char *Str, const cVec3 &Pos, const cColor &Color) {
	return DrawString(FontID, Str, Pos, StringAlign::TopLeft, Color);
}

// cRender::DrawString : const cRect (const int, const char *, const cVec3 &, const int, const cColor &)
const cRect cRender::DrawString(const int FontID, const char *Str, const cVec3 &Pos, const int Align, const cColor &Color) {
	if(GetViewer()->CullPoint(Pos)) {
		return cRect::Zero;
	}
	const cVec3 ScreenPos = cVec3::TransformCoordinate(Pos, GetViewer()->GetViewProjectionScreenMatrix());
	return DrawString(FontID, Str, ScreenPos.x, ScreenPos.y, Align, &Color);
}

//-----------------------------------------------------------------------------
// cRender::DrawString
//-----------------------------------------------------------------------------
const cRect cRender::DrawString(const int FontID, const cList<word> &UniChars, const float X, const float Y, const int Align, const cColor *OverrideColor) {
	if(FontID < 0 || FontID >= cRender_Fonts.Count()) {
		return cRect::Zero; // Invalid font ID
	}
	if(UniChars.IsEmpty()) {
		return cRect::Zero; // Nothing to draw / measure
	}
	
	const cRender_FONT &F = cRender_Fonts[FontID];

	static cList<cVertex::PositionColoredTextured> vb;
	static cList<int> ib;

	vb.Clear();
	ib.Clear();

	cVertex::PositionColoredTextured u[4];
	u[0].Color = u[1].Color = u[2].Color = u[3].Color = OverrideColor != nullptr ? OverrideColor->ToDword() : cColor::Black.ToDword();
	cColor Color;
	
	// To ensure that chars will be crisp when no scale is applied, we should round position
	const cVec2 Ref(cMath::Round(X), cMath::Round(Y));
	cVec2 CurPos(Ref);
	
	cRect rc;
	rc.SetEmpty();

	// Enumerating string chars and filling vb & ib if "CalcRect" is not specified
	int i, j, i0;
	cFont::Glyph t;
	cRect gr, b;
	for(i = 0; i < UniChars.Count(); i++) {
		t.Char = UniChars[i];
		j = F.Format.Glyphs.BinarySearch(t, cFont::Glyph::Compare);
		if(-1 == j) { // The font doesn't have this char
			j = 31; // Question mark
		}
		const cFont::Glyph &g = F.Format.Glyphs[j];

		// Space
		if(' ' == t.Char) {
			CurPos.x += float(g.CellInc[Xelt]);
			CurPos.y += float(g.CellInc[Yelt]);
			continue;
		}

		// Calculating glyph rect for drawing mode or if "TightRect" is specified even in "CalcRect" mode
		if(!(Align & StringAlign::CalcRect) || (Align & StringAlign::TightRect)) {
			gr.SetLeft(CurPos.x + float(g.Origin[Xelt]));
			gr.SetTop(CurPos.y - float(g.Origin[Yelt]));
			gr.SetWidth(cAlign::Left, float(g.SizeX));
			gr.SetHeight(cAlign::Top, float(g.SizeY));

			// Updating tight bounding rect
			if(Align & StringAlign::TightRect) {
				// Taking into account glyph border
				b = gr;
				b.Inflate(-(float)F.Format.GlyphBorder);
				rc.AddRect(b);
			}
			
			// Appending textured char rect to vb & ib if "CalcRect" is not specified
			if(!(Align & StringAlign::CalcRect)) {
				const cRect &tc = g.TexCoord;
				
				i0 = vb.Count();
				
				u[0].Pos.Set(gr.GetTopLeft(), 0.0f);
				u[0].TexCoord = tc.GetTopLeft();
				
				u[1].Pos.Set(gr.GetBottomLeft(), 0.0f);
				u[1].TexCoord = tc.GetBottomLeft();

				u[2].Pos.Set(gr.GetBottomRight(), 0.0f);
				u[2].TexCoord = tc.GetBottomRight();

				u[3].Pos.Set(gr.GetTopRight(), 0.0f);
				u[3].TexCoord = tc.GetTopRight();
				
				vb.AddRange(u, 4);
				
				ib.Add(i0);
				ib.Add(i0 + 1);
				ib.Add(i0 + 2);
				
				ib.Add(i0);
				ib.Add(i0 + 2);
				ib.Add(i0 + 3);
			}
		}
		
		// Go to next char cell
		CurPos.x += float(g.CellInc[Xelt]);
		CurPos.y += float(g.CellInc[Yelt]);
	}

	// If "TightRect" is not specified, we should estimate boundind rect base upon font cells
	if(!(Align & StringAlign::TightRect)) {
		rc.AlignTopLeft(Ref);
		rc.SetWidth(cAlign::Left, CurPos.x - Ref.x);
		rc.SetHeight(cAlign::Top, float(F.Format.LineSpacing));
	}

	cVec2 Shift(0.0f);

	// Satisfying horizontal alignments
	if(Align & StringAlign::Center) { // Center
		// To ensure that chars will be crisp when no scale is applied,
		// we should cut off fraction part of center value
		Shift.x = Ref.x - cMath::Floor(rc.GetCenterX());
	} else if(Align & StringAlign::Right) { // Right
		Shift.x = Ref.x - rc.GetRight();
	} else { // Left
		// Left is already satisfied for non tight bounding rect
		if(Align & StringAlign::TightRect) {
			Shift.x = Ref.x - rc.GetLeft();
		}
	}
	
	// Satisfying vertical alignment
	if(Align & StringAlign::Middle) { // Middle
		// To ensure that chars will be crisp when no scale is applied,
		// we should cut off fraction part of middle value
		Shift.y = Ref.y - cMath::Floor(rc.GetCenterY());
	} else if(Align & StringAlign::BaseLine) { // BaseLine
		Shift.y = float(F.Format.CellAscent);
	} else if(Align & StringAlign::Bottom) { // Bottom
		Shift.y = Ref.y - rc.GetBottom();
	} else { // Top
		// Top is already satisfied for non tight bounding rect
		if(Align & StringAlign::TightRect) {
			Shift.y = Ref.y - rc.GetTop();
		}
	}
	
	// Applying shift to align string according to flags and bounding rect type
	if(!Shift.IsZero()) {
		// to bounding rect
		rc.Translate(Shift);
		
		// to vb
		for(i = 0; i < vb.Count(); i++) {
			vb[i].Pos.ToVec2() += Shift;
		}
	}

	// Actually drawing the string
	if(!ib.IsEmpty()) {
		// We should not apply wireframe mode to strings
		bool Wireframe = GetWireframe();
		if(Wireframe) {
			SetWireframe(false);
		}

		static int Blend = GetBlendStateID(cBlendFactor::SrcAlpha, cBlendFactor::OneMinusSrcAlpha);
		SetBlendState(Blend);
		SetDepthState(-1);
		SetCullMode(cCullMode::None);
		
		static int ShaderID = GetShaderID("Font", cVertex::PositionColoredTextured::FormatID);
		static int SamplerID = GetSamplerID(ShaderID, "s_Glyphs");
		static int SamplerStateID = GetSamplerStateID(cFilter::Linear, cAddressMode::Clamp);
		SetShader(ShaderID);
		SetTexture(SamplerID, F.TextureID, SamplerStateID);
		DrawIndexed(cTopology::TriangleList, vb.ToPtr(), vb.Count(), ib.ToPtr(), ib.Count());

		SetWireframe(Wireframe);
	}

	return rc; // Function result - string bounding rect
} // cRender::DrawString

// cRender::DrawString
const cRect cRender::DrawString(const int FontID, const char *Str, const float X, const float Y, const int Align, const cColor *OverrideColor) {
	cList<word> UniChars;
	cStr S(Str);
	S.DecodeUTF8(&UniChars);
	return DrawString(FontID, UniChars, X, Y, Align, OverrideColor);
}

// cRender::MeasureStringTightRect : const cRect (const int, const char *)
const cRect cRender::MeasureStringTightRect(const int FontID, const char *Str) {
	return DrawString(FontID, Str, 0.0f, 0.0f, StringAlign::TightRect | StringAlign::CalcRect, nullptr);
}

// cRender::MeasureStringCells : const cRect (const int, const char *)
const cRect cRender::MeasureStringCells(const int FontID, const char *Str) {
	return DrawString(FontID, Str, 0.0f, 0.0f, StringAlign::CalcRect, nullptr);
}

// cRender::DrawLine : (const int, ...)
void cRender::DrawLine(const int X0, const int Y0, const int X1, const int Y1, const cColor &Color) {
	const cVec2 u[2] = {
		cVec2(float(X0), float(Y0)),
		cVec2(float(X1), float(Y1))
	};
	DrawLine(u[0], u[1], Color);
}

// cRender::DrawLine : (const float, ...)
void cRender::DrawLine(const float X0, const float Y0, const float X1, const float Y1, const cColor &Color) {
	const cVec2 u[2] = {
		cVec2(X0, Y0),
		cVec2(X1, Y1)
	};
	DrawLine(u[0], u[1], Color);
}

//-----------------------------------------------------------------------------
// cRender::DrawLine : void (const cVec2 &, ...)
//-----------------------------------------------------------------------------
void cRender::DrawLine(const cVec2 &P0, const cVec2 &P1, const cColor &Color) {
	cVertex::PositionColored u[2];
	u[0].Color = u[1].Color = Color.ToDword();
	u[0].Pos.Set(P0, 0.0f);
	u[1].Pos.Set(P1, 0.0f);
	
	static int Sh = GetShaderID("LinesPointsUI", cVertex::PositionColored::FormatID);
	SetShader(Sh);
	DrawArrays(cTopology::LineList, u, 2);
}

//-----------------------------------------------------------------------------
// cRender::DrawBounds
//-----------------------------------------------------------------------------
void cRender::DrawBounds(const cBounds &bs, const cColor &Color) {
	if(bs.IsEmpty()) {
		return;
	}

	static cList<cVertex::PositionColored> vb;
	vb.Clear();

	static cList<int> ib;
	ib.Clear();

	cVertex::PositionColored u;
	u.Color = Color.ToDword();

	const cVec3 &Min = bs.GetMin();
	const cVec3 &Max = bs.GetMax();

	u.Pos = Min;
	vb.Add(u);

	u.Pos.Set(Min.x, Min.y, Max.z);
	vb.Add(u);

	u.Pos.Set(Max.x, Min.y, Max.z);
	vb.Add(u);

	u.Pos.Set(Max.x, Min.y, Min.z);
	vb.Add(u);

	u.Pos.Set(Min.x, Max.y, Min.z);
	vb.Add(u);

	u.Pos.Set(Min.x, Max.y, Max.z);
	vb.Add(u);

	u.Pos.Set(Max.x, Max.y, Max.z);
	vb.Add(u);

	u.Pos.Set(Max.x, Max.y, Min.z);
	vb.Add(u);

	ib.Add(0);
	ib.Add(1);
	
	ib.Add(1);
	ib.Add(2);

	ib.Add(2);
	ib.Add(3);

	ib.Add(3);
	ib.Add(0);

	ib.Add(4);
	ib.Add(5);
	
	ib.Add(5);
	ib.Add(6);

	ib.Add(6);
	ib.Add(7);

	ib.Add(7);
	ib.Add(4);

	ib.Add(0);
	ib.Add(4);

	ib.Add(1);
	ib.Add(5);

	ib.Add(2);
	ib.Add(6);

	ib.Add(3);
	ib.Add(7);

	static int Sh = GetShaderID("Colored", cVertex::PositionColored::FormatID);
	SetShader(Sh);
	DrawIndexed(cTopology::LineList, vb.ToPtr(), vb.Count(), ib.ToPtr(), ib.Count());
} // cRender::DrawBounds

// cRender::DrawRectangle : (const int, ...)
void cRender::DrawRectangle(const int X, const int Y, const int Width, const int Height, const cColor &Color) {
	DrawRectangle(cRect(cRect::SizeCtor, (float)X, (float)Y, (float)Width, (float)Height), Color);
}

// cRender::DrawRectangle : (const float, ...)
void cRender::DrawRectangle(const float X, const float Y, const float Width, const float Height, const cColor &Color) {
	DrawRectangle(cRect(cRect::SizeCtor, X, Y, Width, Height), Color);
}

//-----------------------------------------------------------------------------
// cRender::DrawRectangle : (const cRect &, ...)
//-----------------------------------------------------------------------------
void cRender::DrawRectangle(const cRect &rc, const cColor &Color) {
	cVertex::PositionColored vb[8];
	
	dword C = Color.ToDword();
	int i;
	for(i = 0; i < 8; i++) {
		vb[i].Color = C;
	}

	static bool ATI = GetVendor().Contains("RADEON");
	
	// These offsets are done to match lines rect with solid rect
	vb[0].Pos.Set(rc.GetBottomLeft(), 0.0f);
	vb[1].Pos.Set(rc.GetTopLeft() - cVec2(0.0f, 1.0f), 0.0f);

	vb[2].Pos.Set(rc.GetBottomLeft() + cVec2(1.0f, 0.0f), 0.0f);
	vb[3].Pos.Set(rc.GetBottomRight() - cVec2(1.0f, 0.0f), 0.0f);

	vb[4].Pos.Set(rc.GetBottomRight() - cVec2(1.0f, 0.0f), 0.0f);
	if(cRenderType::OpenGL == GetType() || ATI) {
		vb[5].Pos.Set(rc.GetTopRight() - cVec2(1.0f, 1.0f), 0.0f);
	} else {
		vb[5].Pos.Set(rc.GetTopRight() - cVec2(1.0f, 0.0f), 0.0f);
	}
	
	vb[6].Pos.Set(rc.GetTopLeft() - cVec2(0.0f, 1.0f), 0.0f);
	vb[7].Pos.Set(rc.GetTopRight() - cVec2(0.0f, 1.0f), 0.0f);

	static int Sh = GetShaderID("LinesPointsUI", cVertex::PositionColored::FormatID);
	SetShader(Sh);
	DrawArrays(cTopology::LineList, vb, 8);
} // cRender::DrawRectangle : (const cRect &, ...)

//*****************************************************************************
// cRender::DrawRectangle(s)Dot
//*****************************************************************************
void cRender::DrawRectangleDot(const cRect &Rc, const cColor &Color) {
	DrawRectanglesDot(&Rc, 1, Color);
}

void cRender::DrawRectanglesDot(const cRect *Rects, const int Count, const cColor &Color) {
	static cList<cVertex::PositionColored> vb;
	vb.Clear();

	cVertex::PositionColored u;
	u.Color = Color.ToDword();
	int i;
	float X0, X1, X, Y0, Y1, Y, T0, T1, B0, B1, L0, L1, R0, R1, M;
	bool EX, EY;

	for(i = 0; i < Count; i++) {
		const cRect &Rc = Rects[i];
		if(Rc.IsEmpty()) {
			continue;
		}
		// Round bounds
		X0 = cMath::Round(Rc.GetLeft());
		X1 = cMath::Round(Rc.GetRight());
		Y0 = cMath::Round(Rc.GetBottom());
		Y1 = cMath::Round(Rc.GetTop());
		// Sides even flags
		M = (X1 - X0) / 2.0f;
		EX = M - cMath::Floor(M) == 0.0f;
		M = (Y1 - Y0) / 2.0f;
		EY = M - cMath::Floor(M) == 0.0f;
		// Considering rasterization rules
		X1 -= 1.0f;
		Y1 -= 1.0f;
		// Calcing optimal dots fill
		T0 = X0; T1 = X1; B0 = X0; B1 = X1;
		L0 = Y0; L1 = Y1; R0 = Y0; R1 = Y1;
		if(!EX && !EY) {
			T0 = B0 = X0 + 1.0f;
			T1 = B1 = X1 - 1.0f;
			L0 = R0 = Y0 + 1.0f;
			L1 = R1 = Y1 - 1.0f;
		} else if(EX && !EY) {
			T0 = B0 = X0;
			T1 = B1 = X1 - 1.0f;
			L0 = Y0;
			L1 = Y1;
			R0 = Y0 + 1.0f;
			R1 = Y1 - 1.0f;
		} else if(!EX && EY) {
			T0 = X0;
			T1 = X1 - 1.0f;

			L0 = Y0 + 1.0f;
			L1 = Y1 - 1.0f;

			B0 = X0 + 1.0f;
			B1 = X1 - 1.0f;

			R0 = Y0 + 1.0f;
			R1 = Y1;
		} else {
			cAssert(EX && EY);
			T0 = X0;
			T1 = X1 - 1.0f;

			L0 = Y0 + 1.0f;
			L1 = Y1 - 1.0f;

			B0 = X0 + 1.0f;
			B1 = X1 - 1.0f;

			R0 = Y0;
			R1 = Y1 - 1.0f;
		}
		// Top
		for(X = T0; X <= T1; X += 2.0f) {
			u.Pos.Set(X, Y1, 0.0f);
			vb.Add(u);
		}
		// Bottom
		for(X = B0; X <= B1; X += 2.0f) {
			u.Pos.Set(X, Y0, 0.0f);
			vb.Add(u);
		}
		// Left
		for(Y = L0; Y <= L1; Y += 2.0f) {
			u.Pos.Set(X0, Y, 0.0f);
			vb.Add(u);
		}
		// Right
		for(Y = R0; Y <= R1; Y += 2.0f) {
			u.Pos.Set(X1, Y, 0.0f);
			vb.Add(u);
		}
	}

	if(!vb.IsEmpty()) {
		static int Sh = GetShaderID("LinesPointsUI", cVertex::PositionColored::FormatID);
		SetShader(Sh);
		DrawArrays(cTopology::PointList, vb.ToPtr(), vb.Count());
	}
}

//*****************************************************************************
// cRender::FillRectangle(s)Solid
//*****************************************************************************

// cRender::FillRectangleSolid : (const int, ...)
void cRender::FillRectangleSolid(const int X, const int Y, const int Width, const int Height, const cColor &Color) {
	cRect Rc(cRect::SizeCtor, (float)X, (float)Y, (float)Width, (float)Height);
	FillRectanglesSolid(&Rc, 1, Color);
}

// cRender::FillRectangleSolid : (const float, ...)
void cRender::FillRectangleSolid(const float X, const float Y, const float Width, const float Height, const cColor &Color) {
	cRect Rc(cRect::SizeCtor, X, Y, Width, Height);
	FillRectanglesSolid(&Rc, 1, Color);
}

// cRender::FillRectangleSolid : (const cRect &, ...)
void cRender::FillRectangleSolid(const cRect &Rc, const cColor &Color) {
	FillRectanglesSolid(&Rc, 1, Color);
}

//--------------------------------------------------------------------------------------------
// cRender::FillRectanglesSolid
//--------------------------------------------------------------------------------------------
void cRender::FillRectanglesSolid(const cRect *Rects, const int Count, const cColor &Color, const int CustomShader) {
	static cList<cVertex::PositionColored> vb;
	static cList<int> ib;

	vb.Clear();
	ib.Clear();

	cVertex::PositionColored u[4];
	u[0].Color = u[1].Color = u[2].Color = u[3].Color = Color.ToDword();
	int i, i0;

	for(i = 0; i < Count; i++) {
		const cRect &rc = Rects[i];
		if(rc.IsEmpty()) {
			continue;
		}

		u[0].Pos.Set(rc.GetTopLeft(), 0.0f);
		u[1].Pos.Set(rc.GetBottomLeft(), 0.0f);
		u[2].Pos.Set(rc.GetBottomRight(), 0.0f);
		u[3].Pos.Set(rc.GetTopRight(), 0.0f);

		i0 = vb.Count();
		vb.AddRange(u, 4);

		ib.Add(i0);
		ib.Add(i0 + 1);
		ib.Add(i0 + 2);

		ib.Add(i0);
		ib.Add(i0 + 2);
		ib.Add(i0 + 3);
	}

	if(!ib.IsEmpty()) {
		SetDepthState(-1);
		SetCullMode(cCullMode::None);

		static int ShaderID = cRender::GetShaderID("SolidUI", cVertex::PositionColored::FormatID);
        if(CustomShader != -1) {
            SetShader(CustomShader);
        } else {
            SetShader(ShaderID);
        }
		DrawIndexed(cTopology::TriangleList, vb.ToPtr(), vb.Count(), ib.ToPtr(), ib.Count());
	}
} // cRender::FillRectanglesSolid

//---------------------------------------------------------------------------------------------------------------------
// cRender::FillRectangleDither
//---------------------------------------------------------------------------------------------------------------------
void cRender::FillRectangleDither(const cRect &Rc, const cVec2 &Orig, const cColor &Color0, const cColor &Color1) {
	if(Rc.IsEmpty()) {
		return;
	}
	
	cVertex::PositionOnly vb[4];
	vb[0].Pos.Set(Rc.GetTopLeft(), 0.0f);
	vb[1].Pos.Set(Rc.GetBottomLeft(), 0.0f);
	vb[2].Pos.Set(Rc.GetBottomRight(), 0.0f);
	vb[3].Pos.Set(Rc.GetTopRight(), 0.0f);
	const int ib[6] = { 0, 1, 2, 0, 2, 3 };
	
	static int Blend = GetBlendStateID(cBlendFactor::SrcAlpha, cBlendFactor::OneMinusSrcAlpha);
	SetBlendState(Blend);
	SetDepthState(-1);
	SetCullMode(cCullMode::None);

	static int ShaderID = cRender::GetShaderID("DitherUI", cVertex::PositionOnly::FormatID);
	static int OrigID = cRender::GetShaderConstID(ShaderID, "c_Orig");
	static int Color0ID = cRender::GetShaderConstID(ShaderID, "c_Color0");
	static int Color1ID = cRender::GetShaderConstID(ShaderID, "c_Color1");
	
	cRender::SetShader(ShaderID);
	cRender::SetShaderConst(OrigID, Orig.ToRound());
	cRender::SetShaderConst(Color0ID, Color0);
	cRender::SetShaderConst(Color1ID, Color1);
	cRender::DrawIndexed(cTopology::TriangleList, vb, 4, ib, 6);
} // cRender::FillRectangleDither

//*****************************************************************************
// cRender::FillRectangle(s)LinearGradient
//*****************************************************************************

// cRender::FillRectangleLinearGradient : (const int, ...
void cRender::FillRectangleLinearGradient(const int X, const int Y, const int Width, const int Height, const cColor &Color0, const cColor &Color1, const cRender::LinearGradient::Enum Mode) {
	cRect Rc(cRect::SizeCtor, (float)X, (float)Y, (float)Width, (float)Height);
	FillRectanglesLinearGradient(&Rc, 1, Color0, Color1, Mode);
}

// cRender::FillRectangleLinearGradient : (const float, ...)
void cRender::FillRectangleLinearGradient(const float X, const float Y, const float Width, const float Height, const cColor &Color0, const cColor &Color1, const cRender::LinearGradient::Enum Mode) {
	cRect Rc(cRect::SizeCtor, X, Y, Width, Height);
	FillRectanglesLinearGradient(&Rc, 1, Color0, Color1, Mode);
}

// cRender::FillRectangleLinearGradient : (const cRect &, ...)
void cRender::FillRectangleLinearGradient(const cRect &Rc, const cColor &Color0, const cColor &Color1, const cRender::LinearGradient::Enum Mode) {
	FillRectanglesLinearGradient(&Rc, 1, Color0, Color1, Mode);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
// cRender::FillRectanglesLinearGradient : (const cRect *, ...)
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------
void cRender::FillRectanglesLinearGradient(const cRect *Rects, const int Count, const cColor &Color0, const cColor &Color1, const cRender::LinearGradient::Enum Mode) {
	static cList<cVertex::PositionColored> vb;
	static cList<int> ib;

	vb.Clear();
	ib.Clear();

	cVertex::PositionColored u[4];
	int i, i0, is(0), il(0);
	cVec2 Along, V, N;
	float L, s, l;
	cSeg S;
	int From(0), To(0);
	
	if(LinearGradient::Horizontal == Mode) {
		u[0].Color = u[1].Color = Color0.ToDword();
		u[2].Color = u[3].Color = Color1.ToDword();
	} else if(LinearGradient::Vertical == Mode) {
		u[0].Color = u[3].Color = Color0.ToDword();
		u[1].Color = u[2].Color = Color1.ToDword();
	} else if(LinearGradient::ForwardDiagonal == Mode) {
		u[0].Color = Color0.ToDword();
		u[2].Color = Color1.ToDword();
		From = 0;
		To = 2;
		is = 1;
		il = 3;
	} else if(LinearGradient::BackwardDiagonal == Mode) {
		u[1].Color = Color0.ToDword();
		u[3].Color = Color1.ToDword();
		From = 1;
		To = 3;
		is = 0;
		il = 2;
	}

	for(i = 0; i < Count; i++) {
		const cRect &Rc = Rects[i];

		u[0].Pos.Set(Rc.GetTopLeft(), 0.0f);
		u[1].Pos.Set(Rc.GetBottomLeft(), 0.0f);
		u[2].Pos.Set(Rc.GetBottomRight(), 0.0f);
		u[3].Pos.Set(Rc.GetTopRight(), 0.0f);

		if(LinearGradient::ForwardDiagonal == Mode || LinearGradient::BackwardDiagonal == Mode) {
			Along = u[To].Pos.ToVec2() - u[From].Pos.ToVec2();
			L = Along.Length();
			if(!cMath::IsZero(L)) {
				V = Along.ToNormal();
				N = V.ToPerpCw();
				S.SetFromRay(u[From].Pos, cVec3(N, 0.0f));
				s = cSeg::Distance(cSeg::Line, S, u[is].Pos);
				S.SetFromRay(u[To].Pos, cVec3(N, 0.0f));
				l = cSeg::Distance(cSeg::Line, S, u[il].Pos);
				u[is].Color = cColor::Lerp(Color0, Color1, s / L).ToDword();
				u[il].Color = cColor::Lerp(Color1, Color0, l / L).ToDword();
			}
		}
		
		i0 = vb.Count();
		vb.AddRange(u, 4);

		ib.Add(i0);
		ib.Add(i0 + 1);
		ib.Add(i0 + 2);

		ib.Add(i0);
		ib.Add(i0 + 2);
		ib.Add(i0 + 3);
	}

	if(!ib.IsEmpty()) {
		static int Blend = GetBlendStateID(cBlendFactor::SrcAlpha, cBlendFactor::OneMinusSrcAlpha);
		SetBlendState(Blend);
		SetDepthState(-1);
		SetCullMode(cCullMode::None);

		static int ShaderID = cRender::GetShaderID("SolidUI", cVertex::PositionColored::FormatID);
		SetShader(ShaderID);
		DrawIndexed(cTopology::TriangleList, vb.ToPtr(), vb.Count(), ib.ToPtr(), ib.Count());
	}
} // cRender::FillRectanglesLinearGradient

//-------------------------------------------------------------------------------------------------------
// cRender::FillSegmentRadialGradient
//-------------------------------------------------------------------------------------------------------
void cRender::FillSegmentRadialGradient(const cVec2 &From, const cVec2 &To,
										const float SegRadius, const float GradRadius, const float Power,
										const int Style,
										const cColor &CenterColor, const cColor &BoundColor) {

	const bool HalfCcw = (Style & RadialGradient::HalfCcwFlat) || (Style & RadialGradient::HalfCcwSharp);
	const bool HalfCw = (Style & RadialGradient::HalfCwFlat) || (Style & RadialGradient::HalfCwSharp);
	const bool HalfFlat = (Style & RadialGradient::HalfCcwFlat) || (Style & RadialGradient::HalfCwFlat);

	// Point case
	cVec2 F = cVec2::Min(From, To);
	cVec2 T = cVec2::Max(From, To);

	if(F == T) {
		// If "From" and "To" are the same this segment is degraded to point.
		// So we should shift one end a little.
		T += cVec2(cMath::Epsilon, 0.0f);
	}
	
	// vb
	cVertex::PositionOnly vb[4];
	
	cVec2 V = cVec2::Normalize(T - F);
	cAssert(V.IsNormalized()); // Should be, even if segment is degraded to point
	cVec2 N = V.ToPerpCw();
	cVec2 Ccw = HalfCw ? cVec2::Zero : - SegRadius * N; // No shift to counter - clockwise side if we are drawing only clockwise half
	cVec2 Cw = HalfCcw ? cVec2::Zero : SegRadius * N; // No shift to clockwise side if we are drawing only counter - clockwise half
	vb[0].Pos.Set(F - SegRadius * V + Ccw, 0.0f);
	vb[1].Pos.Set(F - SegRadius * V + Cw, 0.0f);
	vb[2].Pos.Set(T + SegRadius * V + Cw, 0.0f);
	vb[3].Pos.Set(T + SegRadius * V + Ccw, 0.0f);

	// ib
	const int ib[6] = { 0, 1, 2, 0, 2, 3 };

	// Draw
	static int Blend = GetBlendStateID(cBlendFactor::SrcAlpha, cBlendFactor::OneMinusSrcAlpha);
	SetBlendState(Blend);
	SetDepthState(-1);
	SetCullMode(cCullMode::None);

	static int Sh[2] = {
		GetShaderID("RadialGradientUI", cVertex::PositionOnly::FormatID, "#define HORIZONTAL"),
		GetShaderID("RadialGradientUI", cVertex::PositionOnly::FormatID) // VERTICAL
	};
	static int Sh_SegFrom[2] = {
		GetShaderConstID(Sh[0], "c_SegFrom"),
		GetShaderConstID(Sh[1], "c_SegFrom")
	};
	static int Sh_SegTo[2] = {
		GetShaderConstID(Sh[0], "c_SegTo"),
		GetShaderConstID(Sh[1], "c_SegTo")
	};
	static int Sh_CenterColor[2] = {
		GetShaderConstID(Sh[0], "c_CenterColor"),
		GetShaderConstID(Sh[1], "c_CenterColor")
	};
	static int Sh_BoundColor[2] = {
		GetShaderConstID(Sh[0], "c_BoundColor"),
		GetShaderConstID(Sh[1], "c_BoundColor")
	};
	static int Sh_SegRadius[2] = {
		GetShaderConstID(Sh[0], "c_SegRadius"),
		GetShaderConstID(Sh[1], "c_SegRadius")
	};
	static int Sh_GradRadius[2] = {
		GetShaderConstID(Sh[0], "c_GradRadius"),
		GetShaderConstID(Sh[1], "c_GradRadius")
	};
	static int Sh_Power[2] = {
		GetShaderConstID(Sh[0], "c_Power"),
		GetShaderConstID(Sh[1], "c_Power")
	};
	static int Sh_CaveFrom[2] = {
		GetShaderConstID(Sh[0], "c_CaveFrom"),
		GetShaderConstID(Sh[1], "c_CaveFrom")
	};
	static int Sh_CaveTo[2] = {
		GetShaderConstID(Sh[0], "c_CaveTo"),
		GetShaderConstID(Sh[1], "c_CaveTo")
	};
	static int Sh_FlatFrom[2] = {
		GetShaderConstID(Sh[0], "c_FlatFrom"),
		GetShaderConstID(Sh[1], "c_FlatFrom")
	};
	static int Sh_FlatTo[2] = {
		GetShaderConstID(Sh[0], "c_FlatTo"),
		GetShaderConstID(Sh[1], "c_FlatTo")
	};
	static int Sh_HalfFrom[2] = {
		GetShaderConstID(Sh[0], "c_HalfFrom"),
		GetShaderConstID(Sh[1], "c_HalfFrom")
	};
	static int Sh_HalfTo[2] = {
		GetShaderConstID(Sh[0], "c_HalfTo"),
		GetShaderConstID(Sh[1], "c_HalfTo")
	};
	static int Sh_HalfFlat[2] = {
		GetShaderConstID(Sh[0], "c_HalfFlat"),
		GetShaderConstID(Sh[1], "c_HalfFlat")
	};

	int Index = cVec2::Equals(V, cVec2::AxisX) ? 0 : 1;

	cRect ClipRect;
	ClipRect.SetToPoint(F);
	ClipRect.AddPoint(T);
	ClipRect.Inflate(SegRadius);
	if(0 == Index) {
		if((Style & RadialGradient::SharpFrom) || (Style & RadialGradient::FlatFrom)) {
			ClipRect.SetLeft(F.x);
		}
		if((Style & RadialGradient::SharpTo) || (Style & RadialGradient::FlatTo)) {
			ClipRect.SetRight(T.x);
		}
	} else {
		if((Style & RadialGradient::SharpFrom) || (Style & RadialGradient::FlatFrom)) {
			ClipRect.SetBottom(F.y);
		}
		if((Style & RadialGradient::SharpTo) || (Style & RadialGradient::FlatTo)) {
			ClipRect.SetTop(T.y);
		}
	}
	ClipRect.Transform(cRender_WorldMatrix);
	ClipRect.Round();
	if(!cRender_CurClipRect.IsEmpty()) {
		ClipRect = cRect::Intersect(ClipRect, cRender_CurClipRect);
		if(ClipRect.IsEmpty()) {
			return; // Nowhere to draw
		}
	}
	
	SetShader(Sh[Index]);
	SetShaderConst(Sh_CenterColor[Index], CenterColor);
	SetShaderConst(Sh_BoundColor[Index], BoundColor);
	SetShaderConst(Sh_SegFrom[Index], F);
	SetShaderConst(Sh_SegTo[Index], T);
	SetShaderConst(Sh_SegRadius[Index], SegRadius);
	SetShaderConst(Sh_GradRadius[Index], GradRadius);
	SetShaderConst(Sh_Power[Index], Power);
	SetShaderConst(Sh_CaveFrom[Index], (Style & RadialGradient::CaveFrom) ? 1.0f : 0.0f);
	SetShaderConst(Sh_CaveTo[Index], (Style & RadialGradient::CaveTo) ? 1.0f : 0.0f);
	SetShaderConst(Sh_FlatFrom[Index], (Style & RadialGradient::FlatFrom) ? 1.0f : 0.0f);
	SetShaderConst(Sh_FlatTo[Index], (Style & RadialGradient::FlatTo) ? 1.0f : 0.0f);
	SetShaderConst(Sh_HalfFrom[Index], HalfCcw ? vb[0].Pos.ToVec2() : vb[1].Pos.ToVec2());
	SetShaderConst(Sh_HalfTo[Index], HalfCcw ? vb[3].Pos.ToVec2() : vb[2].Pos.ToVec2());
	SetShaderConst(Sh_HalfFlat[Index], HalfFlat ? 1.0f : 0.0f);

	PushClipRect();
	SetClipRect(ClipRect);

	DrawIndexed(cTopology::TriangleList, vb, 4, ib, 6);
	PopClipRect();
} // cRender::FillSegmentRadialGradient

//-----------------------------------------------------------------------------
// cRender::DrawLine : (const cVec3 &, ...)
//-----------------------------------------------------------------------------
void cRender::DrawLine(const cVec3 &P0, const cVec3 &P1, const cColor &Color) {
	cVertex::PositionColored u[2];
	u[0].Color = u[1].Color = Color.ToDword();
	u[0].Pos = P0;
	u[1].Pos = P1;
	
	static int Sh = GetShaderID("Colored", cVertex::PositionColored::FormatID);
	SetShader(Sh);
	DrawArrays(cTopology::LineList, u, 2);
} // cRender::DrawLine : (const cVec3 &, ...)

//-------------------------------------------------------------------------------------------
// cRender::FillSegmentRadialGradient : (const cRect &, ...)
//-------------------------------------------------------------------------------------------
void cRender::FillSegmentRadialGradient(const cRect &BoundingRect,
										const float GradRadius, const float Power, const int Style,
										const cColor &CenterColor, const cColor &BoundColor) {
	float SegRadius, HalfRadius;
	cVec2 SegFrom, SegTo;
	int Index;
	
	const bool HalfCcw = (Style & RadialGradient::HalfCcwFlat) || (Style & RadialGradient::HalfCcwSharp);
	const bool HalfCw = (Style & RadialGradient::HalfCwFlat) || (Style & RadialGradient::HalfCwSharp);

	Index = cMath::MinIndex(BoundingRect.GetWidth(), BoundingRect.GetHeight());
	// 0 - vertical segment
	// 1 - horizontal segment
	SegRadius = HalfRadius = 0.5f * cMath::Min(BoundingRect.GetWidth(), BoundingRect.GetHeight());

	if(HalfCcw || HalfCw) {
		SegRadius *= 2.0f;
	}
	
	SegFrom = BoundingRect.GetBottomLeft() + cVec2(0 == Index ? HalfRadius : SegRadius, 0 == Index ? SegRadius : HalfRadius);
	if(Style & RadialGradient::CaveFrom) {
		if(1 == Index) {
			SegFrom = BoundingRect.GetMiddleLeft() - cVec2(SegRadius, 0.0f);
		} else {
			SegFrom = BoundingRect.GetBottomCenter() - cVec2(0.0f, SegRadius);
		}
	} else if((Style & RadialGradient::SharpFrom) || (Style & RadialGradient::FlatFrom)) {
		if(1 == Index) {
			SegFrom = BoundingRect.GetMiddleLeft();
		} else {
			SegFrom = BoundingRect.GetBottomCenter();
		}
	}

	SegTo = BoundingRect.GetTopRight() - cVec2(0 == Index ? HalfRadius : SegRadius, 0 == Index ? SegRadius : HalfRadius);
	if(Style & RadialGradient::CaveTo) {
		if(1 == Index) {
			SegTo = BoundingRect.GetMiddleRight() + cVec2(SegRadius, 0.0f);
		} else {
			SegTo = BoundingRect.GetTopCenter() + cVec2(0.0f, SegRadius);
		}
	} else if((Style & RadialGradient::SharpTo) || (Style & RadialGradient::FlatTo)) {
		if(1 == Index) {
			SegTo = BoundingRect.GetMiddleRight();
		} else {
			SegTo = BoundingRect.GetTopCenter();
		}
	}

	if(HalfCcw) {
		if(1 == Index) { // Horizontal segment
			SegFrom -= cVec2(0.0f, HalfRadius);
			SegTo -= cVec2(0.0f, HalfRadius);
		} else { // Vertical Segment
			SegFrom += cVec2(HalfRadius, 0.0f);
			SegTo += cVec2(HalfRadius, 0.0f);
		}
	} else if(HalfCw) {
		if(1 == Index) { // Horizontal segment
			SegFrom += cVec2(0.0f, HalfRadius);
			SegTo += cVec2(0.0f, HalfRadius);
		} else { // Vertical Segment
			SegFrom -= cVec2(HalfRadius, 0.0f);
			SegTo -= cVec2(HalfRadius, 0.0f);
		}
	}

	float Gr = GradRadius;
	if(0.0f == Gr) {
		Gr = HalfRadius;
		if(HalfCcw || HalfCw) { // If we are drawing only half, gradient should be the same as in whole part
			Gr *= 2.0f;
		}
	}
	
	FillSegmentRadialGradient(SegFrom, SegTo, SegRadius, Gr, Power, Style, CenterColor, BoundColor);
} // cRender::FillSegmentRadialGradient

//-----------------------------------------------------------------------------------------------------------------------
// cRender::FillRectangleRadialGradient
//-----------------------------------------------------------------------------------------------------------------------
void cRender::FillRectangleRadialGradient(const cRect &BoundingRect, const float SegRadius, const float GradRadius, const float Power,
										  const cColor &CenterColor, const cColor &BoundColor) {
	cRect Center;
	float Gr = GradRadius == 0.0f ? SegRadius : GradRadius;
	
	Center = BoundingRect;
	Center.Inflate(-SegRadius);

	FillSegmentRadialGradient(Center.GetTopLeft(), Center.GetTopRight(), SegRadius, Gr, Power, RadialGradient::RoundRound | RadialGradient::HalfCcwSharp, CenterColor, BoundColor);
	FillSegmentRadialGradient(Center.GetBottomLeft(), Center.GetBottomRight(), SegRadius, Gr, Power, RadialGradient::RoundRound | RadialGradient::HalfCwSharp, CenterColor, BoundColor);
	FillSegmentRadialGradient(Center.GetBottomLeft(), Center.GetTopLeft(), SegRadius, Gr, Power, RadialGradient::SharpSharp | RadialGradient::HalfCcwSharp, CenterColor, BoundColor);
	FillSegmentRadialGradient(Center.GetBottomRight(), Center.GetTopRight(), SegRadius, Gr, Power, RadialGradient::SharpSharp | RadialGradient::HalfCwSharp, CenterColor, BoundColor);
	FillRectangleSolid(Center, CenterColor);
} // cRender::FillRectangleRadialGradient

// cRender::FillCircleRadialGradient : (const float, ...)
void cRender::FillCircleRadialGradient(const float X, const float Y, const float Radius, const float GradRadius, const float Power, const cColor &CenterColor, const cColor &BoundColor) {
	const cVec2 Pos(X, Y);
	FillSegmentRadialGradient(Pos, Pos, Radius, GradRadius, Power, RadialGradient::RoundRound, CenterColor, BoundColor);
}

// cRender::FillCircleRadialGradient : (const cVec2 &, ...)
void cRender::FillCircleRadialGradient(const cVec2 &Pos, const float Radius, const float GradRadius, const float Power, const cColor &CenterColor, const cColor &BoundColor) {
	FillSegmentRadialGradient(Pos, Pos, Radius, GradRadius, Power, RadialGradient::RoundRound, CenterColor, BoundColor);
}

// cRender::FillCircleRadialGradient : (const int, ...)
void cRender::FillCircleRadialGradient(const int X, const int Y, const int Radius, const int GradRadius, const int Power, const cColor &CenterColor, const cColor &BoundColor) {
	const cVec2 Pos((float)X, (float)Y);
	FillSegmentRadialGradient(Pos, Pos, (float)Radius, (float)GradRadius, (float)Power, RadialGradient::RoundRound, CenterColor, BoundColor);
}

//-----------------------------------------------------------------------------------------------------------------------------------------------------------
// cRender::DrawCaveCaveCorner
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
void cRender::DrawCaveCaveCorner(const cRect &BoundingQuad, const float GradRadius, const float Power, const cColor &CenterColor, const cColor &BoundColor) {
	const cVec2 Pos = BoundingQuad.GetTopLeft();
	const float SegRadius = cMath::Min(BoundingQuad.GetWidth(), BoundingQuad.GetHeight()); // Actually they should be the same
	
	// vb
	cVertex::PositionOnly vb[8];
	vb[0].Pos.Set(Pos - cVec2(SegRadius, 0.0f), 0.0f);
	vb[1].Pos = vb[0].Pos - cVec3(0.0f, SegRadius, 0.0f);
	vb[2].Pos.Set(Pos, 0.0f);
	vb[3].Pos = vb[2].Pos - cVec3(0.0f, SegRadius, 0.0f);
	vb[4].Pos.Set(Pos + cVec2(SegRadius, -SegRadius), 0.0f);
	vb[5].Pos.Set(Pos + cVec2(SegRadius, 0.0f), 0.0f);
	vb[6].Pos.Set(Pos + cVec2(0.0f, SegRadius), 0.0f);
	vb[7].Pos = vb[6].Pos + cVec3(SegRadius, 0.0f, 0.0f);
	
	// ib
	const int ibl[9] = { 0, 1, 2, 2, 1, 3, 2, 3, 4 };
	const int ibt[9] = { 2, 4, 5, 6, 2, 7, 7, 2, 5 };
	
	// Draw
	static int Blend = GetBlendStateID(cBlendFactor::SrcAlpha, cBlendFactor::OneMinusSrcAlpha);
	SetBlendState(Blend);
	SetDepthState(-1);
	SetCullMode(cCullMode::None);
	
	static int Sh[2] = {
		GetShaderID("RadialGradientUI", cVertex::PositionOnly::FormatID, "#define HORIZONTAL"),
		GetShaderID("RadialGradientUI", cVertex::PositionOnly::FormatID) // VERTICAL
	};
	static int Sh_SegFrom[2] = {
		GetShaderConstID(Sh[0], "c_SegFrom"),
		GetShaderConstID(Sh[1], "c_SegFrom")
	};
	static int Sh_SegTo[2] = {
		GetShaderConstID(Sh[0], "c_SegTo"),
		GetShaderConstID(Sh[1], "c_SegTo")
	};
	static int Sh_CenterColor[2] = {
		GetShaderConstID(Sh[0], "c_CenterColor"),
		GetShaderConstID(Sh[1], "c_CenterColor")
	};
	static int Sh_BoundColor[2] = {
		GetShaderConstID(Sh[0], "c_BoundColor"),
		GetShaderConstID(Sh[1], "c_BoundColor")
	};
	static int Sh_SegRadius[2] = {
		GetShaderConstID(Sh[0], "c_SegRadius"),
		GetShaderConstID(Sh[1], "c_SegRadius")
	};
	static int Sh_GradRadius[2] = {
		GetShaderConstID(Sh[0], "c_GradRadius"),
		GetShaderConstID(Sh[1], "c_GradRadius")
	};
	static int Sh_Power[2] = {
		GetShaderConstID(Sh[0], "c_Power"),
		GetShaderConstID(Sh[1], "c_Power")
	};
	static int Sh_CaveFrom[2] = {
		GetShaderConstID(Sh[0], "c_CaveFrom"),
		GetShaderConstID(Sh[1], "c_CaveFrom")
	};
	static int Sh_CaveTo[2] = {
		GetShaderConstID(Sh[0], "c_CaveTo"),
		GetShaderConstID(Sh[1], "c_CaveTo")
	};
	static int Sh_FlatFrom[2] = {
		GetShaderConstID(Sh[0], "c_FlatFrom"),
		GetShaderConstID(Sh[1], "c_FlatFrom")
	};
	static int Sh_FlatTo[2] = {
		GetShaderConstID(Sh[0], "c_FlatTo"),
		GetShaderConstID(Sh[1], "c_FlatTo")
	};
	static int Sh_HalfFrom[2] = {
		GetShaderConstID(Sh[0], "c_HalfFrom"),
		GetShaderConstID(Sh[1], "c_HalfFrom")
	};
	static int Sh_HalfTo[2] = {
		GetShaderConstID(Sh[0], "c_HalfTo"),
		GetShaderConstID(Sh[1], "c_HalfTo")
	};
	static int Sh_HalfFlat[2] = {
		GetShaderConstID(Sh[0], "c_HalfFlat"),
		GetShaderConstID(Sh[1], "c_HalfFlat")
	};

	int i;
	for(i = 0; i < 2; i++) {
		SetShader(Sh[i]);
		SetShaderConst(Sh_CenterColor[i], CenterColor);
		SetShaderConst(Sh_BoundColor[i], BoundColor);
		if(0 == i) {
			SetShaderConst(Sh_SegFrom[i], vb[0].Pos.ToVec2());
			SetShaderConst(Sh_SegTo[i], vb[2].Pos.ToVec2());
			SetShaderConst(Sh_CaveFrom[i], 1.0f);
			SetShaderConst(Sh_CaveTo[i], 0.0f);
		} else {
			SetShaderConst(Sh_SegFrom[i], vb[2].Pos.ToVec2());
			SetShaderConst(Sh_SegTo[i], vb[6].Pos.ToVec2());
			SetShaderConst(Sh_CaveFrom[i], 0.0f);
			SetShaderConst(Sh_CaveTo[i], 1.0f);
		}
		SetShaderConst(Sh_SegRadius[i], SegRadius);
		SetShaderConst(Sh_GradRadius[i], GradRadius);
		SetShaderConst(Sh_Power[i], Power);
		SetShaderConst(Sh_FlatFrom[i], 0.0f);
		SetShaderConst(Sh_FlatTo[i], 0.0f);
		SetShaderConst(Sh_HalfFrom[i], cVec2::Zero);
		SetShaderConst(Sh_HalfTo[i], cVec2::Zero);
		SetShaderConst(Sh_HalfFlat[i], 0.0f);
		
		DrawIndexed(cTopology::TriangleList, vb, 8, 0 == i ? ibl : ibt, 9);
	}
} // cRender::DrawCaveCaveCorner

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// cRender::DrawIcon
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void cRender::DrawIcon(const cRect &Rc, const int ToolBarID, const int Index, const cRender::IconState::Enum State, const float Alpha, const int IconWidth, const int IconHeight) {
	int Width = GetTextureWidth(ToolBarID);
	int Height = GetTextureHeight(ToolBarID);
	if(Width < 1 || Height < 1) {
		return;
	}
	int IconsPerRow = Width / IconWidth;
	cAssert(IconsPerRow > 0);
	if(IconsPerRow < 1) {
		return;
	}
	int Col = Index % IconsPerRow;
	int Row = Index / IconsPerRow;

	cVertex::PositionTextured vb[4];

	vb[0].Pos.Set(Rc.GetTopLeft(), 0.0f);
	vb[1].Pos.Set(Rc.GetBottomLeft(), 0.0f);
	vb[2].Pos.Set(Rc.GetBottomRight(), 0.0f);
	vb[3].Pos.Set(Rc.GetTopRight(), 0.0f);

	vb[0].TexCoord.Set((float)(Col * IconWidth) / (float)Width, 1.0f - (float)(Row * IconHeight) / (float)Height);
	vb[1].TexCoord.Set(vb[0].TexCoord.x, 1.0f - (float)((Row + 1) * IconHeight) / (float)Height);

	vb[2].TexCoord.Set((float)((Col + 1) * IconWidth) / (float)Width, vb[1].TexCoord.y);
	vb[3].TexCoord.Set(vb[2].TexCoord.x, vb[0].TexCoord.y);

	const int ib[6] = { 0, 1, 2, 0, 2, 3 };

	static int Blend = GetBlendStateID(cBlendFactor::SrcAlpha, cBlendFactor::OneMinusSrcAlpha);
	SetBlendState(Blend);
	SetDepthState(-1);
	SetCullMode(cCullMode::None);

	static int ShaderID = GetShaderID("IconUI", cVertex::PositionTextured::FormatID);
	static int SamplerID = GetSamplerID(ShaderID, "s_Decal");
	static int DisabledID = GetShaderConstID(ShaderID, "c_Disabled");
	static int HotID = GetShaderConstID(ShaderID, "c_Hot");
	static int AlphaID = GetShaderConstID(ShaderID, "c_Alpha");
	SetShader(ShaderID);
	static int SamplerStateID = GetSamplerStateID(cFilter::Linear, cAddressMode::Clamp);
	SetTexture(SamplerID, ToolBarID, SamplerStateID);
	SetShaderConst(DisabledID, IconState::Disabled == State ? 1.0f : 0.0f);
	SetShaderConst(HotID, IconState::Hot == State ? 1.0f : 0.0f);
	SetShaderConst(AlphaID, Alpha);
	DrawIndexed(cTopology::TriangleList, vb, 4, ib, 6);
} // cRender::DrawIcon

//---------------------------------------------------------------------------------------------------------
// cRender::FillRectangleTexturedCurShader
//---------------------------------------------------------------------------------------------------------
void cRender::FillRectangleTexturedCurShader(const cRect &Rc, const FillRectangleTexturedCurShaderArgs &Args) {
	cVertex::PositionTextured vb[4];

	vb[0].Pos.Set(Rc.GetBottomLeft(), Args.Z);
	vb[1].Pos.Set(Rc.GetBottomRight(), Args.Z);
	vb[2].Pos.Set(Rc.GetTopRight(), Args.Z);
	vb[3].Pos.Set(Rc.GetTopLeft(), Args.Z);

	if(Args.Flip) {
		vb[0].TexCoord.Set(Args.t0, 1.0f);
		vb[1].TexCoord.Set(Args.t1, 1.0f);
		vb[2].TexCoord.Set(Args.t1, 0.0f);
		vb[3].TexCoord.Set(Args.t0, 0.0f);
    } else {
        vb[0].TexCoord.Set(Args.t0, 0.0f);
        vb[1].TexCoord.Set(Args.t1, 0.0f);
        vb[2].TexCoord.Set(Args.t1, 1.0f);
        vb[3].TexCoord.Set(Args.t0, 1.0f);
    }
    
    const int ib[6] = { 0, 1, 2, 0, 2, 3 };

	bool b = cRender::GetWireframe();
	if(b) {
		cRender::SetWireframe(false);
	}
	
	DrawIndexed(cTopology::TriangleList, vb, 4, ib, 6);

	cRender::SetWireframe(b);
} // cRender::FillRectangleTexturedCurShader

//-------------------------------------------------------------------------------------------------
// cRender::FillRectangleCubeCurShader
//-------------------------------------------------------------------------------------------------
void cRender::FillRectangleCubeCurShader(const cRect &Rc, const float Z, const cFrustum &Frustum) {
	cVertex::PositionNormal vb[4];

	vb[0].Pos.Set(Rc.GetBottomLeft(), Z);
	vb[1].Pos.Set(Rc.GetBottomRight(), Z);
	vb[2].Pos.Set(Rc.GetTopRight(), Z);
	vb[3].Pos.Set(Rc.GetTopLeft(), Z);

	const cVec3 *P = Frustum.GetPoints();
	cVec3 lb = P[4] - P[0]; // (l, b, f) - (l, b, n)
	cVec3 rb = P[5] - P[1];	// (r, b, f) - (r, b, n)
	cVec3 tr = P[6] - P[2]; // (r, t, f) - (r, t, n)
	cVec3 tl = P[7] - P[3]; // (l, t, f) - (l, t, n)

	// Coordinates inside DDS cube map
	// https://msdn.microsoft.com/en-us/library/windows/desktop/bb204881(v=vs.85).aspx
	const cMat3 S = cMat3::Scaling(1.0f, 1.0f, -1.0f);
	vb[0].Normal = lb * S;
	vb[1].Normal = rb * S;
	vb[2].Normal = tr * S;
	vb[3].Normal = tl * S;

	const int ib[6] = { 0, 1, 2, 0, 2, 3 };

	bool b = cRender::GetWireframe();
	if(b) {
		cRender::SetWireframe(false);
	}
	cRender::DrawIndexed(cTopology::TriangleList, vb, 4, ib, 6);
	cRender::SetWireframe(b);
} // cRender::FillRectangleCubeCurShader

//----------------------------------------------------------------------------------------------------------------
// cRender::FillRectangleTextured
//----------------------------------------------------------------------------------------------------------------
void cRender::FillRectangleTextured(const cRect &Rc, const int TextureID, const FillRectangleTexturedArgs &Args) {
	SetDepthState(Args.DepthState);
	SetCullMode(cCullMode::None);

	cDimension::Enum Dim = GetTextureDimension(TextureID);
	if(cDimension::None == Dim) {
		return;
	}

	static int LinearClamp = GetSamplerStateID(cFilter::Linear, cAddressMode::Clamp);
	int SamplerStateID = (-1 == Args.OverrideSamplerStateID ? LinearClamp : Args.OverrideSamplerStateID);
	if(cDimension::OneD == Dim) {
		//*********************************************************************
		// 1D
		//*********************************************************************
		if(-1.0f == Args.Alpha) {
			static int Shader1D = GetShaderID("TexturedUI", cVertex::PositionTextured::FormatID, "#define SRC_1D");
			static int Shader1D_Sampler = GetSamplerID(Shader1D, "s_Src");
			
			SetShader(Shader1D);
			SetTexture(Shader1D_Sampler, TextureID, SamplerStateID);
		} else {
			static int Shader1D_Over = GetShaderID("TexturedUI", cVertex::PositionTextured::FormatID, "#define SRC_1D\n#define OVERRIDE_ALPHA");
			static int Shader1D_Over_Sampler = GetSamplerID(Shader1D_Over, "s_Src");
			static int Shader1D_Over_Alpha = GetShaderConstID(Shader1D_Over, "c_Alpha");
			
			SetShader(Shader1D_Over);
			SetTexture(Shader1D_Over_Sampler, TextureID, SamplerStateID);
			SetShaderConst(Shader1D_Over_Alpha, Args.Alpha);
		}
	} else if(cDimension::ThreeD == Dim) {
		//*********************************************************************
		// 3D
		//*********************************************************************
		if(-1.0f == Args.Alpha) {
			static int Shader3D = GetShaderID("TexturedUI", cVertex::PositionTextured::FormatID, "#define SRC_3D");
			static int Shader3D_Sampler = GetSamplerID(Shader3D, "s_Src");

			SetShader(Shader3D);
			SetTexture(Shader3D_Sampler, TextureID, SamplerStateID);
		} else {
			if(!Args.AlphaChannelAsColor) {
				static int Shader3D_Over = GetShaderID("TexturedUI", cVertex::PositionTextured::FormatID, "#define SRC_3D\n#define OVERRIDE_ALPHA");
				static int Shader3D_Over_Sampler = GetSamplerID(Shader3D_Over, "s_Src");
				static int Shader3D_Over_Alpha = GetShaderConstID(Shader3D_Over, "c_Alpha");
				
				SetShader(Shader3D_Over);
				SetTexture(Shader3D_Over_Sampler, TextureID, SamplerStateID);
				SetShaderConst(Shader3D_Over_Alpha, Args.Alpha);
			} else {
				static int Shader3D_Over_Alpha = GetShaderID("TexturedUI", cVertex::PositionTextured::FormatID, "#define SRC_3D\n#define OVERRIDE_ALPHA\n#define ALPHA_CHANNEL_AS_COLOR");
				static int Shader3D_Over_Alpha_Sampler = GetSamplerID(Shader3D_Over_Alpha, "s_Src");
				static int Shader3D_Over_Alpha_Alpha = GetShaderConstID(Shader3D_Over_Alpha, "c_Alpha");
				
				SetShader(Shader3D_Over_Alpha);
				SetTexture(Shader3D_Over_Alpha_Sampler, TextureID, SamplerStateID);
				SetShaderConst(Shader3D_Over_Alpha_Alpha, Args.Alpha);
			}
		}
	} else if(cDimension::Cube == Dim) {
		//*********************************************************************
		// Cube
		//*********************************************************************
		if(Args.CubeFace >= 0 && Args.CubeFace < 6) {
			if(-1.0f == Args.Alpha && !Args.AlphaChannelAsColor) {
				static int ShaderCube = GetShaderID("TexturedUI", cVertex::PositionTextured::FormatID, "#define SRC_CUBE");
				static int ShaderCube_Sampler = GetSamplerID(ShaderCube, "s_Src");
				static int ShaderCube_FaceIndex = GetShaderConstID(ShaderCube, "c_FaceIndex");

				SetShader(ShaderCube);
				SetTexture(ShaderCube_Sampler, TextureID, SamplerStateID);
				SetShaderConst(ShaderCube_FaceIndex, Args.CubeFace);
			} else if(-1.0f == Args.Alpha && Args.AlphaChannelAsColor) {
				static int ShaderCube_Alpha = GetShaderID("TexturedUI", cVertex::PositionTextured::FormatID, "#define SRC_CUBE\n#define ALPHA_CHANNEL_AS_COLOR");
				static int ShaderCube_Alpha_Sampler = GetSamplerID(ShaderCube_Alpha, "s_Src");
				static int ShaderCube_Alpha_FaceIndex = GetShaderConstID(ShaderCube_Alpha, "c_FaceIndex");

				SetShader(ShaderCube_Alpha);
				SetTexture(ShaderCube_Alpha_Sampler, TextureID, SamplerStateID);
				SetShaderConst(ShaderCube_Alpha_FaceIndex, Args.CubeFace);
			} else if(Args.Alpha != -1.0f && !Args.AlphaChannelAsColor) {
				static int ShaderCube_Over = GetShaderID("TexturedUI", cVertex::PositionTextured::FormatID, "#define SRC_CUBE\n#define OVERRIDE_ALPHA\n");
				static int ShaderCube_Over_Sampler = GetSamplerID(ShaderCube_Over, "s_Src");
				static int ShaderCube_Over_Alpha = GetShaderConstID(ShaderCube_Over, "c_Alpha");
				static int ShaderCube_Over_FaceIndex = GetShaderConstID(ShaderCube_Over, "c_FaceIndex");
			
				SetShader(ShaderCube_Over);
				SetTexture(ShaderCube_Over_Sampler, TextureID, SamplerStateID);
				SetShaderConst(ShaderCube_Over_Alpha, Args.Alpha);
				SetShaderConst(ShaderCube_Over_FaceIndex, Args.CubeFace);
			} else {
				static int ShaderCube_Over_Alpha = GetShaderID("TexturedUI", cVertex::PositionTextured::FormatID, "#define SRC_CUBE\n#define OVERRIDE_ALPHA\n#define ALPHA_CHANNEL_AS_COLOR");
				static int ShaderCube_Over_Alpha_Sampler = GetSamplerID(ShaderCube_Over_Alpha, "s_Src");
				static int ShaderCube_Over_Alpha_Alpha = GetShaderConstID(ShaderCube_Over_Alpha, "c_Alpha");
				static int ShaderCube_Over_Alpha_FaceIndex = GetShaderConstID(ShaderCube_Over_Alpha, "c_FaceIndex");
			
				SetShader(ShaderCube_Over_Alpha);
				SetTexture(ShaderCube_Over_Alpha_Sampler, TextureID, SamplerStateID);
				SetShaderConst(ShaderCube_Over_Alpha_Alpha, Args.Alpha);
				SetShaderConst(ShaderCube_Over_Alpha_FaceIndex, Args.CubeFace);
			}
		} else {
			// Coordinates inside DDS cube map
			// https://msdn.microsoft.com/en-us/library/windows/desktop/bb204881(v=vs.85).aspx
			FillRectangleTexturedArgs A = Args;
			cRect rc = Rc;
			rc.SetRight(rc.GetLeft() + rc.GetWidth() / 4.0f);
			rc.SetHeight(cAlign::MiddleLeft, rc.GetHeight() / 4.0f);
			rc.Round();
			A.CubeFace = 1;
			FillRectangleTextured(rc, TextureID, A);
			rc.Translate(rc.GetWidth(), 0.0f);
			A.CubeFace = 4;
			FillRectangleTextured(rc, TextureID, A);
			rc.Translate(rc.GetWidth(), 0.0f);
			A.CubeFace = 0;
			FillRectangleTextured(rc, TextureID, A);
			rc.Translate(rc.GetWidth(), 0.0f);
			A.CubeFace = 5;
			FillRectangleTextured(rc, TextureID, A);
			rc.Translate(-2.0f * rc.GetWidth(), rc.GetHeight());
			A.CubeFace = 2;
			FillRectangleTextured(rc, TextureID, A);
			rc.Translate(0.0f, -2.0f * rc.GetHeight());
			A.CubeFace = 3;
			FillRectangleTextured(rc, TextureID, A);
			return;
		}
	} else {
		//*********************************************************************
		// 2D
		//*********************************************************************
		if(-1.0f == Args.Alpha && !Args.AlphaChannelAsColor) {
			static int Shader2D = GetShaderID("TexturedUI", cVertex::PositionTextured::FormatID, "#define SRC_2D");
			static int Shader2D_Sampler = GetSamplerID(Shader2D, "s_Src");

			SetShader(Shader2D);
			SetTexture(Shader2D_Sampler, TextureID, SamplerStateID);
		} else if(-1.0f == Args.Alpha && Args.AlphaChannelAsColor) {
			static int Shader2D_Alpha = GetShaderID("TexturedUI", cVertex::PositionTextured::FormatID, "#define SRC_2D\n#define ALPHA_CHANNEL_AS_COLOR");
			static int Shader2D_Alpha_Sampler = GetSamplerID(Shader2D_Alpha, "s_Src");

			SetShader(Shader2D_Alpha);
			SetTexture(Shader2D_Alpha_Sampler, TextureID, SamplerStateID);
		} else if(Args.Alpha != -1.0f && !Args.AlphaChannelAsColor) {
			static int Shader2D_Over = GetShaderID("TexturedUI", cVertex::PositionTextured::FormatID, "#define SRC_2D\n#define OVERRIDE_ALPHA");
			static int Shader2D_Over_Sampler = GetSamplerID(Shader2D_Over, "s_Src");
			static int Shader2D_Over_Alpha = GetShaderConstID(Shader2D_Over, "c_Alpha");
		
			SetShader(Shader2D_Over);
			SetTexture(Shader2D_Over_Sampler, TextureID, SamplerStateID);
			SetShaderConst(Shader2D_Over_Alpha, Args.Alpha);
		} else {
			static int Shader2D_Over_Alpha = GetShaderID("TexturedUI", cVertex::PositionTextured::FormatID, "#define SRC_2D\n#define OVERRIDE_ALPHA\n#define ALPHA_CHANNEL_AS_COLOR");
			static int Shader2D_Over_Alpha_Sampler = GetSamplerID(Shader2D_Over_Alpha, "s_Src");
			static int Shader2D_Over_Alpha_Alpha = GetShaderConstID(Shader2D_Over_Alpha, "c_Alpha");
			
			SetShader(Shader2D_Over_Alpha);
			SetTexture(Shader2D_Over_Alpha_Sampler, TextureID, SamplerStateID);
			SetShaderConst(Shader2D_Over_Alpha_Alpha, Args.Alpha);
		}
	}

	bool Flip = GetTextureRenderTargetUsage(TextureID) ? GetFlipRenderTarget() : false;
    FillRectangleTexturedCurShaderArgs Args2;
    Args2.Flip = Flip;
	Args2.Z = Args.Z;
	FillRectangleTexturedCurShader(Rc, Args2);
} // cRender::FillRectangleTextured

//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// cRender::DrawSector
//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void cRender::DrawSector(const cVec3 &Center, const float Radius, const cVec3 &Fm, const cVec3 &To, const cColor &BorderColor, const cColor &FillColor, const float dAngle) {
	const cVec3 n0 = Fm.ToNormal();
	const cVec3 n1 = To.ToNormal();
	const float Angle = cVec3::Angle(n0, n1);
	const int NTris = cMath::Max(1, (int)cMath::Round(Angle / dAngle));
	
	// Vertices:
	static cList<cVertex::PositionColored> Lines;
	Lines.Clear();
	cVertex::PositionColored l;
	l.Color = BorderColor.ToDword();

	static cList<cVertex::PositionColored> vb;
	vb.Clear();
	
	static cList<int> ib;
	ib.Clear();

	cVertex::PositionColored u;
	u.Color = FillColor.ToDword();
	
    u.Pos = Center; vb.Add(u);
	l.Pos = u.Pos;
	Lines.Add(l);
    u.Pos = Center + n0 * Radius; vb.Add(u);
	l.Pos = u.Pos;
	Lines.Add(l);
	int nTri;
	for(nTri = 1; nTri <= NTris; nTri++) {
		u.Pos = Center + cVec3::Slerp(n0, n1, (float)nTri / (float)NTris).ToNormal() * Radius;
		vb.Add(u);
		l.Pos = u.Pos;
		Lines.Add(l);
	}
	l.Pos = vb[0].Pos;
	Lines.Add(l);
	// Indexes:
	const int s = cVec3::Dot(GetViewer()->GetForward(), cVec3::Cross(n0, n1)) > 0.0f ? 1 : 0;
	for(nTri = 1; nTri <= NTris; nTri++) {
		ib.Add(0);
		ib.Add(nTri + s);
		ib.Add(nTri + (s + 1) % 2);
	}

	if(ib.Count() < 3) {
		return;
	}

	static int Sh = GetShaderID("Colored", cVertex::PositionColored::FormatID);
	SetShader(Sh);
	
	DrawIndexed(cTopology::TriangleList, vb.ToPtr(), vb.Count(), ib.ToPtr(), ib.Count());
	DrawArrays(cTopology::LineStrip, Lines.ToPtr(), Lines.Count());
} // cRender::DrawSector

//-------------------------------------------------------------------------------------------------------
// cRender::DrawCubeSolid
//-------------------------------------------------------------------------------------------------------
void cRender::DrawCubeSolid(const cVec3 &Center, const float Side, const cColor &Color, const cQuat &q) {
	// Offsets:
	const float h = 0.5f * Side;
	int i;

	cVec3 Offsets[8] = { cVec3(-h, -h, -h), cVec3(h, -h, -h), cVec3(h, -h, h), cVec3(-h, -h, h),
		cVec3(-h, h, -h), cVec3(h, h, -h), cVec3(h, h, h), cVec3(-h, h, h) };
	for(i = 0; i < 8; i++) {
		Offsets[i] *= q;
	}

	// Vertices:
	cList<cVertex::PositionColored> vb;
	cVertex::PositionColored u;

	u.Color = Color.ToDword();
	for(i = 0; i < 8; i++) {
		u.Pos = Center + Offsets[i];
		vb.Add(u);
	}
	
	// Indexes:
	const int ib[] = {
		0, 1, 3,
		1, 2, 3,
		4, 7, 5,
		5, 7, 6,
		0, 7, 4,
		0, 3, 7,
		3, 6, 7,
		3, 2, 6,
		2, 5, 6,
		2, 1, 5,
		1, 4, 5,
		1, 0, 4
	};

	static int ShaderID = GetShaderID("Colored", cVertex::PositionColored::FormatID);
	SetShader(ShaderID);
	DrawIndexed(cTopology::TriangleList, vb.ToPtr(), vb.Count(), ib, sizeof(ib) / sizeof(ib[0]));
} // cRender::DrawCubeSolid

void cRender::DrawCubeColored(const float Size) {
	// Offsets
	const float h = 0.5f * Size;
	cVec3 O[8] = {
		cVec3(-h, -h, -h),
		cVec3(h, -h, -h),
		cVec3(h, -h, h), 
		cVec3(-h, -h, h),
		cVec3(-h, h, -h),
		cVec3(h, h, -h),
		cVec3(h, h, h),
		cVec3(-h, h, h)
	};

	// Vertices
	cList<cVertex::PositionColored> vb;
	vb.SetCount(24);
	// Bottom
	vb[0].Pos = O[0];
	vb[1].Pos = O[1];
	vb[2].Pos = O[2];
	vb[3].Pos = O[3];
	vb[0].Color = vb[1].Color = vb[2].Color = vb[3].Color = cColor::Green.ToDword();
	// Right
	vb[4].Pos = O[2];
	vb[5].Pos = O[1];
	vb[6].Pos = O[5];
	vb[7].Pos = O[6];
	vb[4].Color = vb[5].Color = vb[6].Color = vb[7].Color = cColor::Red.ToDword();
	// Back
	vb[8].Pos = O[1];
	vb[9].Pos = O[0];
	vb[10].Pos = O[4];
	vb[11].Pos = O[5];
	vb[8].Color = vb[9].Color = vb[10].Color = vb[11].Color = cColor::Blue.ToDword();
	// Left
	vb[12].Pos = O[0];
	vb[13].Pos = O[3];
	vb[14].Pos = O[7];
	vb[15].Pos = O[4];
	vb[12].Color = vb[13].Color = vb[14].Color = vb[15].Color = cColor::Red.ToDword();
	// Front
	vb[16].Pos = O[3];
	vb[17].Pos = O[2];
	vb[18].Pos = O[6];
	vb[19].Pos = O[7];
	vb[16].Color = vb[17].Color = vb[18].Color = vb[19].Color = cColor::Blue.ToDword();
	// Top
	vb[20].Pos = O[7];
	vb[21].Pos = O[6];
	vb[22].Pos = O[5];
	vb[23].Pos = O[4];
	vb[20].Color = vb[21].Color = vb[22].Color = vb[23].Color = cColor::Green.ToDword();
	
	// Indices
	const int ib[] = {
		// Bottom
		0, 1, 2,
		0, 2, 3,
		// Right
		4, 5, 6,
		4, 6, 7,
		// Back
		8, 9, 10,
		8, 10, 11,
		// Left
		12, 13, 14,
		12, 14, 15,
		// Front
		16, 17, 18,
		16, 18, 19,
		// Top
		20, 21, 22,
		20, 22, 23
	};

	static int ShaderID = GetShaderID("Colored", cVertex::PositionColored::FormatID);
	SetShader(ShaderID);
	DrawIndexed(cTopology::TriangleList, vb.ToPtr(), vb.Count(), ib, sizeof(ib) / sizeof(ib[0]));
}

//--------------------------------------------------------------------------------------
// cRender::DrawCubeWire
//--------------------------------------------------------------------------------------
void cRender::DrawCubeWire(const cVec3 &Center, const float Side, const cColor &Color) {
	// vb
	const float h = 0.5f * Side;
	dword C = Color.ToDword();

	const cVertex::PositionColored vb[8] = {
		// Bottom
		cVertex::PositionColored(cVec3(-h, -h, -h), C),
		cVertex::PositionColored(cVec3(h, -h, -h), C),
		cVertex::PositionColored(cVec3(h, -h, h), C),
		cVertex::PositionColored(cVec3(-h, -h, h), C),
		// Top
		cVertex::PositionColored(cVec3(-h, h, -h), C),
		cVertex::PositionColored(cVec3(h, h, -h), C),
		cVertex::PositionColored(cVec3(h, h, h), C),
		cVertex::PositionColored(cVec3(-h, h, h), C)
	};
	
	// ib
	const int ib[24] = {
		// Bottom
		0, 1,
		1, 2,
		2, 3,
		3, 0,
		// Top
		4, 5,
		5, 6,
		6, 7,
		7, 4,
		// Side
		0, 4,
		1, 5,
		2, 6,
		3, 7
	};

	static int Sh = GetShaderID("Colored", cVertex::PositionColored::FormatID);
	SetShader(Sh);
	DrawIndexed(cTopology::LineList, vb, 8, ib, 24);
} // cRender::DrawCubeWire

//-------------------------------------------------------------------------------------------
// cRender::DrawCubesCurShader
//-------------------------------------------------------------------------------------------
void cRender::DrawCubesCurShader(const cVec3 *Centers, const float *Sides, const int Count) {
	static cList<cVec3> vb;
	static cList<int> ib;
	vb.Clear();
	ib.Clear();

	int i, j, i0;
	float h;
	cVec3 Offsets[8];

	const int Templ[] = {
		0, 1, 3,
		1, 2, 3,
		4, 7, 5,
		5, 7, 6,
		0, 7, 4,
		0, 3, 7,
		3, 6, 7,
		3, 2, 6,
		2, 5, 6,
		2, 1, 5,
		1, 4, 5,
		1, 0, 4
	};
	
	for(i = 0; i < Count; i++) {
		const cVec3 &c = Centers[i];
		h = 0.5f * Sides[i];
		
		Offsets[0].Set(-h, -h, -h);
		Offsets[1].Set(h, -h, -h);
		Offsets[2].Set(h, -h, h);
		Offsets[3].Set(-h, -h, h);
		Offsets[4].Set(-h, h, -h);
		Offsets[5].Set(h, h, -h);
		Offsets[6].Set(h, h, h);
		Offsets[7].Set(-h, h, h);

		i0 = vb.Count();
		for(j = 0; j < 8; j++) {
			vb.Add(c + Offsets[j]);
		}

		for(j = 0; j < 36; j++) {
			ib.Add(Templ[j] + i0);
		}
	}

	if(!ib.IsEmpty()) {
		DrawIndexed(cTopology::TriangleList, vb.ToPtr(), vb.Count(), ib.ToPtr(), ib.Count());
	}
} // cRender::DrawCubesCurShader

//-----------------------------------------------------------------------------------------
// cRender::DrawCrossHair
//-----------------------------------------------------------------------------------------
void cRender::DrawCrossHair(const cVec3 &Center, const float Radius, const cColor &Color) {
	dword C = Color.ToDword();
	
	const cVertex::PositionColored vb[8] = {
		cVertex::PositionColored(cVec3(Center.x - Radius, Center.y, Center.z), C),
		cVertex::PositionColored(cVec3(Center.x + Radius, Center.y, Center.z), C),
		
		cVertex::PositionColored(cVec3(Center.x, Center.y - Radius, Center.z), C),
		cVertex::PositionColored(cVec3(Center.x, Center.y + Radius, Center.z), C),
		
		cVertex::PositionColored(cVec3(Center.x, Center.y, Center.z - Radius), C),
		cVertex::PositionColored(cVec3(Center.x, Center.y, Center.z + Radius), C)
	};
	
	static int Sh = GetShaderID("Colored", cVertex::PositionColored::FormatID);
	SetShader(Sh);
	DrawArrays(cTopology::LineList, vb, 6);
} // cRender::DrawCrossHair

void cRender::DrawNormals(const cList<cVec3>& Centers, const cList<cVec3>& Normals, const cColor& Color, const float Length) {
	cAssert(Centers.Count() == Normals.Count());
	if (Centers.Count() != Normals.Count()) {
		return;
	}
	if (Centers.IsEmpty()) {
		return;
	}
	dword C = Color.ToDword();
	static cList<cVertex::PositionColored> vb;
	vb.Clear();
	int i;
	for (i = 0; i < Centers.Count(); i++) {
		const cVec3& c = Centers[i];
		const cVec3& n = Normals[i];
		vb.Add(cVertex::PositionColored(c, C));
		vb.Add(cVertex::PositionColored(c + Length * n, C));
	}
	static int Sh = GetShaderID("Colored", cVertex::PositionColored::FormatID);
	SetShader(Sh);
	DrawArrays(cTopology::LineList, vb.ToPtr(), vb.Count());
}

//-------------------------------------------------------------------------------------------------------------------
// cRender::DrawBillboardQuadsSolid
//-------------------------------------------------------------------------------------------------------------------
void cRender::DrawBillboardQuadsSolid(const cVec3 *Centers, const int Count, const float Side, const cColor &Color) {
	const cVec3 Up = GetViewer()->GetUp();
	const cVec3 Right = GetViewer()->GetRight();
	
	static cList<cVertex::PositionColored> vb;
	vb.Clear();

	static cList<int> ib;
	ib.Clear();

	const int Tris[6] = { 0, 1, 3, 0, 3, 2 };
	int i, j;
	cVertex::PositionColored u[4];
	u[0].Color = u[1].Color = u[2].Color = u[3].Color = Color.ToDword();
	
	for(i = 0; i < Count; i++) {
		for(j = 0; j < 6; j++) {
			ib.Add(vb.Count() + Tris[j]);
		}
		
		u[0].Pos = Centers[i] - Right * Side * 0.5f + Up * Side * 0.5f; // upper - left
		u[1].Pos = u[0].Pos - Up * Side; // bottom - left
		u[2].Pos = u[0].Pos + Side * Right; // upper - right
		u[3].Pos = u[1].Pos + Side * Right; // bottom - right
		vb.AddRange(u, 4);
	}

	if(!ib.IsEmpty()) {
		static int ShaderID = GetShaderID("Colored", cVertex::PositionColored::FormatID);
		SetShader(ShaderID);
		DrawIndexed(cTopology::TriangleList, vb.ToPtr(), vb.Count(), ib.ToPtr(), ib.Count());
	}
} // cRender::DrawBillboardQuadsSolid

//------------------------------------------------------------------------------------------------------------------------------------------------------
// cRender::DrawCircle : (const cVec2 &, ...)
//------------------------------------------------------------------------------------------------------------------------------------------------------
void cRender::DrawCircle(const cVec2 &Pos, const float Radius, const float StartAngle, const float SweepAngle, const cColor &Color, const int SubDivs) {
	cVec2 Start, u[2], S0;
	int i;
	float Cur;
	
	S0 = Pos + cVec2(Radius, 0.0f);
	u[0] = cVec2::TransformCoordinate(S0, cMat4::RotationAt(Pos, StartAngle));
	for(i = 0; i < SubDivs; i++) {
		Cur = StartAngle + SweepAngle / (float)SubDivs * (float)(i + 1);
		u[1] = cVec2::TransformCoordinate(S0, cMat4::RotationAt(Pos, Cur));
		DrawLine(u[0], u[1], Color);
		u[0] = u[1];
	}
} // cRender::DrawCircle : (const cVec2 &, ...)

//-----------------------------------------------------------------------------
// Helpers
//-----------------------------------------------------------------------------

// cRender::FillCirclesColoredCurShader
void cRender::FillCirclesColoredCurShader(const cVec2 *Pos, const float *Radius, const int Count, const cColor &Color, const float StartAngle, const float SweepAngle, const int SubDivs) {
	static cList<cVertex::PositionColored> vb;
	static cList<int> ib;

	vb.Clear();
	ib.Clear();
	
	cVertex::PositionColored u;
	cVec2 S0;
	int i, j, i0;
	float Cur;

	for (i = 0; i < Count; i++) {
		u.Pos.Set(Pos[i], 0.0f);
		u.Color = Color.ToDword();
		i0 = vb.Add(u);
		
		S0 = Pos[i] + cVec2(Radius[i], 0.0f);
		u.Pos.Set(cVec2::TransformCoordinate(S0, cMat4::RotationAt(Pos[i], StartAngle)), 0.0f);
		vb.Add(u);
		for (j = 0; j < SubDivs; j++) {
			Cur = StartAngle + SweepAngle / (float)SubDivs * (float)(j + 1);
			u.Pos.Set(cVec2::TransformCoordinate(S0, cMat4::RotationAt(Pos[i], Cur)), 0.0f);
			vb.Add(u);

			ib.Add(i0);
			ib.Add(vb.Count() - 2);
			ib.Add(vb.Count() - 1);
		}
	}

	if(!ib.IsEmpty()) {
		DrawIndexed(cTopology::TriangleList, vb.ToPtr(), vb.Count(), ib.ToPtr(), ib.Count());
	}
}

//-----------------------------------------------------------------------------
// cRender::FillCircleSolid
//-----------------------------------------------------------------------------
void cRender::FillCircleSolid(const int X, const int Y, const int Radius, const cColor &Color, const int StartAngle, const int SweepAngle, const int SubDivs) {
	FillCircleSolid(cVec2((float)X, (float)Y), (float)Radius, Color, (float)StartAngle, (float)SweepAngle, SubDivs);
}

void cRender::FillCircleSolid(const float X, const float Y, const float Radius, const cColor &Color, const float StartAngle, const float SweepAngle, const int SubDivs) {
	FillCircleSolid(cVec2(X, Y), Radius, Color, StartAngle, SweepAngle, SubDivs);
}

// cRender.FillCirclesSolid
void cRender::FillCirclesSolid(const cVec2 *Pos, const float *Radius, const int Count, const cColor &Color, const float StartAngle, const float SweepAngle, const int SubDivs) {
	static int ShaderID = cRender::GetShaderID("SolidUI", cVertex::PositionColored::FormatID);
	SetShader(ShaderID);
	FillCirclesColoredCurShader(Pos, Radius, Count, Color, StartAngle, SweepAngle, SubDivs);
}

// cRender::DrawCircle
void cRender::DrawCircle(const cVec3 &Center, const float Radius, const cColor &Color, const cVec3 *OverrideAxis) {
	const float StartAngle = 0.0f, EndAngle = 360.0f;
	static cList<cVertex::PositionColored> VB;
	VB.Clear();
	cVertex::PositionColored u[2];
	u[0].Color = u[1].Color = Color.ToDword();
	cVec3 Forward = GetViewer()->GetForward();
	cVec3 Up = GetViewer()->GetUp();
	if(OverrideAxis != nullptr) {
		Forward = OverrideAxis->ToNormal();
		Up = Forward.ToPerp().ToNormal();
	}
	cRotation Circle(Center, Forward, StartAngle);
	const cVec3 Start = Center + Up * Radius;
	u[0].Pos = cVec3::Rotate(Start, Circle);
	float SweepAngle = EndAngle - StartAngle;
	int SubDivs = cMath::Max(1, (int)cMath::Round(SweepAngle)), i;
	float SubDivAngle = SweepAngle / (float)SubDivs;
	for(i = 0; i < SubDivs; i++) {
		Circle.SetAngle(StartAngle + SubDivAngle * (float)(i + 1));
		u[1].Pos = cVec3::Rotate(Start, Circle);
		VB.AddRange(u, 2);
		u[0].Pos = u[1].Pos;
	}
	static int Sh = GetShaderID("Colored", cVertex::PositionColored::FormatID);
	SetShader(Sh);
	DrawArrays(cTopology::LineList, VB.ToPtr(), VB.Count());
}

//-----------------------------------------------------------------------------
// cRender_FillCircle_Add
//-----------------------------------------------------------------------------
static cList<cVertex::PositionColored> cRender_FillCircle_VB;
static cList<int> cRender_FillCircle_IB;
static void cRender_FillCircle_Add(const cVec3 &Center, const float Radius, const cColor &Color) {
	const float StartAngle = 0.0f, EndAngle = 360.0f;
	float SweepAngle = EndAngle - StartAngle;
	int SubDivs = cMath::Max(1, (int)cMath::Round(SweepAngle));
	float SubDivAngle = SweepAngle / (float)SubDivs;
	int i0 = cRender_FillCircle_VB.Count();
	cVertex::PositionColored u;
	u.Color = Color.ToDword();
	u.Pos = Center;
	cRender_FillCircle_VB.Add(u);
	cRotation Circle(Center, -cRender::GetViewer()->GetForward(), StartAngle);
	const cVec3 Start = Center + cRender::GetViewer()->GetUp() * Radius;
	u.Pos = cVec3::Rotate(Start, Circle);
	cRender_FillCircle_VB.Add(u);
	int i;
	for(i = 0; i < SubDivs; i++) {
		Circle.SetAngle(StartAngle + SubDivAngle * (float)(i + 1));
		u.Pos = cVec3::Rotate(Start, Circle);
		cRender_FillCircle_VB.Add(u);
	}
	for(i = 0; i < SubDivs; i++) {
		cRender_FillCircle_IB.Add(i0);
		cRender_FillCircle_IB.Add(i0 + i + 1);
		cRender_FillCircle_IB.Add(i0 + i + 2);
		cAssert(cRender_FillCircle_IB.GetLast() < cRender_FillCircle_VB.Count());
	}
} // cRender_FillCircle_Add

// cRender::FillCircleSolid
void cRender::FillCircleSolid(const cVec3 &Center, const float Radius, const cColor &Color) {
	cRender_FillCircle_VB.Clear();
	cRender_FillCircle_IB.Clear();
	cRender_FillCircle_Add(Center, Radius, Color);
	static int Sh = GetShaderID("Colored", cVertex::PositionColored::FormatID);
	SetShader(Sh);
	DrawIndexed(cTopology::TriangleList, cRender_FillCircle_VB.ToPtr(), cRender_FillCircle_VB.Count(), cRender_FillCircle_IB.ToPtr(), cRender_FillCircle_IB.Count());
}

static cList<cVec3> cRender_CircleVB;
static cList<int> cRender_CircleIB;
static void cRender_AddCircle(const cVec3 &Center, const float Radius, const cVec3 &Axis, const int SubDivs) {
	cRotation Circle(Center, Axis.ToNormal(), 0.0f);
	const cVec3 Start = Center + Circle.GetAxis().ToPerp().ToNormal() * Radius;
	int i0 = cRender_CircleVB.Count();
	cRender_CircleVB.Add(Center);
	cRender_CircleVB.Add(Start);
	int i;
	for(i = 1; i < SubDivs; i++) {
		Circle.SetAngle(360.0f / (float)SubDivs * (float)i);
		cRender_CircleVB.Add(cVec3::Rotate(Start, Circle));
	}
	int j;
	for(i = 0; i < SubDivs; i++) {
		cRender_CircleIB.Add(i0 + 0);
		cRender_CircleIB.Add(i0 + i + 1);
		j = (i + 1) % SubDivs + 1;
		cRender_CircleIB.Add(i0 + j);
	}
}

// cRender::FillCirclesCurShader
void cRender::FillCirclesCurShader(const cVec3 *Center, const float *Radius, const int Count, const cVec3 &Axis, const int SubDivs) {
	if(Count > 0 && SubDivs >= 3) {
		cRender_CircleVB.Clear();
		cRender_CircleIB.Clear();
		int i;
		for(i = 0; i < Count; i++) {
			cRender_AddCircle(Center[i], Radius[i], Axis, SubDivs);
		}
		DrawIndexed(cTopology::TriangleList, cRender_CircleVB.ToPtr(), cRender_CircleVB.Count(), cRender_CircleIB.ToPtr(), cRender_CircleIB.Count());
	}
}

//----------------------------------------------------------------------------------------------------------------------------------
// cRender::DrawFacingCircle
//----------------------------------------------------------------------------------------------------------------------------------
void cRender::DrawFacingCircle(const cVec3 &Center, const float Radius, const cVec3 &Axis, const cColor &Color, const int SubDivs) {
	cAssert(SubDivs >= 3);
	const cVec3 OnScreenCenter = cVec3::TransformCoordinate(Center, GetViewer()->GetViewProjectionScreenMatrix());
	const cVec3 OnScreenUp = cVec3::TransformCoordinate(Center + GetViewer()->GetUp() * Radius, GetViewer()->GetViewProjectionScreenMatrix());
	const float OnScreenRadius = cVec2::Distance(OnScreenCenter.ToVec2(), OnScreenUp.ToVec2());
	cRotation Circle(Center, Axis.ToNormal(), 0.0f);
	const cVec3 Start = Center + Circle.GetAxis().ToPerp().ToNormal() * Radius;
	cVec3 u[2], Cross;
	u[0] = Start;
	cVec3 FmEye;
	for(int i = 0; i < SubDivs; i++) {
		Circle.SetAngle(360.0f / (float)SubDivs * (float)(i + 1));
		u[1] = cVec3::Rotate(Start, Circle);
		const cVec3 c = cVec3::Lerp05(u[0], u[1]);
		if(GetViewer()->GetOrthoProj()) {
			FmEye = GetViewer()->GetForward();
		} else {
			FmEye = (c - GetViewer()->GetPos()).ToNormal();
		}
		const float d = cVec3::Dot(FmEye, c - Circle.GetOrig());
		if(d <= 0.0f) {
			const cVec3 OnScreen = cVec3::TransformCoordinate(c, GetViewer()->GetViewProjectionScreenMatrix());
			const float r = cVec2::Distance(OnScreenCenter.ToVec2(), OnScreen.ToVec2());
			if(r <= OnScreenRadius) {
				DrawLine(u[0], u[1], Color);
			}
		}
		u[0] = u[1];
	}
} // cRender::DrawFacingCircle

//-----------------------------------------------------------------------------------------------------------------------------------------------
// cRender::DrawConeSolid
//-----------------------------------------------------------------------------------------------------------------------------------------------
void cRender::DrawConeSolid(const cVec3 &Apex, const cVec3 &Axis, const float Radius, const float Length, const cColor &Color, const int Sides) {
	static cList<cVertex::PositionColored> vb;
	vb.Clear();

	static cList<int> ib;
	ib.Clear();

	const cVec3 Normal = Axis.ToNormal();
	const cVec3 Cap = Apex - Length * Normal;
	cRotation Cone(Cap, Normal, 0.0f);
	
	// Vertices
	const cVec3 Start = Cap + Radius * Normal.ToPerp().ToNormal();
	cVertex::PositionColored u;
	u.Color = Color.ToDword();
	
	u.Pos = Cap;
	vb.Add(u);
	
	u.Pos = Apex;
	vb.Add(u);

	u.Pos = Start;
	vb.Add(u);
	
	int i;
	for(i = 1; i < Sides; i++) {
		Cone.SetAngle(360.0f / (float)Sides * (float)i);
		u.Pos = cVec3::Rotate(Start, Cone);
		vb.Add(u);
	}
	const int OnBorderVertsCount = vb.Count() - 2;

	// Indexes
	for(i = 0; i < Sides; i++) {
		// Cap:
		ib.Add(0);
		const int Next = (i + 1) % OnBorderVertsCount + 2;
		ib.Add(Next);
		ib.Add(i + 2);
		// Cone:
		ib.Add(i + 2);
		ib.Add(Next);
		ib.Add(1);
	}

	if(ib.Count() < 3) {
		return;
	}

	static int ShaderID = GetShaderID("Colored", cVertex::PositionColored::FormatID);
	SetShader(ShaderID);
	DrawIndexed(cTopology::TriangleList, vb.ToPtr(), vb.Count(), ib.ToPtr(), ib.Count());
} // cRender::DrawConeSolid

//----------------------------------------------------------------------------------------------------------------------------------------------
// cRender::DrawConeWire
//----------------------------------------------------------------------------------------------------------------------------------------------
void cRender::DrawConeWire(const cVec3 &Apex, const cVec3 &Axis, const float Radius, const float Length, const cColor &Color, const int Sides) {
	static cList<cVertex::PositionColored> vb;
	vb.Clear();

	static cList<int> ib;
	ib.Clear();

	const cVec3 Normal = Axis.ToNormal();
	const cVec3 Cap = Apex - Length * Normal;
	cRotation Cone(Cap, Normal, 0.0f);
	
	// Vertices
	const cVec3 Start = Cap + Radius * Normal.ToPerp().ToNormal();
	cVertex::PositionColored u;
	u.Color = Color.ToDword();

	u.Pos = Apex;
	vb.Add(u);
	
	u.Pos = Start;
	vb.Add(u);
	
	int i;
	for(i = 1; i < Sides; i++) {
		Cone.SetAngle(360.0f / (float)Sides * (float)i);
		u.Pos = cVec3::Rotate(Start, Cone);
		vb.Add(u);
	}
	const int OnBorderVertsCount = vb.Count() - 1;

	// Indices
	for(i = 0; i < Sides; i++) {
		const int Next = (i + 1) % OnBorderVertsCount + 1;

		ib.Add(i + 1);
		ib.Add(0);

		ib.Add(i + 1);
		ib.Add(Next);
	}


	static int ShaderID = GetShaderID("Colored", cVertex::PositionColored::FormatID);
	if(!ib.IsEmpty()) {
		SetShader(ShaderID);
		DrawIndexed(cTopology::LineList, vb.ToPtr(), vb.Count(), ib.ToPtr(), ib.Count());
	}
} // cRender::DrawConeWire

//-----------------------------------------------------------------------------
// cRender::GridArgs::SetDefaults
//-----------------------------------------------------------------------------
void cRender::GridArgs::SetDefaults() {
	Size = 20.0f; // units
	UpAxisLength = 20.0f;
	GridLinesEvery = 5.0f; // units
	Subdivisions = 5;
	ShowAxes = true;
	ShowGridLines = true;
	ShowSubdivisionLines = true;
	OverrideViewProjectionMatrix = false;
	ViewProjectionMatrix.SetIdentity();
} // cRender::GridArgs::SetDefaults

//-----------------------------------------------------------------------------
// cRender::GridArgs::Validate
//-----------------------------------------------------------------------------
void cRender::GridArgs::Validate() {
	const GridArgs Defs;

	if(Size < 1.0f) {
		Size = Defs.Size;
	}
	if(UpAxisLength < cMath::Epsilon) {
		UpAxisLength = Defs.UpAxisLength;
	}
	if(GridLinesEvery < cMath::Epsilon) {
		GridLinesEvery = Defs.GridLinesEvery;
	}
	if(Subdivisions < 1) {
		Subdivisions = 1;
	}
} // cRender::GridArgs::Validate

//-----------------------------------------------------------------------------
// cRender::DrawGrid
//-----------------------------------------------------------------------------
void cRender::DrawGrid(GridArgs Args) {
	Args.Validate();
	
	const int UpAxis = 0; // 0 - Y, 1 - Z

	const int h = UpAxis == 0 ? 2 : 1;

	static cList<cVertex::PositionColored> vb;
	vb.Clear();
	
	cVertex::PositionColored u[2];

	u[0].Pos = u[1].Pos = cVec3::Zero;

	float d, c;
	int LinesCount, i;

	//*************************************************************************
	// Subdivision lines
	//*************************************************************************
	if(Args.ShowSubdivisionLines) {
		u[0].Color = u[1].Color = cColor::Gray.ToDword();
		
		d = Args.GridLinesEvery / (float)Args.Subdivisions;
		LinesCount = (int)(Args.Size / d);
		for(i = 1; i <= LinesCount; i++) {
			if(i % Args.Subdivisions == 0) {
				continue;
			}
			c = i * d;

			u[0].Pos[0] = -Args.Size;
			u[0].Pos[h] = c;
			u[1].Pos[0] = Args.Size;
			u[1].Pos[h] = c;
			vb.AddRange(u, 2);

			u[0].Pos[0] = -Args.Size;
			u[0].Pos[h] = -c;
			u[1].Pos[0] = Args.Size;
			u[1].Pos[h] = -c;
			vb.AddRange(u, 2);

			u[0].Pos[0] = c;
			u[0].Pos[h] = -Args.Size;
			u[1].Pos[0] = c;
			u[1].Pos[h] = Args.Size;
			vb.AddRange(u, 2);

			u[0].Pos[0] = -c;
			u[0].Pos[h] = -Args.Size;
			u[1].Pos[0] = -c;
			u[1].Pos[h] = Args.Size;
			vb.AddRange(u, 2);
		}
	}

	//*************************************************************************
	// Axes
	//*************************************************************************
	if(Args.ShowAxes) {
		u[0].Pos.SetZero();
		u[0].Color = u[1].Color = cColor::Black.ToDword();
		
		// Along negative X
		u[1].Pos = Args.Size * cVec3::AxisNegX;
		vb.AddRange(u, 2);
		if(UpAxis == 0) {
			u[1].Pos = Args.Size * cVec3::AxisNegZ;
			vb.AddRange(u, 2);
		} else {
			u[1].Pos = Args.Size * cVec3::AxisNegY;
			vb.AddRange(u, 2);
		}

		// Along positive X
		u[0].Color = u[1].Color = cColor::ToDword(255, 0, 0);
		u[1].Pos = Args.Size * cVec3::AxisX;
		vb.AddRange(u, 2);

		// Along positive up and remaining non up axes
		if(UpAxis == 0) {
			u[0].Color = u[1].Color = cColor::ToDword(0, 255, 0);
			u[1].Pos = Args.UpAxisLength * cVec3::AxisY;
			vb.AddRange(u, 2);

			u[0].Color = u[1].Color = cColor::ToDword(0, 0, 255);
			u[1].Pos = Args.Size * cVec3::AxisZ;
			vb.AddRange(u, 2);
		} else {
			u[0].Color = u[1].Color = cColor::ToDword(0, 0, 255);
			u[1].Pos = Args.UpAxisLength * cVec3::AxisZ;
			vb.AddRange(u, 2);

			u[0].Color = u[1].Color = cColor::ToDword(0, 255, 0);
			u[1].Pos = Args.UpAxisLength * cVec3::AxisZ;
			vb.AddRange(u, 2);
		}
	}

	//*************************************************************************
	// Grid lines
	//*************************************************************************
	if(Args.ShowGridLines) {
		u[0].Color = u[1].Color = cColor::DimGray.ToDword();

		LinesCount = (int)(Args.Size / Args.GridLinesEvery);
		for(i = 1; i <= LinesCount; i++) {
			d = i * Args.GridLinesEvery;

			u[0].Pos[0] = -Args.Size;
			u[0].Pos[h] = d;
			u[1].Pos[0] = Args.Size;
			u[1].Pos[h] = d;
			vb.AddRange(u, 2);

			u[0].Pos[0] = -Args.Size;
			u[0].Pos[h] = -d;
			u[1].Pos[0] = Args.Size;
			u[1].Pos[h] = -d;
			vb.AddRange(u, 2);

			u[0].Pos[0] = d;
			u[0].Pos[h] = -Args.Size;
			u[1].Pos[0] = d;
			u[1].Pos[h] = Args.Size;
			vb.AddRange(u, 2);

			u[0].Pos[0] = -d;
			u[0].Pos[h] = -Args.Size;
			u[1].Pos[0] = -d;
			u[1].Pos[h] = Args.Size;
			vb.AddRange(u, 2);
		}
	}

	// Draw
	if(!vb.IsEmpty()) {
		static int ShaderID = GetShaderID("Grid", cVertex::PositionColored::FormatID);
		static int WorldViewProjectionMatrixID = cRender::GetShaderConstID(ShaderID, "c_WorldViewProjectionMatrix");
		SetShader(ShaderID);
		SetShaderConst(WorldViewProjectionMatrixID, cMat4::Mul(cRender_WorldMatrix, Args.OverrideViewProjectionMatrix ? Args.ViewProjectionMatrix : GetViewer()->GetViewProjectionMatrix()));
		DrawArrays(cTopology::LineList, vb.ToPtr(), vb.Count());
	}
} // cRender::DrawGrid

//-----------------------------------------------------------------------------
// cRender::DrawNoPreview
//-----------------------------------------------------------------------------
void cRender::DrawNoPreview(const cRect &Bounds, const cColor &Color) {
	const float Standard = 165.0f;
	const float w = Bounds.GetWidth();
	const float h = Bounds.GetHeight();
	const float ScaleX = w / Standard;
	const float ScaleY = h / Standard;
	const cVec2 d(20.0f * ScaleX, 20.0f * ScaleY);
	const cVec2 t(7.0f * ScaleX, 7.0f * ScaleY);

	const float dx = w <= h ? d.x : d.x + (w - h) / 2.0f;
	const float dy = h <= w ? d.y : d.y + (h - w) / 2.0f;

	const cVec3 xs(t.x, 0.0f, 0.0f);
	const cVec3 ys(0.0f, t.y, 0.0f);

	cVertex::PositionColored lt, lt_ys, lt_xs;
	lt.Pos.Set(dx, -dy, 0.0f);
	lt.Pos += cVec3(Bounds.GetTopLeft(), 0.0f);
	lt.Color = Color.ToDword();
	lt_ys = lt;
	lt_ys.Pos -= ys;
	lt_xs = lt;
	lt_xs.Pos += xs;
	cVertex::PositionColored rt, rt_ys, rt_xs;
	rt.Pos.Set(w - dx, -dy, 0.0f);
	rt.Pos += cVec3(Bounds.GetTopLeft(), 0.0f);
	rt.Color = lt.Color;
	rt_ys = rt;
	rt_ys.Pos -= ys;
	rt_xs = rt;
	rt_xs.Pos -= xs;
	cVertex::PositionColored lb, lb_xs, lb_ys;
	lb.Pos.Set(dx, dy, 0.0f);
	lb.Pos += cVec3(Bounds.GetBottomLeft(), 0.0f);
	lb.Color = lt.Color;
	lb_xs = lb;
	lb_xs.Pos += xs;
	lb_ys = lb;
	lb_ys.Pos += ys;
	cVertex::PositionColored rb, rb_xs, rb_ys;
	rb.Pos.Set(w - dx, dy, 0.0f);
	rb.Pos += cVec3(Bounds.GetBottomLeft(), 0.0f);
	rb.Color = lt.Color;
	rb_xs = rb;
	rb_xs.Pos -= xs;
	rb_ys = rb;
	rb_ys.Pos += ys;

	static cList<cVertex::PositionColored> vb;
	vb.Clear();

	// First strip
	vb.Add(lt);
	vb.Add(lt_ys);
	vb.Add(rb_xs);
	vb.Add(rb);
	vb.Add(rb_ys);
	vb.Add(lt_xs);
	// Second strip
	vb.Add(lb);
	vb.Add(lb_xs);
	vb.Add(rt_ys);
	vb.Add(rt);
	vb.Add(rt_xs);
	vb.Add(lb_ys);
	
	static const int ib[] = {
		0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5,
		6, 7, 8, 6, 8, 9, 6, 9, 10, 6, 10, 11
	};

	static int Blend = GetBlendStateID(cBlendFactor::SrcAlpha, cBlendFactor::OneMinusSrcAlpha);
	SetBlendState(Blend);
	SetDepthState(-1);
	SetCullMode(cCullMode::None);

	static int ShaderID = cRender::GetShaderID("SolidUI", cVertex::PositionColored::FormatID);
	cRender::SetShader(ShaderID);
	cRender::DrawIndexed(cTopology::TriangleList, vb.ToPtr(), vb.Count(), ib, 24);
} // cRender::DrawNoPreview

//---------------------------------------------------------------------------------------------------------------------------------------------------
// cRender::DrawSphereWire
//---------------------------------------------------------------------------------------------------------------------------------------------------
void cRender::DrawSphereWire(const cVec3 &Center, const float Radius, const cColor &Color, const int Sections, const int Slices, const cVec3 &Axis) {
	float SectionAngle = 360.0f / (float)Sections;
	float SliceAngle = 180.0f / (float)Slices;

	int NVerts = Sections * (Slices - 1) + 2;
	int NIndices = Sections * 2 * 2 + Sections * 2 + Sections * (Slices - 2) * 2 * 2;

	static cList<cVertex::PositionColored> vb;
	vb.Clear();

	static cList<int> ib;
	ib.Clear();

	vb.SetCount(NVerts);
	ib.SetCount(NIndices, 0);

	// Vertices:
	int nVert = 0, nRing, nSeg;
	cRotation AxisTube(cVec3::Zero, Axis, 0.0f), RingTube;
	cVec3 Pole = AxisTube.GetAxis() * Radius;
	cVec3 StartRef = AxisTube.GetAxis().ToPerp().ToNormal() * Radius, RingRef;
	float Theta, Phi;
	dword C = Color.ToDword();

	for(nRing = 0; nRing < Sections; nRing++) {
		Theta = (float)nRing * SectionAngle;
		AxisTube.SetAngle(Theta);
		RingRef = nRing > 0 ? cVec3::Rotate(StartRef, AxisTube) : StartRef;
		RingTube.Set(cVec3::Zero, cVec3::Cross(Pole, RingRef).ToNormal(), 0.0f);
		
		for(nSeg = 1; nSeg < Slices; nSeg++, nVert++) {
			cVertex::PositionColored &r = vb[nVert];
			Phi = -90.0f + (float)nSeg * SliceAngle;
			RingTube.SetAngle(Phi);
			r.Pos = cVec3::Rotate(RingRef, RingTube);
			r.Color = C;
		}
	}

	// Top pole
	vb[nVert].Pos = Pole;
	vb[nVert].Color = C;
	nVert++;

	// Bottom pole
	vb[nVert].Pos = -Pole;
	vb[nVert].Color = C;
	nVert++;

	cAssert(NVerts == nVert);

	// Indices:
	int *Ln = ib.ToPtr(), nSec, nSlice, tl, tr;

	for(nSec = 0; nSec < Sections; nSec++) {
		// Top pole
		Ln[0] = NVerts - 2;
		Ln[1] = Ln[2] = nSec * (Slices - 1);
		Ln += 2;
		
		// Central slices
		for(nSlice = 0; nSlice < Slices - 2; nSlice++) {
			tl = nSec * (Slices - 1) + nSlice;
			tr = ((nSec + 1) % Sections) * (Slices - 1) + nSlice;
			Ln[0] = tl;
			Ln[1] = tl + 1;
			Ln += 2;

			Ln[0] = tl;
			Ln[1] = tr;
			Ln += 2;
		}

		// Bottom pole
		Ln[0] = NVerts - 1;
		Ln[1] = Ln[2] = nSec * (Slices - 1) + Slices - 2;
		Ln += 2;

		Ln[1] = ((nSec + 1) % Sections) * (Slices - 1) + Slices - 2;
		Ln += 2;
	}
	cAssert(Ln - ib.ToPtr() == NIndices);

	for(nVert = 0; nVert < vb.Count(); nVert++) {
		vb[nVert].Pos += Center;
	}

	static int Sh = GetShaderID("Colored", cVertex::PositionColored::FormatID);

	if(!ib.IsEmpty()) {
		SetShader(Sh);
		DrawIndexed(cTopology::LineList, vb.ToPtr(), vb.Count(), ib.ToPtr(), ib.Count());
	}
} // cRender::DrawSphereWire

//-----------------------------------------------------------------------------
// cRender::DrawCamera
//-----------------------------------------------------------------------------
void cRender::DrawCamera(const cColor &Color) {
	static cList<cVertex::PositionColored> vb;
	vb.Clear();
	
	cVertex::PositionColored u;
	u.Color = cColor::White.ToDword();

	static int Sh = cRender::GetShaderID("Colored", cVertex::PositionColored::FormatID);
	cRender::SetShader(Sh);
	
	int i;
	const cVec2 Profile[10] = {
		cVec2(-0.3f, 0.55f), cVec2(0.0f, 0.45f),
		cVec2(0.0f, 0.75f), cVec2(0.3f, 1.05f), cVec2(2.0f, 1.05f),
		cVec2(2.0f, -0.45f), cVec2(1.7f, -0.75f), cVec2(0.0f, -0.75f),
		cVec2(0.0f, -0.45f), cVec2(-0.3f, -0.55f)
	};

	for(i = 0; i < 10; i++) {
		u.Pos.Set(0.45f, Profile[i][1], Profile[i][0]);
		vb.Add(u);
	}
	u = vb[0];
	vb.Add(u);
	cRender::DrawArrays(cTopology::LineStrip, vb.ToPtr(), vb.Count());

	vb.Clear();
	for(i = 0; i < 10; i++) {
		u.Pos.Set(-0.45f, Profile[i][1], Profile[i][0]);
		vb.Add(u);
	}
	u = vb[0];
	vb.Add(u);
	cRender::DrawArrays(cTopology::LineStrip, vb.ToPtr(), vb.Count());

	vb.Clear();
	for(i = 0; i < 10; i++) {
		u.Pos.Set(-0.45f, Profile[i][1], Profile[i][0]);
		vb.Add(u);
		
		u.Pos.Set(0.45f, Profile[i][1], Profile[i][0]);
		vb.Add(u);
	}
	cRender::DrawArrays(cTopology::LineList, vb.ToPtr(), vb.Count());
} // cRender::DrawCamera

//-----------------------------------------------------------------------------
// cRender::DrawFrustumWire
//-----------------------------------------------------------------------------
void cRender::DrawFrustumWire(const cFrustum &Frustum, const cColor &Color) {
	cVertex::PositionColored vb[24];
	dword c = Color.ToDword();

	const cVec3 *P = Frustum.GetPoints();

	int i, i0;
	for(i = 0; i < 24; i++) {
		vb[i].Color = c;
		i0 = cFrustum::Segments[i];
		vb[i].Pos = P[i0];
	}
	
	static int Sh = GetShaderID("Colored", cVertex::PositionColored::FormatID);
	SetShader(Sh);
	DrawArrays(cTopology::LineList, vb, 24);
} // cRender::DrawFrustumWire

//-----------------------------------------------------------------------------
// cRender::DrawFrustumWireCurShader
//-----------------------------------------------------------------------------
void cRender::DrawFrustumWireCurShader(const cFrustum &Frustum) {
	cVec3 vb[24];

	const cVec3 *P = Frustum.GetPoints();

	int i, i0;
	for(i = 0; i < 24; i++) {
		i0 = cFrustum::Segments[i];
		vb[0] = P[i0];
	}
	
	DrawArrays(cTopology::LineList, vb, 24);
} // cRender::DrawFrustumWireCurShader

//-----------------------------------------------------------------------------
// cRender::DrawFrustumSolidCurShader
//-----------------------------------------------------------------------------
void cRender::DrawFrustumSolidCurShader(const cFrustum &Frustum) {
	cVec3 vb[36];

	const cVec3 *P = Frustum.GetPoints();
	
	int i, i0;
	for(i = 0; i < 36; i++) {
		i0 = cFrustum::Triangles[i];
		vb[i] = P[i0];
	}
	
	DrawArrays(cTopology::TriangleList, vb, 36);
} // cRender::DrawFrustumSolidCurShader

//-----------------------------------------------------------------------------
// cRender::PrecompileShaders
//-----------------------------------------------------------------------------
void cRender::PrecompileShaders(const int Platform) {
	const cStr SourceExt[] = {
		"hlsl",
		"hlsl"
	};
	const cStr Compiler[] = {
		"../Libs/PrecompileShaders/PC.exe",
		"../Libs/PrecompileShaders/Xbox360.exe"
	};
	const cStr VSArgs[] = {
		"/T vs_3_0 /Fo OBJ SRC",
		"\"SRC\" /Tvs_3_0 /Fo\"OBJ\""
	};
	const cStr PSArgs[] = {
		"/T ps_3_0 /Fo OBJ SRC",
		"\"SRC\" /Tps_3_0 /Fo\"OBJ\""
	};
	
	cStr TempFilePn = cRender_CacheFolder[Platform];
	TempFilePn.AppendPath("Temp.txt");
	TempFilePn = cIO::EnsureAbsolutePath(TempFilePn);
	// Remove precompiled shaders
	cList<cStr> L;
	cIO::SearchFiles(cRender_CacheFolder[Platform], &L);
	int i;
	for(i = 0; i < L.Count(); i++) {
		cIO::RemoveFile(L[i]);
	}
	// Ensure path
	cIO::CreatePath(cRender_CacheFolder[Platform]);
	// Search sources
	cList<cStr> S;
	cIO::SearchFilesRecursive("data/Shaders", &S, SourceExt[Platform]);
	// Enum sources
	cFile File;
	cStr l;
	cList<cStr> Defs, t, Src;
	const char *Magic = "// Precompile";
	const char *VSMagic = "// Vertex shader";
	const char *FSMagic = "// Fragment shader";
	const char *PSMagic = "// Pixel shader";

	bool r;
	int j, p, i0, i1, i2, iTCS, iTES;
	cStr D, VS, GS, TCS, TES, PS, Args, B, n, Cache;
	for(i = 0; i < S.Count(); i++) {
		const cStr &P = S[i];
		if(!cIO::LoadFile(P, &File)) {
			continue;
		}
		Defs.Clear();
		Src.Clear();
		r = false;
		while(File.ReadString(&l)) {
			if(cStr::Equals(l, Magic)) {
				r = true;
				break;
			}
		}
		if(!r) {
			continue;
		}
		while(File.ReadString(&l)) {
			if(cStr::Equals(l, VSMagic) || cStr::Equals(l, FSMagic) || cStr::Equals(l, PSMagic)) {
				break;
			}
			if(l.IsEmpty()) {
				continue;
			}
			l.Split(&t);
			if(t.IsEmpty()) {
				continue;
			}
			if(t[0].StartsWith("//")) {
				continue;
			}
			D.Clear();
			if(t[0] != "nullptr") {
				for(p = 0; p < t.Count(); p++) {
					D += "#define " + t[p] + cStr::EndLn;
				}
			}
			if(Defs.Contains(D, cStr::Equals)) {
				continue;
			}
			Defs.Add(D);
			Src.Add(l);
			Src.GetLast().Trim(" ");
		}
		if(Defs.IsEmpty()) {
			continue;
		}

		Stub::LoadShader(P, &VS, &GS, &TCS, &TES,  &PS, &i0, &i2, &iTCS, &iTES, &i1);
		for(j = 0; j < Defs.Count(); j++) {
			const cStr &d = Defs[j];
			// Compile VS
			File.Clear();
			File.WriteBytes(d.ToCharPtr(), d.Length());
			File.WriteBytes(VS.ToCharPtr(), VS.Length());
			cIO::SaveFile(TempFilePn, File);
			B = cStr::ToString(j);
			B.PadLeft(4, '0');
			B.Insert(0, cStr::ToString(i));
			B.PadLeft(8, '0');
			n = B;
			n += ".VS";
			n.Insert(0, cStr(cRender_CacheFolder[Platform]) + "/");
			n = cIO::EnsureAbsolutePath(n);
			Args = VSArgs[Platform];
			Args.Replace("SRC", TempFilePn);
			Args.Replace("OBJ", n);
			if(!cIO::Exec(cIO::EnsureAbsolutePath(Compiler[Platform]), Args, true)) {
				cLog::Warning("Can't compile vertex shader \"%s\"%s%s", P.ToCharPtr(), cStr::EndLn.ToCharPtr(), d.ToCharPtr());
				continue;
			}
			// Compile PS
			File.Clear();
			File.WriteBytes(d.ToCharPtr(), d.Length());
			File.WriteBytes(PS.ToCharPtr(), PS.Length());
			cIO::SaveFile(TempFilePn, File);
			n.SetFileExtension("PS");
			Args = PSArgs[Platform];
			Args.Replace("SRC", TempFilePn);
			Args.Replace("OBJ", n);
			if(!cIO::Exec(cIO::EnsureAbsolutePath(Compiler[Platform]), Args, true)) {
				cLog::Warning("Can't compile pixel shader \"%s\"%s%s", P.ToCharPtr(), cStr::EndLn.ToCharPtr(), d.ToCharPtr());
				continue;
			}
			// Cache
			Cache += P;
			Cache += cStr::EndLn + Src[j];
			Cache += cStr::EndLn + B + cStr::EndLn + cStr::EndLn;
		}
	}
	n = cRender_CacheFolder[Platform];
	n.AppendPath("Cache.txt");
	cIO::SaveFile(n, Cache.ToCharPtr(), Cache.Length());
	cIO::RemoveFile(TempFilePn);
} // cRender::PrecompileShaders

} // comms
