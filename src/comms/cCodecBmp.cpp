#include "comms.h"

namespace comms {

#ifdef COMMS_BIG_ENDIAN
static const char *BmpFileHeader_Format = "wd4bd";
static const char *BmpInfoHeader_Format = "3d2w2d8bd4b";
#endif // COMMS_BIG_ENDIAN

#pragma pack(1)

struct BmpFileHeader {
	word	Magic;
	dword	FileSize;
	byte	Junk[4]; // Reserved1 + Reserved2 = 2 + 2 = 4 bytes
	dword	OffsetToBits;
};

struct BmpInfoHeader {
	dword	Size;
	dword	Width;
	dword	Height;
	word	Planes;
	word	BitCount;
	dword	Compression;
	dword	SizeImage;
	byte	Junk0[8]; // XPelsPerMeter + YPelsPerMeter = 4 + 4 = 8 bytes
	dword	ClrUsed;
	byte	Junk1[4]; // ClrImportant = 4 bytes
};

struct RgbQuad {
	byte	Blue;
	byte	Green;
	byte	Red;
	byte	Reserved;
};

#pragma pack()

//-----------------------------------------------------------------------------
// cCodecBmp::Encode
//-----------------------------------------------------------------------------
bool cCodecBmp::Encode(const cImage &Image, cFile *To) {
	cAssert(To != nullptr);

	int Format = Image.GetFormat();
	if(Format != cFormat::R8 && Format != cFormat::Rgb8 && Format != cFormat::Rgba8) {
		return false;
	}

	BmpFileHeader FileHdr;
	BmpInfoHeader InfoHdr;

	memset(&FileHdr, 0, sizeof(FileHdr));
	memset(&InfoHdr, 0, sizeof(InfoHdr));

	FileHdr.Magic = 'B' | 'M' << 8;

	InfoHdr.Size = sizeof(InfoHdr);
	InfoHdr.Width = Image.GetWidth();
	InfoHdr.Height = Image.GetHeight();
	InfoHdr.Planes = 1;

	int i, BitmapWidthBytes, SrcWidthBytes, PaddBytes;
	RgbQuad Palette[256];
	cList<byte> Buffer;
	
	if(cFormat::R8 == Format) {
		//*********************************************************************
		// R8
		//*********************************************************************
		// Info header
		InfoHdr.BitCount = 8;
		InfoHdr.ClrUsed = 256;
		InfoHdr.SizeImage = InfoHdr.Width * InfoHdr.Height;

		// Gray scale palette
		memset(&Palette, 0, sizeof(Palette));
		for(i = 0; i < 256; i++) {
			RgbQuad &r = Palette[i];
			r.Red = r.Green = r.Blue = (byte)i;
		}

		// File header
		FileHdr.OffsetToBits = sizeof(FileHdr) + InfoHdr.Size + sizeof(Palette);
		FileHdr.FileSize = FileHdr.OffsetToBits + InfoHdr.SizeImage;
#ifdef COMMS_BIG_ENDIAN
		cMath::EndianSwap(&FileHdr, BmpFileHeader_Format);
		cMath::EndianSwap(&InfoHdr, BmpInfoHeader_Format);
#endif // COMMS_BIG_ENDIAN
		To->WriteBytes(&FileHdr, sizeof(FileHdr));
		To->WriteBytes(&InfoHdr, sizeof(InfoHdr));
		To->WriteBytes(Palette, sizeof(Palette));
		To->WriteBytes(Image.GetPixels(), InfoHdr.SizeImage);
	} else if(cFormat::Rgb8 == Format) {
		//*********************************************************************
		// Rgb8
		//*********************************************************************
		// Info header
		InfoHdr.BitCount = 24;
		BitmapWidthBytes = cMath::AlignToDword(3 * InfoHdr.Width);
		InfoHdr.SizeImage = BitmapWidthBytes * InfoHdr.Height;

		// File header
		FileHdr.OffsetToBits = sizeof(FileHdr) + InfoHdr.Size;
		FileHdr.FileSize = FileHdr.OffsetToBits + InfoHdr.SizeImage;
#ifdef COMMS_BIG_ENDIAN
		cMath::EndianSwap(&FileHdr, BmpFileHeader_Format);
		cMath::EndianSwap(&InfoHdr, BmpInfoHeader_Format);
#endif // COMMS_BIG_ENDIAN
		To->WriteBytes(&FileHdr, sizeof(FileHdr));
		To->WriteBytes(&InfoHdr, sizeof(InfoHdr));

		SrcWidthBytes = 3 * Image.GetWidth();
		PaddBytes = BitmapWidthBytes - SrcWidthBytes;
		for(i = 0; i < Image.GetHeight(); i++) {
            Buffer.Copy( Image.GetPixels() + i * SrcWidthBytes, SrcWidthBytes );
			cMath::SwapChannels((byte *)Buffer.ToPtr(), Image.GetWidth(), 3, 0, 2);
			Buffer.Add(0, PaddBytes);
			To->WriteBytes(Buffer.ToPtr(), Buffer.Count());
		}
	} else if(cFormat::Rgba8 == Format) {
		//*********************************************************************
		// Rgba8
		//*********************************************************************
		// Info header
		InfoHdr.BitCount = 32;
		BitmapWidthBytes = 4 * InfoHdr.Width;
		InfoHdr.SizeImage = BitmapWidthBytes * InfoHdr.Height;

		// File header
		FileHdr.OffsetToBits = sizeof(FileHdr) + InfoHdr.Size;
		FileHdr.FileSize = FileHdr.OffsetToBits + InfoHdr.SizeImage;
#ifdef COMMS_BIG_ENDIAN
		cMath::EndianSwap(&FileHdr, BmpFileHeader_Format);
		cMath::EndianSwap(&InfoHdr, BmpInfoHeader_Format);
#endif // COMMS_BIG_ENDIAN
		To->WriteBytes(&FileHdr, sizeof(FileHdr));
		To->WriteBytes(&InfoHdr, sizeof(InfoHdr));
		
        Buffer.Copy( Image.GetPixels(), BitmapWidthBytes * Image.GetHeight() );
		cMath::SwapChannels((byte *)Buffer.ToPtr(), Buffer.Count() / 4, 4, 0, 2);
		To->WriteBytes(Buffer.ToPtr(), Buffer.Count());
	}
	
	return true;
} // cCodecBmp::Encode

//-----------------------------------------------------------------------------
// cCodecBmp::Decode
//-----------------------------------------------------------------------------
bool cCodecBmp::Decode(const cFile &Src, cImage *To) {
	cAssert(To != nullptr);
	
	BmpFileHeader FileHdr;
	if(Src.ReadBytes(&FileHdr, sizeof(BmpFileHeader)) != sizeof(BmpFileHeader)) {
		return false;
	}

	BmpInfoHeader InfoHdr;
	if(Src.ReadBytes(&InfoHdr, sizeof(BmpInfoHeader)) != sizeof(BmpInfoHeader)) {
		return false;
	}

#ifdef COMMS_BIG_ENDIAN
		cMath::EndianSwap(&FileHdr, BmpFileHeader_Format);
		cMath::EndianSwap(&InfoHdr, BmpInfoHeader_Format);
#endif // COMMS_BIG_ENDIAN
	
	bool Success = FileHdr.Magic == ('B' | 'M' << 8) && InfoHdr.Size == sizeof(BmpInfoHeader) && InfoHdr.Planes == 1 &&
		(1 == InfoHdr.BitCount || 4 == InfoHdr.BitCount || 8 == InfoHdr.BitCount || 24 == InfoHdr.BitCount || 32 == InfoHdr.BitCount) &&
		(0 == InfoHdr.Compression || 3 == InfoHdr.Compression);

	if(!Success) {
		return false;
	}

	// Reading palette in paletted bitmaps:
	cList<byte> Palette;
	int NColors = 0;
	if(InfoHdr.BitCount <= 8) {
		if(InfoHdr.ClrUsed > 0) {
			NColors = InfoHdr.ClrUsed;
		} else {
			NColors = 1 << InfoHdr.BitCount;
		}
		Palette.SetCount(4 * NColors);
		if(Src.ReadBytes(Palette.ToPtr(), Palette.Count()) != Palette.Count()) {
			return false;
		}
		cMath::SwapChannels(Palette.ToPtr(), NColors, 4, 0, 2);
	}

	// Selecting format:
	const cFormat::Enum Format = 32 == InfoHdr.BitCount ? cFormat::Rgba8 : cFormat::Rgb8;
	int h = InfoHdr.Height;
	bool Flip = false;
	if(h < 0) {
		InfoHdr.Height = -h;
		Flip = true;
	}
	To->Create(Format, InfoHdr.Width, InfoHdr.Height, 1, 1);

	switch(InfoHdr.BitCount) {
		case 1: // 2 colors (paletted mono)
			Success = LoadPaletted1(Src, To, Palette);
			break;
		case 4: // 16 colors (paletted)
			Success = LoadPaletted4(Src, To, Palette);
			break;
		case 8: // 256 colors (paletted)
			Success = LoadPaletted8(Src, To, Palette);
			break;
		case 24: // True color
			Success = LoadTrueColor24(Src, To);
			break;
		case 32: // True color with alpha - channel
			Success = LoadTrueColor32(Src, To);
			break;
		default:
			Success = false;
	}
	if(Success && Flip) {
		To->Flip();
	}
	return Success;
} // cCodecBmp::Decode

//------------------------------------------------------------------------------------------
// cCodecBmp::LoadPaletted1
//------------------------------------------------------------------------------------------
bool cCodecBmp::LoadPaletted1(const cFile &Src, cImage *Image, const cList<byte> &Palette) {
	int BytesPerLine, Index, h, x;
	cList<byte> Buffer;
	byte *Line;
	const byte *Color;

	BytesPerLine = (Image->GetWidth() + 7) / 8;
	
	// Alignment to "dword":
	BytesPerLine = cMath::AlignToDword(BytesPerLine);
	
	Buffer.SetCount(BytesPerLine);
	
	for(h = 0; h < Image->GetHeight(); h++) {
		if(Src.ReadBytes(Buffer.ToPtr(), BytesPerLine) != BytesPerLine) {
			return false;
		}

		Line = Image->GetPixels() + h * Image->GetWidth() * 3;
		for(x = 0; x < Image->GetWidth(); x++) {
			if(Buffer[x >> 3] & (0x80 >> (x & 7))) {
				Index = 4;
			} else {
				Index = 0;
			}
			if(Index >= Palette.Count()) {
				return false;
			}
			Color = &Palette[Index];
			*Line++ = *Color++;
			*Line++ = *Color++;
			*Line++ = *Color++;
		}
	}
	
	return true;
} // cCodecBmp::LoadPaletted1

//------------------------------------------------------------------------------------------
// cCodecBmp::LoadPaletted4
//------------------------------------------------------------------------------------------
bool cCodecBmp::LoadPaletted4(const cFile &Src, cImage *Image, const cList<byte> &Palette) {
	int BytesPerLine, Index, h, x;
	cList<byte> Buffer;
	byte *Line;
	const byte *Color;

	BytesPerLine = (Image->GetWidth() + 1) / 2;
	
	// Alignment to "dword":
	BytesPerLine = cMath::AlignToDword(BytesPerLine);

	Buffer.SetCount(BytesPerLine);

	for(h = 0; h < Image->GetHeight(); h++) {
		if(Src.ReadBytes(Buffer.ToPtr(), BytesPerLine) != BytesPerLine) {
			return false;
		}

		Line = Image->GetPixels() + h * Image->GetWidth() * 3;
		for(x = 0; x < Image->GetWidth(); x++) {
			if((x & 1) == 0) {
				Index = (Buffer[x >> 1] >> 4) << 2;
			} else {
				Index = (Buffer[x >> 1] & 0x0f) << 2;
			}
			if(Index >= Palette.Count()) {
				return false;
			}
			Color = &Palette[Index];
			*Line++ = *Color++;
			*Line++ = *Color++;
			*Line++ = *Color++;
		}
	}

	return true;
} // cCodecBmp::LoadPaletted4

//------------------------------------------------------------------------------------------
// cCodecBmp::LoadPaletted8
//------------------------------------------------------------------------------------------
bool cCodecBmp::LoadPaletted8(const cFile &Src, cImage *Image, const cList<byte> &Palette) {
	int BytesPerLine, h, x, Index;
	cList<byte> Buffer;
	byte *Line;
	const byte *Color;

	BytesPerLine = Image->GetWidth();
	
	// Alignment to "dword":
	BytesPerLine = cMath::AlignToDword(BytesPerLine);

	Buffer.SetCount(BytesPerLine);
	
	for(h = 0; h < Image->GetHeight(); h++) {
		if(Src.ReadBytes(Buffer.ToPtr(), BytesPerLine) != BytesPerLine) {
			return false;
		}

		Line = Image->GetPixels() + h * Image->GetWidth() * 3;
		for(x = 0; x < Image->GetWidth(); x++) {
			Index = Buffer[x] << 2;
			if(Index >= Palette.Count()) {
				return false;
			}
			Color = &Palette[Index];
			*Line++ = *Color++;
			*Line++ = *Color++;
			*Line++ = *Color++;
		}
	}

	return true;
} // cCodecBmp::LoadPaletted8

//-----------------------------------------------------------------------------
// cCodecBmp::LoadTrueColor24
//-----------------------------------------------------------------------------
bool cCodecBmp::LoadTrueColor24(const cFile &Src, cImage *Image) {
	int LineSize, h, r;
	byte *Line, b;

	LineSize = Image->GetWidth() * 3;
	for(h = 0; h < Image->GetHeight(); h++) {
		Line = Image->GetPixels() + h * LineSize;
		if(Src.ReadBytes(Line, LineSize) != LineSize) {
			return false;
		}
		cMath::SwapChannels(Line, Image->GetWidth(), 3, 0, 2);

		// Alignment to "dword":
		for(r = LineSize; r % 4; r++) {
			if(!Src.ReadByte(&b)) {
				return false;
			}
		}
	}
	return true;
} // cCodecBmp::LoadTrueColor24

//-----------------------------------------------------------------------------
// cCodecBmp::LoadTrueColor32
//-----------------------------------------------------------------------------
bool cCodecBmp::LoadTrueColor32(const cFile &Src, cImage *Image) {
	int LineSize, h;
	byte *Line;

	LineSize = Image->GetWidth() * 4;
	for(h = 0; h < Image->GetHeight(); h++) {
		Line = Image->GetPixels() + h * LineSize;
		if(Src.ReadBytes(Line, LineSize) != LineSize) {
			return false;
		}
		cMath::SwapChannels(Line, Image->GetWidth(), 4, 0, 2);
	}
	return true;
} // cCodecBmp::LoadTrueColor32
int cCodecBmp::CheckMagic(dword Magic, const char* ext){
	char* s = (char*)&Magic;
	return (s[0] == 'B' && s[1] == 'M') ? 1 : 0;
}

} // comms
