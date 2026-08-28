#pragma once

#define maxclen 128
#define maxngon 256
typedef char chars[maxclen]; 

//*****************************************************************************
// cMeshObj
//*****************************************************************************
class cMeshObj : public cMeshCodec {
public:
	cMeshObj() : cMeshCodec() {TempPos=TempMaxPos=0;}
	cMeshContainer * Decode(cData &Fm);
	void Encode(cMeshContainer &Mesh, cData *To);
private:
	char Temp[1024];
	int TempPos;
	int TempMaxPos;
	char m_Buffer[2048];
	chars m_Tokens[maxngon+1];
	int nTokens;

	const char* ButFirstParam();

	bool ReadString(cData& Data,char* dest,int Len);
	bool DecodeObj(cData &Src, cMeshContainer &Mesh);
	int LoadLine(cData &Src, int line, cMeshContainer& Mesh);
	const cVec3i GetIndices(chars &Token); // iPosition, iTexCoord, iNormal
	bool AddFace(cMeshContainer &Mesh, const int idMtl, const int idObj);
	
	//***********************************************************************************
	// The "Obj" file tokens:
	//***********************************************************************************
	// # some text
	//		Line is a comment until the end of the line.
	//-----------------------------------------------------------------------------------
	// v float float float
	//		A single vertex's geometric position in space. The first vertex listed in the file
	//		has index 1, and subsequent vertices are numbered sequentially.
	//-----------------------------------------------------------------------------------
	// vn float float float
	//		A normal. The first normal in the file has index 1,
	//		and subsequent normals are numbered sequentially.
	//-----------------------------------------------------------------------------------
	// vt float float
	//		A texture coordinate. The first texture coordinate in the file has index 1,
	//		and subsequent textures are numbered sequentially.
	//-----------------------------------------------------------------------------------
	// f int int int ...
	//		or
	// f int/int int/int int/int ...
	//		or
	// f int/int/int int/int/int int/int/int ...
	//		A polygonal face. The numbers are indexes into the arrays of vertex positions,
	//		texture coordinates, and normals respectively. A number may be ommited if,
	//		for example, texture coordinates are not being defined in the model.
	//		There is no maximum number of vertices that a single polygon may contain.
	//		The .obj file specification says that each face must be flat and convex.
	//-----------------------------------------------------------------------------------
	// mtllib string
	//		Material file "filename.mtl" reference.
	//		Material files contain illumination variables and texture filenames.
	//-----------------------------------------------------------------------------------
	// usemtl string
	//		Material selection by name.
	//		Faces which are listed after this point in the file will use the selected material.
	//-----------------------------------------------------------------------------------

	//***********************************************************************************
	// The "Mtl" file tokens:
	//***********************************************************************************
	// newmtl string
	//		Begins a new named material description.
	//-----------------------------------------------------------------------------------
	// Ka float float float
	//		Ambient color.
	//-----------------------------------------------------------------------------------
	// Kd float float float
	//		Diffuse color.
	//-----------------------------------------------------------------------------------
	// Ks float float float
	//		Specular color.
	//-----------------------------------------------------------------------------------
	// d float Tr float
	//		Transparency.
	//-----------------------------------------------------------------------------------
	// Ns int
	//		Shininess (specular power).
	//-----------------------------------------------------------------------------------
	// illum int
	//		Illumination model: 1 - specular is disabled, 2 - specular is enabled.
	//-----------------------------------------------------------------------------------
	// map_Kd string
	//		Diffuse texture.
	//-----------------------------------------------------------------------------------
};
