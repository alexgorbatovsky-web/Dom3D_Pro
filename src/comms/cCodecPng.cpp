#include "comms.h"

// When you place this include inside namespace below you will get error under Linux:
// error : conflicting types for '__sigsetjmp'
#include "png.h"

namespace comms {

#ifdef COMMS_PNG

#pragma comment (lib, "libpng16.lib")

static const cFile *LoadFile = nullptr;
static cFile *SaveFile = nullptr;

static void user_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	SaveFile->WriteBytes(data, (int)length);
}

static void user_flush_data(png_structp png_ptr) {
}

static void user_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	LoadFile->ReadBytes(data, (int)length);
}

static png_voidp malloc_fn(png_structp png_ptr, png_size_t size) {
	return malloc(size);
}

static void free_fn(png_structp png_ptr, png_voidp ptr) {
	free(ptr);
}

//-----------------------------------------------------------------------------
// cCodecPng::Decode
//-----------------------------------------------------------------------------
bool cCodecPng::Decode(const cFile &Src, cImage *To) {
	cAssert(To != nullptr);
	To->Free();

	if(Src.Size()>4){
		dword D=Src.ReadDword();
		Src.SeekCur(-4);
		D&=0xFFFFFF00;
		if(D!=0x474E5000){
			return false;
		}
	}

	png_structp png_ptr = nullptr;
	png_infop info_ptr = nullptr;

	png_byte pbSig[8];
	Src.ReadBytes(pbSig, 8);
	if(!png_check_sig(pbSig, 8)) {
		return false;
	}

	png_ptr = png_create_read_struct_2(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr, nullptr, malloc_fn, free_fn);
	if(nullptr == png_ptr) {
		return false;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if(nullptr == info_ptr) {
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
		return false;
	}

	LoadFile = &Src;
	png_set_read_fn(png_ptr, nullptr, user_read_data);
	png_set_sig_bytes(png_ptr, 8);

	png_read_info(png_ptr, info_ptr);

	png_uint_32 width, height;
	int bit_depth, color_type;
	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, nullptr, nullptr, nullptr);

	To->m_Width = (int)width;
	To->m_Height = (int)height;
	To->m_Depth = 1;
	To->m_MipMapCount = 1;

	int NChannels = png_get_channels(png_ptr, info_ptr);
	switch(NChannels) {
		case 1:
			To->m_Format = bit_depth > 8 ? cFormat::R16 : cFormat::R8;
			break;
		case 2:
			To->m_Format = bit_depth > 8 ? cFormat::Rg16 : cFormat::Rg8;
			break;
		case 3:
			To->m_Format = bit_depth > 8 ? cFormat::Rgb16 : cFormat::Rgb8;
			break;
		case 4:
			To->m_Format = bit_depth > 8 ? cFormat::Rgba16 : cFormat::Rgba8;
			break;
	}

	int RowSize = To->m_Width * NChannels * bit_depth / 8;

	To->m_Pixels = new byte[RowSize * To->m_Height];

	png_byte **RowPointers = new png_bytep[To->m_Height];
	int i;
	for(i = 0; i < To->m_Height; i++) {
		RowPointers[To->m_Height - i - 1] = To->m_Pixels + i * RowSize;
	}

	png_read_image(png_ptr, RowPointers);
	png_read_end(png_ptr, nullptr);

	delete[] RowPointers;

	png_colorp palette = nullptr;
	int num_palette;
	byte *Buffer;

	if(PNG_COLOR_TYPE_PALETTE == color_type) {
		if (bit_depth < 4) {
			//png_set_palette_to_rgb(png_ptr);
		}
		png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);
		png_bytep trans_alpha = nullptr;
		int num_trans = 0;
		int r1 = png_get_tRNS(png_ptr, info_ptr, &trans_alpha, &num_trans,nullptr);
		int numc = 3;
		if (r1 && num_trans)numc = 4;
		
		Buffer = new byte[To->m_Width * To->m_Height * numc];
		if (8 > bit_depth) {
			int ns = 8 / bit_depth - 1;
			for (i = 0; i < RowSize * To->m_Height; i++) {
				byte b = To->m_Pixels[i];				
				for(int k=0;k <= ns;k++) {
					int p = ((b >> ((ns - k) * bit_depth)) & ns);
					int ofs = i * (ns + 1) * numc + k * numc;
					Buffer[ofs    ] = palette[p].red;
					Buffer[ofs + 1] = palette[p].green;
					Buffer[ofs + 2] = palette[p].blue;
					if (numc == 4)Buffer[ofs + 3] = trans_alpha[p];
				}
			}
		} else {
			for(i = 0; i < RowSize * To->m_Height; i++) {
				int p = To->m_Pixels[i];
				Buffer[numc * i] = palette[p].red;
				Buffer[numc * i + 1] = palette[p].green;
				Buffer[numc * i + 2] = palette[p].blue;
				if (numc == 4)Buffer[numc * i + 3] = trans_alpha[p];
			}
		}
		To->m_Format = numc == 3 ? cFormat::Rgb8 : cFormat::Rgba8;

		delete[] To->m_Pixels;
		To->m_Pixels = Buffer;
	}

	int Size;
	if(16 == bit_depth) {
		Size = To->m_Width * To->m_Height * NChannels * 2;
		for(i = 0; i < Size; i += 2) {
			cMath::Swap(To->m_Pixels[i], To->m_Pixels[i + 1]);
		}
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);

	LoadFile = nullptr;
	if(NChannels==2) {
		if (16 == bit_depth) {
			To->ToFormat(cFormat::Rgba16);
		}
		if (8 == bit_depth) {
			To->ToFormat(cFormat::Rgba8);
		}
	}
	return true;
} // cCodecPng::Decode

//-----------------------------------------------------------------------------
// cCodecPng::Encode
//-----------------------------------------------------------------------------
bool cCodecPng::Encode(const cImage &Src, cFile *To) {
	cAssert(To != nullptr);

	int Type;
	switch(Src.GetFormat()) {
		case cFormat::R8:
		case cFormat::R16:
			Type = PNG_COLOR_TYPE_GRAY;
			break;
		case cFormat::Rg8:
		case cFormat::Rg16:
			Type = PNG_COLOR_TYPE_GRAY_ALPHA;
			break;
		case cFormat::Rgb8:
		case cFormat::Rgb16:
			Type = PNG_COLOR_TYPE_RGB;
			break;
		case cFormat::Rgba8:
		case cFormat::Rgba16:
			Type = PNG_COLOR_TYPE_RGBA;
			break;
		default:
			return false;
	}

	png_structp png_ptr;
	png_infop info_ptr;

	png_ptr = png_create_write_struct_2(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr, nullptr, malloc_fn, free_fn);
	if(nullptr == png_ptr) {
		return false;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if(nullptr == info_ptr) {
		png_destroy_write_struct(&png_ptr, nullptr);
		return false;
	}

	SaveFile = To;
	png_set_write_fn(png_ptr, nullptr, user_write_data, user_flush_data);
	int Bpp = Src.GetFormat() >= cFormat::R16 ? 16 : 8;

	png_set_IHDR(png_ptr, info_ptr, Src.GetWidth(), Src.GetHeight(), Bpp, Type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_write_info(png_ptr, info_ptr);

	int NElems = Src.GetWidth() * cFormat::ChannelCount(Src.GetFormat()), Y, X;
	cList<word> Line;
	const word *Ptr;

	if(Src.GetFormat() >= cFormat::R16) {
		Line.SetCount(NElems);
		for(Y = 0; Y < Src.GetHeight(); Y++) {
			Ptr = ((const word *)Src.GetPixels()) + (Src.GetHeight() - 1 - Y) * NElems;
			for(X = 0; X < NElems; X++) {
				Line[X] = (Ptr[X] >> 8) | (Ptr[X] << 8);
			}
			png_write_row(png_ptr, (byte *)Line.ToPtr());
		}
	} else {
		for(Y = 0; Y < Src.GetHeight(); Y++) {
			png_write_row(png_ptr, Src.GetPixels() + (Src.GetHeight() - 1 - Y) * NElems);
		}
	}

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	SaveFile = nullptr;
	return true;
} // cCodecPng::Encode
int cCodecPng::CheckMagic(dword Magic, const char* ext){
	char* s = (char*)&Magic;
	return (s[1] == 'P' && s[2] == 'N' && s[3] == 'G') ? 1 : 0;
}
#endif // COMMS_PNG

} // comms
