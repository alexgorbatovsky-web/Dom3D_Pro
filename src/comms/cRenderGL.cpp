#include <map>

#include "comms.h"
#ifdef COMMS_3DCOAT
#include "../ClassEngine/BaseClassIO.h"
#include "../ClassEngine/bigdynarray.h"
#include <assert.h>
#include "../ClassEngine/bitset.h"
#endif // COMMS_3DCOAT

#ifdef COMMS_OPENGL

#ifdef COMMS_WINDOWS
extern "C" {
    // Enable an NVIDIA/AMD high performance graphics processor
	_declspec(dllexport) DWORD NvOptimusEnablement = 1;
	_declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#include <gl/gl.h>
#pragma comment (lib, "opengl32.lib")
HWND cWinMain_GetWindow();
#include "glext.h"
#endif // Windows

#ifdef COMMS_MACOS
#import <OpenGL/gl3.h>
#import <OpenGL/gl3ext.h>
#endif // macOS

#ifdef COMMS_3DCOAT
void LogNewShader(const char* name,int VType,const char* Extra);
#endif // COMMS_3DCOAT

#ifdef COMMS_LINUX
#include <X11/Xlib.h>
#undef None
#undef Success
#define GL_GLEXT_PROTOTYPES
#include <GL/glx.h>
#include <gdk/gdkx.h>
Display * cLinuxMain_GetDisplay();
GLXDrawable cLinuxMain_GetDrawable();
#endif // COMMS_LINUX

namespace comms {

//*****************************************************************************
// cRenderGL
//*****************************************************************************
class cRenderGL : public cRender::Stub {
public:
	const cRenderType::Enum GetType() {
		return cRenderType::OpenGL;
	}

	bool Init(const int MaxSamples);
	void Free();

	void SetViewport(const cRect &Viewport);
	void SetClipRect(const cRect &ClipRect);
	
	void Clear(const bool ClearColor, const bool ClearDepth, const cColor &Color, const float Depth);
	
	void BeginFrame();
	void EndFrame();

	void Finish();

	const cStr& GetVendor() const;
	const cStr& GetVersion() const;
	const cStr& GetRenderer() const;
	int GetMRTCount();
	bool SupportsFormat(const cFormat::Enum Format);
	bool SupportsNonPowerOfTwo();
	bool ScreenShot(cImage *To);
	
	bool GetDeviceRecentReset() {
		return false;
	}

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
	void UpdateTexture(const int TextureID, const cImage& Image);
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
    bool SaveRenderTargetRaw(const int RenderTargetID, cRender::SaveRenderTargetArgs *Args);

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

    bool RequestSaveRenderTargetAsyncRaw(const int RenderTargetID, cRender::SaveRenderTargetArgs *Args);
    bool GetSaveRenderTargetAsyncRaw(const int RenderTargetID, cRender::SaveRenderTargetArgs *Args);
    void FreeSaveRenderTargetAsyncRaw(const int RenderTargetID);
    bool IsSaveRenderTargetAsyncReadyRaw(const int RenderTargetID);

	// Bindless texture
	bool IsSupported_BindlessTexture();

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
}; // cRenderGL

// These functions use pixel dimensions: glViewport, glScissor, glReadPixels, glLineWidth, glRenderbufferStorage, glTexImage2D
// https://developer.apple.com/library/mac/documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/CapturingScreenContents/CapturingScreenContents.html

// cRender_CreateGL
cRender::Stub * cRender_CreateGL() {
	return new cRenderGL;
}

//*****************************************************************************
// OpenGL Extensions
//*****************************************************************************

#ifdef COMMS_WINDOWS
	
// WGL_ARB_extensions_string
typedef const char * (WINAPI * PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC Hdc);
PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = nullptr;

// WGL_EXT_swap_control
typedef BOOL (WINAPI * PFNWGLSWAPINTERVALPROC)(int);
PFNWGLSWAPINTERVALPROC wglSwapIntervalEXT = nullptr;

// WGL_ARB_pixel_format
typedef BOOL (WINAPI * PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC Hdc, const int *AttribsI, const FLOAT *AttribsF, UINT MaxFormats, int *Formats, UINT *Count);
typedef BOOL (WINAPI * PFNWGLGETPIXELFORMATATTRIBIVARBPROC)(HDC Hdc, int PixelFormat, int LayerPlane, UINT Count, const int *Attribs, int *Values);
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = nullptr;
PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB = nullptr;

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

typedef unsigned int GLhandleARB;
typedef ptrdiff_t GLsizeiptrARB;
typedef char GLcharARB;

#define GL_RENDERBUFFER_EXT						0x8D41
#define GL_MULTISAMPLE_ARB						0x809D
#define GL_COLOR_ATTACHMENT0_EXT				0x8CE0
#define GL_ARRAY_BUFFER_ARB						0x8892
#define GL_STATIC_DRAW_ARB						0x88E4
#define GL_DYNAMIC_DRAW_ARB                     0x88E8
#define GL_ELEMENT_ARRAY_BUFFER_ARB				0x8893
#define GL_OBJECT_INFO_LOG_LENGTH_ARB			0x8B84
#define GL_VERTEX_SHADER_ARB					0x8B31
#define GL_OBJECT_COMPILE_STATUS_ARB			0x8B81
#define GL_GEOMETRY_SHADER_ARB					0x8DD9
#define GL_FRAGMENT_SHADER_ARB					0x8B30
#define GL_OBJECT_LINK_STATUS_ARB				0x8B82
#define GL_OBJECT_ACTIVE_UNIFORMS_ARB			0x8B86
#define GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB	0x8B87
#define GL_SAMPLER_1D_ARB						0x8B5D
#define GL_SAMPLER_2D_RECT_SHADOW_ARB			0x8B64
#define GL_TEXTURE0_ARB							0x84C0
#define GL_CLAMP_TO_EDGE						0x812F
#define GL_MIRRORED_REPEAT						0x8370
#define GL_TEXTURE_3D							0x806F
#define GL_TEXTURE_WRAP_R						0x8072
#define GL_TEXTURE_MAX_ANISOTROPY_EXT			0x84FE
#define GL_FUNC_ADD								0x8006
#define GL_FUNC_SUBTRACT						0x800A
#define GL_FUNC_REVERSE_SUBTRACT				0x800B
#define GL_MIN									0x8007
#define GL_MAX									0x8008
#define GL_INTENSITY16F_ARB						0x881D
#define GL_LUMINANCE_ALPHA16F_ARB				0x881F
#define GL_RGB16F_ARB							0x881B
#define GL_RGBA16F_ARB							0x881A
#define GL_INTENSITY32F_ARB						0x8817
#define GL_LUMINANCE_ALPHA32F_ARB				0x8819
#define GL_RGB32F_ARB							0x8815
#define GL_RGBA32F_ARB							0x8814
#define GL_DEPTH_COMPONENT16					0x81A5
#define GL_DEPTH_COMPONENT24					0x81A6
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT			0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT		0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT		0x83F3
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X			0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z			0x851A
#define GL_TEXTURE_CUBE_MAP						0x8513
#define GL_FRAMEBUFFER_EXT						0x8D40
#define GL_DEPTH_ATTACHMENT_EXT					0x8D00
#define GL_STENCIL_ATTACHMENT_EXT				0x8D20
#define GL_MAX_TEXTURE_IMAGE_UNITS				0x8872
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT		0x84FF
#define GL_MAX_DRAW_BUFFERS_ARB					0x8824
#define GL_MAX_VERTEX_UNIFORM_COMPONENTS		0x8B4A
#define GL_STREAM_DRAW_ARB						0x88E0
#define GL_MAX_VERTEX_ATTRIBS					0x8869
#define GL_FRAMEBUFFER_COMPLETE_EXT				0x8CD5
#define GL_NUM_EXTENSIONS                       0x821D
#define GL_NUM_COMPRESSED_TEXTURE_FORMATS		0x86A2
#define GL_COMPRESSED_TEXTURE_FORMATS			0x86A3
#define GL_PIXEL_PACK_BUFFER                    0x88EB

// GL_ARB_framebuffer_sRGB
#define GL_FRAMEBUFFER_SRGB						0x8DB9

// GL_VERSION_1_2
typedef void (APIENTRYP PFNGLBLENDEQUATIONPROC) (GLenum mode);
PFNGLBLENDEQUATIONPROC glBlendEquation = nullptr;

// GL_VERSION_2_0
typedef void (APIENTRYP PFNGLDRAWBUFFERSPROC) (GLsizei n, const GLenum *bufs);
PFNGLDRAWBUFFERSPROC glDrawBuffers = nullptr;

// GL_ARB_multitexture
typedef void (APIENTRYP PFNGLACTIVETEXTUREARBPROC) (GLenum texture);
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = nullptr;

// GL_EXT_texture3D
typedef void (APIENTRYP PFNGLTEXIMAGE3DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
PFNGLTEXIMAGE3DPROC glTexImage3D = nullptr;

// GL_ARB_texture_compression
typedef void (APIENTRYP PFNGLCOMPRESSEDTEXIMAGE2DARBPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data);
typedef void (APIENTRYP PFNGLCOMPRESSEDTEXIMAGE3DARBPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data);
PFNGLCOMPRESSEDTEXIMAGE2DARBPROC glCompressedTexImage2DARB = nullptr;
PFNGLCOMPRESSEDTEXIMAGE3DARBPROC glCompressedTexImage3DARB = nullptr;

//-----------------------------------------------------------------------------
// VBO
//-----------------------------------------------------------------------------

// GL_ARB_vertex_buffer_object
typedef void (APIENTRYP PFNGLGENBUFFERSARBPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLBINDBUFFERARBPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRYP PFNGLBUFFERDATAARBPROC) (GLenum target, GLsizeiptrARB size, const GLvoid *data, GLenum usage);
typedef void (APIENTRYP PFNGLDELETEBUFFERSARBPROC) (GLsizei n, const GLuint *buffers);
typedef void* (APIENTRYP PFNGLMAPBUFFERARBPROC) (GLenum target, GLenum access);
typedef GLboolean (APIENTRYP PFNGLUNMAPBUFFERARBPROC) (GLenum target);
PFNGLGENBUFFERSARBPROC glGenBuffersARB = nullptr;
PFNGLBINDBUFFERARBPROC glBindBufferARB = nullptr;
PFNGLBUFFERDATAARBPROC glBufferDataARB = nullptr;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB = nullptr;
PFNGLMAPBUFFERARBPROC glMapBufferARB = nullptr;
PFNGLUNMAPBUFFERARBPROC glUnmapBufferARB = nullptr;

typedef struct __GLsync *GLsync;
typedef GLsync (APIENTRYP PFNGLFENCESYNCPROC) (GLenum condition, GLbitfield flags);
typedef GLenum (APIENTRYP PFNGLCLIENTWAITSYNCPROC) (GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void (APIENTRYP PFNGLDELETESYNCPROC) (GLsync sync);
PFNGLFENCESYNCPROC glFenceSync = nullptr;
PFNGLCLIENTWAITSYNCPROC glClientWaitSync = nullptr;
PFNGLDELETESYNCPROC glDeleteSync = nullptr;

// GL_ARB_vertex_program
typedef void (APIENTRYP PFNGLENABLEVERTEXATTRIBARRAYARBPROC) (GLuint index);
typedef void (APIENTRYP PFNGLDISABLEVERTEXATTRIBARRAYARBPROC) (GLuint index);
typedef void (APIENTRYP PFNGLVERTEXATTRIBPOINTERARBPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
PFNGLENABLEVERTEXATTRIBARRAYARBPROC glEnableVertexAttribArrayARB = nullptr;
PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glDisableVertexAttribArrayARB = nullptr;
PFNGLVERTEXATTRIBPOINTERARBPROC glVertexAttribPointerARB = nullptr;

//-----------------------------------------------------------------------------
// VAO
//-----------------------------------------------------------------------------

// GL_ARB_vertex_array_object
typedef void (APIENTRYP PFNGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef void (APIENTRYP PFNGLBINDVERTEXARRAYPROC) (GLuint array);
typedef void (APIENTRYP PFNGLDELETEVERTEXARRAYSPROC) (GLsizei n, const GLuint *arrays);
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = nullptr;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = nullptr;

//-----------------------------------------------------------------------------
// GLSL
//-----------------------------------------------------------------------------

// GL_ARB_shading_language_100

// GL_ARB_shader_objects
typedef GLhandleARB (APIENTRYP PFNGLCREATEPROGRAMOBJECTARBPROC) (void);
typedef GLhandleARB (APIENTRYP PFNGLCREATESHADEROBJECTARBPROC) (GLenum shaderType);
typedef void (APIENTRYP PFNGLDELETEOBJECTARBPROC) (GLhandleARB obj);
typedef void (APIENTRYP PFNGLATTACHOBJECTARBPROC) (GLhandleARB containerObj, GLhandleARB obj);
typedef void (APIENTRYP PFNGLSHADERSOURCEARBPROC) (GLhandleARB shaderObj, GLsizei count, const GLcharARB* *string, const GLint *length);
typedef void (APIENTRYP PFNGLCOMPILESHADERARBPROC) (GLhandleARB shaderObj);
typedef void (APIENTRYP PFNGLLINKPROGRAMARBPROC) (GLhandleARB programObj);
typedef void (APIENTRYP PFNGLUSEPROGRAMOBJECTARBPROC) (GLhandleARB programObj);
typedef void (APIENTRYP PFNGLGETOBJECTPARAMETERIVARBPROC) (GLhandleARB obj, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETINFOLOGARBPROC) (GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog);
typedef GLint (APIENTRYP PFNGLGETUNIFORMLOCATIONARBPROC) (GLhandleARB programObj, const GLcharARB *name);
typedef void (APIENTRYP PFNGLUNIFORM1IARBPROC) (GLint location, GLint v0);
typedef void (APIENTRYP PFNGLUNIFORM1FARBPROC) (GLint location, GLfloat v0);
typedef void (APIENTRYP PFNGLUNIFORM1FVARBPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORM2FVARBPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORM3FVARBPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORM4FVARBPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX3FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX4FVARBPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLGETUNIFORMFVARBPROC) (GLhandleARB programObj, GLint location, GLfloat *params);
typedef void (APIENTRYP PFNGLGETACTIVEUNIFORMARBPROC) (GLhandleARB programObj, GLuint index, GLsizei maxLength, GLsizei *length, GLint *size, GLenum *type, GLcharARB *name);
PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB = nullptr;
PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB = nullptr;
PFNGLDELETEOBJECTARBPROC glDeleteObjectARB = nullptr;
PFNGLATTACHOBJECTARBPROC glAttachObjectARB = nullptr;
PFNGLSHADERSOURCEARBPROC glShaderSourceARB = nullptr;
PFNGLCOMPILESHADERARBPROC glCompileShaderARB = nullptr;
PFNGLLINKPROGRAMARBPROC glLinkProgramARB = nullptr;
PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB = nullptr;
PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB = nullptr;
PFNGLGETINFOLOGARBPROC glGetInfoLogARB = nullptr;
PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB = nullptr;
PFNGLUNIFORM1IARBPROC glUniform1iARB = nullptr;
PFNGLUNIFORM1FARBPROC glUniform1fARB = nullptr;
PFNGLUNIFORM1FVARBPROC glUniform1fvARB = nullptr;
PFNGLUNIFORM2FVARBPROC glUniform2fvARB = nullptr;
PFNGLUNIFORM3FVARBPROC glUniform3fvARB = nullptr;
PFNGLUNIFORM4FVARBPROC glUniform4fvARB = nullptr;
PFNGLUNIFORMMATRIX3FVARBPROC glUniformMatrix3fvARB = nullptr;
PFNGLUNIFORMMATRIX4FVARBPROC glUniformMatrix4fvARB = nullptr;
PFNGLGETUNIFORMFVARBPROC glGetUniformfvARB = nullptr;
PFNGLGETACTIVEUNIFORMARBPROC glGetActiveUniformARB = nullptr;

// GL_ARB_vertex_shader
typedef void (APIENTRYP PFNGLBINDATTRIBLOCATIONARBPROC) (GLhandleARB programObj, GLuint index, const GLcharARB *name);
PFNGLBINDATTRIBLOCATIONARBPROC glBindAttribLocationARB = nullptr;

// GL_ARB_fragment_shader

// ARB_shading_language_include
typedef void (APIENTRYP PFNGLNAMEDSTRINGARBPROC) (int type, int namelen, const char* name, int stringlen, const char* string);
PFNGLNAMEDSTRINGARBPROC glNamedStringARB = nullptr;

//-----------------------------------------------------------------------------
// FBO
//-----------------------------------------------------------------------------

// GL_EXT_framebuffer_object
#define GL_MAX_RENDERBUFFER_SIZE_EXT      0x84E8
typedef void (APIENTRYP PFNGLPATCHPARAMETERIPROC)(GLenum pname, GLint value);
typedef void (APIENTRYP PFNGLPATCHPARAMETERFVPROC)(GLenum pname, const GLfloat* values);
typedef void (APIENTRYP PFNGLGENFRAMEBUFFERSEXTPROC) (GLsizei n, GLuint *framebuffers);
typedef void (APIENTRYP PFNGLBINDFRAMEBUFFEREXTPROC) (GLenum target, GLuint framebuffer);
typedef void (APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DEXTPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRYP PFNGLDELETEFRAMEBUFFERSEXTPROC) (GLsizei n, const GLuint *framebuffers);
typedef void (APIENTRYP PFNGLGENRENDERBUFFERSEXTPROC) (GLsizei n, GLuint *renderbuffers);
typedef void (APIENTRYP PFNGLBINDRENDERBUFFEREXTPROC) (GLenum target, GLuint renderbuffer);
typedef void (APIENTRYP PFNGLRENDERBUFFERSTORAGEEXTPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (APIENTRYP PFNGLDELETERENDERBUFFERSEXTPROC) (GLsizei n, const GLuint *renderbuffers);
typedef GLenum (APIENTRYP PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC) (GLenum target);
PFNGLGENFRAMEBUFFERSEXTPROC glGenFramebuffersEXT = nullptr;
PFNGLBINDFRAMEBUFFEREXTPROC glBindFramebufferEXT = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DEXTPROC glFramebufferTexture2DEXT = nullptr;
PFNGLDELETEFRAMEBUFFERSEXTPROC glDeleteFramebuffersEXT = nullptr;
PFNGLGENRENDERBUFFERSEXTPROC glGenRenderbuffersEXT = nullptr;
PFNGLBINDRENDERBUFFEREXTPROC glBindRenderbufferEXT = nullptr;
PFNGLRENDERBUFFERSTORAGEEXTPROC glRenderbufferStorageEXT = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC glFramebufferRenderbufferEXT = nullptr;
PFNGLDELETERENDERBUFFERSEXTPROC glDeleteRenderbuffersEXT = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC glCheckFramebufferStatusEXT = nullptr;
PFNGLPATCHPARAMETERIPROC glPatchParameteri = nullptr;
PFNGLPATCHPARAMETERFVPROC glPatchParameterfv = nullptr;


// ARB_bindless_texture

static bool glExt_Init(HDC Hdc);
static bool wglExt_IsSupported(HDC Hdc, const char *Ext);

// WGL_ARB_multisample
#define	WGL_SAMPLES_ARB				0x2042

// WGL_ARB_pixel_format
#define	WGL_DRAW_TO_WINDOW_ARB		0x2001
#define	WGL_ACCELERATION_ARB		0x2003
#define	WGL_FULL_ACCELERATION_ARB	0x2027
#define	WGL_DOUBLE_BUFFER_ARB		0x2011
#define	WGL_RED_BITS_ARB			0x2015
#define	WGL_GREEN_BITS_ARB			0x2017
#define	WGL_BLUE_BITS_ARB			0x2019
#define	WGL_ALPHA_BITS_ARB			0x201B
#define	WGL_DEPTH_BITS_ARB			0x2022
#define	WGL_STENCIL_BITS_ARB		0x2023

#define GET_PROC_ADDRESS(Type, Proc) Proc = (Type)wglGetProcAddress(#Proc);
#endif // Windows

// ARB_shading_language_include
#define SHADER_INCLUDE_ARB 0x8DAE // For some reason, it is not defined under macOS

#ifndef COMMS_WINDOWS
static bool glExt_Init();
#endif

static cList<cStr> glExt_List;
static bool glExt_IsSupported(const char *Ext);
static bool glExt_AreSupported(const char *ExtList);
struct glExt_Supported {
    static GLint MaxAnisotropy;
    static bool TextureCompressionDXT1, TextureCompressionDXT3, TextureCompressionDXT5;
    static bool TextureCompressionPVR;
    static GLint MaxTexUnits;
    static cStr Vendor;
	static cStr Renderer;
	static cStr Version;
    static GLint FrameBufferMaxSize;
	static cList<GLenum> DrawBuffers; // Draw buffers count equals supported MRT count
    static GLuint FBO;
	static bool BindlessTexture;
    static bool TessellationShader;
    static bool Include;
};
GLint glExt_Supported::MaxAnisotropy = 0;
bool glExt_Supported::TextureCompressionDXT1 = false, glExt_Supported::TextureCompressionDXT3 = false, glExt_Supported::TextureCompressionDXT5 = false;
bool glExt_Supported::TextureCompressionPVR = false;
GLint glExt_Supported::MaxTexUnits = 0;
cStr glExt_Supported::Vendor;
cStr glExt_Supported::Renderer;
cStr glExt_Supported::Version;
GLint glExt_Supported::FrameBufferMaxSize = 0;
cList<GLenum> glExt_Supported::DrawBuffers;
GLuint glExt_Supported::FBO = 0;
bool glExt_Supported::BindlessTexture = false;
bool glExt_Supported::TessellationShader = false;
bool glExt_Supported::Include = false;

static int cRenderGL_DrawingIntoFrameBuffer = true;

#ifdef COMMS_ASSERT
static void HandleGLError() {
    GLenum Err = glGetError();
    if(Err != GL_NO_ERROR) {
        if(GL_INVALID_OPERATION == Err) {
            cAssertM(0, "GL_INVALID_OPERATION");
        } else if(GL_INVALID_VALUE == Err) {
            cAssertM(0, "GL_INVALID_VALUE");
        } else if(GL_INVALID_ENUM == Err) {
            cAssertM(0, "GL_INVALID_ENUM");
        } else {
            cAssertM(0, "Err != GL_NO_ERROR");
        }
    }
}
#else // Release
#define HandleGLError()
/*
void HandleGLError() {
	GLenum Err = glGetError();
	if (Err != GL_NO_ERROR) {
		comms::cMessageBox::Ok("GL error!", "");
	}
}
*/
#endif // COMMS_ASSERT

//*****************************************************************************
// cRenderGL_VERTEX_FORMAT
//*****************************************************************************
struct cRenderGL_VERTEX_FORMAT {
	cVertex::Format SrcFormat;
    static int UsedAttribs;
	
	enum Constants {
		MaxAttribs = 16
	};
	
	struct Attrib {
		int Type;
		int Dim;
		int Offset;
		
		Attrib() : Type(0), Dim(0), Offset(0) {
		}
	};
	
	Attrib Attribs[MaxAttribs];
	int VertexSize;

	cRenderGL_VERTEX_FORMAT() {
		VertexSize = 0;
	}
}; // cRenderGL_VERTEX_FORMAT

int cRenderGL_VERTEX_FORMAT::UsedAttribs = 0;
static cList<cRenderGL_VERTEX_FORMAT> s_VertexFormats;

#ifdef COMMS_WINDOWS
#define GLsizeiptr GLsizeiptrARB
#define glDeleteBuffers glDeleteBuffersARB
#endif // Windows

//*****************************************************************************
// cRenderGL_VERTEX_BUFFER
//*****************************************************************************
struct cRenderGL_VERTEX_BUFFER {
    // OpenGL 3.2 core context requires vertex arrays object (VAO) because vertex related calls are bound to VAO
    static GLuint Array;
    static void CreateArray() {
        if(0 == Array) {
            HandleGLError();
            glGenVertexArrays(1, &Array);
            HandleGLError();
            glBindVertexArray(Array);
            HandleGLError();
        }
    }
    static void FreeArray() {
        if(Array > 0) {
            HandleGLError();
            glBindVertexArray(0);
            HandleGLError();
            glDeleteVertexArrays(1, &Array);
            HandleGLError();
        }
        Array = 0;
    }

    GLuint Buffer;
	GLsizeiptr Size;
    
    cRenderGL_VERTEX_BUFFER() {
        Clear();
    }
    void Free() {
        if(Buffer > 0) {
            HandleGLError();
            glDeleteBuffers(1, &Buffer);
            HandleGLError();
        }
        Clear();
    }
	void Clear() {
        Buffer = 0;
		Size = 0;
	}
    void SetData(const void *Data, const size_t _Size, const bool Static);
};
GLuint cRenderGL_VERTEX_BUFFER::Array = 0;
static cList<cRenderGL_VERTEX_BUFFER> s_VertexBuffers;
#ifndef COMMS_3DCOAT
typedef cList<int> UnusedPool;
#endif // !COMMS_3DCOAT
static UnusedPool s_VertexBuffersFreeID;
static int s_CurVertexBufferID = -1;
static int s_DynVertexBufferID = -1;

//*****************************************************************************
// cRenderGL_INDEX_BUFFER
//*****************************************************************************
struct cRenderGL_INDEX_BUFFER {
    GLuint Buffer;
    int IndexCount;
    int IndexSize;

	cRenderGL_INDEX_BUFFER() {
		Clear();
	}
    void Free() {
        if(Buffer > 0) {
            HandleGLError();
            glDeleteBuffers(1, &Buffer);
            HandleGLError();
        }
        Clear();
    }
    void Clear() {
        Buffer = 0;
		IndexCount = 0;
		IndexSize = 4;
	}
    void SetData(const void *Data, const int _IndexCount, const int _IndexSize, const bool Static);
};
static cList<cRenderGL_INDEX_BUFFER> s_IndexBuffers;
static UnusedPool s_IndexBuffersFreeID;
static int s_CurIndexBufferID = -1;
static int s_DynIndexBufferID = -1;

#ifdef COMMS_WINDOWS
#define glDeleteProgram glDeleteObjectARB
#define glDeleteShader glDeleteObjectARB
#endif // Windows

//*****************************************************************************
// cRenderGL_SHADER
//*****************************************************************************
struct cRenderGL_SHADER {
	cStr Name;
	int VertexFormatID;
	cStr Extra;
	cStr FilePn;

	GLuint Program;
	GLuint VS;
	GLuint GS;
	GLuint TCS;
	GLuint TES;
	GLuint FS;

	struct Sampler {
		cStr Name;
		int ImageUnit;
        static int Compare(const Sampler *l, const Sampler *r) {
            return cStr::Compare(l->Name, r->Name);
        }
    };
	
	cList<Sampler> Samplers;
	
	struct AutoConst {
		int Index;
		GLint Loc;
	};
	cList<AutoConst> AutoConsts;

	struct Const {
		cStr Name;
		GLint Loc;
	};
	cList<Const> Consts;

	void Clear() {
		Name.Clear();
		VertexFormatID = -1;
		Extra.Clear();
		FilePn.Clear();

		Program = 0;
		VS = 0;
		GS = 0;
		TCS = 0;
		TES = 0;
		FS = 0;

		Samplers.Clear();
		AutoConsts.Clear();
		Consts.Clear();
	}
	void Free() {
		if(Program != 0) {
			glDeleteProgram(Program);
            HandleGLError();
			Program = 0;
		}
		if (VS != 0) {
			glDeleteShader(VS);
			HandleGLError();
			VS = 0;
		}
		if (GS != 0) {
			glDeleteShader(GS);
			HandleGLError();
			GS = 0;
		}
		if (TCS != 0) {
			glDeleteShader(TCS);
			HandleGLError();
			TCS = 0;
		}
		if (TES != 0) {
			glDeleteShader(TES);
			HandleGLError();
			TES = 0;
		}
		if(FS != 0) {
			glDeleteShader(FS);
            HandleGLError();
			FS = 0;
		}
	}
	bool IsValid() const {
		return Program != 0 && VS != 0 && FS != 0;
	}

	cRenderGL_SHADER() {
		Clear();
	}
}; // cRenderGL_SHADER

static cList<cRenderGL_SHADER> s_Shaders;
static UnusedPool s_ShadersFreeID;
static int s_CurShaderID = -1;

//*****************************************************************************
// cRenderGL_DEPTH_STATE
//*****************************************************************************
struct cRenderGL_DEPTH_STATE {
	bool TestEnabled;
	bool WriteEnabled;
	cDepthFunc::Enum Func;
}; // cRenderGL_DEPTH_STATE

static cList<cRenderGL_DEPTH_STATE> s_DepthStates;

//*****************************************************************************
// cRenderGL_BLEND_STATE
//*****************************************************************************
struct cRenderGL_BLEND_STATE {
	cBlendFactor::Enum SrcFactor;
	cBlendFactor::Enum DstFactor;
	cBlendMode::Enum Mode;
	dword Mask;
	bool EnableBlend;
}; // cRenderGL_BLEND_STATE

static cList<cRenderGL_BLEND_STATE> s_BlendStates;

//*****************************************************************************
// cRenderGL_SAMPLER_STATE
//*****************************************************************************
struct cRenderGL_SAMPLER_STATE {
	GLint MinFilter;
	GLint MagFilter;
	GLint WrapS;
	GLint WrapT;
	GLint WrapR;
	bool Aniso;
	bool MipMaps;

	static bool Equals(const cRenderGL_SAMPLER_STATE &l, const cRenderGL_SAMPLER_STATE &r) {
		return l.MinFilter == r.MinFilter && l.MagFilter == r.MagFilter &&
			l.WrapS == r.WrapS && l.WrapT == r.WrapT && l.WrapR == r.WrapR &&
			l.Aniso == r.Aniso;
	}
}; // cRenderGL_SAMPLER_STATE

static cList<cRenderGL_SAMPLER_STATE> s_SamplerStates;

#ifdef COMMS_WINDOWS
#define glDeleteRenderbuffers glDeleteRenderbuffersEXT
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER GL_RENDERBUFFER_EXT
#endif
#endif // Windows

//*****************************************************************************
// cRenderGL_TEXTURE
//*****************************************************************************
struct cRenderGL_TEXTURE {
	cStr FilePn;
	int DesiredSamplerStateID, BindedSamplerStateID;
	cFormat::Enum Format;
	int Width, Height, Depth, MipMapCount;
	bool CubeMap; // Used only for color render target
	bool RenderTarget;
	
	union {
		GLuint glTexID;
		GLuint glDepthID;
	};
	GLuint glTarget;

    GLuint pbo[2];
    bool pbo_has_data[2];
    int current_pbo_index;
    GLsync pbo_sync[2];

	void Clear() {
		FilePn.Clear();
		DesiredSamplerStateID = -1;
		BindedSamplerStateID = -1;
		Format = cFormat::fmtNone;
		
		Width = 0;
		Height = 0;
		Depth = 0;
		MipMapCount = 1;
		
		CubeMap = false;
		RenderTarget = false;
		
		glTexID = 0;
		glTarget = 0;
		
        pbo[0] = 0;
        pbo[1] = 0;
        pbo_has_data[0] = false;
        pbo_has_data[1] = false;
        current_pbo_index = 0;
        pbo_sync[0] = nullptr;
        pbo_sync[1] = nullptr;
	}

	void Free() {
		if(glTarget > 0) {
			if(GL_RENDERBUFFER == glTarget) {
				glDeleteRenderbuffers(1, &glDepthID);
                HandleGLError();
			} else {
				glDeleteTextures(1, &glTexID);
                HandleGLError();
			}
			glTexID = 0;
			glTarget = 0;
		}
        if(pbo[0] != 0) {
            glDeleteBuffers(2, pbo);
            HandleGLError();
            pbo[0] = 0;
            pbo[1] = 0;
        }
        if (glDeleteSync) {
            if (pbo_sync[0]) { glDeleteSync(pbo_sync[0]); pbo_sync[0] = nullptr; }
            if (pbo_sync[1]) { glDeleteSync(pbo_sync[1]); pbo_sync[1] = nullptr; }
        }
	}

	cRenderGL_TEXTURE() {
		Clear();
	}
}; // cRenderGL_TEXTURE

static cList<cRenderGL_TEXTURE> s_Textures;
static UnusedPool s_TexturesFreeID;
static std::map<std::string, int> s_TexturesMap;
static std::map<int,std::string> s_TexturesMapID;

#ifdef COMMS_WINDOWS
	static HWND s_hWnd = nullptr;
#endif // Windows

#ifdef COMMS_WINDOWS
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#endif
#endif // Windows

static bool s_VSync = false;
static bool s_GammaCorrection = false;

// cRenderGL_LoadTemplates
static void cRenderGL_LoadTemplates() {
    // Support "#include" inside "glsl"
    // Source:
    // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_shading_language_include.txt
    // https://stackoverflow.com/questions/10754437/how-to-using-the-include-in-glsl-support-arb-shading-language-include
    cList<cStr> Files;
    const cStr Parent = "data/Shaders/";
    const cStr Path = Parent + "Templates";
    cIO::SearchFiles(Path, &Files, "glsl");
    cLog::Message(" * Loading includes from \"%s\"", Path.ToCharPtr());
    cFile C;
    for(int i = 0; i < Files.Count(); i++) {
        cStr &F = Files[i];
        if(cIO::LoadFile(F, &C)) {
            F.Remove(0, Parent.Length() - 1); // <name> must begin with the character '/'
            F.BackSlashesToSlashes(); // Needed for Windows
            cStr Include = F;
            Include.Insert(0, "#include \"");
            Include.Append("\"");
            { // Replace "@insert" with "#include"
                cStr Insert = "//@insert:" + F.GetFileBase();
                cRender::ReplaceInShaders(Insert.ToCharPtr(), Include.ToCharPtr());
            }
            if(glExt_Supported::Include) { // "#include" is supported
                HandleGLError();
                glNamedStringARB(SHADER_INCLUDE_ARB, F.Length(), F.ToCharPtr(), (int)C.Size(), C.ToCharPtr());
                HandleGLError();
            } else { // "#include" is not supported
                C.SeekEnd(0);
                C.WriteByte(0);
                cRender::ReplaceInShaders(Include.ToCharPtr(), C.ToCharPtr());
            }
        }
    }
}

//-----------------------------------------------------------------------------
// cRenderGL::Init
//-----------------------------------------------------------------------------
bool cRenderGL::Init(const int MaxSamples) {
	int i;

#ifdef COMMS_WINDOWS
//	glfwInit();
/*	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}
	*/
	s_hWnd = cWinMain_GetWindow();
	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd, 0, sizeof(pfd));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;
	
	HDC dc = GetDC(s_hWnd);
	int PixelFormat = ChoosePixelFormat(dc, &pfd);
	SetPixelFormat(dc, PixelFormat, &pfd);
	HGLRC rc = wglCreateContext(dc);
	wglMakeCurrent(dc, rc);
	
	bool s = glExt_Init(dc);

	wglMakeCurrent(nullptr, nullptr);
	wglDeleteContext(rc); rc = nullptr;
	ReleaseDC(s_hWnd, dc); dc = nullptr;

	if(!s) {
		return false;
	}

	// Reinit OpenGL with extensions
	
	int Attribs[] = {
		WGL_DRAW_TO_WINDOW_ARB,	GL_TRUE,
		WGL_ACCELERATION_ARB,	WGL_FULL_ACCELERATION_ARB,
		WGL_DOUBLE_BUFFER_ARB,	GL_TRUE,
		WGL_RED_BITS_ARB,		8,
		WGL_GREEN_BITS_ARB,		8,
		WGL_BLUE_BITS_ARB,		8,
		WGL_ALPHA_BITS_ARB,		8,
		WGL_DEPTH_BITS_ARB,		24,
		WGL_STENCIL_BITS_ARB,	8,
		0
	};

	cList<int> PixelFormats(256);
	UINT Count = 0;
	int Attrib = WGL_SAMPLES_ARB, Samples, BestFormat = 0, BestSamples = -1;
	
	HDC Hdc = GetDC(s_hWnd);
	if(wglChoosePixelFormatARB(Hdc, Attribs, nullptr, PixelFormats.Count(), PixelFormats.ToPtr(), &Count) && Count > 0) {
		PixelFormats.SetCount((int)Count);
		// Find multisample format
		for(i = 0; i < PixelFormats.Count(); i++) {
			wglGetPixelFormatAttribivARB(Hdc, PixelFormats[i], 0, 1, &Attrib, &Samples);
			if((Samples > BestSamples) && ((Samples <= MaxSamples) || (-1 == MaxSamples))) {
				BestFormat = i;
				BestSamples = Samples;
			}
		}
	} else {
		PixelFormats.SetCount(1);
		PixelFormats[0] = ChoosePixelFormat(Hdc, &pfd);
	}
	
	SetPixelFormat(Hdc, PixelFormats[BestFormat], &pfd);
	HGLRC Hrc = wglCreateContext(Hdc);
	wglMakeCurrent(Hdc, Hrc);

	if(BestSamples > 0) {
		glEnable(GL_MULTISAMPLE_ARB);
	}

	if(wglSwapIntervalEXT != nullptr) {
		wglSwapIntervalEXT(s_VSync);
		HandleGLError();
	}
#else // macOS, Linux
	if(!glExt_Init()) {
		Free();
		return false;
	}
#endif // Windows

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
    HandleGLError();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    HandleGLError();

	// Draw buffers
	for(i = 0; i < glExt_Supported::DrawBuffers.Count(); i++) {
		glExt_Supported::DrawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
	}

	cRenderGL_VERTEX_BUFFER::CreateArray();
    cRenderGL_LoadTemplates();
    return true;
} // cRenderGL::Init

#ifdef COMMS_WINDOWS
#define glUseProgram glUseProgramObjectARB
#define glDeleteFramebuffers glDeleteFramebuffersEXT
#endif // Windows

//-----------------------------------------------------------------------------
// cRenderGL::Free
//-----------------------------------------------------------------------------
void cRenderGL::Free() {
	int i;

	//*************************************************************************
	// Free vertex formats
	//*************************************************************************
	s_VertexFormats.Clear();
    cRenderGL_VERTEX_FORMAT::UsedAttribs = 0;

	//*************************************************************************
	// Free vertex buffers
	//*************************************************************************
	for(i = 0; i < s_VertexBuffers.Count(); i++) {
		cRenderGL_VERTEX_BUFFER &vb = s_VertexBuffers[i];
        vb.Free();
	}
	s_VertexBuffers.Clear();
	s_VertexBuffersFreeID.Clear();
	s_CurVertexBufferID = -1;
    s_DynVertexBufferID = -1;

	//*************************************************************************
	// Free index buffers
	//*************************************************************************
	for(i = 0; i < s_IndexBuffers.Count(); i++) {
		cRenderGL_INDEX_BUFFER &ib = s_IndexBuffers[i];
        ib.Free();
	}
	s_IndexBuffers.Clear();
	s_IndexBuffersFreeID.Clear();
	s_CurIndexBufferID = -1;
    s_DynIndexBufferID = -1;

    // Free Vertex Buffer Array
    cRenderGL_VERTEX_BUFFER::FreeArray();

	//*************************************************************************
	// Free shaders
	//*************************************************************************
	for(i = 0; i < s_Shaders.Count(); i++) {
		if(s_CurShaderID == i) {
			glUseProgram(0);
            HandleGLError();
		}
		cRenderGL_SHADER &S = s_Shaders[i];
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
		cRenderGL_TEXTURE &T = s_Textures[i];
		T.Free();
	}
	s_Textures.Clear();
	s_TexturesFreeID.Clear();

	//*********************************************************************
	// Free frame buffer
	//*********************************************************************
	if(glExt_Supported::FBO != 0) {
		glDeleteFramebuffers(1, &glExt_Supported::FBO);
        HandleGLError();
		glExt_Supported::FBO = 0;
	}

	//*************************************************************************
	// Free OpenGL rendering context
	//*************************************************************************
#ifdef COMMS_WINDOWS
	HGLRC hglrc = wglGetCurrentContext();
	if(hglrc != nullptr) {
		HDC hdc = wglGetCurrentDC();
		wglMakeCurrent(hdc, nullptr);
		ReleaseDC(s_hWnd, hdc);
		wglDeleteContext(hglrc);
	}
	s_hWnd = nullptr;
#endif // Windows

	glExt_Supported::Vendor.Clear();
	glExt_Supported::Version.Clear();
	glExt_Supported::Renderer.Clear();
	glExt_Supported::MaxAnisotropy = 0;
	glExt_Supported::DrawBuffers.Clear();
} // cRenderGL::Free

//-----------------------------------------------------------------------------
// cRenderGL::AddVertexFormat
//-----------------------------------------------------------------------------
int cRenderGL::AddVertexFormat(const cVertex::Format &Format) {
	cRenderGL_VERTEX_FORMAT vf;
	vf.SrcFormat = Format;

	int i;
	
	for(i = 0; i < Format.Count(); i++) {
		cAssert(i < cRenderGL_VERTEX_FORMAT::MaxAttribs);
		if(i >= cRenderGL_VERTEX_FORMAT::MaxAttribs) {
			return -1;
		}
	
		const cVertex::Attrib &a = Format[i];
		cAssert(a.IsValid());
		if(!a.IsValid()) {
			return -1;
		}
		
		vf.Attribs[i].Type = a.Type;
		vf.Attribs[i].Dim = a.Dim;
		vf.Attribs[i].Offset = vf.VertexSize;
		
		vf.VertexSize += a.Dim * cVertexType::SizeOf(a.Type);
	}
	
    cRenderGL_VERTEX_FORMAT::UsedAttribs = cMath::Max(Format.Count(), cRenderGL_VERTEX_FORMAT::UsedAttribs);
	return s_VertexFormats.Add(vf);
} // cRenderGL::AddVertexFormat

// cRenderGL::GetVertexSize
int cRenderGL::GetVertexSize(const int VertexFormatID) {
	if(VertexFormatID < 0 || VertexFormatID >= s_VertexFormats.Count()) {
		return 0;
	}
	const cRenderGL_VERTEX_FORMAT &vf = s_VertexFormats[VertexFormatID];
	return vf.VertexSize;
}

#ifdef COMMS_WINDOWS
#define glBindBuffer glBindBufferARB
#define glVertexAttribPointer glVertexAttribPointerARB
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER GL_ARRAY_BUFFER_ARB
#endif
#endif // Windows

//---------------------------------------------------------------------------------
// cRenderGL::SetVertexBuffer
//---------------------------------------------------------------------------------
void cRenderGL::SetVertexBuffer(const int VertexBufferID) {
    HandleGLError();
	if(VertexBufferID == s_CurVertexBufferID) {
		return;
	}
	if(VertexBufferID < 0 || VertexBufferID >= s_VertexBuffers.Count()  || s_VertexBuffersFreeID.Contains(VertexBufferID)) {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        HandleGLError();
        s_CurVertexBufferID = -1;
		return;
	}
	const cRenderGL_VERTEX_BUFFER &VB = s_VertexBuffers[VertexBufferID];
    glBindBuffer(GL_ARRAY_BUFFER, VB.Buffer);
    HandleGLError();
    s_CurVertexBufferID = VertexBufferID;
} // cRenderGL::SetVertexBuffer

// cRenderGL::SetDynVertexBuffer
void cRenderGL::SetDynVertexBuffer(const void *Data, const size_t Size) {
    if(-1 == s_DynVertexBufferID) {
        s_DynVertexBufferID = AddVertexBuffer(Data, Size);
        SetVertexBuffer(s_DynVertexBufferID);
    } else {
        SetVertexBuffer(s_DynVertexBufferID);
        cRenderGL_VERTEX_BUFFER &VB = s_VertexBuffers[s_DynVertexBufferID];
        VB.SetData(Data, Size, false);
    }
}

#ifdef COMMS_WINDOWS
#define glEnableVertexAttribArray glEnableVertexAttribArrayARB
#define glDisableVertexAttribArray glDisableVertexAttribArrayARB
#endif // Windows

//-----------------------------------------------------------------------------
// ApplyVertexFormat
//-----------------------------------------------------------------------------
static void ApplyVertexFormat() {
    const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
    const cRenderGL_VERTEX_FORMAT &VF = s_VertexFormats[Sh.VertexFormatID];
    GLsizei glType[] = {
        GL_FLOAT,
        GL_UNSIGNED_BYTE
    };
    int i;
    char *Base = nullptr;

    HandleGLError();
    for(i = 0; i < cRenderGL_VERTEX_FORMAT::UsedAttribs; i++) {
        const cRenderGL_VERTEX_FORMAT::Attrib &A = VF.Attribs[i];
        if(A.Dim > 0) {
            glEnableVertexAttribArray(i);
            HandleGLError();
            glVertexAttribPointer(i, A.Dim, glType[A.Type], GL_TRUE, VF.VertexSize, Base + A.Offset);
            HandleGLError();
        } else {
            glDisableVertexAttribArray(i);
            HandleGLError();
        }
    }
} // ApplyVertexFormat

//*****************************************************************************
// cRenderGL_TOPOLOGY
//*****************************************************************************
static const GLenum cRenderGL_TOPOLOGY[cTopology::Count] = {
	GL_POINTS,			// cTopology::PointList
	GL_LINES,			// cTopology::LineList
	GL_LINE_STRIP,		// cTopology::LineStrip
	GL_TRIANGLES,		// cTopology::TriangleList
	GL_TRIANGLE_STRIP,	// cTopology::TriangleStrip
	GL_PATCHES,	// cTopology::TrianglePatches
	GL_PATCHES	// cTopology::QuadPatches
}; // cRenderGL_TOPOLOGY

//-----------------------------------------------------------------------------
// cRenderGL::DrawArrays : (..., const int, ...)
//-----------------------------------------------------------------------------
void cRenderGL::DrawArrays(const cTopology::Enum Topology) {
	cAssertM(s_CurShaderID != -1, "Set shader before drawing");
	if(-1 == s_CurShaderID) {
        return;
	}
	cAssertM(s_CurVertexBufferID != -1, "Set vertex buffer before drawing");
	if(-1 == s_CurVertexBufferID) {
		return;
	}
    ApplyVertexFormat();

	const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
	const cRenderGL_VERTEX_FORMAT &VF = s_VertexFormats[Sh.VertexFormatID];
    const cRenderGL_VERTEX_BUFFER &VB = s_VertexBuffers[s_CurVertexBufferID];
	int VertexCount = (int)(VB.Size / VF.VertexSize);
	
	cTopology::Enum topology = Topology;
	if (topology == cTopology::TriangleList && Sh.TCS > 0)
		topology = cTopology::TrianglePatches;

	if (topology == cTopology::TrianglePatches) 
		glPatchParameteri(GL_PATCH_VERTICES, 3);

	if (topology == cTopology::QuadPatches)
		glPatchParameteri(GL_PATCH_VERTICES, 4);

	HandleGLError();
	glDrawArrays(cRenderGL_TOPOLOGY[(int)topology], 0, VertexCount);
    HandleGLError();
} // cRenderGL::DrawArrays : (..., const int, ...)

//---------------------------------------------------------------------------------------------------------
// cRenderGL::DrawArrays : (..., const void *, ...)
//---------------------------------------------------------------------------------------------------------
void cRenderGL::DrawArrays(const cTopology::Enum Topology, const void *VertexData, const int VertexCount) {
	cAssertM(s_CurShaderID != -1, "Set shader before drawing");
	if(-1 == s_CurShaderID) {
		return;
	}
    const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
    const cRenderGL_VERTEX_FORMAT &VF = s_VertexFormats[Sh.VertexFormatID];
    size_t Size = VF.VertexSize * VertexCount;
    SetDynVertexBuffer(VertexData, Size);
    DrawArrays(Topology);
} // cRenderGL::DrawArrays : (..., const void *, ...)

//-----------------------------------------------------------------------------
// cRenderGL::DrawIndexed : (..., const int, ...)
//-----------------------------------------------------------------------------
void cRenderGL::DrawIndexed(const cTopology::Enum Topology) {
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
    ApplyVertexFormat();

	const cRenderGL_SHADER& Sh = s_Shaders[s_CurShaderID];

	cTopology::Enum topology = Topology;
	if (topology == cTopology::TriangleList && Sh.TCS > 0)
		topology = cTopology::TrianglePatches;

	if (topology == cTopology::TrianglePatches)
		glPatchParameteri(GL_PATCH_VERTICES, 3);

	if (topology == cTopology::QuadPatches)
		glPatchParameteri(GL_PATCH_VERTICES, 4);

	const cRenderGL_INDEX_BUFFER &ib = s_IndexBuffers[s_CurIndexBufferID];
    HandleGLError();
	glDrawElements(cRenderGL_TOPOLOGY[(int)topology], ib.IndexCount, 2 == ib.IndexSize ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, nullptr);
    HandleGLError();
} // cRenderGL::DrawIndexed : (..., const int, ...)

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// cRenderGL::DrawIndexed : (..., const void *, ...)
//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void cRenderGL::DrawIndexed(const cTopology::Enum Topology, const void *VertexData, const int VertexCount, const void *IndexData, const int IndexCount, const int IndexSize) {
	cAssertM(s_CurShaderID != -1, "Set shader before drawing");
	if(-1 == s_CurShaderID) {
		return;
	}
    const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
    const cRenderGL_VERTEX_FORMAT &VF = s_VertexFormats[Sh.VertexFormatID];
    size_t Size = VF.VertexSize * VertexCount;
    SetDynVertexBuffer(VertexData, Size);
    SetDynIndexBuffer(IndexData, IndexCount, IndexSize);
    DrawIndexed(Topology);
} // cRenderGL::DrawIndexed : (..., const void *, ...)

// cRenderGL::SetWireframe
void cRenderGL::SetWireframe(const bool Enabled) {
    HandleGLError();
	glPolygonMode(GL_FRONT_AND_BACK, Enabled ? GL_LINE : GL_FILL);
    HandleGLError();
}

#ifdef COMMS_WINDOWS
#define glGenBuffers glGenBuffersARB
#define glMapBuffer glMapBufferARB
#define glUnmapBuffer glUnmapBufferARB
#define glBufferData glBufferDataARB
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW GL_STATIC_DRAW_ARB
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW GL_DYNAMIC_DRAW_ARB
#endif
#endif // Windows

// cRenderGL_VERTEX_BUFFER::SetData
void cRenderGL_VERTEX_BUFFER::SetData(const void *Data, const size_t _Size, const bool Static) {
    Size = _Size;
    HandleGLError();
    glBufferData(GL_ARRAY_BUFFER, Size, Data, Static ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
    HandleGLError();
}

//-----------------------------------------------------------------------------
// cRenderGL::AddVertexBuffer
//-----------------------------------------------------------------------------
int cRenderGL::AddVertexBuffer(const void *Data, const size_t Size) {
    GLuint hCurVertexBuffer = (-1 == s_CurVertexBufferID) ? 0 : s_VertexBuffers[s_CurVertexBufferID].Buffer; // Store current vertex buffer (if any)

	cRenderGL_VERTEX_BUFFER VB;
    HandleGLError();
	glGenBuffers(1, &VB.Buffer);
    HandleGLError();
	glBindBuffer(GL_ARRAY_BUFFER, VB.Buffer);
    HandleGLError();
    VB.SetData(Data, Size, true);
    
	glBindBuffer(GL_ARRAY_BUFFER, hCurVertexBuffer); // Restore current vertex buffer
    HandleGLError();

	// Add vertex buffer slot or use empty
	int VertexBufferID = -1;
	if(!s_VertexBuffersFreeID.IsEmpty()) {
		VertexBufferID = s_VertexBuffersFreeID.GetAndRemoveLast();
		s_VertexBuffers[VertexBufferID] = VB;
		return VertexBufferID;
	} else {
		return s_VertexBuffers.Add(VB);
	}
} // cRenderGL::AddVertexBuffer

#ifdef COMMS_WINDOWS
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER GL_ELEMENT_ARRAY_BUFFER_ARB
#endif
#endif // Windows

// cRenderGL_INDEX_BUFFER::SetData
void cRenderGL_INDEX_BUFFER::SetData(const void *Data, const int _IndexCount, const int _IndexSize, const bool Static) {
    cAssert(Buffer != 0);
    IndexCount = _IndexCount;
    IndexSize = _IndexSize;
    const void *Src = Data;
    const dword Size = IndexCount * IndexSize;
    HandleGLError();
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, Size, Src, Static ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
    HandleGLError();
}

//------------------------------------------------------------------------------------------
// cRenderGL::AddIndexBuffer
//------------------------------------------------------------------------------------------
int cRenderGL::AddIndexBuffer(const void *Data, const int IndexCount, const int IndexSize) {
    GLuint hCurIndexBuffer = (-1 == s_CurIndexBufferID) ? 0 : s_IndexBuffers[s_CurIndexBufferID].Buffer; // Store current index buffer (if any)

	cRenderGL_INDEX_BUFFER IB;
    HandleGLError();
	glGenBuffers(1, &IB.Buffer);
    HandleGLError();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IB.Buffer);
    HandleGLError();
    IB.SetData(Data, IndexCount, IndexSize, true);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, hCurIndexBuffer); // Restore current index buffer
    HandleGLError();

	// Add index buffer slot or use empty
	int IndexBufferID = -1;
	if(!s_IndexBuffersFreeID.IsEmpty()) {
		IndexBufferID = s_IndexBuffersFreeID.GetAndRemoveLast();
		s_IndexBuffers[IndexBufferID] = IB;
		return IndexBufferID;
	}
    return s_IndexBuffers.Add(IB);
} // cRenderGL::AddIndexBuffer

//-----------------------------------------------------------------------------
// cRenderGL::FreeVertexBuffer
//-----------------------------------------------------------------------------
void cRenderGL::FreeVertexBuffer(const int VertexBufferID) {
	if(VertexBufferID < 0 || VertexBufferID >= s_VertexBuffers.Count() || s_VertexBuffersFreeID.Contains(VertexBufferID)) {
        return; // Invalid vertex buffer ID
	}
    if(VertexBufferID == s_CurVertexBufferID) {
        SetVertexBuffer(-1);
	}
	cRenderGL_VERTEX_BUFFER &VB = s_VertexBuffers[VertexBufferID];
    VB.Free();
    s_VertexBuffersFreeID.Add(VertexBufferID);
} // cRenderGL::FreeVertexBuffer

//-----------------------------------------------------------------------------
// cRenderGL::FreeIndexBuffer
//-----------------------------------------------------------------------------
void cRenderGL::FreeIndexBuffer(const int IndexBufferID) {
	if(IndexBufferID < 0 || IndexBufferID >= s_IndexBuffers.Count() || s_IndexBuffersFreeID.Contains(IndexBufferID)) {
        return; // Invalid index buffer ID
    }
    if(IndexBufferID == s_CurIndexBufferID) {
        SetIndexBuffer(-1);
    }
	cRenderGL_INDEX_BUFFER &IB = s_IndexBuffers[IndexBufferID];
    IB.Free();
    s_IndexBuffersFreeID.Add(IndexBufferID);
} // cRenderGL::FreeIndexBuffer

//-----------------------------------------------------------------------------
// cRenderGL::SetIndexBuffer
//-----------------------------------------------------------------------------
void cRenderGL::SetIndexBuffer(const int IndexBufferID) {
	if(IndexBufferID == s_CurIndexBufferID) {
		return;
	}
	if(IndexBufferID < 0 || IndexBufferID >= s_IndexBuffers.Count() || s_IndexBuffersFreeID.Contains(IndexBufferID)) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        HandleGLError();
		s_CurIndexBufferID = -1;
		return;
	}
    const cRenderGL_INDEX_BUFFER &IB = s_IndexBuffers[IndexBufferID];
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IB.Buffer);
    HandleGLError();
    s_CurIndexBufferID = IndexBufferID;
} // cRenderGL::SetIndexBuffer

// cRenderGL::SetDynIndexBuffer
void cRenderGL::SetDynIndexBuffer(const void *Data, const int IndexCount, const int IndexSize) {
    if(-1 == s_DynIndexBufferID) {
        s_DynIndexBufferID = AddIndexBuffer(Data, IndexCount, IndexSize);
        SetIndexBuffer(s_DynIndexBufferID);
    } else {
        SetIndexBuffer(s_DynIndexBufferID);
        cRenderGL_INDEX_BUFFER &IB = s_IndexBuffers[s_DynIndexBufferID];
        IB.SetData(Data, IndexCount, IndexSize, false);
    }
}

#ifdef COMMS_WINDOWS
#define glGetShaderiv glGetObjectParameterivARB
#define glGetProgramiv glGetObjectParameterivARB
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH GL_OBJECT_INFO_LOG_LENGTH_ARB
#endif
#define glGetShaderInfoLog glGetInfoLogARB
#define glGetProgramInfoLog glGetInfoLogARB
#endif // Windows

// CreateShader_ShowLog
static void CreateShader_ShowLog(GLuint Shader, GLuint Program, const bool Success, cStr *OptionalLog) {
	GLint LogLength = 0;
    if(Shader != 0) {
        glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &LogLength);
        HandleGLError();
    } else {
        cAssert(Program != 0);
        glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &LogLength);
        HandleGLError();
    }
	if (LogLength <= 1) { // Number of characters including the null termination character
		return;
	}
	cStr Buffer(LogLength - 1);
    if(Shader != 0) {
        glGetShaderInfoLog(Shader, LogLength, nullptr, Buffer.ToNonConstCharPtr());
        HandleGLError();
    } else {
        cAssert(Program != 0);
        glGetProgramInfoLog(Program, LogLength, nullptr, Buffer.ToNonConstCharPtr());
        HandleGLError();
    }
	Buffer.Replace('%','P');
	if(Success) {
		cLog::Message(Buffer);
	} else {
		cLog::Warning(Buffer);
		if (OptionalLog != nullptr) {
			OptionalLog->Append(Buffer);
		}
	}
}

#ifdef COMMS_WINDOWS
#define glCreateProgram glCreateProgramObjectARB
#define glCreateShader glCreateShaderObjectARB
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER GL_VERTEX_SHADER_ARB
#endif
#define glShaderSource glShaderSourceARB
#define glCompileShader glCompileShaderARB
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS GL_OBJECT_COMPILE_STATUS_ARB
#endif
#ifndef GL_GEOMETRY_SHADER
#define GL_GEOMETRY_SHADER GL_GEOMETRY_SHADER_ARB
#endif
#define glAttachShader glAttachObjectARB
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER GL_FRAGMENT_SHADER_ARB
#endif
#define glBindAttribLocation glBindAttribLocationARB
#define glLinkProgram glLinkProgramARB
#define glGetProgramiv glGetObjectParameterivARB
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS GL_OBJECT_LINK_STATUS_ARB
#endif
#define glUseProgram glUseProgramObjectARB
#ifndef GL_ACTIVE_UNIFORMS
#define GL_ACTIVE_UNIFORMS GL_OBJECT_ACTIVE_UNIFORMS_ARB
#endif
#ifndef GL_ACTIVE_UNIFORM_MAX_LENGTH
#define GL_ACTIVE_UNIFORM_MAX_LENGTH GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB
#endif
#define glGetActiveUniform glGetActiveUniformARB
#define glGetUniformLocation glGetUniformLocationARB
#define glUniform1i glUniform1iARB
#ifndef GL_SAMPLER_1D
#define GL_SAMPLER_1D GL_SAMPLER_1D_ARB
#endif
#ifndef GL_SAMPLER_2D_RECT_SHADOW
#define GL_SAMPLER_2D_RECT_SHADOW GL_SAMPLER_2D_RECT_SHADOW_ARB
#endif
#endif // Windows

//-------------------------------------------------------------------------------------------------------------------
// CreateShader
//-------------------------------------------------------------------------------------------------------------------
static bool CreateShader(const char *ShaderName, const int VertexFormatID, const char *Extra, cRenderGL_SHADER *Sh, cStr *OptionalLog) {
    HandleGLError();
	// Check shader name
	cAssert(ShaderName != nullptr);
	if(cStr::Length(ShaderName) < 1) {
		return false; // No name
	}

    cAssert(VertexFormatID >= 0 && VertexFormatID < s_VertexFormats.Count());
	if(VertexFormatID < 0 || VertexFormatID >= s_VertexFormats.Count()) {
		return false; // Invalid vertex format ID
	}
	
	// GL shader file pathname should be with the extension "glsl":
    cStr FilePn = cStr::Format("%s.glsl", ShaderName);
    if(!FilePn.StartsWith("UserPrefs/Shaders")) { // Skips logging message 2 times: Can't load file "data/Shaders/UserPrefs/Shaders/CurrentMcubes/mcubes.glsl"
        FilePn.Insert(0, "data/Shaders/");
    }
	
	//*************************************************************************
	// Loading file
	//*************************************************************************
	cStr VSText, GSText, TCSText, TESText, FSText;
	int VSLine, GSLine, TCSLine, TESLine, FSLine;
	
	if (!cRender::Stub::LoadShader(FilePn, &VSText, &GSText, &TCSText, &TESText, &FSText, &VSLine, &GSLine, &TCSLine, &TESLine, &FSLine)) {
		FilePn = cStr::Format("%s.glsl", ShaderName);
		if (!cRender::Stub::LoadShader(FilePn, &VSText, &GSText, &TCSText, &TESText, &FSText, &VSLine, &GSLine, &TCSLine, &TESLine, &FSLine)) {
			return false;
		}
	}
	
	Sh->Name = ShaderName;
	Sh->VertexFormatID = VertexFormatID;
	Sh->Extra = Extra; // We should not modify inline extra because "GetShaderID" will be unable to find the match
	cStr E = Sh->Extra;
	if(!E.IsEmpty()) {
		E.Trim(cStr::EndLn);
		if(!E.EndsWith("\n")) { // Intel Haswell Mobile requires trailing EOL
			E += "\n";
		}
	//	cLog::Message(E); // Show extra
	}
	Sh->FilePn = FilePn;
	
	Sh->Program = glCreateProgram();
    HandleGLError();
	Sh->VS = 0;
	Sh->FS = 0;

	const char *VS_PS_Version = "#version 130\n"; // GLSL function "textureLod" and storage specifiers in/out
	// are available starting from version 130. We should not use 140 because Intel Haswell Mobile doesn't support it.
	// http://en.wikipedia.org/wiki/OpenGL_Shading_Language
	// 110, 120, 130, 140, 150, 330, 400, 410, 420, 430, 440, 450
#if defined COMMS_MACOS || defined COMMS_LINUX
	VS_PS_Version = "#version 140\n"; // OpenGL 3.2 (core) requires at least version 140 (not 150)
#endif // macOS, Linux
	const char *GSVersion = "#version 150 core\n"; // "The API was expanded with geometry shaders in OpenGL 3.2 (#version 150)"
	// Any version below generates errors "No matching overloaded function found: EmitVertex/EndPrimitive"
	const char* TSVersion = "#version 410 core\n";

    const int BodyLen = 6;
	const char *Body[BodyLen];
	Body[1] = "";
	if (glExt_Supported::Include) {
		Body[1] = "#extension GL_ARB_shading_language_include : require\n"; // This solves shaders compilation error "Invalid Directive: include"
	}
	Body[2] = "#define saturate(x) clamp(x, 0.0, 1.0)\n#define lerp mix\n"; // This eliminates difference between "glsl" and "hlsl"
	Body[3] = E.ToCharPtr();
        
    //*************************************************************************
	// Compiling VS
	//*************************************************************************
	cStr Line;
	GLint VSStatus, GSStatus, TCSStatus, TESStatus, FSStatus;

	if(!VSText.IsEmpty()) {
		//cLog::Message("Compiling VS...");

		Sh->VS = glCreateShader(GL_VERTEX_SHADER);
        HandleGLError();

		Line = cStr::Format("#line %d\n", VSLine);
        Body[4] = Line.ToCharPtr();
		Body[5] = VSText.ToCharPtr();
		Body[0] = VS_PS_Version;
		
		glShaderSource(Sh->VS, BodyLen, Body, nullptr);
        HandleGLError();
		glCompileShader(Sh->VS);
        HandleGLError();
		glGetShaderiv(Sh->VS, GL_COMPILE_STATUS, &VSStatus);
        HandleGLError();
		if(VSStatus) {
			glAttachShader(Sh->Program, Sh->VS);
            HandleGLError();
		}
		CreateShader_ShowLog(Sh->VS, 0, VSStatus != 0, OptionalLog);
	} else {
		VSStatus = GL_TRUE;
	}

	//*************************************************************************
	// Compiling GS
	//*************************************************************************
	if (!GSText.IsEmpty()) {
		//cLog::Message("Compiling GS...");

		Sh->GS = glCreateShader(GL_GEOMETRY_SHADER);
		HandleGLError();

		Line = cStr::Format("#line %d\n", GSLine);
		Body[4] = Line.ToCharPtr();
		Body[5] = GSText.ToCharPtr();
		Body[0] = GSVersion;

		glShaderSource(Sh->GS, BodyLen, Body, nullptr);
		HandleGLError();
		glCompileShader(Sh->GS);
		HandleGLError();
		glGetShaderiv(Sh->GS, GL_COMPILE_STATUS, &GSStatus);
		HandleGLError();
		if (GSStatus) {
			glAttachShader(Sh->Program, Sh->GS);
			HandleGLError();
		}
		CreateShader_ShowLog(Sh->GS, 0, GSStatus != 0, OptionalLog);
	}
	else {
		GSStatus = GL_TRUE;
	}

	//*************************************************************************
	// Compiling TCS
	//*************************************************************************
	if (!TCSText.IsEmpty()) {
		//cLog::Message("Compiling TCS...");

		Sh->TCS = glCreateShader(GL_TESS_CONTROL_SHADER);
		HandleGLError();

		Line = cStr::Format("#line %d\n", TCSLine);
		Body[4] = Line.ToCharPtr();
		Body[5] = TCSText.ToCharPtr();
		Body[0] = TSVersion;

		glShaderSource(Sh->TCS, BodyLen, Body, nullptr);
		HandleGLError();
		glCompileShader(Sh->TCS);
		HandleGLError();
		glGetShaderiv(Sh->TCS, GL_COMPILE_STATUS, &TCSStatus);
		HandleGLError();
		if (TCSStatus) {
			glAttachShader(Sh->Program, Sh->TCS);
			HandleGLError();
		}
		CreateShader_ShowLog(Sh->TCS, 0, TCSStatus != 0, OptionalLog);
	}
	else {
		TCSStatus = GL_TRUE;
	}

	//*************************************************************************
	// Compiling TES
	//*************************************************************************
	if (!TESText.IsEmpty()) {
		//cLog::Message("Compiling TES...");

		Sh->TES = glCreateShader(GL_TESS_EVALUATION_SHADER);
		HandleGLError();

		Line = cStr::Format("#line %d\n", TESLine);
		Body[4] = Line.ToCharPtr();
		Body[5] = TESText.ToCharPtr();
		Body[0] = TSVersion;

		glShaderSource(Sh->TES, BodyLen, Body, nullptr);
		HandleGLError();
		glCompileShader(Sh->TES);
		HandleGLError();
		glGetShaderiv(Sh->TES, GL_COMPILE_STATUS, &TESStatus);
		HandleGLError();
		if (TESStatus) {
			glAttachShader(Sh->Program, Sh->TES);
			HandleGLError();
		}
		CreateShader_ShowLog(Sh->TES, 0, TESStatus != 0, OptionalLog);
	}
	else {
		TESStatus = GL_TRUE;
	}

	//*************************************************************************
	// Compiling FS
	//*************************************************************************
	if(!FSText.IsEmpty()) {
		//cLog::Message("Compiling FS...");
		
		Sh->FS = glCreateShader(GL_FRAGMENT_SHADER);
        HandleGLError();

		Line = cStr::Format("#line %d\n", FSLine);
        Body[4] = Line.ToCharPtr();
		Body[5] = FSText;
		Body[0] = VS_PS_Version;

        glShaderSource(Sh->FS, BodyLen, Body, nullptr);
        HandleGLError();
		glCompileShader(Sh->FS);
        HandleGLError();
		glGetShaderiv(Sh->FS, GL_COMPILE_STATUS, &FSStatus);
        HandleGLError();
		if(FSStatus) {
			glAttachShader(Sh->Program, Sh->FS);
            HandleGLError();
		}
		CreateShader_ShowLog(Sh->FS, 0, FSStatus != 0, OptionalLog);
	} else {
		FSStatus = GL_TRUE;
	}

	if(!VSStatus || !FSStatus) {
		return false;
	}

	// Bind vertex attributes
	const cVertex::Format &F = s_VertexFormats[VertexFormatID].SrcFormat;
	int i;
	for(i = 0; i < F.Count(); i++) {
		const cStr &Attrib = F[i].Name;
		if(!Attrib.IsEmpty()) {
			glBindAttribLocation(Sh->Program, i, Attrib);
            HandleGLError();
		}
	}

	//*************************************************************************
	// Linking program
	//*************************************************************************
	//cLog::Message("Linking program...");
	GLint ProgramStatus;
	glLinkProgram(Sh->Program);
    HandleGLError();
	glGetProgramiv(Sh->Program, GL_LINK_STATUS, &ProgramStatus);
    HandleGLError();
	CreateShader_ShowLog(0, Sh->Program, ProgramStatus != 0, OptionalLog);

	if(!ProgramStatus) {
		return false;
	}
    
	//*************************************************************************
	// Query samplers and auto constants
	//*************************************************************************
	GLuint hCurProgram;
	GLint NUniforms, MaxLength, Length, Size, Loc;
	cStr UniformName;
	GLenum Type;
	cRenderGL_SHADER::Sampler Sm;
	cRenderGL_SHADER::AutoConst Ac;
	cRenderGL_SHADER::Const C;
	
	hCurProgram = (-1 == s_CurShaderID) ? 0 : s_Shaders[s_CurShaderID].Program; // Store current shader (if any)
	glUseProgram(Sh->Program);
    HandleGLError();
	glGetProgramiv(Sh->Program, GL_ACTIVE_UNIFORMS, &NUniforms);
    HandleGLError();
	glGetProgramiv(Sh->Program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &MaxLength);
    HandleGLError();

	UniformName.SetLength(MaxLength);
	for(i = 0; i < NUniforms; i++) {
		glGetActiveUniform(Sh->Program, i, MaxLength, &Length, &Size, &Type, UniformName.ToNonConstCharPtr());
        HandleGLError();
		UniformName.CalcLength();
		// ATI case: uniform array always ends with "[0]" which we don't want
		if(UniformName.EndsWith("[0]")) {
			UniformName.Remove(UniformName.Length() - 3);
		}
        if(Type >= GL_SAMPLER_1D && Type <= GL_SAMPLER_2D_RECT_SHADOW) { // Uniform is "Sampler"
			Loc = glGetUniformLocation(Sh->Program, UniformName);
            HandleGLError();
			glUniform1i(Loc, Sh->Samplers.Count());
            HandleGLError();
			Sm.Name = UniformName;
			Sm.ImageUnit = Sh->Samplers.Count();
			Sh->Samplers.Add(Sm);
		} else if((Ac.Index = cRender::AutoConst::FromString(UniformName)) != -1) { // Uniform is "AutoConst"
			Ac.Loc = glGetUniformLocation(Sh->Program, UniformName);
            HandleGLError();
			Sh->AutoConsts.Add(Ac);
		} else { // Uniform is "Const"
			C.Name = UniformName;
			C.Loc = glGetUniformLocation(Sh->Program, UniformName);
            HandleGLError();
			Sh->Consts.Add(C);
		}
	}
    // 3D-Coat often sets textures w/o "cRender::GetSamplerID" call.
    // It assumes that "s_Sampler0" is on image unit 0, and "s_Sampler1" is on image unit 1.
    // But under Mac OS X Lion this is not true when shader has 3 or more samplers.
    // "s_Sampler0" could be on image unit 1, "s_Sampler1" on image unit 2, and "s_Sampler2" on image unit 0.
    // To compensate this behaviour we should sort the samplers by name.
    Sh->Samplers.Sort(cRenderGL_SHADER::Sampler::Compare);

    glUseProgram(hCurProgram); // Restore current shader
    HandleGLError();
	return true;
} // CreateShader

//---------------------------------------------------------------------------------------------
// cRenderGL::AddShader
//---------------------------------------------------------------------------------------------
int cRenderGL::AddShader(const char *ShaderName, const int VertexFormatID, const char *Extra, cStr *OptionalLog) {
#ifdef COMMS_3DCOAT
	LogNewShader(ShaderName,VertexFormatID,Extra);
#endif // COMMS_3DCOAT

	cRenderGL_SHADER Sh;
	if (!CreateShader(ShaderName, VertexFormatID, Extra, &Sh, OptionalLog)) {
		return -1;
	}
	
	// Add shader slot or use empty
	int ShaderID = -1;
	if(!s_ShadersFreeID.IsEmpty()) {
		ShaderID = s_ShadersFreeID.GetAndRemoveLast();
		s_Shaders[ShaderID] = Sh;
		return ShaderID;
	} else {
		return s_Shaders.Add(Sh);
	}
} // cRenderGL::AddShader

//-----------------------------------------------------------------------------------------------
// cRenderGL::GetShaderID
//-----------------------------------------------------------------------------------------------
int cRenderGL::GetShaderID(const char *ShaderName, const int VertexFormatID, const char *Extra, cStr *OptionalLog) {
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
		const cRenderGL_SHADER &r = s_Shaders[i];
		if(cStr::EqualsNoCase(r.Name, ShaderName)) { // Same name
			if(r.VertexFormatID == VertexFormatID) { // Same vertex format
				if(cStr::Equals(r.Extra, E)) { // Same extra
					return i; // Such shader is already loaded
				}
			}
		}
	}

	return AddShader(ShaderName, VertexFormatID, Extra, OptionalLog);
} // cRenderGL::GetShaderID

//-----------------------------------------------------------------------------
// cRenderGL::ReloadShaders
//-----------------------------------------------------------------------------
void cRenderGL::ReloadShaders() {
    cRenderGL_LoadTemplates();

	int i;
	cRenderGL_SHADER Rel;
	cStr S, E;
	int F;

	cLog::Message(cStr(60, '-'));
	SetShader(-1);
	
	int c = 0;
	for(i = 0; i < s_Shaders.Count(); i++) {
		cRenderGL_SHADER &Sh = s_Shaders[i];
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
} // cRenderGL::ReloadShaders

//-----------------------------------------------------------------------------
// cRenderGL::GetShaderConstID
//-----------------------------------------------------------------------------
int cRenderGL::GetShaderConstID(const int ShaderID, const char *ConstName) {
	cAssert(ConstName != nullptr);
	if(cStr::Length(ConstName) < 1) {
		return -1; // No name
	}

	cAssert(ShaderID >= 0 && ShaderID < s_Shaders.Count() && !s_ShadersFreeID.Contains(ShaderID));
	if(ShaderID < 0 || ShaderID >= s_Shaders.Count() || s_ShadersFreeID.Contains(ShaderID)) {
		return -1; // Invalid shader ID
	}

	const cRenderGL_SHADER &Sh = s_Shaders[ShaderID];
	int i;
	for(i = 0; i < Sh.Consts.Count(); i++) {
		const cRenderGL_SHADER::Const &C = Sh.Consts[i];
		if(cStr::Equals(C.Name, ConstName)) {
			return i;
		}
	}
	return -1;
} // cRenderGL::GetShaderConstID

//------------------------------------------------------------------------------
// cRenderGL::GetSamplerID
//------------------------------------------------------------------------------
int cRenderGL::GetSamplerID(const int ShaderID, const char *SamplerName) const {
	cAssert(SamplerName != nullptr);
	if(cStr::Length(SamplerName) < 1) {
		return -1; // No name
	}

	cAssert(ShaderID >= 0 && ShaderID < s_Shaders.Count() && !s_ShadersFreeID.Contains(ShaderID));
	if(ShaderID < 0 || ShaderID >= s_Shaders.Count() || s_ShadersFreeID.Contains(ShaderID)) {
		return -1; // Invalid shader ID
	}
	
	const cRenderGL_SHADER &Sh = s_Shaders[ShaderID];
	int i;
	for(i = 0; i < Sh.Samplers.Count(); i++) {
		const cRenderGL_SHADER::Sampler &Sm = Sh.Samplers[i];
		if(cStr::Equals(Sm.Name, SamplerName)) {
			return i;
		}
	}
	return -1;
} // cRenderGL::GetSamplerID

// cRenderGL_SetupSampler
static void cRenderGL_SetupSampler(GLenum glTarget, const cRenderGL_SAMPLER_STATE &SS) {
    HandleGLError();
	// Set wrapping mode
	glTexParameteri(glTarget, GL_TEXTURE_WRAP_S, SS.WrapS);
    HandleGLError();
#if defined COMMS_IOS || defined COMMS_TIZEN
    glTexParameteri(glTarget, GL_TEXTURE_WRAP_T, SS.WrapT);
    HandleGLError();
#else // Linux, Windows, OS X
    if(glTarget != GL_TEXTURE_1D) {
		glTexParameteri(glTarget, GL_TEXTURE_WRAP_T, SS.WrapT);
        HandleGLError();
	}
	if(GL_TEXTURE_3D == glTarget) {
		glTexParameteri(glTarget, GL_TEXTURE_WRAP_R, SS.WrapR);
        HandleGLError();
	}
#endif // COMMS_IOS || COMMS_TIZEN

	// Set filter modes
	glTexParameteri(glTarget, GL_TEXTURE_MAG_FILTER, SS.MagFilter);
    HandleGLError();
	glTexParameteri(glTarget, GL_TEXTURE_MIN_FILTER, SS.MinFilter);
    HandleGLError();

	// Setup anisotropic filtering
	if(SS.Aniso && GL_TEXTURE_2D == glTarget && glExt_Supported::MaxAnisotropy > 0) {
		glTexParameteri(glTarget, GL_TEXTURE_MAX_ANISOTROPY_EXT, glExt_Supported::MaxAnisotropy);
        HandleGLError();
	}
}

// cRenderGL::SetSamplerState
void cRenderGL::SetSamplerState(const int TextureID, const int SamplerStateID) {
	if(SamplerStateID < 0 || SamplerStateID >= s_SamplerStates.Count()) {
		return;
	}
	if(TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return;
	}
	cRenderGL_TEXTURE &T = s_Textures[TextureID];
	if(SamplerStateID == T.DesiredSamplerStateID) {
		return;
	}
	T.DesiredSamplerStateID = SamplerStateID;
}

#ifdef COMMS_WINDOWS
#define glActiveTexture glActiveTextureARB
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 GL_TEXTURE0_ARB
#endif
#endif // Windows

//----------------------------------------------------------------------------------------------
// cRenderGL::SetTexture
//----------------------------------------------------------------------------------------------
void cRenderGL::SetTexture(const int SamplerID, const int TextureID) {
	if(-1 == s_CurShaderID) {
        return;
	}
	const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
	if(SamplerID < 0 || SamplerID >= Sh.Samplers.Count()) {
		return;
	}
	const cRenderGL_SHADER::Sampler &Sm = Sh.Samplers[SamplerID];
	if(TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return;
	}
	cRenderGL_TEXTURE &T = s_Textures[TextureID];

	cAssert(T.DesiredSamplerStateID != -1);
	if(T.DesiredSamplerStateID < 0 || T.DesiredSamplerStateID >= s_SamplerStates.Count()) {
		return;
	}
	cRenderGL_SAMPLER_STATE SS = s_SamplerStates[T.DesiredSamplerStateID];

    HandleGLError();
	glActiveTexture(GL_TEXTURE0 + Sm.ImageUnit); // We should change active texture before potential reload
    HandleGLError();
	if(0 == T.glTarget) { // Unloaded texture
		cAssert(!T.FilePn.IsEmpty());
		ReloadTexture(TextureID);
	}
    // glEnable(GL_TEXTURE_1D/2D/3D/CUBE_MAP) is a directive to the fixed-function pipeline which generates error in core profile
	//glEnable(T.glTarget);
    HandleGLError();
	glBindTexture(T.glTarget, T.glTexID);
    HandleGLError();
	if(T.BindedSamplerStateID != T.DesiredSamplerStateID) {
		T.BindedSamplerStateID = T.DesiredSamplerStateID;
		if(SS.MipMaps && T.MipMapCount <= 1) {
			// Sampler state will use mip maps, but the texture doesn't have them.
			// Therefor we should degrade the filter to "Linear".
			SS.MinFilter = GL_LINEAR;
			SS.MagFilter = GL_LINEAR;
			SS.Aniso = false;
		}
		cRenderGL_SetupSampler(T.glTarget, SS);
	}
} // cRenderGL::SetTexture

//-----------------------------------------------------------------------------
// Bindless texture
//-----------------------------------------------------------------------------
bool cRenderGL::IsSupported_BindlessTexture() {
	return glExt_Supported::BindlessTexture;
}

//-----------------------------------------------------------------------------
// cRenderGL::SetShader
//-----------------------------------------------------------------------------
void cRenderGL::SetShader(const int ShaderID) {
	if(ShaderID == s_CurShaderID) {
		return;
	}
	if(ShaderID < 0 || ShaderID >= s_Shaders.Count() || s_ShadersFreeID.Contains(ShaderID)) {
		glUseProgram(0);
        HandleGLError();
		s_CurShaderID = -1;
		return;
	}
	
	const cRenderGL_SHADER &Sh = s_Shaders[ShaderID];

	if(!Sh.IsValid()) {
		glUseProgram(0);
        HandleGLError();
		s_CurShaderID = -1;
		return;
	}
	
	glUseProgram(Sh.Program);
    HandleGLError();
	s_CurShaderID = ShaderID;
	
	SetShaderAutoConstants();
} // cRenderGL::SetShader

#ifdef COMMS_WINDOWS
#define glUniform1f glUniform1fARB
#define glUniform1fv glUniform1fvARB
#define glUniform2fv glUniform2fvARB
#define glUniform3fv glUniform3fvARB
#define glUniform4fv glUniform4fvARB
#define glUniformMatrix3fv glUniformMatrix3fvARB
#define glUniformMatrix4fv glUniformMatrix4fvARB
#endif // Windows

//-----------------------------------------------------------------------------
// cRenderGL::SetShaderAutoConstants
//-----------------------------------------------------------------------------
void cRenderGL::SetShaderAutoConstants() {
	if(-1 == s_CurShaderID) {
		return; // There is no active shader
	}
	
	int i;
	const cRenderGL_SHADER &S = s_Shaders[s_CurShaderID];
	
	for(i = 0; i < S.AutoConsts.Count(); i++) {
		const cRenderGL_SHADER::AutoConst &Ac = S.AutoConsts[i];
		
		switch(Ac.Index) {
			case cRender::AutoConst::ViewportWidth:
				glUniform1f(Ac.Loc, cRender::GetViewer()->GetViewport().GetWidth());
                HandleGLError();
				break;
			case cRender::AutoConst::ViewportHeight:
				glUniform1f(Ac.Loc, cRender::GetViewer()->GetViewport().GetHeight());
                HandleGLError();
				break;
			case cRender::AutoConst::ViewportInvWidth:
				glUniform1f(Ac.Loc, 1.0f / cRender::GetViewer()->GetViewport().GetWidth());
                HandleGLError();
				break;
			case cRender::AutoConst::ViewportInvHeight:
				glUniform1f(Ac.Loc, 1.0f / cRender::GetViewer()->GetViewport().GetHeight());
                HandleGLError();
				break;
			case cRender::AutoConst::WorldMatrix:
				glUniformMatrix4fv(Ac.Loc, 1, GL_FALSE, cMat4::Transpose(cRender::GetWorldMatrix()).ToFloatPtr());
                HandleGLError();
				break;
			case cRender::AutoConst::WorldMatrixInverse:
				glUniformMatrix4fv(Ac.Loc, 1, GL_FALSE, cMat4::Transpose(cRender::GetWorldMatrixInverse()).ToFloatPtr());
                HandleGLError();
				break;
			case cRender::AutoConst::ViewerPos:
				if(cRender::GetViewer()->GetOrthoProj()) {
					cVec3 u = cRender::GetViewer()->GetForward() * (-1e8f);
					glUniform3fv(Ac.Loc, 1, u.ToFloatPtr());
                    HandleGLError();
				} else {
					glUniform3fv(Ac.Loc, 1, cRender::GetViewer()->GetPos().ToFloatPtr());
                    HandleGLError();
				}
				break;
			case cRender::AutoConst::WorldViewMatrix:
				glUniformMatrix4fv(Ac.Loc, 1, GL_FALSE, cMat4::Transpose(cMat4::Mul(cRender::GetWorldMatrix(), cRender::GetViewer()->GetViewMatrix())).ToFloatPtr());
                HandleGLError();
				break;
			case cRender::AutoConst::ViewProjectionMatrix:
				glUniformMatrix4fv(Ac.Loc, 1, GL_FALSE, cMat4::Transpose(cRender::GetViewer()->GetViewProjectionMatrix()).ToFloatPtr());
                HandleGLError();
				break;
			case cRender::AutoConst::WorldViewProjectionMatrix:
                glUniformMatrix4fv(Ac.Loc, 1, GL_FALSE, cMat4::Transpose(cMat4::Mul(cRender::GetWorldMatrix(), cRender::GetViewer()->GetViewProjectionMatrix())).ToFloatPtr());
                HandleGLError();
				break;
			case cRender::AutoConst::ProjectionMatrix:
				glUniformMatrix4fv(Ac.Loc, 1, GL_FALSE, cMat4::Transpose(cRender::GetViewer()->GetProjectionMatrix()).ToFloatPtr());
                HandleGLError();
				break;
			case cRender::AutoConst::ScreenMatrix:
				glUniformMatrix4fv(Ac.Loc, 1, GL_FALSE, cMat4::Transpose(cRender::GetViewer()->GetScreenMatrix()).ToFloatPtr());
                HandleGLError();
				break;
			case cRender::AutoConst::ScreenMatrixInverse:
				glUniformMatrix4fv(Ac.Loc, 1, GL_FALSE, cMat4::Transpose(cRender::GetViewer()->GetScreenMatrixInverse()).ToFloatPtr());
                HandleGLError();
				break;
			case cRender::AutoConst::NormalMatrix:
				glUniformMatrix3fv(Ac.Loc, 1, GL_FALSE, cMat3::Transpose(cRender::GetNormalMatrix()).ToFloatPtr());
                HandleGLError();
				break;
			case cRender::AutoConst::NormalViewMatrix:
				glUniformMatrix3fv(Ac.Loc, 1, GL_FALSE, cMat3::Transpose(cMat3::Mul(cRender::GetNormalMatrix(), cRender::GetViewer()->GetViewMatrix().ToMat3())).ToFloatPtr());
                HandleGLError();
				break;
			case cRender::AutoConst::TimeSec:
				glUniform1f(Ac.Loc, cTimer::GetTimeSec());
                HandleGLError();
				break;
			case cRender::AutoConst::FrameTimeSec:
				glUniform1f(Ac.Loc, cTimer::GetFrameTimeSec());
                HandleGLError();
				break;
			case cRender::AutoConst::TextureMatrix0:
				glUniformMatrix4fv(Ac.Loc, 1, GL_FALSE, cMat4::Transpose(cRender::GetTextureMatrix(0)).ToFloatPtr());
                HandleGLError();
				break;
			case cRender::AutoConst::TextureMatrix1:
				glUniformMatrix4fv(Ac.Loc, 1, GL_FALSE, cMat4::Transpose(cRender::GetTextureMatrix(1)).ToFloatPtr());
                HandleGLError();
				break;
			case cRender::AutoConst::TextureMatrix2:
				glUniformMatrix4fv(Ac.Loc, 1, GL_FALSE, cMat4::Transpose(cRender::GetTextureMatrix(2)).ToFloatPtr());
                HandleGLError();
				break;
			case cRender::AutoConst::TextureMatrix3:
				glUniformMatrix4fv(Ac.Loc, 1, GL_FALSE, cMat4::Transpose(cRender::GetTextureMatrix(3)).ToFloatPtr());
                HandleGLError();
				break;
			default:
				break;
		}
	}
} // cRenderGL::SetShaderAutoConstants

//-----------------------------------------------------------------------------
// cRenderGL::FreeShader
//-----------------------------------------------------------------------------
void cRenderGL::FreeShader(const int ShaderID) {
	if(ShaderID < 0 || ShaderID >= s_Shaders.Count() || s_ShadersFreeID.Contains(ShaderID)) {
		return; // Invalid shader ID
    }
	if(ShaderID == s_CurShaderID) {
        SetShader(-1);
    }
	cRenderGL_SHADER &S = s_Shaders[ShaderID];
	S.Free();
	S.Clear();
    s_ShadersFreeID.Add(ShaderID);
} // cRenderGL::FreeShader

//-----------------------------------------------------------------------------
// SetShaderConst : (..., const int)
//-----------------------------------------------------------------------------
void cRenderGL::SetShaderConst(const int ConstID, const int Value) {
	if(s_CurShaderID != -1) {
		const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderGL_SHADER::Const &C = Sh.Consts[ConstID];
			glUniform1i(C.Loc, Value);
            HandleGLError();
		}
	}
}

//-----------------------------------------------------------------------------
// SetShaderConst : (..., const float)
//-----------------------------------------------------------------------------
void cRenderGL::SetShaderConst(const int ConstID, const float Value) {
	if(s_CurShaderID != -1) {
		const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderGL_SHADER::Const &C = Sh.Consts[ConstID];
			glUniform1f(C.Loc, Value);
            HandleGLError();
		}
	}
}

//-----------------------------------------------------------------------------
// SetShaderConst : (..., const cVec2 &)
//-----------------------------------------------------------------------------
void cRenderGL::SetShaderConst(const int ConstID, const cVec2 &Value) {
	if(s_CurShaderID != -1) {
		const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderGL_SHADER::Const &C = Sh.Consts[ConstID];
			glUniform2fv(C.Loc, 1, Value.ToFloatPtr());
            HandleGLError();
		}
	}
}

//-----------------------------------------------------------------------------
// SetShaderConst : (..., const cVec3 &)
//-----------------------------------------------------------------------------
void cRenderGL::SetShaderConst(const int ConstID, const cVec3 &Value) {
	if(s_CurShaderID != -1) {
		const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderGL_SHADER::Const &C = Sh.Consts[ConstID];
			glUniform3fv(C.Loc, 1, Value.ToFloatPtr());
            HandleGLError();
		}
	}
}

//-----------------------------------------------------------------------------
// SetShaderConst : (..., const cVec4 &)
//-----------------------------------------------------------------------------
void cRenderGL::SetShaderConst(const int ConstID, const cVec4 &Value) {
	if(s_CurShaderID != -1) {
		const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderGL_SHADER::Const &C = Sh.Consts[ConstID];
			glUniform4fv(C.Loc, 1, Value.ToFloatPtr());
            HandleGLError();
		}
	}
}

//-----------------------------------------------------------------------------
// SetShaderConst : (..., const cMat3 &)
//-----------------------------------------------------------------------------
void cRenderGL::SetShaderConst(const int ConstID, const cMat3 &Value) {
	if(s_CurShaderID != -1) {
		const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderGL_SHADER::Const &C = Sh.Consts[ConstID];
			glUniformMatrix3fv(C.Loc, 1, GL_FALSE, cMat3::Transpose(Value).ToFloatPtr());
            HandleGLError();
		}
	}
}

//-----------------------------------------------------------------------------
// SetShaderConst : (..., const cMat4 &)
//-----------------------------------------------------------------------------
void cRenderGL::SetShaderConst(const int ConstID, const cMat4 &Value) {
	if(s_CurShaderID != -1) {
		const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderGL_SHADER::Const &C = Sh.Consts[ConstID];
			glUniformMatrix4fv(C.Loc, 1, GL_FALSE, cMat4::Transpose(Value).ToFloatPtr());
            HandleGLError();
		}
	}
}

//--------------------------------------------------------------------------------------
// SetShaderConst : (..., const float *, ...)
//--------------------------------------------------------------------------------------
void cRenderGL::SetShaderConst(const int ConstID, const float *Array, const int Count) {
	if(s_CurShaderID != -1) {
		const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderGL_SHADER::Const &C = Sh.Consts[ConstID];
			glUniform1fv(C.Loc, Count, Array);
            HandleGLError();
		}
	}
}

//--------------------------------------------------------------------------------------
// SetShaderConst : (..., const cVec2 *, ...)
//--------------------------------------------------------------------------------------
void cRenderGL::SetShaderConst(const int ConstID, const cVec2 *Array, const int Count) {
	if(s_CurShaderID != -1) {
		const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderGL_SHADER::Const &C = Sh.Consts[ConstID];
			glUniform2fv(C.Loc, Count, (const float *)Array);
            HandleGLError();
		}
	}
}

//--------------------------------------------------------------------------------------
// SetShaderConst : (..., const cVec3 *, ...)
//--------------------------------------------------------------------------------------
void cRenderGL::SetShaderConst(const int ConstID, const cVec3 *Array, const int Count) {
	if(s_CurShaderID != -1) {
		const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderGL_SHADER::Const &C = Sh.Consts[ConstID];
			glUniform3fv(C.Loc, Count, (const float *)Array);
            HandleGLError();
		}
	}
}

//--------------------------------------------------------------------------------------
// SetShaderConst : (..., const cVec4 *, ...)
//--------------------------------------------------------------------------------------
void cRenderGL::SetShaderConst(const int ConstID, const cVec4 *Array, const int Count) {
	if(s_CurShaderID != -1) {
		const cRenderGL_SHADER &Sh = s_Shaders[s_CurShaderID];
		if(ConstID >= 0 && ConstID < Sh.Consts.Count()) {
			const cRenderGL_SHADER::Const &C = Sh.Consts[ConstID];
			glUniform4fv(C.Loc, Count, (const float *)Array);
            HandleGLError();
		}
	}
}


//*****************************************************************************
// cRenderGL_MIN_FILTER
//*****************************************************************************
static const GLenum cRenderGL_MIN_FILTER[] = {
	GL_NEAREST,					// Nearest
	GL_LINEAR,					// Linear
	GL_LINEAR_MIPMAP_NEAREST,	// Bilinear
	GL_LINEAR_MIPMAP_LINEAR,	// Trilinear
	GL_LINEAR_MIPMAP_NEAREST,	// Bilinear_Aniso
	GL_LINEAR_MIPMAP_LINEAR		// Trilinear_Aniso
}; // cRenderGL_MIN_FILTER

// cRenderGL_ADDRESS_MODE
static const GLenum cRenderGL_ADDRESS_MODE[] = {
	GL_REPEAT,
	GL_CLAMP_TO_EDGE, // Added in 1998: never samples a border texel (color is derived only from texels at the edge of the texture image)
	GL_MIRRORED_REPEAT
	// "GL_CLAMP" was initially in 1994: on the edge of the texture image takes half values from within the texture image, and the other half from the texture border.
    // It was deprecated and removed in OpenGL 3.2 Core in 2009 (Core Profile is required under macOS).
};

//------------------------------------------------------------------------------------------------------------------------------------------------
// cRenderGL::GetSamplerStateID
//------------------------------------------------------------------------------------------------------------------------------------------------
int cRenderGL::GetSamplerStateID(const cFilter::Enum Filter, const cAddressMode::Enum s, const cAddressMode::Enum t, const cAddressMode::Enum r) {
	cRenderGL_SAMPLER_STATE SS;
	
	SS.MinFilter = cRenderGL_MIN_FILTER[Filter];
	SS.MagFilter = cFilter::Nearest == Filter ? GL_NEAREST : GL_LINEAR;
	SS.WrapS = cRenderGL_ADDRESS_MODE[s];
	SS.WrapT = cRenderGL_ADDRESS_MODE[t];
	SS.WrapR = cRenderGL_ADDRESS_MODE[r];
	SS.Aniso = cFilter::HasAniso(Filter);
	SS.MipMaps = cFilter::HasMipMaps(Filter);

	// Searching added sampler states
	int i;
	for(i = 0; i < s_SamplerStates.Count(); i++) {
		const cRenderGL_SAMPLER_STATE &r = s_SamplerStates[i];
		if(cRenderGL_SAMPLER_STATE::Equals(r, SS)) {
			return i;
		}
	}

	return s_SamplerStates.Add(SS);
} // cRenderGL::GetSamplerStateID

//-----------------------------------------------------------------------------------------------------------------------------------------------------
// cRenderGL::GetBlendStateID
//-----------------------------------------------------------------------------------------------------------------------------------------------------
int cRenderGL::GetBlendStateID(const cBlendFactor::Enum SrcFactor, const cBlendFactor::Enum DstFactor, const cBlendMode::Enum Mode, const dword Mask) {
	int i;
	for(i = 0; i < s_BlendStates.Count(); i++) {
		const cRenderGL_BLEND_STATE &r = s_BlendStates[i];
		if(r.SrcFactor == SrcFactor && r.DstFactor == DstFactor && r.Mode == Mode && r.Mask == Mask) {
			return i;
		}
	}
	cRenderGL_BLEND_STATE BS;
	BS.SrcFactor = SrcFactor;
	BS.DstFactor = DstFactor;
	BS.Mode = Mode;
	BS.Mask = Mask;
	BS.EnableBlend = SrcFactor != cBlendFactor::One || DstFactor != cBlendFactor::Zero;
	return s_BlendStates.Add(BS);
} // cRenderGL::GetBlendStateID

//*****************************************************************************
// cRenderGL_BLEND_FACTOR
//*****************************************************************************
static const GLenum cRenderGL_BLEND_FACTOR[] = {
	GL_ZERO,					// cBlendFactor::Zero
	GL_ONE,						// cBlendFactor::One
	GL_SRC_COLOR,				// cBlendFactor::SrcColor
	GL_ONE_MINUS_SRC_COLOR,		// cBlendFactor::OneMinusSrcColor
	GL_SRC_ALPHA,				// cBlendFactor::SrcAlpha
	GL_ONE_MINUS_SRC_ALPHA,		// cBlendFactor::OneMinusSrcAlpha
	GL_DST_ALPHA,				// cBlendFactor::DstAlpha
	GL_ONE_MINUS_DST_ALPHA,		// cBlendFactor::OneMinusDstAlpha
	GL_DST_COLOR,				// cBlendFactor::DstColor
	GL_ONE_MINUS_DST_COLOR,		// cBlendFactor::OneMinusDstColor
	GL_SRC_ALPHA_SATURATE		// cBlendFactor::SrcAlphaSat
}; // cRenderGL_BLEND_FACTOR

#ifdef COMMS_TIZEN
#define GL_MIN_EXT 0
#define GL_MAX_EXT 0
#endif // COMMS_TIZEN

#if defined COMMS_IOS || defined COMMS_TIZEN
// iPad 2 supports extension "GL_EXT_blend_minmax"
#define GL_MIN GL_MIN_EXT
#define GL_MAX GL_MAX_EXT
#endif // COMMS_IOS || COMMS_TIZEN

//*****************************************************************************
// cRenderGL_BLEND_MODE
//*****************************************************************************
static const GLenum cRenderGL_BLEND_MODE[] = {
	GL_FUNC_ADD,				// cBlendMode::Add
	GL_FUNC_SUBTRACT,			// cBlendMode::Subtract
	GL_FUNC_REVERSE_SUBTRACT,	// cBlendMode::ReverseSubtract
	GL_MIN,						// cBlendMode::Min
	GL_MAX						// cBlendMode::Max
}; // cRenderGL_BLEND_MODE

//-----------------------------------------------------------------------------
// cRenderGL::SetBlendState
//-----------------------------------------------------------------------------
void cRenderGL::SetBlendState(const int BlendStateID) {
    HandleGLError();
	cAssert(BlendStateID >= -1 && BlendStateID < s_BlendStates.Count());
	if(BlendStateID < -1 || BlendStateID >= s_BlendStates.Count()) {
		return;
	}

	GLboolean Red = GL_TRUE, Green = GL_TRUE, Blue = GL_TRUE, Alpha = GL_TRUE;

	if(-1 == BlendStateID) {
		glDisable(GL_BLEND);
        HandleGLError();
	} else {
		const cRenderGL_BLEND_STATE &BS = s_BlendStates[BlendStateID];
		if(!BS.EnableBlend) {
			glDisable(GL_BLEND);
            HandleGLError();
		} else {
			glEnable(GL_BLEND);
            HandleGLError();
			glBlendFunc(cRenderGL_BLEND_FACTOR[BS.SrcFactor], cRenderGL_BLEND_FACTOR[BS.DstFactor]);
            HandleGLError();
			glBlendEquation(cRenderGL_BLEND_MODE[BS.Mode]);
            HandleGLError();
		}
		
		Red = (BS.Mask & cBlendMask::Red) != 0;
		Green = (BS.Mask & cBlendMask::Green) != 0;
		Blue = (BS.Mask & cBlendMask::Blue) != 0;
		Alpha = (BS.Mask & cBlendMask::Alpha) != 0;
	}
	glColorMask(Red, Green, Blue, Alpha);
    HandleGLError();
} // cRenderGL::SetBlendState

#if defined COMMS_IOS || defined COMMS_TIZEN
#define RED_EXT 0x1903
#define RG_EXT 0x8227
#define GL_DEPTH24_STENCIL8 GL_DEPTH24_STENCIL8_OES
#define GL_DEPTH_COMPONENT24 GL_DEPTH_COMPONENT24_OES
#define GL_RGB8 GL_RGB
#define GL_RGBA8 GL_RGBA
#define GL_RGB16 0
#define GL_RGBA16 0
#define GL_RGB16F_ARB 0
#define GL_RGBA16F_ARB 0
#define GL_RGB32F_ARB 0
#define GL_RGBA32F_ARB 0
#define GL_RGB5 0
#define GL_RGB10_A2 0
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0
#else // Linux, Windows, OS X
#define GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG 0
#define GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG 0
#endif // COMMS_IOS || COMMS_TIZEN

#ifdef COMMS_WINDOWS
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif
#ifndef GL_RGB16F
#define GL_RGB16F GL_RGB16F_ARB
#endif
#ifndef GL_RGBA16F
#define GL_RGBA16F GL_RGBA16F_ARB
#endif
#ifndef GL_RGB32F
#define GL_RGB32F GL_RGB32F_ARB
#endif
#ifndef GL_RGBA32F
#define GL_RGBA32F GL_RGBA32F_ARB
#endif
#endif // Windows

//*****************************************************************************
// cRenderGL_INTERNAL_FORMAT
//*****************************************************************************
static const GLint cRenderGL_INTERNAL_FORMAT[cFormat::Count] = {
	0,								// None
#ifdef COMMS_WINDOWS
    GL_INTENSITY8,
#else // macOS, Linux
    GL_R8,                          // R8
#endif

#ifdef COMMS_WINDOWS
    GL_LUMINANCE8_ALPHA8,
#else // macOS, Linux
    GL_RG8,                         // Rg8
#endif

    GL_RGB8,						// Rgb8
	GL_RGBA8,						// Rgba8

#ifdef COMMS_WINDOWS
    GL_INTENSITY16,
#else // macOS, Linux
    GL_R16,                         // R16
#endif

#ifdef COMMS_WINDOWS
    GL_LUMINANCE16_ALPHA16,
#else // macOS, Linux
    GL_RG16,                         // Rg16
#endif

	GL_RGB16,						// Rgb16
	GL_RGBA16,						// Rgba16

#ifdef COMMS_WINDOWS
    GL_INTENSITY16F_ARB,
#else // macOS, Linux
    GL_R16F,                        // R16f
#endif


#ifdef COMMS_WINDOWS
    GL_LUMINANCE_ALPHA16F_ARB,
#else // macOS, Linux
    GL_RG16F,                       // Rg16f
#endif

	GL_RGB16F,                      // Rgb16f
	GL_RGBA16F,                     // Rgba16f

#ifdef COMMS_WINDOWS
    GL_INTENSITY32F_ARB,
#else // macOS, Linux
    GL_R32F,                        // R32f
#endif

#ifdef COMMS_WINDOWS
    GL_LUMINANCE_ALPHA32F_ARB,
#else // macOS, Linux
    GL_RG32F,                       // Rg32f
#endif

	GL_RGB32F,                      // Rgb32f
	GL_RGBA32F,                     // Rgba32f

	GL_DEPTH_COMPONENT16,			// Depth16
	GL_DEPTH_COMPONENT24,           // Depth24
	GL_DEPTH24_STENCIL8,            // Depth24Stencil8
	
	GL_COMPRESSED_RGB_S3TC_DXT1_EXT,	// Dxt1
	GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,	// Dxt3
	GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,	// Dxt5
    
    GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG, // PVRTC4
    GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG // PVRTC4_Alpha
}; // cRenderGL_INTERNAL_FORMAT

//*****************************************************************************
// cRenderGL_SRC_FORMAT
//*****************************************************************************
static const GLenum cRenderGL_SRC_FORMAT[] = {
	0,                          // 0 channels
#ifdef COMMS_WINDOWS
	GL_LUMINANCE,
#else // macOS, Linux
    GL_RED,                     // 1 channel
#endif
#ifdef COMMS_WINDOWS
	GL_LUMINANCE_ALPHA,
#else // macOS, Linux
    GL_RG,                      // 2 channels
#endif
	GL_RGB,                     // 3 channels
	GL_RGBA                     // 4 channels
}; // cRenderGL_SRC_FORMAT

#ifdef COMMS_WINDOWS
#ifndef GL_HALF_FLOAT
#define GL_HALF_FLOAT 0x140B
#endif
#endif // Windows

//*****************************************************************************
// cRenderGL_SRC_TYPE
//*****************************************************************************
static const GLenum cRenderGL_SRC_TYPE[cFormat::Count] = {
	0,								// None

	GL_UNSIGNED_BYTE,				// R8
	GL_UNSIGNED_BYTE,				// Rg8
	GL_UNSIGNED_BYTE,				// Rgb8
	GL_UNSIGNED_BYTE,				// Rgba8

	GL_UNSIGNED_SHORT,				// R16
	GL_UNSIGNED_SHORT,				// Rg16
	GL_UNSIGNED_SHORT,				// Rgb16
	GL_UNSIGNED_SHORT,				// Rgba16

	GL_HALF_FLOAT,                  // R16f
	GL_HALF_FLOAT,                  // Rg16f
	GL_HALF_FLOAT,                  // Rgb16f
	GL_HALF_FLOAT,                  // Rgba16f

	GL_FLOAT,						// R32f
	GL_FLOAT,						// Rg32f
	GL_FLOAT,						// Rgb32f
	GL_FLOAT,						// Rgba32f
	
	0,								// Depth16
	0,								// Depth24
	0,								// Depth24Stencil8
	
	0,								// Dxt1
	0,								// Dxt3
	0,								// Dxt5

    0,                              // PVRTC4
    0                               // PVRTC4_Alpha
}; // cRenderGL_SRC_TYPE

#ifdef COMMS_WINDOWS
#define glBindRenderbuffer glBindRenderbufferEXT
#define glRenderbufferStorage glRenderbufferStorageEXT
#endif // Windows

//------------------------------------------------------------------------------------------------
// cRenderGL::SetRenderTargetSize
//------------------------------------------------------------------------------------------------
void cRenderGL::SetRenderTargetSize(const int RenderTargetID, const int Width, const int Height) {
    HandleGLError();
	cAssert(RenderTargetID >= 0 && RenderTargetID < s_Textures.Count() && !s_TexturesFreeID.Contains(RenderTargetID));
	if(RenderTargetID < 0 || RenderTargetID >= s_Textures.Count() || s_TexturesFreeID.Contains(RenderTargetID)) {
		return;
	}
	
	const int W = cMath::Min(Width, (int)glExt_Supported::FrameBufferMaxSize);
	const int H = cMath::Min(Height, (int)glExt_Supported::FrameBufferMaxSize);
	
	int i;
	cRenderGL_TEXTURE &T = s_Textures[RenderTargetID];
	if(W == T.Width && H == T.Height) {
		return; // Same size
	}
	
	T.Width = W;
	T.Height = H;
	
	if(GL_RENDERBUFFER == T.glTarget) { // Render depth
		glBindRenderbuffer(GL_RENDERBUFFER, T.glDepthID);
        HandleGLError();
		glRenderbufferStorage(GL_RENDERBUFFER, cRenderGL_INTERNAL_FORMAT[T.Format], W, H);
        HandleGLError();
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
        HandleGLError();
	} else { // Render target
		GLenum InternalFormat = cRenderGL_INTERNAL_FORMAT[T.Format];
		GLenum SrcFormat = cRenderGL_SRC_FORMAT[cFormat::ChannelCount(T.Format)];
		GLenum SrcType = cRenderGL_SRC_TYPE[T.Format];

		if(cFormat::IsFloat(T.Format)) {
			SrcType = GL_FLOAT;
		}

		glBindTexture(T.glTarget, T.glTexID);
        HandleGLError();
		if(T.CubeMap) {
			for(i = GL_TEXTURE_CUBE_MAP_POSITIVE_X; i <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z; i++) {
				glTexImage2D(i, 0, InternalFormat, W, H, 0, SrcFormat, SrcType, nullptr);
                HandleGLError();
			}
		} else {
			glTexImage2D(T.glTarget, 0, InternalFormat, W, H, 0, SrcFormat, SrcType, nullptr);
            HandleGLError();
		}

	}
} // cRenderGL::SetRenderTargetSize

//------------------------------------------------------------------------------------------------------------------------------------
// cRenderGL::AddRenderTarget
//------------------------------------------------------------------------------------------------------------------------------------
int cRenderGL::AddRenderTarget(const int Width, const int Height, const cFormat::Enum Format, const bool CubeMap, const int Samples) {
    HandleGLError();
	if(cFormat::IsCompressed(Format)) {
		return -1;
	}

	cRenderGL_TEXTURE T;
	T.Width = 0;
	T.Height = 0;
	T.Depth = CubeMap ? 0 : 1;
	T.Format = Format;
	T.CubeMap = CubeMap;
	T.RenderTarget = true;

	T.glTarget = CubeMap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
	glGenTextures(1, &T.glTexID);
    HandleGLError();
	glBindTexture(T.glTarget, T.glTexID);
    HandleGLError();

	static int SS = GetSamplerStateID(cFilter::Nearest, cAddressMode::Clamp, cAddressMode::Clamp, cAddressMode::Clamp);
	cRenderGL_SetupSampler(T.glTarget, s_SamplerStates[SS]); // Render target requires initial sampler state
	
	int ID = -1;
	if(!s_TexturesFreeID.IsEmpty()) {
		ID = s_TexturesFreeID.GetAndRemoveLast();
		s_Textures[ID] = T;
	} else {
		ID = s_Textures.Add(T);
	}
	SetRenderTargetSize(ID, Width, Height);
	
	return ID;
} // cRenderGL::AddRenderTarget

#ifdef COMMS_WINDOWS
#define glGenRenderbuffers glGenRenderbuffersEXT
#endif // Windows

//-----------------------------------------------------------------------------------
// cRenderGL::AddRenderDepth
//-----------------------------------------------------------------------------------
int cRenderGL::AddRenderDepth(const int RenderTargetID, const cFormat::Enum Format) {
	cAssert(cFormat::IsDepth(Format));
	if(!cFormat::IsDepth(Format)) {
		return -1;
	}
	cAssert(RenderTargetID >= 0 && RenderTargetID < s_Textures.Count() && !s_TexturesFreeID.Contains(RenderTargetID));
	if(RenderTargetID < 0 || RenderTargetID >= s_Textures.Count() || s_TexturesFreeID.Contains(RenderTargetID)) {
		return -1;
	}
	
	cRenderGL_TEXTURE T;
	T.Width = 0;
	T.Height = 0;
	T.Depth = 0;
	T.Format = Format;
	T.CubeMap = false;
	T.glTarget = GL_RENDERBUFFER;

    HandleGLError();
	glGenRenderbuffers(1, &T.glDepthID);
    HandleGLError();
	
	int ID = -1;
	if(!s_TexturesFreeID.IsEmpty()) {
		ID = s_TexturesFreeID.GetAndRemoveLast();
		s_Textures[ID] = T;
	} else {
		ID = s_Textures.Add(T);
	}

	const cRenderGL_TEXTURE &R = s_Textures[RenderTargetID];
	cAssert(R.RenderTarget);
	SetRenderTargetSize(ID, R.Width, R.Height);
	
	return ID;
} // cRenderGL::AddRenderDepth
    
#ifdef COMMS_WINDOWS
#define glGenFramebuffers glGenFramebuffersEXT
#define glBindFramebuffer glBindFramebufferEXT
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#endif
#define glFramebufferTexture2D glFramebufferTexture2DEXT
#define glFramebufferRenderbuffer glFramebufferRenderbufferEXT
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT GL_DEPTH_ATTACHMENT_EXT
#endif
#ifndef GL_STENCIL_ATTACHMENT
#define GL_STENCIL_ATTACHMENT GL_STENCIL_ATTACHMENT_EXT
#endif
#endif // Windows

//----------------------------------------------------------------------------------------------------------------------------------------------------
// cRenderGL::SetRenderTargets
//----------------------------------------------------------------------------------------------------------------------------------------------------
void cRenderGL::SetRenderTargets(const int *ColorRenderTargetIDs, const int ColorRenderTargetCount, const int DepthRenderTargetID, const int *Faces) {
    HandleGLError();
	int i;
	if(nullptr == ColorRenderTargetIDs) {
#ifdef COMMS_IOS
        cIosMain_RestoreFrameBuffer();
#elif defined COMMS_TIZEN
		cTizenMain_RestoreFrameBuffer();
#else // Linux, Windows, OS X
        for(i = 0; i < glExt_Supported::DrawBuffers.Count(); i++) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, 0, 0);
            HandleGLError();
		}
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        HandleGLError();
#endif // COMMS_IOS, COMMS_TIZEN
        cRenderGL_DrawingIntoFrameBuffer = true;
		return;
	}
#if !defined COMMS_IOS && !defined COMMS_TIZEN
	if(0 == glExt_Supported::FBO) {
		glGenFramebuffers(1, &glExt_Supported::FBO);
        HandleGLError();
	}
	glBindFramebuffer(GL_FRAMEBUFFER, glExt_Supported::FBO);
    HandleGLError();
#endif // !COMMS_IOS && !COMMS_TIZEN
	int ColorRT;
	for(i = 0; i < ColorRenderTargetCount; i++) {
		ColorRT = ColorRenderTargetIDs[i];
		cAssert(ColorRT >= 0 && ColorRT < s_Textures.Count() && !s_TexturesFreeID.Contains(ColorRT));
		if(ColorRT < 0 || ColorRT >= s_Textures.Count() || s_TexturesFreeID.Contains(ColorRT)) {
			return;
		}
		cRenderGL_TEXTURE &T = s_Textures[ColorRT];
		if(T.CubeMap) {
			cAssert(Faces != nullptr);
			if(nullptr == Faces) {
				return;
			}
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_CUBE_MAP_POSITIVE_X + Faces[i], T.glTexID, 0);
            HandleGLError();
		} else {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, T.glTexID, 0);
            HandleGLError();
		}
	}

	for(i = ColorRenderTargetCount; i < glExt_Supported::DrawBuffers.Count(); i++) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, 0, 0);
        HandleGLError();
	}

#if !defined COMMS_IOS && !defined COMMS_TIZEN
	glDrawBuffers(ColorRenderTargetCount, glExt_Supported::DrawBuffers.ToPtr());
    HandleGLError();
#endif // !COMMS_IOS && !COMMS_TIZEN

	GLuint glDepth = 0, glStencil = 0;
	if(DepthRenderTargetID >= 0 && DepthRenderTargetID < s_Textures.Count() && !s_TexturesFreeID.Contains(DepthRenderTargetID)) {
		const cRenderGL_TEXTURE &T = s_Textures[DepthRenderTargetID];
		glDepth = T.glDepthID;
		if(cFormat::HasStencil(T.Format)) {
			glStencil = T.glDepthID;
		}
	}
	
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, glDepth);
    HandleGLError();
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, glStencil);
    HandleGLError();
    cRenderGL_DrawingIntoFrameBuffer = false;
} // cRenderGL::SetRenderTargets
    
#ifdef COMMS_WINDOWS
#define glCompressedTexImage2D glCompressedTexImage2DARB
#define glCompressedTexImage3D glCompressedTexImage3DARB
#endif // Windows

//-----------------------------------------------------------------------------------------------------------
// cRenderGL_CreateTexture
//-----------------------------------------------------------------------------------------------------------
static bool cRenderGL_CreateTexture(const cImage &SrcImage, cRenderGL_TEXTURE *To, cImage *SrcImageMutable) {
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

	bool Uncompress = false;
	if(cFormat::IsCompressed(SrcPtr->GetFormat())) {
		if(cDimension::ThreeD == SrcPtr->GetDimension()) {
			Uncompress = true;
		} else if(cDimension::TwoD == SrcPtr->GetDimension()) {
			if((cFormat::Dxt1 == SrcPtr->GetFormat()) && !glExt_Supported::TextureCompressionDXT1) {
				Uncompress = true;
			}
			if((cFormat::Dxt3 == SrcPtr->GetFormat()) && !glExt_Supported::TextureCompressionDXT3) {
				Uncompress = true;
			}
			if((cFormat::Dxt5 == SrcPtr->GetFormat()) && !glExt_Supported::TextureCompressionDXT5) {
				Uncompress = true;
			}
		}
	}
	if(Uncompress) {
		if(SrcPtr != Buffer) {
			SrcPtr = Buffer;
			if(Buffer != SrcImageMutable) {
				Buffer->Copy(SrcImage);
			}
		}
		Buffer->Uncompress();
	}

	To->Width = SrcPtr->GetWidth();
	To->Height = SrcPtr->GetHeight();
	To->Depth = SrcPtr->GetDepth();
	To->MipMapCount = SrcPtr->GetMipMapCount();
	To->CubeMap = false; // This field is used only for color render target
	
	GLenum SrcFormat = cRenderGL_SRC_FORMAT[cFormat::ChannelCount(SrcPtr->GetFormat())];
	GLenum SrcType = cRenderGL_SRC_TYPE[SrcPtr->GetFormat()];
	GLenum InternalFormat = cRenderGL_INTERNAL_FORMAT[SrcPtr->GetFormat()];
	To->Format = SrcPtr->GetFormat();
	To->glTarget = cDimension::Cube == SrcPtr->GetDimension() ? GL_TEXTURE_CUBE_MAP : (cDimension::ThreeD == SrcPtr->GetDimension() ? GL_TEXTURE_3D : (cDimension::TwoD == SrcPtr->GetDimension() ? GL_TEXTURE_2D : GL_TEXTURE_1D));

    HandleGLError();
	glGenTextures(1, &To->glTexID);
    HandleGLError();
	glBindTexture(To->glTarget, To->glTexID);
    HandleGLError();

	To->BindedSamplerStateID = -1;
	
	const byte *Src;
	int MipMapLevel = 0, Size;
	dword i;
	while((Src = SrcPtr->GetPixels(MipMapLevel)) != nullptr) {
		if(cDimension::Cube == SrcPtr->GetDimension()) {
			Size = SrcPtr->GetMipMappedSize(MipMapLevel, 1) / 6;
			for(i = 0; i < 6; i++) {
				if(cFormat::IsCompressed(SrcPtr->GetFormat())) {
					glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, MipMapLevel, InternalFormat, SrcPtr->GetWidth(MipMapLevel), SrcPtr->GetHeight(MipMapLevel), 0, Size, Src + i * Size);
                    HandleGLError();
				} else {
					glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, MipMapLevel, InternalFormat, SrcPtr->GetWidth(MipMapLevel), SrcPtr->GetHeight(MipMapLevel), 0, SrcFormat, SrcType, Src + i * Size);
                    HandleGLError();
				}
			}
		} else if(cDimension::ThreeD == SrcPtr->GetDimension()) {
			if(cFormat::IsCompressed(SrcPtr->GetFormat())) {
				glCompressedTexImage3D(To->glTarget, MipMapLevel, InternalFormat, SrcPtr->GetWidth(MipMapLevel), SrcPtr->GetHeight(MipMapLevel), SrcPtr->GetDepth(MipMapLevel), 0, SrcPtr->GetMipMappedSize(MipMapLevel, 1), Src);
                HandleGLError();
			} else {
				glTexImage3D(To->glTarget, MipMapLevel, InternalFormat, SrcPtr->GetWidth(MipMapLevel), SrcPtr->GetHeight(MipMapLevel), SrcPtr->GetDepth(MipMapLevel), 0, SrcFormat, SrcType, Src);
                HandleGLError();
			}
		} else if(cDimension::TwoD == SrcPtr->GetDimension()) {
			if(cFormat::IsCompressed(SrcPtr->GetFormat())) {
				glCompressedTexImage2D(To->glTarget, MipMapLevel, InternalFormat, SrcPtr->GetWidth(MipMapLevel), SrcPtr->GetHeight(MipMapLevel), 0, SrcPtr->GetMipMappedSize(MipMapLevel, 1), Src);
                HandleGLError();
			} else {
				glTexImage2D(To->glTarget, MipMapLevel, InternalFormat, SrcPtr->GetWidth(MipMapLevel), SrcPtr->GetHeight(MipMapLevel), 0, SrcFormat, SrcType, Src);
                HandleGLError();
			}
		} else {
			glTexImage1D(To->glTarget, MipMapLevel, InternalFormat, SrcPtr->GetWidth(MipMapLevel), 0, SrcFormat, SrcType, Src);
            HandleGLError();
		}
		MipMapLevel++;
	}
	return true;
} // cRenderGL_CreateTexture

//-----------------------------------------------------------------------------
// cRenderGL::GetTextureID
//-----------------------------------------------------------------------------
int cRenderGL::GetTextureID(const char *FilePn) {
	cAssert(FilePn != nullptr);
	if(cStr::Length(FilePn) < 1) {
		return -1; // Invalid texture file pathname
	}
	cStr P = cIO::EnsureAbsolutePath(FilePn);
	// Searching loaded textures
	cStr uFilePn =FilePn;
	uFilePn.MakeUpper();
	auto it = s_TexturesMap.find(uFilePn.ToCharPtr());
	if(it != s_TexturesMap.end()) {
		if (it->second != -1) {
			return it->second;
		}
	}
	int t = AddTexture(FilePn);
	s_TexturesMap[uFilePn.ToCharPtr()] = t;
	s_TexturesMapID[t] = uFilePn.ToCharPtr();
	return t;
} // cRenderGL::GetTextureID

// cRenderGL::ReloadTexture
bool cRenderGL::ReloadTexture(const int TextureID) {
	cImage Src;
	cRenderGL_TEXTURE T;
	if(TextureID >= 0 && TextureID < s_Textures.Count() && !s_TexturesFreeID.Contains(TextureID)) {
		cRenderGL_TEXTURE &r = s_Textures[TextureID];
		if(!r.FilePn.IsEmpty()) { // This texture is not render target and it was not added as "cImage"
			if(cIO::LoadImage(r.FilePn, &Src)) {
				if(cRenderGL_CreateTexture(Src, &T, &Src)) {
					T.FilePn = r.FilePn; // Keep file pathname
					T.DesiredSamplerStateID = r.DesiredSamplerStateID; // Keep desired sampler state
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

// cRenderGL::ReloadTextures
void cRenderGL::ReloadTextures() {
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

// cRenderGL::ReloadTexture
void cRenderGL::ReloadTexture(const char *FilePn) {
	cStr F = cIO::EnsureAbsolutePath(FilePn);
	int i;
	for(i = 0; i < s_Textures.Count(); i++) {
		cRenderGL_TEXTURE &r = s_Textures[i];
		if(cStr::EqualsPath(r.FilePn, F)) {
			ReloadTexture(i);
		}
	}
}

// cRenderGL::UnloadTextures
void cRenderGL::UnloadTextures() {
	int i, c = 0;
	for(i = 0; i < s_Textures.Count(); i++) {
		cRenderGL_TEXTURE &r = s_Textures[i];
		if(r.FilePn.IsEmpty()) {
			continue;
		}
		r.Free();
		c++;
	}
	cLog::Message("Unloaded " + cStr::ToString(c) + " Textures.");
}

//-----------------------------------------------------------------------------
// cRenderGL::AddTexture : (const char *)
//-----------------------------------------------------------------------------
int cRenderGL::AddTexture(const char *FilePn) {
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
	int ID = AddTexture(Src);
	if(ID != -1) {
		s_Textures[ID].FilePn = P;
	}
	return ID;
} // cRenderGL::AddTexture : (const char *)

//-----------------------------------------------------------------------------
// cRenderGL::AddTexture
//-----------------------------------------------------------------------------
int cRenderGL::AddTexture(const cImage &SrcImage) {
	cRenderGL_TEXTURE T;
	if(!cRenderGL_CreateTexture(SrcImage, &T, nullptr)) {
		return -1;
	}
	
	int TextureID = -1;
	if(!s_TexturesFreeID.IsEmpty()) {
		TextureID = s_TexturesFreeID.GetAndRemoveLast();
		s_Textures[TextureID] = T;
		return TextureID;
	} else {
		return s_Textures.Add(T);
	}
} // cRenderGL::AddTexture

// cRenderGL::UpdateTexture
void cRenderGL::UpdateTexture(const int TextureID, const cList<cImage *> &SubImages, const cList<cRect> &Rects) {
	if(TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return; // Invalid texture ID
	}
	cRenderGL_TEXTURE &T = s_Textures[TextureID];
	if(T.glTarget != GL_TEXTURE_2D) {
		return; // Only 2D textures
	}
	if(!cFormat::IsPlain(T.Format)) {
		return; // Only plain formats
	}
	GLenum Format = cRenderGL_SRC_FORMAT[cFormat::ChannelCount(T.Format)];
	GLenum Type = cRenderGL_SRC_TYPE[T.Format];
    HandleGLError();
	glBindTexture(T.glTarget, T.glTexID);
    HandleGLError();
	GLint X, Y, W, H;
	const byte *Src = nullptr;
	int i;
	for(i = 0; i < Rects.Count(); i++) {
		const cImage *I = SubImages[i];
		if(T.Format != I->GetFormat()) {
			continue; // Only the same format
		}
		const cRect &rc = Rects[i];
		X = (GLint)rc.GetLeft();
		Y = (GLint)rc.GetBottom();
		W = (GLint)rc.GetWidth();
		H = (GLint)rc.GetHeight();
		Src = I->GetPixels();
		glTexSubImage2D(GL_TEXTURE_2D, 0, X, Y, W, H, Format, Type, Src);
        HandleGLError();
	}
}

void cRenderGL::UpdateTexture(const int TextureID, const cImage& Image) {
	if (TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return; // Invalid texture ID
	}
	cRenderGL_TEXTURE& T = s_Textures[TextureID];
	if (T.glTarget != GL_TEXTURE_2D) {
		return; // Only 2D textures
	}
	if (!cFormat::IsPlain(T.Format)) {
		return; // Only plain formats
	}
	GLenum Format = cRenderGL_SRC_FORMAT[cFormat::ChannelCount(T.Format)];
	GLenum Type = cRenderGL_SRC_TYPE[T.Format];
	HandleGLError();
	glBindTexture(T.glTarget, T.glTexID);
	HandleGLError();
	GLint W = (GLint)Image.GetWidth();
	GLint H = (GLint)Image.GetHeight();
	const byte* Src = Image.GetPixels();
	glTexImage2D(GL_TEXTURE_2D, 0, Format, W, H, 0, Format, Type, Src);
	HandleGLError();
}

//-----------------------------------------------------------------------------
// cRenderGL::FreeTexture
//-----------------------------------------------------------------------------
void cRenderGL::FreeTexture(const int TextureID) {
	if(s_TexturesFreeID.Contains(TextureID)) {
		return; // Already freed
	}
	if(TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return; // Invalid texture ID
	}
	cRenderGL_TEXTURE &T = s_Textures[TextureID];
	T.Free();
	T.Clear();
	s_TexturesFreeID.Add(TextureID);
	auto it = s_TexturesMapID.find(TextureID);
	if(it != s_TexturesMapID.end()) {
		s_TexturesMap.erase(it->second);
		s_TexturesMapID.erase(TextureID);
	}
} // cRenderGL::FreeTexture

// cRenderGL::GetTextureFilePn
const cStr * cRenderGL::GetTextureFilePn(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return nullptr; // Invalid texture ID
	}
	return &s_Textures[TextureID].FilePn;
}

// cRenderGL::GetTextureWidth
int cRenderGL::GetTextureWidth(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return -1; // Invalid texture ID
	}
    const cRenderGL_TEXTURE *T = &s_Textures[TextureID];
	return T->Width;
}

// cRenderGL::GetTextureHeight
int cRenderGL::GetTextureHeight(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return -1; // Invalid texture ID
	}
	return s_Textures[TextureID].Height;
}

// cRenderGL::GetTextureDepth
int cRenderGL::GetTextureDepth(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return -1; // Invalid texture ID
	}
	return s_Textures[TextureID].Depth;
}

// cRenderGL::GetTextureFormat
const cFormat::Enum cRenderGL::GetTextureFormat(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return cFormat::fmtNone; // Invalid texture ID
	}
	return s_Textures[TextureID].Format;
}

// cRenderGL::GetTextureMipMapCount
int cRenderGL::GetTextureMipMapCount(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return 1;
	}
	return s_Textures[TextureID].MipMapCount;
}

// cRenderGL::GetTextureRenderTargetUsage
bool cRenderGL::GetTextureRenderTargetUsage(const int TextureID) {
	if(TextureID < 0 || TextureID >= s_Textures.Count() || s_TexturesFreeID.Contains(TextureID)) {
		return false; // Invalid texture ID
	}
	return s_Textures[TextureID].RenderTarget;
}

// cRenderGL::SetViewport
void cRenderGL::SetViewport(const cRect &Viewport) {
    HandleGLError();
    int X = (int)Viewport.GetLeft();
    int Y = (int)Viewport.GetBottom();
    int W = (int)Viewport.GetWidth();
    int H = (int)Viewport.GetHeight();
    if(cRenderGL_DrawingIntoFrameBuffer) {
        X *= cRender::PixelsPerPoint;
        Y *= cRender::PixelsPerPoint;
        W *= cRender::PixelsPerPoint;
        H *= cRender::PixelsPerPoint;
    }
    glViewport(X, Y, W, H);
    HandleGLError();
}

// cRenderGL::SetClipRect
void cRenderGL::SetClipRect(const cRect &ClipRect) {
    HandleGLError();
	if(ClipRect.IsEmpty()) {
		glDisable(GL_SCISSOR_TEST);
        HandleGLError();
	} else {
		glEnable(GL_SCISSOR_TEST);
        HandleGLError();
        int X = (int)ClipRect.GetLeft();
        int Y = (int)ClipRect.GetBottom();
        int W = (int)ClipRect.GetWidth();
        int H = (int)ClipRect.GetHeight();
        if(cRenderGL_DrawingIntoFrameBuffer) {
            X *= cRender::PixelsPerPoint;
            Y *= cRender::PixelsPerPoint;
            W *= cRender::PixelsPerPoint;
            H *= cRender::PixelsPerPoint;
        }
        glScissor(X, Y, W, H);
        HandleGLError();
	}
}


// cRenderGL::GetVendor
const cStr& cRenderGL::GetVendor() const {
	return glExt_Supported::Vendor;
}

// cRenderGL::GetVersion
const cStr& cRenderGL::GetVersion() const {
	return glExt_Supported::Version;
}

// cRenderGL::GetRenderer
const cStr& cRenderGL::GetRenderer() const {
	return glExt_Supported::Renderer;
}

// cRenderGL::GetMRTCount
int cRenderGL::GetMRTCount() {
	return glExt_Supported::DrawBuffers.Count();
}

#ifdef COMMS_WINDOWS
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE GL_FRAMEBUFFER_COMPLETE_EXT
#endif
#define glCheckFramebufferStatus glCheckFramebufferStatusEXT
#endif // Windows

//-----------------------------------------------------------------------------
// cRenderGL::SupportsFormat
//-----------------------------------------------------------------------------
bool cRenderGL::SupportsFormat(const cFormat::Enum Format) {
    // iPad 2
    //  GL_OES_depth24              cFormat::Depth24    
    //  GL_OES_packed_depth_stencil cFormat::Depth24Stencil8
    if(cFormat::IsDepth(Format)) {
		return true;
	}
#if defined COMMS_IOS || defined COMMS_TIZEN
    return (Format >= cFormat::R8 && Format <= cFormat::Rgba8);
#else // Windows, Linux, OS X
	// It seems like the only correctly working way of checking support for specific texture format in OpenGL
	// is binding framebuffer and verifying status of "glCheckFramebufferStatus" function.
	
	static int SamplerState_Nearest = cRender::GetSamplerStateID(cFilter::Nearest, cAddressMode::Clamp);
	GLuint glTexID;
	bool r = false;
	
    HandleGLError();
	glGenTextures(1, &glTexID);
    HandleGLError();
	glBindTexture(GL_TEXTURE_2D, glTexID);
    HandleGLError();
	cRenderGL_SetupSampler(GL_TEXTURE_2D, s_SamplerStates[SamplerState_Nearest]);
	
	GLenum InternalFormat = cRenderGL_INTERNAL_FORMAT[Format];
	GLenum SrcFormat = cRenderGL_SRC_FORMAT[cFormat::ChannelCount(Format)];
	GLenum SrcType = cRenderGL_SRC_TYPE[Format];
	if(cFormat::IsFloat(Format)) {
		SrcType = GL_FLOAT;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, 32,  32, 0, SrcFormat, SrcType, nullptr);
	GLenum Err = glGetError(); // No "HandleGLError()" here because it throws "GL_INVALID_VALUE" in case of unsupported format
	cAssert((GL_NO_ERROR == Err) || (GL_INVALID_VALUE == Err));
	if(GL_NO_ERROR == Err) {
		if(0 == glExt_Supported::FBO) {
			glGenFramebuffers(1, &glExt_Supported::FBO);
			HandleGLError();
		}
		glBindFramebuffer(GL_FRAMEBUFFER, glExt_Supported::FBO);
		HandleGLError();
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 0, GL_TEXTURE_2D, glTexID, 0);
		HandleGLError();
		int i;
		for(i = 1; i < glExt_Supported::DrawBuffers.Count(); i++) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, 0, 0);
			HandleGLError();
		}
		glDrawBuffers(1, glExt_Supported::DrawBuffers.ToPtr());
		HandleGLError();

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
		HandleGLError();
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
		HandleGLError();

		r = (GL_FRAMEBUFFER_COMPLETE == glCheckFramebufferStatus(GL_FRAMEBUFFER));
		HandleGLError();

		for(i = 0; i < glExt_Supported::DrawBuffers.Count(); i++) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, 0, 0);
			HandleGLError();
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		HandleGLError();
	}
	glBindTexture(GL_TEXTURE_2D, 0);
    HandleGLError();
	glDeleteTextures(1, &glTexID);
    HandleGLError();
	return r;
#endif // COMMS_IOS || COMMS_TIZEN
} // cRenderGL::SupportsFormat

// cRenderGL::SupportsNonPowerOfTwo
bool cRenderGL::SupportsNonPowerOfTwo() {
	return true; // "GL_ARB_texture_non_power_of_two" is a core feature since OpenGL 2.0
}

//-----------------------------------------------------------------------------
// cRenderGL::ScreenShot
//-----------------------------------------------------------------------------
bool cRenderGL::ScreenShot(cImage *To) {
	int W = cMain_GetClientWidth();
	int H = cMain_GetClientHeight();
	if(W < 1 || H < 1) {
		return false;
	}
    W *= cRender::PixelsPerPoint;
    H *= cRender::PixelsPerPoint;
	To->Create(cFormat::Rgb8, W, H, 1, 1);
    HandleGLError();
	glReadPixels(0, 0, W, H, GL_RGB, GL_UNSIGNED_BYTE, To->GetPixels());
    HandleGLError();
	return true;
} // cRenderGL::ScreenShot

//--------------------------------------------------------------------------------------------------
// cRenderGL::SaveRenderTargetRaw
//--------------------------------------------------------------------------------------------------
bool cRenderGL::SaveRenderTargetRaw(const int RenderTargetID, cRender::SaveRenderTargetArgs *Args) {
    cAssert(Args != nullptr);
    if(RenderTargetID < 0 || RenderTargetID >= s_Textures.Count() || s_TexturesFreeID.Contains(RenderTargetID)) {
        return false;
    }
    cRenderGL_TEXTURE &T = s_Textures[RenderTargetID];
    if(!T.RenderTarget || GL_RENDERBUFFER == T.glTarget || 0 == T.glTexID) {
        return false;
    }
    Args->Format = T.Format;
	const bool UseRect = Args->Width > 0 && Args->Height > 0;
	if (UseRect) {
		cAssert(Args->Width <= T.Width);
		cAssert(Args->Height <= T.Height);
	} else {
		Args->Width = T.Width;
		Args->Height = T.Height;
	}
    Args->RequiredSize = cFormat::BytesPerPixel(Args->Format) * Args->Width * Args->Height;
    if(Args->BufferSize < Args->RequiredSize) {
        return false;
    }
    HandleGLError();
    if(0 == glExt_Supported::FBO) {
        glGenFramebuffers(1, &glExt_Supported::FBO);
        HandleGLError();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, glExt_Supported::FBO);
    HandleGLError();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 0, GL_TEXTURE_2D, T.glTexID, 0);
    HandleGLError();
    int i;
    for(i = 1; i < glExt_Supported::DrawBuffers.Count(); i++) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, 0, 0);
        HandleGLError();
    }
    glDrawBuffers(1, glExt_Supported::DrawBuffers.ToPtr());
    HandleGLError();
    
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    HandleGLError();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
    HandleGLError();
    
    const bool r = (GL_FRAMEBUFFER_COMPLETE == glCheckFramebufferStatus(GL_FRAMEBUFFER));
    HandleGLError();

    if(r) {
        GLenum SrcFormat = cRenderGL_SRC_FORMAT[cFormat::ChannelCount(T.Format)];
        GLenum SrcType = cRenderGL_SRC_TYPE[T.Format];
        glReadPixels(Args->X, Args->Y, Args->Width, Args->Height, SrcFormat, SrcType, Args->Buffer);
        HandleGLError();
    }
    
    for(i = 0; i < glExt_Supported::DrawBuffers.Count(); i++) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, 0, 0);
        HandleGLError();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    HandleGLError();
    return r;
} // cRenderGL::SaveRenderTargetRaw

//--------------------------------------------------------------------------------------------------
// cRenderGL::RequestSaveRenderTargetAsyncRaw
//--------------------------------------------------------------------------------------------------
bool cRenderGL::RequestSaveRenderTargetAsyncRaw(const int RenderTargetID, cRender::SaveRenderTargetArgs *Args) {
    cAssert(Args != nullptr);
    if(RenderTargetID < 0 || RenderTargetID >= s_Textures.Count() || s_TexturesFreeID.Contains(RenderTargetID)) {
        return false;
    }
    cRenderGL_TEXTURE &T = s_Textures[RenderTargetID];
    if(!T.RenderTarget || GL_RENDERBUFFER == T.glTarget || 0 == T.glTexID) {
        return false;
    }
    
    Args->Format = T.Format;
	const bool UseRect = Args->Width > 0 && Args->Height > 0;
	if (!UseRect) {
		Args->Width = T.Width;
		Args->Height = T.Height;
	} else {
		cAssert(Args->Width <= T.Width);
		cAssert(Args->Height <= T.Height);
	}
    Args->RequiredSize = cFormat::BytesPerPixel(Args->Format) * Args->Width * Args->Height;
    
    if (glGenBuffers == nullptr || glMapBuffer == nullptr) {
        return false;
    }
    
    HandleGLError();
    
    if (T.pbo[0] == 0) {
        glGenBuffers(2, T.pbo);
        HandleGLError();
        for(int i = 0; i < 2; ++i) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, T.pbo[i]);
            glBufferData(GL_PIXEL_PACK_BUFFER, Args->RequiredSize, nullptr, GL_STREAM_READ);
            T.pbo_has_data[i] = false;
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        T.current_pbo_index = 0;
        HandleGLError();
    }
    
    T.current_pbo_index = 1 - T.current_pbo_index;
    
    if(0 == glExt_Supported::FBO) {
        glGenFramebuffers(1, &glExt_Supported::FBO);
        HandleGLError();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, glExt_Supported::FBO);
    HandleGLError();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 0, GL_TEXTURE_2D, T.glTexID, 0);
    HandleGLError();
    int i;
    for(i = 1; i < glExt_Supported::DrawBuffers.Count(); i++) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, 0, 0);
        HandleGLError();
    }
    glDrawBuffers(1, glExt_Supported::DrawBuffers.ToPtr());
    HandleGLError();
    
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    HandleGLError();
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
    HandleGLError();
    
    const bool r = (GL_FRAMEBUFFER_COMPLETE == glCheckFramebufferStatus(GL_FRAMEBUFFER));
    HandleGLError();

    if(r) {
        GLenum SrcFormat = cRenderGL_SRC_FORMAT[cFormat::ChannelCount(T.Format)];
        GLenum SrcType = cRenderGL_SRC_TYPE[T.Format];
        glBindBuffer(GL_PIXEL_PACK_BUFFER, T.pbo[T.current_pbo_index]);
        glReadPixels(Args->X, Args->Y, Args->Width, Args->Height, SrcFormat, SrcType, 0);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        HandleGLError();
        T.pbo_has_data[T.current_pbo_index] = true;

        if (glFenceSync && glDeleteSync) {
            if (T.pbo_sync[T.current_pbo_index]) {
                glDeleteSync(T.pbo_sync[T.current_pbo_index]);
            }
            T.pbo_sync[T.current_pbo_index] = glFenceSync(0x9117 /* GL_SYNC_GPU_COMMANDS_COMPLETE */, 0);
            HandleGLError();
        }
    }
    
    for(i = 0; i < glExt_Supported::DrawBuffers.Count(); i++) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, 0, 0);
        HandleGLError();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    HandleGLError();
    return true;
} // cRenderGL::RequestSaveRenderTargetAsyncRaw

//--------------------------------------------------------------------------------------------------
// cRenderGL::GetSaveRenderTargetAsyncRaw
//--------------------------------------------------------------------------------------------------
bool cRenderGL::GetSaveRenderTargetAsyncRaw(const int RenderTargetID, cRender::SaveRenderTargetArgs *Args) {
    cAssert(Args != nullptr);
    if(RenderTargetID < 0 || RenderTargetID >= s_Textures.Count() || s_TexturesFreeID.Contains(RenderTargetID)) {
        return false;
    }
    cRenderGL_TEXTURE &T = s_Textures[RenderTargetID];
    if(!T.RenderTarget || GL_RENDERBUFFER == T.glTarget || 0 == T.glTexID) {
        return false;
    }
    Args->Format = T.Format;
	const bool UseRect = Args->Width > 0 && Args->Height > 0;
	if (UseRect) {
		cAssert(Args->Width <= T.Width);
		cAssert(Args->Height <= T.Height);
	} else {
		Args->Width = T.Width;
		Args->Height = T.Height;
	}
    Args->RequiredSize = cFormat::BytesPerPixel(Args->Format) * Args->Width * Args->Height;
    if(Args->BufferSize < Args->RequiredSize) {
        return false;
    }
    
    if (glGenBuffers == nullptr || glMapBuffer == nullptr) {
        // Fallback to synchronous if PBO is not supported by driver
        return SaveRenderTargetRaw(RenderTargetID, Args);
    }
    
    HandleGLError();
    
    int index_to_read = 1 - T.current_pbo_index;
    // Fallback if this FBO was only requested once (e.g. dynamically generated FBOs)
    if (!T.pbo_has_data[index_to_read] && T.pbo_has_data[T.current_pbo_index]) {
        index_to_read = T.current_pbo_index;
    }

    if (T.pbo_has_data[index_to_read] && Args->Buffer != nullptr) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, T.pbo[index_to_read]);
        void* ptr = glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
        if (ptr) {
            memcpy(Args->Buffer, ptr, Args->RequiredSize);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        HandleGLError();
        T.pbo_has_data[index_to_read] = false; // Mark as read
        return true;
    }
    
    return false;
} // cRenderGL::GetSaveRenderTargetAsyncRaw

//--------------------------------------------------------------------------------------------------
// cRenderGL::IsSaveRenderTargetAsyncReadyRaw
//--------------------------------------------------------------------------------------------------
bool cRenderGL::IsSaveRenderTargetAsyncReadyRaw(const int RenderTargetID) {
    if(RenderTargetID < 0 || RenderTargetID >= s_Textures.Count() || s_TexturesFreeID.Contains(RenderTargetID)) {
        return false; // texture doesn't exist
    }
    cRenderGL_TEXTURE &T = s_Textures[RenderTargetID];
    
    // Check if the hardware explicitly supports checking via FenceSync
    if (glClientWaitSync) {
        int index_to_read = 1 - T.current_pbo_index;
        // Fallback for one-shot dynamic FBO lifecycle requests (just like Get())
        if (!T.pbo_has_data[index_to_read] && T.pbo_has_data[T.current_pbo_index]) {
            index_to_read = T.current_pbo_index;
        }

        if (T.pbo_sync[index_to_read] != nullptr) {
            GLenum res = glClientWaitSync(T.pbo_sync[index_to_read], 0, 0);
            // 0x911A = GL_ALREADY_SIGNALED, 0x911C = GL_CONDITION_SATISFIED
            if (res == 0x911A || res == 0x911C) {
                return true;
            }
            return false;
        }
    }
    // Base assumption: if no sync API, we return false and rely on Python multi-frame queue.
    // However, if we do return true here, it forces the user into blocking. But the user ASKED not to block.
    // Let's assume it's true only if not doing PBO (sync mode):
    return true; 
} // cRenderGL::IsSaveRenderTargetAsyncReadyRaw

//--------------------------------------------------------------------------------------------------
// cRenderGL::FreeSaveRenderTargetAsyncRaw
//--------------------------------------------------------------------------------------------------
void cRenderGL::FreeSaveRenderTargetAsyncRaw(const int RenderTargetID) {
    if(RenderTargetID < 0 || RenderTargetID >= s_Textures.Count() || s_TexturesFreeID.Contains(RenderTargetID)) {
        return;
    }
    cRenderGL_TEXTURE &T = s_Textures[RenderTargetID];
    if(T.pbo[0] != 0) {
        glDeleteBuffers(2, T.pbo);
        HandleGLError();
        T.pbo[0] = 0;
        T.pbo[1] = 0;
        T.pbo_has_data[0] = false;
        T.pbo_has_data[1] = false;
        T.current_pbo_index = 0;
    }
} // cRenderGL::FreeSaveRenderTargetAsyncRaw

//------------------------------------------------------------------------------------------------------------
// cRenderGL::GetDepthStateID
//------------------------------------------------------------------------------------------------------------
int cRenderGL::GetDepthStateID(const bool TestEnabled, const bool WriteEnabled, const cDepthFunc::Enum Func) {
	int i;
	for(i = 0; i < s_DepthStates.Count(); i++) {
		const cRenderGL_DEPTH_STATE &r = s_DepthStates[i];
		if(r.TestEnabled == TestEnabled && r.WriteEnabled == WriteEnabled && r.Func == Func) {
			return i;
		}
	}
	
	cRenderGL_DEPTH_STATE DS;
	DS.TestEnabled = TestEnabled;
	DS.WriteEnabled = WriteEnabled;
	DS.Func = Func;
	return s_DepthStates.Add(DS);
} // cRenderGL::GetDepthStateID

//*****************************************************************************
// cRenderGL_DEPTH_FUNC
//*****************************************************************************
static const GLenum cRenderGL_DEPTH_FUNC[] = {
	GL_NEVER,		// cDepthFunc::Never
	GL_LESS,		// cDepthFunc::Less
	GL_EQUAL,		// cDepthFunc::Equal
	GL_LEQUAL,		// cDepthFunc::LessEqual
	GL_GREATER,		// cDepthFunc::Greater
	GL_NOTEQUAL,	// cDepthFunc::NotEqual
	GL_GEQUAL,		// cDepthFunc::GreaterEqual
	GL_ALWAYS		// cDepthFunc::Always
}; // cRenderGL_DEPTH_FUNC

//-----------------------------------------------------------------------------
// cRenderGL::SetDepthState
//-----------------------------------------------------------------------------
void cRenderGL::SetDepthState(const int DepthStateID) {
	cAssert(DepthStateID >= -1 && DepthStateID < s_DepthStates.Count());
	if(DepthStateID < -1 || DepthStateID >= s_DepthStates.Count()) {
		return;
	}
    HandleGLError();
	if(-1 == DepthStateID) {
		glDisable(GL_DEPTH_TEST);
        HandleGLError();
		glDepthMask(GL_FALSE);
        HandleGLError();
		glDepthFunc(GL_NEVER);
        HandleGLError();
		return;
	}

	const cRenderGL_DEPTH_STATE &DS = s_DepthStates[DepthStateID];
	if(DS.TestEnabled) {
		glEnable(GL_DEPTH_TEST);
        HandleGLError();
	} else {
		glDisable(GL_DEPTH_TEST);
        HandleGLError();
	}

	glDepthMask(DS.WriteEnabled ? GL_TRUE : GL_FALSE);
    HandleGLError();
	glDepthFunc(cRenderGL_DEPTH_FUNC[DS.Func]);
    HandleGLError();
} // cRenderGL::SetDepthState

//-----------------------------------------------------------------------------
// cRenderGL::SetCullMode
//-----------------------------------------------------------------------------
void cRenderGL::SetCullMode(const cCullMode::Enum CullMode) {
    HandleGLError();
	if(CullMode == cCullMode::None) {
		glDisable(GL_CULL_FACE);
        HandleGLError();
	} else if(CullMode == cCullMode::Clockwise) {
		glEnable(GL_CULL_FACE);
        HandleGLError();
		glCullFace(GL_BACK);
        HandleGLError();
	} else if(CullMode == cCullMode::CounterClockwise) {
		glEnable(GL_CULL_FACE);
        HandleGLError();
		glCullFace(GL_FRONT);
        HandleGLError();
	}
} // cRenderGL::SetCullMode

#if defined COMMS_IOS || defined COMMS_TIZEN
#define glClearDepth glClearDepthf
#endif // COMMS_IOS || COMMS_TIZEN

//-----------------------------------------------------------------------------------------------------------
// cRenderGL::Clear
//-----------------------------------------------------------------------------------------------------------
void cRenderGL::Clear(const bool ClearColor, const bool ClearDepth, const cColor &Color, const float Depth) {
    HandleGLError();
	GLbitfield ClearBits = 0;
	GLboolean CurColorMask[4], CurDepthMask(0);
	
	if(ClearColor) {
		ClearBits |= GL_COLOR_BUFFER_BIT;
		glClearColor(Color.r, Color.g, Color.b, Color.a);
        HandleGLError();
		glGetBooleanv(GL_COLOR_WRITEMASK, CurColorMask);
        HandleGLError();
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        HandleGLError();
	}

	if(ClearDepth) {
		ClearBits |= GL_DEPTH_BUFFER_BIT;
		glClearDepth(Depth);
        HandleGLError();
		glGetBooleanv(GL_DEPTH_WRITEMASK, &CurDepthMask);
        HandleGLError();
		glDepthMask(GL_TRUE);
        HandleGLError();
	}
	
	if(ClearBits != 0) {
		glClear(ClearBits);
        HandleGLError();
	}

	if(ClearColor) {
		glColorMask(CurColorMask[0], CurColorMask[1], CurColorMask[2], CurColorMask[3]);
        HandleGLError();
	}
	if(ClearDepth) {
		glDepthMask(CurDepthMask);
        HandleGLError();
	}
} // cRenderGL::Clear

// cRenderGL::BeginFrame
void cRenderGL::BeginFrame() {
    HandleGLError();
	if(s_GammaCorrection != cSettings::GetInstance()->GammaCorrection) {
		s_GammaCorrection = cSettings::GetInstance()->GammaCorrection;
		if(s_GammaCorrection) {
			glEnable(GL_FRAMEBUFFER_SRGB);
		} else {
			glDisable(GL_FRAMEBUFFER_SRGB);
		}
		HandleGLError();
	}
}

// cRenderGL::EndFrame
void cRenderGL::EndFrame() {
    HandleGLError();

#ifdef COMMS_WINDOWS
	SwapBuffers(wglGetCurrentDC());
    HandleGLError();
	if(s_VSync != cSettings::GetInstance()->VSync) {
		s_VSync = cSettings::GetInstance()->VSync;
		if(wglSwapIntervalEXT != nullptr) {
			wglSwapIntervalEXT(s_VSync);
			HandleGLError();
		}
	}
#endif // Windows
#ifdef COMMS_LINUX
    glXSwapBuffers(cLinuxMain_GetDisplay(), cLinuxMain_GetDrawable());
    HandleGLError();
    glFinish();
    HandleGLError();
#endif // COMMS_LINUX
}

// cRenderGL::Finish
void cRenderGL::Finish() {
    HandleGLError();
	glFinish();
    HandleGLError();
}

#ifdef COMMS_WINDOWS
//-----------------------------------------------------------------------------
// InitProc
//-----------------------------------------------------------------------------
static void InitProc() {
	static bool Inited = false;
	if(Inited) {
		return;
	}

	// GL_VERSION_1_2
	GET_PROC_ADDRESS(PFNGLBLENDEQUATIONPROC, glBlendEquation);

	// GL_VERSION_2_0
	GET_PROC_ADDRESS(PFNGLDRAWBUFFERSPROC, glDrawBuffers);

	// WGL_ARB_extensions_string
	GET_PROC_ADDRESS(PFNWGLGETEXTENSIONSSTRINGARBPROC, wglGetExtensionsStringARB);

	// WGL_EXT_swap_control
	GET_PROC_ADDRESS(PFNWGLSWAPINTERVALPROC, wglSwapIntervalEXT);
	
	// WGL_ARB_pixel_format
	GET_PROC_ADDRESS(PFNWGLCHOOSEPIXELFORMATARBPROC, wglChoosePixelFormatARB);
	GET_PROC_ADDRESS(PFNWGLGETPIXELFORMATATTRIBIVARBPROC, wglGetPixelFormatAttribivARB);

	// GL_ARB_multitexture
	GET_PROC_ADDRESS(PFNGLACTIVETEXTUREARBPROC, glActiveTextureARB);
	
	// GL_EXT_texture3D
	GET_PROC_ADDRESS(PFNGLTEXIMAGE3DPROC, glTexImage3D);
	
	// GL_ARB_texture_compression
	GET_PROC_ADDRESS(PFNGLCOMPRESSEDTEXIMAGE2DARBPROC, glCompressedTexImage2DARB);
	GET_PROC_ADDRESS(PFNGLCOMPRESSEDTEXIMAGE3DARBPROC, glCompressedTexImage3DARB);
	
	//-----------------------------------------------------------------------------
	// VBO
	//-----------------------------------------------------------------------------

	// GL_ARB_vertex_buffer_object
	GET_PROC_ADDRESS(PFNGLGENBUFFERSARBPROC, glGenBuffersARB);
	GET_PROC_ADDRESS(PFNGLBINDBUFFERARBPROC, glBindBufferARB);
	GET_PROC_ADDRESS(PFNGLBUFFERDATAARBPROC, glBufferDataARB);
	GET_PROC_ADDRESS(PFNGLDELETEBUFFERSARBPROC, glDeleteBuffersARB);
	GET_PROC_ADDRESS(PFNGLMAPBUFFERARBPROC, glMapBufferARB);
	GET_PROC_ADDRESS(PFNGLUNMAPBUFFERARBPROC, glUnmapBufferARB);

	GET_PROC_ADDRESS(PFNGLFENCESYNCPROC, glFenceSync);
	GET_PROC_ADDRESS(PFNGLCLIENTWAITSYNCPROC, glClientWaitSync);
	GET_PROC_ADDRESS(PFNGLDELETESYNCPROC, glDeleteSync);

	// GL_ARB_vertex_program
	GET_PROC_ADDRESS(PFNGLENABLEVERTEXATTRIBARRAYARBPROC, glEnableVertexAttribArrayARB);
	GET_PROC_ADDRESS(PFNGLDISABLEVERTEXATTRIBARRAYARBPROC, glDisableVertexAttribArrayARB);
	GET_PROC_ADDRESS(PFNGLVERTEXATTRIBPOINTERARBPROC, glVertexAttribPointerARB);
    
    //-----------------------------------------------------------------------------
    // VAO
    //-----------------------------------------------------------------------------
    
    // GL_ARB_vertex_array_object
    GET_PROC_ADDRESS(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays);
    GET_PROC_ADDRESS(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray);
    GET_PROC_ADDRESS(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays);
	
	//-----------------------------------------------------------------------------
	// GLSL
	//-----------------------------------------------------------------------------

	// GL_ARB_shading_language_100

	// GL_ARB_shader_objects
	GET_PROC_ADDRESS(PFNGLCREATEPROGRAMOBJECTARBPROC, glCreateProgramObjectARB);
	GET_PROC_ADDRESS(PFNGLCREATESHADEROBJECTARBPROC, glCreateShaderObjectARB);
	GET_PROC_ADDRESS(PFNGLDELETEOBJECTARBPROC, glDeleteObjectARB);
	GET_PROC_ADDRESS(PFNGLATTACHOBJECTARBPROC, glAttachObjectARB);
	GET_PROC_ADDRESS(PFNGLSHADERSOURCEARBPROC, glShaderSourceARB);
	GET_PROC_ADDRESS(PFNGLCOMPILESHADERARBPROC, glCompileShaderARB);
	GET_PROC_ADDRESS(PFNGLLINKPROGRAMARBPROC, glLinkProgramARB);
	GET_PROC_ADDRESS(PFNGLUSEPROGRAMOBJECTARBPROC, glUseProgramObjectARB);
	GET_PROC_ADDRESS(PFNGLGETOBJECTPARAMETERIVARBPROC, glGetObjectParameterivARB);
	GET_PROC_ADDRESS(PFNGLGETINFOLOGARBPROC, glGetInfoLogARB);
	GET_PROC_ADDRESS(PFNGLGETUNIFORMLOCATIONARBPROC, glGetUniformLocationARB);
	GET_PROC_ADDRESS(PFNGLUNIFORM1IARBPROC, glUniform1iARB);
	GET_PROC_ADDRESS(PFNGLUNIFORM1FARBPROC, glUniform1fARB);
	GET_PROC_ADDRESS(PFNGLUNIFORM1FVARBPROC, glUniform1fvARB);
	GET_PROC_ADDRESS(PFNGLUNIFORM2FVARBPROC, glUniform2fvARB);
	GET_PROC_ADDRESS(PFNGLUNIFORM3FVARBPROC, glUniform3fvARB);
	GET_PROC_ADDRESS(PFNGLUNIFORM4FVARBPROC, glUniform4fvARB);
	GET_PROC_ADDRESS(PFNGLUNIFORMMATRIX3FVARBPROC, glUniformMatrix3fvARB);
	GET_PROC_ADDRESS(PFNGLUNIFORMMATRIX4FVARBPROC, glUniformMatrix4fvARB);
	GET_PROC_ADDRESS(PFNGLGETUNIFORMFVARBPROC, glGetUniformfvARB);
	GET_PROC_ADDRESS(PFNGLGETACTIVEUNIFORMARBPROC, glGetActiveUniformARB);

	// GL_ARB_vertex_shader
	GET_PROC_ADDRESS(PFNGLBINDATTRIBLOCATIONARBPROC, glBindAttribLocationARB);

	// GL_ARB_fragment_shader

	// ARB_shading_language_include
	GET_PROC_ADDRESS(PFNGLNAMEDSTRINGARBPROC, glNamedStringARB);

	//-----------------------------------------------------------------------------
	// FBO
	//-----------------------------------------------------------------------------

	// GL_EXT_framebuffer_object
	GET_PROC_ADDRESS(PFNGLGENFRAMEBUFFERSEXTPROC, glGenFramebuffersEXT);
	GET_PROC_ADDRESS(PFNGLBINDFRAMEBUFFEREXTPROC, glBindFramebufferEXT);
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERTEXTURE2DEXTPROC, glFramebufferTexture2DEXT);
	GET_PROC_ADDRESS(PFNGLDELETEFRAMEBUFFERSEXTPROC, glDeleteFramebuffersEXT);
	GET_PROC_ADDRESS(PFNGLGENRENDERBUFFERSEXTPROC, glGenRenderbuffersEXT);
	GET_PROC_ADDRESS(PFNGLBINDRENDERBUFFEREXTPROC, glBindRenderbufferEXT);
	GET_PROC_ADDRESS(PFNGLRENDERBUFFERSTORAGEEXTPROC, glRenderbufferStorageEXT);
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERRENDERBUFFEREXTPROC, glFramebufferRenderbufferEXT);
	GET_PROC_ADDRESS(PFNGLDELETERENDERBUFFERSEXTPROC, glDeleteRenderbuffersEXT);
	GET_PROC_ADDRESS(PFNGLCHECKFRAMEBUFFERSTATUSEXTPROC, glCheckFramebufferStatusEXT);
	GET_PROC_ADDRESS(PFNGLPATCHPARAMETERIPROC, glPatchParameteri);
	GET_PROC_ADDRESS(PFNGLPATCHPARAMETERFVPROC, glPatchParameterfv);

	Inited = true;
} // InitProc
#endif // Windows

//-----------------------------------------------------------------------------
// glExt_IsSupported
//-----------------------------------------------------------------------------
static bool glExt_IsSupported(const char *Ext) {
    HandleGLError();
    if(glExt_List.IsEmpty()) {
#ifdef COMMS_WINDOWS // Before OpenGL 3.0
		// Under Linux "glGetString(GL_EXTENSIONS)" rises "GL_INVALID_ENUM"
        cStr Buffer((const char *)glGetString(GL_EXTENSIONS));
        HandleGLError();
        Buffer.Split(&glExt_List, " ");
#else // macOS, Linux
        // Argument GL_EXTENSIONS is deprecated since OpenGL 3.0
        GLint n, i;
        glGetIntegerv(GL_NUM_EXTENSIONS, &n);
        HandleGLError();
        for(i = 0; i < n; i++) {
            glExt_List.Add(cStr((const char *)glGetStringi(GL_EXTENSIONS, i)));
            HandleGLError();
        }
#endif
    }
    return glExt_List.Contains(Ext);
} // glExt_IsSupported

#ifdef COMMS_WINDOWS
// ExtensionIsInList
static bool ExtensionIsInList(const char *Ext, const char *ExtList) {
	const char *Start = ExtList;
	const char *Ptr;

	while((Ptr = strstr(Start, Ext)) != nullptr) {
		const char *End = Ptr + strlen(Ext);
		if(cStr::CharIsWhitespace(*End) || *End == '\0') {
			return true;
		}
		Start = End;
	}

	return false;
}

// wglExt_IsSupported
static bool wglExt_IsSupported(HDC Hdc, const char *Ext) {
	if(nullptr == wglGetExtensionsStringARB) {
		return false;
	}
	const char *List = wglGetExtensionsStringARB(Hdc);
	return ExtensionIsInList(Ext, List);
}
#endif // Windows

//-----------------------------------------------------------------------------
// glExt_AreSupported
//-----------------------------------------------------------------------------
static bool glExt_AreSupported(const char *ExtList) {
	cList<cStr> List;
	cStr::Split(ExtList, &List);
	for(int i = 0; i < List.Count(); i++) {
		if(!glExt_IsSupported(List[i])) {
			return false;
		}
	}
	return true;
} // glExt_AreSupported
    
#ifdef COMMS_WINDOWS
#ifndef GL_MAX_DRAW_BUFFERS
#define GL_MAX_DRAW_BUFFERS GL_MAX_DRAW_BUFFERS_ARB
#endif
#ifndef GL_MAX_RENDERBUFFER_SIZE
#define GL_MAX_RENDERBUFFER_SIZE GL_MAX_RENDERBUFFER_SIZE_EXT
#endif
#endif // Windows

//-----------------------------------------------------------------------------
// glExt_Init
//-----------------------------------------------------------------------------
#ifdef COMMS_WINDOWS
static bool glExt_Init(HDC Hdc) {
	InitProc();
#else // OS X, Linux, iOS, Tizen
static bool glExt_Init() {
#endif // Windows

	cStr Supported, Unsupported;
	cStr *AppendTo = nullptr;
    const char *Prefix = " OpenGL | ";

#ifdef COMMS_WINDOWS
	// Pixel format
	AppendTo = wglExt_IsSupported(Hdc, "WGL_ARB_pixel_format") ? &Supported : &Unsupported;
	*AppendTo << Prefix << "Pixel format" << cStr::EndLn;

	// Multisample
	AppendTo = glExt_IsSupported("GL_ARB_multisample") && wglExt_IsSupported(Hdc, "WGL_ARB_multisample") ? &Supported : &Unsupported;
	*AppendTo << Prefix << "Multisample" << cStr::EndLn;
#endif // Windows
	
	// Multitexture
    glExt_Supported::MaxTexUnits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &glExt_Supported::MaxTexUnits);
    HandleGLError();
    if(glExt_IsSupported("GL_ARB_multitexture") || (glExt_Supported::MaxTexUnits > 1)) {
        Supported << Prefix << "Multitexture (fragment shader units: " << (int)glExt_Supported::MaxTexUnits << ")" << cStr::EndLn;
    } else {
		Unsupported << Prefix << "Multitexture" << cStr::EndLn;
	}

	// Anisotropic filtering
    glExt_Supported::MaxAnisotropy = 0;
	if(glExt_IsSupported("GL_EXT_texture_filter_anisotropic")) {
		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glExt_Supported::MaxAnisotropy);
        HandleGLError();
		Supported << Prefix << "Anisotropic filtering (level: " << (int)glExt_Supported::MaxAnisotropy << ")" << cStr::EndLn;
	}
	// Texture Compression
	// Under CentOS 6 inside VMware extension "GL_ARB_texture_compression" is supported, but compressed DXT formats are not listed
	GLint N = 0;
	glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &N);
	cList<GLint> F(N, 0);
	glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, F.ToPtr());
	glExt_Supported::TextureCompressionDXT1 = F.Contains(cRenderGL_INTERNAL_FORMAT[cFormat::Dxt1]);
	glExt_Supported::TextureCompressionDXT3 = F.Contains(cRenderGL_INTERNAL_FORMAT[cFormat::Dxt3]);
	glExt_Supported::TextureCompressionDXT5 = F.Contains(cRenderGL_INTERNAL_FORMAT[cFormat::Dxt5]);
	Supported << Prefix << "Texture Compression: ";
	Supported << (glExt_Supported::TextureCompressionDXT1 ? "DXT1" : "-");
	Supported << (glExt_Supported::TextureCompressionDXT3 ? "/DXT3" : "/-");
	Supported << (glExt_Supported::TextureCompressionDXT5 ? "/DXT5" : "/-");
    glExt_Supported::TextureCompressionPVR = glExt_IsSupported("GL_IMG_texture_compression_pvrtc"); // PowerVR Texture Compression
    Supported << (glExt_Supported::TextureCompressionPVR ? "/PVR" : "/-");
    Supported << cStr::EndLn;

    // VAO
    bool VAO = glExt_IsSupported("GL_ARB_vertex_array_object");
#ifdef COMMS_MACOS
    VAO = true; // "GL_ARB_vertex_array_object" is provided by OpenGL 3.2 (Core) and OpenGL ES 2.0
#endif
	if(VAO) {
		AppendTo = &Supported;
	} else {
		AppendTo = &Unsupported;
	}
    *AppendTo << Prefix << "Vertex Array Object" << cStr::EndLn;

    // VBO
    bool VBO = glExt_IsSupported("GL_ARB_vertex_buffer_object");
#ifdef COMMS_MACOS
    VBO = true; // "ARB_vertex_buffer_object" is provided by OpenGL 3.2 (Core) and OpenGL ES 2.0
#endif
	if(VBO) {
		AppendTo = &Supported;
	} else {
		AppendTo = &Unsupported;
	}
    *AppendTo << Prefix << "Vertex Buffer Object" << cStr::EndLn;

    GLint MaxAttribs = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &MaxAttribs);
    HandleGLError();
    GLint MaxUniforms = 0;
#if defined COMMS_IOS || defined COMMS_TIZEN
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &MaxUniforms);
    HandleGLError();
#else // OS X, Linux, Windows
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &MaxUniforms);
    HandleGLError();
    MaxUniforms /= 4;
#endif // COMMS_IOS || COMMS_TIZEN
	if(glExt_IsSupported("GL_ARB_vertex_program") || ((MaxAttribs > 0) && (MaxUniforms > 0))) {
		Supported << Prefix << "Vertex Attributes (count: " << MaxAttribs << ")" << cStr::EndLn;
		Supported << Prefix << "Vertex Uniform Vectors (count: " << MaxUniforms << ")" << cStr::EndLn;
	} else {
		Unsupported << Prefix << "Vertex Attributes" << cStr::EndLn;
	}

	// GLSL
	// Do not test "GL_ARB_shading_language_100" because under Linux it exposes as "GL_ARB_shading_language_420pack"
    bool Shaders = glExt_AreSupported("GL_ARB_shader_objects, GL_ARB_vertex_shader, GL_ARB_fragment_shader");
#ifdef COMMS_MACOS
    Shaders = true; // "ARB_shader_objects", "ARB_vertex_shader", and "ARB_fragment_shader" are provided by OpenGL 3.2 (Core) and OpenGL ES 2.0
#endif
	if(Shaders) {
		AppendTo = &Supported;
	} else {
		AppendTo = &Unsupported;
	}
	*AppendTo << Prefix << "OpenGL Shading Language" << cStr::EndLn;

	// Vendor/Renderer/Version
	glExt_Supported::Vendor = (const char*)glGetString(GL_VENDOR);
	HandleGLError();
	glExt_Supported::Renderer = (const char*)glGetString(GL_RENDERER);
	HandleGLError();
	glExt_Supported::Version = (const char*)glGetString(GL_VERSION);
	HandleGLError();
    
    // #include
	glExt_Supported::Include = glExt_IsSupported("GL_ARB_shading_language_include");
#ifdef COMMS_WINDOWS
	const bool ATI = glExt_Supported::Vendor.Equals("ATI Technologies Inc.");
	if (ATI) { // User has provided a log from Windows, where his card "AMD Radeon RX 6600 XT" supports extension "GL_ARB_shading_language_include"
		glExt_Supported::Include = false; // but then it failed to compile shaders with error "Extension GL_ARB_shading_language_include is not supported on this card".
	}
#endif // Windows
    if(glExt_Supported::Include) {
        Supported << Prefix << "#include" << cStr::EndLn;
    }
    
	// FBO
    glExt_Supported::FrameBufferMaxSize = 0;
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &glExt_Supported::FrameBufferMaxSize);
    HandleGLError();
	if(glExt_IsSupported("GL_EXT_framebuffer_object") || (glExt_Supported::FrameBufferMaxSize > 0)) {
        Supported << Prefix << "Frame Buffer Object (max size: " << cStr::ToString(glExt_Supported::FrameBufferMaxSize) + " X " << cStr::ToString(glExt_Supported::FrameBufferMaxSize) << ")" << cStr::EndLn;
	} else {
        Unsupported << Prefix << "Frame Buffer Object" << cStr::EndLn;
    }

    // MRT
    glExt_Supported::DrawBuffers.SetCount(1, 0);
    GLint MaxRTs;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &MaxRTs);
    HandleGLError();
    if(glExt_IsSupported("GL_ARB_draw_buffers") || (MaxRTs > 1)) {
		glExt_Supported::DrawBuffers.SetCount(MaxRTs, 0);
		Supported << Prefix << "MRT (targets: " << (int)MaxRTs << ")" << cStr::EndLn;
	}

	// Bindless texture
	glExt_Supported::BindlessTexture = glExt_IsSupported("ARB_bindless_texture");
    
    // Tessellation shader
    glExt_Supported::TessellationShader = glExt_IsSupported("GL_ARB_tessellation_shader");
    if(glExt_Supported::TessellationShader) {
        Supported << Prefix << "Tessellation shader" << cStr::EndLn;
    }

	cStr Log;
#if defined COMMS_IOS || defined COMMS_TIZEN
    Log += cStr::EndLn;
#endif // COMMS_IOS || COMMS_TIZEN
	Log << cStr(60, '-') << cStr::EndLn;
    Log << Prefix << "Version: " << glExt_Supported::Version << cStr::EndLn;
    Log << Prefix << "Vendor: " << glExt_Supported::Vendor << cStr::EndLn;
    Log << Prefix << "Renderer: " << glExt_Supported::Renderer << cStr::EndLn;
	if(Supported.Length()) {
		Log << Prefix << "Supported features:" << cStr::EndLn << Supported;
	}
	if(Unsupported.Length()) {
		Log << Prefix << "Unsupported features:" << cStr::EndLn << Unsupported;
	}
    Log << Prefix << "Extensions (" << cStr::ToString(glExt_List.Count()) << ") :";
    int i;
    for(i = 0; i < glExt_List.Count(); i++) {
        Log << " " << glExt_List[i];
    }
    Log << cStr::EndLn;
	Log << cStr(60, '-') << cStr::EndLn;
	if(Unsupported.Length() == 0) {
		cLog::Message(Log);
		return true;
	} else {
		cLog::Error(Log);
		return false;
	}
} // glExt_Init

} // comms

#endif // COMMS_OPENGL
