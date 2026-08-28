#include "stdafx.h"
namespace comms{

//-----------------------------------------------------------------------------
// cSurface::Map.ctor
//-----------------------------------------------------------------------------
cSurface::Map::Map() {
	Unit = 0;
	MipMap = true;
	//id = cRender::idNone;
	Offset.SetZero();
	Scale.SetOne();
    DepthTransform = Matrix4D::Identity;
} // cSurface::Map.ctor

//-----------------------------------------------------------------------------
// cSurface::Map::Load
//-----------------------------------------------------------------------------
bool cSurface::Map::Load() {
	//id = cRender::idNone;
	if(FileName.Length() == 0) {
		return true;
	}
	
	//id = cRender::GetInstance()->AddTexture(FileName, MipMap);
	return false;//id != cRender::idNone;
} // cSurface::Map::Load

// cSurface.ctor
cSurface::cSurface(const char *Name) : Name(Name) {
	// @todo fine  Remove dublicates from ctors.
    UseLimitAngle=false;
    LimitAngle=3.1415/6;
    Luminosity=0;
    Reflection=0;
    Refraction=0;
    Transparency=0;
    Translucensy=0;
	Shininess=0;
    cColor id(255,255,255,255);
    cColor z(0,0,0,0);
    DepthModulator=1;
    memcpy(&Color,&id,sizeof(id));
    memcpy(&Ambient,&z,sizeof(id));
    memcpy(&Diffuse,&id,sizeof(id));
    memcpy(&Specular,&z,sizeof(id));
    memcpy(&Gloss,&id,sizeof(id));
}

//---------------------------------------------------------------------------------------------------------------------------------
// cSurface.ctor
//---------------------------------------------------------------------------------------------------------------------------------
cSurface::cSurface(const char *Name, const cColor &Ambient, const cColor &Diffuse, const cColor &Specular, const float Shininess)
: Name(Name), Ambient(Ambient), Diffuse(Diffuse), Specular(Specular), Shininess(Shininess) {
	for(int i = 0; i < 8; i++) {
		Tex[i].Unit = i;
	}
    UseLimitAngle=false;
    LimitAngle=3.1415/6;
    DepthModulator=1;
    Luminosity=0;
    Reflection=0;
    Refraction=0;
    Transparency=0;
    Translucensy=0;
    DepthModulator=1;
    Color=cColor(1);	
    Gloss=cColor(0);
} // cSurface.ctor

//-----------------------------------------------------------------------------
// cSurface::Load
//-----------------------------------------------------------------------------
void cSurface::Load() {
	for(int i = 0; i < 8; i++) {
		Tex[i].Load();
	}
} // cSurface::Load

const cSurface cSurface::Brass("Brass",
								 cColor(0.329412f, 0.223529f, 0.027451f, 1.0f),
								 cColor(0.780392f, 0.568627f, 0.113725f, 1.0f),
								 cColor(0.992157f, 0.941176f, 0.807843f, 1.0f), 27.8974f);

const cSurface cSurface::Bronze("Bronze",
								  cColor(0.2125f, 0.1275f, 0.054f, 1.0f),
								  cColor(0.714f, 0.4284f, 0.18144f, 1.0f),
								  cColor(0.393548f, 0.271906f, 0.166721f, 1.0f), 25.6f);

const cSurface cSurface::PolishedBronze("Polished Bronze",
										  cColor(0.25f, 0.148f, 0.06475f, 1.0f),
										  cColor(0.4f, 0.2368f, 0.1036f, 1.0f),
										  cColor(0.774597f, 0.458561f, 0.200621f, 1.0f), 76.8f);

const cSurface cSurface::Chrome("Chrome",
								  cColor(0.25f, 0.25f, 0.25f, 1.0f),
								  cColor(0.4f, 0.4f, 0.4f, 1.0f),
								  cColor(0.774597f, 0.774597f, 0.774597f, 1.0f), 76.8f);

const cSurface cSurface::Copper("Copper",
								  cColor(0.19125f, 0.0735f, 0.0225f, 1.0f),
								  cColor(0.7038f, 0.27048f, 0.0828f, 1.0f),
								  cColor(0.256777f, 0.137622f, 0.086014f, 1.0f), 12.8f);

const cSurface cSurface::PolishedCopper("Polished Copper",
										  cColor(0.2295f, 0.08825f, 0.0275f, 1.0f),
										  cColor(0.5508f, 0.2118f, 0.066f, 1.0f),
										  cColor(0.580594f, 0.223257f, 0.0695701f, 1.0f), 51.2f);

const cSurface cSurface::Gold("Gold",
								cColor(0.24725f, 0.1995f, 0.0745f, 1.0f),
								cColor(0.75164f, 0.60648f, 0.22648f, 1.0f),
								cColor(0.628281f, 0.555802f, 0.366065f, 1.0f), 51.2f);

const cSurface cSurface::PolishedGold("Polished Gold",
										cColor(0.24725f, 0.2245f, 0.0645f, 1.0f),
										cColor(0.34615f, 0.3143f, 0.0903f, 1.0f),
										cColor(0.797357f, 0.723991f, 0.208006f, 1.0f), 83.2f);

const cSurface cSurface::Pewter("Pewter",
								  cColor(0.105882f, 0.058824f, 0.113725f, 1.0f),
								  cColor(0.427451f, 0.470588f, 0.541176f, 1.0f),
								  cColor(0.333333f, 0.333333f, 0.521569f, 1.0f), 9.84615f);

const cSurface cSurface::Silver("Silver",
								  cColor(0.19225f, 0.19225f, 0.19225f, 1.0f),
								  cColor(0.50754f, 0.50754f, 0.50754f, 1.0f),
								  cColor(0.508273f, 0.508273f, 0.508273f, 1.0f), 51.2f);

const cSurface cSurface::PolishedSilver("Polished Silver",
										  cColor(0.23125f, 0.23125f, 0.23125f, 1.0f),
										  cColor(0.2775f, 0.2775f, 0.2775f, 1.0f),
										  cColor(0.773911f, 0.773911f, 0.773911f, 1.0f), 89.6f);

const cSurface cSurface::Emerald("Emerald",
								   cColor(0.0215f, 0.1745f, 0.0215f, 0.55f),
								   cColor(0.07568f, 0.61424f, 0.07568f, 0.55f),
								   cColor(0.633f, 0.727811f, 0.633f, 0.55f), 76.8f);

const cSurface cSurface::Jade("Jade",
								cColor(0.135f, 0.2225f, 0.1575f, 0.95f),
								cColor(0.54f, 0.89f, 0.63f, 0.95f),
								cColor(0.316228f, 0.316228f, 0.316228f, 0.95f), 12.8f);

const cSurface cSurface::Obsidian("Obsidian",
									cColor(0.05375f, 0.05f, 0.06625f, 0.82f),
									cColor(0.18275f, 0.17f, 0.22525f, 0.82f),
									cColor(0.332741f, 0.328634f, 0.346435f, 0.82f), 38.4f);

const cSurface cSurface::Pearl("Pearl",
								 cColor(0.25f, 0.20725f, 0.20725f, 0.922f),
								 cColor(1.0f, 0.829f, 0.829f, 0.922f),
								 cColor(0.296648f, 0.296648f, 0.296648f, 0.922f), 11.264f);

const cSurface cSurface::Ruby("Ruby",
								cColor(0.1745f, 0.01175f, 0.01175f, 0.55f),
								cColor(0.61424f, 0.04136f, 0.04136f, 0.55f),
								cColor(0.727811f, 0.626959f, 0.626959f, 0.55f), 76.8f);

const cSurface cSurface::Turquoise("Turquoise",
									 cColor(0.1f, 0.18725f, 0.1745f, 0.8f),
									 cColor(0.396f, 0.74151f, 0.69102f, 0.8f),
									 cColor(0.297254f, 0.30829f, 0.306678f, 0.8f), 12.8f);

const cSurface cSurface::BlackPlastic("Black Plastic",
										cColor(0.0f, 0.0f, 0.0f, 1.0f),
										cColor(0.01f, 0.01f, 0.01f, 1.0f),
										cColor(0.50f, 0.50f, 0.50f, 1.0f), 32.0f);

const cSurface cSurface::BlackRubber("Black Rubber", 
									   cColor(0.02f, 0.02f, 0.02f, 1.0f),
									   cColor(0.01f, 0.01f, 0.01f, 1.0f),
									   cColor(0.4f, 0.4f, 0.4f, 1.0f), 10.0f);

void cSurface::CopyTo(cSurface& dest){
    dest.Name=Name;
    dest.Color=Color;
	dest.Ambient=Ambient;
	dest.Diffuse=Diffuse;
	dest.Specular=Specular;
    dest.Gloss=Gloss;
    dest.Luminosity=Luminosity;
    dest.Reflection=Reflection;
    dest.Refraction=Refraction;
    dest.Transparency=Transparency;
    dest.Translucensy=Translucensy;
	dest.Shininess=Shininess;
    dest.DepthModulator=DepthModulator;
    dest.UseLimitAngle=UseLimitAngle;
    dest.LimitAngle=LimitAngle;
    for(int i=0;i<ExData.Count();i++){
        cMtlDataChunk c;
        dest.ExData.Add(c);
        cMtlDataChunk& dc=dest.ExData.GetLast();
        dc.Type=ExData[i].Type;
        int sz=ExData[i].Data.Count();
        dest.ExData[i].Data.Add(0,sz);
        memcpy(dest.ExData[i].Data.ToPtr(),ExData[i].Data.ToPtr(),sz);
    }
}
void WriteColor(cColor& c,comms::cFile& d){
    d.WriteFloat(c.r);
    d.WriteFloat(c.g);
    d.WriteFloat(c.b);
    d.WriteFloat(c.a);
}
void ReadColor(cColor& c,comms::cFile& d){
    c.r=d.ReadFloat();
    c.r=d.ReadFloat();
    c.r=d.ReadFloat();
    c.r=d.ReadFloat();
}
void cSurface::Store(comms::cFile& dest){
    dest.WriteDword(ExData.Count());
    for(int i=0;i<ExData.Count();i++){
        dest.WriteDword(ExData[i].Type);
        int sz=ExData[i].Data.Count();
        dest.WriteDword(sz);
        dest.WriteBytes(ExData[i].Data.ToPtr(),sz);
    }

    dest.WriteDword(1);
    WriteColor(Color,dest);
    WriteColor(Ambient,dest);
    WriteColor(Diffuse,dest);
    WriteColor(Specular,dest);
    WriteColor(Gloss,dest);
    dest.WriteFloat(Luminosity);
    dest.WriteFloat(Reflection);
    dest.WriteFloat(Refraction);
    dest.WriteFloat(Transparency);
    dest.WriteFloat(Translucensy);
    dest.WriteFloat(Shininess);
    dest.WriteFloat(DepthModulator);
    dest.WriteByte(UseLimitAngle);
    dest.WriteFloat(LimitAngle);       
}
void cSurface::ReStore(comms::cFile& dest){
    int n=dest.ReadDword();
    for(int i=0;i<n;i++){
        cMtlDataChunk c;
        ExData.Add(c);
        cMtlDataChunk& dc=ExData.GetLast();
        dc.Type=dest.ReadDword();
        int sz=dest.ReadDword();
        dc.Data.Add(0,sz);
        dest.ReadBytes(dc.Data.ToPtr(),sz);
    }
    DWORD v=dest.ReadDword();
    if(v==1){
        ReadColor(Color,dest);
        ReadColor(Ambient,dest);
        ReadColor(Diffuse,dest);
        ReadColor(Specular,dest);
        ReadColor(Gloss,dest);
        Luminosity=dest.ReadFloat();
        Reflection=dest.ReadFloat();
        Refraction=dest.ReadFloat();
        Transparency=dest.ReadFloat();
        Translucensy=dest.ReadFloat();
        Shininess=dest.ReadFloat();
        DepthModulator=dest.ReadFloat();
        UseLimitAngle=dest.ReadByte();
        LimitAngle=dest.ReadFloat();
    }
}
}
