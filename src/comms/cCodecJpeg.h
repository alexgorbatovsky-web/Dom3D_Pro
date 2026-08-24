#pragma once

#ifdef COMMS_JPEG

// Decodes: R8, Rgb8, Rgba8
// Encodes: R8, Rgb8

//*****************************************************************************
// cCodecJpeg
//*****************************************************************************
class cCodecJpeg : public cImageCodec {
public:
	static int Quality; // Ranges from 0 to 100. Default: 75
	
	cCodecJpeg() : cImageCodec() {}
	bool Decode(const cFile &Src, cImage *To);
	bool Encode(const cImage &Image, cFile *To);
	int CheckMagic(dword Magic, const char* ext);
};

#endif // COMMS_JPEG
