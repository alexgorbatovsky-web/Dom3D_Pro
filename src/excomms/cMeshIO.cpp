#include "stdafx.h"
#include "tlimits.h"
bool AllowSwapYZ(const char* FileName);
bool CheckIfMedical();
bool CheckIfFileExists(const char* path);

#ifdef _WIN64
#define COLLADACODEC
#endif // _WIN64

#ifdef COLLADACODEC
#ifdef _DEBUG
#pragma comment(lib, "Libs/ColladaCodec/x64/Debug/ColladaCodec.lib")
#else // Release
#pragma comment(lib, "Libs/ColladaCodec/x64/Release/ColladaCodec.lib")
#endif // _DEBUG
comms::cMeshCodec* GetColladaCodec();
#endif // COLLADACODEC

namespace comms{

//#include "Share.h"
#include "cMeshObj.h"
#include "../3D-Coat/cMeshLwo.h"
#include "../3D-Coat/cMeshFbx.h"
#include "../3D-Coat/cMeshStl.h"
#include "../3D-Coat/cMeshPly.h"
#include "../3D-Coat/cMeshGltf.h"
#include "../3D-Coat/cMeshUsd.h"
#include "../3D-Coat/cMeshJges.h"
#include "../3D-Coat/cMeshBlend.h"

#include "cMeshVRML.h"

#ifdef _WIN64
#define COAT_TIN
#include "cMeshTin.h"
#endif

//#include "../3D-Coat/cMeshPtex.h"
//#include "..\common.h"

cList<cMeshIO::MeshCodecInfo> cMeshIO::m_MeshCodecs;

//-----------------------------------------------------------------------------
// cMeshIO::AddCodec
//-----------------------------------------------------------------------------
bool cMeshIO::AddCodec(const char *FileExtension, cMeshCodec *Codec) {
	Init();

	for(int i=0;i<m_MeshCodecs.Count();i++) {
		if(cStr::EqualsNoCase(FileExtension, m_MeshCodecs[i].FileExtension)) {
			return false;
		}
	}

	MeshCodecInfo Info;
	Info.FileExtension = FileExtension;
	Info.Codec = Codec;
	m_MeshCodecs.Add(Info);

	cLog::Message("cMeshIO::AddCodec(): Added \"%s\" mesh codec.", Info.FileExtension.ToCharPtr());
	return true;
} // cMeshIO::AddCodec

//-----------------------------------------------------------------------------
// cMeshIO::Init
//-----------------------------------------------------------------------------
typedef int tpGetNumMeshCodecs();
typedef cMeshCodec* tpGetMeshCodec(int idx);
typedef const char* tpGetMeshCodecName(int idx);
static void TestCorrectLocaleForUSDA() {
	std::ostringstream Stream;
	Stream << 1000;
	const cStr StreamContent = Stream.str().c_str();
	const cStr LocaleName = std::locale().name().c_str();
	if(StreamContent.Equals("1,000")) {
		cMessageBox::Ok("ERROR", "Indices in USDA(Z) will be grouped by comma: %s\nLocale name: %s", StreamContent.ToCharPtr(), LocaleName.ToCharPtr());
	}
}
void cMeshIO::Init() {
	static bool Inited = false;
	if(Inited) {
		return;
	}
	Inited = true;
	
#ifdef COMMS_WINDOWS
	FileList FL;
	CreateSortedFileList("data/Plugins\\MeshCodecs\\","*.dll",FL,false);
	for(int i=0;i<FL.GetAmount();i++){
		comms::cStr s=comms::cIO::EnsureAbsolutePath(FL[i].ToCharPtr());
		HMODULE H=LoadLibrary(s.ToCharPtr());
		DWORD E=GetLastError();
		if(H){
			tpGetNumMeshCodecs* num=(tpGetNumMeshCodecs*)GetProcAddress(H,"?GetNumMeshCodecs@@YAHXZ");
			tpGetMeshCodec* gmc=(tpGetMeshCodec*)GetProcAddress(H,"?GetMeshCodec@@YAPAVcMeshCodec@comms@@H@Z");
			tpGetMeshCodecName* mcn=(tpGetMeshCodecName*)GetProcAddress(H,"?GetMeshCodecName@@YAPBDH@Z");
			if(num && gmc && mcn){
				int n=num();
				for(int j=0;j<n;j++){
					AddCodec(mcn(j),gmc(j));	
				}
			}
		}
	}
#endif // COMMS_WINDOWS
	
	// Mesh codecs:
	bool med=CheckIfMedical();
	if(med)AddCodec("Stl", new cMeshStl);
	AddCodec("Obj", new cMeshObj);
    AddCodec("Lwo", new cMeshLwo);
#ifdef COMMS_FBX
	AddCodec("Fbx", new cMeshFbx);
#endif // COMMS_FBX
	AddCodec("igs", new cMeshJges);
	if(!med)AddCodec("Stl", new cMeshStl);
	AddCodec("Ply", new cMeshPly);
#ifdef COLLADACODEC
	AddCodec("dae",GetColladaCodec());
#endif // COLLADACODEC
	AddCodec("wrl",new cMeshVRML);
	AddCodec("gltf", new cMeshGltf);
	AddCodec("glb", new cMeshGltf);
#ifdef COAT_USD
	AddCodec("blend", new cMeshBlend);
	AddCodec("usd", new cMeshUsd);
	TestCorrectLocaleForUSDA();
	AddCodec("usda", new cMeshUsd);
	AddCodec("usdc", new cMeshUsd);
	AddCodec("usdz", new cMeshUsd);
#endif // COAT_USD
	//AddCodec("Ptx", new cMeshPtex);
#ifdef COAT_TIN
	AddCodec("tin", new cMeshTin); // binary
	AddCodec("ttin", new cMeshTin);// text ascii
#endif // COAT_TIN
} // cMeshIO::Init

//-----------------------------------------------------------------------------
// cMeshIO::LoadMesh
//-----------------------------------------------------------------------------
cMeshContainer * cMeshIO::LoadMesh(const char *_FileName) {
	Init();
	if(strstr(_FileName,";")){
		cMeshContainer* mm=new cMeshContainer;
		StringsList List;
		List.Split(_FileName,";");
		for(int i=0;i<List.Count();i++){
			cMeshContainer* mc=LoadMesh(List[i]);
			if(mc){
				mm->ConcateWith(mc);
				delete(mc);
			}
		}
		return mm;
	}
	if(NULL == _FileName) {
		cLog::Warning("cMeshIO::LoadMesh(): File name is NULL.");
		return NULL;
	}
	
	cStr S = cIO::EnsureAbsolutePath(_FileName);

	const cStr FileExtension = S.GetFileExtension();
	if(!FileExtension.Length()) {
		cLog::Warning("cMeshIO::LoadMesh(): File name \"%s\" has no extension.", S.ToCharPtr());
		return NULL;
	}

	// Load file:
	cData Src;
	Src.SetFilePn(S,false);
	if(Src.Size()){		
		cLog::Message("cMeshIO::LoadMesh(): Loaded mesh file \"%s\".", S.ToCharPtr());
		// Looking for codec:
		cMeshCodec *Codec = NULL;
		for(int i=0;i<m_MeshCodecs.Count(); i++){
			if(cStr::EqualsNoCase(m_MeshCodecs[i].FileExtension, FileExtension)) {
				Codec = m_MeshCodecs[i].Codec;
				break;
			}
		}
		if(!Codec) {
			cLog::Warning("cMeshIO::LoadMesh(): No codec for mesh file \"%s\".", S.ToCharPtr());
			return NULL;
		}

		// Decoding:
		cMeshContainer *Mesh = Codec->Decode(Src);	
		if (Mesh){
			//trying to find textures
			cStr s1 = _FileName;
			s1.RemoveFileName();
			s1.EnsureTrailingBackslash();
			for (int i = 0; i < Mesh->GetMaterials().Count(); i++){
				for (int j = 0; j < 8; j++){
					cStr s = Mesh->GetMaterials()[i].Tex[j].FileName;
					if (!s.IsEmpty()){
						int L = s.Length();
						if (L > 2 && s[0] == '.' && (s[1] == '\\' || s[1] == '/')){
							s.Remove(0, 2);
						}
						cStr s2 = s1;
						s2 += s;
						if (CheckIfFileExists(s2.ToCharPtr())){
							Mesh->GetMaterials()[i].Tex[j].FileName = s2;
						}
						else{
							s2 = s1;
							s2 += "Images\\";
							s2 += s;
							if (CheckIfFileExists(s2.ToCharPtr())){
								Mesh->GetMaterials()[i].Tex[j].FileName = s2;
							}
						}
					}
				}
			}
			//checking uv-sets
			if (Mesh && Mesh->GetUVSets().Count() == 0){
				for (int i = 0; i < Mesh->GetMaterials().Count(); i++){
					cUVSet uv;
					Mesh->GetUVSets().Add(uv);
					cUVSet& uvs = Mesh->GetUVSets().GetLast();
					uvs.Name = Mesh->GetMaterials()[i].Name;
				}
				for (int i = 0; i < Mesh->GetRaw().Count(); i++){
					cVec3i& c3 = Mesh->GetRaw()[i];
					c3[1] |= c3[1] << 16;
					i += c3[0];
				}
			}
			if (Mesh && m_SwapYZ && AllowSwapYZ(_FileName))SwapYZ(Mesh, false);
		}
		if (Mesh->ImportHook)Mesh->ImportHook(Mesh, _FileName);
		return Mesh;
	}else{		
		cLog::Warning("cMeshIO::LoadMesh(): Can't open file \"%s\".", S.ToCharPtr());
		return NULL;		
	}
} // cMeshIO::LoadMesh

//-----------------------------------------------------------------------------
// cMeshIO::SaveMesh
//-----------------------------------------------------------------------------
bool cMeshIO::SaveMesh(cMeshContainer &Mesh, const char *FileName) {
	Init();
	if(m_SwapYZ && AllowSwapYZ(FileName))SwapYZ(&Mesh,true);

	if(NULL == FileName) {
		cLog::Warning("cMeshIO::SaveMesh(): File name is NULL.");
		return false;
   	}
	const cStr FileExtension = cStr(FileName).GetFileExtension();
	if(!FileExtension.Length()) {
		cLog::Warning("cMeshIO::SaveMesh(): File name \"%s\" has no extension.", FileName);
		return false;
	}

	// Looking for codec:
	cMeshCodec *Codec = NULL;
	for(int i=0;i<m_MeshCodecs.Count();i++) {
		if(cStr::EqualsNoCase(m_MeshCodecs[i].FileExtension, FileExtension)) {
			Codec = m_MeshCodecs[i].Codec;
			break;
		}
	}
	if(!Codec) {
		cLog::Warning("cMeshIO::SaveMesh(): No codec for mesh file \"%s\".", FileName);
		return false;
	}

	// Encoding:
	cData File;
    File.SetFilePn(FileName,true);
	Mesh.RemoveUnusedObjMtl();
	if (Mesh.ExportHook)Mesh.ExportHook(&Mesh, FileName);
	cMeshContainer* m2 = li_mesh(Mesh);
	if (m2) {
		Codec->Encode(*m2, &File);
		delete(m2);
	}
	else {
		Codec->Encode(Mesh, &File);
	}
	if(m_SwapYZ)SwapYZ(&Mesh,false);
	//return cIO::SaveFile(FileName,File);
	return true;
} // cMeshIO::SaveMesh

bool cData::IsEof(){
	if(Reader)return Reader->GetReadPos()>=Reader->Size();
	return true;
}
size_t cData::Size(){
	if(Reader)return Reader->Size();
	if(Writer)return Writer->Size();
	return 0; 
}
int cData::GetPos(){
	if(Reader)return Reader->GetReadPos();
	if(Writer)return Writer->GetWritePos();
	return 0; 
}
int cData::SeekCur(const int Offset){
	if(Reader){
		int ofs=Reader->GetReadPos()+Offset;
		if(Offset>0)Reader->Skip(Offset);
		else Reader->SetReadPos(ofs);
		return ofs;
	}
	if(Writer){
		int ofs=Writer->GetWritePos()+Offset;
		Writer->SetWritePos(ofs);
		return ofs;
	}
	return 0;
}
int cData::SetPos(const int Pos){
	if(Reader)Reader->SetReadPos(Pos);
	if(Writer)Writer->SetWritePos(Pos);
	return Pos;
}
int cData::Read(void *To, const int MaxSize){
	if(Reader){
		int sz=Reader->Read(To,MaxSize);
		if(IsEof())return sz;
		else return MaxSize;
	}
	return 0;
}
void cData::Write(const void *Fm, const int Size){
	if(Writer)Writer->Write((void*)Fm,Size); 
}	
	
byte cData::ReadByte(){
	return Reader?Reader->ReadBYTE():0;
}
bool cData::ReadByte(byte *b){
	*b=Reader?Reader->ReadBYTE():0;
	return true;
}
void cData::WriteByte(const byte b){
	if(Writer)Writer->WriteBYTE(b);
}
short cData::ReadShort(){
	return Reader?Reader->ReadWORD():0;
}
bool cData::ReadShort(short *s){
	*s=Reader?Reader->ReadWORD():0;
	return true;
}
void cData::WriteShort(const short s){
	if(Writer)Writer->WriteWORD(s);
}

word cData::ReadWord(){
	return Reader?Reader->ReadWORD():0;
}
bool cData::ReadWord(word *w){
	*w=Reader?Reader->ReadWORD():0;
	return true;
}
void cData::WriteWord(const word w){
	if(Writer)Writer->WriteWORD(w);
}

int  cData::ReadInt(){
	return Reader?Reader->ReadDWORD():0;
}
bool cData::ReadInt(int *i){
	*i=Reader?Reader->ReadDWORD():0;
	return true;
}
void cData::WriteInt(const int i){
	if(Writer)Writer->WriteDWORD(i);
}

dword cData::ReadDword(){
	return Reader?Reader->ReadDWORD():0;
}
bool cData::ReadDword(dword *dw){
	*dw=Reader?Reader->ReadDWORD():false;
	return true;
}
void cData::WriteDword(const dword dw){
	if(Writer)Writer->WriteDWORD(dw);
}

float cData::ReadFloat(){
	float f=0;
	if(Reader)Reader->Read(&f,4);
	return f;
}
bool cData::ReadFloat(float *f){
	return Reader?Reader->Read(f,4)==4:false;
}
void cData::WriteFloat(float f){
	if(Writer)Writer->Write(&f,4);
}

double cData::ReadDouble(){
	double d=0;
	if(Reader)Reader->Read(&d,8);
	return d;
}
bool cData::ReadDouble(double *d){
	return Reader?Reader->Read(d,8)==8:false;
}
void cData::WriteDouble(double d){
	if(Writer)Writer->Write(&d,8);
}	
bool cData::ReadString(cStr *S, const char *Terminators,int maxlen){
	cAssert(S != NULL);
	cAssert(Terminators != NULL);

	S->Clear();
	bool start=false;
	do{
		char c;
		if(Read(&c,1)!=1){
			return S->Length()!=0;
		}
		if(strchr(Terminators,c)){
			if(start)break;
		}else{
			*S+=c;
			start=true;
		}
		maxlen--;
	}while(true && maxlen!=0);
	return true;
}
void cData::WriteString(const char * S){
	Write(S,strlen(S));
}
void cData::SetFilePn(const char *FilePn, bool ForWrite){
	m_FilePn = FilePn; 
	if(ForWrite)Writer=new FILE_WriteBinStream(FilePn);
	else Reader=new FILE_ReadBinStream(FilePn);
}
cData::~cData(){
	Clear();
}
void cData::Clear(){
	if(Writer)delete(Writer);
	if(Reader)delete(Reader);
	Reader=NULL;
	Writer=NULL;
}
bool cMeshIO::m_SwapYZ=false;
void cMeshIO::SwapYZ(cMeshContainer* M,bool Export){
	Export=!Export;
	auto& VL=M->GetPositions();
	for(int i=0;i<VL.Count();i++){
		if(Export){			
			::std::swap(VL[i].z,VL[i].y);
			::std::swap(VL[i].z,VL[i].x);
		}else{
			::std::swap(VL[i].z,VL[i].y);
			::std::swap(VL[i].y,VL[i].x);
		}
	}
	int no=M->GetObjects().Count();
	for(int i=0;i<no;i++){
		Vector3D& pp=M->GetObjects()[i].Pivot;
		if(Export){			
			::std::swap(pp.z,pp.y);
			::std::swap(pp.z,pp.x);
		}else{
			::std::swap(pp.z,pp.y);
			::std::swap(pp.y,pp.x);
		}
	}
	auto& VN=M->GetNormals();
	for(int i=0;i<VN.Count();i++){
		if(Export){
			::std::swap(VN[i].z,VN[i].y);
			::std::swap(VN[i].z,VN[i].x);
		}else{
			::std::swap(VN[i].z,VN[i].y);
			::std::swap(VN[i].y,VN[i].x);
		}
	}
}

}
