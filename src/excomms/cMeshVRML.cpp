/*
 * File		: cMeshVRML.cpp
 * Purpose	: Implement the VRML files conversion
 * Data		: 25/03/2013
 */

#include "stdafx.h"
namespace comms {
#include "cMeshVRML.h"
}
#include "vrml/vrml.h"
#include "vrml/VRMLExporter.h"
#include "vrml/VRMLImporter.h"

namespace comms {

cMeshVRML::cMeshVRML(){
}

cMeshVRML::~cMeshVRML(){
}

void cMeshVRML::Encode( cMeshContainer &Mesh, cData *To) {
	if(NULL != To){
		VRMLExporter *theExporter = new VRMLExporter(&Mesh,To);
		if(theExporter){
			theExporter->Export();
			delete theExporter;
			theExporter = NULL;
		}
	}
}

cMeshContainer *cMeshVRML::Decode(cData &Src) {
	if(Src.IsEof()){
		return NULL;
	}
	const char *src = Src.GetFilePn();
	if(!src) {
		return NULL;
	}
#if !defined(__APPLE__) && !defined(LINUX)
	assert(_CrtCheckMemory());
#endif
	cMeshContainer *mesh = new cMeshContainer;
	mesh->SetName(src);
	VRMLImporter* vrmlImporter = new VRMLImporter(&Src,mesh);
	if(vrmlImporter){
		if(!vrmlImporter->Import()){
		   delete mesh;
		   mesh=NULL;	
		}
		delete vrmlImporter;
	}
	return mesh;
}
};

#ifdef VRML_PLUGIN

EXPORTED_DLL int GetNumMeshCodecs (){
	return 1;
}

EXPORTED_DLL comms::cMeshCodec* GetMeshCodec(int idx){
	return new comms::cMeshVRML;
}

EXPORTED_DLL const char* GetMeshCodecName(int idx){
	return "wrl";
}

EXPORTED_DLL bool ImportVRML(const char* file) {
  cData data; 
  
  data.SetFilePn(file,false);
  comms::cMeshCodec* codec = new comms::cMeshVRML;
  comms::cMeshContainer* mesh = codec->Decode(data);
  delete codec;
  if( mesh ) {
	delete mesh;
	return true;
  }
  return false;
}

#endif
