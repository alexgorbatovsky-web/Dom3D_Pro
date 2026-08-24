#pragma once

#ifdef COMMS_PNG

// Decodes / encodes: R8(16) (paletted), Rg8(16), Rgb8(16), Rgba8(16)

//*****************************************************************************
// cCodecPng
//*****************************************************************************
class cCodecPng : public cImageCodec {
public:
	cCodecPng() : cImageCodec() {}
	bool Decode(const cFile &Src, cImage *To);
	bool Encode(const cImage &Src, cFile *To);
	int CheckMagic(dword Magic, const char* ext);
};

#endif // COMMS_PNG
