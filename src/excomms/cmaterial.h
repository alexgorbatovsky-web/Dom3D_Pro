#pragma once
class cObject{
public:
	explicit cObject(const cStr& Name = "") :
		Name(Name),
        Transform(cMat4::Identity),
		Pivot(cVec3::Zero),
		PivotPresent(false),
		NodeID(0),
		ParentID(0)
	{}
    cStr Name;
    cMat4 Transform;
	cVec3 Pivot;
	bool PivotPresent;
	int NodeID;
	int ParentID;

};
//*****************************************************************************
// cSurface
//*****************************************************************************
struct cMtlDataChunk{
    DWORD Type;
    cList<char> Data;
};
class cSurface {
public:
	explicit cSurface(const char *Name = "");
	cSurface(const char *Name, const cColor &Ambient, const cColor &Diffuse, const cColor &Specular, const float Shininess);

    void CopyTo(cSurface& dest);
    void Store(comms::cFile& dest);
    void ReStore(comms::cFile& dest);
	
	cStr Name;

    cColor Color;
	cColor Ambient;
	cColor Diffuse;
	cColor Specular;
    cColor Gloss;

    float Luminosity;
    float Reflection;
    float Refraction;
    float Transparency;
    float Translucensy;
	float Shininess;
    float DepthModulator;

    bool  UseLimitAngle;
    float LimitAngle;
    cList<cMtlDataChunk> ExData;
	
	struct Map {
		Map();
		
		cStr FileName;
		int Unit;
		bool MipMap;
		int  id;
		cVec2 Offset;
		cVec2 Scale;
		cMat4 DepthTransform;
		
		bool Load();
	};

	Map Tex[8];
	
	void Load();
	
	static const cSurface Brass;
	static const cSurface Bronze;
	static const cSurface PolishedBronze;
	static const cSurface Chrome;
	static const cSurface Copper;
	static const cSurface PolishedCopper;
	static const cSurface Gold;
	static const cSurface PolishedGold;
	static const cSurface Pewter;
	static const cSurface Silver;
	static const cSurface PolishedSilver;
	static const cSurface Emerald;
	static const cSurface Jade;
	static const cSurface Obsidian;
	static const cSurface Pearl;
	static const cSurface Ruby;
	static const cSurface Turquoise;
	static const cSurface BlackPlastic;
	static const cSurface BlackRubber;
};
class cUVSet{
public:		
	cStr Name;
};
