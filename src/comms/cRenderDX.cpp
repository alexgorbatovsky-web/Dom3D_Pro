#include "comms.h"

#ifdef COMMS_DIRECTX
//#define COMMS_DIRECTX11

#ifdef COMMS_3DCOAT
void LogNewShader(const char* name,int VType,const char* Extra);
#endif // COMMS_3DCOAT

HWND cWinMain_GetWindow();

namespace comms {

//*****************************************************************************
// cRenderDX
//*****************************************************************************
class cRenderDX : public cRender::Stub {
public:
	const cRenderType::Enum GetType() {
		return cRenderType::DirectX;
	}

	bool Init(const int MaxSamples);
	void Free();

	void SetViewport(const cRect &Viewport);
	void SetClipRect(const cRect &ClipRect);
	
	void Clear(const bool ClearColor, const bool ClearDepth, const cColor &Color, const float Depth);

	void BeginFrame();
	void EndFrame();

	void Finish();

	const cStr & GetVendor() const;
	int GetMRTCount();
	bool SupportsFormat(const cFormat::Enum Format);
	bool SupportsNonPowerOfTwo();
	bool ScreenShot(cImage *To);

	bool GetDeviceRecentReset();

	int GetDepthStateID(const bool TestEnabled, const bool WriteEnabled, const cDepthFunc::Enum Func);
	void SetDepthState(const int DepthStateID);

	void SetCullMode(const cCullMode::Enum CullMode);
	
	int GetTextureID(const char *FilePn);
	void ReloadTextures();
	bool ReloadTexture(const int TextureID);
	void ReloadTexture(const char *FilePn);
	void UnloadTextures();
	int AddTexture(const char *FilePn);
	int AddTexture(const cImage &Image);
	void UpdateTexture(const int TextureID, const cList<cImage *> &SubImages, const cList<cRect> &Rects);
	void FreeTexture(const int TextureID);
	
	const cStr * GetTextureFilePn(const int TextureID);
	int GetTextureWidth(const int TextureID);
	int GetTextureHeight(const int TextureID);
	int GetTextureDepth(const int TextureID);
	const cFormat::Enum GetTextureFormat(const int TextureID);
	int GetTextureMipMapCount(const int TextureID);
	bool GetTextureRenderTargetUsage(const int TextureID);
	
	int GetShaderID(const char *ShaderName, const int VertexFormatID, const char *Extra, cStr *OptionalLog);
	int AddShader(const char *ShaderName, const int VertexFormatID, const char *Extra, cStr *OptionalLog);
	int GetShaderConstID(const int ShaderID, const char *ConstName);
	void SetShader(const int ShaderID);
	void FreeShader(const int ShaderID);
	void ReloadShaders();

	int AddVertexFormat(const cVertex::Format &Format);
	int GetVertexSize(const int VertexFormatID);

	int AddVertexBuffer(const void *Data, const size_t Size);
	int AddIndexBuffer(const void *Data, const int IndexCount, const int IndexSize);
	void FreeVertexBuffer(const int VertexBufferID);
	void FreeIndexBuffer(const int IndexBufferID);

	void SetVertexBuffer(const int VertexBufferID);
	void SetIndexBuffer(const int IndexBufferID);

	//*************************************************************************
	// Render target
	//*************************************************************************
	int AddRenderTarget(const int Width, const int Height, const cFormat::Enum Format, const bool CubeMap, const int Samples);
	int AddRenderDepth(const int RenderTargetID, const cFormat::Enum Format);
	void SetRenderTargets(const int *ColorRenderTargetIDs, const int ColorRenderTargetCount, const int DepthRenderTargetID, const int *Faces);
	void SetRenderTargetSize(const int RenderTargetID, const int Width, const int Height);
	bool SaveRenderTarget(const int RenderTargetID, cImage *To);
    bool SaveRenderTargetRaw(const int RenderTargetID, cRender::SaveRenderTargetArgs *Args);
    bool SaveRenderTargetAsyncRaw(const int RenderTargetID, cRender::SaveRenderTargetArgs *Args);
	
	//*************************************************************************
	// SamplerState
	//*************************************************************************
	int GetSamplerStateID(const cFilter::Enum Filter, const cAddressMode::Enum s, const cAddressMode::Enum t, const cAddressMode::Enum r);
	void SetSamplerState(const int TextureID, const int SamplerStateID);
	
	//*************************************************************************
	// Blend
	//*************************************************************************
	int GetBlendStateID(const cBlendFactor::Enum SrcFactor, const cBlendFactor::Enum DstFactor, const cBlendMode::Enum Mode, const dword Mask);
	void SetBlendState(const int BlendStateID);

	int GetSamplerID(const int ShaderID, const char *SamplerName) const;
	void SetTexture(const int SamplerID, const int TextureID);

	void SetShaderConst(const int ConstID, const int Value);
	void SetShaderConst(const int ConstID, const float Value);
	void SetShaderConst(const int ConstID, const cVec2 &Value);
	void SetShaderConst(const int ConstID, const cVec3 &Value);
	void SetShaderConst(const int ConstID, const cVec4 &Value);
	void SetShaderConst(const int ConstID, const cMat3 &Value);
	void SetShaderConst(const int ConstID, const cMat4 &Value);
	void SetShaderConst(const int ConstID, const float *Array, const int Count);
	void SetShaderConst(const int ConstID, const cVec2 *Array, const int Count);
	void SetShaderConst(const int ConstID, const cVec3 *Array, const int Count);
	void SetShaderConst(const int ConstID, const cVec4 *Array, const int Count);
	void SetShaderAutoConstants();
	
	void DrawArrays(const cTopology::Enum Topology);
	void DrawArrays(const cTopology::Enum Topology, const void *VertexData, const int VertexCount);
	void DrawIndexed(const cTopology::Enum Topology);
	void DrawIndexed(const cTopology::Enum Topology, const void *VertexData, const int VertexCount, const void *IndexData, const int IndexCount, const int IndexSize);

	void SetWireframe(const bool Enabled);
private:
	void SetDynVertexBuffer(const void *Data, const size_t Size);
	void SetDynIndexBuffer(const void *Data, const int IndexCount, const int IndexSize);
}; // cRenderDX

// cRender_CreateDX
cRender::Stub * cRender_CreateDX() {
	return new cRenderDX;
}

#ifdef COMMS_DIRECTX11

// h		C:\Program Files (x86)\Windows Kits\8.1\Include\um
// lib32	C:\Program Files (x86)\Windows Kits\8.1\Lib\winv6.3\um\x86
// lib64	C:\Program Files (x86)\Windows Kits\8.1\Lib\winv6.3\um\x64

#include "../Libs/DirectX11/d3d11.h"
#include "../Libs/DirectX11/d3dcompiler.h"
#pragma comment (lib, "Libs/DirectX11/x86/d3d11.lib")
#pragma comment (lib, "Libs/DirectX11/x86/d3dcompiler.lib")

// Direct3D for Windows Phone 8
// https://msdn.microsoft.com/en-us/library/windows/apps/jj207062(v=vs.105).aspx

// 1. Don't need to offset position or texture coords by 0.5 texel
// 2. ID3D10Device::CheckFormatSupport()
// 4. Backwards compatible shader compilation with special compiler flag D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY
// 5. There's no none mip filtering mode. Pass texture with only 1 mip level or set MaxLOD in sampler state to 0: D3D10_SAMPLER_DESC samp; samp.MaxLOD = 0.0f;
// 6. Enable render target masks: D3D10_BLEND_DESC bd; bd.RenderTargetWriteMask[0] = 0x0f; bd.RenderTargetWriteMask[1] = 0x0f; bd.RenderTargetWriteMask[4] = 0x0f;
// 7. D3D10_RASTERIZER_DESC rd; rd.DepthClipEnable = true;
// 8. Alpha blend is always on. Set "SrcBlendAlpha", "DestBlendAlpha", "BlendOpAlpha"

// Direct3D 9	Direct3D 10
// POSITION		SV_POSITION
// COLOR		SV_TARGET

// Direct3D feature levels
// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476876(v=vs.85).aspx


//*****************************************************************************
// cRenderDX_Format
//*****************************************************************************
static const DXGI_FORMAT cRenderDX_Format[cFormat::Count] = {
	DXGI_FORMAT_UNKNOWN,	// None
	
	DXGI_FORMAT_R8_UNORM,			// R8
	DXGI_FORMAT_R8G8_UNORM,			// Rg8
	DXGI_FORMAT_UNKNOWN,			// Rgb8 (not supported)
	DXGI_FORMAT_R8G8B8A8_UNORM,		// Rgba8

	DXGI_FORMAT_R16_UNORM,			// R16
	DXGI_FORMAT_R16G16_UNORM,		// Rg16
	DXGI_FORMAT_UNKNOWN,			// Rgb16 (not supported)
	DXGI_FORMAT_R16G16B16A16_UNORM,	// Rgba16

	DXGI_FORMAT_R16_FLOAT,			// R16f
	DXGI_FORMAT_R16G16_FLOAT,		// Rg16f
	DXGI_FORMAT_UNKNOWN,			// Rgb16f (not supported)
	DXGI_FORMAT_R16G16B16A16_FLOAT,	// Rgba16f

	DXGI_FORMAT_R32_FLOAT,			// R32f
	DXGI_FORMAT_R32G32_FLOAT,		// Rg32f
	DXGI_FORMAT_R32G32B32_FLOAT,	// Rgb32f
	DXGI_FORMAT_R32G32B32A32_FLOAT,	// Rgba32f
	
	DXGI_FORMAT_D16_UNORM,			// Depth16
	DXGI_FORMAT_D24_UNORM_S8_UINT,	// Depth24
	DXGI_FORMAT_D24_UNORM_S8_UINT,	// Depth24Stencil8
	
	DXGI_FORMAT_BC1_UNORM,			// Dxt1
	DXGI_FORMAT_BC3_UNORM,			// Dxt3
	DXGI_FORMAT_BC5_UNORM			// Dxt5
}; // cRenderDX_Format

// Multisampling in Windows Runtime apps
// https://msdn.microsoft.com/en-us/library/windows/apps/dn458384.aspx

struct cRenderDX_Sys {
	IDXGISwapChain *SwapChain;
	ID3D11Device *Device;
	ID3D11DeviceContext *DeviceContext;
	cStr Vendor;
	void SamplesForFormat(DXGI_FORMAT Format, DXGI_SAMPLE_DESC *SD, UINT MaxSamples) {
		memset(SD, 0, sizeof(*SD));
		SD->Count = 1; // Minimum is 1.
		for(UINT M = 2; M < D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT && M < MaxSamples; M *= 2) {
			UINT Q = 0;
			HRESULT hr = Device->CheckMultisampleQualityLevels(Format, M, &Q);
			if(SUCCEEDED(hr) && Q > 0) {
				SD->Count = M;
				// "Quality" should be between zero and one less than
				// the level returned by ID3D11Device::CheckMultisampleQualityLevels.
				continue;
			}
			break;
		}
	}
	void Finish() {
		ID3D11Query *E = nullptr;
		D3D11_QUERY_DESC D;
		D.MiscFlags = 0;
		D.Query = D3D11_QUERY_EVENT;
		Device->CreateQuery(&D, &E);
		DeviceContext->End(E);
		while(S_FALSE == DeviceContext->GetData(E, nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH)) {}
		E->Release(); E = nullptr;
	}
	cRenderDX_Sys() {
		SwapChain = nullptr;
		Device = nullptr;
		DeviceContext = nullptr;
	}
};
static cRenderDX_Sys s_Sys;

struct cRenderDX_Caps {
	D3D_FEATURE_LEVEL FeatureLevel;
	int MaxTextureDimension;
	int MaxAnisotropy;
	int SimultaneousRenderTargets;
	bool NonPowerOfTwo;
	cStr VSTarget, PSTarget;
	const char * GetVersion() {
		switch(FeatureLevel) {
		case D3D_FEATURE_LEVEL_9_1: return "D3D_FEATURE_LEVEL_9_1";
		case D3D_FEATURE_LEVEL_9_2: return "D3D_FEATURE_LEVEL_9_2";
		case D3D_FEATURE_LEVEL_9_3: return "D3D_FEATURE_LEVEL_9_3";
		case D3D_FEATURE_LEVEL_10_0: return "D3D_FEATURE_LEVEL_10_0";
		case D3D_FEATURE_LEVEL_10_1: return "D3D_FEATURE_LEVEL_10_1";
		case D3D_FEATURE_LEVEL_11_0: return "D3D_FEATURE_LEVEL_11_0";
		case D3D_FEATURE_LEVEL_11_1: return "D3D_FEATURE_LEVEL_11_1";
		}
		return nullptr;
	}
	cRenderDX_Caps() {
		// Direct3D feature level 9_3 for Windows Phone 8
		// https://msdn.microsoft.com/en-us/library/windows/apps/jj714085(v=vs.105).aspx
		FeatureLevel = (D3D_FEATURE_LEVEL)0;
		MaxTextureDimension = D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION;
		MaxAnisotropy = D3D11_MAX_MAXANISOTROPY;
		SimultaneousRenderTargets = D3D_FL9_3_SIMULTANEOUS_RENDER_TARGET_COUNT;
		NonPowerOfTwo = true; // For 2D textures with dimensions that are not powers of two only one MIP-map level and
		// no wrap modes (AddressU, AddressV, and AddressW of D3D11_SAMPLER_DESC cannot be set to D3D11_TEXTURE_ADDRESS_WRAP).
		VSTarget = "vs_4_0_level_9_3";
		PSTarget = "ps_4_0_level_9_3";
	}
};
static cRenderDX_Caps s_Caps;

#define SAFE_RELEASE(Ptr) { if(Ptr != nullptr) { Ptr->Release(); Ptr = nullptr; } }

struct cRenderDX_RASTERIZER_STATE {
	ID3D11RasterizerState *Ptr;
	cCullMode::Enum CullMode;
	bool Wireframe;
	bool DepthTest;
	cRenderDX_RASTERIZER_STATE() {
		Ptr = nullptr;
		CullMode = cCullMode::None;
		Wireframe = false;
		DepthTest = false;
	}
	void Create() {
		cAssert(nullptr == Ptr);
		D3D11_RASTERIZER_DESC RD;
		memset(&RD, 0, sizeof(RD));
		RD.CullMode = D3D11_CULL_NONE;
		if(cCullMode::Clockwise) {
			RD.CullMode = D3D11_CULL_BACK;
		} else if(cCullMode::CounterClockwise) {
			RD.CullMode = D3D11_CULL_FRONT;
		}
		RD.FillMode = Wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
		RD.DepthClipEnable = DepthTest;
		RD.MultisampleEnable = true;
		HRESULT hr = s_Sys.Device->CreateRasterizerState(&RD, &Ptr);
		cAssertM(SUCCEEDED(hr), "Couldn't create rasterizer state");
	}
	void Free() {
		SAFE_RELEASE(Ptr);
	}
};
static cList<cRenderDX_RASTERIZER_STATE> s_RasterizerStates;
static int s_CurRasterizerStateID = -1;

struct cRenderDX_FrameBuffer {
	UINT Width;
	UINT Height;
	DXGI_SAMPLE_DESC SampleDesc;
	DXGI_FORMAT ColorFormat;
	DXGI_FORMAT DepthFormat;
	ID3D11Texture2D *ColorTex;
	ID3D11Texture2D *DepthTex;
	ID3D11RenderTargetView *ColorView;
	ID3D11DepthStencilView *DepthView;
	cRenderDX_FrameBuffer() {
		Width = 0;
		Height = 0;
		memset(&SampleDesc, 0, sizeof(SampleDesc));
		ColorFormat = (DXGI_FORMAT)0;
		DepthFormat = (DXGI_FORMAT)0;
		ColorTex = nullptr;
		DepthTex = nullptr;
		ColorView = nullptr;
		DepthView = nullptr;
	}
	void Create() {
		s_Sys.SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&ColorTex);
		s_Sys.Device->CreateRenderTargetView(ColorTex, nullptr, &ColorView);
		
		D3D11_TEXTURE2D_DESC TD;
		memset(&TD, 0, sizeof(TD));
		TD.Width = Width;
		TD.Height = Height;
		TD.MipLevels = 1;
		TD.ArraySize = 1;
		TD.Format = DepthFormat;
		TD.SampleDesc = SampleDesc;
		TD.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		HRESULT hr = s_Sys.Device->CreateTexture2D(&TD, nullptr, &DepthTex);
		cAssertM(SUCCEEDED(hr), "Failed to create depth buffer texture.");
		
		D3D11_DEPTH_STENCIL_VIEW_DESC DD;
		memset(&DD, 0, sizeof(DD));
		DD.Format = TD.Format;
		DD.ViewDimension = (TD.SampleDesc.Count > 1) ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
		hr = s_Sys.Device->CreateDepthStencilView(DepthTex, &DD, &DepthView);
		cAssertM(SUCCEEDED(hr), "Failed to create depth buffer view.");
		
		s_Sys.DeviceContext->OMSetRenderTargets(1, &ColorView, DepthView);
		
		D3D11_VIEWPORT Viewport;
		ZeroMemory(&Viewport, sizeof(Viewport));
		Viewport.Width = (FLOAT)Width;
		Viewport.Height = (FLOAT)Height;
		Viewport.MaxDepth = 1.0f;
		s_Sys.DeviceContext->RSSetViewports(1, &Viewport);
	}
	void Free() {
		if(s_Sys.DeviceContext != nullptr) {
			s_Sys.DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
		}
		SAFE_RELEASE(ColorTex);
		SAFE_RELEASE(DepthTex);
		SAFE_RELEASE(ColorView);
		SAFE_RELEASE(DepthView);
	}
};
static cRenderDX_FrameBuffer s_FrameBuffer;

//*****************************************************************************
// cRenderDX_VERTEX_FORMAT
//*****************************************************************************
struct cRenderDX_VERTEX_FORMAT {
	int VertexSize;
	cList<D3D11_INPUT_ELEMENT_DESC> Desc;
	cStr StrDesc;
	
	cRenderDX_VERTEX_FORMAT() {
		VertexSize = 0;
	}
}; // cRenderDX_VERTEX_FORMAT

static cList<cRenderDX_VERTEX_FORMAT> s_VertexFormats;

//*****************************************************************************
// cRenderDX_VERTEX_BUFFER
//*****************************************************************************
struct cRenderDX_VERTEX_BUFFER {
	ID3D11Buffer *Ptr;
	size_t Size;

	cRenderDX_VERTEX_BUFFER() {
		Ptr = nullptr;
		Size = 0;
	}
	void Free() {
		SAFE_RELEASE(Ptr);
	}
}; // cRenderDX_VERTEX_BUFFER

static cList<cRenderDX_VERTEX_BUFFER> s_VertexBuffers;
static cList<int> s_VertexBuffersFreeID;
static int s_CurVertexBufferID = -1, s_CurVertexBufferStride = 0, s_DynVertexBufferID = -1;
static size_t s_DynVertexBufferSize = 0;

//*****************************************************************************
// cRenderDX_INDEX_BUFFER
//*****************************************************************************
struct cRenderDX_INDEX_BUFFER {
	ID3D11Buffer *Ptr;
	int IndexCount;
	int IndexSize;

	cRenderDX_INDEX_BUFFER() {
		Ptr = nullptr;
		IndexCount = 0;
		IndexSize = 0;
	}
	void Free() {
		SAFE_RELEASE(Ptr);
	}
}; // cRenderDX_INDEX_BUFFER

static cList<cRenderDX_INDEX_BUFFER> s_IndexBuffers;
static cList<int> s_IndexBuffersFreeID;
static int s_CurIndexBufferID = -1, s_CurIndexBufferStride = 0, s_DynIndexBufferID = -1;
static size_t s_DynIndexBufferSize = 0;

//*****************************************************************************
// cRenderDX_SHADER
//*****************************************************************************
struct cRenderDX_SHADER {
	cStr Name;
	int VertexFormatID;
	cStr Extra;
	cStr FilePn;

	ID3D11VertexShader *VS;
	ID3D11PixelShader *PS;
	ID3D11InputLayout *InputLayout;
	
	struct ConstBuffer {
		ID3D11Buffer *Ptr;
		cList<byte> Memory;
		bool Dirty;
		ConstBuffer() {
			Ptr = nullptr;
			Dirty = false;
		}
	};
	cList<ConstBuffer> VSConstBuffers, PSConstBuffers;
	void ApplyConstants() {
		int i;
		for(i = 0; i < VSConstBuffers.Count(); i++) {
			ConstBuffer &C = VSConstBuffers[i];
			if(C.Dirty) {
				s_Sys.DeviceContext->UpdateSubresource(C.Ptr, 0, nullptr, C.Memory.ToPtr(), 0, 0);
				C.Dirty = false;
			}
		}
		for(i = 0; i < PSConstBuffers.Count(); i++) {
			ConstBuffer &C = PSConstBuffers[i];
			if(C.Dirty) {
				s_Sys.DeviceContext->UpdateSubresource(C.Ptr, 0, nullptr, C.Memory.ToPtr(), 0, 0);
				C.Dirty = false;
			}
		}
	}
	struct Const {
		cStr Name;
		int AutoConst;
		byte *VSData, *PSData;
		int VSBuffer, PSBuffer;
		static int Compare(const Const *l, const Const *r) {
			return cStr::Compare(l->Name, r->Name);
		}
	};
	cList<Const> Consts;
	void SetConst(const int ConstID, const void *Value, const size_t Size) {
		if(ConstID >= 0 && ConstID < Consts.Count()) {
			Const &C = Consts[ConstID];
			if(C.VSData != nullptr) {
				ConstBuffer &CB = VSConstBuffers[C.VSBuffer];
				cAssert((C.VSData >= CB.Memory.ToPtr()) && (C.VSData <= CB.Memory.ToPtr() + CB.Memory.Count() - Size));
				if(memcmp(C.VSData, Value, Size) != 0) {
					memcpy(C.VSData, Value, Size);
					CB.Dirty = true;
				}
			}
			if(C.PSData != nullptr) {
				ConstBuffer &CB = PSConstBuffers[C.PSBuffer];
				cAssert((C.PSData >= CB.Memory.ToPtr()) && (C.PSData <= CB.Memory.ToPtr() + CB.Memory.Count() - Size));
				if(memcmp(C.PSData, Value, Size) != 0) {
					memcpy(C.PSData, Value, Size);
					CB.Dirty = true;
				}
			}
		}
	}
	void Free() {
		SAFE_RELEASE(VS);
		SAFE_RELEASE(PS);
		SAFE_RELEASE(InputLayout);
		int i;
		for(i = 0; i < VSConstBuffers.Count(); i++) {
			SAFE_RELEASE(VSConstBuffers[i].Ptr);
		}
		for(i = 0; i < PSConstBuffers.Count(); i++) {
			SAFE_RELEASE(PSConstBuffers[i].Ptr);
		}
	}
	bool IsValid() const {
		return (VS != nullptr) && (PS != nullptr) && (InputLayout != nullptr);
	}
	void Ctor() {
		VertexFormatID = -1;

		VS = nullptr;
		PS = nullptr;
		InputLayout = nullptr;
	}
	cRenderDX_SHADER() {
		Ctor();
	}
	static int Count;
	static cRenderDX_SHADER * Alloc() {
		Count++;
		return new cRenderDX_SHADER;
	}
	static void Free(cRenderDX_SHADER **Sh) {
		if(*Sh != nullptr) {
			delete *Sh;
			*Sh = nullptr;
			Count--;
		}
	}
}; // cRenderDX_SHADER

int cRenderDX_SHADER::Count = 0;
static cList<cRenderDX_SHADER *> s_Shaders;
static cList<int> s_ShadersFreeID;
static int s_CurShaderID = -1;

//-------------------------------------------------------------------------------------------------------------------
// CreateShader
//-------------------------------------------------------------------------------------------------------------------
static bool CreateShader(const char *ShaderName, const int VertexFormatID, const char *Extra, cRenderDX_SHADER *Sh) {
	// Check shader name
	cAssert(ShaderName != nullptr);
	if(cStr::Length(ShaderName) < 1) {
		return false; // No name
	}

    cAssert(VertexFormatID >= 0 && VertexFormatID < s_VertexFormats.Count());
	if(VertexFormatID < 0 || VertexFormatID >= s_VertexFormats.Count()) {
		return false; // Invalid vertex format ID
	}
	const cRenderDX_VERTEX_FORMAT &vf = s_VertexFormats[VertexFormatID];
	
	// DX shader file pathname should be with the extension "hlsl":
	cStr FilePn = cStr::Format("data/Shaders/%s.hlsl", ShaderName);
	
	//*************************************************************************
	// Loading file
	//*************************************************************************
	cStr VSText, PSText;
	int VSLine, PSLine;
	
	if(!cRender::Stub::LoadShader(FilePn, &VSText, &PSText, &VSLine, &PSLine)) {
		return false;
	}
	
	Sh->Name = ShaderName;
	Sh->VertexFormatID = VertexFormatID;
	Sh->Extra = Extra; // We should not modify inline extra because "GetShaderID" will be unable to find the match
	cStr E = Sh->Extra;
	if(!E.IsEmpty()) {
		E.Trim(cStr::EndLn);
		if(!E.EndsWith("\n")) { // Trailing EOL
			E += "\n";
		}
		cLog::Message(E); // Show extra
	}
	Sh->FilePn = FilePn;
	
	Sh->VS = nullptr;
	Sh->PS = nullptr;
	
	UINT Flags = D3DCOMPILE_PACK_MATRIX_ROW_MAJOR | D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY;
	ID3D10Blob *ShaderBuffer = nullptr, *ErrorsBuffer = nullptr, *InputSignature = nullptr;
	ID3D11ShaderReflection *VSRefl = nullptr, *PSRefl = nullptr;
	HRESULT hr = 0;
	cStr Body;

	//*************************************************************************
	// Compiling VS
	//*************************************************************************
	if(!VSText.IsEmpty()) {
		cLog::Message("Compiling VS...");
		Body = E;
		Body += cStr::Format("#line %d\n", VSLine);
		Body += VSText;
		hr = D3DCompile(Body.ToCharPtr(), Body.Length(), Sh->FilePn.ToCharPtr(), nullptr, nullptr, "main", s_Caps.VSTarget.ToCharPtr(), Flags, 0, &ShaderBuffer, &ErrorsBuffer);
		if(SUCCEEDED(hr)) {
			hr = s_Sys.Device->CreateVertexShader(ShaderBuffer->GetBufferPointer(), ShaderBuffer->GetBufferSize(), nullptr, &Sh->VS);
			if(SUCCEEDED(hr)) {
				D3DGetInputSignatureBlob(ShaderBuffer->GetBufferPointer(), ShaderBuffer->GetBufferSize(), &InputSignature);
				D3DReflect(ShaderBuffer->GetBufferPointer(), ShaderBuffer->GetBufferSize(), __uuidof(ID3D11ShaderReflection), (void **)&VSRefl);
			}
		} else {
			cLog::Warning((const char *)ErrorsBuffer->GetBufferPointer());
		}
		SAFE_RELEASE(ShaderBuffer);
		SAFE_RELEASE(ErrorsBuffer);
	}

	//*************************************************************************
	// Compiling PS
	//*************************************************************************
	if(!PSText.IsEmpty()) {
		cLog::Message("Compiling PS...");
		Body = E;
		Body += cStr::Format("#line %d\n", PSLine);
		Body += PSText;
		hr = D3DCompile(Body.ToCharPtr(), Body.Length(), Sh->FilePn.ToCharPtr(), nullptr, nullptr, "main", s_Caps.PSTarget, Flags, 0, &ShaderBuffer, &ErrorsBuffer);
		if(SUCCEEDED(hr)) {
			hr = s_Sys.Device->CreatePixelShader(ShaderBuffer->GetBufferPointer(), ShaderBuffer->GetBufferSize(), nullptr, &Sh->PS);
			if(SUCCEEDED(hr)) {
				D3DReflect(ShaderBuffer->GetBufferPointer(), ShaderBuffer->GetBufferSize(), __uuidof(ID3D11ShaderReflection), (void **)&PSRefl);
			}
		} else {
			cLog::Warning((const char *)ErrorsBuffer->GetBufferPointer());
		}
		SAFE_RELEASE(ShaderBuffer);
		SAFE_RELEASE(ErrorsBuffer);
	}
	
	//*************************************************************************
	// Creating input layout
	//*************************************************************************
	if(InputSignature != nullptr) {
		hr = s_Sys.Device->CreateInputLayout(vf.Desc.ToPtr(), vf.Desc.Count(), InputSignature->GetBufferPointer(), InputSignature->GetBufferSize(), &Sh->InputLayout);
		cAssertM(SUCCEEDED(hr), "Couldn't match input layout " + vf.StrDesc);
		SAFE_RELEASE(InputSignature);
	}
	
	//*************************************************************************
	// Getting constant buffers
	//*************************************************************************
	D3D11_SHADER_DESC VSDesc, PSDesc;
	if(VSRefl != nullptr) {
		VSRefl->GetDesc(&VSDesc);
		Sh->VSConstBuffers.SetCount(VSDesc.ConstantBuffers);
	}
	if(PSRefl) {
		PSRefl->GetDesc(&PSDesc);
		Sh->PSConstBuffers.SetCount(PSDesc.ConstantBuffers);
	}

	D3D11_SHADER_BUFFER_DESC SB;
	D3D11_SHADER_VARIABLE_DESC V;
	D3D11_BUFFER_DESC B;
	memset(&B, 0, sizeof(B));
	B.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	int i, j;
	for(i = 0; i < Sh->VSConstBuffers.Count(); i++) {
		cRenderDX_SHADER::ConstBuffer &CB = Sh->VSConstBuffers[i];
		VSRefl->GetConstantBufferByIndex(i)->GetDesc(&SB);
		B.ByteWidth = SB.Size;
		s_Sys.Device->CreateBuffer(&B, nullptr, &CB.Ptr);
		CB.Memory.SetCount(B.ByteWidth, 0);
		for(j = 0; j < (int)SB.Variables; j++) {
			VSRefl->GetConstantBufferByIndex(i)->GetVariableByIndex(j)->GetDesc(&V);
			cRenderDX_SHADER::Const C;
			C.Name = V.Name;
			C.AutoConst = cRender::AutoConst::FromString(V.Name);
			C.VSData = CB.Memory.ToPtr() + V.StartOffset;
			C.PSData = nullptr;
			C.VSBuffer = i;
			C.PSBuffer = -1;
			Sh->Consts.Add(C);
		}
	}
	
	int MaxConst = Sh->Consts.Count(), Merge, l;
	for(i = 0; i < Sh->PSConstBuffers.Count(); i++) {
		cRenderDX_SHADER::ConstBuffer &CB = Sh->PSConstBuffers[i];
		PSRefl->GetConstantBufferByIndex(i)->GetDesc(&SB);
		B.ByteWidth = SB.Size;
		s_Sys.Device->CreateBuffer(&B, nullptr, &CB.Ptr);
		CB.Memory.SetCount(B.ByteWidth, 0);
		for(j = 0; j < (int)SB.Variables; j++) {
			PSRefl->GetConstantBufferByIndex(i)->GetVariableByIndex(j)->GetDesc(&V);
			Merge = -1;
			for(l = 0; l < MaxConst; l++) {
				if(cStr::Equals(Sh->Consts[i].Name, V.Name)) {
					Merge = l;
					break;
				}
			}
			if(-1 == Merge) {
				cRenderDX_SHADER::Const C;
				C.Name = V.Name;
				C.AutoConst = cRender::AutoConst::FromString(V.Name);
				C.VSData = nullptr;
				C.PSData = CB.Memory.ToPtr() + V.StartOffset;
				C.VSBuffer = -1;
				C.PSBuffer = i;
				Sh->Consts.Add(C);
			} else {
				Sh->Consts[Merge].PSData = CB.Memory.ToPtr() + V.StartOffset;
				Sh->Consts[Merge].PSBuffer = i;
			}
		}
	}
	Sh->Consts.Sort(cRenderDX_SHADER::Const::Compare);

	SAFE_RELEASE(VSRefl);
	SAFE_RELEASE(PSRefl);

	return Sh->IsValid();
} // CreateShader

// ApplyRasterizerState
static void ApplyRasterizerState(ID3D11RasterizerState *State) {
	s_Sys.DeviceContext->RSSetState(State);
}

// cRenderDX_ShowLog
static void cRenderDX_ShowLog() {
	cStr R = cStr(60, '-') << cStr::EndLn;
	const cStr Prefix(" DirectX | ");
	R += Prefix + "Version: " + cStr(s_Caps.GetVersion()) + cStr::EndLn;
	R += Prefix + "Vendor : " + s_Sys.Vendor + cStr::EndLn;
	R += Prefix + "Vertex shaders: " + s_Caps.VSTarget + cStr::EndLn;
	R += Prefix + "Pixel shaders: " + s_Caps.PSTarget + cStr::EndLn;
	if(s_FrameBuffer.SampleDesc.Count > 1) {
		R += Prefix + "Framebuffer antialiasing " + cStr::ToString((int)s_FrameBuffer.SampleDesc.Count) + "x" + cStr::EndLn;
	}
	R += cStr(60, '-') + cStr::EndLn;
	cLog::Message(R);
}

//-----------------------------------------------------------------------------
// cRenderDX::Init
//-----------------------------------------------------------------------------
bool cRenderDX::Init(const int MaxSamples) {
	// Anatomy of Direct3D 11 Create Device
	// http://blogs.msdn.com/b/chuckw/archive/2014/02/05/anatomy-of-direct3d-11-create-device.aspx
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &s_Sys.Device, &s_Caps.FeatureLevel, &s_Sys.DeviceContext);
	if(FAILED(hr)) {
		cLog::Warning("Failed to create Direct3D 11 device.");
		return false;
	}
	// Initialize Direct3D 11
	// https://msdn.microsoft.com/en-us/library/windows/apps/dn166878.aspx
	// To ensure we aren't rendering more often than the screen can actually display, we set frame latency to 1 and
	// use DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL. This saves power and is a store certification requirement.
	IDXGIFactory1 *pDXGIFactory = nullptr;
	IDXGIDevice1 *pDXGIDevice = nullptr;
	hr = s_Sys.Device->QueryInterface(__uuidof(IDXGIDevice1), (void **)&pDXGIDevice);
	if(SUCCEEDED(hr)) {
		IDXGIAdapter *pDXGIAdapter = nullptr;
		hr = pDXGIDevice->GetAdapter(&pDXGIAdapter);
		if(SUCCEEDED(hr)) {
			pDXGIAdapter->GetParent(__uuidof(IDXGIFactory1), (void **)&pDXGIFactory);
			DXGI_ADAPTER_DESC AdapterDesc;
			hr = pDXGIAdapter->GetDesc(&AdapterDesc);
			if(SUCCEEDED(hr)) {
				int W = lstrlenW(AdapterDesc.Description);
				int L = WideCharToMultiByte(CP_ACP, 0, AdapterDesc.Description, W, nullptr, 0, nullptr, nullptr);
				if(L > 0) {
					s_Sys.Vendor.SetLength(L);
					WideCharToMultiByte(CP_ACP, 0, AdapterDesc.Description, W, s_Sys.Vendor.ToCharPtr(), L, nullptr, nullptr);
				}
			}
			SAFE_RELEASE(pDXGIAdapter);
		}
		//pDXGIDevice->SetMaximumFrameLatency(1); // Windows Store apps must set frame latency to 1.
		SAFE_RELEASE(pDXGIDevice);
	}

	DXGI_SWAP_CHAIN_DESC SCD;
	memset(&SCD, 0, sizeof(SCD));
	SCD.BufferCount = 1;
	SCD.BufferDesc.Format = cRenderDX_Format[cFormat::Rgba8];
	SCD.BufferDesc.Width = cMain_GetClientWidth();
	SCD.BufferDesc.Height = cMain_GetClientHeight();
	SCD.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SCD.OutputWindow = cWinMain_GetWindow();
	SCD.Windowed = TRUE;
	// SCD.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // Windows Store apps must use DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL. Cannot be used with multisampling.
	s_Sys.SamplesForFormat(SCD.BufferDesc.Format, &SCD.SampleDesc, MaxSamples);
	
	hr = pDXGIFactory->CreateSwapChain(s_Sys.Device, &SCD, &s_Sys.SwapChain);
	if(FAILED(hr)) {
		cLog::Warning("Failed to create swap chain.");
		return false;
	}
	pDXGIFactory->MakeWindowAssociation(cWinMain_GetWindow(), DXGI_MWA_NO_ALT_ENTER);
	SAFE_RELEASE(pDXGIFactory);
	
	s_FrameBuffer.Width = SCD.BufferDesc.Width;
	s_FrameBuffer.Height = SCD.BufferDesc.Height;
	s_FrameBuffer.SampleDesc = SCD.SampleDesc;
	s_FrameBuffer.ColorFormat = SCD.BufferDesc.Format;
	s_FrameBuffer.DepthFormat = cRenderDX_Format[cFormat::Depth24Stencil8];
	s_FrameBuffer.Create();
	
	cRenderDX_VERTEX_BUFFER VB;
	s_DynVertexBufferID = s_VertexBuffers.Add(VB);
	
	cRenderDX_INDEX_BUFFER IB;
	s_DynIndexBufferID = s_IndexBuffers.Add(IB);

	cRenderDX_RASTERIZER_STATE RS;
	RS.Create();
	ApplyRasterizerState(RS.Ptr);
	s_CurRasterizerStateID = s_RasterizerStates.Add(RS);

	cRenderDX_ShowLog();
	return true;
}

// ApplyShader
static void ApplyShader(const cRenderDX_SHADER *Sh) {
	if(nullptr == Sh) {
		s_Sys.DeviceContext->IASetInputLayout(nullptr);
		s_Sys.DeviceContext->VSSetShader(nullptr, 0, 0);
		s_Sys.DeviceContext->PSSetShader(nullptr, 0, 0);
	} else {
		s_Sys.DeviceContext->IASetInputLayout(Sh->InputLayout);
		s_Sys.DeviceContext->VSSetShader(Sh->VS, 0, 0);
		s_Sys.DeviceContext->PSSetShader(Sh->PS, 0, 0);
		static cList<ID3D11Buffer *> ConstBuffers;
		int i;
		if(!Sh->VSConstBuffers.IsEmpty()) {
			ConstBuffers.Clear();
			for(i = 0; i < Sh->VSConstBuffers.Count(); i++) {
				ConstBuffers.Add(Sh->VSConstBuffers[i].Ptr);
			}
			s_Sys.DeviceContext->VSSetConstantBuffers(0, ConstBuffers.Count(), ConstBuffers.ToPtr());
		}
		if(!Sh->PSConstBuffers.IsEmpty()) {
			ConstBuffers.Clear();
			for(i = 0; i < Sh->PSConstBuffers.Count(); i++) {
				ConstBuffers.Add(Sh->PSConstBuffers[i].Ptr);
			}
			s_Sys.DeviceContext->PSSetConstantBuffers(0, ConstBuffers.Count(), ConstBuffers.ToPtr());
		}
	}
}

// ApplyVertexBuffer
static void ApplyVertexBuffer(ID3D11Buffer *Buffer, UINT Stride) {
	UINT ZeroOffset = 0;
	s_Sys.DeviceContext->IASetVertexBuffers(0, 1, &Buffer, &Stride, &ZeroOffset);
}

// ApplyIndexBuffer
static void ApplyIndexBuffer(ID3D11Buffer *Buffer, int IndexSize) {
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
	if(4 == IndexSize) {
		Format = DXGI_FORMAT_R32_UINT;
	} else if(2 == IndexSize) {
		Format = DXGI_FORMAT_R16_UINT;
	}
	s_Sys.DeviceContext->IASetIndexBuffer(Buffer, Format, 0);
}

//-----------------------------------------------------------------------------
// cRenderDX::Free
//-----------------------------------------------------------------------------
void cRenderDX::Free() {
	int i;

	//*************************************************************************
	// Free vertex formats
	//*************************************************************************
	s_VertexFormats.Clear();
	
	//*************************************************************************
	// Free vertex buffers
	//*************************************************************************
	for(i = 0; i < s_VertexBuffers.Count(); i++) {
		if(s_CurVertexBufferID == i) {
			ApplyVertexBuffer(nullptr, 0);
		}
		cRenderDX_VERTEX_BUFFER &VB = s_VertexBuffers[i];
		VB.Free();
	}
	s_VertexBuffers.Clear();
	s_VertexBuffersFreeID.Clear();
	s_CurVertexBufferID = -1;
	s_CurVertexBufferStride = 0;
	s_DynVertexBufferID = -1;
	s_DynVertexBufferSize = 0;
	
	//*************************************************************************
	// Free index buffers
	//*************************************************************************
	for(i = 0; i < s_IndexBuffers.Count(); i++) {
		if(s_CurIndexBufferID == i) {
			ApplyIndexBuffer(nullptr, 0);
		}
		cRenderDX_INDEX_BUFFER &IB = s_IndexBuffers[i];
        IB.Free();
	}
	s_IndexBuffers.Clear();
	s_IndexBuffersFreeID.Clear();
	s_CurIndexBufferID = -1;
	s_CurIndexBufferStride = 0;
    s_DynIndexBufferID = -1;
	s_DynIndexBufferSize = 0;

	//*************************************************************************
	// Free shaders
	//*************************************************************************
	for(i = 0; i < s_Shaders.Count(); i++) {
		if(s_CurShaderID == i) {
			ApplyShader(nullptr);
		}
		cRenderDX_SHADER *S = s_Shaders[i];
		S->Free();
		cRenderDX_SHADER::Free(&S);
	}
	cAssert(0 == cRenderDX_SHADER::Count);
	s_Shaders.Clear();
	s_ShadersFreeID.Clear();
	s_CurShaderID = -1;

	//*************************************************************************
	// Free rasterizer states
	//*************************************************************************
	for(i = 0; i < s_RasterizerStates.Count(); i++) {
		if(s_CurRasterizerStateID == i) {
			ApplyRasterizerState(nullptr);
		}
		cRenderDX_RASTERIZER_STATE &RS = s_RasterizerStates[i];
		RS.Free();
	}
	s_RasterizerStates.Clear();
	s_CurRasterizerStateID = -1;


	// Free states
	// Free textures

	//************************************************************************
	// Free frame buffer
	//*************************************************************************
	SAFE_RELEASE(s_Sys.SwapChain);
	s_FrameBuffer.Free();

	//*************************************************************************
	// Free context and device
	//*************************************************************************
	SAFE_RELEASE(s_Sys.DeviceContext);
	if(s_Sys.Device != nullptr) {
		ULONG ResourcesLeft = s_Sys.Device->Release();
		cAssert(0 == ResourcesLeft);
		s_Sys.Device = nullptr;
	}

	s_Sys.Vendor.Clear();
} // cRenderDX::Free

// cRenderDX::SetViewport
void cRenderDX::SetViewport(const cRect &Viewport) {
}

// cRenderDX::SetClipRect
void cRenderDX::SetClipRect(const cRect &ClipRect) {
}

// cRenderDX::Clear
void cRenderDX::Clear(const bool ClearColor, const bool ClearDepth, const cColor &Color, const float Depth) {
	if(ClearColor) {
		s_Sys.DeviceContext->ClearRenderTargetView(s_FrameBuffer.ColorView, Color.ToFloatPtr());
	}
	if(ClearDepth) {
		s_Sys.DeviceContext->ClearDepthStencilView(s_FrameBuffer.DepthView, D3D11_CLEAR_DEPTH, Depth, 0);
	}
}

// cRenderDX::BeginFrame
void cRenderDX::BeginFrame() {
#ifdef COMMS_WINDOWS
	//*************************************************************************
	// Handle possible window client size change
	//*************************************************************************
	UINT Width = cMain_GetClientWidth();
	UINT Height = cMain_GetClientHeight();
	cAssert(Width > 0 && Height > 0);
	if(Width != s_FrameBuffer.Width || Height != s_FrameBuffer.Height) {
		s_FrameBuffer.Free();
		s_Sys.SwapChain->ResizeBuffers(1, Width, Height, s_FrameBuffer.ColorFormat, 0);
		s_FrameBuffer.Width = Width;
		s_FrameBuffer.Height = Height;
		s_FrameBuffer.Create();
	}
#endif // COMMS_WINDOWS
}

// cRenderDX::EndFrame
void cRenderDX::EndFrame() {
	s_Sys.SwapChain->Present(0, 0);
}

// cRenderDX::Finish
void cRenderDX::Finish() {
	s_Sys.Finish();
}

// cRenderDX::GetVendor
const cStr & cRenderDX::GetVendor() const {
	return s_Sys.Vendor;
}

// cRenderDX::GetMRTCount
int cRenderDX::GetMRTCount() {
	return s_Caps.SimultaneousRenderTargets;
}

// cRenderDX::SupportsFormat
bool cRenderDX::SupportsFormat(const cFormat::Enum Format) {
	return true;
}

// cRenderDX::SupportsNonPowerOfTwo
bool cRenderDX::SupportsNonPowerOfTwo() {
	return s_Caps.NonPowerOfTwo;
}

// cRenderDX::ScreenShot
bool cRenderDX::ScreenShot(cImage *To) {
	return false;
}

// cRenderDX::GetDeviceRecentReset
bool cRenderDX::GetDeviceRecentReset() {
	return false; // DirectX 11 doesn't lost device unlike DirectX 9
}

// cRenderDX::GetDepthStateID
int cRenderDX::GetDepthStateID(const bool TestEnabled, const bool WriteEnabled, const cDepthFunc::Enum Func) {
	return -1;
}

// cRenderDX::SetDepthState
void cRenderDX::SetDepthState(const int DepthStateID) {
}

// cRenderDX::SetCullMode
void cRenderDX::SetCullMode(const cCullMode::Enum CullMode) {
}

// cRenderDX::GetTextureID
int cRenderDX::GetTextureID(const char *FilePn) {
	return -1;
}

// cRenderDX::ReloadTextures
void cRenderDX::ReloadTextures() {
}

// cRenderDX::ReloadTexture
bool cRenderDX::ReloadTexture(const int TextureID) {
	return true;
}

// cRenderDX::ReloadTexture
void cRenderDX::ReloadTexture(const char *FilePn) {
}

// cRenderDX::UnloadTextures
void cRenderDX::UnloadTextures() {
}

// cRenderDX::AddTexture : (const char *)
int cRenderDX::AddTexture(const char *FilePn) {
	return -1;
}

// cRenderDX::AddTexture : (const cImage &)
int cRenderDX::AddTexture(const cImage &Image) {
	return -1;
}

// cRenderDX::UpdateTexture
void cRenderDX::UpdateTexture(const int TextureID, const cList<cImage *> &SubImages, const cList<cRect> &Rects) {
}

// cRenderDX::FreeTexture
void cRenderDX::FreeTexture(const int TextureID) {
}

// cRenderDX::GetTextureFilePn
const cStr * cRenderDX::GetTextureFilePn(const int TextureID) {
	return nullptr;
}

// cRenderDX::GetTextureWidth
int cRenderDX::GetTextureWidth(const int TextureID) {
	return -1;
}

// cRenderDX::GetTextureHeight
int cRenderDX::GetTextureHeight(const int TextureID) {
	return -1;
}

// cRenderDX::GetTextureDepth
int cRenderDX::GetTextureDepth(const int TextureID) {
	return -1;
}

// cRenderDX::GetTextureFormat
const cFormat::Enum cRenderDX::GetTextureFormat(const int TextureID) {
	return cFormat::fmtNone;
}

// cRenderDX::GetTextureMipMapCount
int cRenderDX::GetTextureMipMapCount(const int TextureID) {
	return -1;
}

// cRenderDX::GetTextureRenderTargetUsage
bool cRenderDX::GetTextureRenderTargetUsage(const int TextureID) {
	return false;
}

//-----------------------------------------------------------------------------------------------
// cRenderDX::GetShaderID
//-----------------------------------------------------------------------------------------------
int cRenderDX::GetShaderID(const char *ShaderName, const int VertexFormatID, const char *Extra, cStr* OptionalLog) {
	// Check shader name
	cAssert(ShaderName != nullptr);
	if(cStr::Length(ShaderName) < 1) {
		return -1; // No name
	}

	cAssert(VertexFormatID >= 0 && VertexFormatID < s_VertexFormats.Count());
	if(VertexFormatID < 0 || VertexFormatID >= s_VertexFormats.Count()) {
		return -1; // Invalid vertex format ID
	}

	// Searching loaded shaders
	int i;
	cStr E = Extra; // Since "cStr::Compare" asserts not NULLs
	for(i = 0; i < s_Shaders.Count(); i++) {
		const cRenderDX_SHADER *r = s_Shaders[i];
		if(cStr::EqualsNoCase(r->Name, ShaderName)) { // Same name
			if(r->VertexFormatID == VertexFormatID) { // Same vertex format
				if(cStr::Equals(r->Extra, E)) { // Same extra
					return i; // Such shader is already loaded
				}
			}
		}
	}

	return AddShader(ShaderName, VertexFormatID, Extra, OptionalLog);
} // cRenderDX::GetShaderID

//---------------------------------------------------------------------------------------------
// cRenderDX::AddShader
//---------------------------------------------------------------------------------------------
int cRenderDX::AddShader(const char *ShaderName, const int VertexFormatID, const char *Extra, cStr *OptionalLog) {
#ifdef COMMS_3DCOAT
	LogNewShader(ShaderName,VertexFormatID,Extra);
#endif // COMMS_3DCOAT

	cRenderDX_SHADER *Sh = cRenderDX_SHADER::Alloc();
	if(!CreateShader(ShaderName, VertexFormatID, Extra, Sh)) {
		Sh->Free();
		cRenderDX_SHADER::Free(&Sh);
		return -1;
	}
	
	// Add shader slot or use empty
	int ShaderID = -1;
	if(!s_ShadersFreeID.IsEmpty()) {
		ShaderID = s_ShadersFreeID.GetLast();
		s_ShadersFreeID.RemoveLast();
		s_Shaders[ShaderID] = Sh;
		return ShaderID;
	} else {
		return s_Shaders.Add(Sh);
	}
} // cRenderDX::AddShader

// cRenderDX::GetShaderConstID
int cRenderDX::GetShaderConstID(const int ShaderID, const char *ConstName) {
	cAssert(ConstName != nullptr);
	if(cStr::Length(ConstName) < 1) {
		return -1; // No name
	}

	cAssert(ShaderID >= 0 && ShaderID < s_Shaders.Count() && !s_ShadersFreeID.Contains(ShaderID));
	if(ShaderID < 0 || ShaderID >= s_Shaders.Count() || s_ShadersFreeID.Contains(ShaderID)) {
		return -1; // Invalid shader ID
	}

	const cRenderDX_SHADER *Sh = s_Shaders[ShaderID];
	cRenderDX_SHADER::Const C;
	C.Name = ConstName;
	int i = Sh->Consts.BinarySearch(C, cRenderDX_SHADER::Const::Compare);
	return i;
}

//-----------------------------------------------------------------------------
// cRenderDX::SetShader
//-----------------------------------------------------------------------------
void cRenderDX::SetShader(const int ShaderID) {
	if(ShaderID == s_CurShaderID) {
		return;
	}
	if(ShaderID < 0 || ShaderID >= s_Shaders.Count() || s_ShadersFreeID.Contains(ShaderID)) {
		ApplyShader(nullptr);
		s_CurShaderID = -1;
		return;
	}

	const cRenderDX_SHADER *S = s_Shaders[ShaderID];

	if(!S->IsValid()) {
		ApplyShader(nullptr);
		s_CurShaderID = -1;
		return;
	}
	
	ApplyShader(S);
	s_CurShaderID = ShaderID;
	
	SetShaderAutoConstants();
} // cRenderDX::SetShader

// cRenderDX::FreeShader
void cRenderDX::FreeShader(const int ShaderID) {
}

//-----------------------------------------------------------------------------
// cRenderGL::ReloadShaders
//-----------------------------------------------------------------------------
void cRenderDX::ReloadShaders() {
	int i;
	cStr S, E;
	int F;

	cLog::Message(cStr(60, '-'));
	SetShader(-1);
	
	int c = 0;
	for(i = 0; i < s_Shaders.Count(); i++) {
		cRenderDX_SHADER **Sh = &s_Shaders[i];
		if(s_ShadersFreeID.Contains(i)) {
			continue;
		}
		S = (*Sh)->Name;
		E = (*Sh)->Extra;
		F = (*Sh)->VertexFormatID;
		cRenderDX_SHADER *Rel = cRenderDX_SHADER::Alloc();
		if(!CreateShader(S.ToCharPtr(), F, E.ToCharPtr(), Rel)) {
			Rel->Free();
			cRenderDX_SHADER::Free(&Rel);
		} else {
			(*Sh)->Free();
			cRenderDX_SHADER::Free(Sh);
			*Sh = Rel;
			c++;
		}
	}
	cLog::Message(cStr(60, '-') + cStr::EndLn + "Reloaded " + cStr::ToString(c) + " Shaders.");
} // cRenderDX::ReloadShaders

// cRenderDX::AddVertexFormat
int cRenderDX::AddVertexFormat(const cVertex::Format &Format) {
	DXGI_FORMAT FloatTypes[4] = {
		DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32G32_FLOAT, DXGI_FORMAT_R32G32B32_FLOAT, DXGI_FORMAT_R32G32B32A32_FLOAT
	};
	cRenderDX_VERTEX_FORMAT vf;
	vf.Desc.SetCount(Format.Count());
	int TexCoordUsageIndex = 0;
	
	static const char *Name[] = { "POSITION", "TEXCOORD" };
	int i;
	for(i = 0; i < Format.Count(); i++) {
		const cVertex::Attrib &a = Format[i];
		cAssert(a.IsValid());
		if(!a.IsValid()) {
			return -1;
		}
		D3D11_INPUT_ELEMENT_DESC &D = vf.Desc[i];
		memset(&D, 0, sizeof(D));
		D.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		D.AlignedByteOffset = vf.VertexSize;
		if(a.Type == cVertexType::Float) {
			D.Format = FloatTypes[a.Dim - 1];
		} else {
			cAssert(a.Type == cVertexType::Byte);
			cAssert(a.Dim == 4);
			D.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		}
		if(cVertexUsage::Position == a.Usage) {
			D.SemanticName = Name[0];
			vf.StrDesc += D.SemanticName;
		} else {
			D.SemanticName = Name[1];
			D.SemanticIndex = TexCoordUsageIndex;
			vf.StrDesc += " " + cStr(D.SemanticName) + cStr::ToString(TexCoordUsageIndex);
			TexCoordUsageIndex++;
		}
		vf.VertexSize += a.Dim * cVertexType::SizeOf(a.Type);
	}
	return s_VertexFormats.Add(vf);
}

// cRenderDX::GetVertexSize
int cRenderDX::GetVertexSize(const int VertexFormatID) {
	if(VertexFormatID < 0 || VertexFormatID >= s_VertexFormats.Count()) {
		return 0;
	}
	const cRenderDX_VERTEX_FORMAT &vf = s_VertexFormats[VertexFormatID];
	return vf.VertexSize;
}

// cRenderDX::AddVertexBuffer
int cRenderDX::AddVertexBuffer(const void *Data, const size_t Size) {
	cRenderDX_VERTEX_BUFFER VB;
	VB.Size = Size;
	
	D3D11_BUFFER_DESC Desc;
	memset(&Desc, 0, sizeof(Desc));
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.ByteWidth = (UINT)Size;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	
	D3D11_SUBRESOURCE_DATA Res;
	memset(&Res, 0, sizeof(Res));
	Res.pSysMem = Data;
	
	HRESULT hr = s_Sys.Device->CreateBuffer(&Desc, &Res, &VB.Ptr);
	cAssertM(SUCCEEDED(hr), "Couldn't create static vertex buffer");
	if(FAILED(hr)) {
		return -1;
	}
	
	// Add vertex buffer slot or use empty
	int VertexBufferID = -1;
	if(!s_VertexBuffersFreeID.IsEmpty()) {
		VertexBufferID = s_VertexBuffersFreeID.GetLast();
		s_VertexBuffersFreeID.RemoveLast();
		s_VertexBuffers[VertexBufferID] = VB;
		return VertexBufferID;
	} else {
		return s_VertexBuffers.Add(VB);
	}
}

// cRenderDX::AddIndexBuffer
int cRenderDX::AddIndexBuffer(const void *Data, const int IndexCount, const int IndexSize) {
	cRenderDX_INDEX_BUFFER IB;
	IB.IndexCount = IndexCount;
	IB.IndexSize = IndexSize;
	UINT Size = IndexSize * IndexCount;
	
	D3D11_BUFFER_DESC Desc;
	memset(&Desc, 0, sizeof(Desc));
	Desc.Usage = D3D11_USAGE_IMMUTABLE;
	Desc.ByteWidth = Size;
	Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA Res;
	memset(&Res, 0, sizeof(Res));
	Res.pSysMem = Data;

	HRESULT hr = s_Sys.Device->CreateBuffer(&Desc, &Res, &IB.Ptr);
	cAssertM(SUCCEEDED(hr), "Couldn't create static index buffer");
	if(FAILED(hr)) {
		return -1;
	}
	
	// Add index buffer slot or use empty
	int IndexBufferID = -1;
	if(!s_IndexBuffersFreeID.IsEmpty()) {
		IndexBufferID = s_IndexBuffersFreeID.GetLast();
		s_IndexBuffersFreeID.RemoveLast();
		s_IndexBuffers[IndexBufferID] = IB;
		return IndexBufferID;
	}
    return s_IndexBuffers.Add(IB);
}

// cRenderDX::FreeVertexBuffer
void cRenderDX::FreeVertexBuffer(const int VertexBufferID) {
	if(VertexBufferID < 0 || VertexBufferID >= s_VertexBuffers.Count() || s_VertexBuffersFreeID.Contains(VertexBufferID)) {
		return; // Invalid vertex buffer ID
	}
	if(VertexBufferID == s_CurVertexBufferID) {
		SetVertexBuffer(-1);
	}
	cRenderDX_VERTEX_BUFFER &VB = s_VertexBuffers[VertexBufferID];
	VB.Free();
	s_VertexBuffersFreeID.Add(VertexBufferID);
}

// cRenderDX::FreeIndexBuffer
void cRenderDX::FreeIndexBuffer(const int IndexBufferID) {
	if(IndexBufferID < 0 || IndexBufferID >= s_IndexBuffers.Count() || s_IndexBuffersFreeID.Contains(IndexBufferID)) {
		return; // Invalid index buffer ID
	}
	if(IndexBufferID == s_CurIndexBufferID) {
		SetIndexBuffer(-1);
	}
	cRenderDX_INDEX_BUFFER &IB = s_IndexBuffers[IndexBufferID];
	IB.Free();
	s_IndexBuffersFreeID.Add(IndexBufferID);
}

// cRenderDX::SetVertexBuffer
void cRenderDX::SetVertexBuffer(const int VertexBufferID) {
	if(VertexBufferID < 0 || VertexBufferID >= s_VertexBuffers.Count() || s_VertexBuffersFreeID.Contains(VertexBufferID)) {
		ApplyVertexBuffer(nullptr, 0);
		s_CurVertexBufferID = -1;
		s_CurVertexBufferStride = 0;
		return;
	}
	cAssertM(s_CurShaderID != -1, "Set shader before setting vertex buffer");
	if(-1 == s_CurShaderID) {
        return;
	}
	const cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
	const cRenderDX_VERTEX_FORMAT &VF = s_VertexFormats[Sh->VertexFormatID];
	if((VertexBufferID == s_CurVertexBufferID) && (VF.VertexSize == s_CurVertexBufferStride)) {
		return;
	}
	const cRenderDX_VERTEX_BUFFER &VB = s_VertexBuffers[VertexBufferID];
	s_CurVertexBufferStride = s_VertexFormats[Sh->VertexFormatID].VertexSize;
	ApplyVertexBuffer(VB.Ptr, s_CurVertexBufferStride);
	s_CurVertexBufferID = VertexBufferID;
}

// cRenderDX::SetIndexBuffer
void cRenderDX::SetIndexBuffer(const int IndexBufferID) {
	if(IndexBufferID < 0 || IndexBufferID >= s_IndexBuffers.Count() || s_IndexBuffersFreeID.Contains(IndexBufferID)) {
		ApplyIndexBuffer(nullptr, 0);
		s_CurIndexBufferID = -1;
		s_CurIndexBufferStride = 0;
		return;
	}
    const cRenderDX_INDEX_BUFFER &IB = s_IndexBuffers[IndexBufferID];
	if((IndexBufferID == s_CurIndexBufferID) && (IB.IndexSize == s_CurIndexBufferStride)) {
		return;
	}
	s_CurIndexBufferStride = IB.IndexSize;
	ApplyIndexBuffer(IB.Ptr, s_CurIndexBufferStride);
    s_CurIndexBufferID = IndexBufferID;
}

//*************************************************************************
// Render target
//*************************************************************************

// cRenderDX::AddRenderTarget
int cRenderDX::AddRenderTarget(const int Width, const int Height, const cFormat::Enum Format, const bool CubeMap, const int Samples) {
	return -1;
}

// cRenderDX::AddRenderDepth
int cRenderDX::AddRenderDepth(const int RenderTargetID, const cFormat::Enum Format) {
	return -1;
}

// cRenderDX::SetRenderTargets
void cRenderDX::SetRenderTargets(const int *ColorRenderTargetIDs, const int ColorRenderTargetCount, const int DepthRenderTargetID, const int *Faces) {
}

// cRenderDX::SetRenderTargetSize
void cRenderDX::SetRenderTargetSize(const int RenderTargetID, const int Width, const int Height) {
}

bool cRenderDX::SaveRenderTargetRaw(const int RenderTargetID, cRender::SaveRenderTargetArgs *Args) {
    return false;
}
bool cRenderDX::RequestSaveRenderTargetAsyncRaw(const int RenderTargetID, cRender::SaveRenderTargetArgs *Args) {
    return false;
}
bool cRenderDX::GetSaveRenderTargetAsyncRaw(const int RenderTargetID, cRender::SaveRenderTargetArgs *Args) {
    return false;
}
void cRenderDX::FreeSaveRenderTargetAsyncRaw(const int RenderTargetID) {
}
bool cRenderDX::IsSaveRenderTargetAsyncReadyRaw(const int RenderTargetID) {
    return false;
}
// cRenderDX::SaveRenderTarget
bool cRenderDX::SaveRenderTarget(const int RenderTargetID, cImage *To) {
	return false;
}
	
//*************************************************************************
// SamplerState
//*************************************************************************

// cRenderDX::GetSamplerStateID
int cRenderDX::GetSamplerStateID(const cFilter::Enum Filter, const cAddressMode::Enum s, const cAddressMode::Enum t, const cAddressMode::Enum r) {
	return -1;
}

// cRenderDX::SetSamplerState
void cRenderDX::SetSamplerState(const int TextureID, const int SamplerStateID) {
}

//*************************************************************************
// Blend
//*************************************************************************

// cRenderDX::GetBlendStateID
int cRenderDX::GetBlendStateID(const cBlendFactor::Enum SrcFactor, const cBlendFactor::Enum DstFactor, const cBlendMode::Enum Mode, const dword Mask) {
	return -1;
}

// cRenderDX::SetBlendState
void cRenderDX::SetBlendState(const int BlendStateID) {
}

// cRenderDX::GetSamplerID
int cRenderDX::GetSamplerID(const int ShaderID, const char *SamplerName) const {
	return -1;
}

// cRenderDX::SetTexture
void cRenderDX::SetTexture(const int SamplerID, const int TextureID) {
}

// cRenderDX::SetShaderConst : (..., const int)
void cRenderDX::SetShaderConst(const int ConstID, const int Value) {
	if(s_CurShaderID != -1) {
		cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
		Sh->SetConst(ConstID, &Value, sizeof(Value));
	}
}

// cRenderDX::SetShaderConst : (..., const float)
void cRenderDX::SetShaderConst(const int ConstID, const float Value) {
	if(s_CurShaderID != -1) {
		cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
		Sh->SetConst(ConstID, &Value, sizeof(Value));
	}
}

// cRenderDX::SetShaderConst : (..., const cVec2 &)
void cRenderDX::SetShaderConst(const int ConstID, const cVec2 &Value) {
	if(s_CurShaderID != -1) {
		cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
		Sh->SetConst(ConstID, Value.ToFloatPtr(), sizeof(Value));
	}
}

// cRenderDX::SetShaderConst : (..., const cVec3 &)
void cRenderDX::SetShaderConst(const int ConstID, const cVec3 &Value) {
	if(s_CurShaderID != -1) {
		cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
		Sh->SetConst(ConstID, Value.ToFloatPtr(), sizeof(Value));
	}
}

// cRenderDX::SetShaderConst : (..., const cVec4 &)
void cRenderDX::SetShaderConst(const int ConstID, const cVec4 &Value) {
	if(s_CurShaderID != -1) {
		cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
		Sh->SetConst(ConstID, Value.ToFloatPtr(), sizeof(Value));
	}
}

// cRenderDX::SetShaderConst : (..., const cMat3 &)
void cRenderDX::SetShaderConst(const int ConstID, const cMat3 &Value) {
	if(s_CurShaderID != -1) {
		cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
		Sh->SetConst(ConstID, Value.ToFloatPtr(), sizeof(Value));
	}
}

// cRenderDX::SetShaderConst : (..., const cMat4 &)
void cRenderDX::SetShaderConst(const int ConstID, const cMat4 &Value) {
	if(s_CurShaderID != -1) {
		cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
		Sh->SetConst(ConstID, Value.ToFloatPtr(), sizeof(Value));
	}
}

// cRenderDX::SetShaderConst : (..., const float *, ...)
void cRenderDX::SetShaderConst(const int ConstID, const float *Array, const int Count) {
	if(s_CurShaderID != -1) {
		cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
		Sh->SetConst(ConstID, Array, sizeof(Array[0]) * Count);
	}
}

// cRenderDX::SetShaderConst : (..., const cVec2 *, ...)
void cRenderDX::SetShaderConst(const int ConstID, const cVec2 *Array, const int Count) {
	if(s_CurShaderID != -1) {
		cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
		Sh->SetConst(ConstID, Array, sizeof(Array[0]) * Count);
	}
}

// cRenderDX::SetShaderConst : (..., const cVec3 *, ...)
void cRenderDX::SetShaderConst(const int ConstID, const cVec3 *Array, const int Count) {
	if(s_CurShaderID != -1) {
		cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
		Sh->SetConst(ConstID, Array, sizeof(Array[0]) * Count);
	}
}

// cRenderDX::SetShaderConst : (..., const cVec4 *, ...)
void cRenderDX::SetShaderConst(const int ConstID, const cVec4 *Array, const int Count) {
	if(s_CurShaderID != -1) {
		cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
		Sh->SetConst(ConstID, Array, sizeof(Array[0]) * Count);
	}
}

// cRenderDX::SetShaderAutoConstants
void cRenderDX::SetShaderAutoConstants() {
	if(-1 == s_CurShaderID) {
		return; // There is no active shader
	}

	int i;
	cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
	float F;
	cVec4 u;
	cMat4 M;
	cMat3 R;
	
	for(i = 0; i < Sh->Consts.Count(); i++) {
		const cRenderDX_SHADER::Const &C = Sh->Consts[i];
		
		switch(C.AutoConst) {
			case cRender::AutoConst::ViewportWidth:
				F = cRender::GetViewer()->GetViewport().GetWidth();
				Sh->SetConst(i, &F, sizeof(F));
				break;
			case cRender::AutoConst::ViewportHeight:
				F = cRender::GetViewer()->GetViewport().GetHeight();
				Sh->SetConst(i, &F, sizeof(F));
				break;
			case cRender::AutoConst::ViewportInvWidth:
				F = 1.0f / cRender::GetViewer()->GetViewport().GetWidth();
				Sh->SetConst(i, &F, sizeof(F));
				break;
			case cRender::AutoConst::ViewportInvHeight:
				F = 1.0f / cRender::GetViewer()->GetViewport().GetHeight();
				Sh->SetConst(i, &F, sizeof(F));
				break;
			case cRender::AutoConst::WorldMatrix:
				M = cRender::GetWorldMatrix();
				Sh->SetConst(i, &M, sizeof(M));
				break;
			case cRender::AutoConst::WorldMatrixInverse:
				M = cRender::GetWorldMatrixInverse();
				Sh->SetConst(i, &M, sizeof(M));
				break;
			case cRender::AutoConst::ViewerPos:
				if(cRender::GetViewer()->GetOrthoProj()) {
					u.Set(cRender::GetViewer()->GetForward() * (-1e8f), 1.0f);
				} else {
					u.Set(cRender::GetViewer()->GetPos(), 1.0f);
				}
				Sh->SetConst(i, &u, sizeof(u));
				break;
			case cRender::AutoConst::WorldViewMatrix:
				M = cMat4::Mul(cRender::GetWorldMatrix(), cRender::GetViewer()->GetViewMatrix());
				Sh->SetConst(i, &M, sizeof(M));
				break;
			case cRender::AutoConst::ViewProjectionMatrix:
				M = cRender::GetViewer()->GetViewProjectionMatrix();
				Sh->SetConst(i, &M, sizeof(M));
				break;
			case cRender::AutoConst::WorldViewProjectionMatrix:
				M = cMat4::Mul(cRender::GetWorldMatrix(), cRender::GetViewer()->GetViewProjectionMatrix());
				Sh->SetConst(i, &M, sizeof(M));
				break;
			case cRender::AutoConst::ProjectionMatrix:
				M = cRender::GetViewer()->GetProjectionMatrix();
				Sh->SetConst(i, &M, sizeof(M));
				break;
			case cRender::AutoConst::ScreenMatrix:
				M = cRender::GetViewer()->GetScreenMatrix();
				Sh->SetConst(i, &M, sizeof(M));
				break;
			case cRender::AutoConst::ScreenMatrixInverse:
				M = cRender::GetViewer()->GetScreenMatrixInverse();
				Sh->SetConst(i, &M, sizeof(M));
				break;
			case cRender::AutoConst::NormalMatrix:
				R = cRender::GetNormalMatrix();
				Sh->SetConst(i, &R, sizeof(R));
				break;
			case cRender::AutoConst::NormalViewMatrix:
				R = cMat3::Mul(cRender::GetNormalMatrix(), cRender::GetViewer()->GetViewMatrix().ToMat3());
				Sh->SetConst(i, &R, sizeof(R));
				break;
			case cRender::AutoConst::TimeSec:
				F = cTimer::GetTimeSec();
				Sh->SetConst(i, &F, sizeof(F));
				break;
			case cRender::AutoConst::FrameTimeSec:
				F = cTimer::GetFrameTimeSec();
				Sh->SetConst(i, &F, sizeof(F));
				break;
			case cRender::AutoConst::TextureMatrix0:
				M = cRender::GetTextureMatrix(0);
				Sh->SetConst(i, &M, sizeof(M));
				break;
			case cRender::AutoConst::TextureMatrix1:
				M = cRender::GetTextureMatrix(1);
				Sh->SetConst(i, &M, sizeof(M));
				break;
			case cRender::AutoConst::TextureMatrix2:
				M = cRender::GetTextureMatrix(2);
				Sh->SetConst(i, &M, sizeof(M));
				break;
			case cRender::AutoConst::TextureMatrix3:
				M = cRender::GetTextureMatrix(3);
				Sh->SetConst(i, &M, sizeof(M));
				break;
			default:
				break;
		}
	}
} // cRenderDX::SetShaderAutoConstants

//*****************************************************************************
// cRenderDX_TOPOLOGY
//*****************************************************************************
static const D3D11_PRIMITIVE_TOPOLOGY cRenderDX_TOPOLOGY[cTopology::Count] = {
	D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,		// cTopology::PointList
	D3D11_PRIMITIVE_TOPOLOGY_LINELIST,		// cTopology::LineList
	D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP,		// cTopology::LineStrip
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,	// cTopology::TriangleList
	D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP	// cTopology::TriangleStrip
}; // cRenderDX_TOPOLOGY

// cRenderDX::DrawArrays : static
void cRenderDX::DrawArrays(const cTopology::Enum Topology) {
	cAssertM(s_CurShaderID != -1, "Set shader before drawing");
	if(-1 == s_CurShaderID) {
        return;
	}
	cAssertM(s_CurVertexBufferID != -1, "Set vertex buffer before drawing");
	if(-1 == s_CurVertexBufferID) {
		return;
	}

	cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
	Sh->ApplyConstants();
	const cRenderDX_VERTEX_FORMAT &VF = s_VertexFormats[Sh->VertexFormatID];
	cAssertM(VF.VertexSize == s_CurVertexBufferStride, "Set vertex buffer after setting shader with appropriate vertex format");
    const cRenderDX_VERTEX_BUFFER &VB = s_VertexBuffers[s_CurVertexBufferID];
	int VertexCount = (int)(VB.Size / VF.VertexSize);
	s_Sys.DeviceContext->IASetPrimitiveTopology(cRenderDX_TOPOLOGY[(int)Topology]);
	s_Sys.DeviceContext->Draw(VertexCount, 0);
}

// cRenderDX::SetDynVertexBuffer
void cRenderDX::SetDynVertexBuffer(const void *Data, const size_t Size) {
	cRenderDX_VERTEX_BUFFER &VB = s_VertexBuffers[s_DynVertexBufferID];
	if(s_DynVertexBufferSize < Size) {
		if(s_DynVertexBufferID == s_CurVertexBufferID) {
			SetVertexBuffer(-1);
		}
		VB.Free();
	}
	if(nullptr == VB.Ptr) {
		D3D11_BUFFER_DESC Desc;
		memset(&Desc, 0, sizeof(Desc));
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		s_DynVertexBufferSize = Size;
		Desc.ByteWidth = (UINT)s_DynVertexBufferSize;
		Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		HRESULT hr = s_Sys.Device->CreateBuffer(&Desc, nullptr, &VB.Ptr);
		cAssertM(SUCCEEDED(hr), "Couldn't create dynamic vertex buffer");
	}
	VB.Size = Size;
	cAssert(VB.Size <= s_DynVertexBufferSize);
	D3D11_MAPPED_SUBRESOURCE MS;
	HRESULT hr = s_Sys.DeviceContext->Map(VB.Ptr, nullptr, D3D11_MAP_WRITE_DISCARD, 0, &MS);
	cAssertM(SUCCEEDED(hr), "Couldn't map dynamic vertex buffer");
	memcpy(MS.pData, Data, Size);
	s_Sys.DeviceContext->Unmap(VB.Ptr, 0);
	SetVertexBuffer(s_DynVertexBufferID);
}

// cRenderDX::DrawArrays : dynamic
void cRenderDX::DrawArrays(const cTopology::Enum Topology, const void *VertexData, const int VertexCount) {
	cAssertM(s_CurShaderID != -1, "Set shader before drawing");
	if(-1 == s_CurShaderID) {
		return;
	}
    const cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
    const cRenderDX_VERTEX_FORMAT &VF = s_VertexFormats[Sh->VertexFormatID];
    size_t Size = VF.VertexSize * VertexCount;
    SetDynVertexBuffer(VertexData, Size);
	DrawArrays(Topology);
}

// cRenderDX::DrawIndexed : static
void cRenderDX::DrawIndexed(const cTopology::Enum Topology) {
	cAssertM(s_CurShaderID != -1, "Set shader before drawing");
	if(-1 == s_CurShaderID) {
		return;
	}
	cAssertM(s_CurVertexBufferID != -1, "Set vertex buffer before drawing");
	if(-1 == s_CurVertexBufferID) {
		return;
	}
	cAssertM(s_CurIndexBufferID != -1, "Set index buffer before drawing");
	if(-1 == s_CurIndexBufferID) {
		return;
	}

	cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
	Sh->ApplyConstants();
	const cRenderDX_VERTEX_FORMAT &VF = s_VertexFormats[Sh->VertexFormatID];
	cAssertM(VF.VertexSize == s_CurVertexBufferStride, "Set vertex buffer after setting shader with appropriate vertex format");

	const cRenderDX_INDEX_BUFFER &IB = s_IndexBuffers[s_CurIndexBufferID];
	s_Sys.DeviceContext->IASetPrimitiveTopology(cRenderDX_TOPOLOGY[(int)Topology]);
	s_Sys.DeviceContext->DrawIndexed(IB.IndexCount, 0, 0);
}

// cRenderDX::SetDynIndexBuffer
void cRenderDX::SetDynIndexBuffer(const void *Data, const int IndexCount, const int IndexSize) {
	cRenderDX_INDEX_BUFFER &IB = s_IndexBuffers[s_DynIndexBufferID];
	UINT Size = IndexSize * IndexCount;
	if(s_DynIndexBufferSize < Size) {
		if(s_DynIndexBufferID == s_CurIndexBufferID) {
			SetIndexBuffer(-1);
		}
		IB.Free();
	}
	if(nullptr == IB.Ptr) {
		D3D11_BUFFER_DESC Desc;
		memset(&Desc, 0, sizeof(Desc));
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		s_DynIndexBufferSize = Size;
		Desc.ByteWidth = (UINT)s_DynIndexBufferSize;
		Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		HRESULT hr = s_Sys.Device->CreateBuffer(&Desc, nullptr, &IB.Ptr);
		cAssertM(SUCCEEDED(hr), "Couldn't create dynamic index buffer");
	}
	IB.IndexCount = IndexCount;
	IB.IndexSize = IndexSize;
	cAssert((size_t)(IB.IndexCount * IB.IndexSize) <= s_DynIndexBufferSize);
	D3D11_MAPPED_SUBRESOURCE MS;
	HRESULT hr = s_Sys.DeviceContext->Map(IB.Ptr, nullptr, D3D11_MAP_WRITE_DISCARD, 0, &MS);
	cAssertM(SUCCEEDED(hr), "Couldn't map dynamic index buffer");
	memcpy(MS.pData, Data, Size);
	s_Sys.DeviceContext->Unmap(IB.Ptr, 0);
	SetIndexBuffer(s_DynIndexBufferID);
}

// cRenderDX::DrawIndexed : dynamic
void cRenderDX::DrawIndexed(const cTopology::Enum Topology, const void *VertexData, const int VertexCount, const void *IndexData, const int IndexCount, const int IndexSize) {
	cAssertM(s_CurShaderID != -1, "Set shader before drawing");
	if(-1 == s_CurShaderID) {
		return;
	}
    const cRenderDX_SHADER *Sh = s_Shaders[s_CurShaderID];
    const cRenderDX_VERTEX_FORMAT &VF = s_VertexFormats[Sh->VertexFormatID];
    size_t Size = VF.VertexSize * VertexCount;
	SetDynVertexBuffer(VertexData, Size);
	SetDynIndexBuffer(IndexData, IndexCount, IndexSize);
	DrawIndexed(Topology);
}

// cRenderDX::SetWireframe
void cRenderDX::SetWireframe(const bool Enabled) {
}

#else // DirectX 9

#include "../Libs/DirectX9_SDK/d3d9.h"
#include "../Libs/DirectX9_SDK/d3dx9.h"
#pragma comment (lib, "Libs/DirectX9_SDK/d3d9.lib")
#pragma comment (lib, "Libs/DirectX9_SDK/d3dx9.lib")

D3DCAPS9 s_Caps;
LPDIRECT3D9 s_D3D = nullptr;
LPDIRECT3DDEVICE9 s_Device = nullptr;
D3DPRESENT_PARAMETERS s_PP;
LPDIRECT3DBASETEXTURE9 s_SampledTextures[16];

//*****************************************************************************
// cRenderDX_VERTEX_FORMAT
//*****************************************************************************
struct cRenderDX_VERTEX_FORMAT {
	LPDIRECT3DVERTEXDECLARATION9 Decl;
	int VertexSize;

	cRenderDX_VERTEX_FORMAT() {
		Decl = nullptr;
		VertexSize = 0;
	}
}; // cRenderDX_VERTEX_FORMAT

static cList<cRenderDX_VERTEX_FORMAT> s_VertexFormats;
static int s_CurVertexFormatID = -1;

//*****************************************************************************
// cRenderDX_VERTEX_BUFFER
//*****************************************************************************
struct cRenderDX_VERTEX_BUFFER {
	LPDIRECT3DVERTEXBUFFER9 Ptr;
	size_t Size;

	cRenderDX_VERTEX_BUFFER() {
		Ptr = nullptr;
		Size = 0;
	}
}; // cRenderDX_VERTEX_BUFFER

static cList<cRenderDX_VERTEX_BUFFER> s_VertexBuffers;
static cList<int> s_VertexBuffersFreeID;
static int s_CurVertexBufferID = -1;

//*****************************************************************************
// cRenderDX_INDEX_BUFFER
//*****************************************************************************
struct cRenderDX_INDEX_BUFFER {
	LPDIRECT3DINDEXBUFFER9 Ptr;
	int IndexCount;

	cRenderDX_INDEX_BUFFER() {
		Ptr = nullptr;
		IndexCount = 0;
	}
}; // cRenderDX_INDEX_BUFFER

static cList<cRenderDX_INDEX_BUFFER> s_IndexBuffers;
static cList<int> s_IndexBuffersFreeID;
static int s_CurIndexBufferID = -1;

//*****************************************************************************
// cRenderDX_SHADER
//*****************************************************************************
struct cRenderDX_SHADER {
	cStr Name;
	int VertexFormatID;
	cStr Extra;
	cStr FilePn;

	LPDIRECT3DVERTEXSHADER9 VS;
	LPDIRECT3DPIXELSHADER9 PS;
	ID3DXConstantTable *VSConstants;
	ID3DXConstantTable *PSConstants;

	struct Sampler {
		cStr Name;
		int ImageUnit;
	};
	cList<Sampler> Samplers;

	struct Const {
		cStr Name;
		int VSReg;
		int PSReg;
		D3DXHANDLE VSHandle;
		D3DXHANDLE PSHandle;
		
		Const() {
			VSReg = -1;
			PSReg = -1;
			VSHandle = nullptr;
			PSHandle = nullptr;
		}
	};
	cList<Const> Consts;

	struct AutoConst {
		cStr Name;
		int Index;
		int VSReg;
		int PSReg;
		D3DXHANDLE VSHandle;
		D3DXHANDLE PSHandle;

		AutoConst() {
			Index = -1;
			VSReg = -1;
			PSReg = -1;
			VSHandle = nullptr;
			PSHandle = nullptr;
		}
	};
	cList<AutoConst> AutoConsts;

	void Clear() {
		Name.Clear();
		VertexFormatID = -1;
		Extra.Clear();
		FilePn.Clear();

		VS = nullptr;
		PS = nullptr;
		VSConstants = nullptr;
		PSConstants = nullptr;

		Samplers.Clear();
		Consts.Clear();
		AutoConsts.Clear();
	}

	void Free() {
		if(VS != nullptr) {
			VS->Release();
			VS = nullptr;
		}
		if(PS != nullptr) {
			PS->Release();
			PS = nullptr;
		}
		if(VSConstants != nullptr) {
			VSConstants->Release();
			VSConstants = nullptr;
		}
		if(PSConstants != nullptr) {
			PSConstants->Release();
			PSConstants = nullptr;
		}
	}

	bool IsValid() const {
		return VS != nullptr && PS != nullptr;
	}

	cRenderDX_SHADER() {
		Clear();
	}
}; // cRenderDX_SHADER

static cList<cRenderDX_SHADER> s_Shaders;
static cList<int> s_ShadersFreeID;
static int s_CurShaderID = -1;

//*****************************************************************************
// cRenderDX_DEPTH_STATE
//*****************************************************************************
struct cRenderDX_DEPTH_STATE {
	bool TestEnabled;
	bool WriteEnabled;
	cDepthFunc::Enum Func;
}; // cRenderDX_DEPTH_STATE

static cList<cRenderDX_DEPTH_STATE> s_DepthStates;

//*****************************************************************************
// cRenderDX_BLEND_STATE
//*****************************************************************************
struct cRenderDX_BLEND_STATE {
	cBlendFactor::Enum SrcFactor;
	cBlendFactor::Enum DstFactor;
	cBlendMode::Enum Mode;
	dword Mask;
	bool EnableBlend;
}; // cRenderDX_BLEND_STATE

static cList<cRenderDX_BLEND_STATE> s_BlendStates;

//*****************************************************************************
// cRenderDX_SAMPLER_STATE
//*****************************************************************************
struct cRenderDX_SAMPLER_STATE {
	DWORD MinFilter;
	DWORD MagFilter;
	DWORD MipFilter;
	DWORD S;
	DWORD T;
	DWORD R;
	bool Aniso;
	bool MipMaps;

	static bool Equals(const cRenderDX_SAMPLER_STATE &l, const cRenderDX_SAMPLER_STATE &r) {
		return l.MinFilter == r.MinFilter && l.MagFilter == r.MagFilter && l.MipFilter == r.MipFilter &&
			l.S == r.S && l.T == r.T && l.R == r.R;
	}
}; // cRenderDX_SAMPLER_STATE

static cList<cRenderDX_SAMPLER_STATE> s_SamplerStates;

//*****************************************************************************
// cRenderDX_TEXTURE
//*****************************************************************************
struct cRenderDX_TEXTURE {
	cStr FilePn;
	cFormat::Enum Format;
	int Width, Height, Depth, MipMapCount, SamplerStateID;
	bool CubeMap;
	bool RenderTarget;
	int Samples;
#ifdef COMMS_XBOX360
	cList<D3DRECT> Tiles;
	int TileWidth, TileHeight;
	DWORD DepthBase;
#endif // COMMS_XBOX360

	LPDIRECT3DBASETEXTURE9 Ptr;
	LPDIRECT3DSURFACE9 *Surfaces;

	void Clear() {
		FilePn.Clear();
		Format = cFormat::fmtNone;
		
		Width = 0;
		Height = 0;
		Depth = 0;
		MipMapCount = 1;
		SamplerStateID = -1;
		
		CubeMap = false;
		RenderTarget = false;
		Samples = 1;
#ifdef COMMS_XBOX360
		Tiles.Clear();
		TileWidth = 0;
		TileHeight = 0;
		DepthBase = 0;
#endif // COMMS_XBOX360
		
		Ptr = nullptr;
		Surfaces = nullptr;
	}

	void Free() {
		int c, i, S;
		HRESULT hr;
		if(Surfaces != nullptr) {
			c = CubeMap ? 6 : 1;
			for(i = 0; i < c; i++) {
				Surfaces[i]->Release();
			}
			delete Surfaces;
			Surfaces = nullptr;
		}
		if(Ptr != nullptr) {
			S = sizeof(s_SampledTextures) / sizeof(s_SampledTextures[0]);
			for(i = 0; i < S; i++) {
				if(s_SampledTextures[i] == Ptr) {
					hr = s_Device->SetTexture(i, nullptr);
					cAssert(SUCCEEDED(hr));
					s_SampledTextures[i] = nullptr;
				}
			}
			Ptr->Release();
			Ptr = nullptr;
		}
	}

	cRenderDX_TEXTURE() {
		Clear();
	}
}; // cRenderDX_TEXTURE

static cList<cRenderDX_TEXTURE> s_Textures;
static cList<int> s_TexturesFreeID;

static LPDIRECT3DSURFACE9 s_FrameBufferColor = nullptr;
static LPDIRECT3DSURFACE9 s_FrameBufferDepth = nullptr;

static cStr s_Vendor;
#ifdef COMMS_WINDOWS
static UINT s_ClientWidth = 0, s_ClientHeight = 0;
#endif // COMMS_WINDOWS
static bool s_RecentReset = false;
static bool s_VSync = false;

//-----------------------------------------------------------------------------
// cRenderDX::Init
//-----------------------------------------------------------------------------
bool cRenderDX::Init(const int MaxSamples) {
	s_D3D = Direct3DCreate9(D3D_SDK_VERSION);
	if(nullptr == s_D3D) {
		cLog::Error("Couldn't init Direct3D. You should have DirectX 9.0c or later installed.");
		return false;
	}

	// Vendor
	D3DADAPTER_IDENTIFIER9 Adapter;
	HRESULT hr = s_D3D->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &Adapter);
	cAssert(SUCCEEDED(hr));

	s_Vendor = Adapter.Description;
	cStr Driver = Adapter.Driver;
	
	//*************************************************************************
	// Caps
	//*************************************************************************
	s_D3D->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &s_Caps);
	
	cStr Supported, Unsupported;
	cStr *AppendTo = nullptr;
	
	int Major, Minor;

	// Vertex shaders
	if(s_Caps.VertexShaderVersion >= D3DVS_VERSION(1, 1)) {
		Major = (s_Caps.VertexShaderVersion >> 8) & 0xff;
		Minor = s_Caps.VertexShaderVersion & 0xff;
		Supported << "Vertex shaders " << Major << "." << Minor << cStr::EndLn;
	}
	
	// Pixel shaders
	if(s_Caps.PixelShaderVersion >= D3DPS_VERSION(2, 0)) {
		Major = (s_Caps.PixelShaderVersion >> 8) & 0xff;
		Minor = s_Caps.PixelShaderVersion & 0xff;
		Supported << "Pixel shaders " << Major << "." << Minor << cStr::EndLn;
	}
	
	// MRT
	if(s_Caps.NumSimultaneousRTs >= 2) {
		Supported << "MRT (targets: " << (int)s_Caps.NumSimultaneousRTs << ")" << cStr::EndLn;
	}

	// Non power of two
	if(SupportsNonPowerOfTwo()) {
		Supported << "Non power of two" << cStr::EndLn;
	}
	
	cStr Log;
	Log << cStr(60, '-') << cStr::EndLn;
	Log << "               Graphics card and driver info" << cStr::EndLn;
	Log << cStr(60, '-') << cStr::EndLn;
	Log << "Direct3D Renderer" << cStr::EndLn;
	Log << cStr::EndLn << "Adapter: " << s_Vendor << cStr::EndLn << "Driver: " << Driver << cStr::EndLn;
	if(!Supported.IsEmpty()) {
		Log << cStr::EndLn << "Supported:" << cStr::EndLn << Supported;
	}
	if(!Unsupported.IsEmpty()) {
		Log << cStr::EndLn << "Unsupported:" << cStr::EndLn << Unsupported;
	}
	Log << cStr(60, '-');
	if(Unsupported.IsEmpty()) {
		cLog::Message(Log);
	} else {
		cLog::Error(Log);
		s_D3D->Release();
		s_D3D = nullptr;
		return false;
	}
		
	//*************************************************************************
	// Init
	//*************************************************************************
#ifdef COMMS_WINDOWS
	RECT rc;
	GetClientRect(cWinMain_GetWindow(), &rc);
	s_ClientWidth = rc.right - rc.left;
	s_ClientHeight = rc.bottom - rc.top;
#endif // COMMS_WINDOWS
	
	memset(&s_PP, 0, sizeof(s_PP));
	s_PP.BackBufferFormat = D3DFMT_X8R8G8B8;
	s_PP.PresentationInterval = s_VSync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
	s_PP.BackBufferCount = 1;
	s_PP.SwapEffect = D3DSWAPEFFECT_DISCARD;
	s_PP.EnableAutoDepthStencil = true;
	s_PP.AutoDepthStencilFormat = D3DFMT_D24S8;
#ifdef COMMS_WINDOWS
	s_PP.BackBufferWidth = s_ClientWidth;
	s_PP.BackBufferHeight = s_ClientHeight;
	s_PP.Windowed = true;
	s_PP.hDeviceWindow = cWinMain_GetWindow();
#endif // COMMS_WINDOWS
#ifdef COMMS_XBOX360
	s_PP.BackBufferWidth = cXbox360Main_Width;
	s_PP.BackBufferHeight = cXbox360Main_Height;
#endif // COMMS_XBOX360
	
#ifdef COMMS_WINDOWS
	// Check for "PerfHUD"
	UINT AdapterToUse = 0;
	D3DDEVTYPE DeviceType = D3DDEVTYPE_HAL;
	for(UINT Adapter = 0; Adapter < s_D3D->GetAdapterCount(); Adapter++) {
		D3DADAPTER_IDENTIFIER9 Identifier;
		HRESULT Res;
		Res = s_D3D->GetAdapterIdentifier(Adapter, 0, &Identifier);
		if(strstr(Identifier.Description, "PerfHUD") != 0) {
			AdapterToUse = Adapter;
			DeviceType = D3DDEVTYPE_REF;
			break;
		}
	}
	int S = (-1 == MaxSamples) ? D3DMULTISAMPLE_16_SAMPLES : MaxSamples;
	while(true) {
		s_PP.MultiSampleType = (D3DMULTISAMPLE_TYPE)S;
		if(s_D3D->CreateDevice(AdapterToUse, DeviceType, cWinMain_GetWindow(), D3DCREATE_HARDWARE_VERTEXPROCESSING, &s_PP, &s_Device) == D3D_OK) {
			break;
		} else if(s_D3D->CreateDevice(AdapterToUse, DeviceType, cWinMain_GetWindow(), D3DCREATE_SOFTWARE_VERTEXPROCESSING, &s_PP, &s_Device) == D3D_OK) {
			cLog::Message("Hardware vertex processing failed, software vertex processing used.");
			break;
		} else if(S > 0) {
			S -= 2;
		} else {
			cLog::Error("Couldn't create Direct3D device interface.");
			return false;
		}
	}
#endif // COMMS_WINDOWS

#ifdef COMMS_XBOX360
	s_D3D->CreateDevice(0, D3DDEVTYPE_HAL, nullptr, D3DCREATE_HARDWARE_VERTEXPROCESSING, &s_PP, &s_Device);
#endif // COMMS_XBOX360
	
	s_Device->GetRenderTarget(0, &s_FrameBufferColor);
	s_Device->GetDepthStencilSurface(&s_FrameBufferDepth);
	memset(s_SampledTextures, 0, sizeof(s_SampledTextures));

	return true;
} // cRenderDX::Init

// FreeFrameBufferSurfaces
static void FreeFrameBufferSurfaces() {
	if(s_FrameBufferColor != nullptr) {
		s_FrameBufferColor->Release();
		s_FrameBufferColor = nullptr;
	}
	if(s_FrameBufferDepth != nullptr) {
		s_FrameBufferDepth->Release();
		s_FrameBufferDepth = nullptr;
	}
}

//-----------------------------------------------------------------------------
// cRenderDX::Free
//-----------------------------------------------------------------------------
void cRenderDX::Free() {
	int i;

	//*************************************************************************
	// Free vertex formats
	//*************************************************************************
	for(i = 0; i < s_VertexFormats.Count(); i++) {
		cRenderDX_VERTEX_FORMAT &vf = s_VertexFormats[i];
		if(vf.Decl != nullptr) {
			if(s_CurVertexFormatID == i) {
				s_Device->SetVertexDeclaration(nullptr);
			}
			vf.Decl->Release();
			vf.Decl = nullptr;
		}
	}
	s_VertexFormats.Clear();
	s_CurVertexFormatID = -1;

	//*************************************************************************
	// Free vertex buffers
	//*************************************************************************
	for(i = 0; i < s_VertexBuffers.Count(); i++) {
		cRenderDX_VERTEX_BUFFER &vb = s_VertexBuffers[i];
		if(vb.Ptr != nullptr) {
			vb.Ptr->Release();
			vb.Ptr = nullptr;
		}
	}
	s_VertexBuffers.Clear();
	s_VertexBuffersFreeID.Clear();
	s_CurVertexBufferID = -1;

	//*************************************************************************
	// Free index buffers
	//*************************************************************************
	for(i = 0; i < s_IndexBuffers.Count(); i++) {
		cRenderDX_INDEX_BUFFER &ib = s_IndexBuffers[i];
		if(ib.Ptr != nullptr) {
			ib.Ptr->Release();
			ib.Ptr = nullptr;
		}
	}
	s_IndexBuffers.Clear();
	s_IndexBuffersFreeID.Clear();
	s_CurIndexBufferID = -1;

	//*************************************************************************
	// Free shaders
	//*************************************************************************
	for(i = 0; i < s_Shaders.Count(); i++) {
		if(s_CurShaderID == i) {
			s_Device->SetVertexShader(nullptr);
			s_Device->SetPixelShader(nullptr);
		}
		
		cRenderDX_SHADER &S = s_Shaders[i];
		S.Free();
	}
	s_Shaders.Clear();
	s_ShadersFreeID.Clear();
	s_CurShaderID = -1;

	//*************************************************************************
	// Free states
	//*************************************************************************
	s_DepthStates.Clear();
	s_BlendStates.Clear();
	s_SamplerStates.Clear();

	//*************************************************************************
	// Free textures
	//*************************************************************************
	for(i = 0; i < s_Textures.Count(); i++) {
		cRenderDX_TEXTURE &T = s_Textures[i];
		T.Free();
	}
	s_Textures.Clear();
	s_TexturesFreeID.Clear();

	// Free frame buffer
	FreeFrameBufferSurfaces();
	
	if(s_Device != nullptr) {
		s_Device->Release();
		s_Device = nullptr;
	}
	if(s_D3D != nullptr) {
		s_D3D->Release();
		s_D3D = nullptr;
	}
	
	s_Vendor.Clear();
	s_RecentReset = false;
} // cRenderDX::Free

//-----------------------------------------------------------------------------
// cRenderDX::AddVertexFormat
//-----------------------------------------------------------------------------
int cRenderDX::AddVertexFormat(const cVertex::Format &Format) {
	int i;
	
	cRenderDX_VERTEX_FORMAT vf;
	cList<D3DVERTEXELEMENT9> Elem;
	Elem.SetCount(Format.Count() + 1);
	int TexCoordUsageIndex = 0;

#ifdef COMMS_WINDOWS
	BYTE FloatTypes[4];
#endif // COMMS_WINDOWS
#ifdef COMMS_XBOX360
	DWORD FloatTypes[4];
#endif // COMMS_XBOX360
	FloatTypes[0] = D3DDECLTYPE_FLOAT1;
	FloatTypes[1] = D3DDECLTYPE_FLOAT2;
	FloatTypes[2] = D3DDECLTYPE_FLOAT3;
	FloatTypes[3] = D3DDECLTYPE_FLOAT4;
	
	for(i = 0; i < Format.Count(); i++) {
		const cVertex::Attrib &a = Format[i];
		cAssert(a.IsValid());
		if(!a.IsValid()) {
			return -1;
		}
		
		D3DVERTEXELEMENT9 &E = Elem[i];

		E.Stream = 0;
		E.Offset = vf.VertexSize;
		if(a.Type == cVertexType::Float) {
			E.Type = FloatTypes[a.Dim - 1];
		} else {
			cAssert(a.Type == cVertexType::Byte);
			cAssert(a.Dim == 4);
			E.Type = D3DDECLTYPE_UBYTE4N;
		}
		E.Method = D3DDECLMETHOD_DEFAULT;
		if(cVertexUsage::Position == a.Usage) {
			E.Usage = D3DDECLUSAGE_POSITION;
			E.UsageIndex = 0;
		} else {
			E.Usage = D3DDECLUSAGE_TEXCOORD;
			E.UsageIndex = TexCoordUsageIndex;
			TexCoordUsageIndex++;
		}
		vf.VertexSize += a.Dim * cVertexType::SizeOf(a.Type);
	}

	memset(&Elem.GetLast(), 0, sizeof(D3DVERTEXELEMENT9));
	Elem.GetLast().Stream = 0xff;
	Elem.GetLast().Type = D3DDECLTYPE_UNUSED;
	
	HRESULT hr = s_Device->CreateVertexDeclaration(Elem.ToPtr(), &vf.Decl);
	cAssert(SUCCEEDED(hr));
	if(!SUCCEEDED(hr)) {
		return -1;
	}
	
	return s_VertexFormats.Add(vf);
} // cRenderDX::AddVertexFormat

// cRenderDX::GetVertexSize
int cRenderDX::GetVertexSize(const int VertexFormatID) {
	if(VertexFormatID < 0 || VertexFormatID >= s_VertexFormats.Count()) {
		return 0;
	}
	const cRenderDX_VERTEX_FORMAT &vf = s_VertexFormats[VertexFormatID];
	return vf.VertexSize;
}

//-----------------------------------------------------------------------------
// cRenderDX::AddVertexBuffer
//-----------------------------------------------------------------------------
int cRenderDX::AddVertexBuffer(const void *Data, const size_t Size) {
	cRenderDX_VERTEX_BUFFER vb;
	vb.Size = Size;
	
	HRESULT hr = s_Device->CreateVertexBuffer((UINT)Size, 0, 0, D3DPOOL_MANAGED, &vb.Ptr, nullptr);
	cAssert(SUCCEEDED(hr));
	if(!SUCCEEDED(hr)) {
		return -1;
	}
	void *Ptr;
	if(Data != nullptr) {
		hr = vb.Ptr->Lock(0, (UINT)Size, &Ptr, 0);
		if(SUCCEEDED(hr)) {
			memcpy(Ptr, Data, Size);
			vb.Ptr->Unlock();
		}
	}

	int VertexBufferID = -1;
	if(!s_VertexBuffersFreeID.IsEmpty()) {
		VertexBufferID = s_VertexBuffersFreeID.GetLast();
		s_VertexBuffersFreeID.RemoveLast();
		s_VertexBuffers[VertexBufferID] = vb;
		return VertexBufferID;
	} else {
		return s_VertexBuffers.Add(vb);
	}
} // cRenderDX::AddVertexBuffer

//---------------------------------------------------------------------------------
// cRenderDX::SetVertexBuffer
//---------------------------------------------------------------------------------
void cRenderDX::SetVertexBuffer(const int VertexBufferID) {
	if(VertexBufferID == s_CurVertexBufferID) {
		return;
	}

	if(-1 == VertexBufferID) {
		s_Device->SetStreamSource(0, nullptr, 0, 0);
	} else {
		cAssert(VertexBufferID >= 0 && VertexBufferID < s_VertexBuffers.Count());
		if(VertexBufferID >= 0 && VertexBufferID < s_VertexBuffers.Count()) {
			cAssert(s_CurVertexFormatID >= 0 && s_CurVertexFormatID < s_VertexFormats.Count());
			if(s_CurVertexFormatID >= 0 && s_CurVertexFormatID < s_VertexFormats.Count()) {
				cRenderDX_VERTEX_BUFFER &vb = s_VertexBuffers[VertexBufferID];
				cRenderDX_VERTEX_FORMAT &vf = s_VertexFormats[s_CurVertexFormatID];
				s_Device->SetStreamSource(0, vb.Ptr, 0, vf.VertexSize);
			}
		}
	}
	s_CurVertexBufferID = VertexBufferID;
} // cRenderDX::SetVertexBuffer

//-----------------------------------------------------------------------------
// cRenderDX::FreeVertexBuffer
//-----------------------------------------------------------------------------
void cRenderDX::FreeVertexBuffer(const int VertexBufferID) {
	if(VertexBufferID < 0 || VertexBufferID >= s_VertexBuffers.Count()) {
		return; // Invalid vertex buffer ID
	}

	cRenderDX_VERTEX_BUFFER &vb = s_VertexBuffers[VertexBufferID];
	if(vb.Ptr != nullptr) {
		vb.Ptr->Release();
		vb.Ptr = nullptr;
		s_VertexBuffersFreeID.Add(VertexBufferID);
	}
} // cRenderDX::FreeVertexBuffer

//------------------------------------------------------------------------------------------
// cRenderDX::AddIndexBuffer
//------------------------------------------------------------------------------------------
int cRenderDX::AddIndexBuffer(const void *Data, const int IndexCount, const int IndexSize) {
	cRenderDX_INDEX_BUFFER ib;
	ib.IndexCount = IndexCount;
	UINT Size = IndexSize * IndexCount;
	
	HRESULT hr = s_Device->CreateIndexBuffer(Size, 0, 2 == IndexSize ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_MANAGED, &ib.Ptr, nullptr);
	cAssert(SUCCEEDED(hr));
	if(!SUCCEEDED(hr)) {
		return -1;
	}
	void *Ptr;
	if(Data != nullptr) {
		hr = ib.Ptr->Lock(0, Size, &Ptr, 0);
		if(SUCCEEDED(hr)) {
			memcpy(Ptr, Data, Size);
			ib.Ptr->Unlock();
		}
	}

	int IndexBufferID = -1;
	if(!s_IndexBuffersFreeID.IsEmpty()) {
		IndexBufferID = s_IndexBuffersFreeID.GetLast();
		s_IndexBuffersFreeID.RemoveLast();
		s_IndexBuffers[IndexBufferID] = ib;
		return IndexBufferID;
	} else {
		return s_IndexBuffers.Add(ib);
	}
} // cRenderDX::AddIndexBuffer

//-----------------------------------------------------------------------------
// cRenderDX::SetIndexBuffer
//-----------------------------------------------------------------------------
void cRenderDX::SetIndexBuffer(const int IndexBufferID) {
	if(IndexBufferID == s_CurIndexBufferID) {
		return;
	}

	if(IndexBufferID == -1) {
		s_Device->SetIndices(nullptr);
	} else {
		cAssert(IndexBufferID >= 0 && IndexBufferID < s_IndexBuffers.Count());
		if(IndexBufferID >= 0 && IndexBufferID < s_IndexBuffers.Count()) {
			cRenderDX_INDEX_BUFFER &ib = s_IndexBuffers[IndexBufferID];
			s_Device->SetIndices(ib.Ptr);
		}
	}

	s_CurIndexBufferID = IndexBufferID;
} // cRenderDX::SetIndexBuffer

//-----------------------------------------------------------------------------
// cRenderDX::FreeIndexBuffer
//-----------------------------------------------------------------------------
void cRenderDX::FreeIndexBuffer(const int IndexBufferID) {
	if(IndexBufferID < 0 || IndexBufferID >= s_IndexBuffers.Count()) {
		return; // Invalid index buffer ID
	}

	cRenderDX_INDEX_BUFFER &ib = s_IndexBuffers[IndexBufferID];
	if(ib.Ptr != nullptr) {
		ib.Ptr->Release();
		ib.Ptr = nullptr;
		s_IndexBuffersFreeID.Add(IndexBufferID);
	}
} // cRenderDX::FreeIndexBuffer

//-----------------------------------------------------------------------------
// cRenderDX::GetShaderID
//-----------------------------------------------------------------------------
int cRenderDX::GetShaderID(const char *ShaderName, const int VertexFormatID, const char *Extra, cStr *OptionalLog) {
	// Check shader name
	cAssert(ShaderName != nullptr);
	if(cStr::Length(ShaderName) < 1) {
		return -1; // No name
	}

	cAssert(VertexFormatID >= 0 && VertexFormatID < s_VertexFormats.Count());
	if(VertexFormatID < 0 || VertexFormatID >= s_VertexFormats.Count()) {
		return -1; // Invalid vertex format ID
	}

	// Searching loaded shaders
	int i;
	cStr E = Extra; // Since "cStr::Compare" asserts not NULLs
	for(i = 0; i < s_Shaders.Count(); i++) {
		const cRenderDX_SHADER &r = s_Shaders[i];
		if(cStr::EqualsNoCase(r.Name, ShaderName)) { // Same name
			if(r.VertexFormatID == VertexFormatID) { // Same vertex format
				if(cStr::Equals(r.Extra, E)) { // Same extra
					return i; // Such shader is already loaded
				}
			}
		}
	}

	return AddShader(ShaderName, VertexFormatID, Extra, OptionalLog);
} // cRenderDX::GetShaderID

//-------------------------------------------------------------------------------------------------------------------
// CreateShader
//-------------------------------------------------------------------------------------------------------------------
static bool CreateShader(const char *ShaderName, const int VertexFormatID, const char *Extra, cRenderDX_SHADER *Sh, cStr *OptionalLog) {
	// Check shader name
	cAssert(ShaderName != nullptr);
	if(cStr::Length(ShaderName) < 1) {
		return false; // No name
	}

	cAssert(VertexFormatID >= 0 && VertexFormatID < s_VertexFormats.Count());
	if(VertexFormatID < 0 || VertexFormatID >= s_VertexFormats.Count()) {
		return false; // Invalid vertex format ID
	}

	// DX shader file pathname should be with the extension "hlsl"
	cStr FilePn = cStr::Format("data/Shaders/%s.hlsl", ShaderName);
	
	//*************************************************************************
	// Loading file
	//*************************************************************************
	cStr VSText, GSText, PSText;
	int VSLine, GSLine, PSLine;
	cFile VSBin, PSBin;
	bool Bin = false;
	Sh->Extra = Extra; // We should not modify inline extra because "GetShaderID" will be unable to find the match
	cStr E = Sh->Extra;
	if(!E.IsEmpty()) {
		E.Trim(cStr::EndLn);
		if(!E.EndsWith("\n")) { // Trailing EOL
			E += "\n";
		}
	}
	if(cRender::Stub::LoadShaderFromCache(FilePn, Sh->Extra, &VSBin, &PSBin)) {
		Bin = true;
	} else {
		if(!cRender::Stub::LoadShader(FilePn, &VSText, &GSText, &PSText, &VSLine, &GSLine, &PSLine)) {
			return false;
		}
		if(!E.IsEmpty()) {
			cLog::Message(E); // Show extra
		}
	}
	
	Sh->Name = ShaderName;
	Sh->VertexFormatID = VertexFormatID;
	Sh->FilePn = FilePn;
	
	if(Bin) {
		// Adding VS & PS
		s_Device->CreateVertexShader((DWORD *)VSBin.ToPtr(), &Sh->VS);
		s_Device->CreatePixelShader((DWORD *)PSBin.ToPtr(), &Sh->PS);
		D3DXGetShaderConstantTable((DWORD *)VSBin.ToPtr(), &Sh->VSConstants);
		D3DXGetShaderConstantTable((DWORD *)PSBin.ToPtr(), &Sh->PSConstants);
	} else {
		LPD3DXBUFFER ShaderBuffer = nullptr;
		LPD3DXBUFFER ErrorsBuffer = nullptr;
		HRESULT hr;
		
		//*************************************************************************
		// Compiling VS
		//*************************************************************************
		cStr Body;
		const char *Profile;
		if(!VSText.IsEmpty()) {
			cLog::Message("Compiling VS...");

			Body = E;
			Body += cStr::Format("#line %d\n", VSLine + 1);
			Body += VSText;
			Profile = D3DXGetVertexShaderProfile(s_Device);
			// D3DCompile
			hr = D3DXCompileShader(Body.ToCharPtr(), Body.Length(), nullptr, nullptr, "main", Profile, 0, &ShaderBuffer, &ErrorsBuffer, &Sh->VSConstants);
			if(SUCCEEDED(hr)) {
				s_Device->CreateVertexShader((DWORD *)ShaderBuffer->GetBufferPointer(), &Sh->VS);
			} else {
				cLog::Warning((const char *)ErrorsBuffer->GetBufferPointer());
				if (OptionalLog != nullptr) {
					OptionalLog->Copy((const char *)ErrorsBuffer->GetBufferPointer());
				}
			}
			if(ShaderBuffer != nullptr) {
				ShaderBuffer->Release();
				ShaderBuffer = nullptr;
			}
			if(ErrorsBuffer != nullptr) {
				ErrorsBuffer->Release();
				ErrorsBuffer = nullptr;
			}
			if(nullptr == Sh->VS) {
				return false;
			}
		}

		//*************************************************************************
		// Compiling PS
		//*************************************************************************
		if(!PSText.IsEmpty()) {
			cLog::Message("Compiling PS...");

			Body.Clear();
			Body = E;
			Body += cStr::Format("#line %d\n", PSLine + 1);
			Body += PSText;
			Profile = D3DXGetPixelShaderProfile(s_Device);
			// D3DCompile
			hr = D3DXCompileShader(Body.ToCharPtr(), Body.Length(), nullptr, nullptr, "main", Profile, 0, &ShaderBuffer, &ErrorsBuffer, &Sh->PSConstants);
			if(SUCCEEDED(hr)) {
				s_Device->CreatePixelShader((DWORD *)ShaderBuffer->GetBufferPointer(), &Sh->PS);
			} else {
				cLog::Warning((const char *)ErrorsBuffer->GetBufferPointer());
				if (OptionalLog != nullptr) {
					OptionalLog->Copy((const char *)ErrorsBuffer->GetBufferPointer());
				}
			}
			if(ShaderBuffer != nullptr) {
				ShaderBuffer->Release();
				ShaderBuffer = nullptr;
			}
			if(ErrorsBuffer != nullptr) {
				ErrorsBuffer->Release();
				ErrorsBuffer = nullptr;
			}
			if(nullptr == Sh->PS) {
				return false;
			}
		}
	}
	
	if(nullptr == Sh->VS || nullptr == Sh->PS) {
		return false;
	}

	//*************************************************************************
	// Query samplers, constants, and auto constants
	//*************************************************************************
	D3DXCONSTANTTABLE_DESC VSDesc, PSDesc;
	Sh->VSConstants->GetDesc(&VSDesc);
	Sh->PSConstants->GetDesc(&PSDesc);

	cRenderDX_SHADER::Sampler Sm;
	cRenderDX_SHADER::AutoConst Ac;
	cRenderDX_SHADER::Const C;
	D3DXCONSTANT_DESC Desc;
	int i;
	UINT One = 1;
	cList<D3DXPARAMETER_TYPE> SamplerTypes;
	SamplerTypes.Add(D3DXPT_SAMPLER);
	SamplerTypes.Add(D3DXPT_SAMPLER1D);
	SamplerTypes.Add(D3DXPT_SAMPLER2D);
	SamplerTypes.Add(D3DXPT_SAMPLER3D);
	SamplerTypes.Add(D3DXPT_SAMPLERCUBE);
	
	// Vertex shader constants and auto constants
	for(i = 0; i < (int)VSDesc.Constants; i++) {
		Sh->VSConstants->GetConstantDesc(Sh->VSConstants->GetConstant(nullptr, i), &Desc, &One);
		if(SamplerTypes.Contains(Desc.Type)) {
			// Skip samplers in vertex shader
		} else if((Ac.Index = cRender::AutoConst::FromString(Desc.Name)) != -1) { // VS auto constant
			Ac.Name = Desc.Name;
			Ac.VSReg = Desc.RegisterIndex;
			Ac.PSReg = -1;
			Sh->AutoConsts.Add(Ac);
		} else { // VS Constant
			C.Name = Desc.Name;
			C.VSReg = Desc.RegisterIndex;
			C.PSReg = -1;
			Sh->Consts.Add(C);
		}
	}
	
	// Pixel shader samplers, constants, and auto constants
	int Merge, j;
	for(i = 0; i < (int)PSDesc.Constants; i++) {
		Sh->PSConstants->GetConstantDesc(Sh->PSConstants->GetConstant(nullptr, i), &Desc, &One);
		if(SamplerTypes.Contains(Desc.Type)) { // PS sampler
			Sm.Name = Desc.Name;
			Sm.ImageUnit = Desc.RegisterIndex;
			Sh->Samplers.Add(Sm);
		} else if((Ac.Index = cRender::AutoConst::FromString(Desc.Name)) != -1) { // PS auto constant
			Merge = -1;
			for(j = 0; j < Sh->AutoConsts.Count(); j++) {
				if(Sh->AutoConsts[j].Index == Ac.Index) {
					Merge = j;
					break;
				}
			}
			if(-1 == Merge) {
				Ac.Name = Desc.Name;
				Ac.VSReg = -1;
				Ac.PSReg = Desc.RegisterIndex;
				Sh->AutoConsts.Add(Ac);
			} else {
				Sh->AutoConsts[Merge].PSReg = Desc.RegisterIndex;
			}
		} else { // PS constant
			Merge = -1;
			for(j = 0; j < Sh->Consts.Count(); j++) {
				if(cStr::Equals(Sh->Consts[j].Name, Desc.Name)) {
					Merge = j;
					break;
				}
			}
			if(-1 == Merge) {
				C.Name = Desc.Name;
				C.VSReg = -1;
				C.PSReg = Desc.RegisterIndex;
				Sh->Consts.Add(C);
			} else {
				Sh->Consts[Merge].PSReg = Desc.RegisterIndex;
			}
		}
	}

	// Get handles to constants
	for(i = 0; i < Sh->Consts.Count(); i++) {
		cRenderDX_SHADER::Const &C = Sh->Consts[i];
		if(C.VSReg != -1) {
			C.VSHandle = Sh->VSConstants->GetConstantByName(nullptr, C.Name);
		}
		if(C.PSReg != -1) {
			C.PSHandle = Sh->PSConstants->GetConstantByName(nullptr, C.Name);
		}
	}
	// Get handles to auto constants
	for(i = 0; i < Sh->AutoConsts.Count(); i++) {
		cRenderDX_SHADER::AutoConst &Ac = Sh->AutoConsts[i];
		if(Ac.VSReg != -1) {
			Ac.VSHandle = Sh->VSConstants->GetConstantByName(nullptr, Ac.Name);
		}
		if(Ac.PSReg != -1) {
			Ac.PSHandle = Sh->PSConstants->GetConstantByName(nullptr, Ac.Name);
		}
	}

	return true;
} // CreateShader

//---------------------------------------------------------------------------------------------
// cRenderDX::AddShader
//---------------------------------------------------------------------------------------------
int cRenderDX::AddShader(const char *ShaderName, const int VertexFormatID, const char *Extra, cStr *OptionalLog) {
#ifdef COMMS_3DCOAT
	LogNewShader(ShaderName,VertexFormatID,Extra);
#endif // COMMS_3DCOAT

	cRenderDX_SHADER Sh;
	if (!CreateShader(ShaderName, VertexFormatID, Extra, &Sh, OptionalLog)) {
		return -1;
	}
	
	// Add shader or use empty slot
	int ShaderID = -1;
	if(!s_ShadersFreeID.IsEmpty()) {
		ShaderID = s_ShadersFreeID.GetLast();
		s_ShadersFreeID.RemoveLast();
		s_Shaders[ShaderID] = Sh;
		return ShaderID;
	} else {
		return s_Shaders.Add(Sh);
	}
} // cRenderDX::AddShader

//-----------------------------------------------------------------------------
// cRenderDX::ReloadShaders
//-----------------------------------------------------------------------------
void cRenderDX::ReloadShaders() {
	int i;
	cRenderDX_SHADER Rel;
	cStr S, E;
	int F;

	cLog::Message(cStr(60, '-'));
	SetShader(-1);
	
	int c = 0;
	for(i = 0; i < s_Shaders.Count(); i++) {
		cRenderDX_SHADER &Sh = s_Shaders[i];
		if(s_ShadersFreeID.Contains(i)) {
			continue;
		}
		S = Sh.Name;
		E = Sh.Extra;
		F = Sh.VertexFormatID;
		Rel.Clear();
		if(!CreateShader(S.ToCharPtr(), F, E.ToCharPtr(), &Rel, nullptr)) {
			Rel.Free();
		} else {
			Sh.Free();
			Sh = Rel;
			c++;
		}
	}
	cLog::Message(cStr(60, '-') + cStr::EndLn + "Reloaded " + cStr::ToString(c) + " Shaders.");
} // cRenderDX::ReloadShaders

//-----------------------------------------------------------------------------
// SetVertexFormat
//-----------------------------------------------------------------------------
static void SetVertexFormat(const int VertexFormatID) {
	if(VertexFormatID == s_CurVertexFormatID) {
		return;
	}
	
	cAssert(VertexFormatID >= 0 && VertexFormatID < s_VertexFormats.Count());
	if(VertexFormatID < 0 || VertexFormatID >= s_VertexFormats.Count()) {
		return;
	}

	s_Device->SetVertexDeclaration(s_VertexFormats[VertexFormatID].Decl);
	s_CurVertexFormatID = VertexFormatID;
} // SetVertexFormat

//-----------------------------------------------------------------------------
// cRenderDX::SetShader
//-----------------------------------------------------------------------------
void cRenderDX::SetShader(const int ShaderID) {
	if(ShaderID == s_CurShaderID) {
		return;
	}

	if(ShaderID < 0 || ShaderID >= s_Shaders.Count() || s_ShadersFreeID.Contains(ShaderID)) {
		s_Device->SetVertexShader(nullptr);
		s_Device->SetPixelShader(nullptr);
		s_CurShaderID = -1;
		return;
	}
	
	cRenderDX_SHADER &S = s_Shaders[ShaderID];
	SetVertexFormat(S.VertexFormatID);

	if(!S.IsValid()) {
		s_Device->SetVertexShader(nullptr);
		s_Device->SetPixelShader(nullptr);
		s_CurShaderID = -1;
		return;
	}
	
	s_Device->SetVertexShader(S.VS);
	s_Device->SetPixelShader(S.PS);
	s_CurShaderID = ShaderID;
	
	SetShaderAutoConstants();
} // cRenderDX::SetShader

//-----------------------------------------------------------------------------
// cRenderDX::FreeShader
//-----------------------------------------------------------------------------
void cRenderDX::FreeShader(const int ShaderID) {
	if(ShaderID < 0 || ShaderID >= s_Shaders.Count()) {
		return; // Invalid shader ID
	}

	if(ShaderID == s_CurShaderID) {
		s_Device->SetVertexShader(nullptr);
		s_Device->SetPixelShader(nullptr);
	}

	cRenderDX_SHADER &S = s_Shaders[ShaderID];
	S.Free();
	S.Clear();
	if(!s_ShadersFreeID.Contains(ShaderID)) {
		s_ShadersFreeID.Add(ShaderID);
	}
} // cRenderDX::FreeShader

//-----------------------------------------------------------------------------
// cRenderDX::GetShaderConstID
//-----------------------------------------------------------------------------
int cRenderDX::GetShaderConstID(const int ShaderID, const char *ConstName) {
	cAssert(ConstName != nullptr);
	if(cStr::Length(ConstName) < 1) {
		return -1; // Invalid const name
	}

	cAssert(ShaderID >= 0 && ShaderID < s_Shaders.Count());
	if(ShaderID < 0 || ShaderID >= s_Shaders.Count()) {
		return -1;
	}

	const cRenderDX_SHADER &Sh = s_Shaders[ShaderID];
	int i;
	for(i = 0; i < Sh.Consts.Count(); i++) {
		const cRenderDX_SHADER::Const &C = Sh.Consts[i];
		if(cStr::Equals(C.Name, ConstName)) {
			return i;
		}
	}
	return -1;
} // cRenderDX::GetShaderConstID

//------------------------------------------------------------------------------
// cRenderDX::GetSamplerID
//------------------------------------------------------------------------------
int cRenderDX::GetSamplerID(const int ShaderID, const char *SamplerName) const {
	cAssert(SamplerName != nullptr);
	if(cStr::Length(SamplerName) < 1) {
		return -1; // Invalid sampler name
	}

	cAssert(ShaderID >= 0 && ShaderID < s_Shaders.Count());
	if(ShaderID < 0 || ShaderID >= s_Shaders.Count()) {
		return -1;
	}

	const cRenderDX_SHADER &Sh = s_Shaders[ShaderID];
	int i;
	for(i = 0; i < Sh.Samplers.Count(); i++) {
		const cRenderDX_SHADER::Sampler &Sm = Sh.Samplers[i];
		if(cStr::Equals(Sm.Name, SamplerName)) {
			return i;
		}
	}
	return -1;
} // cRenderDX::GetSamplerID

//-----------------------------------------------------------------------------
// cRenderDX::SetShaderConst : (..., const int)
//-----------------------------------------------------------------------------
void cRenderDX::SetShaderConst(const int ConstID, const int Value) {
	if(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count()) {
		const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderDX_SHADER::Const &C = Sh.Consts[ConstID];
			if(C.VSHandle != nullptr) {
				Sh.VSConstants->SetInt(s_Device, C.VSHandle, Value);
			}
			if(C.PSHandle != nullptr) {
				Sh.PSConstants->SetInt(s_Device, C.PSHandle, Value);
			}
		}
	}
} // cRenderDX::SetShaderConst : (..., const int)

//-----------------------------------------------------------------------------
// cRenderDX::SetShaderConst : (..., const float)
//-----------------------------------------------------------------------------
void cRenderDX::SetShaderConst(const int ConstID, const float Value) {
	if(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count()) {
		const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderDX_SHADER::Const &C = Sh.Consts[ConstID];
			if(C.VSHandle != nullptr) {
				Sh.VSConstants->SetFloat(s_Device, C.VSHandle, Value);
			}
			if(C.PSHandle != nullptr) {
				Sh.PSConstants->SetFloat(s_Device, C.PSHandle, Value);
			}
		}
	}
} // cRenderDX::SetShaderConst : (..., const float)

//-----------------------------------------------------------------------------
// cRenderDX::SetShaderConst : (..., const cVec2 &)
//-----------------------------------------------------------------------------
void cRenderDX::SetShaderConst(const int ConstID, const cVec2 &Value) {
	if(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count()) {
		const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderDX_SHADER::Const &C = Sh.Consts[ConstID];
			cVec4 u(Value, 0.0f, 1.0f);
			if(C.VSHandle != nullptr) {
				Sh.VSConstants->SetVector(s_Device, C.VSHandle, (const D3DXVECTOR4 *)&u); // XMFLOAT4
			}
			if(C.PSHandle != nullptr) {
				Sh.PSConstants->SetVector(s_Device, C.PSHandle, (const D3DXVECTOR4 *)&u);
			}
		}
	}
} // cRenderDX::SetShaderConst : (..., const cVec2 &)

//-----------------------------------------------------------------------------
// cRenderDX::SetShaderConst : (..., const cVec3 &)
//-----------------------------------------------------------------------------
void cRenderDX::SetShaderConst(const int ConstID, const cVec3 &Value) {
	if(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count()) {
		const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderDX_SHADER::Const &C = Sh.Consts[ConstID];
			cVec4 u(Value, 1.0f);
			if(C.VSHandle != nullptr) {
				Sh.VSConstants->SetVector(s_Device, C.VSHandle, (const D3DXVECTOR4 *)&u);
			}
			if(C.PSHandle != nullptr) {
				Sh.PSConstants->SetVector(s_Device, C.PSHandle, (const D3DXVECTOR4 *)&u);
			}
		}
	}
} // cRenderDX::SetShaderConst : (..., const cVec3 &)

//-----------------------------------------------------------------------------
// cRenderDX::SetShaderConst : (..., const cVec4 &)
//-----------------------------------------------------------------------------
void cRenderDX::SetShaderConst(const int ConstID, const cVec4 &Value) {
	if(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count()) {
		const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderDX_SHADER::Const &C = Sh.Consts[ConstID];
			if(C.VSHandle != nullptr) {
				Sh.VSConstants->SetVector(s_Device, C.VSHandle, (const D3DXVECTOR4 *)&Value);
			}
			if(C.PSHandle != nullptr) {
				Sh.PSConstants->SetVector(s_Device, C.PSHandle, (const D3DXVECTOR4 *)&Value);
			}
		}
	}
} // cRenderDX::SetShaderConst : (..., const cVec4 &)

//-----------------------------------------------------------------------------
// cRenderDX::SetShaderConst : (..., const cMat3 &)
//-----------------------------------------------------------------------------
void cRenderDX::SetShaderConst(const int ConstID, const cMat3 &Value) {
	if(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count()) {
		const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderDX_SHADER::Const &C = Sh.Consts[ConstID];
			if(C.VSHandle != nullptr) {
				Sh.VSConstants->SetFloatArray(s_Device, C.VSHandle, Value.ToFloatPtr(), 9);
			}
			if(C.PSHandle != nullptr) {
				Sh.PSConstants->SetFloatArray(s_Device, C.PSHandle, Value.ToFloatPtr(), 9);
			}
		}
	}
} // cRenderDX::SetShaderConst : (..., const cMat3 &)

//-----------------------------------------------------------------------------
// cRenderDX::SetShaderConst : (..., const cMat4 &)
//-----------------------------------------------------------------------------
void cRenderDX::SetShaderConst(const int ConstID, const cMat4 &Value) {
	if(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count()) {
		const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderDX_SHADER::Const &C = Sh.Consts[ConstID];
			if(C.VSHandle != nullptr) {
				Sh.VSConstants->SetMatrix(s_Device, C.VSHandle, (const D3DXMATRIX *)&Value); // XMMATRIX
			}
			if(C.PSHandle != nullptr) {
				Sh.PSConstants->SetMatrix(s_Device, C.PSHandle, (const D3DXMATRIX *)&Value);
			}
		}
	}
} // cRenderDX::SetShaderConst : (..., const cMat4 &)

//--------------------------------------------------------------------------------------
// cRenderDX::SetShaderConst : (..., const float *, ...)
//--------------------------------------------------------------------------------------
void cRenderDX::SetShaderConst(const int ConstID, const float *Array, const int Count) {
	if(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count()) {
		const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderDX_SHADER::Const &C = Sh.Consts[ConstID];
			if(C.VSHandle != nullptr) {
				Sh.VSConstants->SetFloatArray(s_Device, C.VSHandle, Array, Count);
			}
			if(C.PSHandle != nullptr) {
				Sh.PSConstants->SetFloatArray(s_Device, C.PSHandle, Array, Count);
			}
		}
	}
} // cRenderDX::SetShaderConst : (..., const float *, ...)

//--------------------------------------------------------------------------------------
// cRenderDX::SetShaderConst : (..., const cVec2 *, ...)
//--------------------------------------------------------------------------------------
void cRenderDX::SetShaderConst(const int ConstID, const cVec2 *Array, const int Count) {
	static cList<cVec4> Array4;
	Array4.Clear();
	int i;
	
	if(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count()) {
		const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			for(i = 0; i < Count; i++) {
				Array4.Add(cVec4(Array[i], 0.0f, 1.0f));
			}

			const cRenderDX_SHADER::Const &C = Sh.Consts[ConstID];
			if(C.VSHandle != nullptr) {
				Sh.VSConstants->SetVectorArray(s_Device, C.VSHandle, (const D3DXVECTOR4 *)Array4.ToPtr(), Count);
			}
			if(C.PSHandle != nullptr) {
				Sh.PSConstants->SetVectorArray(s_Device, C.PSHandle, (const D3DXVECTOR4 *)Array4.ToPtr(), Count);
			}
		}
	}
} // cRenderDX::SetShaderConst : (..., const cVec2 *, ...)

//--------------------------------------------------------------------------------------
// cRenderDX::SetShaderConst : (..., const cVec3 *, ...)
//--------------------------------------------------------------------------------------
void cRenderDX::SetShaderConst(const int ConstID, const cVec3 *Array, const int Count) {
	static cList<cVec4> Array4;
	Array4.Clear();
	int i;
	
	if(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count()) {
		const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			for(i = 0; i < Count; i++) {
				Array4.Add(cVec4(Array[i], 1.0f));
			}

			const cRenderDX_SHADER::Const &C = Sh.Consts[ConstID];
			if(C.VSHandle != nullptr) {
				Sh.VSConstants->SetVectorArray(s_Device, C.VSHandle, (const D3DXVECTOR4 *)Array4.ToPtr(), Count);
			}
			if(C.PSHandle != nullptr) {
				Sh.PSConstants->SetVectorArray(s_Device, C.PSHandle, (const D3DXVECTOR4 *)Array4.ToPtr(), Count);
			}
		}
	}
} // cRenderDX::SetShaderConst : (..., const cVec3 *, ...)

//--------------------------------------------------------------------------------------
// cRenderDX::SetShaderConst : (..., const cVec4 *, ...)
//--------------------------------------------------------------------------------------
void cRenderDX::SetShaderConst(const int ConstID, const cVec4 *Array, const int Count) {
	if(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count()) {
		const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderDX_SHADER::Const &C = Sh.Consts[ConstID];
			if(C.VSHandle != nullptr) {
				Sh.VSConstants->SetVectorArray(s_Device, C.VSHandle, (const D3DXVECTOR4 *)Array, Count);
			}
			if(C.PSHandle != nullptr) {
				Sh.PSConstants->SetVectorArray(s_Device, C.PSHandle, (const D3DXVECTOR4 *)Array, Count);
			}
		}
	}
} // cRenderDX::SetShaderConst : (..., const cVec4 *, ...)

//-----------------------------------------------------------------------------
// cRenderDX::SetShaderAutoConstants
//-----------------------------------------------------------------------------
void cRenderDX::SetShaderAutoConstants() {
	if(s_CurShaderID < 0 || s_CurShaderID >= s_Shaders.Count()) {
		return; // There is no active shader
	}
	
	int i;
	const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
	float F;
	cVec4 u;
	cMat4 M;
	cMat3 R;
	
	for(i = 0; i < Sh.AutoConsts.Count(); i++) {
		const cRenderDX_SHADER::AutoConst &Ac = Sh.AutoConsts[i];
		
		switch(Ac.Index) {
			case cRender::AutoConst::ViewportWidth:
				F = cRender::GetViewer()->GetViewport().GetWidth();
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetFloat(s_Device, Ac.VSHandle, F);
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetFloat(s_Device, Ac.PSHandle, F);
				}
				break;
			case cRender::AutoConst::ViewportHeight:
				F = cRender::GetViewer()->GetViewport().GetHeight();
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetFloat(s_Device, Ac.VSHandle, F);
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetFloat(s_Device, Ac.PSHandle, F);
				}
				break;
			case cRender::AutoConst::ViewportInvWidth:
				F = 1.0f / cRender::GetViewer()->GetViewport().GetWidth();
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetFloat(s_Device, Ac.VSHandle, F);
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetFloat(s_Device, Ac.PSHandle, F);
				}
				break;
			case cRender::AutoConst::ViewportInvHeight:
				F = 1.0f / cRender::GetViewer()->GetViewport().GetHeight();
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetFloat(s_Device, Ac.VSHandle, F);
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetFloat(s_Device, Ac.PSHandle, F);
				}
				break;
			case cRender::AutoConst::WorldMatrix:
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetMatrix(s_Device, Ac.VSHandle, (const D3DXMATRIX *)&cRender::GetWorldMatrix());
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetMatrix(s_Device, Ac.PSHandle, (const D3DXMATRIX *)&cRender::GetWorldMatrix());
				}
				break;
			case cRender::AutoConst::WorldMatrixInverse:
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetMatrix(s_Device, Ac.VSHandle, (const D3DXMATRIX *)&cRender::GetWorldMatrixInverse());
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetMatrix(s_Device, Ac.PSHandle, (const D3DXMATRIX *)&cRender::GetWorldMatrixInverse());
				}
				break;
			case cRender::AutoConst::ViewerPos:
				if(cRender::GetViewer()->GetOrthoProj()) {
					u.Set(cRender::GetViewer()->GetForward() * (-1e8f), 1.0f);
				} else {
					u.Set(cRender::GetViewer()->GetPos(), 1.0f);
				}
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetVector(s_Device, Ac.VSHandle, (const D3DXVECTOR4 *)&u);
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetVector(s_Device, Ac.PSHandle, (const D3DXVECTOR4 *)&u);
				}
				break;
			case cRender::AutoConst::WorldViewMatrix:
				M = cMat4::Mul(cRender::GetWorldMatrix(), cRender::GetViewer()->GetViewMatrix());
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetMatrix(s_Device, Ac.VSHandle, (const D3DXMATRIX *)M.ToFloatPtr());
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetMatrix(s_Device, Ac.PSHandle, (const D3DXMATRIX *)M.ToFloatPtr());
				}
				break;
			case cRender::AutoConst::ViewProjectionMatrix:
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetMatrix(s_Device, Ac.VSHandle, (const D3DXMATRIX *)&cRender::GetViewer()->GetViewProjectionMatrix());
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetMatrix(s_Device, Ac.PSHandle, (const D3DXMATRIX *)&cRender::GetViewer()->GetViewProjectionMatrix());
				}
				break;
			case cRender::AutoConst::WorldViewProjectionMatrix:
				M = cMat4::Mul(cRender::GetWorldMatrix(), cRender::GetViewer()->GetViewProjectionMatrix());
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetMatrix(s_Device, Ac.VSHandle, (const D3DXMATRIX *)&M);
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetMatrix(s_Device, Ac.PSHandle, (const D3DXMATRIX *)&M);
				}
				break;
			case cRender::AutoConst::ProjectionMatrix:
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetMatrix(s_Device, Ac.VSHandle, (const D3DXMATRIX *)&cRender::GetViewer()->GetProjectionMatrix());
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetMatrix(s_Device, Ac.PSHandle, (const D3DXMATRIX *)&cRender::GetViewer()->GetProjectionMatrix());
				}
				break;
			case cRender::AutoConst::ScreenMatrix:
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetMatrix(s_Device, Ac.VSHandle, (const D3DXMATRIX *)&cRender::GetViewer()->GetScreenMatrix());
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetMatrix(s_Device, Ac.PSHandle, (const D3DXMATRIX *)&cRender::GetViewer()->GetScreenMatrix());
				}
				break;
			case cRender::AutoConst::ScreenMatrixInverse:
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetMatrix(s_Device, Ac.VSHandle, (const D3DXMATRIX *)&cRender::GetViewer()->GetScreenMatrixInverse());
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetMatrix(s_Device, Ac.PSHandle, (const D3DXMATRIX *)&cRender::GetViewer()->GetScreenMatrixInverse());
				}
				break;
			case cRender::AutoConst::NormalMatrix:
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetFloatArray(s_Device, Ac.VSHandle, cRender::GetNormalMatrix().ToFloatPtr(), 9);
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetFloatArray(s_Device, Ac.PSHandle, cRender::GetNormalMatrix().ToFloatPtr(), 9);
				}
				break;
			case cRender::AutoConst::NormalViewMatrix:
				R = cMat3::Mul(cRender::GetNormalMatrix(), cRender::GetViewer()->GetViewMatrix().ToMat3());
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetFloatArray(s_Device, Ac.VSHandle, R.ToFloatPtr(), 9);
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetFloatArray(s_Device, Ac.PSHandle, R.ToFloatPtr(), 9);
				}
				break;
			case cRender::AutoConst::TimeSec:
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetFloat(s_Device, Ac.VSHandle, cTimer::GetTimeSec());
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetFloat(s_Device, Ac.PSHandle, cTimer::GetTimeSec());
				}
				break;
			case cRender::AutoConst::FrameTimeSec:
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetFloat(s_Device, Ac.VSHandle, cTimer::GetFrameTimeSec());
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetFloat(s_Device, Ac.PSHandle, cTimer::GetFrameTimeSec());
				}
				break;
			case cRender::AutoConst::TextureMatrix0:
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetMatrix(s_Device, Ac.VSHandle, (const D3DXMATRIX *)&cRender::GetTextureMatrix(0));
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetMatrix(s_Device, Ac.PSHandle, (const D3DXMATRIX *)&cRender::GetTextureMatrix(0));
				}
				break;
			case cRender::AutoConst::TextureMatrix1:
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetMatrix(s_Device, Ac.VSHandle, (const D3DXMATRIX *)&cRender::GetTextureMatrix(1));
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetMatrix(s_Device, Ac.PSHandle, (const D3DXMATRIX *)&cRender::GetTextureMatrix(1));
				}
				break;
			case cRender::AutoConst::TextureMatrix2:
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetMatrix(s_Device, Ac.VSHandle, (const D3DXMATRIX *)&cRender::GetTextureMatrix(2));
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetMatrix(s_Device, Ac.PSHandle, (const D3DXMATRIX *)&cRender::GetTextureMatrix(2));
				}
				break;
			case cRender::AutoConst::TextureMatrix3:
				if(Ac.VSHandle != nullptr) {
					Sh.VSConstants->SetMatrix(s_Device, Ac.VSHandle, (const D3DXMATRIX *)&cRender::GetTextureMatrix(3));
				}
				if(Ac.PSHandle != nullptr) {
					Sh.PSConstants->SetMatrix(s_Device, Ac.PSHandle, (const D3DXMATRIX *)&cRender::GetTextureMatrix(3));
				}
				break;
			default:
				break;
		}
	}
} // cRenderDX::SetShaderAutoConstants

//------------------------------------------------------------------------------------------------------------
// cRenderDX::GetDepthStateID
//------------------------------------------------------------------------------------------------------------
int cRenderDX::GetDepthStateID(const bool TestEnabled, const bool WriteEnabled, const cDepthFunc::Enum Func) {
	int i;
	for(i = 0; i < s_DepthStates.Count(); i++) {
		const cRenderDX_DEPTH_STATE &r = s_DepthStates[i];
		if(r.TestEnabled == TestEnabled && r.WriteEnabled == WriteEnabled && r.Func == Func) {
			return i;
		}
	}

	cRenderDX_DEPTH_STATE DS;
	DS.TestEnabled = TestEnabled;
	DS.WriteEnabled = WriteEnabled;
	DS.Func = Func;
	return s_DepthStates.Add(DS);
} // cRenderDX::GetDepthStateID

//-----------------------------------------------------------------------------
// cRenderDX::SetDepthState
//-----------------------------------------------------------------------------
void cRenderDX::SetDepthState(const int DepthStateID) {
	static const DWORD ZFunc[] = {
		D3DCMP_NEVER,			// cDepthFunc::Never
		D3DCMP_LESS,			// cDepthFunc::Less
		D3DCMP_EQUAL,			// cDepthFunc::Equal
		D3DCMP_LESSEQUAL,		// cDepthFunc::LessEqual
		D3DCMP_GREATER,			// cDepthFunc::Greater
		D3DCMP_NOTEQUAL,		// cDepthFunc::NotEqual
		D3DCMP_GREATEREQUAL,	// cDepthFunc::GreaterEqual
		D3DCMP_ALWAYS			// cDepthFunc::Always
	};

	cAssert(DepthStateID >= -1 && DepthStateID < s_DepthStates.Count());
	if(DepthStateID < -1 || DepthStateID >= s_DepthStates.Count()) {
		return;
	}

	if(-1 == DepthStateID) {
		// Disable test and write
		s_Device->SetRenderState(D3DRS_ZENABLE, FALSE);
		s_Device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		s_Device->SetRenderState(D3DRS_ZFUNC, D3DCMP_NEVER);
		return;
	}

	const cRenderDX_DEPTH_STATE &DS = s_DepthStates[DepthStateID];
	s_Device->SetRenderState(D3DRS_ZENABLE, DS.TestEnabled);
	s_Device->SetRenderState(D3DRS_ZWRITEENABLE, DS.WriteEnabled);
	s_Device->SetRenderState(D3DRS_ZFUNC, ZFunc[DS.Func]);
} // cRenderDX::SetDepthState

//----------------------------------------------------------------------------------------------------------------------------------------------------
// cRenderDX::GetBlendStateID
//----------------------------------------------------------------------------------------------------------------------------------------------------
int cRenderDX::GetBlendStateID(const cBlendFactor::Enum SrcFactor,const cBlendFactor::Enum DstFactor, const cBlendMode::Enum Mode, const dword Mask) {
	int i;
	for(i = 0; i < s_BlendStates.Count(); i++) {
		const cRenderDX_BLEND_STATE &r = s_BlendStates[i];
		if(r.SrcFactor == SrcFactor && r.DstFactor == DstFactor && r.Mode == Mode && r.Mask == Mask) {
			return i;
		}
	}
	cRenderDX_BLEND_STATE BS;
	BS.SrcFactor = SrcFactor;
	BS.DstFactor = DstFactor;
	BS.Mode = Mode;
	BS.Mask = Mask;
	BS.EnableBlend = SrcFactor != cBlendFactor::One || DstFactor != cBlendFactor::Zero;
	return s_BlendStates.Add(BS);
} // cRenderDX::GetBlendStateID

//-----------------------------------------------------------------------------
// cRenderDX::SetBlendState
//-----------------------------------------------------------------------------
void cRenderDX::SetBlendState(const int BlendStateID) {
	static const DWORD BlendFactor[] = {
		D3DBLEND_ZERO,			// cBlendFactor::Zero
		D3DBLEND_ONE,			// cBlendFactor::One
		D3DBLEND_SRCCOLOR,		// cBlendFactor::SrcColor
		D3DBLEND_INVSRCCOLOR,	// cBlendFactor::OneMinusSrcColor
		D3DBLEND_SRCALPHA,		// cBlendFactor::SrcAlpha
		D3DBLEND_INVSRCALPHA,	// cBlendFactor::OneMinusSrcAlpha
		D3DBLEND_DESTALPHA,		// cBlendFactor::DstAlpha
		D3DBLEND_INVDESTALPHA,	// cBlendFactor::OneMinusDstAlpha
		D3DBLEND_DESTCOLOR,		// cBlendFactor::DstColor
		D3DBLEND_INVDESTCOLOR,	// cBlendFactor::OneMinusDstColor
		D3DBLEND_SRCALPHASAT	// cBlendFactor::SrcAlphaSat
	};

	static const DWORD BlendMode[] = {
		D3DBLENDOP_ADD,			// cBlendMode::Add
		D3DBLENDOP_SUBTRACT,	// cBlendMode::Subtract
		D3DBLENDOP_REVSUBTRACT, // cBlendMode::ReverseSubtract
		D3DBLENDOP_MIN,			// cBlendMode::Min
		D3DBLENDOP_MAX			// cBlendMode::Max
	};

	cAssert(BlendStateID >= -1 && BlendStateID < s_BlendStates.Count());
	if(BlendStateID < -1 || BlendStateID >= s_BlendStates.Count()) {
		return;
	}

	DWORD Mask = D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA;

	if(-1 == BlendStateID) {
		s_Device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	} else {
		const cRenderDX_BLEND_STATE &BS = s_BlendStates[BlendStateID];
		if(!BS.EnableBlend) {
			s_Device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		} else {
			s_Device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			s_Device->SetRenderState(D3DRS_SRCBLEND, BlendFactor[BS.SrcFactor]);
			s_Device->SetRenderState(D3DRS_DESTBLEND, BlendFactor[BS.DstFactor]);
			s_Device->SetRenderState(D3DRS_BLENDOP, BlendMode[BS.Mode]);
		}

		Mask = 0;
		if(BS.Mask & cBlendMask::Red) {
			Mask |= D3DCOLORWRITEENABLE_RED;
		}
		if(BS.Mask & cBlendMask::Green) {
			Mask |= D3DCOLORWRITEENABLE_GREEN;
		}
		if(BS.Mask & cBlendMask::Blue) {
			Mask |= D3DCOLORWRITEENABLE_BLUE;
		}
		if(BS.Mask & cBlendMask::Alpha) {
			Mask |= D3DCOLORWRITEENABLE_ALPHA;
		}
	}
	s_Device->SetRenderState(D3DRS_COLORWRITEENABLE, Mask);
} // cRenderDX::SetBlendState

//------------------------------------------------------------------------------------------------------------------------------------------------
// cRenderDX::GetSamplerStateID
//------------------------------------------------------------------------------------------------------------------------------------------------
int cRenderDX::GetSamplerStateID(const cFilter::Enum Filter, const cAddressMode::Enum s, const cAddressMode::Enum t, const cAddressMode::Enum r) {
	static const DWORD AddressMode[] = {
		D3DTADDRESS_WRAP,
		D3DTADDRESS_CLAMP,
		D3DTADDRESS_MIRROR
	};

	cRenderDX_SAMPLER_STATE SS;

	SS.MinFilter = cFilter::HasAniso(Filter) ? D3DTEXF_ANISOTROPIC : (Filter != cFilter::Nearest ? D3DTEXF_LINEAR : D3DTEXF_POINT);
	SS.MagFilter = cFilter::HasAniso(Filter) ? D3DTEXF_ANISOTROPIC : (Filter != cFilter::Nearest ? D3DTEXF_LINEAR : D3DTEXF_POINT);
	SS.MipFilter = (cFilter::Trilinear == Filter || cFilter::Trilinear_Aniso == Filter) ? D3DTEXF_LINEAR : (cFilter::HasMipMaps(Filter) ? D3DTEXF_POINT : D3DTEXF_NONE);
	SS.S = AddressMode[s];
	SS.T = AddressMode[t];
	SS.R = AddressMode[r];
	SS.Aniso = cFilter::HasAniso(Filter);
	SS.MipMaps = cFilter::HasMipMaps(Filter);

	int i;
	for(i = 0; i < s_SamplerStates.Count(); i++) {
		const cRenderDX_SAMPLER_STATE &r = s_SamplerStates[i];
		if(cRenderDX_SAMPLER_STATE::Equals(r, SS)) {
			return i;
		}
	}
	return s_SamplerStates.Add(SS);
} // cRenderDX::GetSamplerStateID

// cRenderDX::SetSamplerState
void cRenderDX::SetSamplerState(const int TextureID, const int SamplerStateID) {
	if(SamplerStateID < 0 || SamplerStateID >= s_SamplerStates.Count()) {
		return;
	}
	if(TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return;
	}
	cRenderDX_TEXTURE &T = s_Textures[TextureID];
	if(SamplerStateID == T.SamplerStateID) {
		return;
	}
	T.SamplerStateID = SamplerStateID;
}

// cRenderDX::SetWireframe
void cRenderDX::SetWireframe(const bool Enabled) {
	s_Device->SetRenderState(D3DRS_FILLMODE, Enabled ? D3DFILL_WIREFRAME : D3DFILL_SOLID);
}

//-----------------------------------------------------------------------------
// cRenderDX::SetCullMode
//-----------------------------------------------------------------------------
void cRenderDX::SetCullMode(const cCullMode::Enum CullMode) {
	static const DWORD CM[] = {
		D3DCULL_NONE,
		D3DCULL_CW,
		D3DCULL_CCW
	};
	s_Device->SetRenderState(D3DRS_CULLMODE, CM[CullMode]);
} // cRenderDX::SetCullMode

//-----------------------------------------------------------------------------
// cRenderDX::SetClipRect
//-----------------------------------------------------------------------------
void cRenderDX::SetClipRect(const cRect &ClipRect) {
	IDirect3DSurface9 *CurRT;
	RECT cr;
	D3DSURFACE_DESC Desc;
	HRESULT hr;
	
	if(ClipRect.IsEmpty()) {
		s_Device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	} else {
		hr = s_Device->GetRenderTarget(0, &CurRT);
		if (CurRT && SUCCEEDED(hr)){
			cAssert(SUCCEEDED(hr));
			CurRT->GetDesc(&Desc);
			CurRT->Release();
			CurRT = nullptr;

			s_Device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

			cr.left = (LONG)ClipRect.GetLeft();
			cr.right = (LONG)ClipRect.GetRight();
			cr.top = Desc.Height - (LONG)ClipRect.GetTop();
			cr.bottom = Desc.Height - (LONG)ClipRect.GetBottom();
			s_Device->SetScissorRect(&cr);
		}
	}
} // cRenderDX::SetClipRect

// cRenderDX::GetTextureFilePn
const cStr * cRenderDX::GetTextureFilePn(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count()) {
		return nullptr; // Invalid texture ID
	}
	return &s_Textures[TextureID].FilePn;
}

// cRenderDX::GetTextureWidth
int cRenderDX::GetTextureWidth(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count()) {
		return -1; // Invalid texture ID
	}
	return s_Textures[TextureID].Width;
}

// cRenderDX::GetTextureHeight
int cRenderDX::GetTextureHeight(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count()) {
		return -1; // Invalid texture ID
	}
	return s_Textures[TextureID].Height;
}

// cRenderDX::GetTextureDepth
int cRenderDX::GetTextureDepth(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count()) {
		return -1; // Invalid texture ID
	}
	return s_Textures[TextureID].Depth;
}

// cRenderDX::GetTextureFormat
const cFormat::Enum cRenderDX::GetTextureFormat(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count()) {
		return cFormat::fmtNone; // Invalid texture ID
	}
	return s_Textures[TextureID].Format;
}

// cRenderDX::GetTextureMipMapCount
int cRenderDX::GetTextureMipMapCount(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count()) {
		return 1;
	}
	return s_Textures[TextureID].MipMapCount;
}

// cRenderDX::GetTextureRenderTargetUsage
bool cRenderDX::GetTextureRenderTargetUsage(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count()) {
		return false; // Invalid texture ID
	}
	return s_Textures[TextureID].RenderTarget;
}

//*****************************************************************************
// cRenderDX_Format
//*****************************************************************************
static const D3DFORMAT cRenderDX_Format[cFormat::Count] = {
	D3DFMT_UNKNOWN,				// None

	D3DFMT_L8,					// R8
	D3DFMT_A8L8,				// Rg8
	D3DFMT_X8R8G8B8,			// Rgb8
	D3DFMT_A8R8G8B8,			// Rgba8

	D3DFMT_L16,					// R16
	D3DFMT_G16R16,				// Rg16
	D3DFMT_UNKNOWN,				// Rgb16 (not supported)
	D3DFMT_A16B16G16R16,		// Rgba16

	D3DFMT_R16F,				// R16f
	D3DFMT_G16R16F,				// Rg16f
	D3DFMT_UNKNOWN,				// Rgb16f (not supported)
	D3DFMT_A16B16G16R16F,		// Rgba16f

	D3DFMT_R32F,				// R32f
	D3DFMT_G32R32F,				// Rg32f
	D3DFMT_UNKNOWN,				// Rgb32f (not supported)
	D3DFMT_A32B32G32R32F,		// Rgba32f

	D3DFMT_D16,					// Depth16
	D3DFMT_D24X8,				// Depth24
	D3DFMT_D24S8,				// Depth24Stencil8
	
	D3DFMT_DXT1,				// Dxt1
	D3DFMT_DXT3,				// Dxt3
	D3DFMT_DXT5,				// Dxt5
	
	D3DFMT_UNKNOWN,				// PVRTC4
	D3DFMT_UNKNOWN				// PVRTC4_Alpha
}; // cRenderDX_Format

//-----------------------------------------------------------------------------------------------------------
// cRenderDX_CreateTexture
//-----------------------------------------------------------------------------------------------------------
static bool cRenderDX_CreateTexture(const cImage &SrcImage, cRenderDX_TEXTURE *To, cImage *SrcImageMutable) {
	cImage Temp;
	cImage *Buffer = (SrcImageMutable != nullptr) ? SrcImageMutable : &Temp;
	const cImage *SrcPtr = &SrcImage;

	bool NPOT = (!cMath::IsPowerOfTwo(SrcPtr->GetWidth()) || !cMath::IsPowerOfTwo(SrcPtr->GetHeight()));
	if(cRender::GetExpandTexturesToPowerOfTwo() && NPOT) {
		SrcPtr = Buffer;
		if(Buffer != SrcImageMutable) {
			Buffer->Copy(SrcImage);
		}
		if(!Buffer->MakePowerOfTwo()) {
			return false;
		}
	}
	
	To->Width = SrcPtr->GetWidth();
	To->Height = SrcPtr->GetHeight();
	To->Depth = SrcPtr->GetDepth();
	To->MipMapCount = SrcPtr->GetMipMapCount();
	To->CubeMap = (cDimension::Cube == SrcPtr->GetDimension());

	D3DFORMAT Format = cRenderDX_Format[SrcPtr->GetFormat()];
#ifdef COMMS_XBOX360
	Format = (D3DFORMAT)(Format & ~D3DFORMAT_TILED_MASK);
	Format = (D3DFORMAT)(Format & ~D3DFORMAT_ENDIAN_MASK);
#endif // COMMS_XBOX360
	if (cFormat::Rgb32f == SrcPtr->GetFormat()) { // Since D3D doesn't support "Rgb32f" we should add "X" channel
		if (SrcPtr != Buffer) {
			SrcPtr = Buffer;
			if (Buffer != SrcImageMutable) {
				Buffer->Copy(SrcImage);
			}
		}
		Buffer->ToFormat(cFormat::Rgba32f);
		Format = cRenderDX_Format[SrcPtr->GetFormat()];
	}
	if (cFormat::Rgb16f == SrcPtr->GetFormat()) { // Since D3D doesn't support "Rgb32f" we should add "X" channel
		if (SrcPtr != Buffer) {
			SrcPtr = Buffer;
			if (Buffer != SrcImageMutable) {
				Buffer->Copy(SrcImage);
			}
		}
		Buffer->ToFormat(cFormat::Rgba16f);
		Format = cRenderDX_Format[SrcPtr->GetFormat()];
	}
	if(cFormat::Rgb8 == SrcPtr->GetFormat()) { // Since D3D doesn't support "Rgb8" we should add "X" channel
		if(SrcPtr != Buffer) {
			SrcPtr = Buffer;
			if(Buffer != SrcImageMutable) {
				Buffer->Copy(SrcImage);
			}
		}
		Buffer->ToFormat(cFormat::Rgba8);
	}
	To->Format = SrcPtr->GetFormat();
	
#ifdef COMMS_XBOX360
	if(cFormat::Rgba8 == SrcPtr->GetFormat()) {
		Format = (D3DFORMAT)(Format & ~D3DFORMAT_SWIZZLEX_MASK);
		Format = (D3DFORMAT)(Format | ((GPUSWIZZLE_ABGR) << D3DFORMAT_SWIZZLEX_SHIFT));
	}
#endif // COMMS_XBOX360
	
	HRESULT hr;
	if(cDimension::Cube == SrcPtr->GetDimension()) {
		hr = s_Device->CreateCubeTexture(SrcPtr->GetWidth(), To->MipMapCount, 0, Format, D3DPOOL_MANAGED, (LPDIRECT3DCUBETEXTURE9 *)&To->Ptr, nullptr);
		cAssert(SUCCEEDED(hr));
		if(!SUCCEEDED(hr)) {
			return false;
		}
	} else if(cDimension::ThreeD == SrcPtr->GetDimension()) {
		hr = s_Device->CreateVolumeTexture(SrcPtr->GetWidth(), SrcPtr->GetHeight(), SrcPtr->GetDepth(), To->MipMapCount, 0, Format, D3DPOOL_MANAGED, (LPDIRECT3DVOLUMETEXTURE9 *)&To->Ptr, nullptr);
		cAssert(SUCCEEDED(hr));
		if(!SUCCEEDED(hr)) {
			return false;
		}
	} else {
		hr = s_Device->CreateTexture(SrcPtr->GetWidth(), SrcPtr->GetHeight(), To->MipMapCount, 0, Format, D3DPOOL_MANAGED, (LPDIRECT3DTEXTURE9 *)&To->Ptr, nullptr);
		cAssert(SUCCEEDED(hr));
		if(!SUCCEEDED(hr)) {
			return false;
		}
	}

	const byte *Src;
	int MipMapLevel = 0, Size, i;
	D3DLOCKED_RECT Rc;
	D3DLOCKED_BOX Box;
	
	int PackedLevel = cImage::MipMapsAll;
#ifdef COMMS_XBOX360
	byte *Dst, *Cur;
	int S, r, l, V;
	D3DMIPTAIL_DESC TailDesc;
	if(XGIsPackedTexture(To->Ptr)) {
		To->Ptr->GetTailDesc(&TailDesc);
		PackedLevel = TailDesc.BaseLevel;
	}
#endif // COMMS_XBOX360
	
	while(((Src = SrcPtr->GetPixels(MipMapLevel)) != nullptr) && (MipMapLevel < PackedLevel)) {
		Size = SrcPtr->GetMipMappedSize(MipMapLevel, 1);
		if(cDimension::Cube == SrcPtr->GetDimension()) {
			Size /= 6;
			for(i = 0; i < 6; i++) {
				hr = ((LPDIRECT3DCUBETEXTURE9)To->Ptr)->LockRect((D3DCUBEMAP_FACES)i, MipMapLevel, &Rc, nullptr, 0);
				if(SUCCEEDED(hr)) {
					memcpy(Rc.pBits, Src, Size);
#ifdef COMMS_WINDOWS
					if(cFormat::Rgba8 == SrcPtr->GetFormat()) {
						cMath::SwapChannels((byte *)Rc.pBits, SrcPtr->GetPixelCount(MipMapLevel, 1) / 6, 4, 0, 2);
					}
#endif // COMMS_WINDOWS
					((LPDIRECT3DCUBETEXTURE9)To->Ptr)->UnlockRect((D3DCUBEMAP_FACES)i, MipMapLevel);
				}
				Src += Size;
			}
		} else if(cDimension::ThreeD == SrcPtr->GetDimension()) {
			hr = ((LPDIRECT3DVOLUMETEXTURE9)To->Ptr)->LockBox(MipMapLevel, &Box, nullptr, 0);
			if(SUCCEEDED(hr)) {
#ifdef COMMS_XBOX360
				r = SrcPtr->GetRowSize(MipMapLevel);
				l = SrcPtr->GetSliceSize(MipMapLevel);
				if(r < Box.RowPitch || l < Box.SlicePitch) {
					Dst = (byte *)Box.pBits;
					V = 0;
					while(V < Size) {
						Cur = Dst;
						S = 0;
						while(S < l) {
							memcpy(Cur, Src, r);
							Src += r;
							Cur += Box.RowPitch;
							S += r;
						}
						Dst += Box.SlicePitch;
						V += l;
					}
				} else {
#endif // COMMS_XBOX360
					memcpy(Box.pBits, Src, Size);
#ifdef COMMS_WINDOWS
					if(cFormat::Rgba8 == SrcPtr->GetFormat()) {
						cMath::SwapChannels((byte *)Box.pBits, SrcPtr->GetPixelCount(MipMapLevel, 1), 4, 0, 2);
					}
#endif // COMMS_WINDOWS
#ifdef COMMS_XBOX360
				}
#endif // COMMS_XBOX360
				((LPDIRECT3DVOLUMETEXTURE9)To->Ptr)->UnlockBox(MipMapLevel);
			}
		} else {
			hr = ((LPDIRECT3DTEXTURE9)To->Ptr)->LockRect(MipMapLevel, &Rc, nullptr, 0);
			if(SUCCEEDED(hr)) {
#ifdef COMMS_XBOX360
				r = SrcPtr->GetRowSize(MipMapLevel);
				if(r < Rc.Pitch) {
					Dst = (byte *)Rc.pBits;
					S = 0;
					while(S < Size) {
						memcpy(Dst, Src, r);
						Dst += Rc.Pitch;
						Src += r;
						S += r;
					}
				} else {
#endif // COMMS_XBOX360
					memcpy(Rc.pBits, Src, Size);
#ifdef COMMS_WINDOWS
					if(cFormat::Rgba8 == SrcPtr->GetFormat()) {
						cMath::SwapChannels((byte *)Rc.pBits, SrcPtr->GetPixelCount(MipMapLevel, 1), 4, 0, 2);
					}
#endif // COMMS_WINDOWS
#ifdef COMMS_XBOX360
				}
#endif // COMMS_XBOX360
				((LPDIRECT3DTEXTURE9)To->Ptr)->UnlockRect(MipMapLevel);
			}
			
		}
		MipMapLevel++;
	}
#ifdef COMMS_XBOX360
	if(PackedLevel < cImage::MipMapsAll) {
		D3DLOCKED_TAIL Tail;
		((LPDIRECT3DTEXTURE9)To->Ptr)->LockTail(&Tail, 0);
		int Offset;
		while((Src = SrcPtr->GetPixels(MipMapLevel)) != nullptr) {
			Size = SrcPtr->GetMipMappedSize(MipMapLevel, 1);
			Offset = XGGetMipTailLevelOffset(To->Width, To->Height, To->Depth, MipMapLevel, (TailDesc.Format & D3DFORMAT_TEXTUREFORMAT_MASK), FALSE, FALSE);
			Dst = (byte *)Tail.pBits;
			Dst += Offset;
			r = SrcPtr->GetRowSize(MipMapLevel);
			S = 0;
			while(S < Size) {
				memcpy(Dst, Src, r);
				Dst += Tail.RowPitch;
				Src += r;
				S += r;
			}
			MipMapLevel++;
		}
		((LPDIRECT3DTEXTURE9)To->Ptr)->UnlockTail();
	}
#endif // COMMS_XBOX360
	
	return true;
} // cRenderDX_CreateTexture

//-----------------------------------------------------------------------------
// cRenderDX::GetTextureID
//-----------------------------------------------------------------------------
int cRenderDX::GetTextureID(const char *FilePn) {
	cAssert(FilePn != nullptr);
	if(cStr::Length(FilePn) < 1) {
		return -1; // Invalid texture file pathname
	}

	cStr P = cIO::EnsureAbsolutePath(FilePn);

	// Searching loaded textures
	int i;
	for(i = 0; i < s_Textures.Count(); i++) {
		const cRenderDX_TEXTURE &T = s_Textures[i];
		if(cStr::EqualsPath(T.FilePn, P)) { // Same file pathname
			return i;
		}
	}
	return AddTexture(FilePn);
} // cRenderDX::GetTextureID

// cRenderDX::ReloadTexture
bool cRenderDX::ReloadTexture(const int TextureID) {
	cImage Src;
	cRenderDX_TEXTURE T;
	if(TextureID >= 0 && TextureID < s_Textures.Count()) {
		cRenderDX_TEXTURE &r = s_Textures[TextureID];
		if(!r.FilePn.IsEmpty()) { // This is not render target or added as "cImage"
			if(cIO::LoadImage(r.FilePn, &Src)) {
				if(cRenderDX_CreateTexture(Src, &T, &Src)) {
					T.FilePn = r.FilePn; // Keep file pathname
					T.SamplerStateID = r.SamplerStateID; // Keep desired sampler state
					r.Free();
					r.Clear();
					r = T;
					return true;
				}
			}
		}
	}
	return false;
}

// cRenderDX::ReloadTextures
void cRenderDX::ReloadTextures() {
	int i, c = 0;
	cLog::Message(cStr(60, '-'));
	for(i = 0; i < s_Textures.Count(); i++) {
		if(ReloadTexture(i)) {
			c++;
		}
	}
	cStr S(60, '-');
	S += cStr::EndLn + "Reloaded " + cStr::ToString(c) + " textures.";
	cLog::Message(S);
}

// cRenderDX::ReloadTexture
void cRenderDX::ReloadTexture(const char *FilePn) {
	cStr F = cIO::EnsureAbsolutePath(FilePn);
	int i;
	for(i = 0; i < s_Textures.Count(); i++) {
		cRenderDX_TEXTURE &r = s_Textures[i];
		if(cStr::EqualsPath(r.FilePn, F)) {
			ReloadTexture(i);
		}
	}
}

// cRenderDX::UnloadTextures
void cRenderDX::UnloadTextures() {
	int i, c = 0;
	for(i = 0; i < s_Textures.Count(); i++) {
		cRenderDX_TEXTURE &r = s_Textures[i];
		if(r.FilePn.IsEmpty()) { // Render target or added as "cImage"
			continue;
		}
		r.Free();
		c++;
	}
	cLog::Message("Unloaded " + cStr::ToString(c) + " Textures.");
}

//-----------------------------------------------------------------------------
// cRenderDX::AddTexture : (const char *)
//-----------------------------------------------------------------------------
int cRenderDX::AddTexture(const char *FilePn) {
	cAssert(FilePn != nullptr);
	if(cStr::Length(FilePn) < 1) {
		return -1; // Invalid texture file pathname
	}
	// Load image
	cStr P = cIO::EnsureAbsolutePath(FilePn);
	cImage Src;
	if(!cIO::LoadImage(P, &Src)) {
		return -1; // Can't load or decode
	}
	static bool dd = false;
	if (dd){
		comms::cIO::SaveImage("test.dds", Src);
	}
	int ID = AddTexture(Src);
	if(ID >= 0 && ID < s_Textures.Count()) {
		s_Textures[ID].FilePn = P;
	}
	return ID;
} // cRenderDX::AddTexture : (const char *)

//-----------------------------------------------------------------------------
// cRenderDX::AddTexture : (const cImage &)
//-----------------------------------------------------------------------------
int cRenderDX::AddTexture(const cImage &SrcImage) {
	cRenderDX_TEXTURE T;
	if(!cRenderDX_CreateTexture(SrcImage, &T, nullptr)) {
		return -1;
	}
	
	int TextureID = -1;
	if(!s_TexturesFreeID.IsEmpty()) {
		TextureID = s_TexturesFreeID.GetLast();
		s_TexturesFreeID.RemoveLast();
		s_Textures[TextureID] = T;
		return TextureID;
	} else {
		return s_Textures.Add(T);
	}
} // cRenderDX::AddTexture : (const cImage &)

// cRenderDX::UpdateTexture
void cRenderDX::UpdateTexture(const int TextureID, const cList<cImage *> &SubImages, const cList<cRect> &Rects) {
#ifdef COMMS_WINDOWS
	if(TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return; // Invalid texture ID
	}
	cRenderDX_TEXTURE &T = s_Textures[TextureID];
	if(T.Depth != 1) {
		return; // Only 2D textures
	}
	if(!cFormat::IsPlain(T.Format)) {
		return; // Only plain formats
	}
	D3DLOCKED_RECT RC;
	HRESULT hr = ((LPDIRECT3DTEXTURE9)T.Ptr)->LockRect(0, &RC, nullptr, D3DLOCK_NO_DIRTY_UPDATE);
	if(!SUCCEEDED(hr)) {
		return;
	}
	int SizeOfPixel = cFormat::BytesPerPixel(T.Format);
	int i, X, Y, W, H, SrcPitch, j;
	const byte *Src = nullptr;
	byte *Dst = nullptr;
	RECT dr;
	for(i = 0; i < Rects.Count(); i++) {
		const cImage *I = SubImages[i];
		if(T.Format != I->GetFormat()) {
			continue; // Only the same format
		}
		const cRect &rc = Rects[i];
		X = (int)rc.GetLeft();
		Y = (int)rc.GetBottom();
		W = (int)rc.GetWidth();
		H = (int)rc.GetHeight();
		SrcPitch = W * SizeOfPixel;
		Src = I->GetPixels();
		Dst = (byte *)RC.pBits + Y * RC.Pitch + X * SizeOfPixel;
		for(j = 0; j < H; j++) {
			memcpy(Dst, Src, SrcPitch);
			if(cFormat::Rgba8 == T.Format) {
				cMath::SwapChannels(Dst, W, 4, 0, 2);
			}
			Src += SrcPitch;
			Dst += RC.Pitch;
		}
		dr.left = X;
		dr.right = X + W;
		dr.bottom = Y + H;
		dr.top = Y;
		((LPDIRECT3DTEXTURE9)T.Ptr)->AddDirtyRect(&dr);
	}
	((LPDIRECT3DTEXTURE9)T.Ptr)->UnlockRect(0);
#endif // COMMS_WINDOWS
}

//-----------------------------------------------------------------------------
// cRenderDX::FreeTexture
//-----------------------------------------------------------------------------
void cRenderDX::FreeTexture(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count()) {
		return; // Invalid texture ID
	}

	cRenderDX_TEXTURE &T = s_Textures[TextureID];
	T.Free();
	T.Clear();
	
	if(!s_TexturesFreeID.Contains(TextureID)) {
		s_TexturesFreeID.Add(TextureID);
	}
} // cRenderDX::FreeTexture

//-----------------------------------------------------------------------------
// cRenderDX::SetTexture
//-----------------------------------------------------------------------------
void cRenderDX::SetTexture(const int SamplerID, const int TextureID) {
	if(s_CurShaderID < 0 || s_CurShaderID >= s_Shaders.Count()) {
		return;
	}
	const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];

	if(SamplerID < 0 || SamplerID >= Sh.Samplers.Count()) {
		return;
	}
	const cRenderDX_SHADER::Sampler &Sm = Sh.Samplers[SamplerID];

	if(TextureID < 0 || TextureID >= s_Textures.Count()) {
		return;
	}
	const cRenderDX_TEXTURE &T = s_Textures[TextureID];
	
	cAssert(T.SamplerStateID != -1);
	if(T.SamplerStateID < 0 || T.SamplerStateID >= s_SamplerStates.Count()) {
		return;
	}
	cRenderDX_SAMPLER_STATE SS = s_SamplerStates[T.SamplerStateID];

	if(nullptr == T.Ptr) { // Unloaded texture
		cAssert(!T.FilePn.IsEmpty());
		ReloadTexture(TextureID);
	}

	s_Device->SetTexture(Sm.ImageUnit, T.Ptr);
	int S = sizeof(s_SampledTextures) / sizeof(s_SampledTextures[0]);
	cAssert(Sm.ImageUnit >= 0 && Sm.ImageUnit < S);
	s_SampledTextures[Sm.ImageUnit] = T.Ptr;
	
	if(SS.MipMaps && T.MipMapCount <= 1) {
		// Sampler state will use mip maps, but the texture doesn't have them.
		// Therefor we should degrade the filter to "Linear".
		SS.MinFilter = D3DTEXF_LINEAR;
		SS.MagFilter = D3DTEXF_LINEAR;
		SS.MipFilter = D3DTEXF_NONE;
		SS.Aniso = false;
	}
	
	s_Device->SetSamplerState(Sm.ImageUnit, D3DSAMP_MINFILTER, SS.MinFilter);
	s_Device->SetSamplerState(Sm.ImageUnit, D3DSAMP_MAGFILTER, SS.MagFilter);
	s_Device->SetSamplerState(Sm.ImageUnit, D3DSAMP_MIPFILTER, SS.MipFilter);
	s_Device->SetSamplerState(Sm.ImageUnit, D3DSAMP_ADDRESSU, SS.S);
	s_Device->SetSamplerState(Sm.ImageUnit, D3DSAMP_ADDRESSV, SS.T);
	s_Device->SetSamplerState(Sm.ImageUnit, D3DSAMP_ADDRESSW, SS.R);
	int Aniso = SS.Aniso ? s_Caps.MaxAnisotropy : 1;
	s_Device->SetSamplerState(Sm.ImageUnit, D3DSAMP_MAXANISOTROPY, Aniso);
} // cRenderDX::SetTexture

//*****************************************************************************
// cRenderDX_TOPOLOGY
//*****************************************************************************
static const D3DPRIMITIVETYPE cRenderDX_TOPOLOGY[cTopology::Count] = {
	D3DPT_POINTLIST,		// cTopology::PointList
	D3DPT_LINELIST,			// cTopology::LineList
	D3DPT_LINESTRIP,		// cTopology::LineStrip
	D3DPT_TRIANGLELIST,		// cTopology::TriangleList
	D3DPT_TRIANGLESTRIP		// cTopology::TriangleStrip
}; // cRenderDX_TOPOLOGY

//-----------------------------------------------------------------------------
// cRenderDX::DrawArrays : (..., const int, ...)
//-----------------------------------------------------------------------------
void cRenderDX::DrawArrays(const cTopology::Enum Topology) {
	cAssertM(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count(), "Set shader before drawing");
	if(s_CurShaderID < 0 || s_CurShaderID >= s_Shaders.Count()) {
		return;
	}
	const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
	cAssert(Sh.VertexFormatID == s_CurVertexFormatID);

	cAssertM(s_CurVertexBufferID >= 0 && s_CurVertexBufferID < s_VertexBuffers.Count(), "Set vertex buffer before drawing");
	if(s_CurVertexBufferID < 0 || s_CurVertexBufferID >= s_VertexBuffers.Count()) {
		return;
	}

	SetIndexBuffer(-1);

	const cRenderDX_VERTEX_BUFFER &vb = s_VertexBuffers[s_CurVertexBufferID];
	const cRenderDX_VERTEX_FORMAT &vf = s_VertexFormats[s_CurVertexFormatID];
	int VertexCount = (int)(vb.Size / vf.VertexSize);
	int NPrims = cTopology::PrimitiveCount(Topology, VertexCount);
	s_Device->DrawPrimitive(cRenderDX_TOPOLOGY[Topology], 0, NPrims);
} // cRenderDX::DrawArrays : (..., const int, ...)

//---------------------------------------------------------------------------------------------------------
// cRenderDX::DrawArrays : (..., const void *, ...)
//---------------------------------------------------------------------------------------------------------
void cRenderDX::DrawArrays(const cTopology::Enum Topology, const void *VertexData, const int VertexCount) {
	cAssertM(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count(), "Set shader before drawing");
	if(s_CurShaderID < 0 || s_CurShaderID >= s_Shaders.Count()) {
		return;
	}
	const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
	cAssert(Sh.VertexFormatID == s_CurVertexFormatID);
	const cRenderDX_VERTEX_FORMAT &vf = s_VertexFormats[s_CurVertexFormatID];

	SetVertexBuffer(-1);
	SetIndexBuffer(-1);

	int NPrims = cTopology::PrimitiveCount(Topology, VertexCount);
	s_Device->DrawPrimitiveUP(cRenderDX_TOPOLOGY[Topology], NPrims, VertexData, vf.VertexSize);
} // cRenderDX::DrawArrays : (..., const void *, ...)

//-----------------------------------------------------------------------------
// cRenderDX::DrawIndexed : (..., const int, ...)
//-----------------------------------------------------------------------------
void cRenderDX::DrawIndexed(const cTopology::Enum Topology) {
	cAssertM(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count(), "Set shader before drawing");
	if(s_CurShaderID < 0 || s_CurShaderID >= s_Shaders.Count()) {
		return;
	}
	const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
	cAssert(Sh.VertexFormatID == s_CurVertexFormatID);
	const cRenderDX_VERTEX_FORMAT &vf = s_VertexFormats[s_CurVertexFormatID];

	cAssertM(s_CurVertexBufferID >= 0 && s_CurVertexBufferID < s_VertexBuffers.Count(), "Set vertex buffer before drawing");
	if(s_CurVertexBufferID < 0 || s_CurVertexBufferID >= s_VertexBuffers.Count()) {
		return;
	}
	const cRenderDX_VERTEX_BUFFER &vb = s_VertexBuffers[s_CurVertexBufferID];
	int VertexCount = (int)(vb.Size / vf.VertexSize);
	
	cAssertM(s_CurIndexBufferID >= 0 && s_CurIndexBufferID < s_IndexBuffers.Count(), "Set index buffer before drawing");
	if(s_CurIndexBufferID < 0 || s_CurIndexBufferID >= s_IndexBuffers.Count()) {
		return;
	}
	const cRenderDX_INDEX_BUFFER &ib = s_IndexBuffers[s_CurIndexBufferID];
	int NPrims = cTopology::PrimitiveCount(Topology, ib.IndexCount);
	s_Device->DrawIndexedPrimitive(cRenderDX_TOPOLOGY[Topology], 0, 0, VertexCount, 0, NPrims);
} // cRenderDX::DrawIndexed : (..., const int, ...)

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// cRenderDX::DrawIndexed : (..., const void *, ...)
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void cRenderDX::DrawIndexed(const cTopology::Enum Topology, const void *VertexData, const int VertexCount, const void *IndexData, const int IndexCount, const int IndexSize) {
	cAssertM(s_CurShaderID >= 0 && s_CurShaderID < s_Shaders.Count(), "Set shader before drawing");
	if(s_CurShaderID < 0 || s_CurShaderID >= s_Shaders.Count()) {
		return;
	}
	const cRenderDX_SHADER &Sh = s_Shaders[s_CurShaderID];
	cAssert(Sh.VertexFormatID == s_CurVertexFormatID);

	SetIndexBuffer(-1);
	SetVertexBuffer(-1);

	const cRenderDX_VERTEX_FORMAT &vf = s_VertexFormats[s_CurVertexFormatID];
	int NPrims = cTopology::PrimitiveCount(Topology, IndexCount);
	s_Device->DrawIndexedPrimitiveUP(cRenderDX_TOPOLOGY[Topology], 0, VertexCount, NPrims, IndexData, 2 == IndexSize ? D3DFMT_INDEX16 : D3DFMT_INDEX32, VertexData, vf.VertexSize);
} // cRenderDX::DrawIndexed : (..., const void *, ...)

//-----------------------------------------------------------------------------------------------------------
// cRenderDX::Clear
//-----------------------------------------------------------------------------------------------------------
void cRenderDX::Clear(const bool ClearColor, const bool ClearDepth, const cColor &Color, const float Depth) {
	dword Flags = 0, C = 0;
	if(ClearColor) {
		Flags |= D3DCLEAR_TARGET;
		C = Color.ToDword();
#ifdef COMMS_WINDOWS
		cMath::Swap(((byte *)&C)[0], ((byte *)&C)[2]);
#endif // COMMS_WINDOWS
#ifdef COMMS_XBOX360
		cMath::Swap(((byte *)&C)[1], ((byte *)&C)[3]);
#endif // COMMS_XBOX360
	}
	if(ClearDepth) {
		Flags |= D3DCLEAR_ZBUFFER;
	}
	s_Device->Clear(0, nullptr, Flags, C, Depth, 0);
} // cRenderDX::Clear

//-----------------------------------------------------------------------------
// cRenderDX::SetViewport
//-----------------------------------------------------------------------------
void cRenderDX::SetViewport(const cRect &Viewport) {
	IDirect3DSurface9 *CurRT = nullptr;
	D3DVIEWPORT9 vp;
	D3DSURFACE_DESC Desc;
	HRESULT hr = s_Device->GetRenderTarget(0, &CurRT);
	cAssert(SUCCEEDED(hr));
	if (CurRT && SUCCEEDED(hr)){
		CurRT->GetDesc(&Desc);
		CurRT->Release();
		CurRT = nullptr;

		vp.X = (DWORD)Viewport.GetLeft();
		vp.Y = Desc.Height - (DWORD)Viewport.GetBottom() - (DWORD)Viewport.GetHeight();
		vp.Width = (DWORD)Viewport.GetWidth();
		vp.Height = (DWORD)Viewport.GetHeight();
		vp.MinZ = 0.0f;
		vp.MaxZ = 1.0f;
		s_Device->SetViewport(&vp);
	}
} // cRenderDX::SetViewport

// cRenderDX::GetVendor
const cStr & cRenderDX::GetVendor() const {
	return s_Vendor;
}

// cRenderDX::GetMRTCount
int cRenderDX::GetMRTCount() {
	return s_Caps.NumSimultaneousRTs;
}

// cRenderDX::SupportsFormat
bool cRenderDX::SupportsFormat(const cFormat::Enum Format) {
	DWORD Usage;
	D3DRESOURCETYPE Type;
	if(cFormat::IsDepth(Format)) {
		Usage = D3DUSAGE_DEPTHSTENCIL;
		Type = D3DRTYPE_SURFACE;
	} else {
		Usage = 0;
		Type = D3DRTYPE_TEXTURE;
	}
	HRESULT hr = s_D3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, s_PP.BackBufferFormat, Usage, Type, cRenderDX_Format[Format]);
	return SUCCEEDED(hr);
}

// cRenderDX::SupportsNonPowerOfTwo
bool cRenderDX::SupportsNonPowerOfTwo() {
	bool ReqPow2 = ((s_Caps.TextureCaps & D3DPTEXTURECAPS_POW2) != 0);
	if(!ReqPow2) {
		return true;
	}
	bool CondSupp = ((s_Caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) != 0);
	return CondSupp;
}

// cRenderDX_Surface2Image
static bool cRenderDX_Surface2Image(IDirect3DSurface9 *pSurface, cImage *pImage, const cFormat::Enum Format) {
	bool r = false;
	D3DSURFACE_DESC SD;
	pSurface->GetDesc(&SD);
	IDirect3DSurface9 *PS = nullptr;
	HRESULT hr = s_Device->CreateOffscreenPlainSurface(SD.Width, SD.Height, SD.Format, D3DPOOL_SYSTEMMEM, &PS, nullptr);
	if (SUCCEEDED(hr)) {
		hr = s_Device->GetRenderTargetData(pSurface, PS);
		if (SUCCEEDED(hr)) {
			D3DLOCKED_RECT lr;
			hr = PS->LockRect(&lr, nullptr, D3DLOCK_READONLY);
			if (SUCCEEDED(hr)) {
				pImage->Create(Format, SD.Width, SD.Height, 1, 1);
				memcpy(pImage->GetPixels(), lr.pBits, cFormat::BytesPerPixel(Format) * SD.Width * SD.Height);
				PS->UnlockRect();
				pImage->SwapChannels(0, 2);
				pImage->Flip();
				r = true;
			}

		}
		PS->Release(); PS = nullptr;
	}
	return r;
}

//-----------------------------------------------------------------------------
// cRenderDX::ScreenShot
//-----------------------------------------------------------------------------
bool cRenderDX::ScreenShot(cImage *To) {
	bool r = false;
	IDirect3DSurface9 *pBackBuffer = nullptr;
	HRESULT hr = s_Device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);
	if (SUCCEEDED(hr)) {
		r = cRenderDX_Surface2Image(pBackBuffer, To, cFormat::Rgba8);
		pBackBuffer->Release(); pBackBuffer = nullptr;
	}
	return r;
} // cRenderDX::ScreenShot

bool cRenderDX::RequestSaveRenderTargetAsyncRaw(const int RenderTargetID, cRender::SaveRenderTargetArgs *Args) {
    return false;
}
bool cRenderDX::GetSaveRenderTargetAsyncRaw(const int RenderTargetID, cRender::SaveRenderTargetArgs *Args) {
    return false;
}
void cRenderDX::FreeSaveRenderTargetAsyncRaw(const int RenderTargetID) {
}
bool cRenderDX::IsSaveRenderTargetAsyncReadyRaw(const int RenderTargetID) {
    return false;
}

//-----------------------------------------------------------------------------
// cRenderDX::SaveRenderTarget
//-----------------------------------------------------------------------------
bool cRenderDX::SaveRenderTarget(const int RenderTargetID, cImage *To) {
	if(RenderTargetID < 0 || RenderTargetID >= s_Textures.Count()) {
		return false;
	}
	cRenderDX_TEXTURE &T = s_Textures[RenderTargetID];
	if(!T.RenderTarget || nullptr == T.Surfaces || nullptr == T.Surfaces[0]) {
		return false;
	}
	return cRenderDX_Surface2Image(T.Surfaces[0], To, T.Format);
} // cRenderDX::SaveRenderTarget

// cRenderDX::GetDeviceRecentReset
bool cRenderDX::GetDeviceRecentReset() {
	return s_RecentReset;
}

//-----------------------------------------------------------------------------
// CreateRenderTarget
//-----------------------------------------------------------------------------
static bool CreateRenderTarget(cRenderDX_TEXTURE *T) {
	HRESULT hr;
	int i;
#ifdef COMMS_XBOX360
	D3DSURFACE_PARAMETERS S;
	memset(&S, 0, sizeof(S));
	D3DMULTISAMPLE_TYPE Samples = (2 == T->Samples ? D3DMULTISAMPLE_2_SAMPLES : (4 == T->Samples ? D3DMULTISAMPLE_4_SAMPLES : D3DMULTISAMPLE_NONE));
#endif // COMMS_XBOX360
	
	if(cFormat::IsDepth(T->Format)) {
		if(nullptr == T->Surfaces) {
			T->Surfaces = new LPDIRECT3DSURFACE9;
		}
#ifdef COMMS_WINDOWS
		hr = s_Device->CreateDepthStencilSurface(T->Width, T->Height, cRenderDX_Format[T->Format], D3DMULTISAMPLE_NONE, 0, FALSE, T->Surfaces, nullptr);
#endif // COMMS_WINDOWS
#ifdef COMMS_XBOX360
		S.Base = T->DepthBase;
		hr = s_Device->CreateDepthStencilSurface(T->Width, T->Height, cRenderDX_Format[T->Format], Samples, 0, FALSE, T->Surfaces, &S);
#endif // COMMS_XBOX360
		cAssert(SUCCEEDED(hr));
		if(!SUCCEEDED(hr)) {
			delete T->Surfaces;
			T->Surfaces = nullptr;
			return false;
		}
	} else {
		if(T->CubeMap) {
			hr = s_Device->CreateCubeTexture(T->Width, 1, D3DUSAGE_RENDERTARGET, cRenderDX_Format[T->Format], D3DPOOL_DEFAULT, (LPDIRECT3DCUBETEXTURE9 *)&T->Ptr, nullptr);
			cAssert(SUCCEEDED(hr));
			if(!SUCCEEDED(hr)) {
				return false;
			}
			if(nullptr == T->Surfaces) {
				T->Surfaces = new LPDIRECT3DSURFACE9[6];
			}
			for(i = 0; i < 6; i++) {
				((LPDIRECT3DCUBETEXTURE9)T->Ptr)->GetCubeMapSurface((D3DCUBEMAP_FACES)i, 0, &T->Surfaces[i]);
			}
		} else {
			D3DFORMAT Format = cRenderDX_Format[T->Format];
			hr = s_Device->CreateTexture(T->Width, T->Height, 1, D3DUSAGE_RENDERTARGET, Format, D3DPOOL_DEFAULT, (LPDIRECT3DTEXTURE9 *)&T->Ptr, nullptr);
			cAssert(SUCCEEDED(hr));
			if(!SUCCEEDED(hr)) {
				return false;
			}
			if(nullptr == T->Surfaces) {
				T->Surfaces = new LPDIRECT3DSURFACE9;
			}
#ifdef COMMS_WINDOWS
			((LPDIRECT3DTEXTURE9)T->Ptr)->GetSurfaceLevel(0, T->Surfaces);
#endif // COMMS_WINDOWS
#ifdef COMMS_XBOX360
			T->TileWidth = T->Width;
			T->TileHeight = T->Height;
			UINT c = XGSurfaceSize(T->Width, T->Height, cRenderDX_Format[T->Format], Samples); // Color
			UINT d = XGSurfaceSize(T->Width, T->Height, cRenderDX_Format[cFormat::Depth24Stencil8], Samples); // Depth
			UINT Q = c + d;
			if(Q > GPU_EDRAM_TILES) {
				D3DRECT rc;
				if(1280 == T->Width && 720 == T->Height && 4 == T->Samples) {
					rc.x1 = 0;
					rc.y1 = 0;
					rc.x2 = 1280;
					rc.y2 = 256;
					T->Tiles.Add(rc);

					rc.y1 = 256;
					rc.y2 = 512;
					T->Tiles.Add(rc);

					rc.y1 = 512;
					rc.y2 = 720;
					T->Tiles.Add(rc);
					T->TileHeight = 256;
				} else {
					int t = (Q + GPU_EDRAM_TILES - 1) / GPU_EDRAM_TILES;
					T->TileWidth /= t;
					int i;
					rc.y1 = 0;
					rc.y2 = T->Height;
					for(i = 0; i < t; i++) {
						rc.x1 = i * T->TileWidth;
						rc.x2 = (i + 1) * T->TileWidth;
						T->Tiles.Add(rc);
					}
				}
			}
			hr = s_Device->CreateRenderTarget(T->TileWidth, T->TileHeight, Format, Samples, 0, FALSE, T->Surfaces, &S);
			cAssert(SUCCEEDED(hr));
			if(!SUCCEEDED(hr)) {
				delete T->Surfaces;
				T->Surfaces = nullptr;
				T->Ptr->Release();
				T->Ptr = nullptr;
				return false;
			}
#endif // COMMS_XBOX360
		}
	}
	return true;
} // CreateRenderTarget

//-----------------------------------------------------------------------------
// ResetDevice
//-----------------------------------------------------------------------------
static void ResetDevice() {
	int i, c, j;
	for(i = 0; i < s_Textures.Count(); i++) {
		cRenderDX_TEXTURE &T = s_Textures[i];
		if(T.Surfaces != nullptr) { // Render target or render depth
			cAssert(T.RenderTarget || cFormat::IsDepth(T.Format));
			c = T.CubeMap ? 6 : 1;
			for(j = 0; j < c; j++) {
				T.Surfaces[j]->Release();
			}
			if(T.Ptr != nullptr) {
				T.Ptr->Release();
				T.Ptr = nullptr;
			}
		}
	}

	FreeFrameBufferSurfaces();

	HRESULT hr = s_Device->Reset(&s_PP);
	cAssert(SUCCEEDED(hr));
	if(!SUCCEEDED(hr)) {
		return;
	}

	s_Device->GetRenderTarget(0, &s_FrameBufferColor);
	s_Device->GetDepthStencilSurface(&s_FrameBufferDepth);

	for(i = 0; i < s_Textures.Count(); i++) {
		cRenderDX_TEXTURE &T = s_Textures[i];
		if(T.Surfaces != nullptr) {
			CreateRenderTarget(&T);
		}
	}

	cRender::ResetStates();
	s_CurVertexFormatID = -1;
	s_CurVertexBufferID = -1;
	s_CurIndexBufferID = -1;
	s_CurShaderID = -1;
	s_RecentReset = true;
} // ResetDevice

//-----------------------------------------------------------------------------
// cRenderDX::BeginFrame
//-----------------------------------------------------------------------------
void cRenderDX::BeginFrame() {
	//*************************************************************************
	// Handle possible window client size change
	//*************************************************************************
#ifdef COMMS_WINDOWS
	RECT rc;
	GetClientRect(s_PP.hDeviceWindow, &rc);
	UINT Width = rc.right - rc.left;
	UINT Height = rc.bottom - rc.top;
	if(0 == Width || 0 == Height) {
		return;
	}
	if(Width != s_ClientWidth || Height != s_ClientHeight) {
		s_PP.BackBufferWidth = s_ClientWidth = Width;
		s_PP.BackBufferHeight = s_ClientHeight = Height;
		ResetDevice();
		Sleep(100);
	}
#endif // COMMS_WINDOWS
	if(s_VSync != cSettings::GetInstance()->VSync) {
		s_VSync = cSettings::GetInstance()->VSync;
		s_PP.PresentationInterval = s_VSync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
		ResetDevice();
	}
	s_Device->BeginScene();
} // cRenderDX::BeginFrame

// cRenderDX::EndFrame
void cRenderDX::EndFrame() {
	s_Device->EndScene();
	HRESULT hr = s_Device->Present(nullptr, nullptr, nullptr, nullptr);
	s_RecentReset = false;
#ifdef COMMS_WINDOWS
	if(D3DERR_DEVICELOST == hr) {
		hr = s_Device->TestCooperativeLevel();
		if(D3DERR_DEVICENOTRESET == hr) {
			ResetDevice();
			s_RecentReset = true;
		}
	}
#endif // COMMS_WINDOWS
}

// cRenderDX::Finish
void cRenderDX::Finish() {
	IDirect3DQuery9 *E = nullptr;
	s_Device->CreateQuery(D3DQUERYTYPE_EVENT, &E);
	E->Issue(D3DISSUE_END);
	while(S_FALSE == E->GetData(nullptr, 0, D3DGETDATA_FLUSH)) {}
	E->Release(); E = nullptr;
}

//------------------------------------------------------------------------------------------------------------------------------------
// cRenderDX::AddRenderTarget
//------------------------------------------------------------------------------------------------------------------------------------
int cRenderDX::AddRenderTarget(const int Width, const int Height, const cFormat::Enum Format, const bool CubeMap, const int Samples) {
	if(cFormat::IsCompressed(Format)) {
		return -1;
	}

	cRenderDX_TEXTURE T;
	T.Format = Format;
	T.Width = Width;
	T.Height = Height;
	T.Depth = CubeMap ? 0 : 1;
	T.CubeMap = CubeMap;
	T.RenderTarget = true;
	T.Samples = Samples;

	if(!CreateRenderTarget(&T)) {
		return -1;
	}
	
	int ID = -1;
	if(!s_TexturesFreeID.IsEmpty()) {
		ID = s_TexturesFreeID.GetLast();
		s_TexturesFreeID.RemoveLast();
		s_Textures[ID] = T;
	} else {
		ID = s_Textures.Add(T);
	}
	return ID;
} // cRenderDX::AddRenderTarget

//-----------------------------------------------------------------------------------
// cRenderDX::AddRenderDepth
//-----------------------------------------------------------------------------------
int cRenderDX::AddRenderDepth(const int RenderTargetID, const cFormat::Enum Format) {
	cAssert(cFormat::IsDepth(Format));
	if(!cFormat::IsDepth(Format)) {
		return -1;
	}
	cAssert(RenderTargetID >= 0 && RenderTargetID < s_Textures.Count());
	if(RenderTargetID < 0 || RenderTargetID >= s_Textures.Count()) {
		return -1;
	}
	const cRenderDX_TEXTURE &R = s_Textures[RenderTargetID];
	cAssert(R.RenderTarget);
	if(!R.RenderTarget) {
		return -1;
	}
	cRenderDX_TEXTURE T;
	T.Format = Format;
	T.Width = R.Width;
	T.Height = R.Height;
	T.Samples = R.Samples;
#ifdef COMMS_XBOX360
	if(!R.Tiles.IsEmpty()) {
		T.Width = R.TileWidth;
		T.Height = R.TileHeight;
	}
	D3DMULTISAMPLE_TYPE Samples = (2 == T.Samples ? D3DMULTISAMPLE_2_SAMPLES : (4 == T.Samples ? D3DMULTISAMPLE_4_SAMPLES : D3DMULTISAMPLE_NONE));
	T.DepthBase = XGSurfaceSize(T.Width, T.Height, cRenderDX_Format[R.Format], Samples);
#endif // COMMS_XBOX360
	
	if(!CreateRenderTarget(&T)) {
		return -1;
	}

	int ID = -1;
	if(!s_TexturesFreeID.IsEmpty()) {
		ID = s_TexturesFreeID.GetLast();
		s_TexturesFreeID.RemoveLast();
		s_Textures[ID] = T;
	} else {
		ID = s_Textures.Add(T);
	}
	return ID;
} // cRenderDX::AddRenderDepth

//------------------------------------------------------------------------------------------------
// cRenderDX::SetRenderTargetSize
//------------------------------------------------------------------------------------------------
void cRenderDX::SetRenderTargetSize(const int RenderTargetID, const int Width, const int Height) {
	cAssert(RenderTargetID >= 0 && RenderTargetID < s_Textures.Count());
	if(RenderTargetID < 0 || RenderTargetID >= s_Textures.Count()) {
		return;
	}

	cRenderDX_TEXTURE &T = s_Textures[RenderTargetID];
	if(Width == T.Width && Height == T.Height) {
		return; // Same size
	}

	T.Free();

	T.Width = Width;
	T.Height = Height;
	CreateRenderTarget(&T);
} // cRenderDX::SetRenderTargetSize

#ifdef COMMS_XBOX360
static cRenderDX_TEXTURE *s_ResolveTexture = nullptr;
#endif // COMMS_XBOX360

//----------------------------------------------------------------------------------------------------------------------------------------------------
// cRenderDX::SetRenderTargets
//----------------------------------------------------------------------------------------------------------------------------------------------------
void cRenderDX::SetRenderTargets(const int *ColorRenderTargetIDs, const int ColorRenderTargetCount, const int DepthRenderTargetID, const int *Faces) {
	int i;
	HRESULT hr;

	if(nullptr == ColorRenderTargetIDs) {
#ifdef COMMS_XBOX360
		if(s_ResolveTexture != nullptr) {
			if(!s_ResolveTexture->Tiles.IsEmpty()) {
				DWORD U, R;
				s_Device->QueryBufferSpace(&U, &R);
				s_Device->EndTiling(D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS, nullptr, s_ResolveTexture->Ptr, nullptr, 1.0f, 0, nullptr);
			} else {
				s_Device->Resolve(D3DRESOLVE_RENDERTARGET0, nullptr, s_ResolveTexture->Ptr, nullptr, 0, 0, nullptr, 0, 0, nullptr);
			}
			s_ResolveTexture = nullptr;
		}
#endif // COMMS_XBOX360
		hr = s_Device->SetRenderTarget(0, s_FrameBufferColor);
		cAssert(SUCCEEDED(hr));
		for(i = 1; i < (int)s_Caps.NumSimultaneousRTs; i++) {
			hr = s_Device->SetRenderTarget(i, nullptr);
			cAssert(SUCCEEDED(hr));
		}
		hr = s_Device->SetDepthStencilSurface(s_FrameBufferDepth);
		cAssert(SUCCEEDED(hr));
		return;
	}

	int ColorRT;
	for(i = 0; i < ColorRenderTargetCount; i++) {
		ColorRT = ColorRenderTargetIDs[i];
		cAssert(ColorRT >= 0 && ColorRT < s_Textures.Count());
		if(ColorRT < 0 || ColorRT >= s_Textures.Count()) {
			return;
		}
		cRenderDX_TEXTURE &T = s_Textures[ColorRT];
		if(T.CubeMap) {
			cAssert(Faces != nullptr);
			if(nullptr == Faces) {
				return;
			}
			hr = s_Device->SetRenderTarget(i, T.Surfaces[Faces[i]]);
			cAssert(SUCCEEDED(hr));
		} else {
			hr = s_Device->SetRenderTarget(i, T.Surfaces[0]);
			cAssert(SUCCEEDED(hr));
#ifdef COMMS_XBOX360
			s_ResolveTexture = &T;
#endif // COMMS_XBOX360
		}
	}
	
	for(i = ColorRenderTargetCount; i < (int)s_Caps.NumSimultaneousRTs; i++) {
		hr = s_Device->SetRenderTarget(i, nullptr);
		cAssert(SUCCEEDED(hr));
	}

	if(-1 == DepthRenderTargetID) {
		hr = s_Device->SetDepthStencilSurface(nullptr);
		cAssert(SUCCEEDED(hr));
	} else {
		cAssert(DepthRenderTargetID >= 0 && DepthRenderTargetID < s_Textures.Count());
		if(DepthRenderTargetID < 0 || DepthRenderTargetID >= s_Textures.Count()) {
			return;
		}
		cRenderDX_TEXTURE &D = s_Textures[DepthRenderTargetID];
		hr = s_Device->SetDepthStencilSurface(D.Surfaces[0]);
		cAssert(SUCCEEDED(hr));
	}

#ifdef COMMS_XBOX360
	if(s_ResolveTexture != nullptr && !s_ResolveTexture->Tiles.IsEmpty()) {
		s_Device->BeginTiling(D3DTILING_SKIP_FIRST_TILE_CLEAR, s_ResolveTexture->Tiles.Count(), s_ResolveTexture->Tiles.ToPtr(), nullptr, 1.0f, 0);
	}
#endif // COMMS_XBOX360
} // cRenderDX::SetRenderTargets

#endif // COMMS_DIRECTX10

} // comms

#endif // COMMS_DIRECTX
