#pragma once
#ifndef __cCodec_Bin_H_
#define __cCodec_Bin_H_

// Decodes: Unsupported.
// Encodes: R16f, R32f

//*****************************************************************************
// cCodecBin
//*****************************************************************************
class cCodecBin : public cImageCodec {
public:
	cCodecBin() : cImageCodec() {}
	bool Decode(const cFile& Src, cImage* To);
	bool Encode(const cImage& Image, cFile* To);
	int CheckMagic(dword Magic, const char* ext);
};
#endif
