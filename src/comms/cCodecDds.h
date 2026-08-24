#pragma once

//*****************************************************************************
// cCodecDds
//*****************************************************************************
class cCodecDds : public comms::cImageCodec {
public:
	cCodecDds() : comms::cImageCodec() {}
	bool Decode(const cFile &Src, cImage *To);
	bool Encode(const cImage &Image, cFile *To);
	int CheckMagic(dword Magic, const char* ext);
};
