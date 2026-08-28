/*
 * File		: cMeshVRML.h
 * Purpose	: Interface for VRML files conversion
 * Data		: 25/03/2013
 */

#ifndef __CMESH_VRML_H__
#define __CMESH_VRML_H__

class cMeshVRML:public cMeshCodec
{
	public:
		cMeshVRML();
		virtual ~cMeshVRML();
		virtual cMeshContainer *Decode(cData &Fm);
		virtual void Encode( cMeshContainer &Mesh, cData *To);
};

#endif

