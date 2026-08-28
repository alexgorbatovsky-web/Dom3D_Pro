/*
 * File         : VRMLScene.cpp
 * Purpouse     : VRML Scene class implemented 
 * Data         : 04/04/2013
 */

#include "stdafx.h"
#include "vrml.h"
#include "VRMLNode.h"
#include "VRMLScene.h"
#include "VRMLParser.h"

VRMLScene::~VRMLScene(){
	for(int i=0;i<nodes.Count();i++){
		SAFE_RELEASE_OBJ(nodes[i]);
	}
}

bool VRMLScene::Create(cData &data){
  VRMLParser* parser = new VRMLParser(this,data);
  if(parser){
	  int iret = parser->Run();
	  delete parser;
	  return (iret>=0);
  }
  return false;
}

const cStr VRMLScene::GetDefNodePath(VRMLNode *node){
	cStr s;
	if(node) {

		if(node->DefID().IsEmpty()) 
		{ return s; }

		const VRMLNode *temp=node;
		while(temp) { 
			const cStr &defName = temp->DefID();
			bool isDefEmpty = defName.IsEmpty();
			if(!s.IsEmpty() && !isDefEmpty) {
				s.Insert(0,':');
			}
			if(!isDefEmpty){
				s.Insert(0,defName);
			}
	    	temp=temp->GetParent();
		}
	} 
	return s;
}

void VRMLScene::SetDefsNode(VRMLNode* node){
	if(node){
	  	cStr s = node->DefID();
		if(!s.IsEmpty()) mdefs[s] = node;
	}
}

VRMLNode* VRMLScene::GetDefsNode(cStr &name) {
	if(!name.IsEmpty()){
		return mdefs[name];
	} return NULL;
}
	
