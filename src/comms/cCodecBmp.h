#pragma once

// Decodes: 256 colors (paletted), true color, and true color w/ alpha.
// Encodes: R8, Rgb8, Rgba8

//*****************************************************************************
// cCodecBmp
//*****************************************************************************
class cCodecBmp : public cImageCodec {
public:
	cCodecBmp() : cImageCodec() {}
	bool Decode(const cFile &Src, cImage *To);
	bool Encode(const cImage &Image, cFile *To);
	int CheckMagic(dword Magic, const char* ext);
private:
	bool LoadPaletted1(const cFile &Src, cImage *Image, const cList<byte> &Palette);
	bool LoadPaletted4(const cFile &Src, cImage *Image, const cList<byte> &Palette);
	bool LoadPaletted8(const cFile &Src, cImage *Image, const cList<byte> &Palette);
	bool LoadTrueColor24(const cFile &Src, cImage *Image);
	bool LoadTrueColor32(const cFile &Src, cImage *Image);
};
