/*
 * File     : VRMLImporter.h
 * Purpouse : VRML Importer class definition 
 * Data     : 1/04/2013
 */

#ifndef __VRMLImporter_H__
#define __VRMLImporter_H__


class VRMLContext;
class VRMLScene;
class VRMLImporter {
public:
	VRMLImporter(cData* inData, cMesh* outMesh):
    m_data(inData)
	,m_mesh(outMesh)
	,context(NULL)
	{}

	~VRMLImporter();
	
	inline cMesh* GetMeshContainer() const { return m_mesh;}

	bool Import();
	bool Render(VRMLScene* scenes);

protected:
	bool Initialize();
	int Header(); 
private:
	VRMLContext* context;
	cData* m_data;
	cMesh* m_mesh;
};

#endif