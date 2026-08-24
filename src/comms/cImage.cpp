#include "comms.h"

namespace comms {

	int cImage::MipMapsAll = 127;

//*****************************************************************************
// DimensionString
//*****************************************************************************
struct DimensionString {
	cDimension::Enum Dimension;
	const char *String;
};

static const DimensionString DimensionStrings[] = {
	{ cDimension::None, "None" },
	{ cDimension::OneD, "1D" },
	{ cDimension::TwoD,	"2D" },
	{ cDimension::ThreeD, "3D" },
	{ cDimension::Cube, "Cube" }
};

//-----------------------------------------------------------------------------
// cDimension::ToString
//-----------------------------------------------------------------------------
const char * cDimension::ToString(const Enum Dimension) {
	const int Count = sizeof(DimensionStrings) / sizeof(DimensionString);
	int i;
	for(i = 0; i < Count; i++) {
		if(Dimension == DimensionStrings[i].Dimension) {
			return DimensionStrings[i].String;
		}
	}
	return nullptr;
} // cDimension::ToString

//*****************************************************************************
// FormatString
//*****************************************************************************
struct FormatString {
	cFormat::Enum Format;
	const char *String;
};

static const FormatString FormatStrings[] = {
	{ cFormat::fmtNone, "None" },

	{ cFormat::R8, "R8" },
	{ cFormat::Rg8, "Rg8" },
	{ cFormat::Rgb8, "Rgb8" },
	{ cFormat::Rgba8, "Rgba8" },

	{ cFormat::R16, "R16" },
	{ cFormat::Rg16, "Rg16" },
	{ cFormat::Rgb16, "Rgb16" },
	{ cFormat::Rgba16, "Rgba16" },

	{ cFormat::R16f, "R16f" },
	{ cFormat::Rg16f, "Rg16f" },
	{ cFormat::Rgb16f, "Rgb16f" },
	{ cFormat::Rgba16f, "Rgba16f" },

	{ cFormat::R32f, "R32f" },
	{ cFormat::Rg32f, "Rg32f" },
	{ cFormat::Rgb32f, "Rgb32f" },
	{ cFormat::Rgba32f, "Rgba32f" },

	{ cFormat::Depth16, "Depth16" },
	{ cFormat::Depth24, "Depth24" },
	{ cFormat::Depth24Stencil8, "Depth24Stencil8" },
	
	{ cFormat::Dxt1, "Dxt1" },
	{ cFormat::Dxt3, "Dxt3" },
	{ cFormat::Dxt5, "Dxt5" }
};

//-----------------------------------------------------------------------------
// cFormat::ToString
//-----------------------------------------------------------------------------
const char * cFormat::ToString(const Enum Format) {
	const int Count = sizeof(FormatStrings) / sizeof(FormatString);
	int i;
	for(i = 0; i < Count; i++) {
		if(Format == FormatStrings[i].Format) {
			return FormatStrings[i].String;
		}
	}
	return nullptr;
} // cFormat::ToString

//-----------------------------------------------------------------------------
// cFormat::ToFormat
//-----------------------------------------------------------------------------
cFormat::Enum cFormat::ToFormat(const char *String) {
	const int Count = sizeof(FormatStrings) / sizeof(FormatString);
	int i;
	for(i = 0; i < Count; i++) {
		if(cStr::EqualsNoCase(String, FormatStrings[i].String)) {
			return FormatStrings[i].Format;
		}
	}
	return fmtNone;
} // cFormat::ToFormat


//-----------------------------------------------------------------------------
// cImage.ctor
//-----------------------------------------------------------------------------
cImage::cImage() {
	m_Width = 0;
	m_Height = 0;
	m_Depth = 0;
	m_Format = cFormat::fmtNone;
	m_MipMapCount = 0;
	m_Pixels = nullptr;
} // cImage.ctor

// cImage.ctor : (const cImage &)
cImage::cImage(const cImage &Src) {
	m_Pixels = nullptr;
	Copy(Src);
}

// cImage::Copy : (...)
void cImage::Copy(const cImage &Src) {
	Free();
	
	m_Width = Src.m_Width;
	m_Height = Src.m_Height;
	m_Depth = Src.m_Depth;
	m_Format = Src.m_Format;
	m_MipMapCount = Src.m_MipMapCount;

	const int Size = GetMipMappedSize(0, m_MipMapCount);
	m_Pixels = new byte[Size];
	memcpy(m_Pixels, Src.m_Pixels, Size);
}

// cImage.ctor : (const cImage &, const cRect &)
cImage::cImage(const cImage &Src, const cRect &rc) {
	m_Pixels = nullptr;
	Copy(Src, rc);
}

// cImage::Copy : (..., const cRect &)
void cImage::Copy(const cImage &Src, const cRect &rc) {
	Free();
	if(!cFormat::IsPlain(Src.GetFormat())) {
		return; // Only plain formats
	}
	int X = (int)rc.GetLeft();
	int Y = (int)rc.GetBottom();
	m_Width = (int)rc.GetWidth();
	m_Height = (int)rc.GetHeight();
	m_Depth = 1;
	m_Format = Src.GetFormat();
	m_MipMapCount = 1;
	int SizeOfPixel = cFormat::BytesPerPixel(m_Format);
	int SrcRow = Src.GetWidth() * SizeOfPixel;
	int ThisRow = m_Width * SizeOfPixel;
	int ThisSize = ThisRow * m_Height;
	m_Pixels = new byte[ThisSize];
	const byte *From = Src.GetPixels() + Y * SrcRow + X * SizeOfPixel;
	byte *To = m_Pixels;
	int i;
	for(i = 0; i < m_Height; i++) {
		memcpy(To, From, ThisRow);
		From += SrcRow;
		To += ThisRow;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------------------
// cImage::Copy
//--------------------------------------------------------------------------------------------------------------------------------------------
void cImage::Copy(const void *Pixels, const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount) {
	Free();

	m_Width = Width;
	m_Height = Height;
	m_Depth = Depth;
	m_Format = Format;
	m_MipMapCount = MipMapCount;

	const int Size = GetMipMappedSize(0, m_MipMapCount);
	m_Pixels = new byte[Size];
	memcpy(m_Pixels, Pixels, Size);
} // cImage::Copy

// cImage.ctor
cImage::cImage(const ECopyCtor, const void *Pixels, const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount) {
	m_Pixels = nullptr;
	Copy(Pixels, Format, Width, Height, Depth, MipMapCount);
}

//-------------------------------------------------------------------------------------------------------------------------------------
// cImage::Set
//-------------------------------------------------------------------------------------------------------------------------------------
void cImage::Set(void *Pixels, const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount) {
	if (Pixels)Free();

	m_Width = Width;
	m_Height = Height;
	m_Depth = Depth;
	m_Format = Format;
	m_MipMapCount = MipMapCount;

	m_Pixels = (byte *)Pixels;
} // cImage::Set
void cImage::SetHeight(int Height) {
	m_Height = Height;
}

// cImage.ctor
cImage::cImage(const ESetCtor, void *Pixels, const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount) {
	m_Pixels = nullptr;
	Set(Pixels, Format, Width, Height, Depth, MipMapCount);
}

//--------------------------------------------------------------------------------------------------------------------------
// cImage::Create
//--------------------------------------------------------------------------------------------------------------------------
void cImage::Create(const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount) {
	Free();

	m_Format = Format;
	m_Width = Width;
	m_Height = Height;
	m_Depth = Depth;
	m_MipMapCount = MipMapCount;

	const int Size = GetMipMappedSize(0, m_MipMapCount);
	m_Pixels = new byte[Size];
} // cImage::Create

// cImage.ctor
cImage::cImage(const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount) {
	m_Pixels = nullptr;
	Create(Format, Width, Height, Depth, MipMapCount);
}

//-----------------------------------------------------------------------------
// cImage::Free
//-----------------------------------------------------------------------------
void cImage::Free() {
	if(m_Pixels != nullptr) {
		delete[] m_Pixels;
		m_Pixels = nullptr;
	}
	m_Width = 0;
	m_Height = 0;
	m_Depth = 0;
	m_MipMapCount = 0;
	m_Format = cFormat::fmtNone;
} // cImage::Free

// cImage.dtor
cImage::~cImage() {
	Free();
}

// cImage::GetPixels
byte * cImage::GetPixels(const int MipMapLevel) const {
	return MipMapLevel < m_MipMapCount ? m_Pixels + GetMipMappedSize(0, MipMapLevel) : nullptr;
}

//-----------------------------------------------------------------------------
// cImage::GetMipMapCountFromDimensions
//-----------------------------------------------------------------------------
int cImage::GetMipMapCountFromDimensions() const {
	int Dim = cMath::Max(m_Width, m_Height);
	int Count = 0;
	while(Dim > 0) {
		Dim >>= 1;
		Count++;
	}
	return Count;
} // cImage::GetMipMapCountFromDimensions

// cImage::CalcImageSize
int cImage::CalcImageSize(const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount) {
	int w = Width;
	int h = Height;
	int d = Depth;
    bool Cubemap = (0 == d);
	if(Cubemap) {
		d = 1;
	}
    int Size = 0;
	int Level = MipMapCount, c;
	while(Level > 0) {
        if(cFormat::IsCompressed(Format)) {
            c = ((w + 3) >> 2) * ((h + 3) >> 2) * d;
		} else {
            c = w * h * d;
		}
    	if(cFormat::IsCompressed(Format)) {
            c *= cFormat::BytesPerBlock(Format);
            if(cFormat::PVRTC4 == Format || cFormat::PVRTC4_Alpha == Format) {
                c = cMath::Max(c, 32); // PVRTC data is at leat 32 bytes in size
            }
        } else {
            c *= cFormat::BytesPerPixel(Format);
        }
        Size += c;
        w >>= 1;
		h >>= 1;
		d >>= 1;
		//if(w + h + d == 0) {
		//	break;
		//}
		w = cMath::Max(w, 1);
		h = cMath::Max(h, 1);
		d = cMath::Max(d, 1);
		Level--;
	}
    if(Cubemap) {
        Size *= 6;
	}
	return Size;
}

// cImage::GetMipMappedSize
int cImage::GetMipMappedSize(const int MipMapFrom, const int MipMapCount, const cFormat::Enum SrcFormat) const {
	int Width = GetWidth(MipMapFrom);
	int Height = GetHeight(MipMapFrom);
	int Depth = GetDepth(MipMapFrom);
    if(0 == m_Depth) {
        Depth = 0;
    }
	const cFormat::Enum Format = SrcFormat == cFormat::fmtNone ? m_Format : SrcFormat;
    return CalcImageSize(Format, Width, Height, Depth, MipMapCount);
}
    
// cImage::GetSliceSize
int cImage::GetSliceSize(const int MipMapLevel, const cFormat::Enum SrcFormat) const {
	const int Width = GetWidth(MipMapLevel);
	const int Height = GetHeight(MipMapLevel);
	const cFormat::Enum Format = SrcFormat == cFormat::fmtNone ? m_Format : SrcFormat;
    return CalcImageSize(Format, Width, Height, 1, 1);
}

// cImage::GetRowSize
int cImage::GetRowSize(const int MipMapLevel, const cFormat::Enum SrcFormat) const {
	const int Width = GetWidth(MipMapLevel);
	const cFormat::Enum Format = (SrcFormat == cFormat::fmtNone) ? m_Format : SrcFormat;
    return CalcImageSize(Format, Width, 1, 1, 1);
}

//-----------------------------------------------------------------------------
// cImage::GetPixelCount
//-----------------------------------------------------------------------------
int cImage::GetPixelCount(const int MipMapFrom, const int MipMapCount) const {
	int Width = GetWidth(MipMapFrom);
	int Height = GetHeight(MipMapFrom);
	int Depth = GetDepth(MipMapFrom);
	int Size = 0;

	int Level = MipMapCount;
	while(Level > 0) {
		Size += Width * Height * Depth;
		Width >>= 1;
		Height >>= 1;
		Depth >>= 1;
		if(Width + Height + Depth == 0) {
			break;
		}
		Width = cMath::Max(Width, 1);
		Height = cMath::Max(Height, 1);
		Depth = cMath::Max(Depth, 1);
		Level--;
	}
	
	if(cDimension::Cube == GetDimension()) {
		Size *= 6;
	}
	return Size;
} // cImage::GetPixelCount

//-----------------------------------------------------------------------------
// cImage::SwapChannels
//-----------------------------------------------------------------------------
bool cImage::SwapChannels(const int Ch0, const int Ch1) {
	if(!cFormat::IsPlain(m_Format)) {
		return false;
	}

	const int NPixels = GetPixelCount(0, m_MipMapCount);
	const int NChannels = cFormat::ChannelCount(m_Format);

	if(m_Format <= cFormat::Rgba8) {
		cMath::SwapChannels((byte *)m_Pixels, NPixels, NChannels, Ch0, Ch1);
	} else if(m_Format <= cFormat::Rgba16f) {
		cMath::SwapChannels((unsigned short *)m_Pixels, NPixels, NChannels, Ch0, Ch1);
	} else {
		cMath::SwapChannels((float *)m_Pixels, NPixels, NChannels, Ch0, Ch1);
	}
	return true;
} // cImage::SwapChannels

//-----------------------------------------------------------------------------------
// cImage::RemoveChannels
//-----------------------------------------------------------------------------------
bool cImage::RemoveChannels(bool KeepCh0, bool KeepCh1, bool KeepCh2, bool KeepCh3) {
	if(!cFormat::IsPlain(m_Format)) {
		return false;
	}

	const int CurChannelCount = cFormat::ChannelCount(m_Format);
	if(CurChannelCount < 4) {
		KeepCh3 = false;
	}
	if(CurChannelCount < 3) {
		KeepCh2 = false;
	}
	if(CurChannelCount < 2) {
		KeepCh1 = false;
	}

	const int NewChannelCount = int(KeepCh0) + int(KeepCh1) + int(KeepCh2) + int(KeepCh3);
	if(NewChannelCount == 0) {
		return false; // Simply call Free!
	}
	if(CurChannelCount == NewChannelCount) {
		return true;
	}

	int NPixels = GetPixelCount(0, m_MipMapCount);
	const int Bpc = cFormat::BytesPerChannel(m_Format);

	m_Format = (cFormat::Enum)(m_Format + (NewChannelCount - CurChannelCount));
	const int Size = GetMipMappedSize(0, m_MipMapCount);
	byte *pPixels = new byte[Size];

	if(Bpc == 1) { // byte
		byte *Fm = m_Pixels;
		byte *To = pPixels;
		do {
			if(KeepCh0) {
				*To++ = Fm[0];
			}
			if(KeepCh1) {
				*To++ = Fm[1];
			}
			if(KeepCh2) {
				*To++ = Fm[2];
			}
			if(KeepCh3) {
				*To++ = Fm[3];
			}
			Fm += CurChannelCount;
		} while(--NPixels);
	} else if(Bpc == 2) { // word (short or half)
		word *Fm = (word *)m_Pixels;
		word *To = (word *)pPixels;
		do {
			if(KeepCh0) {
				*To++ = Fm[0];
			}
			if(KeepCh1) {
				*To++ = Fm[1];
			}
			if(KeepCh2) {
				*To++ = Fm[2];
			}
			if(KeepCh3) {
				*To++ = Fm[3];
			}
			Fm += CurChannelCount;
		} while(--NPixels);
	} else { // dword (float)
		dword *Fm = (dword *)m_Pixels;
		dword *To = (dword *)pPixels;
		do {
			if(KeepCh0) {
				*To++ = Fm[0];
			}
			if(KeepCh1) {
				*To++ = Fm[1];
			}
			if(KeepCh2) {
				*To++ = Fm[2];
			}
			if(KeepCh3) {
				*To++ = Fm[3];
			}
			Fm += CurChannelCount;
		} while(--NPixels);
	}

	delete[] m_Pixels;
	m_Pixels = pPixels;

	return true;
} // cImage::RemoveChannels

//-----------------------------------------------------------------------------
// cImage::InvertChannel
//-----------------------------------------------------------------------------
bool cImage::InvertChannel(const int Ch) {
	if(!cFormat::IsPlain(m_Format)) {
		return false;
	}

	const int NChannels = cFormat::ChannelCount(m_Format);
	if(Ch >= NChannels) {
		return false;
	}
	const int NPixels = GetPixelCount(0, m_MipMapCount);

	int i;
	float f;
	if(m_Format <= cFormat::Rgba8) {
		byte *P = (byte *)m_Pixels;
		for(i = 0; i < NPixels; i++) {
			P[Ch] = 255 - P[Ch];
			P += NChannels;
		}
	} else if(m_Format <= cFormat::Rgba16) {
		word *P = (word *)m_Pixels;
		for(i = 0; i < NPixels; i++) {
			P[Ch] = 65535 - P[Ch];
			P += NChannels;
		}
	} else if(m_Format <= cFormat::Rgba16f) {
		word *P = (word *)m_Pixels;
		for(i = 0; i < NPixels; i++) {
			f = cMath::Half2Float(P[Ch]);
			P[Ch] = cMath::Float2Half(1.0f - f);
			P += NChannels;
		}
	} else {
		cAssert(m_Format <= cFormat::Rgba32f);
		float *P = (float *)m_Pixels;
		for(i = 0; i < NPixels; i++) {
			P[Ch] = 1.0f - P[Ch];
			P += NChannels;
		}
	}
	return true;
} // cImage::InvertChannel

//-----------------------------------------------------------------------------
// cImage::CopyChannel
//-----------------------------------------------------------------------------
bool cImage::CopyChannel(const int From, const int To) {
	if(!cFormat::IsPlain(m_Format)) {
		return false;
	}

	const int NChannels = cFormat::ChannelCount(m_Format);
	if(From >= NChannels) {
		return false;
	}
	if(To >= NChannels) {
		return false;
	}
	const int NPixels = GetPixelCount(0, m_MipMapCount);

	int i;
	if(m_Format <= cFormat::Rgba8) {
		byte *P = (byte *)m_Pixels;
		for(i = 0; i < NPixels; i++) {
			P[To] = P[From];
			P += NChannels;
		}
	} else if(m_Format <= cFormat::Rgba16f) {
		word *P = (word *)m_Pixels;
		for(i = 0; i < NPixels; i++) {
			P[To] = P[From];
			P += NChannels;
		}
	} else {
		cAssert(m_Format <= cFormat::Rgba32f);
		float *P = (float *)m_Pixels;
		for(i = 0; i < NPixels; i++) {
			P[To] = P[From];
			P += NChannels;
		}
	}
	return true;
} // cImage::CopyChannel

//-----------------------------------------------------------------------------
// cImage::ToGrayScale
//-----------------------------------------------------------------------------
bool cImage::ToGrayScale() {
	const int NChannels = cFormat::ChannelCount(m_Format);
	if(!cFormat::IsPlain(m_Format) || cFormat::IsFloat(m_Format) || NChannels < 3) {
		return false;
	}

	int NPixels = GetPixelCount(0, m_MipMapCount);

	if(m_Format <= cFormat::Rgba8) { // byte
		byte *GrayPixels = new byte[NPixels];
		byte *To = GrayPixels;
		byte *Fm = m_Pixels;
		do {
			*To++ = (77 * Fm[0] + 151 * Fm[1] + 28 * Fm[2] + 128) >> 8;
			Fm += NChannels;
		} while(--NPixels);

		delete[] m_Pixels;
		m_Pixels = GrayPixels;
		m_Format = cFormat::R8;
	} else { // word (short)
		word *GrayPixels = new word[NPixels];
		word *To = GrayPixels;
		word *Fm = (word *)m_Pixels;
		do {
			*To++ = (77 * Fm[0] + 151 * Fm[1] + 28 * Fm[2] + 128) >> 8;
			Fm += NChannels;
		} while(--NPixels);

		delete[] m_Pixels;
		m_Pixels = (byte *)GrayPixels;
		m_Format = cFormat::R16;
	}
	
	return true;
} // cImage::ToGrayScale

//----------------------------------------------------------------------------------------
// cImage::ToNormalMap
//----------------------------------------------------------------------------------------
bool cImage::ToNormalMap(const cFormat::Enum Format, const float Z, const bool OldAlpha) {
	const float MipMapScaleZ = 2.0f;
	if(cFormat::IsCompressed(m_Format)) {
		Uncompress();
	}
	RemoveMipMaps();
	if(!MakePowerOfTwo()) {
		return false;
	}
	
	cImage Src;
	if(OldAlpha && cFormat::Rgba8 == Format && cFormat::Rgba8 == m_Format) {
		Src.Copy(*this);
	}
	
	if(cFormat::Rgb8 == m_Format || cFormat::Rgba8 == m_Format) {
		ToGrayScale();
	}
	if(m_Format != cFormat::R8) {
		return false;
	}
	if(m_Depth > 1) { // Special case: Volume texture
		cAssert(cFormat::Rgb8 == Format || cFormat::Rgba8 == Format);
		if(Format != cFormat::Rgb8 && Format != cFormat::Rgba8) {
			return false;
		}
		int i, SS;
		cImage T, R;
		R.Create(cFormat::Rgba8, m_Width, m_Height, m_Depth, 1);
		SS = m_Width * m_Height * cFormat::BytesPerPixel(cFormat::Rgba8);
		for(i = 0; i < m_Depth; i++) {
			const void *P = m_Pixels + (m_Width * m_Height * i);
			T.Copy(P, cFormat::R8, m_Width, m_Height, 1, 1);
			T.ToNormalMap(cFormat::Rgba8);
			void *C = R.m_Pixels + SS * i;
			memcpy(C, T.m_Pixels, SS);
		}
		if(cFormat::Rgb8 == Format) {
			R.RemoveChannels(true, true, true, false);
		}
		Copy(R);
		return true;
	}
	
	dword xMask = 0, yMask = 0, zMask = 0, hMask = 0;
	dword xShift = 0, yShift = 0, zShift = 0, hShift = 0, hFlip = 0;

	switch(Format) {
		case cFormat::Rg8:
			xMask = yMask = 0xff;
			xShift = 8;
			break;
		case cFormat::Rgba8:
			xMask = yMask = zMask = hMask = 0xff;
			yShift = 8;
			zShift = 16;
			hShift = 24;
			break;
		default:
			return false;
	}
	
	const float SobelX[5][5] = {
		{ 1,  2,  0,  -2,  -1 },
		{ 4,  8,  0,  -8,  -4 },
		{ 6, 12,  0, -12,  -6 },
		{ 4,  8,  0,  -8,  -4 },
		{ 1,  2,  0,  -2,  -1 }
	};
	const float SobelY[5][5] = {
		 { 1,  4,   6,  4,  1} ,
		 { 2,  8,  12,  8,  2 },
		 { 0,  0,   0,  0,  0 },
		{ -2, -8, -12, -8, -2 },
		{ -1, -4,  -6, -4, -1 }
	};

	const bool UseWord = cFormat::BytesPerPixel(Format) == 2; // Rg8

	const float xFactor = 0.5f * xMask;
	const float yFactor = 0.5f * yMask;
	const float zFactor = 0.5f * zMask;

	float sZ = Z * 128.0f / cMath::Max(m_Width, m_Height);
	
	const int Size = GetMipMappedSize(0, m_MipMapCount, Format);
	byte *NormalMapPixels = new byte[Size];

	union {
		dword *dwTo;
		word  *wTo;
	};
	dwTo = (dword *)NormalMapPixels;

	for(int Level = 0; Level < m_MipMapCount; Level++) {
		byte *Fm = GetPixels(Level);

		const int Width = GetWidth(Level);
		const int Height = GetHeight(Level);

		for(int y = 0; y < Height; y++) {
			for(int x = 0; x < Width; x++) {
				// Sobel Filter:
				float sX = 0.0f, sY = 0.0f;
				for(int dy = 0; dy < 5; dy++) {
					int fy = (y + dy - 2 + Height) % Height;
					for(int dx = 0; dx < 5; dx++) {
						int fx = (x + dx - 2 + Width) % Width;
						sX += SobelX[dy][dx] * Fm[fy * Width + fx];
						sY += SobelY[dy][dx] * Fm[fy * Width + fx];
					}
				}

				// Construct the components:
				sX *= 1.0f / (48 * 255);
				sY *= 1.0f / (48 * 255);

				// Normalize:
				const float il = 1.0f / cMath::Sqrt(sX * sX + sY * sY + sZ * sZ);
				const float rX = xFactor * (sX * il + 1.0f);
				const float rY = yFactor * (sY * il + 1.0f);
				const float rZ = zFactor * (sZ * il + 1.0f);

				// Store:
				dword Res = 0;
				Res |= (int(rX) & xMask) << xShift;
				Res |= (int(rY) & yMask) << yShift;
				Res |= (int(rZ) & zMask) << zShift;
				Res |= ((Fm[y * Width + x] ^ hFlip) & hMask) << hShift;
#ifdef COMMS_BIG_ENDIAN
				cMath::EndianSwap4(&Res);
#endif // COMMS_BIG_ENDIAN
				if(UseWord) {
					*wTo++ = (word)Res;
				} else {
					*dwTo++ = Res;
				}
			}
		}
		sZ *= MipMapScaleZ;
	}

	m_Format = Format;
	delete[] m_Pixels;
	m_Pixels = NormalMapPixels;

	int T, i;
	if(Src.GetPixels() != nullptr) {
		cAssert(cFormat::Rgba8 == m_Format);
		cAssert(cFormat::Rgba8 == Src.GetFormat());
		T = m_Width * m_Height * m_Depth;
		for(i = 0; i < T; i++) {
			const byte *S = &Src.GetPixels()[4 * i];
			byte *D = &m_Pixels[4 * i];
			D[3] = S[3];
		}
	}

	return true;
} // cImage::ToNormalMap

// BuildMipMap
template<typename TYPE>
static void BuildMipMap(TYPE *To, const TYPE *From, const dword _Width, const dword _Height, const dword Depth, const dword NChannels) {
	dword XOffset = _Width < 2 ? 0 : NChannels;
	dword YOffset = _Height < 2 ? 0 : NChannels * _Width;
	dword ZOffset = Depth < 2 ? 0 : NChannels * _Width * _Height;

	int Width = (_Width >> 1) << 1;
	int Height = (_Height >> 1) << 1;

	dword X, Y, Z, i;	

	for(Z = 0; Z < Depth; Z += 2) {
		for(Y = 0; Y < Height; Y += 2) {
			for(X = 0; X < Width; X += 2) {
				for(i = 0; i < NChannels; i++) {
					*To++ = (From[0] + From[XOffset] + From[YOffset] + From[YOffset + XOffset] + From[ZOffset] + From[ZOffset + XOffset] + From[ZOffset + YOffset] + From[ZOffset + YOffset + XOffset]) / 8;
					From++;
				}
				From += XOffset;
			}
			From += YOffset;
		}
		From += ZOffset;
	}
}

//-----------------------------------------------------------------------------
// cImage::CreateMipMaps
//-----------------------------------------------------------------------------
bool cImage::CreateMipMaps(const int MipMapCount) {
	if (cFormat::IsCompressed(m_Format)) {
		return false;
	}

	int M = cMath::Min(MipMapCount, GetMipMapCountFromDimensions());

	int SizeTotal = 0;
	if (m_MipMapCount != M) {
		SizeTotal = GetMipMappedSize(0, M);
		int Size0 = GetMipMappedSize(0, 1);
		byte* T = new byte[SizeTotal];
		memcpy(T, m_Pixels, Size0); // Keep the first mipmap level
		delete[] m_Pixels;
		m_Pixels = T;
		T = nullptr;

		m_MipMapCount = M;
	}

	const int NChannels = cFormat::ChannelCount(m_Format);
	const int NSides = cDimension::Cube == GetDimension() ? 6 : 1;

	byte* From = GetPixels(0);
	byte* To = GetPixels(1);
	byte* base = From;

	int Level, Width, Height, Depth, FromSize, ToSize, s;

	for (Level = 1; Level < m_MipMapCount; Level++) {
		Width = GetWidth(Level - 1);
		Height = GetHeight(Level - 1);
		Depth = GetDepth(Level - 1);

		FromSize = GetMipMappedSize(Level - 1, 1) / NSides;
		ToSize = GetMipMappedSize(Level, 1) / NSides;

		for (s = 0; s < NSides; s++) {
			if (cFormat::IsPlain(m_Format)) {
				if (cFormat::IsFloat(m_Format)) {
					BuildMipMap((float*)To, (float*)From, Width, Height, Depth, NChannels);
				}
				else if (m_Format >= cFormat::R16) {
					BuildMipMap((word*)To, (word*)From, Width, Height, Depth, NChannels);
				}
				else {
					BuildMipMap(To, From, Width, Height, Depth, NChannels);
				}
			}
			From += FromSize;
			To += ToSize;
		}
	}
	if (SizeTotal) {
		cAssert(To - base == SizeTotal);
	}
	return true;
} // cImage::CreateMipMaps

//-----------------------------------------------------------------------------
// cImage::RemoveMipMaps
//-----------------------------------------------------------------------------
void cImage::RemoveMipMaps() {
    if(m_MipMapCount <= 1) {
		return;
	}
	int Size = GetMipMappedSize(0, 1);
	byte *Pixels = new byte[Size];
	memcpy(Pixels, m_Pixels, Size);
    delete[] m_Pixels;
	m_Pixels = Pixels;
    m_MipMapCount = 1;
} // cImage::RemoveMipMaps

//--------------------------------------------------------------------------------------------------------------------------------------------
// DecodeColorBlock
//--------------------------------------------------------------------------------------------------------------------------------------------
static void DecodeColorBlock(byte *To, int Width, int Height, int XOffset, int YOffset, cFormat::Enum Format, int Red, int Blue, byte *From) {
	byte Colors[4][3];

	word c0 = *(word *)From;
	word c1 = *(word *)(From + 2);

#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap2(&c0);
	cMath::EndianSwap2(&c1);
#endif // COMMS_BIG_ENDIAN

	Colors[0][0] = ((c0 >> 11) & 0x1f) << 3;
	Colors[0][1] = ((c0 >> 5) & 0x3f) << 2;
	Colors[0][2] = (c0 & 0x1f) << 3;

	Colors[1][0] = ((c1 >> 11) & 0x1f) << 3;
	Colors[1][1] = ((c1 >> 5) & 0x3f) << 2;
	Colors[1][2] = (c1 & 0x1f) << 3;

	int i;
	if(c0 > c1 || cFormat::Dxt5 == Format) {
		for(i = 0; i < 3; i++) {
			Colors[2][i] = (2 * Colors[0][i] + Colors[1][i] + 1) / 3;
			Colors[3][i] = (Colors[0][i] + 2 * Colors[1][i] + 1) / 3;
		}
	} else {
		for(i = 0; i < 3; i++) {
			Colors[2][i] = (Colors[0][i] + Colors[1][i] + 1) >> 1;
			Colors[3][i] = 0;
		}
	}

	From += 4;
	int Y, X;
	byte *D;
	dword Indexes, Index;
	for(Y = 0; Y < Height; Y++) {
		D = To + YOffset * Y;
		Indexes = From[Y];
		for(X = 0; X < Width; X++) {
			Index = Indexes & 0x3;
			D[Red] = Colors[Index][0];
			D[1] = Colors[Index][1];
			D[Blue] = Colors[Index][2];
			Indexes >>= 2;

			D += XOffset;
		}
	}
} // DecodeColorBlock

//-------------------------------------------------------------------------------------------------------
// DecodeDxt3AlphaBlock
//-------------------------------------------------------------------------------------------------------
static void DecodeDxt3AlphaBlock(byte *To, int Width, int Height, int XOffset, int YOffset, byte *From) {
	int Y, X;
	byte *D;
	dword Alpha;

	for(Y = 0; Y < Height; Y++) {
		D = To + YOffset * Y;
		Alpha = ((word *)From)[Y];
#ifdef COMMS_BIG_ENDIAN
		cMath::EndianSwap4(&Alpha);
#endif // COMMS_BIG_ENDIAN
		for(X = 0; X < Width; X++) {
			*D = (byte)((Alpha & 0xf) * 17);
			Alpha >>= 4;
			D += XOffset;
		}
	}
} // DecodeDxt3AlphaBlock

//-------------------------------------------------------------------------------------------------------
// DecodeDxt5AlphaBlock
//-------------------------------------------------------------------------------------------------------
static void DecodeDxt5AlphaBlock(byte *To, int Width, int Height, int XOffset, int YOffset, byte *From) {
	byte a0 = From[0];
	byte a1 = From[1];
	qword Alpha = *((qword *)From);
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap8(&Alpha);
#endif // COMMS_BIG_ENDIAN
	Alpha >>= 16;
	int Y, X, k;
	byte *D;
	for(Y = 0; Y < Height; Y++) {
		D = To + YOffset * Y;
		for(X = 0; X < Width; X++) {
			k = ((dword)Alpha) & 0x7;
			if(0 == k) {
				*D = a0;
			} else if(1 == k) {
				*D = a1;
			} else if(a0 > a1) {
				*D = ((8 - k) * a0 + (k - 1) * a1) / 7;
			} else if(k >= 6) {
				*D = 6 == k ? 0 : 255;
			} else {
				*D = ((6 - k) * a0 + (k - 1) * a1) / 5;
			}
			Alpha >>= 3;
			D += XOffset;
		}
		if(Width < 4) {
			Alpha >>= 3 * (4 - Width);
		}
	}
} // DecodeDxt5AlphaBlock

//----------------------------------------------------------------------------------------------------
// DecodeCompressedImage
//----------------------------------------------------------------------------------------------------
static void DecodeCompressedImage(byte *To, byte *From, int Width, int Height, cFormat::Enum Format) {
	int Sx = Width < 4 ? Width : 4;
	int Sy = Height < 4 ? Height : 4;
	int NChannels = cFormat::ChannelCount(Format);

	int Y, X;
	byte *D;

	for(Y = 0; Y < Height; Y += 4) {
		for(X = 0; X < Width; X += 4) {
			D = To + (Y * Width + X) * NChannels;
			if(cFormat::Dxt3 == Format) {
				DecodeDxt3AlphaBlock(D + 3, Sx, Sy, NChannels, Width * NChannels, From);
				From += 8;
			} else if(cFormat::Dxt5 == Format) {
				DecodeDxt5AlphaBlock(D + 3, Sx, Sy, NChannels, Width * NChannels, From);
				From += 8;
			}
			DecodeColorBlock(D, Sx, Sy, NChannels, Width * NChannels, Format, 0, 2, From);
			From += 8;
		}
	}
} // DecodeCompressedImage

//-----------------------------------------------------------------------------
// cImage::Uncompress
//-----------------------------------------------------------------------------
void cImage::Uncompress() {
	if(!cFormat::IsCompressed(m_Format)) {
		return;
	}

	if(!cMath::IsPowerOfTwo(m_Width) || !cMath::IsPowerOfTwo(m_Height)) {
		RemoveMipMaps();
	}

	cFormat::Enum ToFormat = cFormat::Dxt1 == m_Format ? cFormat::Rgb8 : cFormat::Rgba8;
	byte *Pixels = new byte[GetMipMappedSize(0, m_MipMapCount, ToFormat)];
	int Level = 0, Width, Height, Depth;
	byte *From, *To = Pixels;
	int ToSliceSize, FromSliceSize, Slice;

	while((From = GetPixels(Level)) != nullptr) {
		Width = GetWidth(Level);
		Height = GetHeight(Level);
		Depth = 0 == m_Depth ? 6 : GetDepth(Level);

		ToSliceSize = GetSliceSize(Level, ToFormat);
		FromSliceSize = GetSliceSize(Level, m_Format);

		for(Slice = 0; Slice < Depth; Slice++) {
			DecodeCompressedImage(To, From, Width, Height, m_Format);
			
			To += ToSliceSize;
			From += FromSliceSize;
		}
		Level++;
	}

	m_Format = ToFormat;
	delete[] m_Pixels;
	m_Pixels = Pixels;
} // cImage::Uncompress

// FlipDxt3AlphaBlock
static void FlipDxt3AlphaBlock(byte *Ptr, const int Height) {
	if(Height >= 2) {
		word &Row0 = *((word *)Ptr);
		word &Row1 = *((word *)Ptr + 1);
		if(Height >= 4) {
			word &Row2 = *((word *)Ptr + 2);
			word &Row3 = *((word *)Ptr + 3);
			cMath::Swap(Row0, Row3);
			cMath::Swap(Row1, Row2);
		} else {
			cMath::Swap(Row0, Row1);
		}
	}
}

// FlipDxt5AlphaBlock
static void FlipDxt5AlphaBlock(byte *Ptr, const int Height) {
	qword Hdr, Row0, Row1, Row2, Row3;
	if(Height >= 2) {
		qword &Block = *((qword *)Ptr);
#ifdef COMMS_BIG_ENDIAN
		cMath::EndianSwap8(&Block);
#endif // COMMS_BIG_ENDIAN
		Hdr = Block & 0xffff;
		Row0 = (Block >> 16) & 0xfff;
		Row1 = (Block >> 28) & 0xfff;
		if(Height >= 4) {
			Row2 = (Block >> 40) & 0xfff;
			Row3 = (Block >> 52) & 0xfff;
			Block = Hdr + (Row3 << 16) + (Row2 << 28) + (Row1 << 40) + (Row0 << 52);
		} else {
			Block = Hdr + (Row1 << 16) + (Row0 << 28);
		}
#ifdef COMMS_BIG_ENDIAN
		cMath::EndianSwap8(&Block);
#endif // COMMS_BIG_ENDIAN
	}
}

// FlipCompressedColorBlock
static void FlipCompressedColorBlock(byte *Ptr, const int Height) {
	if(Height >= 2) {
		byte &Row0 = *(Ptr + 4);
		byte &Row1 = *(Ptr + 5);
		if(Height >= 4) {
			byte &Row2 = *(Ptr + 6);
			byte &Row3 = *(Ptr + 7);
			cMath::Swap(Row0, Row3);
			cMath::Swap(Row1, Row2);
		} else {
			cMath::Swap(Row0, Row1);
		}
	}
}

// FlipCompressedImage
static void FlipCompressedImage(byte *To, byte *From, int Width, int Height, cFormat::Enum Format) {
	// Assumes that "To" has already duplicated data of "From"
	
	int BytesPerLineOfBlocks = Width / 4 * cFormat::BytesPerBlock(Format);
	int b;
	byte *Line, *LineTo;
	cList<byte> Buffer;
	

	if(Height > 4) { // Flip blocks
		for(b = 0; b < Height / 4; b++) {
			Line = From + b * BytesPerLineOfBlocks;
			LineTo = To + (Height / 4 - b - 1) * BytesPerLineOfBlocks;
            Buffer.Copy( Line, BytesPerLineOfBlocks );
			memcpy(Line, LineTo, BytesPerLineOfBlocks);
			memcpy(LineTo, Buffer.ToPtr(), BytesPerLineOfBlocks);
		}
	}

	int Y, X;
	
	for(Y = 0; Y < Height; Y += 4) {
		for(X = 0; X < Width; X += 4) {
			if(cFormat::Dxt3 == Format) {
				FlipDxt3AlphaBlock(To, Height);
				To += 8;
			} else if(cFormat::Dxt5 == Format) {
				FlipDxt5AlphaBlock(To, Height);
				To += 8;
			}
			
			FlipCompressedColorBlock(To, Height);
			To += 8;
		}
	}
}

// FlipUncompressedImage
static void FlipUncompressedImage(byte *To, byte *From, int Width, int Height, cFormat::Enum Format) {
	cAssert(cFormat::IsPlain(Format));
	
	int BytesPerPixel = cFormat::BytesPerPixel(Format);
	int BytesPerLine = BytesPerPixel * Width;
	int h;
	byte *Line, *LineTo;
	cList<byte> Buffer;
	
	for(h = 0; h < Height; h++) {
		Line = From + h * BytesPerLine;
		LineTo = To + (Height - h - 1) * BytesPerLine;
        Buffer.Copy( Line, BytesPerLine );
		memcpy(Line, LineTo, BytesPerLine);
		memcpy(LineTo, Buffer.ToPtr(), BytesPerLine);
	}
}

//-----------------------------------------------------------------------------
// cImage::Flip
//-----------------------------------------------------------------------------
bool cImage::Flip() {
	cAssert(!cFormat::IsDepth(m_Format));
	if(cFormat::IsDepth(m_Format)) {
		return false;
	}
	
	if(cFormat::IsCompressed(m_Format)) {
		if(((m_Width & 3) != 0) || ((m_Height & 3) != 0)) {
			cLog::Warning("Compressed images must have dimensions that are multiples of 4");
			return false;
		}
	}
	
	int TotalSize = GetMipMappedSize(0, m_MipMapCount);
	byte *Pixels = new byte[TotalSize];
	memcpy(Pixels, m_Pixels, TotalSize);
	
	byte *Src, *Ptr, *To, *PtrTo;
	int MipMapLevel = 0, Size, i, Width, Height;

	int Depth;
	
	while((Src = GetPixels(MipMapLevel)) != nullptr) {
		To = Pixels + GetMipMappedSize(0, MipMapLevel);
		
		Width = GetWidth(MipMapLevel);
		Height = GetHeight(MipMapLevel);

		Size = GetMipMappedSize(MipMapLevel, 1);
		
		if(cDimension::Cube == GetDimension()) {
			Size /= 6;
			for(i = 0; i < 6; i++) {
				Ptr = Src + i * Size;
				PtrTo = To + i * Size;
				if(cFormat::IsCompressed(m_Format)) {
					FlipCompressedImage(PtrTo, Ptr, Width, Height, m_Format);
				} else {
					FlipUncompressedImage(PtrTo, Ptr, Width, Height, m_Format);
				}
			}
		} else if(cDimension::ThreeD == GetDimension()) {
			Depth = GetDepth(MipMapLevel);
			Size /= Depth;
			
			for(i = 0; i < Depth; i++) {
				Ptr = Src + i * Size;
				PtrTo = To + i * Size;
				if(cFormat::IsCompressed(m_Format)) {
					FlipCompressedImage(PtrTo, Ptr, Width, Height, m_Format);
				} else {
					FlipUncompressedImage(PtrTo, Ptr, Width, Height, m_Format);
				}
			}
		} else if(cDimension::TwoD == GetDimension()) {
			if(cFormat::IsCompressed(m_Format)) {
				FlipCompressedImage(To, Src, Width, Height, m_Format);
			} else {
				FlipUncompressedImage(To, Src, Width, Height, m_Format);
			}
		} else {
			// No sense to flip 1D image
		}
		MipMapLevel++;
	}

	delete[] m_Pixels;
	m_Pixels = Pixels;
	
	return true;
} // cImage::Flip

//-------------------------------------------------------------------------------------------------------------
// EncodeGrayDXTColorBlock
//-------------------------------------------------------------------------------------------------------------
static qword EncodeGrayDXTColorBlock(const int w, const int h, const int xOff, const int yOff, byte *source) {
	int x, y, k;

	// Find range
	byte lo = 255;
	byte hi = 0;
	for(y = 0; y < h; y++) {
		byte *src = source + yOff * y;
		for(x = 0; x < w; x++) {
			if(*src < lo) {
				lo = *src;
			}
			if(*src > hi) {
				hi = *src;
			}
			src += xOff;
		}
	}
	
	// Do proper rounding to 5 bits
	byte hi5 = (62 * hi + 31) / 510;
	byte lo5 = (62 * lo + 31) / 510;
	
	// Select first encoding technique defined in DXT by ensuring col0 > col1. Also, avoid division by zero below.
	if(hi5 == lo5) {
		if(hi5 < 31) {
			hi++;
			hi5++;
		} else {
			lo--;
			lo5--;
		}
	}
	int diff = hi - lo;
	
	// The two end colors
	dword c0 = hi5 | (hi5 << 6) | (hi5 << 11);
	dword c1 = lo5 | (lo5 << 6) | (lo5 << 11);
	
	qword block = c0 | (c1 << 16);
	
	static int kRemap[] = { 1, 3, 2, 0 };
	
	for(y = 0; y < h; y++){
		byte *src = source + yOff * y;
		char shift = 8 * y + 32;
		for (x = 0; x < w; x++){
			// Find the best matching index
			k = (3 * (*src - lo) + (diff >> 1)) / diff;
			
			block |= qword(kRemap[k]) << shift;
			
			src += xOff;
			shift += 2;
		}
	}
	return block;
} // EncodeGrayDXTColorBlock

//---------------------------------------------------------------------------------------------------------
// EncodeDXTColorBlock
//---------------------------------------------------------------------------------------------------------
static qword EncodeDXTColorBlock(const int w, const int h, const int xOff, const int yOff, byte *source) {
	// Find range in each channel
	int min[3] = { 255, 255, 255 };
	int max[3] = { 0, 0, 0 };
	
	int y, x, c, i;
	for(y = 0; y < h; y++) {
		byte *src = source + yOff * y;
		for(x = 0; x < w; x++) {
			for(c = 0; c < 3; c++) {
				if(src[c] < min[c]) min[c] = src[c];
				if(src[c] > max[c]) max[c] = src[c];
			}
			src += xOff;
		}
	}
	
	// Find the channel that has highest range
	int maxCh = 0;
	int chMax = max[0] - min[0];
	for(c = 1; c < 3; c++) {
		byte m = max[c] - min[c];
		if(m > chMax) {
			maxCh = c;
			chMax = m;
		}
	}
	
	// Use the highest range channel as a base for finding the end point colors
	byte colors[4][3];
	colors[0][maxCh] = max[maxCh];
	colors[1][maxCh] = min[maxCh];
	
	// Find the sum(x) and sum(x^2) for the Least Squares fitting below
	int sumX = 0;
	int sumX2 = 0;
	for(y = 0; y < h; y++) {
		byte *src = source + yOff * y;
		for(x = 0; x < w; x++) {
			sumX += src[maxCh];
			sumX2 += src[maxCh] * src[maxCh];
			src += xOff;
		}
	}
	
	int n = w * h;
	for(c = 0; c < 3; c++) {
		if(c == maxCh) continue;
		if(min[c] != max[c]) {
			// Find sum(x) and sum(x * y)
			int sumY = 0;
			int sumXY = 0;
			for(y = 0; y < h; y++) {
				byte *src = source + yOff * y;
				for(x = 0; x < w; x++) {
					sumY += src[c];
					sumXY += src[maxCh] * src[c];
					src += xOff;
				}
			}
			
			// Do a Least Squares fitting to y = a + b * x. Division by d deferred to stick to integer math.
			int d = n * sumX2 - sumX * sumX;
			
			int a = (sumY * sumX2 - sumX * sumXY);
			int b = (n * sumXY - sumX * sumY);
			
			// Compute the end points
			int ch0 = (a + b * colors[0][maxCh]) / d;
			int ch1 = (a + b * colors[1][maxCh]) / d;
			
			// Clamp if neccesary
			if(ch0 < 0) ch0 = 0;
			if(ch0 > 255) ch0 = 255;
			if(ch1 < 0) ch1 = 0;
			if(ch1 > 255) ch1 = 255;
			
			colors[0][c] = ch0;
			colors[1][c] = ch1;
		} else {
			colors[0][c] = min[c];
			colors[1][c] = min[c];
		}
	}
	
	// Compute the other two colors in the local palette
	for(c = 0; c < 3; c++) {
		colors[2][c] = (2 * colors[0][c] + colors[1][c]) / 3;
		colors[3][c] = (colors[0][c] + 2 * colors[1][c]) / 3;
	}
	
	dword block = 0;
	for(y = 0; y < h; y++) {
		byte *src = source + yOff * y;
		char shift = 8 * y;
		
		for(x = 0; x < w; x++) {
			// Find the best matching color in the palette (best = least square difference)
			int index = 0;
			int minError = 0x7FFFFFFF;
			for(i = 0; i < 4; i++) {
				int error = 0;
				for(c = 0; c < 3; c++) {
					int d = src[c] - colors[i][c];
					error += d * d;
				}
				if(error < minError) {
					minError = error;
					index = i;
				}
			}
			
			block |= qword(index) << shift;
			
			src += xOff;
			shift += 2;
		}
	}
	
	// Convert colors to RGB565
	dword col0 = (colors[0][2] >> 3) | ((colors[0][1] & 0xFC) << 3) | ((colors[0][0] & 0xF8) << 8);
	dword col1 = (colors[1][2] >> 3) | ((colors[1][1] & 0xFC) << 3) | ((colors[1][0] & 0xF8) << 8);
	
	// Select first encoding technique defined in DXT by ensuring col0 > col1
	if(col0 <= col1) {
		if(col0 == col1) {
			// Make col0 > col1 by setting the LSB in green to 1 and 0 respectively
			col0 |=  0x20;
			col1 &= ~0x20;
		} else {
			// Make col0 > col1 by swapping them and fixing up the index block accordingly
			dword temp = col0;
			col0 = col1;
			col1 = temp;
			
			block ^= 0x55555555;
		}
	}
	return col0 | (col1 << 16) | (qword(block) << 32);
} // EncodeDXTColorBlock

//----------------------------------------------------------------------------------------------------------
// EncodeDXT5AlphaBlock
//----------------------------------------------------------------------------------------------------------
static qword EncodeDXT5AlphaBlock(const int w, const int h, const int xOff, const int yOff, byte *source) {
	// Find range
	byte lo = 255;
	byte hi = 0;
	int y, x, k;
	for(y = 0; y < h; y++) {
		byte *src = source + yOff * y;
		for(x = 0; x < w; x++) {
			if(*src < lo) lo = *src;
			if(*src > hi) hi = *src;
			src += xOff;
		}
	}
	
	// Select first encoding technique defined in DXT5 by ensuring a0 > a1
	if(hi == lo) {
		if(hi < 255) hi++; else lo--;
	}
	int diff = hi - lo;
	
	qword block = hi | (lo << 8);
	
	static int kRemap[] = { 1, 7, 6, 5, 4, 3, 2, 0 };

	for(y = 0; y < h; y++) {
		byte *src = source + yOff * y;
		char shift = 12 * y + 16;
		for(x = 0; x < w; x++) {
			// Find the best matching index
			k = (7 * (*src - lo) + (diff >> 1)) / diff;
			block |= qword(kRemap[k]) << shift;
			
			src += xOff;
			shift += 3;
		}
	}
	return block;
} // EncodeDXT5AlphaBlock

//--------------------------------------------------------------------------------------------------------------------------------------------------
// EncodeCompressedImage
//--------------------------------------------------------------------------------------------------------------------------------------------------
static void EncodeCompressedImage(byte *dst, byte *source, const int width, const int height, const int nChannels, const cFormat::Enum destFormat) {
	int sx = (width  < 4)? width  : 4;
	int sy = (height < 4)? height : 4;
	
	int y, x;
	qword *q;
	for(y = 0; y < height; y += 4) {
		for(x = 0; x < width; x += 4) {
			byte *src = source + (y * width + x) * nChannels;
			if(cFormat::Dxt1 == destFormat) {
				q = (qword *)dst;
				if(1 == nChannels) {
					*q = EncodeGrayDXTColorBlock(sx, sy, 1, width, src);
				} else {
					*q = EncodeDXTColorBlock(sx, sy, nChannels, width * nChannels, src);
				}
#ifdef COMMS_BIG_ENDIAN
				cMath::EndianSwap8(q);
#endif // COMMS_BIG_ENDIAN
				dst += 8;
			} else {
				if(cFormat::Dxt5 == destFormat) {
					q = (qword *)dst;
					*q = EncodeDXT5AlphaBlock(sx, sy, nChannels, width * nChannels, src + 3);
#ifdef COMMS_BIG_ENDIAN
					cMath::EndianSwap8(q);
#endif // COMMS_BIG_ENDIAN
					dst += 8;
				}
				q = (qword *)dst;
				*q = EncodeDXTColorBlock(sx, sy, nChannels, width * nChannels, src);
#ifdef COMMS_BIG_ENDIAN
				cMath::EndianSwap8(q);
#endif // COMMS_BIG_ENDIAN
				dst += 8;
			}
		}
	}
} // EncodeCompressedImage

//-----------------------------------------------------------------------------
// cImage::Compress
//-----------------------------------------------------------------------------
void cImage::Compress() {
	if(m_Format != cFormat::R8 && m_Format != cFormat::Rgb8 && m_Format != cFormat::Rgba8) {
		return;
	}
	int nChannels = cFormat::ChannelCount(m_Format);
	cFormat::Enum destFormat = (cFormat::R8 == m_Format || cFormat::Rgb8 == m_Format) ? cFormat::Dxt1 : cFormat::Dxt5;
	byte *newPixels = new byte[GetMipMappedSize(0, m_MipMapCount, destFormat)];
	
	int level = 0;
	byte *src, *dst = newPixels;
	while((src = GetPixels(level)) != nullptr) {
		int w = GetWidth(level);
		int h = GetHeight(level);
		int d = (0 == m_Depth) ? 6 : GetDepth(level);
		
		int dstSliceSize = CalcImageSize(destFormat, w, h, 1, 1);
		int srcSliceSize = CalcImageSize(m_Format, w, h, 1, 1);
		
		for(int slice = 0; slice < d; slice++) {
			EncodeCompressedImage(dst, src, w, h, nChannels, destFormat);
			dst += dstSliceSize;
			src += srcSliceSize;
		}
		level++;
	}
	m_Format = destFormat;
	delete[] m_Pixels;
	m_Pixels = newPixels;
} // cImage::Compress

//-----------------------------------------------------------------------------
// cImage::ToFormat
//-----------------------------------------------------------------------------
bool cImage::ToFormat(const cFormat::Enum Format) {
	if (GetWidth() == 0 || GetHeight() == 0)return false;
	byte *Pixels, *Src, *To;
	int NPixels = GetPixelCount(0, m_MipMapCount), SrcSize, SrcChannels, ToSize, ToChannels, i;
	cColor C;
	float Rgba[4];

	if(!cFormat::IsPlain(m_Format) || !cFormat::IsPlain(Format)) {
		return false;
	}
	if(Format == m_Format) {
		return true;
	}

	Src = m_Pixels;
	To = Pixels = new byte[GetMipMappedSize(0, m_MipMapCount, Format)];

	//*********************************************************************
	// Rgb8 -> Rgba8
	//*********************************************************************
	if(cFormat::Rgb8 == m_Format && Format == cFormat::Rgba8) {
		do {
			To[0] = Src[0];
			To[1] = Src[1];
			To[2] = Src[2];
			To[3] = 255;
			To += 4;
			Src += 3;
		} while(--NPixels);
	} else {
		SrcSize = cFormat::BytesPerPixel(m_Format);
		SrcChannels = cFormat::ChannelCount(m_Format);

		ToSize = cFormat::BytesPerPixel(Format);
		ToChannels = cFormat::ChannelCount(Format);

		do {
			if(cFormat::IsFloat(m_Format)) {
				if(m_Format <= cFormat::Rgba16f) {
					for(i = 0; i < SrcChannels; i++) {
						Rgba[i] = cMath::Half2Float(((word *)Src)[i]);
					}
				} else {
					for(i = 0; i < SrcChannels; i++) {
						Rgba[i] = ((float *)Src)[i];
					}
				}
			} else if(m_Format >= cFormat::R16 && m_Format <= cFormat::Rgba16) {
				for(i = 0; i < SrcChannels; i++) {
					Rgba[i] = ((word *)Src)[i] * (1.0f / 65535.0f);
				}
			} else {
				for(i = 0; i < SrcChannels; i++) {
					Rgba[i] = Src[i] * (1.0f / 255.0f);
				}
			}

			if(SrcChannels < 4) {
				Rgba[3] = 1.0f;
			}
			if(1 == SrcChannels) {
				Rgba[2] = Rgba[1] = Rgba[0];
			}
			if (2 == SrcChannels) {
				Rgba[3] = Rgba[1];
				Rgba[2] = Rgba[1] = Rgba[0];
			}
			if(1 == ToChannels) {
				Rgba[0] = cColor::ToGray(*((const cColor *)Rgba));
			}

			if(cFormat::IsFloat(Format)) {
				if(Format <= cFormat::Rgba16f) {
					for(i = 0; i < ToChannels; i++) {
						((word *)To)[i] = cMath::Float2Half(Rgba[i]);
					}
				} else {
					for(i = 0; i < ToChannels; i++) {
						((float *)To)[i] = Rgba[i];
					}
				}
			} else if(Format >= cFormat::R16 && Format <= cFormat::Rgba16) {
				for(i = 0; i < ToChannels; i++) {
					((word *)To)[i] = (word)(65535 * cMath::Clamp01(Rgba[i]) + 0.5f);
				}
			} else {
				for(i = 0; i < ToChannels; i++) {
					To[i] = (byte)(255 * cMath::Clamp01(Rgba[i]) + 0.5f);
				}
			}

			Src += SrcSize;
			To += ToSize;
		} while(--NPixels);
	}

	delete[] m_Pixels;
	m_Pixels = Pixels;
	m_Format = Format;

	return true;
} // cImage::ToFormat

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//Rescale
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
template<typename TYPE>
void Rescale(TYPE *Src, TYPE *Dest, const int SrcW, const int SrcH, const int SrcD, const int DestW, const int DestH, const int DestD, const dword c, const bool Linear) {
	int z, dz, y, dy, x, dx, dzO, dyO, dxO;
	dword i;
	float sddx, sddy, sddz, sz, sy, sx;

	if(!Linear) {
		for(z = 0; z < DestD; z++) {
			dz = z * (SrcD - 1) / DestD;
			for(y = 0; y < DestH; y++) {
				dy = y * (SrcH - 1) / DestH;
				for(x = 0; x < DestW; x++) {
					dx = x * (SrcW - 1) / DestW;
					for(i = 0; i < c; i++) {
						*Dest++ = Src[((dz * SrcH + dy) * SrcW + dx) * c + i];
					}
				}
			}
		}
	} else {
		sddx = float(SrcW - 1) / cMath::Max(DestW - 1, 1);
		sddy = float(SrcH - 1) / cMath::Max(DestH - 1, 1);
		sddz = float(SrcD - 1) / cMath::Max(DestD - 1, 1);
		
		for(z = 0; z < DestD; z++) {
			sz = z * sddz;
			dz = (int)sz;
			sz -= dz;
			dzO = cMath::Min(dz + 1, SrcD - 1);
			for(y = 0; y < DestH; y++) {
				sy = y * sddy;
				dy = (int)sy;
				sy -= dy;
				dyO = cMath::Min(dy + 1, SrcH - 1);
				for(x = 0; x < DestW; x++) {
					sx = x * sddx;
					dx = (int)sx;
					sx -= dx;
					dxO = cMath::Min(dx + 1, SrcW - 1);
					for(i = 0; i < c; i++) {
						*Dest++ = (TYPE)(
							((Src[((dz  * SrcH + dy) * SrcW + dx) * c + i] * (1 - sx) +
							Src[((dz  * SrcH + dy ) * SrcW + dxO) * c + i] * (sx)) * (1 - sy) +
							(Src[((dz  * SrcH + dyO) * SrcW + dx ) * c + i] * (1 - sx) +
							Src[((dz  * SrcH + dyO) * SrcW + dxO) * c + i] * (sx)) * (sy)) * (1 - sz) +
							((Src[((dzO * SrcH + dy) * SrcW + dx) * c + i] * (1 - sx) +
							Src[((dzO * SrcH + dy ) * SrcW + dxO) * c + i] * (sx)) * (1 - sy) +
							(Src[((dzO * SrcH + dyO) * SrcW + dx) * c + i] * (1 - sx) +
							Src[((dzO * SrcH + dyO) * SrcW + dxO) * c + i] * (sx)) * (sy)) * (sz));
					}
				}
			}
		}
	}
} // Rescale

//-----------------------------------------------------------------------------
// cImage::Resize
//-----------------------------------------------------------------------------
bool cImage::Resize(const int Width, const int Height) {
	if(cFormat::IsCompressed(m_Format)) {
		Uncompress();
	}
	if(m_MipMapCount > 1) {
		RemoveMipMaps();
	}

	if(!cFormat::IsPlain(m_Format) || cDimension::Cube == GetDimension()) {
		return false;
	}
	
	int NChannels = cFormat::ChannelCount(m_Format);
	int ChSize = cFormat::BytesPerChannel(m_Format);
	byte *Pixels = new byte[Width * Height * m_Depth * NChannels * ChSize];
	
	if(1 == ChSize) {
		Rescale(m_Pixels, Pixels, m_Width, m_Height, m_Depth, Width, Height, m_Depth, NChannels, true);
	} else if(2 == ChSize) {
		Rescale((word *)m_Pixels, (word *)Pixels, m_Width, m_Height, m_Depth, Width, Height, m_Depth, NChannels, (m_Format >= cFormat::R16f) ? false : true);
	} else {
		Rescale((float *)m_Pixels, (float *)Pixels, m_Width, m_Height, m_Depth, Width, Height, m_Depth, NChannels, true);
	}

	delete[] m_Pixels;
	m_Pixels = Pixels;
	m_Width  = Width;
	m_Height = Height;
	
	return true;
} // cImage::Resize

bool cImage::Decrease2X() {

	if (!cFormat::IsPlain(m_Format) || cDimension::Cube == GetDimension()) {
		return false;
	}

	int NChannels = cFormat::ChannelCount(m_Format);
	int ChSize = cFormat::BytesPerChannel(m_Format);

	int w = m_Width;
	int h = m_Height;

	int sz2 = 0;
	int w2 = w / 2;
	int h2 = h / 2;
	for(int i=0;i<m_MipMapCount;i++) {
		if(w2 < 1 || h2 < 1)break;
		sz2 += w2 * h2;
		w2 /= 2;
		h2 /= 2;
	}

	byte* Pixels2 = new byte[sz2 * m_Depth * NChannels * ChSize];
	int pofs = 0;
	int cofs = 0;
	w2 = w / 2;
	h2 = h / 2;
	int nmip = 0;

	for (int m = 0; m < m_MipMapCount; m++) {
		if (w2 < 1 || h2 < 1)break;		
		if (1 == ChSize) {
			Rescale(m_Pixels + cofs, Pixels2 + pofs, w2 * 2, h2 * 2, m_Depth, w2, h2, m_Depth, NChannels, true);
		}
		else if (2 == ChSize) {
			Rescale((word*)m_Pixels + cofs, (word*)Pixels2 + pofs, w2 * 2, h2 * 2, m_Depth, w2, h2, m_Depth, NChannels, (m_Format >= cFormat::R16f) ? false : true);
		}
		else {
			Rescale((float*)m_Pixels + cofs, (float*)Pixels2 + pofs, w2 * 2, h2 * 2, m_Depth, w2, h2, m_Depth, NChannels, true);
		}
		int sz = w2 * h2 * m_Depth * NChannels;
		cofs += sz * 4;
		pofs += sz;
		nmip++;
		w2 /= 2;
		h2 /= 2;
	}
	m_MipMapCount = nmip;
	delete[](m_Pixels);
	m_Pixels = Pixels2;
	m_Width = w / 2;
	m_Height = h / 2;
	return true;
}

// cImage::MakePowerOfTwo
bool cImage::MakePowerOfTwo() {
	if(cMath::IsPowerOfTwo(m_Width) && cMath::IsPowerOfTwo(m_Height)) {
		return true;
	}

	int Width = cMath::ClosestPowerOfTwo(m_Width);
	int Height = cMath::ClosestPowerOfTwo(m_Height);
	return Resize(Width, Height);
}

// cImage::Make2D
bool cImage::Make2D(const bool Square, const int DepthStep) {
	if(GetDimension() != cDimension::ThreeD) {
		return false;
	}
	int FramesTotal = (DepthStep > 0) ? (m_Depth / DepthStep) : (m_Depth * -DepthStep);
	int Strides = 1;
	if(Square) {
		Strides = (int)cMath::Round(cMath::Sqrt((float)FramesTotal));
		if(cMath::Square(Strides) != FramesTotal) {
			cAssert(0 && "cImage::Make2D() | Depth should be power of two for square mode");
			return false;
		}
	}
	int FramesPerRow = FramesTotal / Strides;
	cAssert((1 == Strides) || (FramesPerRow == Strides));
	int SizeOfRow3D = GetRowSize();
	int SizeOfRow2D = SizeOfRow3D * FramesPerRow;
	int SizeOfSlice3D = GetSliceSize();
	int SizeOfStride = SizeOfSlice3D * FramesPerRow;
	byte *Pixels = new byte[SizeOfSlice3D * FramesTotal];
	
	byte *To = nullptr;
	const byte *Src = nullptr;
	int i, j, S, h;
	int c = SizeOfSlice3D / SizeOfRow3D;
	for(i = 0; i < FramesTotal; i++) {
		Src = m_Pixels + ((DepthStep > 0) ? (i * DepthStep) : (i / -DepthStep)) * SizeOfSlice3D;
		S = (int)(i / FramesPerRow);
		h = i % FramesPerRow;
		To = Pixels + S * SizeOfStride + h * SizeOfRow3D;
		for(j = 0; j < c; j++) {
			memcpy(To, Src, SizeOfRow3D);
			Src += SizeOfRow3D;
			To += SizeOfRow2D;
		}
	}
	m_Height *= Strides;
	m_Width *= FramesPerRow;
	m_Depth = 1;
	delete[] m_Pixels;
	m_Pixels = Pixels;
	return true;
}

// cImage::CreateSevenLods
void cImage::CreateSevenLods(const char *SaveAs) {
	const cImage::PixelRgb8 RainbowColors[7] = {
		{ 255, 0, 0 },		// Red (64 x 64)
		{ 255, 127, 0 },	// Orange (32 x 32)
		{ 255, 255, 0 },	// Yellow (16 x 16)
		{ 0, 255, 0 },		// Green (8 x 8)
		{ 0, 0, 255 },		// Blue (4 x 4)
		{ 75, 0, 130 },		// Indigo (2 x 2)
		{ 127, 0, 255 }		// Violet (1 x 1)
	};
	Create(cFormat::Rgb8, 64, 64, 1, 7);
	int M, N, i;
	cImage::PixelRgb8 *Ptr = (cImage::PixelRgb8 *)GetPixels();
	for(M = 0; M < 7; M++) {
		N = GetMipMappedSize(M, 1) / sizeof(RainbowColors[0]);
		for(i = 0; i < N; i++) {
			*Ptr = RainbowColors[M];
			Ptr++;
		}
	}
	Compress();
	if(SaveAs != nullptr) {
		cIO::SaveImage(SaveAs, *this);
	}
}

// cImage::MergeRgb8
void cImage::MergeRgb8(const cImage &From, const int X, const int Y) {
	cAssert(cFormat::Rgb8 == m_Format);
	cAssert(cFormat::Rgb8 == From.GetFormat());
	const int W = From.GetWidth();
	const int H = From.GetHeight();
	PixelRgb8 P;
	int x, y;
	for(x = 0; x < W; x++) {
		for(y = 0; y < H; y++) {
			P = From.GetPixelRgb8(x, y);
			SetPixelRgb8(X + x, Y + y, P);
		}
	}
}

comms::cBufInfo cImage::_py_buffer_info() {

	comms::cFormat::Enum fmt = GetFormat();
	int scalar_cnt = comms::cFormat::ChannelCount(fmt);
	bool plain = comms::cFormat::IsPlain(fmt);
	bool is_float = comms::cFormat::IsFloat(fmt);
	int bytes_per_cnl = comms::cFormat::BytesPerChannel(fmt);
	int bytes_per_pixel = comms::cFormat::BytesPerPixel(fmt);
	if (bytes_per_pixel <= 0) bytes_per_pixel = bytes_per_cnl;

	std::string py_format;
	if (is_float) py_format = "f";// float
	else if (bytes_per_cnl == 1) py_format = "B"; // unsigned char
	else if (bytes_per_cnl == 2) py_format = "H"; // WORD
	else "f";// float


	comms::cBufInfo buf;
	buf.ptr = GetPixels();


	buf.itemsize = bytes_per_cnl;
	buf.format = py_format;
	buf.ndim = 3;
	buf.shape = { GetHeight(), GetWidth(), scalar_cnt };
	buf.strides = { bytes_per_pixel * GetWidth(),
		  bytes_per_pixel, bytes_per_cnl };
	buf.readonly = false;

	buf.size = 1;
	for (size_t i = 0; i < (size_t)buf.ndim; ++i) {
		buf.size *= buf.shape[i];
	}


	return buf;

}


void cImage::MergeRgba8(const cImage& From, const int X, const int Y, bool ColorAsAlpha) {
	if (cFormat::Rgb8 == m_Format || cFormat::Rgba8 == m_Format) {
		const int W = From.GetWidth();
		const int H = From.GetHeight();
		PixelRgba8 Pa;
		PixelRgb8 P;
		PixelRgba8 P1;
		int x, y;
		for (x = 0; x < W; x++) {
			for (y = 0; y < H; y++) {
				if (ColorAsAlpha) {
					P = From.GetPixelRgb8(x, y);
					Pa.a = P.g;
					Pa.r = Pa.g = Pa.b = 255;
				}
				else {
					Pa = From.GetPixelRgba8(x, y);
				}
				float a = Pa.a / 255.0;
				if (cFormat::Rgba8 == m_Format) {
					P1 = GetPixelRgba8(X + x, Y + y);
					P1.r = P1.r * (1 - a) + Pa.r * a;
					P1.g = P1.g * (1 - a) + Pa.g * a;
					P1.b = P1.b * (1 - a) + Pa.b * a;
					SetPixelRgba8(X + x, Y + y, P1);
				}
				else {
					P = GetPixelRgb8(X + x, Y + y);
					P.r = P.r * (1 - a) + Pa.r * a;
					P.g = P.g * (1 - a) + Pa.g * a;
					P.b = P.b * (1 - a) + Pa.b * a;
					SetPixelRgb8(X + x, Y + y, P);
				}
			}
		}
	}
}

} // comms
