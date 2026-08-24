#pragma once

#ifdef COMMS_TIFF

//*****************************************************************************
// cCodecTiff
//*****************************************************************************
class cCodecTiff : public cIO::ImageCodec {
public:
	cCodecTiff() : cIO::ImageCodec() {}
	bool Decode(const cFile &Src, cImage *To);
	bool Encode(const cImage &Image, cFile *To);
	int CheckMagic(dword Magic, const char* ext);
};

#endif // COMMS_TIFF
