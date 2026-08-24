#pragma once

//*****************************************************************************
// cDimension
//*****************************************************************************
/**
 * \brief Defines the dimensionality of an image.
 * * Helper structure to determine if an image is 1D, 2D, 3D (Volume), or a Cubemap
 * based on its dimensions.
 */
struct PYCALL cDimension {
	enum Enum {
		None = 0,
		OneD = 1, ///< 1D Texture (Width > 0, Height = 1, Depth = 1)
		TwoD = 2, ///< 2D Texture (Width > 0, Height > 1, Depth = 1)
		ThreeD = 3, ///< 3D/Volume Texture (Depth > 1)
		Cube = 4  ///< Cubemap Texture (Depth == 0 usually indicates a specific flag for Cubemaps in some engines)
	};

	static const char* ToString(const Enum Dimension);

	/**
	 * \brief Determines the dimension type based on size parameters.
	 * \param Width Image width.
	 * \param Height Image height.
	 * \param Depth Image depth.
	 * \return The calculated dimension enum.
	 */
	static const Enum ToDimension(const int Width, const int Height, const int Depth) {
		if (1 == Depth && 1 == Height) {
			return OneD;
		}
		else if (1 == Depth && Height > 1) {
			return TwoD;
		}
		else if (Depth > 1) {
			return ThreeD;
		}
		else if (0 == Depth) {
			return Cube;
		}
		return None;
	}
}; // cDimension

//*****************************************************************************
// cFormat
//*****************************************************************************
/**
 * \brief Defines pixel formats supported by the engine.
 * * Includes uncompressed (Plain), Floating point, Depth/Stencil, and Compressed formats (DXT, PVRTC).
 * Provides static helper methods to query format properties (stride, channel count, etc.).
 */
struct PYCALL cFormat {
	enum Enum {
		fmtNone = 0,

		// Plain:
		R8 = 1,  ///< 1 channel, 8-bit unsigned integer
		Rg8 = 2,  ///< 2 channels, 8-bit unsigned integer
		Rgb8 = 3,  ///< 3 channels, 8-bit unsigned integer
		Rgba8 = 4,  ///< 4 channels, 8-bit unsigned integer

		R16 = 5,  ///< 1 channel, 16-bit unsigned integer
		Rg16 = 6,  ///< 2 channels, 16-bit unsigned integer
		Rgb16 = 7,  ///< 3 channels, 16-bit unsigned integer
		Rgba16 = 8,  ///< 4 channels, 16-bit unsigned integer

		R16f = 9,  ///< 1 channel, 16-bit float (Half float)
		Rg16f = 10, ///< 2 channels, 16-bit float
		Rgb16f = 11, ///< 3 channels, 16-bit float
		Rgba16f = 12, ///< 4 channels, 16-bit float

		R32f = 13, ///< 1 channel, 32-bit float
		Rg32f = 14, ///< 2 channels, 32-bit float
		Rgb32f = 15, ///< 3 channels, 32-bit float
		Rgba32f = 16, ///< 4 channels, 32-bit float

		// Depth:
		Depth16 = 17,
		Depth24 = 18,
		Depth24Stencil8 = 19,

		// Compressed:
		Dxt1 = 20, ///< S3TC DXT1 compression (RGB, 1-bit Alpha)
		Dxt3 = 21, ///< S3TC DXT3 compression (RGBA, Explicit Alpha)
		Dxt5 = 22, ///< S3TC DXT5 compression (RGBA, Interpolated Alpha)

		// PowerVR Texture Compression
		PVRTC4 = 23,
		PVRTC4_Alpha = 24,
		Count = 25
	};

	/** \brief Returns true if the format is a standard uncompressed integer or float format. */
	static bool IsPlain(const Enum Format) {
		return Format <= Rgba32f;
	}

	/** \brief Returns true if the format is a depth or depth-stencil format. */
	static bool IsDepth(const Enum Format) {
		return Format >= Depth16 && Format <= Depth24Stencil8;
	}

	static bool HasStencil(const Enum Format) {
		return Depth24Stencil8 == Format;
	}

	/** \brief Returns true if the format is block-compressed (DXT or PVRTC). */
	static bool IsCompressed(const Enum Format) {
		return Format >= Dxt1 && Format <= PVRTC4_Alpha;
	}

	/** \brief Returns true if the format uses floating point components (16f or 32f). */
	static bool IsFloat(const Enum Format) {
		return Format >= R16f && Format <= Rgba32f;
	}

	/** \brief Returns the number of color channels (1 to 4). */
	static int ChannelCount(const Enum Format) {
		static const int Count[] = {
			0, // None
			1, // R8
			2, // Rg8
			3, // Rgb8
			4, // Rgba8
			1, // R16
			2, // Rg16
			3, // Rgb16
			4, // Rgba16
			1, // R16f
			2, // Rg16f
			3, // Rgb16f
			4, // Rgba16f
			1, // R32f
			2, // Rg32f
			3, // Rgb32f
			4, // Rgba32f
			1, // Depth16
			1, // Depth24
			1, // Depth24Stencil8
			3, // Dxt1
			4, // Dxt3
			4, // Dxt5
			3, // PVRTC4
			4  // PVRTC4_Alpha
		};
		return Count[Format];
	}

	/** \brief Returns bytes per single channel. Assumes Plain format. */
	static int BytesPerChannel(const Enum Format) { // Only plain formats
		cAssert(IsPlain(Format));

		if (Format <= Rgba8) {
			return 1;
		}
		if (Format <= Rgba16f) {
			return 2;
		}
		return 4;
	}

	/** \brief Returns total bytes per pixel. Valid only for Plain formats. Returns -1 for others. */
	static int BytesPerPixel(const Enum Format) { // Only plain formats
		cAssert(IsPlain(Format));

		static const int Bytes[] = {
			0, // None
			1, // R8
			2, // Rg8
			3, // Rgb8
			4, // Rgba8
			2, // R16
			4, // Rg16
			6, // Rgb16
			8, // Rgba16
			2, // R16f
			4, // Rg16f
			6, // Rgb16f
			8, // Rgba16f
			4, // R32f
			8, // Rg32f
			12, // Rgb32f
			16, // Rgba32f
			-1, // Depth16
			-1, // Depth24
			-1, // Depth24Stencil8
			-1, // Dxt1
			-1, // Dxt3
			-1, // Dxt5
			-1, // PVRTC4
			-1  // PVRTC4_Alpha
		};
		return Bytes[Format];
	}

	/** \brief Returns bytes per block for compressed formats (8 or 16 bytes). */
	static int BytesPerBlock(const Enum Format) { // Only compressed formats
		cAssert(IsCompressed(Format));
		return (Dxt5 == Format || Dxt3 == Format) ? 16 : 8;
	}

	static const char* ToString(const Enum Format);
	static Enum ToFormat(const char* String);
}; // cFormat

//*****************************************************************************
// cImage
//*****************************************************************************
/**
 * \brief Main class for handling raster images.
 * * Provides functionality for creating, loading, manipulating, and querying
 * image data. Supports various pixel formats, mipmapping, and compression operations.
 * Designed to be compatible with OpenGL texture uploads.
 */
class APICALL cImage {
	friend class cCodecDds;
	friend class cCodecJpeg;
	friend class cCodecTga;
	friend class cCodecPng;
	friend class cCodecTiff;
	friend class cCodecBin;
public:
	/** \brief Calculates total size in bytes for an image with given parameters. */
	static int CalcImageSize(const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount);

	// Default max count of mipmaps
	static int MipMapsAll;

	cImage();

	/** \brief Copy constructor. Performs a deep copy of image data. */
	cImage(const cImage& Src);
	void Copy(const cImage& Src);

	/** \brief Region copy constructor. Copies a sub-rectangle from source. */
	cImage(const cImage& Src, const comms::cRect& rc);
	void Copy(const cImage& Src, const comms::cRect& rc);

#ifndef PY_PARSER
	enum ECopyCtor { CopyCtor };
	enum ESetCtor { SetCtor };
#endif
	/** \brief Copies data from a raw buffer. */
	void Copy(const void* Pixels, const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount);

#ifndef PY_PARSER
	/** \brief Constructor using CopyCtor tag. Allocates new memory and copies provided pixels. */
	cImage(const ECopyCtor, const void* Pixels, const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount);
#endif
	/** \brief Sets the image to point to existing memory (or adopts it). */
	void Set(void* Pixels, const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount);
	void SetHeight(int Height);

#ifndef PY_PARSER
	/** \brief Constructor using SetCtor tag. Wraps existing pixel data. */
	cImage(const ESetCtor, void* Pixels, const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount);
#endif
	/** \brief Allocates memory for a new image. */
	void Create(const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount);
	cImage(const cFormat::Enum Format, const int Width, const int Height, const int Depth, const int MipMapCount);

	/** \brief Releases the pixel memory. */
	void Free();
	~cImage();

	int GetWidth() const {
		return m_Width;
	}
	int GetHeight() const {
		return m_Height;
	}
	int GetDepth() const {
		return m_Depth;
	}

	/** \brief Returns the width at a specific mipmap level. */
	int GetWidth(const int MipMapLevel) const {
		return cMath::Max(1, m_Width >> MipMapLevel);
	}
	/** \brief Returns the height at a specific mipmap level. */
	int GetHeight(const int MipMapLevel) const {
		return cMath::Max(1, m_Height >> MipMapLevel);
	}
	/** \brief Returns the depth at a specific mipmap level. */
	int GetDepth(const int MipMapLevel) const {
		return cMath::Max(1, m_Depth >> MipMapLevel);
	}

	cFormat::Enum GetFormat() const {
		return m_Format;
	}

	int GetSize() const {
		return CalcImageSize(m_Format, m_Width, m_Height, m_Depth, m_MipMapCount);
	}

	/** \brief Returns pointer to the raw pixel data. */
	byte* GetPixels() const {
		return m_Pixels;
	}
	/** \brief Returns pointer to the start of a specific mipmap level. */
	byte* GetPixels(const int MipMapLevel) const;

	int GetMipMapCount() const { return m_MipMapCount; }
	int GetMipMapCountFromDimensions() const;
#ifndef PY_PARSER
	/** \brief Returns size in bytes of the mipmap chain starting from a level. */
	int GetMipMappedSize(const int MipMapFrom = 0, const int MipMapCount = cImage::MipMapsAll, const cFormat::Enum SrcFormat = cFormat::fmtNone) const;

	/** \brief Returns size in bytes of one depth slice (2D image size). */
	int GetSliceSize(const int MipMapLevel = 0, const cFormat::Enum SrcFormat = cFormat::fmtNone) const;

	/** \brief Returns size in bytes of one row (Pitch). */
	int GetRowSize(const int MipMapLevel = 0, const cFormat::Enum SrcFormat = cFormat::fmtNone) const;

	int GetPixelCount(const int MipMapFrom = 0, const int MipMapCount = cImage::MipMapsAll) const;
#endif
	const cDimension::Enum GetDimension() const {
		return cDimension::ToDimension(m_Width, m_Height, m_Depth);
	}

	// Only plain formats:
	bool SwapChannels(const int Ch0, const int Ch1);
	bool RemoveChannels(bool KeepCh0, bool KeepCh1 = true, bool KeepCh2 = true, bool KeepCh3 = true);
	bool InvertChannel(const int Ch);
	bool CopyChannel(const int From, const int To);

	bool ToFormat(const cFormat::Enum Format);

	/** \brief Decompresses DXT1/3/5 into Rgb8 or Rgba8. */
	// Dxt1 -> Rgb8
	// Dxt3, Dxt5 -> Rgba8
	void Uncompress();

	/** \brief Compresses R8/Rgb8/Rgba8 into DXT formats. */
	// R8, Rgb8 -> Dxt1
	// Rgba8 -> Dxt5
	void Compress();

	// Rgb8, Rgba8 -> R8
	// Rgb16, Rgba16 -> R16
	bool ToGrayScale();

	/** * \brief Converts a heightmap to a normal map.
	 * \param Format Target format (usually Rgb8 or Rgba8).
	 * \param Z Scale factor for the Z component (flatness).
	 * \param OldAlpha If true, preserves existing alpha.
	 */
	 // From: R8, Rgb8, Rgba8	
	 // To (2D): Rg8, Rgba8
	 // To (3D, Volume Texture): Rgb8, Rgba8
	 // "OldAlpha" works only if(Rgba8 == m_Format && Rgba8 == Format)
	bool ToNormalMap(const cFormat::Enum Format, const float Z = 1.0f, const bool OldAlpha = false);

	/** \brief Generates mipmaps for the image. Not supported for compressed formats. */
	bool CreateMipMaps(const int MipMapCount = cImage::MipMapsAll); // !Compressed
	void RemoveMipMaps();

	//*************************************************************************
	// GetPixel(Rgb8, Rgba8, R8)
	//*************************************************************************
	struct PixelRgb8 {
		byte r, g, b;
	};
	struct PixelRgba8 {
		byte r, g, b, a;
	};

#ifndef PY_PARSER
	/** \brief Returns RGB pixel color. Assumes Rgb8 format. */
	const PixelRgb8 GetPixelRgb8(const int X, const int Y) const;
	void SetPixelRgb8(const int X, const int Y, const PixelRgb8& p);

	/** \brief Returns RGBA pixel color. Assumes Rgba8 or Rgb8 format. */
	const PixelRgba8 GetPixelRgba8(const int X, const int Y) const;
	void SetPixelRgba8(const int X, const int Y, const PixelRgba8& p);
#endif
	/** \brief Returns pixel intensity. Assumes R8 format. */
	byte GetPixelR8(const int X, const int Y) const;
	void SetPixelR8(const int X, const int Y, const byte p);

	void MergeRgb8(const cImage& From, const int X, const int Y);
	void MergeRgba8(const cImage& From, const int X, const int Y, bool ColorAsAlpha);

	/** \brief Flips the image vertically. Supports Plain, Packed, and Compressed formats. */
	bool Flip(); // Plain, packed and compressed formats

	/** \brief Resizes the image using resampling. Plain formats only. */
	bool Resize(const int Width, const int Height); // Plain formats && !Cube

	bool Decrease2X();

	/** \brief Resizes the image to the nearest power of two dimensions. */
	bool MakePowerOfTwo(); // Returns "true" if it is power of two already or has been converted successfully

	/** \brief Converts a 3D volume texture into a 2D strip of slices. */
	bool Make2D(const bool Square = true, const int DepthStep = 1); // Converts 3D texture to 2D set of depth slices

	void CreateSevenLods(const char* SaveAs = "data/textures/SevenLods.dds");

	comms::cBufInfo _py_buffer_info();

protected:
	int m_Width;
	int m_Height;
	int m_Depth;
	cFormat::Enum m_Format;
	int m_MipMapCount;
	byte* m_Pixels;
};

//-----------------------------------------------------------------------------------
// cImage::GetPixelRgb8
//-----------------------------------------------------------------------------------
inline const cImage::PixelRgb8 cImage::GetPixelRgb8(const int X, const int Y) const {
	cAssert(cFormat::Rgb8 == m_Format);

	PixelRgb8 p;
	if (X < 0 || X >= m_Width || Y < 0 || Y >= m_Height) {
		p.r = p.g = p.b = 0;
		return p;
	}
	int Index = 3 * (m_Width * Y + X);
	const byte* Src = &m_Pixels[Index];
	p.r = *Src++;
	p.g = *Src++;
	p.b = *Src++;
	return p;
} // cImage::GetPixelRgb8

//------------------------------------------------------------------------------
// cImage::SetPixelRgb8
//------------------------------------------------------------------------------
inline void cImage::SetPixelRgb8(const int X, const int Y, const PixelRgb8& p) {
	cAssert(cFormat::Rgb8 == m_Format);

	if (X < 0 || X >= m_Width || Y < 0 || Y >= m_Height) {
		return;
	}
	int Index = 3 * (m_Width * Y + X);
	byte* Src = &m_Pixels[Index];
	*Src = p.r;
	Src++;

	*Src = p.g;
	Src++;

	*Src = p.b;
	Src++;
} // cImage::SetPixelRgb8

//-------------------------------------------------------------------------------------
// cImage::GetPixelRgba8
//-------------------------------------------------------------------------------------
inline const cImage::PixelRgba8 cImage::GetPixelRgba8(const int X, const int Y) const {
	cAssert(cFormat::Rgba8 == m_Format || cFormat::Rgb8 == m_Format);
	PixelRgba8 p;
	if (cFormat::Rgb8 == m_Format) {
		int Index = 3 * (m_Width * Y + X);
		const byte* Src = &m_Pixels[Index];
		p.r = *Src++;
		p.g = *Src++;
		p.b = *Src++;
		p.a = 255;
		return p;
	}
	if (X < 0 || X >= m_Width || Y < 0 || Y >= m_Height) {
		p.r = p.g = p.b = p.a = 0;
		return p;
	}
	int Index = 4 * (m_Width * Y + X);
	const byte* Src = &m_Pixels[Index];
	p.r = *Src++;
	p.g = *Src++;
	p.b = *Src++;
	p.a = *Src++;
	return p;
} // cImage::GetPixelRgba8

//--------------------------------------------------------------------------------
// cImage::SetPixelRgba8
//--------------------------------------------------------------------------------
inline void cImage::SetPixelRgba8(const int X, const int Y, const PixelRgba8& p) {
	cAssert(cFormat::Rgba8 == m_Format);

	if (X < 0 || X >= m_Width || Y < 0 || Y >= m_Height) {
		return;
	}
	int Index = 4 * (m_Width * Y + X);
	byte* Src = &m_Pixels[Index];
	*Src = p.r;
	Src++;

	*Src = p.g;
	Src++;

	*Src = p.b;
	Src++;

	*Src = p.a;
} // cImage::SetPixelRgba8

//-----------------------------------------------------------------------------
// cImage::GetPixelR8
//-----------------------------------------------------------------------------
inline byte cImage::GetPixelR8(const int X, const int Y) const {
	cAssert(cFormat::R8 == m_Format);

	if (X < 0 || X >= m_Width || Y < 0 || Y >= m_Height) {
		return 0;
	}
	int Index = m_Width * Y + X;
	return m_Pixels[Index];
} // cImage::GetPixelR8

// cImage::SetPixelR8
inline void cImage::SetPixelR8(const int X, const int Y, const byte p) {
	cAssert(cFormat::R8 == m_Format);

	if (X < 0 || X >= m_Width || Y < 0 || Y >= m_Height) {
		return;
	}
	int Index = m_Width * Y + X;
	m_Pixels[Index] = p;
}

