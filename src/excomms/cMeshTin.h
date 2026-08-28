/*
 * File		: cMeshTin.h
 * Purpose	: Interface for TIN codec
 * Data		: 18/11/2022
 */

#ifndef __CMESH_TIN_H__
#define __CMESH_TIN_H__

class cMeshTin :public cMeshCodec
{
	public:
		cMeshTin();
		virtual ~cMeshTin();
		virtual cMeshContainer* Decode(cData& Fm);
		virtual void Encode(cMeshContainer& Mesh, cData* To);
		virtual bool CanDecode() { return false; }
};
#endif
