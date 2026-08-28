/*
 * File     : VRMLScene.h
 * Purpouse : VRML Scene class definition 
 * Data     : 04/04/2013
 */

#ifndef __VRMLScene_H__
#define __VRMLScene_H__

#include <map>

class VRMLNode;

typedef std::map<cStr,VRMLNode*> VRMLDefsMap;
typedef std::map<cStr,VRMLNode*>::iterator VRMLDefsMapIT;
typedef std::map<cStr,VRMLNode*>::const_iterator VRMLDefsMapCIT;

class VRMLScene {
public:
	VRMLScene(const cStr &_url){ 
		url = _url; 
		next=NULL; 
	}
	~VRMLScene();

	const cStr& Url() const { return url;}
	const cList<VRMLNode*>& Nodes() const { return nodes;}
	cList<VRMLNode*>& Nodes() { return nodes; }

	VRMLDefsMap &GetDefMap() { return mdefs; }  	
	
	bool Create(cData &data); 

	const cStr GetDefNodePath(VRMLNode *node);
	void AddNode(VRMLNode* node) { 
		nodes.Add(node); 
	}
	
	void SetDefsNode(VRMLNode* node);
	VRMLNode* GetDefsNode(cStr &name);
	
	VRMLScene* Next() { return next; }
	void SetNext(VRMLScene* _next) { next=_next; }
private:
	cStr url;
	cList<VRMLNode*> nodes;
	VRMLDefsMap mdefs;
	VRMLScene* next;
};

#endif
