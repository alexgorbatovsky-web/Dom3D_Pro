/*
 * File     : VRMLExporter.h
 * Purpouse : VRML Exporter class definition 
 * Data     : 26/03/2013
 */

#ifndef __VRML_EXPORTER_H__
#define __VRML_EXPORTER_H__

class VRMLExporter {
public:
	VRMLExporter(const cMesh *inMesh, cData *toData):
	m_mesh(inMesh)
	,m_data(toData)
	,indentLevel(0)
	,indent(false){}
	~VRMLExporter(){;}
	bool  Export();
	void SetIndent(bool bindent) { indent = bindent;}
private:
	void writeHeader();
	void writeAppearance(const cSurface &mtl, int used=0);
	void writeMaterial(const cSurface &mtl, int used=0);
	void writeTexture(const cSurface &mtl, int used=0);
	void writeVertexCoordinates(const cList<float> &list, const cList<int> &ids, int nitems=-1);
	void writeNormalCoordinates(const cList<float> &list, const cList<int> &ids, int nitems=-1);
	void writeTextureCoordinates(const cList<float> &list, const cList<int> &ids,int nitems=-1);
	void writeVertexColors(const cList<DWORD> &list, const cList<int> &ids,int nitems=-1);
	void writeCoordinates(const cList<float> &coords, int stride, int nitems=-1);
	void writeIndecies(const cList<int>& indecies);
	void writeIndented(const char *string, int inc=0);
	void writeFloat3D(float v1, float v2, float v3, bool iscomma=0);
	void writeFloat2D(float v1, float v2, bool iscomma=0);
	void replaceCommas(cStr *buf);
private:
	int indentLevel;
	bool indent;
	const cMesh *m_mesh;
	cData *m_data;
};
#endif