#include "comms.h"

namespace comms {

#ifdef COMMS_BIG_ENDIAN
static const char *TgaHeader_Format = "3b2wb4w2b";
#endif // COMMS_BIG_ENDIAN

#pragma pack(1)

struct TgaHeader {
	byte	DescrLength;
	byte	ColormapType;
	byte	ImageType;
	word	ColormapStart;
	word	ColormapLength;
	byte	ColormapEntrySize;
	word	XOrigin;
	word	YOrigin;
	word	Width;
	word	Height;
	byte	PixelSize;
	byte	ImageDesc;
};

#pragma pack()

//-----------------------------------------------------------------------------
// cCodecTga::Decode
//-----------------------------------------------------------------------------
bool cCodecTga::Decode(const cFile &Src, cImage *To) {
	cAssert(To != nullptr);
	To->Free();

	if(Src.Size()>2){
		dword D=Src.ReadWord();
		Src.SeekCur(-2);
		if(D!=0){
			return false;
		}
	}

	TgaHeader Hdr;
	if(Src.ReadBytes(&Hdr, sizeof(Hdr)) != sizeof(Hdr)) {
		return false;
	}
#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap(&Hdr, TgaHeader_Format);
#endif // COMMS_BIG_ENDIAN
	
	To->m_Width = Hdr.Width;
	To->m_Height = Hdr.Height;
	To->m_Depth = 1;
	To->m_MipMapCount = 1;
	int PixelSize = Hdr.PixelSize / 8;

	byte Palette[768+256];
	int PalLength = Hdr.DescrLength + Hdr.ColormapLength * Hdr.ColormapEntrySize / 8;
	if(PalLength > 0) {
		if(Src.ReadBytes(Palette, PalLength) != PalLength) {
			return false;
		}
	}

	cList<byte> F;
	int FSize = (int)(Src.Size() - sizeof(Hdr)) - PalLength;
	F.SetCount(FSize);	
	if(Src.ReadBytes(F.ToPtr(), FSize) != FSize) {
		return false;
	}

	int ImageSize = To->m_Width * To->m_Height * PixelSize;
	cList<byte> T;
	T.SetCount(ImageSize);

	byte *Ptr0, *Ptr1;
	dword c, Count;

	if(Hdr.ImageType & 0x08) {
		Ptr0 = F.ToPtr();
		Ptr1 = T.ToPtr();

		while(ImageSize > 0) {
			c = *Ptr0++;
			Count = (c & 0x7f) + 1;
			ImageSize -= Count * PixelSize;

			if(c & 0x80) {
				// Rle
				do {
					memcpy(Ptr1, Ptr0, PixelSize);
					Ptr1 += PixelSize;
				} while(--Count);
				Ptr0 += PixelSize;
			} else {
				// Raw
				Count *= PixelSize;
				memcpy(Ptr1, Ptr0, Count);
				Ptr0 += Count;
				Ptr1 += Count;
			}
		}
		Ptr0 = T.ToPtr();
	} else {
		Ptr0 = F.ToPtr();
	}

	bool VertInvert = (Hdr.ImageDesc & 0x20) != 0;
	if(VertInvert) {
		Ptr0 += PixelSize * To->m_Width * (To->m_Height - 1);
	}
	
	int X, Y;
	dword P;
	switch(Hdr.PixelSize) {
		case 8:
			if(PalLength > 0) {
				To->m_Format = cFormat::Rgb8;
				Ptr1 = To->m_Pixels = new byte[To->m_Width * To->m_Height * 3];
				for(Y = 0; Y < To->m_Height; Y++) {
					for(X = 0; X < To->m_Width; X++) {
						P = 3 * (*Ptr0++);
						*Ptr1++ = Palette[P + 2];
						*Ptr1++ = Palette[P + 1];
						*Ptr1++ = Palette[P];
					}
					if(VertInvert) {
						Ptr0 -= 2 * To->m_Width;
					}
				}
			} else {
				To->m_Format = cFormat::R8;
				Ptr1 = To->m_Pixels = new byte[To->m_Width * To->m_Height];
				for(Y = 0; Y < To->m_Height; Y++) {
					memcpy(Ptr1, Ptr0, To->m_Width);
					Ptr1 += To->m_Width;
					if(VertInvert) {
						Ptr0 -= To->m_Width;
					} else {
						Ptr0 += To->m_Width;
					}
				}
			}
			break;
		case 16:
			To->m_Format = cFormat::Rgba8;
			Ptr1 = To->m_Pixels = new byte[To->m_Width * To->m_Height * 4];
			for(Y = 0; Y < To->m_Height; Y++) {
				for(X = 0; X < To->m_Width; X++) {
					P = *((word *)Ptr0);
					Ptr1[0] = (byte)(((P >> 10) & 0x1f) << 3);
					Ptr1[1] = (byte)(((P >> 5) & 0x1f) << 3);
					Ptr1[2] = (byte)((P & 0x1f) << 3);
					Ptr1[3] = (byte)(((P >> 15) ? 0xff : 0));
					Ptr1 += 4;
					Ptr0 += 2;
				}
				if(VertInvert) {
					Ptr0 -= 4 * To->m_Width;
				}
			}
			break;
		case 24:
			To->m_Format = cFormat::Rgb8;
			Ptr1 = To->m_Pixels = new byte[To->m_Width * To->m_Height * 3];
			for(Y = 0; Y < To->m_Height; Y++) {
				for(X = 0; X < To->m_Width; X++) {
					*Ptr1++ = Ptr0[2];
					*Ptr1++ = Ptr0[1];
					*Ptr1++ = Ptr0[0];
					Ptr0 += 3;
				}
				if(VertInvert) {
					Ptr0 -= 6 * To->m_Width;
				}
			}
			break;
		case 32:
			To->m_Format = cFormat::Rgba8;
			Ptr1 = To->m_Pixels = new byte[To->m_Width * To->m_Height * 4];
			for(Y = 0; Y < To->m_Height; Y++) {
				for(X = 0; X < To->m_Width; X++) {
					*Ptr1++ = Ptr0[2];
					*Ptr1++ = Ptr0[1];
					*Ptr1++ = Ptr0[0];
					*Ptr1++ = Ptr0[3];
					Ptr0 += 4;
				}
				if(VertInvert) {
					Ptr0 -= 8 * To->m_Width;
				}
			}
			break;
	}

	return true;
} // cCodecTga::Decode

//-----------------------------------------------------------------------------
// cCodecTga::Encode
//-----------------------------------------------------------------------------
bool cCodecTga::Encode(const cImage &Src, cFile *To) {
	cAssert(To != nullptr);

	cFormat::Enum Format = Src.GetFormat();
	if(Format != cFormat::R8 && Format != cFormat::Rgb8 && Format != cFormat::Rgba8) {
		return false;
	}

	int NChannels = cFormat::ChannelCount(Format);

	TgaHeader Hdr;
	Hdr.DescrLength = 0;
	Hdr.ColormapType = cFormat::R8 == Format ? 1 : 0;
	Hdr.ImageType = cFormat::R8 == Format ? 1 : 2;
	Hdr.ColormapStart = 0;
	Hdr.ColormapLength = cFormat::R8 == Format ? 256 : 0;
	Hdr.ColormapEntrySize = cFormat::R8 == Format ? 24 : 0;
	Hdr.XOrigin = 0;
	Hdr.YOrigin = 0;
	Hdr.Width = Src.GetWidth();
	Hdr.Height = Src.GetHeight();
	Hdr.PixelSize = 8 * NChannels;
	Hdr.ImageDesc = 0;

#ifdef COMMS_BIG_ENDIAN
	cMath::EndianSwap(&Hdr, TgaHeader_Format);
#endif // COMMS_BIG_ENDIAN
	To->WriteBytes(&Hdr, sizeof(Hdr));
	
	byte Palette[768];
	int i, p;
	cList<byte> Buffer;

	if(cFormat::R8 == Format) { // R8
		p = 0;
		for(i = 0; i < 256; i++) {
			Palette[p++] = i;
			Palette[p++] = i;
			Palette[p++] = i;
		}
		To->WriteBytes(Palette, sizeof(Palette));
		To->WriteBytes(Src.GetPixels(), Src.GetWidth() * Src.GetHeight());
	} else { // Rgb8, Rgba8
        Buffer.Copy( Src.GetPixels(), Src.GetWidth() * Src.GetHeight() * NChannels );
		cMath::SwapChannels(Buffer.ToPtr(), Buffer.Count() / NChannels, NChannels, 0, 2);
		To->WriteBytes(Buffer.ToPtr(), Buffer.Count());
	}
	return true;
} // cCodecTga::Encode
int cCodecTga::CheckMagic(dword Magic, const char* ext){
	byte* s = (byte*)&Magic;
	///in this case it is TGA for sure
	if (s[0] == 0 && s[1] == 0 && s[2] == 2 && s[3] == 0) return 1;
	return -1;//no magic in tga
}
} // comms
