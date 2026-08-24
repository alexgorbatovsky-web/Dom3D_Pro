
#include "comms.h"

namespace comms {

	bool cCodecBin::Decode(const cFile& Src, cImage* To) {
		cAssert(To != nullptr);
		To->Free();
		const int len = 32;
		int j = 0, n = 0, i = len - 1, posImage = 0;
		char ch = '&';
		char sh[len];
		while (j < len) {
			char c = Src.ReadByte();
			if (c == ch) n++;
			if (n == 3) {
				posImage = j + 1;
				break;
			}
			j++;
		}
		if (posImage == 0) {
			return false;
		}
		Src.SetPos(0);
		Src.ReadBytes(sh, posImage);

		int Width = 0, Height = 0, NChannels = 0;
		for (int k = 0, j = 0, i = 0; i < posImage; i++) {
			if (sh[i] == ch) {
				sh[i] = '\0';
				if (k == 0) Width = atoi(&sh[j]);
				if (k == 1) Height = atoi(&sh[j]);
				if (k == 2) NChannels = atoi(&sh[j]);
				j = i + 1;
				k++;
			}
		}
		if (Width && Height && NChannels) {
			;
		}
		else {
			return false;
		}
		To->m_Width = Width;
		To->m_Height = Height;
		To->m_Depth = 1;
		To->m_MipMapCount = 1;
		int PixelSize = NChannels * sizeof(float);
		int ImageSize = Width * Height * PixelSize;
		cList<byte> From;
		From.SetCount(ImageSize);
		if (Src.ReadBytes(From.ToPtr(), ImageSize) != ImageSize) {
			return false;
		}
		if (NChannels == 3) {
			To->m_Format = cFormat::Rgb32f;
			To->m_Pixels = (byte*)new byte[ImageSize];
			float* Ptr1 = (float*)To->GetPixels();
			float* Ptr0 = (float*)From.ToPtr();
			for (int Y = 0; Y < Height; Y++) {
				for (int X = 0; X < Width; X++) {
					*Ptr1++ = Ptr0[2];
					*Ptr1++ = Ptr0[1];
					*Ptr1++ = Ptr0[0];
					Ptr0 += NChannels;
				}
			}
		}
		else if (NChannels == 1) {
			To->m_Format = cFormat::R32f;
			To->m_Pixels = (byte*)new byte[ImageSize];
			byte* Ptr1 = To->GetPixels();
			byte* Ptr0 = From.ToPtr();
			memcpy(Ptr1, Ptr0, ImageSize);
		}
		return true;
	}

	bool cCodecBin::Encode(const cImage& Src, cFile* To) {
		cAssert(To != nullptr);
		cFormat::Enum Format = Src.GetFormat();
		//R16f
		//R32f
		//RGB32f
		if (Format != cFormat::R16f && Format != cFormat::R32f && Format != cFormat::Rgb32f) {
			return false;
		}
		int NChannels = cFormat::ChannelCount(Format);
		int NBytesChannel = cFormat::BytesPerChannel(Format);
		cStr strHeader;
		strHeader << Src.GetWidth() << "&" << Src.GetHeight() << "&" << NChannels << "&";
		To->WriteString(strHeader.ToCharPtr());
		if (Format == cFormat::Rgb32f) {
			cList<float> Buffer;
			Buffer.SetCount(Src.GetWidth() * Src.GetHeight() * NChannels);
			byte* Ptr = (byte*)Buffer.ToPtr();
			memcpy(Ptr, Src.GetPixels(), Src.GetWidth() * Src.GetHeight() * NChannels * NBytesChannel);
			cMath::SwapChannels(Buffer.ToPtr(), Buffer.Count() / NChannels, NChannels, 0, 2);
			To->WriteBytes((byte*)Buffer.ToPtr(), Buffer.Count() * NBytesChannel);
		}
		else {
			cList<byte> Buffer;
			Buffer.Copy(Src.GetPixels(), Src.GetWidth() * Src.GetHeight() * NChannels * NBytesChannel);
			To->WriteBytes(Buffer.ToPtr(), Buffer.Count());
		}
		return true;
	}

	int cCodecBin::CheckMagic(dword Magic, const char* ext) {
		return -1;//no magic
	}
}