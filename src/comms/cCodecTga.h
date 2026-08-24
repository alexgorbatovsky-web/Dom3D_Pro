#pragma once

// Decodes: 8 (paletted), 16, 24, and 32 bpp images. Supports RLE.
// Encodes: R8, Rgb8, Rgba8

//*****************************************************************************
// cCodecTga
//*****************************************************************************
class cCodecTga : public cImageCodec {
public:
	cCodecTga() : cImageCodec() {}
	bool Decode(const cFile &Src, cImage *To);
	bool Encode(const cImage &Image, cFile *To);
	int CheckMagic(dword Magic, const char* ext);
};
