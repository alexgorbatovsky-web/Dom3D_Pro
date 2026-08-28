#include "stdafx.h"

bool PreserveVertsOrder=false;
bool RawVertsOrder();

void ShowGlobalProgress(const char* msg,int pos,int maxv);
char* _tempchar();
void DoCommas();
int repcom2=2;
void DoCommas2(char* buf){
	if(repcom2==2){
		char cc[20];
		float a=1.2345;
		sprintf(cc,"%f",a);
		if(strchr(cc,','))repcom2=1;
		else repcom2=0;
	}
	for(int p=0;p<256 && buf[p];p++){
		if(buf[p]=='.' || buf[p]==','){
			buf[p]=repcom2==1? ',' : '.';
		}
	}
}
char* Write4Floats(float v1,float v2,float v3,float v4){
	char* tempchar = _tempchar();
	sprintf(tempchar,"%f %f %f %f",v1,v2,v3,v4);
	DoCommas();
	return tempchar;
}
char* Write3Floats(float v1,float v2,float v3){
	char* tempchar = _tempchar();
	sprintf(tempchar,"%f %f %f",v1,v2,v3);
	DoCommas();
	return tempchar;
}
char* Write2Floats(float v1,float v2){
	char* tempchar = _tempchar();
	sprintf(tempchar,"%f %f",v1,v2);
	DoCommas();
	return tempchar;
}

char* FormatFloat(const float v1, const int Prec) {
	char* tempchar = _tempchar();
	char buf[32];
	sprintf(buf, "%%.%df", Prec);
	sprintf(tempchar, buf, v1);
	DoCommas();
	return tempchar;
}


namespace comms{
#include "cMeshObj.h"
//-----------------------------------------------------------------------------
// cMeshObj::Decode
//-----------------------------------------------------------------------------
cMeshContainer * cMeshObj::Decode(cData &Src) {
	if(Src.IsEof()) {
		return NULL;
	}
	cMeshContainer *pMesh = new cMeshContainer;
	pMesh->SetName(Src.GetFilePn());
	
	if(!DecodeObj(Src, *pMesh)) {
		delete pMesh;
		pMesh = NULL;
	}
	return pMesh;
} // cMeshObj::Decode

//-----------------------------------------------------------------------------
// cMeshObj::Encode
//-----------------------------------------------------------------------------
const int n_mtl_tags=5;
char* mtl_tags[n_mtl_tags]={"map_Kd ","map_Ks ","bump ","map_bump ","disp "};
int mtl_idx[n_mtl_tags]={0,1,2,3,2};

float sfloat(float x){
	if(comms::cMath::IsValid(x))return x;
	return 0;
}

void CalcSmoothingGroups(cList<int>& Groups,comms::cMeshContainer& mc){
	auto& raw=mc.GetRaw();
	auto& nrm=mc.GetNormals();
	if(nrm.Count()){
		uni_hash<int,bi_DWORD> fone;
		fone.set_table_size(nrm.Count()+1);
		Groups.Clear();
		Groups.Add(-2,raw.Count());
		for(int i=0,p=0;i<raw.Count();i++,p++){
			int n=raw[i][0];
			Groups[i]=-1;
			for(int j=0;j<n;j++){
				int jn=(j+1)%n;
				int n1=raw[i+j+1][2];
				int n2=raw[i+jn+1][2];
				if(n1!=-1 && n2!=-1){
					bi_DWORD b(n1,n2);
					fone.add_quick(b,i);
				}
			}
			i+=n;
		}
		int g=1;
		cList<int> order;
		for(int i=0;i<raw.Count();i++){
			int n=raw[i][0];
			if(Groups[i]==-1){
				order.Clear();
				order.Add(i);
				for(int k=0;k<order.Count();k++){
					int f=order[k];
					Groups[f]=g;
					int nn=raw[f][0];
					for(int j=0;j<nn;j++){
						int jn=(j+1)%nn;
						int n1=raw[f+j+1][2];
						int n2=raw[f+jn+1][2];
						if(n1!=-1 && n2!=-1){
							bi_DWORD b(n2,n1);
							scan_key(fone,b,int* pf){
								if(Groups[*pf]==-1){
									Groups[*pf]=g;
									order.Add(*pf);
								}
							}scan_end;
						}
					}
				}
				g++;
			}
			i+=n;
		}
		for(int i=0;i<raw.Count();i++){
			int n=raw[i][0];
			if(Groups[i]==-1){
				Groups[i]=g;
			}
			i+=n;
		}
		g++;
		cList<cVec3i>* gs;
		gs=new cList<cVec3i>[g];
		for(int i=0;i<raw.Count();i++){
			int n=raw[i][0];
			auto& cg=gs[Groups[i]];
			for(int k=0;k<=n;k++)cg.Add(raw[i+k]);
			i+=n;
		}
		raw.Clear();
		for(int i=0;i<g;i++){
			int ng=gs[i].Count();
			for(int p=0;p<ng;p++){
				int nn=gs[i][p][0];
				int rp=raw.Add(gs[i][p]);
				Groups[rp]=i;
				for(int k=0;k<nn;k++)raw.Add(gs[i][p+k+1]);
				p+=nn;
			}
		}
		for(int j=0;j<g;j++){
			gs[j].Free();
		}
		delete[]gs;
	}
}
void cMeshObj::Encode(cMeshContainer &Mesh, cData *To) {
#ifdef COMMS_WINDOWS
	DBGCHECK();
#endif // COMMS_WINDOWS
	cStr corepath = To->GetFilePn();
	corepath.RemoveFileName();
	if (corepath.Length() > 0)corepath.EnsureTrailingBackslash();
	int coreL = corepath.Length();
	cAssert(To);
	if(!To) {
		return;
	}
	if(Mesh.FirstLineComment.Length()) {
		To->WriteString(Mesh.FirstLineComment);
		To->WriteString("\r\n");
	}
	To->WriteString("# Wavefront Obj file\r\n");
	To->WriteString(cStr::Format("# %s\r\n", Mesh.GetName().ToCharPtr()));
    To->WriteString(cStr::Format("# verts: %d uv-s: %d\r\n",Mesh.GetPositions().Count(),Mesh.GetTexCoords().Count()));

    cList<cSurface>& mtls=Mesh.GetMaterials();
	for(int i=0;i<mtls.Count();i++){
		mtls[i].Name.Replace(' ','_');
	}
    const cList<cObject>& objs=Mesh.GetObjects();

    char dir	[_MAX_PATH];
	char drive	[_MAX_PATH];
	char file	[_MAX_PATH];
	char ext	[_MAX_PATH];
	_splitpath( To->GetFilePn(), drive, dir, file, ext );

	char mtl[_MAX_PATH];
    char NewName[_MAX_PATH];
    sprintf(NewName,"%s.mtl",file);

	sprintf(mtl,"%s%s%s.mtl",drive,dir,file);
    
    FILE* M=OpenFILE(mtl,"w");
	if(M){
        for(int i=0;i<mtls.Count();i++){
            const cSurface& mt=mtls[i];
            char nname[_MAX_PATH];
			fprintf(M,"newmtl %s\nNs 100.000\nd 1.00000\nillum 2\nKd 1.00000 1.00000 1.00000\nKa 0.00000 0.00000 0.00000\nKs 1.00000 1.00000 1.00000\nKe 0.00000e+0 0.00000e+0 0.00000e+0\n",(const char*)mt.Name);
			for(int j=0;j<4;j++){
				cStr fnm = (const char*)mt.Tex[j].FileName;
				cStr s0 = fnm;
				if (s0.Length() > coreL){
					s0.Remove(coreL);
					if (fnm.EqualsPath(s0, corepath)){
						fnm.Remove(0, coreL);
					}
				}
				strcpy(nname,fnm.ToCharPtr());
				int L=strlen(nname);
				if(L>0){
					fprintf(M,"%s%s\n",mtl_tags[j],nname);
				}
			}
        }        
    	CloseFILE(M);
	}	

    To->WriteString(cStr::Format("mtllib %s\r\n",NewName));

	cList<int> pos_Obj;
	cList<int> n_Obj;
	cList<int> uv_Obj;	

	pos_Obj.Add(10000000,Mesh.GetPositions().Count());
	n_Obj.Add(10000000,Mesh.GetNormals().Count());
	uv_Obj.Add(10000000,Mesh.GetTexCoords().Count());

	bool NoTexCoords = Mesh.GetTexCoords().Count() < 1;
	bool NoNormals = Mesh.GetNormals().Count() < 1;
	int index=0;

	bool raword = RawVertsOrder();
	
	while(index < Mesh.GetRaw().Count()) {        
		const cVec3i &Info = Mesh.GetRaw()[index++];        
		int nDeg = Info[0];
		const int idMtl = Info[1];
        const int idObj = Info[2];
		while(nDeg--) {
			const cVec3i i0 = Mesh.GetRaw()[index++];
			if (pos_Obj[i0[0]]>idObj)pos_Obj[i0[0]] = raword ? 0 : idObj;
			if(i0[1]>=0 && !NoTexCoords){
				if (uv_Obj[i0[1]]>idObj)uv_Obj[i0[1]] = raword ? 0 : idObj;
			}
			if(!NoNormals) {
				if(i0[2]>=0){
					if (n_Obj[i0[2]]>idObj)n_Obj[i0[2]] = raword ? 0 : idObj;
				}else NoNormals=true;
			}
		}
	}	
	// Raw:
	index = 0;
	cStr Face;
	int NFaces = 0;	
    int curmtl=-1;
    int curobj=-1;

	int np=Mesh.GetPositions().Count();
	int nn=Mesh.GetNormals().Count();
	int nuv=Mesh.GetTexCoords().Count();	

	cList<int> posIds;
	posIds.Add(-1,np);
	cList<int> nIds;
	nIds.Add(0,nn);
	cList<int> uvIds;
	uvIds.Add(0,nuv);	

	int cnv=0;
	int cnn=0;
	int cnuv=0;
	const auto& poss=Mesh.GetPositions();
	const auto& vcl=Mesh.GetVertexColor();
	const auto& vsp=Mesh.GetVertexSpecular();
	const auto& vem=Mesh.GetVertexEmissive();
	const auto& uvs=Mesh.GetTexCoords();
	const auto& ns=Mesh.GetNormals();
	bool HaveGroups=false;
	cList<int> Groups;
	if(ns.Count() && ns.Count()>poss.Count()){
		CalcSmoothingGroups(Groups,Mesh);
		HaveGroups=true;
	}
	UnlimitedBitset usedverts;
	int prevp = 0;
	for(int curobj=0;curobj<Mesh.GetObjects().Count();curobj++){	
		int curG=-1;
		To->WriteString(cStr::Format("\r\ng %s\r\n",(const char*)objs[curobj].Name));
		if(objs[curobj].PivotPresent){
			Vector3D p=objs[curobj].Pivot;
			To->WriteString(cStr::Format("#pivot %s\r\n",Write3Floats(p.x,p.y,p.z)));
		}
		curmtl=-1;
		index=0;
		// Positions:
		int nc=0;
		if (PreserveVertsOrder){
			int npidx = 0;
			for (int i = 0; i < np; i++)if (pos_Obj[i] == curobj){
				npidx = i + 1;
			}
			for (int i = prevp; i < npidx; i++){
				const cVec3* It = &poss[i];
				To->WriteString(cStr::Format("\r\nv %s", Write3Floats(sfloat(It->x), sfloat(It->y), sfloat(It->z))));
				posIds[i] = cnv++;
				nc++;
			}
			prevp = npidx;
		}
		else{
			for (int i = 0; i < np; i++)if (pos_Obj[i] == curobj){
				const cVec3* It = &poss[i];
				To->WriteString(cStr::Format("\r\nv %s", Write3Floats(sfloat(It->x), sfloat(It->y), sfloat(It->z))));
				posIds[i] = cnv++;
				nc++;
			}
		}
		To->WriteString(cStr::Format("\r\n# %d positions\r\n", nc));
		if(vcl.Count()==poss.Count()){
			int LL=0;
			int pp=0;
			for(int i=0;i<np;i++)if(pos_Obj[i]==curobj){
				DWORD C=vcl[i];
				if(pp==0){
					To->WriteString("# The following MRGB block contains Vertex Color written by 3D-Coat\r\n");
					To->WriteString("# mrgb data provided to keep compatibility with xNormal (www.xnormal.net) and ZBrush -\r\n");
					To->WriteString("# www.zbrush.com\r\n");
				}
				if(LL==0)To->WriteString("#MRGB ");
				To->WriteString(cStr::Format("%08x",C));
				LL++;
				pp++;
				if(LL>=16){
					To->WriteString("\r\n");
					LL=0;
				}
			}
			if(LL)To->WriteString("\r\n# End of MRGB block\r\n");
		}
		if(vsp.Count()==poss.Count()){
			int LL=0;
			int pp=0;
			for(int i=0;i<np;i++)if(pos_Obj[i]==curobj){
				DWORD C=vsp[i];
				if(pp==0){
					To->WriteString("# The following SPEC block contains Vertex Specular Color written by 3D-Coat\r\n");
					To->WriteString("# Format of DWORDS are - Gloss|Rs|Gs|Bs - 2 characters per channel, Rs,Gs,Bs - color of specularity\r\n");
				}
				if(LL==0)To->WriteString("#SPEC ");
				To->WriteString(cStr::Format("%08x",C));
				LL++;
				pp++;
				if(LL>=16){
					To->WriteString("\r\n");
					LL=0;
				}
			}
			if(LL)To->WriteString("\r\n# End of SPEC block\r\n");
		}
		if(vem.Count()==poss.Count()){
			int LL=0;
			int pp=0;
			for(int i=0;i<np;i++)if(pos_Obj[i]==curobj){
				DWORD C=vem[i];
				if(pp==0){
					To->WriteString("# The following EMIS block contains Vertex emissive Color written by 3D-Coat\r\n");
					To->WriteString("# Format of DWORDS are - EmissiveDegree|Re|Ge|Be - 2 characters per channel, Re,Ge,Be - color of emissive\r\n");
				}
				if(LL==0)To->WriteString("#EMIS ");
				To->WriteString(cStr::Format("%08x",C));
				LL++;
				pp++;
				if(LL>=16){
					To->WriteString("\r\n");
					LL=0;
				}
			}
			if(LL)To->WriteString("\r\n# End of EMIS block\r\n");
		}

		// TexCoords:
		nc=0;		
		for(int i=0;i<nuv;i++)if(uv_Obj[i]==curobj){			
			const cVec2* It=&uvs[i];			
			To->WriteString(cStr::Format("\r\nvt %s", Write2Floats(sfloat(It->x),sfloat(It->y))));
			uvIds[i]=cnuv++;
			nc++;			
		}		
		To->WriteString(cStr::Format("\r\n# %d texture coordinates\r\n", nc));

		// Normals:
		nc=0;
		for(int i=0;i<nn;i++)if(n_Obj[i]==curobj){
			const cVec3* It=&ns[i];
			cVec3 nn=*It;
			if(nn.Length()==0)nn=Vector3D::AxisY;
			To->WriteString(cStr::Format("\r\nvn %s", Write3Floats(sfloat(nn.x), sfloat(nn.y), sfloat(nn.z))));
			nIds[i]=cnn++;
			nc++;
		}
		To->WriteString(cStr::Format("\r\n# %d normals\r\n", nc));		
		while(index < Mesh.GetRaw().Count()) {
			const cVec3i &Info = Mesh.GetRaw()[index++];        
			int nDeg = Info[0];
			const int idMtl = Info[1]&0xFFFF;
			const int idObj = Info[2];
			if(curobj==idObj){
				if(curmtl!=idMtl){
					curmtl=idMtl;
					if(curmtl!=-1){
						To->WriteString(cStr::Format("\r\nusemtl %s\r\n",(const char*)mtls[curmtl].Name));                
					}
				}
				if(HaveGroups){
					int g=index-1 < Groups.Count() ? Groups[index-1] : curG;
					if(g!=curG){
						To->WriteString(cStr::Format("s %d\r\n", g));
						curG=g;
					}
				}
				Face = "f";		
				while(nDeg--) {
					const cVec3i i0 = Mesh.GetRaw()[index++];
					if(NoTexCoords && NoNormals) {
						Face << cStr::Format(" %d", posIds[i0[0]] + 1); // Only position.
					} else if(NoTexCoords && !NoNormals) {
						Face << cStr::Format(" %d//%d", posIds[i0[0]] + 1, nIds[i0[2]] + 1); // Position && Normal.
					} else if(!NoTexCoords && NoNormals) {
						if(i0[1]!=-1){
							Face << cStr::Format(" %d/%d", posIds[i0[0]] + 1, uvIds[i0[1]] + 1); // Position && TexCoord.
						}else{
							Face << cStr::Format(" %d", posIds[i0[0]] + 1); // Only position.
						}
					} else {
						if(i0[1]==-1)Face << cStr::Format(" %d//%d", posIds[i0[0]] + 1, nIds[i0[2]] + 1);
						else Face << cStr::Format(" %d/%d/%d", posIds[i0[0]] + 1, uvIds[i0[1]] + 1, nIds[i0[2]] + 1); // Position, TexCoord, and Normal.
					}
				}
				Face << "\r\n";
				To->WriteString(Face);
				NFaces++;
			}else index+=nDeg;
		}				
	}
	To->WriteString(cStr::Format("\r\n# %d faces\r\n", NFaces));

	cList<OneMorph*> Morphs1;
	Morphs1.Copy(Mesh.Morphs);
	((cMeshContainer&)Mesh).Morphs.Clear();
	for(int i=0;i<Morphs1.Count();i++){
		Morphs1[i]->Pos.Count();
		for(int j=0;j<nn;j++){
			((cMeshContainer&)Mesh).GetPositions()[j]+=Morphs1[i]->Pos[j];
		}
		char cc[512];
		strcpy(cc,To->GetFilePn());
		if(strlen(cc)>5)cc[strlen(cc)-4]=0;
		strcat(cc,"_");
		strcat(cc,Morphs1[i]->Name);
		strcat(cc,".obj");
		cData D1;
		D1.SetFilePn(cc,true);
		Encode(Mesh,&D1);		
		//cIO::SaveFile(cc,D1);
		for(int j=0;j<nn;j++){
			((cMeshContainer&)Mesh).GetPositions()[j]-=Morphs1[i]->Pos[j];
		}
		delete(Morphs1[i]);
	}
#ifdef COMMS_WINDOWS
	DBGCHECK();
#endif // COMMS_WINDOWS
	//MT_ChunksWriter::EndAll();
} // cMeshObj::Encode
typedef char PathArray[4][MAX_PATH];
bool GetTextureFromMtl(const char* obj_name,const char* MtlLibName,const char* MtlName,PathArray& TexName,bool FullName){
	char dir	[_MAX_PATH];
	char drive	[_MAX_PATH];
	char file	[_MAX_PATH];
	char ext	[_MAX_PATH];
	_splitpath( obj_name, drive, dir, file, ext );
	char newn	[_MAX_PATH];
	sprintf(newn,"%s%s%s",drive,dir,MtlLibName);
	FILE* F=OpenFILE(newn,"rb");
	if(F){
		fseek(F,0,SEEK_END);
		int sz=ftell(F);
		fseek(F,0,SEEK_SET);
		char* s=new char[sz+1];
		s[sz]=0;
		fread(s,sz,1,F);
		CloseFILE(F);
		char mn[256];
		sprintf(mn,"newmtl %s",MtlName);
		char* s2=strstr(s,mn);
		if(s2){
			s2+=strlen(mn);
			char* s20=s2;
			char* s21=strstr(s2,"newmtl");
			for(int j=0;j<n_mtl_tags;j++){
				int tx=mtl_idx[j];
				if(s20)s2=strstr(s20,mtl_tags[j]);
				if(s2 && (int(s21-s2)>0 || s21==NULL) && s2[-1]!='_'){
					s2+=strlen(mtl_tags[j]);
					char* s3=strchr(s2,0x0D);
					if(!s3)s3=strchr(s2,0x0A);
					if (!s3)s3 = s2+strlen(s2);
					if(s3){
						int L=s3-s2;
						//if(FullName){
						//	sprintf(TexName,"%s%s",drive,dir);
						//}else TexName[0]=0;
						TexName[tx][0]=0;
						int L1=strlen(TexName[tx]);
						memcpy(TexName[tx]+L1,s2,L);
						TexName[tx][L+L1]=0;					
						comms::cStr s=TexName[tx];
						comms::cStr FName=s.GetFileName();
						FILE* TestF=fopen(TexName[tx],"rb");
						if((comms::cStr::Equals(s,FName) || TestF==NULL) && s.IndexOf("-bm ")==-1){
							sprintf(TexName[tx],"%s%s%s",drive, dir,FName.ToCharPtr());
							if(!CheckIfFileExists(TexName[tx])){
								strcpy(TexName[tx],s.ToCharPtr());
							}
						}
						if(TestF)fclose(TestF);
						//return true;
					}
				}
			}
		}
	}
	return false;
}
class hVec3:public cVec3{//3 floats-s	
public:
	hVec3(){};
	hVec3(float a,float b,float c){
		x=a;
		y=b;
		z=c;
	}
	operator DWORD(){
		return *((DWORD*)&x)+*((DWORD*)&y)+*((DWORD*)&z);
	}	
	bool   operator == (const hVec3& c) const{
		return c.x==x && c.y==y && c.z==z;
	}
};
//-----------------------------------------------------------------------------
// cMeshObj::DecodeObj
//-----------------------------------------------------------------------------
bool cMeshObj::DecodeObj(cData &Src, cMeshContainer &Mesh) {
	const cSurface *DefMtls[] = {
		&cSurface::Brass,
		&cSurface::Chrome,
		&cSurface::Emerald,
		&cSurface::Copper,
		&cSurface::Pewter,
		&cSurface::Jade,
		&cSurface::Pearl
	};	
	int idCurMtl = 0;
    int CurObject=-1;
	uni_hash<DWORD,hVec3,800000,8192> PosHash;
	cList<DWORD> PosID;

	cStr MaterialFileName;
	int pp=0;
	int line = 0;
	while(true) {
		if(Src.Size()>8000000){
			if(Src.GetPos()-pp>2000000){
				ShowGlobalProgress("READING_OBJ",Src.GetPos()>>8,Src.Size()>>8);
				pp=Src.GetPos();
			}
		}
		const int r = LoadLine(Src, line++, Mesh);
		if(-1 == r) { // EOF
			break;
		}
		if(0 == r) { // Comment or empty string.
			continue;
		}
		if(cStr::Equals(m_Tokens[0],"#MRGB")) {
			const char* s=strstr(m_Buffer," ");
			if(s){
				while(s[0]==' ' && s[0])s++;
				int L=strlen(s);
				int ne=L/8;
				for(int i=0;i<ne;i++){
					char cc[9];
					cc[8]=0;
					memcpy(cc,s+i*8,8);
					DWORD D=0xFF808080;
					sscanf(cc,"%X",&D);
					Mesh.GetVertexColor().Add(D);
				}
			}
		}
		if(cStr::Equals(m_Tokens[0],"#pivot")) {
			if(nTokens==4){
				float x=ToFloat(m_Tokens[1]);
				float y=ToFloat(m_Tokens[2]);
				float z=ToFloat(m_Tokens[3]);
				Mesh.GetObjects()[CurObject].Pivot=Vector3D(x,y,z);
				Mesh.GetObjects()[CurObject].PivotPresent=true;
			}
		}
		if(nTokens>=21 && cStr::Equals(m_Tokens[0],"#") && cStr::Equals(m_Tokens[1],"matrix4x4")) {
			cStr s1=m_Tokens[2];
			cStr s2=m_Tokens[4];
			s1.Trim("\"");
			s2.Trim("\"");
			CurObject=Mesh.GetObjects().Count();
			comms::cObject ob;
			Mesh.GetObjects().Add(ob);
			comms::cObject& obj=Mesh.GetObjects().GetLast();
			obj.Name="NewInstance|";
			obj.Name+=s2;
			obj.Name+="|";
			obj.Name+=s1;
			float* fp=obj.Transform.ToFloatPtr();
			for(int j=15;j>=0;j--){
				fp[j] = ToFloat(m_Tokens[20 + j - 15]);
			}
		}else
        if((cStr::Equals(m_Tokens[0], "g") || cStr::Equals(m_Tokens[0], "o")) && nTokens>1) {
			PosHash.reset();
            bool IsFound = false;
			cStr nm = ButFirstParam();
			for(int idObj = 0; idObj < Mesh.GetObjects().Count(); idObj++) {
				if(cStr::Equals(Mesh.GetObjects()[idObj].Name, nm)) {
					IsFound = true;
					CurObject = idObj;
					break;
				}
			}
            if(!IsFound) {
                cObject obj;
                CurObject=Mesh.GetObjects().Count();
                Mesh.GetObjects().Add(obj);
                Mesh.GetObjects().GetLast().Name=nm;
            }
        }else
		if(cStr::Equals(m_Tokens[0], "mtllib")) {
			MaterialFileName = ButFirstParam();
		} else if(cStr::Equals(m_Tokens[0], "v")) { // Position:
			if(nTokens < 4) {
				cLog::Warning("cMeshObj::DecodeObj(): Too few position coordinates: \"%s\".", m_Buffer);
				return false;
			}
			cVec3 Position;
			//cStr::ToFloat(m_Tokens[1],&Position.x);
			Position.x=ToFloat(m_Tokens[1]);
			//cStr::ToFloat(m_Tokens[2],&Position.y);
			Position.y=ToFloat(m_Tokens[2]);
			//cStr::ToFloat(m_Tokens[3],&Position.z);
			Position.z=ToFloat(m_Tokens[3]);
			//hVec3 v2(Position.x,Position.y,Position.z);
			//DWORD* id=PosHash.get(v2);
			//if(id==NULL){				
				DWORD p=Mesh.GetPositions().Add(Position);
				//PosID.Add(p);
				//PosHash.add(v2,p);								
			//}else{
			//	PosID.Add(*id);
			//}			
		} else if(cStr::Equals(m_Tokens[0], "vt")) { // TexCoord:
			if(nTokens < 3) {
				cLog::Warning("cMeshObj::DecodeObj(): Too few texture coordinates: \"%s\".", m_Buffer);
				return false;
			}
			cVec2 TexCoord;
			//cStr::ToFloat(m_Tokens[1],&TexCoord.x);
			TexCoord.x=ToFloat(m_Tokens[1]);
			//cStr::ToFloat(m_Tokens[2],&TexCoord.y);
			TexCoord.y=ToFloat(m_Tokens[2]);
			Mesh.GetTexCoords().Add(TexCoord);
		} else if(cStr::Equals(m_Tokens[0], "vn")) { // Normal:
			if(nTokens < 4) {
				cLog::Warning("cMeshObj::DecodeObj(): Too few normal coordinates: \"%s\".", m_Buffer);
				return false;
			}
			cVec3 Normal;
			Normal.x=ToFloat(m_Tokens[1]);
			Normal.y=ToFloat(m_Tokens[2]);
			Normal.z=ToFloat(m_Tokens[3]);
			Mesh.GetNormals().Add(Normal);
		} else if(cStr::Equals(m_Tokens[0], "f")) { // Face:
			//if(m_Tokens.Count() < 4) {
			//	cLog::Warning("cMeshObj::DecodeObj(): Too few face indices: \"%s\".", m_Buffer.ToCharPtr());
			//	return false;
			//}
			if(nTokens >=4) {
				if(CurObject==-1) {
					cObject obj;
					CurObject=0;
					Mesh.GetObjects().Add(obj);
					Mesh.GetObjects().GetLast().Name="default";
				}
				if(!AddFace(Mesh, idCurMtl, CurObject)) {
					return false;
				}
			}
		} else if(cStr::Equals(m_Tokens[0], "usemtl")) { // Material:
			if(nTokens > 1) { // There is a name token.				
				auto addmtl = [&](const char* mname) {
					cSurface m;
					Mesh.GetMaterials().Add(m);
					Mesh.GetMaterials().GetLast().Name = mname;
					PathArray TexName = { "","","","" };
					GetTextureFromMtl(Src.GetFilePn(), MaterialFileName, mname, TexName, true);
					for (int j = 0; j < 4; j++) {
						Mesh.GetMaterials().GetLast().Tex[j].FileName = TexName[j];
					}
				};
				
				if (Mesh.GetMaterials().Count() == 0 && MaterialFileName.Length()) {
					cStr mlib;
					cStr fp = Src.GetFilePn().GetFilePath();
					fp.EnsureTrailingSlash();
					StrFromFile(mlib, fp + MaterialFileName);
					int p = 0;
					do {
						int p1 = mlib.IndexOf("newmtl", p);
						if (p1 == -1)break;
						p = p1 + 6;
						int p2 = mlib.IndexOf("\n", p1);
						if (p2 == -1)mlib.IndexOf("\r", p1);
						if (p2 == -1)p1 = mlib.Length();
						cStr mname = mlib.Substring(p, p2 - p);
						mname.Trim(" \n\r\t");
						addmtl(mname);						
					} while (true);
				}
				// If raw is empty, usemtl was listed before any faces.
				// So previous default material is not needed at all.
				bool IsFound = false;
				cStr nm = ButFirstParam();
				for(int idMtl = 0; idMtl < Mesh.GetMaterials().Count(); idMtl++) {
					if(cStr::Equals(Mesh.GetMaterials()[idMtl].Name, nm)) {
						IsFound = true;
						idCurMtl = idMtl;
						break;
					}
				}
				if(!IsFound) {                        
					idCurMtl = Mesh.GetMaterials().Count();
					addmtl(nm);
				}
			}
		}
	}
    // Default material:
	auto& raw=Mesh.GetRaw();
	/*for(int i=0;i<raw.Count();i++){
		int n=raw[i][0];
		int j;
		for(j=0;j<n;j++){
			raw[i+j+1][0]=PosID[raw[i+j+1][0]];
		}
		i+=j;
	}*/
    if(Mesh.GetMaterials().Count()==0){
        cSurface m;
	    Mesh.GetMaterials().Add(m);
	    Mesh.GetMaterials().GetLast().Name = "default";
    }
	//restoring uv-sets from texture name
	cList<cUVSet>& uvs=Mesh.GetUVSets();
	cList<cSurface>& surf=Mesh.GetMaterials();
	cList<int> uvPerMtl;
	bool nontriv=false; 
	char common_part[512];
	bool MaxL=0;
	int L=0;
	for(int i=0;i<surf.Count();i++){
		cSurface& sr=surf[i];
		int idx=-1;
		cStr s=sr.Tex[0].FileName;
		if(MaxL==0){
			strcpy(common_part,s.ToCharPtr());
			L=strlen(s);
			MaxL=L;
		}else{
			for(int j=0;j<L;j++){
				if(s[j]!=common_part[j]){
					L=j;
					common_part[j]=0;
					break;
				}
			}
		}
	}
	for(;L>0 && common_part[L]!='\\' && common_part[L]!='/';L--){
		common_part[L]=0;
	};
	if(L)L++;
	for(int i=0;i<surf.Count();i++){
		cSurface& sr=surf[i];
		int idx=-1;
		cStr s1=sr.Tex[0].FileName;
		s1.RemoveFileExtension();
		cStr s;
		if(!s1.IsEmpty()){
			//s.RemoveFilePath();
			s=s1.Substring(L);
			if(!s.IsEmpty()){
				for(int j=0;j<uvs.Count();j++){					
					if(cStr::Equals(uvs[j].Name,s)){
						idx=j;
						nontriv=true;
						break;
					}
				}
			}
		}
		if(idx==-1){
			if(s.IsEmpty()){
				s=sr.Name;				
			}
			cUVSet uv;
			idx=uvs.Add(uv);
			uvs.GetLast().Name=s;			
		}
		uvPerMtl.Add(idx);
	}
	if(nontriv){
		cList<int> Usage;
		Usage.Add(0,uvs.Count());
		for(int i=0;i<raw.Count();i++){
			int n=raw[i][0];
			raw[i][1]&=0xFFFF;
			int uvs=uvPerMtl[raw[i][1]];
			Usage[uvs]++;
			raw[i][1]|=uvs<<16;
			i+=n;
		}
	}else uvs.Clear();
	return true;
} // cMeshObj::DecodeObj

//-----------------------------------------------------------------------------
// cMeshObj::LoadLine
//-----------------------------------------------------------------------------
const char* _Terminators = "\r\n";
const char* cMeshObj::ButFirstParam()
{
	cStr nm;
	int s = 0;
	do {
		char c = m_Buffer[s++];
		if (!(c == ' ' || c == '\t'))break;
	} while (true);
	do {
		char c = m_Buffer[s++];
		if (c == ' ' || c == '\t' || c == 0)break;
	} while (true);
	do {
		char c = m_Buffer[s];
		if (!(c == ' ' || c == '\t'))break;
		s++;
	} while (true);
	return m_Buffer + s;
}
bool cMeshObj::ReadString(cData& Data,char* dest,int Len){
	bool start=false;
	int L=0;
	int maxlen=Len;
	int prevslash=-1;
	do{
		char c;
		if(TempPos==TempMaxPos){
			TempPos=0;
			TempMaxPos=Data.Read(Temp,1024);
			if(TempMaxPos<=0)return L!=0;
		}
		c=Temp[TempPos++];
		//if(Data.Read(&c,1)!=1){
		//	return L!=0;
		//}
		if(strchr(_Terminators,c)){
			if(prevslash!=-1){
				dest[prevslash]=' ';
			}else if(start)break;
		}else{
			if(c=='\\'){
				prevslash=L;
			}else prevslash=-1;
			dest[L++]=c;
			dest[L]=0;
			start=true;
		}
		maxlen--;
	}while(maxlen!=0);
	return true;
}
int cMeshObj::LoadLine(cData &Src, int line, cMeshContainer& Mesh) {
	if(!ReadString(Src,m_Buffer,2047)) {
		return -1; // EOF
	}
	// Cut off comments:
	char* sd=strchr(m_Buffer,'#');
	if(sd && line == 0) {
		Mesh.FirstLineComment = sd;
	}
	if(sd){
		if(!(strncmp(m_Buffer,"# matrix4x4",11) || strncmp(m_Buffer,"#MRGB",5) || strncmp(m_Buffer,"#pivot",6))){
			sd[0]=0;
		}
	}
	int L=strlen(m_Buffer);	
	nTokens=0;
	int iidx=0;
	for(int i=0;i<L;i++){
		if(nTokens<maxngon){
			char c=m_Buffer[i];
			if(c==' ' || c=='\t'){
				if(iidx>0){
					m_Tokens[nTokens][iidx]=0;
					iidx=0;
					nTokens++;
				}
			}else{
				if(iidx<126)m_Tokens[nTokens][iidx++]=c;
			}
		}
	}
	if(nTokens<maxngon)m_Tokens[nTokens][iidx]=0;
	else nTokens=5;
	if(iidx>0)nTokens++;

	if(nTokens < 1) { // Comment or empty string.
		return 0;
	}

	return 1;
} // cMeshObj::LoadLine

//-----------------------------------------------------------------------------
// cMeshObj::GetIndices
//-----------------------------------------------------------------------------
const cVec3i cMeshObj::GetIndices(chars &Token) {
	static cStr Str;
	Str=Token;
    char *None = "-1";
	char *TexCoord = None;
	char *Normal = None;
	// Replace '/' with '\0' and setup TexCoord and Normal:
	for(char *c = &Token[0]; *c != '\0'; c++) {
		if('/' == *c) {
			if(None == TexCoord) {
				TexCoord = c + 1;
			} else {
				Normal = c + 1;
			}
			*c = '\0';
		}
	}
	cVec3i R;
	R[0]=-1;
	R[1]=-1;
	R[2]=-1;
	R[0]=atoi(Str);
	if(TexCoord[0])R[1]=atoi(TexCoord);
	if(Normal[0])R[2]=atoi(Normal);
	return R;
} // cMeshObj::GetIndices

//-------------------------------------------------------------------------------
// cMeshObj::AddFace
//-------------------------------------------------------------------------------
bool cMeshObj::AddFace(cMeshContainer &Mesh, const int idMtl, const int idObj) {
	// Face Info:
	Mesh.GetRaw().Add(cVec3i(nTokens - 1, idMtl, idObj));
	// Indices:
	for(int i = 1; i < nTokens; i++) {
		cVec3i Index = GetIndices(m_Tokens[i]);
		// Position index:
		if(Index[0] > 0) {
			Index[0]--;
		} else if(Index[0] < 0) {
			Index[0] = Mesh.GetPositions().Count() + Index[0];
		} else {
			cLog::Warning("cMeshObj: Zero indices not allowed: \"%s\".", m_Buffer);
			return false;
		}
		// TexCoord index:
		if(Index[1] > 0) {
			Index[1]--;
		} else if(Index[1] < 0) {
			Index[1] = Mesh.GetTexCoords().Count() + Index[1];
		} else {
			// Zero index, i.e. no tex coord.
		}
		// Normal index:
		if(Index[2] > 0) {
			Index[2]--;
		} else if(Index[2] < 0) {
			Index[2] = Mesh.GetNormals().Count() + Index[2];
		} else {
			// Zero index, i.e. no normal.
		}
		Mesh.GetRaw().Add(Index);
	}
	return true;
} // cMeshObj::AddFace
}
