#include "comms.h"
namespace comms {

#ifdef COMMS_JPEG

#include "jpeglib.h"

#pragma comment (lib, "jpeg.lib")

static void InitSource(j_decompress_ptr) {
}

static boolean FillInputBuffer(j_decompress_ptr) {
	return TRUE;
}

static void SkipInputData(j_decompress_ptr cinfo, long Bytes) {
	jpeg_source_mgr *Src = cinfo->src;
	if(Bytes > 0) {
		Src->bytes_in_buffer -= Bytes;
		Src->next_input_byte += Bytes;
	}
}

static void TermSource(j_decompress_ptr) {
}

static const int SaveBufferSize = 65535;
static thread_local cList<byte> SaveBuffer;
static thread_local cFile *SaveFile = nullptr;

static void InitDestination(j_compress_ptr cinfo) {
}

static boolean EmptyOutputBuffer(j_compress_ptr cinfo) {
	SaveFile->WriteBytes(SaveBuffer.ToPtr(), SaveBufferSize);
	cinfo->dest->next_output_byte = SaveBuffer.ToPtr();
	cinfo->dest->free_in_buffer = SaveBufferSize;
	return TRUE;
}

static void TermDestination(j_compress_ptr cinfo) {
	int N = SaveBufferSize - (int)cinfo->dest->free_in_buffer;
	SaveFile->WriteBytes(SaveBuffer.ToPtr(), N);
}

//-----------------------------------------------------------------------------
// cCodecJpeg::Decode
//-----------------------------------------------------------------------------
bool cCodecJpeg::Decode(const cFile &Src, cImage *To) {
	if(Src.Size()>4){
		dword D=Src.ReadDword();
		Src.SeekCur(-4);
		D&=0x00FFFFFF;
		if(D!=0xFFD8FF){
			return false;
		}
	}
	cAssert(To != nullptr);
	To->Free();

	jpeg_decompress_struct cinfo;
	jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_decompress(&cinfo);

	jpeg_source_mgr jsrc;
	jsrc.bytes_in_buffer = Src.Size();
	jsrc.next_input_byte = (const JOCTET *)Src.ToPtr();
	jsrc.init_source = InitSource;
	jsrc.fill_input_buffer = FillInputBuffer;
	jsrc.skip_input_data = SkipInputData;
	jsrc.resync_to_restart = jpeg_resync_to_restart;
	jsrc.term_source = TermSource;
	cinfo.src = &jsrc;
	
	jpeg_read_header(&cinfo, TRUE);
	jpeg_start_decompress(&cinfo);

	switch(cinfo.num_components) {
		case 1:
			To->m_Format = cFormat::R8;
			break;
		case 3:
			To->m_Format = cFormat::Rgb8;
			break;
		case 4:
			To->m_Format = cFormat::Rgba8;
			break;
	}
	To->m_Width = cinfo.output_width;
	To->m_Height = cinfo.output_height;
	To->m_Depth = 1;
	To->m_MipMapCount = 1;

	int WidthBytes = To->m_Width * cinfo.num_components;
	int Size = WidthBytes * To->m_Height;
	To->m_Pixels = new byte[Size];
	byte *CurScanLine = To->m_Pixels + Size - WidthBytes;

	while(cinfo.output_scanline < cinfo.output_height) {
		jpeg_read_scanlines(&cinfo, &CurScanLine, 1);
		CurScanLine -= WidthBytes;
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	return true;
} // cCodecJpeg::Decode

int cCodecJpeg::Quality = 75;

//-----------------------------------------------------------------------------
// cCodecJpeg::Encode
//-----------------------------------------------------------------------------
bool cCodecJpeg::Encode(const cImage &Image, cFile *To) {
	cAssert(To != nullptr);
	
	cFormat::Enum Format = Image.GetFormat();
	if(Format != cFormat::R8 && Format != cFormat::Rgb8) {
		return false;
	}

	jpeg_compress_struct cinfo;
	jpeg_error_mgr jerr;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	int NChannels = cFormat::ChannelCount(Format);
	cinfo.in_color_space = 1 == NChannels ? JCS_GRAYSCALE : JCS_RGB;
	jpeg_set_defaults(&cinfo);

	cinfo.input_components = NChannels;
	cinfo.num_components = NChannels;
	cinfo.image_width = Image.GetWidth();
	cinfo.image_height = Image.GetHeight();
	cinfo.data_precision = 8;
	cinfo.input_gamma = 1.0;

	jpeg_set_quality(&cinfo, Quality, FALSE);

	SaveBuffer.SetCount(SaveBufferSize);
	SaveFile = To;
	
	jpeg_destination_mgr jdst;
	jdst.next_output_byte = SaveBuffer.ToPtr();
	jdst.free_in_buffer = SaveBuffer.Count();
	jdst.init_destination = InitDestination;
	jdst.empty_output_buffer = EmptyOutputBuffer;
	jdst.term_destination = TermDestination;
	cinfo.dest = &jdst;
	
	jpeg_start_compress(&cinfo, TRUE);

	int WidthBytes = Image.GetWidth() * NChannels;
	int Size = WidthBytes * Image.GetHeight();
	byte *CurScanLine = Image.GetPixels() + Size - WidthBytes;

	int h;
	for(h = 0 ; h < Image.GetHeight(); h++) {
		jpeg_write_scanlines(&cinfo, &CurScanLine, 1);
		CurScanLine -= WidthBytes;
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	SaveBuffer.Free();
	SaveFile = nullptr;

	return true;
} // cCodecJpeg::Encode
int cCodecJpeg::CheckMagic(dword Magic, const char* ext){
	byte* s = (byte*)&Magic;
	return (s[0] == 0xFF && s[1] == 0xD8 && s[2] == 0xFF && s[3] == 0xE0) ? 1 : 0;
}
#endif // COMMS_JPEG

} // comms
