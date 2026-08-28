/*
 * File     : VRMLImporer.cpp
 * Purpouse : VRML Importer class implementation
 * Data	    : 26/03/2013
 */

#include "stdafx.h"
#include "vrml.h"
#include "VRMLImporter.h"
#include "VRMLScene.h"
#include "VRMLContext.h"

VRMLImporter::~VRMLImporter(){ 
    if (context) {
        delete context; 
    	context=NULL;
    }
}

int VRMLImporter::Header(){
	cStr str;
	if(!m_data->ReadString(&str)) {
    	return 0; // EOF
    }
	if(cStr::Equals(str,VRML_VERSION,strlen(VRML_VERSION)-1)) {
       return 1;
    }
	return 0;
}

bool VRMLImporter::Initialize() {
	context = new VRMLContext(m_mesh);
	if(!context) return false;
	return true;
}

bool VRMLImporter::Render(VRMLScene* scenes){
	return context->Render(scenes);
}

bool VRMLImporter::Import(){
	if(!Initialize()) {
    	return false;
    }
    const cStr baseName = m_data->GetFilePn().GetFileBase();
    int result=1,iScene = 0;
    VRMLScene* root = NULL;
    while(true){
    if(!Header()) break;
       cStr str = cStr::Format("%s_%d\n",baseName.ToCharPtr(),iScene++);
       VRMLScene* scene = new VRMLScene(str);
       result = scene->Create(*m_data);
       if(!result) break; 
       if(root) {
          scene->SetNext(root); 
          root=scene;
       } else {
         root = scene;
      }
    }
    if(root!=NULL) 
    {
      result=Render(root);
#ifdef _DEBUG
#if TEST_WRL
#include "cMeshIO.h"
      if(result>=0) {
         cMesh* mesh = GetMeshContainer();
         cStr file = m_data->GetFilePn();
         cStr fileObj = cStr::Format("%s.obj",file.GetFileBase());
         comms::cMeshIO::SaveMesh(*mesh,fileObj.ToCharPtr());
      }
#endif
#endif	
      VRMLScene *scene=root;
      while(scene=root->Next()){
        delete root;
        root = scene;
      } 
      delete root;
    }
    return (result)?true:false;
}

