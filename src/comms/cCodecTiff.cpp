#include "comms.h"

#ifdef COMMS_TIFF

#include "tiffio.h"


namespace comms {

#ifdef COMMS_WINDOWS
#pragma comment (lib, "libtiff.lib")
#endif // COMMS_WINDOWS

static cStr cCodecTiff_Warnings;

static void cCodecTiff_WarningHandler(const char *Tag, const char *Format, va_list Args) {
	cCodecTiff_Warnings += cStr::Formatv(Format, Args) + cStr::EndLn;
}

static tsize_t cCodecTiff_Read(thandle_t th, tdata_t buf, tsize_t size) {
	const cFile *F = (const cFile *)th;
	return F->ReadBytes(buf, (int)size);
}

static tsize_t cCodecTiff_Write(thandle_t th, tdata_t buffer, tsize_t size) {
	cFile *F = (cFile *)th;
	if(F->IsEof()) {
		F->WriteBytes(buffer, (int)size);
	} else {
		F->ReplaceBytes(buffer, (int)size);
	}
	return size;
}

static int cCodecTiff_Close(thandle_t) {
    return 0;
}

static toff_t cCodecTiff_Seek(thandle_t th, toff_t pos, int whence) {
	const cFile *F = (const cFile *)th;
	if(SEEK_SET == whence) {
		if(pos > F->Size()) {
			return ((cFile *)th)->SetPosMutable((int)pos);
		}
		return F->SetPos((int)pos);
	} else if(SEEK_END == whence) {
		return F->SeekEnd((int)pos);
	} else {
		cAssert(0);
	}
	return 0;
}

static toff_t cCodecTiff_Size(thandle_t th) {
	return 0;
}

static int cCodecTiff_Map(thandle_t, tdata_t *, toff_t *) {
	return 0;
}

static void cCodecTiff_Unmap(thandle_t, tdata_t, toff_t) {
}

//-----------------------------------------------------------------------------
// cCodecTiff::Decode
//-----------------------------------------------------------------------------
bool cCodecTiff::Decode(const cFile &Src, cImage *To) {
	cAssert(To != nullptr);
	To->Free();
	
	To->m_Format = cFormat::Rgba8;
	To->m_Width = 0;
	To->m_Height = 0;
	To->m_Depth = 1;
	To->m_MipMapCount = 1;

	bool Success = false;
	dword W, H;
	word Type, NChannels;
	size_t RowSize, TotalSize;
	dword *Raster = nullptr;
	
	TIFFSetWarningHandler(cCodecTiff_WarningHandler);
	TIFFSetErrorHandler(cCodecTiff_WarningHandler);
	cCodecTiff_Warnings.Clear();
	TIFF *Tif = TIFFClientOpen("cCodecTiff", "r", (thandle_t)&Src, cCodecTiff_Read, cCodecTiff_Write, cCodecTiff_Seek, cCodecTiff_Close, cCodecTiff_Size, cCodecTiff_Map, cCodecTiff_Unmap);
	if(Tif) {
		TIFFGetField(Tif, TIFFTAG_IMAGEWIDTH, &W);
		TIFFGetField(Tif, TIFFTAG_IMAGELENGTH, &H);
		TIFFGetField(Tif, TIFFTAG_SAMPLESPERPIXEL, &NChannels);
		TIFFGetField(Tif, TIFFTAG_PHOTOMETRIC, &Type);
		bool Depth = (1 == NChannels) && (Type != PHOTOMETRIC_PALETTE);
		bool RemoveAlpha = (NChannels < 4) && !Depth;
		To->m_Width = W;
		To->m_Height = H;
		RowSize = To->m_Width * sizeof(dword);
		TotalSize = RowSize * To->m_Height;
		Raster = (dword *)_TIFFmalloc(TotalSize);
		if(Raster != nullptr) {
			if(TIFFReadRGBAImage(Tif, W, H, Raster, 0)) {
				To->m_Pixels = new byte[TotalSize];
				memcpy(To->m_Pixels, Raster, TotalSize);
				if(Depth) {
					To->RemoveChannels(true, false, false, false);
				}
				if(RemoveAlpha) {
					To->RemoveChannels(true, true, true, false);
				}
				Success = true;
			}
			_TIFFfree(Raster);
			Raster = nullptr;
		}
		TIFFClose(Tif);
	}
	if(!cCodecTiff_Warnings.IsEmpty()) {
		cLog::Message(cCodecTiff_Warnings);
	}
	return Success;
} // cCodecTiff::Decode

//-----------------------------------------------------------------------------
// cCodecTiff::Encode
//-----------------------------------------------------------------------------
bool cCodecTiff::Encode(const cImage &Image, cFile *To) {
	cAssert(To != nullptr);
	cFormat::Enum Format = Image.GetFormat();
	if(Format != cFormat::R8 && Format != cFormat::Rgb8 && Format != cFormat::Rgba8) {
		return false;
	}
	bool Success = false;
	tmsize_t LineSize = 0;
	size_t RowSize = 0;
	byte *Temp = nullptr;
	int i;
	int NChannels = cFormat::ChannelCount(Format);
	TIFFSetWarningHandler(cCodecTiff_WarningHandler);
	TIFFSetErrorHandler(cCodecTiff_WarningHandler);
	cCodecTiff_Warnings.Clear();
	TIFF *Tif = TIFFClientOpen("cCodecTiff", "w", (thandle_t)To, cCodecTiff_Read, cCodecTiff_Write, cCodecTiff_Seek, cCodecTiff_Close, cCodecTiff_Size, cCodecTiff_Map, cCodecTiff_Unmap);
	if(Tif) {
		TIFFSetField(Tif, TIFFTAG_IMAGEWIDTH, Image.GetWidth());
		TIFFSetField(Tif, TIFFTAG_IMAGELENGTH, Image.GetHeight());
		TIFFSetField(Tif, TIFFTAG_SAMPLESPERPIXEL, NChannels);
		TIFFSetField(Tif, TIFFTAG_BITSPERSAMPLE, 8);
		TIFFSetField(Tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
		TIFFSetField(Tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		TIFFSetField(Tif, TIFFTAG_PHOTOMETRIC, (1 == NChannels) ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB);
		RowSize = Image.GetWidth() * NChannels;
		LineSize = TIFFScanlineSize(Tif);
		TIFFSetField(Tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(Tif, (uint32)RowSize));
		Temp = (byte *)_TIFFmalloc(LineSize);
		if(Temp) {
			for(i = 0; i < Image.GetHeight(); i++) {
				memcpy(Temp, Image.GetPixels() + (Image.GetHeight() - i - 1) * RowSize, RowSize);
				if(TIFFWriteScanline(Tif, Temp, i, 0) < 0) {
					break;
				}
			}
		}
		TIFFClose(Tif);
		if(Temp) {
			_TIFFfree(Temp);
			Temp = nullptr;
		}
		Success = true;
	}
	if(!cCodecTiff_Warnings.IsEmpty()) {
		cMessageBox::Ok(To->GetFilePn().GetFileName(), cCodecTiff_Warnings);
	}
	return Success;
} // cCodecTiff::Encode
int cCodecTiff::CheckMagic(dword Magic, const char* ext){
	char* s = (char*)&Magic;
	return (s[0] == 'I' && s[1] == 'I') ? 1:0;
}

} // comms

#endif // COMMS_TIFF
