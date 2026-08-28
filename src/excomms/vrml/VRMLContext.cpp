/* 
 * File     : VRMLContext.cpp
 * Purpouse : VRML Context class implemented
 * Data     : 04/04/2013
 */

#include "stdafx.h"
#include "vrml.h"
#include "VRMLNode.h"
#include "VRMLScene.h"
#include "VRMLContext.h"

int VRMLContext::FindMaterialIndex(VRMLNode *appearance){
  	int index = 0;
	for (materialMap::iterator mi=m_materialMap.begin(); 
		 mi != m_materialMap.end(); ++mi) 
	{
		if(appearance ==  mi->second) 
		{	index = mi->first;
			break;
		}
	}
	return index;
}

int VRMLContext::ConvertMaterial(VRMLNode *appearance) {
	bool bNoMaterial = true;
	for (materialMap::iterator mi=m_materialMap.begin(); 
		 mi != m_materialMap.end(); ++mi){
			 if(appearance ==  mi->second) {
				 bNoMaterial = false; 
				 break;
			 }
	}
	if(bNoMaterial && appearance){
		VRMLAppearance *theAppearance = appearance->to_appearance();
		int32 currIndex = mesh->GetMaterials().Count();
		cSurface surface;
		cStr name = theAppearance->DefID();
		if(name.IsEmpty()){
			name = cStr::Format("material_%d",currIndex);
		}
		surface.Name = name;
		// convert appearance to surface (material & texture)
		VRMLMaterial* theMaterial = theAppearance->GetMaterial();
		if(theMaterial) {
			sfcolor& diffuse = theMaterial->GetDiffuse();
			surface.Diffuse.Set(diffuse.x,diffuse.y,diffuse.z);
			surface.Color.Set(diffuse.x,diffuse.y,diffuse.z); 
			sfcolor& emissive= theMaterial->GetEmissive();
			sfcolor& specular= theMaterial->GetSpecular();
			surface.Specular.Set(specular.x,specular.y,specular.z);
			sffloat ambient = theMaterial->GetAmbient();
			surface.Ambient.Set(diffuse.x*ambient,
								diffuse.y*ambient,
								diffuse.z*ambient);
			surface.Shininess = theMaterial->GetShininess();
			surface.Transparency = theMaterial->GetTransparency();
		}
		VRMLTexture* theTexture = theAppearance->GetTexture();
		if(theTexture) {
			VRMLTexture::texture_type ttype = theTexture->TexType();
			if(ttype == VRMLTexture::image) {
				VRMLImageTexture* theTexImage = DNCAST_OBJ(theTexture,VRMLImageTexture*);
				const cStr &url = theTexImage->GetUrl();
				surface.Tex[0].FileName=url;
			}
			else if (ttype == VRMLTexture::pixel) {
				VRMLPixelTexture* thePixImage = DNCAST_OBJ(theTexture,VRMLPixelTexture*);
				if(thePixImage) {
					sfimage &pixImage = thePixImage->GetPixelImage();
					uint32 nchannel = pixImage.GetChannel();
					const sfbyte *pixels = pixImage.GetData().ToPtr();
					cImage pngImage;
					comms::cFormat::Enum format = comms::cFormat::fmtNone;
					if(nchannel==1) format = comms::cFormat::R8;
					else if(nchannel==2) format = comms::cFormat::Rg8;
					else if(nchannel==3) format = comms::cFormat::Rgb8;
					else if(nchannel==4) format = comms::cFormat::Rgba8;
					pngImage.Copy(pixels,format,pixImage.GetWidth(),pixImage.GetHeight(),nchannel,1);
					// save pixImage to file
					cStr imageName = cStr::Format("Image_%d",currIndex);
					comms::cFile file;
					file.SetFilePn(imageName.ToCharPtr());
				#ifdef COMMS_PNG
					comms::cCodecPng codec;
					codec.Encode(pngImage,&file);
				#endif // PNG
				}
			}
		}
		mesh->GetMaterials().Add(surface);
		m_materialMap[currIndex]=appearance;
		return 1;
	}
	if(mesh->GetMaterials().Count()==0){
		cSurface m;
		mesh->GetMaterials().Add(m);
		mesh->GetMaterials().GetLast().Name="default";
	}
	return 0;
}

void VRMLContext::AddMeshObject(CPCH objName) 
{
	cObject obj;
	mesh->GetObjects().Add(obj);
	mesh->GetObjects().GetLast().Name=objName;
}

int VRMLContext::ConvertFaceSetGeometry(VRMLMeshNode *node, VRMLNode *geometry) {
	mat4f matrix = node->matrix;
	if(node && geometry){
		bool bNoNormals=true;
		bool bNoTexCoords=true;
		bool bNormalPerVertex=true;
		uint32 oldPosIdxOffset = m_positionOffset;
		uint32 oldNormIdxOffset =  m_normalOffset;
		uint32 oldTexIdxOffset = m_textureOffset;
		VRMLFaceSet *theFaceSet = geometry->to_geometry()->to_faceset();
		// convert faceSet to mesh position, normals, texCoords, color
	    auto &posArray = mesh->GetPositions();
		VRMLCoordinate* coord = theFaceSet->Coord();
		if(NULL == coord) return -1;
		mfvec3f &positions = coord->GetPoints();
		int32 ncount = posArray.Count();
		posArray.SetCount(ncount+positions.Count());
		for(int32 i=0; i<positions.Count();i++) {
			cVec3 tmp = positions[i];
			tmp.TransformCoordinate(matrix);
			posArray[ncount+i]=tmp;
		}
		m_positionOffset+=positions.Count();
		VRMLNormal* normal = theFaceSet->Normal();
		if(normal) {
			auto &normsArray = mesh->GetNormals();
			mfvec3f &normals = normal->GetPoints();
			bNormalPerVertex=theFaceSet->NormalPerVertex();
			int32 nncount = normsArray.Count();
			if(bNormalPerVertex) {
				normsArray.SetCount(nncount+normals.Count());
				for(int32 i=0; i<normals.Count();i++) {
					cVec3 tmp =normals[i];
					tmp.TransformCoordinate(matrix);
					normsArray[nncount+i]=tmp;
				}
				m_normalOffset+=normals.Count();
				bNoNormals=false;
			}
			else {
				cList<cList<int32> > idxList;
				int32 nfaces=0;
				int32 npcount = posArray.Count();
				idxList.SetCount(npcount);
				mfint32 &normIdx = theFaceSet->NormIndex();
				for(int i=0;i<normIdx.Count();i++) {
				   int32 nidx = normIdx[i];
				   if(nidx==-1) nfaces++;
				   else if(nidx<npcount) idxList[nidx].Add(nfaces);
				}
				normsArray.SetCount(nncount+npcount);
				for(int32 i=0; i<npcount;i++) {		// i point
					cList<int32>& ids = idxList[i];
					cVec3 norm;
					for(int k=0;k<ids.Count();k++){ // k faces
						norm+=normsArray[ids[k]];
					}
					norm/=3;
					normsArray[nncount+i]=norm;
				}
			}
		}
		VRMLTextureCoordinate* texCoord = theFaceSet->TexCoord();
		if(texCoord) {
			auto &uvArray = mesh->GetTexCoords();
			mfvec2f &uvs = texCoord->GetPoints();
			uvArray.AddRange(uvs);
			m_textureOffset+=uvs.Count();
			bNoTexCoords=false;
		}
		VRMLColor* vertColor = theFaceSet->Color();
		if(vertColor) {
			cList<DWORD> &vcolors = mesh->GetVertexColor();
			mfcolor &srcColors = vertColor->GetColors();
			mfint32 &colorIdx = theFaceSet->ColorIndex();
			if(theFaceSet->ColorPerVertex()) {
				bool NoIndex = colorIdx.IsEmpty();
				if( NoIndex || posArray.Count() == srcColors.Count()) { 
					int32 ncount = vcolors.Count();
					int32 ncolors = srcColors.Count();
					vcolors.SetCount(ncount+ncolors);
					if(NoIndex)	{
						for(int32 i=0; i<ncolors;i++)
							vcolors[ncount+i]=srcColors[i].to_dword();
					} 
					else {
						for(int32 k=0,i=0; i<colorIdx.Count();i++) {
							int ii=colorIdx[i];
							if( ii!=-1 && k < ncolors){
								vcolors[ncount+k]=srcColors[ii].to_dword();
								k++;
							}
						}
					}
				}
			}
			else {
				cList<cList<int32> > idxList;
				int32 nfaces=0;
				int32 npcount = posArray.Count();
				int32 nccount = vcolors.Count();
				idxList.SetCount(npcount);
				for(int i=0;i<colorIdx.Count();i++) {
				   int32 cidx = colorIdx[i];
				   if(cidx==-1) nfaces++;
				   else if(cidx<npcount) idxList[cidx].Add(nfaces);
				}
				vcolors.SetCount(nccount+npcount);
				for(int32 i=0; i<npcount;i++) {
					cList<int32>& ids = idxList[i];
					rgb color;
					for(int k=0;k<ids.Count();k++){
						color+=srcColors[ids[k]];
					}
					color/=3;
					vcolors[nccount+i]=color.to_dword();
				}
			}
		}
		// convert indecies
		mfint32 &posIdx = theFaceSet->CoordIndex();
		mfint32 &normIdx = theFaceSet->NormIndex();
		mfint32 &uvsIdx = theFaceSet->TexCoordIndex();
		
		VRMLChild *parent = NULL;
		if(theFaceSet->GetParent())
			parent=theFaceSet->GetParent()->to_child();

		VRMLAppearance *appearance = NULL;
		if(parent)
			appearance = parent->to_shape()->GetAppearance();
		
		int idMaterial = FindMaterialIndex(appearance);
		int idObject = mesh->GetObjects().Count();
		
		int32 i=0,faceCount=0,uvCurIdx=0,uvIdxOffset=0;
		int32 npCount = posIdx.Count();
		int32 nnCount = normIdx.Count();
		int32 nuvCount = uvsIdx.Count();
		for(int32 j = 0; j < npCount;) 
		{
			int32 vertexCount = 0;
			int32 faceIndex = j;
			//find face closed -1
			for(int32 i = j; i < npCount;i++){
				if(posIdx[i]!=-1) vertexCount++;
				else { 
					j += (vertexCount+1); 
					break;
				}
			}
			if(vertexCount == 0) break;
			if(!bNoTexCoords){
				uvCurIdx = uvIdxOffset;
				for(int32 ii=0,i = uvIdxOffset; i < nuvCount;i++,ii++) {
					if(uvsIdx[i]==-1) { 
						uvIdxOffset += (ii+1); 
						break;
					}
				}
			}
			faceCount++;
			mesh->GetRaw().Add(cVec3i(vertexCount,idMaterial,idObject));
			for(int32 k = faceIndex; k < faceIndex + vertexCount; k++) 
			{
				int ipos = posIdx[k] + oldPosIdxOffset;
				int inrm = -1;
				int itex = -1;
				if(!bNoNormals){
					inrm = (k<nnCount)?(normIdx[k] + oldNormIdxOffset):-1;
				}
				if(!bNoTexCoords){
					if(nuvCount==0) itex = ipos;
					else  {
						int kk = (uvCurIdx < uvIdxOffset-1)?uvCurIdx:uvIdxOffset-2;
						itex = (kk<0)?-1:((kk<nuvCount)?(uvsIdx[kk] + oldTexIdxOffset):-1);
						uvCurIdx++;
					}
				}
				cVec3i index(ipos, itex, inrm);
				mesh->GetRaw().Add(index);
			}
		}
		cStr geoName = cStr::Format("GFaceSet_%d",idObject);
		AddMeshObject(geoName.ToCharPtr());
	}
	return 0;		
}

int VRMLContext::ConvertSceneMeshNode(VRMLMeshNode *anode) {
	int ret = -1;
	if(anode && anode->node){
		VRMLShape *shape = NULL;
		if(anode->node->to_child())
			shape = anode->node->to_child()->to_shape();
			if(shape) {
				ret = ConvertMaterial(shape->GetAppearance());
				VRMLGeometry *geometry = shape->GetGeometry();
				if(geometry){
					if(geometry->to_faceset()) {
						ret = ConvertFaceSetGeometry(anode,geometry);
				}
			}
		}
	}
	return ret;
}			

bool VRMLContext::Render(VRMLScene* scene) {
	int ret = -1;
	if(scene)
	{
		mat4f matrix = mat4f::Identity;
		meshNodeList nodes;
		CollectSceneMeshNodes(scene,nodes,matrix);
		int nn = nodes.Count();
		for( int i = 0; i < nn; i++)
		{ 
			ret = ConvertSceneMeshNode(&nodes[i]);
			if(ret < 0) break; 
		}
	}
	return (ret >= 0);
}

void VRMLContext::CollectMeshNodes(VRMLNode* node, 
                                   meshNodeList &meshNodes,
                                   mat4f &matrix)
{
	if(!node) return;
	mat4f theMatrix = matrix;
	const VRMLNode::node_type typeID = node->TypeID();
	if(typeID == VRMLNode::group_node) {
		VRMLGroup *group = node->to_group();
		cList<VRMLNode*>& child = group->Children();
		for(int j = 0; j < child.Count();j++) {
			CollectMeshNodes(child[j],meshNodes,matrix);
		}
	}
	else if(typeID == VRMLNode::transform_node){
		VRMLTransform *transform = node->to_transform();
		transform->BuildMatrix();
		theMatrix = matrix * transform->GetMatrix();
		cList<VRMLNode*>& child = transform->Children();
		for(int j = 0; j < child.Count();j++) {
			CollectMeshNodes(child[j],meshNodes,theMatrix);
		}
	}
	else if(typeID == VRMLNode::child_node) {
			VRMLShape *shape = node->to_child()->to_shape();
			if(shape){
				VRMLMeshNode theMNode;
				theMNode.node = node;
				theMNode.matrix = theMatrix; 
				meshNodes.Add(theMNode);
			}
	}
}

void VRMLContext::CollectSceneMeshNodes(VRMLScene* scene, 
                                meshNodeList &meshNodes, 
                                mat4f &matrix)
{
    const cList<VRMLNode*>& nodes = scene->Nodes();
	for (int i = 0; i < nodes.Count(); i++)
	{
		VRMLNode* theNode = nodes[i];
		CollectMeshNodes(theNode,meshNodes,matrix);
	}
}
