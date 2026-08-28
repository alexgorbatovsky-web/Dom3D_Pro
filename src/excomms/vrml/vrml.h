/*
 * File	    : VRML.h
 * Purpouse : VRML header definition of the VRML format specification
 * Data	    : 25/03/2013
 */

#ifndef __VRML_H__
#define __VRML_H__

#define VRML_VERSION	  	"#VRML V2.0 utf8\n"
#define VRML_DESCRIPTION  	"#3D-Coat VRML exporter at http://3d-coat.com\n\n" 
#define MAX_VERTECIES	  	0x3fffffff
#define SSTL             	 64

#define VRML_ERR_NO          0
#define VRML_ERR_BAD_WORD   -4 
#define VRML_ERR_BAD_SYMBOL -3 
#define VRML_ERR_PARSE      -2
#define VRML_EOF            -1  // end of file
#define VRML_EON             1  // end of node
#define VRML_EOFD            2  // end of field	

#define STCAST_OBJ(obj,type)  static_cast<type>(obj)
#define DNCAST_OBJ(obj,type)  dynamic_cast<type>(obj)
#define SAFE_RELEASE_OBJ(obj) if(obj) { obj->Release(); obj=NULL; }

#define cObject comms::cObject
#define cMesh   comms::cMeshContainer
#define mfarray cList

typedef  comms::cImage	 cImage;
typedef  comms::cSurface cSurface;			
typedef  comms::cColor   cColor;
typedef  comms::cData    cData;
typedef  comms::cMat4    mat4f;
typedef  comms::cMat3    mat3f;

typedef  unsigned int	uint32;
typedef  int32_t   int32;
typedef  unsigned char	uint8;

typedef struct rgb {
	float r;
	float g;
	float b;
	rgb() { r = g = b = 0.0;}
	uint32 to_fdword(float f){
	   return f >= 1.0f ? 0xff : f <= 0.0f ? 0x00 : uint32(f * 255.0f + 0.5f);
	}
	uint32 to_dword(){
	   return to_fdword(b) | (to_fdword(g) << 8) | (to_fdword(r) << 16) | (255 << 24);
	}
	rgb &operator+=(const rgb& c){
		r+=c.r;
		g+=c.g;
		b+=c.b;
		return (*this);
	}
	rgb &operator/=( const float value){
		if(value!=0.0){
			r/=value;
			g/=value;
			b/=value;
		}
		return (*this);
	}
} rgb;

typedef cList<rgb>  mfcolor;
typedef cList<cVec3> mfvec3f;
typedef cList<cVec2> mfvec2f;
typedef cList<float> mffloat;
typedef cList<int> mfint32;
typedef cVec3 sfvec3f;
typedef comms::cVec2 sfvec2f;
typedef comms::cVec4 sfvec4f;
typedef sfvec3f sfcolor;
typedef float sffloat;
typedef int sfint32;
typedef bool sfbool;
typedef char sfchar;
typedef unsigned char sfbyte;
typedef char strFixed64[SSTL];
typedef char CH;
typedef char* PCH;
typedef const char* CPCH;


class PixImage {
        uint32 width;
        uint32 height;
        uint32 channel;
        cList<unsigned char> data;
public:
	   PixImage();
	   PixImage(uint32 w, uint32 h, uint32 c);
	   
	   uint32 GetWidth() const { return width;}
	   void SetWidth(uint32 _width) { width = _width; } 
		
	   uint32 GetHeight() const { return height; }
	   void SetHeight(uint32 _height) { height = _height; }
		
 	   uint32 GetChannel() const { return channel; }
	   void SetChannel(uint32 _channel) { channel = _channel;}
        
       int32 GetPixel( uint32 x, uint32 y) const;
       void SetPixel(uint32 x, uint32 y, int32 value);

	   const cList<unsigned char>& GetData() const { return data;}
	};

	inline PixImage::PixImage():
		 width(0)
		,height(0)
		,channel(0)
	{;}
	inline PixImage::PixImage(uint32 w, uint32 h, uint32 c):
		width(w)
		,height(h)
		,channel(c)
		,data(w * h * c)
	{}
    
    inline int32 PixImage::GetPixel(const uint32 x, const uint32 y) const
    {
		uint32 size = data.Size();
        assert((x * y * channel) < size);
		if ( x * y * channel < size){
			int32 retval = 0x00000000;
			uint32 index = x*y;
			for (uint32 c = channel, i = index * channel;
				 c > 0;--c, ++i) {
				retval |=
                static_cast<int32>(data[i])<<(8 * (c - 1));
			}
			return retval;
		}
        return 0;
    }
    
    inline void PixImage::SetPixel( const uint32 x, const uint32 y, const int32 value)
    {
        uint32 size = data.Size();
        assert((x * y * channel) < size);
		if ( x * y * channel < size){
			uint32 index = x*y;
			for (uint32 c = channel, i = index * channel;
				 c > 0;--c, ++i) {
				 data[i] = 
                 static_cast<unsigned char>((value >> (8 * (c - 1))) & 0x000000ff);
			}
		}
    }

typedef class PixImage sfimage;

#endif