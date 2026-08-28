/*
 * File     : VRMLContext.h
 * Purpouse : VRML Context class definition 
 * Data		: 04/04/2013
 */

#ifndef __VRMLContext__H_
#define __VRMLContext__H_


class VRMLNode;
class VRMLScene;

class VRMLMeshNode
{
public:
	VRMLNode* node;
	comms::cMat4 matrix;
	VRMLMeshNode()
	{
		node = NULL;
		matrix = comms::cMat4::Identity;
	}
};
typedef cList<VRMLMeshNode> meshNodeList;
typedef std::map<int32,VRMLNode*> materialMap;

class VRMLContext {
public:
	VRMLContext(cMesh* _mesh) { 
		mesh=_mesh;
		m_positionOffset=0;
		m_textureOffset=0;
		m_normalOffset=0;
		m_posIndexOffset=0;
		m_normIndexOffset=0;
		m_texIndexOffset=0;
	}
	~VRMLContext(){}
	cMesh* GetMesh() { return mesh; }
	bool Render(VRMLScene* scene);
protected:
	int ConvertSceneMeshNode(VRMLMeshNode *node);
	void CollectMeshNodes(VRMLNode* node, meshNodeList &meshNodes, mat4f &matrix);
	void CollectSceneMeshNodes(VRMLScene* scene, meshNodeList &meshNodes,mat4f &transform);
	int ConvertMaterial(VRMLNode *material);
	int ConvertFaceSetGeometry(VRMLMeshNode *node, VRMLNode *geometry);
	int FindMaterialIndex(VRMLNode *material);
	void AddMeshObject(CPCH objName); 
private:
	cMesh *mesh;
	comms::dword m_positionOffset;
	comms::dword m_textureOffset;
	comms::dword m_normalOffset;
	comms::dword m_posIndexOffset;
	comms::dword m_normIndexOffset;
	comms::dword m_texIndexOffset;

	materialMap m_materialMap;
		

};

#endif