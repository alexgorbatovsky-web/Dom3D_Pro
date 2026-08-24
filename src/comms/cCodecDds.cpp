#include "comms.h"

namespace comms {

#ifdef COMMS_BIG_ENDIAN
static const char *DdsHeader_Format = "32d";
#endif // COMMS_BIG_ENDIAN

#pragma pack(1)

struct DdsPixelFormat {
	dword	Size;
	dword	Flags;
	dword	FourCC;
	dword	RgbBitCount;
	dword	RBitMask;
	dword	GBitMask;
	dword	BBitMask;
	dword	RgbAlphaBitMask;
};

struct DdsCaps {
	dword	Caps1;
	dword	Caps2;
	dword	Reserved[2];
};

struct DdsHeader {
	dword	Magic;
	dword	Size;
	dword	Flags;
	dword	Height;
	dword	Width;
	dword	PitchOrLinearSize;
	dword	Depth;
	dword	MipMapCount;
	dword	Reserved1[11];
	DdsPixelFormat PixelFormat;
	DdsCaps	Caps;
	dword	Reserved2;
};

#pragma pack()

enum DdsFlags {
	// Surface header:
	DDS_CAPS		= 0x00000001,
	DDS_HEIGHT		= 0x00000002,
	DDS_WIDTH		= 0x00000004,
	DDS_PITCH		= 0x00000008,
	DDS_PIXELFORMAT = 0x00001000,
	DDS_MIPMAPCOUNT = 0x00020000,
	DDS_LINEARSIZE	= 0x00080000,
	DDS_DEPTH		= 0x00800000,

	// Pixel format:
	DDS_ALPHA_PIXELS	= 0x00000001,
	DDS_ALPHA			= 0x00000002,
	DDS_FOURCC			= 0x00000004,
	DDS_RGB				= 0x00000040,
	DDS_RGBA			= 0x00000041,

	// Complex caps:
	DDS_COMPLEX			= 0x00000008,
	DDS_TEXTURE			= 0x00001000,
	DDS_MIPMAP			= 0x00400000,

	// 3D texture:
	DDS_VOLUME				= 0x00200000,

	// Cubemaps:
	DDS_CUBEMAP				= 0x00000200,
	DDS_CUBEMAP_POSITIVEX	= 0x00000400,
	DDS_CUBEMAP_NEGATIVEX	= 0x00000800,
	DDS_CUBEMAP_POSITIVEY	= 0x00001000,
	DDS_CUBEMAP_NEGATIVEY	= 0x00002000,
	DDS_CUBEMAP_POSITIVEZ	= 0x00008000,
	DDS_CUBEMAP_NEGATIVEZ	= 0x00200000,
	DDS_CUBEMAP_ALL_FACES	= DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX | DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY | DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ
};

const dword	DDS_MAGIC = 'D' | 'D' << 8 | 'S' << 16 | ' ' << 24;
const dword FOURCC_DXT1 = 'D' | 'X' << 8 | 'T' << 16 | '1' << 24;
const dword FOURCC_DXT3 = 'D' | 'X' << 8 | 'T' << 16 | '3' << 24;
const dword FOURCC_DXT5 = 'D' | 'X' << 8 | 'T' << 16 | '5' << 24;

//-----------------------------------------------------------------------------
// cCodecDds::Decode
//-----------------------------------------------------------------------------
bool cCodecDds::Decode(const cFile &Src, cImage *To) {
	cAssert(To != nullptr);
	To->Free();

	DdsHeader Hdr;
	if(Src.ReadBytes(&Hdr, sizeof(Hdr)) != sizeof(Hdr)) {
		return false;
	}
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap(&Hdr, DdsHeader_Format);
#endif // COMMS_BIG_ENDIAN

	if(Hdr.Magic != DDS_MAGIC) {
		return false;
	}

	To->m_Width = Hdr.Width;
	To->m_Height = Hdr.Height;
	To->m_Depth = (Hdr.Caps.Caps2 & DDS_CUBEMAP) ? 0 : (0 == Hdr.Depth ? 1 : Hdr.Depth);
	To->m_MipMapCount = 0 == Hdr.MipMapCount ? 1 : Hdr.MipMapCount;
	To->m_Format = cFormat::fmtNone;

	switch(Hdr.PixelFormat.FourCC) {
		case 34:
			To->m_Format = cFormat::Rg16;
			break;
		case 36:
			To->m_Format = cFormat::Rgba16;
			break;
		case 111:
			To->m_Format = cFormat::R16f;
			break;
		case 112:
			To->m_Format = cFormat::Rg16f;
			break;
		case 113:
			To->m_Format = cFormat::Rgba16f;
			break;
		case 114:
			To->m_Format = cFormat::R32f;
			break;
		case 115:
			To->m_Format = cFormat::Rg32f;
			break;
		case 116:
			To->m_Format = cFormat::Rgba32f;
			break;
		case FOURCC_DXT1:
			To->m_Format = cFormat::Dxt1;
			break;
		case FOURCC_DXT3:
			To->m_Format = cFormat::Dxt3;
			break;
		case FOURCC_DXT5:
			To->m_Format = cFormat::Dxt5;
			break;
		default:
			switch(Hdr.PixelFormat.RgbBitCount) {
				case 8:
					To->m_Format = cFormat::R8;
					break;
				case 16:
					cAssert(!(61440 == Hdr.PixelFormat.RgbAlphaBitMask)); // Unsupported "Rgba4"
					cAssert(!((Hdr.PixelFormat.RgbAlphaBitMask != 61440) && (Hdr.PixelFormat.RgbAlphaBitMask != 65280) && (31 == Hdr.PixelFormat.BBitMask))); // Unsupported "Rgb565"
					To->m_Format = (65280 == Hdr.PixelFormat.RgbAlphaBitMask ? cFormat::Rg8 : cFormat::R16);
					break;
				case 24:
					To->m_Format = cFormat::Rgb8;
					break;
				case 32:
					To->m_Format = cFormat::Rgba8;
					break;
				default:
					return false;
			}
	}

	int Size = To->GetMipMappedSize(0, To->m_MipMapCount);
	To->m_Pixels = new byte[Size];
	int Face, MipMapLevel, FaceSize;
	byte *Ptr;
	if(cDimension::Cube == To->GetDimension()) {
		for(Face = 0; Face < 6; Face++) {
			for(MipMapLevel = 0; MipMapLevel < To->m_MipMapCount; MipMapLevel++) {
				FaceSize = To->GetMipMappedSize(MipMapLevel, 1) / 6;
				Ptr = To->GetPixels(MipMapLevel) + Face * FaceSize;
				if(Src.ReadBytes(Ptr, FaceSize) != FaceSize) {
					return false;
				}
			}
		}
	} else {
		if(Src.ReadBytes(To->m_Pixels, Size) != Size) {
			return false;
		}
	}

	int NChannels;
	if((cFormat::Rgb8 == To->m_Format || cFormat::Rgba8 == To->m_Format) && 0xff == Hdr.PixelFormat.BBitMask) {
		NChannels = cFormat::ChannelCount(To->m_Format);
		cMath::SwapChannels(To->m_Pixels, Size / NChannels, NChannels, 0, 2);
	}

	if(cDimension::TwoD == To->GetDimension()) {
		To->Flip();
	}

	if(cDimension::ThreeD == To->GetDimension()) {
		//To->ToFormat(cFormat::Rgba8);
	}
	
	return true;
} // cCodecDds::Decode

//-----------------------------------------------------------------------------
// cCodecDds::Encode
//-----------------------------------------------------------------------------
bool cCodecDds::Encode(const cImage &Image, cFile *To) {
	cAssert(To != nullptr);

	int NChannels = cFormat::ChannelCount(Image.GetFormat());
	if(cFormat::IsPlain(Image.GetFormat())) {
		if(3 == NChannels && Image.GetFormat() != cFormat::Rgb8) {
			return false;
		}
	} else if(!cFormat::IsCompressed(Image.GetFormat())) {
		return false;
	}
	
	dword FloatFourCC[11] = {
		34,		// Rg16
		0,		// Rgb16
		36,		// Rgba16

		111,	// R16f, I16f
		112,	// Rg16f, Ia16f
		0,		// Rgb16f
		113,	// Rgba16f

		114,	// R32f, I32f
		115,	// Rg32f, Ia32f
		0,		// Rgb32f
		116		// Rgba32f
	};

	dword CompressedFourCC[3] = {
		FOURCC_DXT1, FOURCC_DXT3, FOURCC_DXT5
	};
	
	DdsHeader Hdr;
	Hdr.Magic = DDS_MAGIC;
	Hdr.Size = sizeof(Hdr) - sizeof(Hdr.Magic);
	Hdr.Flags = DDS_CAPS | DDS_PIXELFORMAT | DDS_WIDTH | DDS_HEIGHT | (Image.GetMipMapCount() > 1 ? DDS_MIPMAPCOUNT : 0) | (Image.GetDepth() > 1 ? DDS_DEPTH : 0);
	Hdr.Height = Image.GetHeight();
	Hdr.Width = Image.GetWidth();
	Hdr.PitchOrLinearSize = 0;
	Hdr.Depth = Image.GetDepth() > 1 ? Image.GetDepth() : 0;
	Hdr.MipMapCount = Image.GetMipMapCount() > 1 ? Image.GetMipMapCount() : 0;
	memset(Hdr.Reserved1, 0, sizeof(Hdr.Reserved1));
	Hdr.PixelFormat.Size = sizeof(Hdr.PixelFormat);
	if(Image.GetFormat() <= cFormat::R16) {
		Hdr.PixelFormat.Flags = (NChannels < 3 ? 0x00020000 : DDS_RGB) | ((NChannels & 1) ? 0 : DDS_ALPHA_PIXELS);
		Hdr.PixelFormat.FourCC = 0;
		Hdr.PixelFormat.RgbBitCount = 8 * cFormat::BytesPerChannel(Image.GetFormat()) * NChannels;
		if(Image.GetFormat() <= cFormat::Rgba8) {
			Hdr.PixelFormat.RBitMask = NChannels > 2 ? 0x00ff0000 : 0xff;
		} else {
			Hdr.PixelFormat.RBitMask = 0xffff;
		}
		Hdr.PixelFormat.GBitMask = NChannels > 1 ? 0x0000ff00 : 0;
		Hdr.PixelFormat.BBitMask = NChannels > 1 ? 0x000000ff : 0;
		Hdr.PixelFormat.RgbAlphaBitMask = 4 == NChannels ? 0xff000000 : (2 == NChannels ? 0xff00 : 0);
	} else {
		Hdr.PixelFormat.Flags = DDS_FOURCC;
		if(cFormat::IsCompressed(Image.GetFormat())) {
			Hdr.PixelFormat.FourCC = CompressedFourCC[Image.GetFormat() - cFormat::Dxt1];
		} else {
			Hdr.PixelFormat.FourCC = FloatFourCC[Image.GetFormat() - cFormat::Rg16];
		}
		Hdr.PixelFormat.RgbBitCount = 0;
		Hdr.PixelFormat.RBitMask = 0;
		Hdr.PixelFormat.GBitMask = 0;
		Hdr.PixelFormat.BBitMask = 0;
		Hdr.PixelFormat.RgbAlphaBitMask = 0;
	}

	Hdr.Caps.Caps1 = DDS_TEXTURE | (Image.GetMipMapCount() > 1 ? DDS_MIPMAP | DDS_COMPLEX : 0) | (Image.GetDepth() != 1 ? DDS_COMPLEX : 0);
	Hdr.Caps.Caps2 = Image.GetDepth() > 1 ? DDS_VOLUME : (0 == Image.GetDepth() ? DDS_CUBEMAP | DDS_CUBEMAP_ALL_FACES : 0);
	Hdr.Caps.Reserved[0] = 0;
	Hdr.Caps.Reserved[1] = 0;
	Hdr.Reserved2 = 0;

#ifdef COMMS_BIG_ENDIAN
    cMath::EndianSwap(&Hdr, DdsHeader_Format);
#endif // COMMS_BIG_ENDIAN
    To->WriteBytes(&Hdr, sizeof(Hdr));
	
	int Size = Image.GetMipMappedSize(0, Image.GetMipMapCount());
	
	cImage T = Image;
	if(cDimension::TwoD == T.GetDimension()) {
		T.Flip();
	}
	
	if(cFormat::Rgb8 == Image.GetFormat() || cFormat::Rgba8 == Image.GetFormat()) {
		cMath::SwapChannels(T.GetPixels(), Size / NChannels, NChannels, 0, 2);
	}

	int Face, MipMapLevel, FaceSize;
	byte *Ptr;
	if(cDimension::Cube == T.GetDimension()) {
		for(Face = 0; Face < 6; Face++) {
			for(MipMapLevel = 0; MipMapLevel < T.GetMipMapCount(); MipMapLevel++) {
				FaceSize = T.GetMipMappedSize(MipMapLevel, 1) / 6;
				Ptr = T.GetPixels(MipMapLevel) + Face * FaceSize;
				To->WriteBytes(Ptr, FaceSize);
			}
		}
	} else {
		To->WriteBytes(T.GetPixels(), Size);
	}

	return true;
} // cCodecDds::Encode
int cCodecDds::CheckMagic(dword Magic, const char* ext){
	char* s = (char*)&Magic;
	return (s[0] == 'D' && s[1] == 'D' && s[2] == 'S') ? 1 : 0;
}
} // comms
