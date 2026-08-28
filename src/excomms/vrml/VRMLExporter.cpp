/* 
 * File     : VRMLExporter.cpp
 * Purpouse : VRML Exporter class implementation
 * Data		: 26/03/2013
 */

#include "stdafx.h"
#include "vrml.h"
#include "VRMLExporter.h"


void VRMLExporter::replaceCommas(cStr *buf) {
    buf->Replace(',', '.');
}

void VRMLExporter::writeFloat3D(float v1,float v2,float v3,bool iscomma) {
    cStr buf = cStr::Format("%f %f %f", v1, v2, v3);
    replaceCommas(&buf);
    if(iscomma) {
        buf.Append(",\n");
    } else {
        buf.Append("\n");
    }
    writeIndented(buf.ToCharPtr());
}

void VRMLExporter::writeFloat2D(float v1,float v2,bool iscomma) {
    cStr buf = cStr::Format("%f %f", v1, v2);
    replaceCommas(&buf);
    if(iscomma) {
        buf.Append(",\n");
    } else {
        buf.Append("\n");
    }
    writeIndented(buf.ToCharPtr());
}

void VRMLExporter::writeIndented(const char *s, int inc)
{
	if(indent) 
	{
		cStr sstring;
		sstring="";  		 
		if (inc < 1){
			indentLevel += inc;
		}
		for(int i=0;i < indentLevel;i++) {
			sstring.Append(" ");
		}
		sstring.Append(s);
		m_data->WriteString(sstring);
	
	} 
	else 
	{
		m_data->WriteString(s);
	}
	if(indent){ 
		if (inc > 0){
			indentLevel += inc;
		}
	}
}

void VRMLExporter::writeHeader() 
{
	m_data->WriteString(VRML_VERSION);
	m_data->WriteString(VRML_DESCRIPTION);
}

void VRMLExporter::writeAppearance(const cSurface &mtl, int used)
{
	writeIndented("appearance Appearance {\n", 1);
	writeMaterial(mtl,used);
	writeTexture(mtl,used);
	writeIndented("} # Appearance\n", -1);
}

void VRMLExporter::writeMaterial(const cSurface &mtl, int used) {
	if (used) {
		cStr str = cStr::Format("material USE %s\n",mtl.Name.ToCharPtr());
		writeIndented(str);
		return;
   	} 
	
	cStr matName = mtl.Name;
	if (!matName.IsEmpty()) {
		cStr str = cStr::Format("material DEF %s Material {\n",matName.ToCharPtr());
		writeIndented(str,1);
		cStr buf = cStr::Format("diffuseColor %f %f %f\n", mtl.Diffuse.r, mtl.Diffuse.g, mtl.Diffuse.b);
                replaceCommas(&buf);
		writeIndented(buf.ToCharPtr());
		float d = (mtl.Diffuse.r+mtl.Diffuse.g+mtl.Diffuse.b)/3;
		float a = (mtl.Ambient.r+mtl.Ambient.g+mtl.Ambient.b)/3;
		float ambient = (d!=0)?a/d:0;
                buf = cStr::Format("ambientIntensity %f\n", ambient);
		replaceCommas(&buf);
		writeIndented(buf.ToCharPtr());
		buf = cStr::Format("specularColor %f %f %f\n", mtl.Specular.r, mtl.Specular.g, mtl.Specular.b);
                replaceCommas(&buf);
		writeIndented(buf.ToCharPtr());
		buf = cStr::Format("emissiveColor %f %f %f\n", 0., 0., 0.);
		replaceCommas(&buf);
		writeIndented(buf.ToCharPtr());
		buf = cStr::Format("shininess %f\n", mtl.Shininess);
		replaceCommas(&buf);
		writeIndented(buf.ToCharPtr());
		buf = cStr::Format("transparency %f\n", mtl.Transparency);
		replaceCommas(&buf);
		writeIndented(buf.ToCharPtr());
		writeIndented("} # Material\n",-1);
	}
}

void VRMLExporter::writeTexture(const cSurface &mtl, int used)
{
	const comms::cStr &theFileName = mtl.Tex[0].FileName;
	const char* texName = theFileName.GetFileBase().ToCharPtr();
	if (used) {
		cStr str = cStr::Format("texture USE %s\n",texName);
	    writeIndented(str);
		return;
	} else {
		if(!theFileName.IsEmpty()){
			cStr str; 
			str = cStr::Format("texture DEF %s ImageTexture {\n",texName);
			writeIndented(str,1);
			str="";
			str << cStr::Format("url \"./%s\"\n",theFileName.GetFileName().ToCharPtr());
			writeIndented(str);
			writeIndented("} # ImageTexture \n",-1);
		}
   	}
}

void VRMLExporter::writeCoordinates(const cList<float> &coords, int stride, int nitems)
{
	int nCount = (nitems==-1)?coords.Count()/stride:nitems;
	for(int i = 0,ik = 0;i < nCount; i++, ik+=stride) {
		if(stride==2) {
		   writeFloat2D(coords[ik],coords[ik+1],true);
		}
		else if(stride==3){
		   writeFloat3D(coords[ik],coords[ik+1],coords[ik+2],true);
		}
	}
}

void VRMLExporter::writeVertexCoordinates(const cList<float> &vrtx, const cList<int> &ids, int nitems)
{
  //#vertices
    writeIndented("coord Coordinate {\n", 1);
    writeIndented("point [\n", 1);
	writeCoordinates(vrtx, 3, nitems);
    writeIndented("] # point\n", -1);
    writeIndented("} # Coordinate\n", -1);
    writeIndented("coordIndex [\n", 1);
	writeIndecies(ids);
    writeIndented("] # coordIndex\n", -1);
}

void VRMLExporter::writeNormalCoordinates(const cList<float> &norms, const cList<int> &ids, int nitems)
{
  //#normals
	writeIndented("normalPerVertex TRUE\n");
    writeIndented("normal Normal {\n", 1);
    writeIndented("vector [\n", 1);
	writeCoordinates(norms,3,nitems);
    writeIndented("] # vector\n", -1);
    writeIndented("} # Normal\n", -1);
    writeIndented("normalIndex [\n", 1);
	writeIndecies(ids);
    writeIndented("] # normalIndex\n", -1);
}

void VRMLExporter::writeTextureCoordinates(const cList<float> &texlist, const cList<int> &ids, int nitems)
{
  //#textures
	writeIndented("texCoord TextureCoordinate {\n", 1);
	writeIndented("point [\n", 1);
	writeCoordinates(texlist, 2, nitems);
	writeIndented("] # point\n", -1);
	writeIndented("} # texCoord\n", -1);
	writeIndented("texCoordIndex [\n", 1);
	writeIndecies(ids);
	writeIndented("] # texCoordIndex\n", -1);
}

void VRMLExporter::writeVertexColors(const cList<DWORD> &vrtcl, const cList<int> &ids, int nitems)
{
  //#vertex colors
	writeIndented("colorPerVertex TRUE\n");
	writeIndented("color Color {\n",1);
	writeIndented("color [\n",1);
	int ncount = (nitems==-1)?vrtcl.Count():nitems;
	for(int i=0;i<ncount;i++) {
		cColor color;
		color = cColor::FromDword(vrtcl[i]);
		writeFloat3D(color.b,color.g,color.r,true);
	}
	writeIndented("] # color\n",-1);
	writeIndented("} # Color\n",-1);
}
	
void VRMLExporter::writeIndecies(const cList<int>& indecies)
{
	cStr str;
	int nCount = indecies.Count();
	for(int i=0;i<nCount;i++){
	   int index = indecies[i];
	   if(index==-1){
              if(i==nCount-1) str << " -1\n";
              else str << " -1,\n";
              writeIndented(str);
              str.Clear();
	   } 
	   else {
              str << cStr::Format(" %d",index);
	   }
	}
}

bool  VRMLExporter::Export()
{

	if(!m_data || !m_mesh) {
		return false;
	}
	
	writeHeader();

	const cList<cSurface>& mtls=m_mesh->GetMaterials();
	const cList<cObject>& objs=m_mesh->GetObjects();
	const auto& raws=m_mesh->GetRaw();
	
	const auto& poss=m_mesh->GetPositions();
	const auto& vcl=m_mesh->GetVertexColor();
	const auto& ns=m_mesh->GetNormals();
	const auto& uvs=m_mesh->GetTexCoords();

	
	int np = poss.Count();   // vertex count
	int nn = ns.Count();	 // normals count
	int nuv = uvs.Count();	 // texcoords count
	int nvcl = vcl.Count();  // vertex color count 

	cList<int> posObj;
	cList<int> normObj;
	cList<int> uvsObj;

	posObj.Add(MAX_VERTECIES,np);
	normObj.Add(MAX_VERTECIES,nn);
	uvsObj.Add(MAX_VERTECIES,nuv);
	
	bool NoTexCoords = nuv < 1;
	bool NoNormals = nn < 1;
    bool NoVertColors = nvcl < 1; 
	
	int index = 0;
	while(index < raws.Count()) {        
		const cVec3i &Info = raws[index++];        
		int nDeg = Info[0];
		const int idMtl = Info[1];
        const int idObj = Info[2];
		while(nDeg--) {
			const cVec3i i0 = raws[index++];
			if (posObj[i0[0]] > idObj) 
				posObj[i0[0]] = idObj;
			if (i0[1] >= 0 && !NoTexCoords) {
				if(uvsObj[i0[1]] > idObj) uvsObj[i0[1]] = idObj;
			}
			if (!NoNormals) {
				if (i0[2] >= 0) {
					if (normObj[i0[2]] > idObj) normObj[i0[2]] = idObj;
				} else NoNormals = true;
			}
		}
	}	
	
	if(objs.Count()>1) {
	   writeIndented("DEF MESH Group {\n",1);
       writeIndented("children [\n",1);
   	}
	
	cList<float> posList;
	posList.Add(0,np*3);
	cList<float> normList;
	normList.Add(0,nn*3);
	cList<float> uvList;
	uvList.Add(0,nuv*2);
	cList<DWORD> vclList;
	vclList.Add(0,nvcl);

	for(int idxObj=0,curObj=0;curObj<objs.Count();curObj++)
	{	
		int index = 0;
		int curMtl = -1;
		int idv = 0; 
		int idn = 0;
		int iduv = 0;
		int npc = 0;
		int nnc = 0;
		int nuvc = 0;

		cList<int> posIds;  // pos index
		posIds.Add(0,np);
		cList<int> normIds; // normal index
		normIds.Add(0,nn);
		cList<int> uvIds;   // uv-texture index
		uvIds.Add(0,nuv);
		cList<int> mtlIds;  // material index
		mtlIds.Add(0,mtls.Count());

		const char* objName = (const char*)objs[curObj].Name;
		comms::cMat4 mt4 = objs[curObj].Transform;

		//vertex coords
		for(int i = 0; i < np; i++) {
			if(posObj[i]==curObj){
			   const cVec3* It=&poss[i];
			   cVec3 pos = (*It);
			   pos.TransformCoordinate(mt4);
			   posList[3*idv]=pos.x;
			   posList[3*idv+1]=pos.y;
			   posList[3*idv+2]=pos.z;
			   if(!NoVertColors && nvcl==np){
				  vclList[idv]=vcl[i];
			   }
			   posIds[i]=idv++;
			   npc++;
			}
		}
		if(npc==0){
			continue;
		}
		// Normals:
		nnc=0;
		for(int i=0;i<nn;i++) {
			if(normObj[i]==curObj) {
				const cVec3* It=&ns[i];
				cVec3 nn = *It;
				if(nn.Length()==0) nn=comms::cVec3::AxisY;
				nn.TransformNormal(mt4);
				normList[3*idn]=nn.x;
				normList[3*idn+1]=nn.y;
				normList[3*idn+2]=nn.z;
				normIds[i]=idn++;
				nnc++;
			}
		}
		// TexCoords:
		nuvc=0;		
		for(int i=0;i<nuv;i++){
			if(uvsObj[i]==curObj){			
				const cVec2* It=&uvs[i];
				uvList[2*iduv]=It->x;
				uvList[2*iduv+1]=It->y;
				uvIds[i]=iduv++;
				nuvc++;			
			}
		}
		// Collection raws data
		cList<int> newPosIds;
		cList<int> newNormIds;
		cList<int> newUVIds;
		
		bool hasNewShape = false;
		bool hasOnlyFaces = false;
		int iShape = 0;
		
		while(index < raws.Count()) 
		{
			const cVec3i &Info = raws[index++];        
			int nDeg = Info[0];
			const int idMtl = Info[1]&0xFFFF;
			const int idObj = Info[2];
			hasOnlyFaces = (nDeg > 2);
			if(curObj==idObj)
			{
				if(curMtl!=idMtl) 
				{
					if(hasNewShape) 
					{ 
						cStr str = cStr::Format("DEF %s_%d Shape {\n", objName,iShape);
						writeIndented(str.ToCharPtr(),1);
    					       // write appearance 	
						writeAppearance(mtls[curMtl],mtlIds[curMtl]&1);
						// write geometry 
						str=cStr::Format("geometry DEF %s%s_%d IndexedFaceSet {\n",
										 "FS_",
										 objName,
										 iShape);
						writeIndented(str.ToCharPtr(), 1);
						writeIndented("solid TRUE # one sided\n");
						// write the vertex coordinates
						writeVertexCoordinates(posList,newPosIds,npc);
						if (!NoNormals) {   // write the normal coordinates
						    writeNormalCoordinates(normList,newNormIds,nnc);
						    newNormIds.Clear();
						}
						if (!NoTexCoords) {	// write the texture coordinates
						    writeTextureCoordinates(uvList,newUVIds,nuvc);
						    newUVIds.Clear();
						}
						if (!NoVertColors){ // write the vertex colors
						    writeVertexColors(vclList,newPosIds,npc);
						}
						newPosIds.Clear();
						
						writeIndented("} # geometry\n", -1);
						writeIndented("} # Shape\n", -1);
						
						mtlIds[curMtl]=1;

					}
				    curMtl=idMtl;
				   // set values for creating of the new shape
					hasNewShape = true;
					iShape++;
				}
				if(hasNewShape && hasOnlyFaces) 
				{
					while(nDeg--) {
						const cVec3i i0 = raws[index++];
						newPosIds.Add(posIds[i0[0]]);
						if(!NoNormals) {
							newNormIds.Add(normIds[i0[2]]);			
						}
						if(!NoTexCoords) {
							newUVIds.Add(uvIds[i0[1]]);
						}
					}
					newPosIds.Add(-1);
					if(!NoNormals) {
					   newNormIds.Add(-1);
					}
					if(!NoTexCoords) {
					   newUVIds.Add(-1);
					}
				}
			} else { index+=nDeg; } //curObj==idObj
		} //end raw 
		if (hasNewShape) { 
			cStr str = cStr::Format("DEF %s_%d Shape {\n",objName,iShape);
			writeIndented(str.ToCharPtr(),1);
            writeAppearance(mtls[curMtl],mtlIds[curMtl]&1);
			str=cStr::Format("geometry DEF %s%s_%d IndexedFaceSet {\n",
							 "FS_",
							  objName,
							  iShape);
			writeIndented(str.ToCharPtr(), 1);
			writeIndented("solid TRUE # one sided\n");
			writeVertexCoordinates(posList,newPosIds,npc);
			if (!NoNormals) {   // write the normal coordinates
				writeNormalCoordinates(normList,newNormIds,nnc);
				newNormIds.Clear();
			}
			if (!NoTexCoords) {	// write the texture coordinates
				writeTextureCoordinates(uvList,newUVIds,nuvc);
				newUVIds.Clear();
			}
			if (!NoVertColors){ // write the vertex colors
				writeVertexColors(vclList,newPosIds,npc);
			}
			newPosIds.Clear();
			writeIndented("} # geometry\n", -1);
			writeIndented("} # Shape\n", -1);
		} // hasNewShape
	    idxObj++;
	} // curObjects	
	
	if(objs.Count()>1) {
		writeIndented("] # children\n", -1);
        writeIndented("} # Group\n", -1);
	}
	
	return true;
}
