/* 
 * File     : VRMLNode.h
 * Purpouse : VRML nodes definitions
 * Data     : 04/04/2013
 */

#include "stdafx.h"
#include "vrml.h"
#include "VRMLNode.h"

VRMLAppearance::~VRMLAppearance(){
	SAFE_RELEASE_OBJ(material);
	SAFE_RELEASE_OBJ(texture);
	SAFE_RELEASE_OBJ(texture_transform);
}

VRMLShape::~VRMLShape(){
	SAFE_RELEASE_OBJ(appearance);
	SAFE_RELEASE_OBJ(geometry);
}

VRMLFaceSet::~VRMLFaceSet(){ 
	SAFE_RELEASE_OBJ(coord);
	SAFE_RELEASE_OBJ(normal);
	SAFE_RELEASE_OBJ(tex_coord);
	SAFE_RELEASE_OBJ(color);
}

VRMLGroupingBase::~VRMLGroupingBase(){
	for(int i=0; i<children.Count();i++){
	  SAFE_RELEASE_OBJ(children[i]);
	}
}
	
VRMLAppearance* VRMLNode::to_appearance() {
	return 0;
}

VRMLMaterial* VRMLNode::to_material() {
	return 0;
}

VRMLTexture* VRMLNode::to_texture() {
	return 0;
}

VRMLColor* VRMLNode::to_color(){
	return 0;
}

VRMLGroupingBase* VRMLNode::to_grouping(){
	return 0;
}

VRMLChild* VRMLNode::to_child() {
	return 0;
}

VRMLShape* VRMLChild::to_shape() {
    return 0;
}

VRMLGeometry* VRMLNode::to_geometry() {
	return 0;
}

VRMLNormal* VRMLNode::to_normal() {
	return 0;
}

VRMLCoordinate* VRMLNode::to_coordinate() {
	return 0;
}

VRMLTextureCoordinate* VRMLNode::to_tex_coordinate() {
	return 0;
}

VRMLTextureTransform* VRMLNode::to_tex_transform() {
	return 0;
}

VRMLTransform* VRMLNode::to_transform() {
	return 0;
}
VRMLGroup* VRMLNode::to_group() {
	return 0;
}

void VRMLTextureTransform::BuildMatrix() {
	//Tc' = -C * S * R * C * T * Tc
	mat3f m = mat3f::Identity;
	sfvec3f tmp(-center.x,-center.y,-1);
	m.SetRow2(tmp);
	matrix = m * mat3f::Scaling(scale);
	matrix *= mat3f::RotationZ(rotation);
	matrix *=(-m);
	m = mat3f::Identity;
	m.SetRow2(translation.x,translation.y,1);
	matrix *= m;
}

void VRMLTransform::BuildMatrix() {
	// P' = T * C * R * SR * S * -SR * -C * P 
	mat_transform = mat4f::Translation(-center) * mat_transform;
	
	sfvec3f saxes(scale_orient.x,scale_orient.y,scale_orient.z); 
	if(!saxes.IsZero()) {
	    mat_transform = mat4f::Rotation(-saxes,-scale_orient.w) * mat_transform;  
	}
	mat_transform = mat4f::Scaling(scale) * mat_transform;					  
	if(!saxes.IsZero()) {
		mat_transform = mat4f::Rotation(saxes,scale_orient.w) * mat_transform;	  
	}
	sfvec3f axes(rotation.x,rotation.y,rotation.z);
	if(!axes.IsZero()) {
		mat_transform = mat4f::Rotation(axes,rotation.w) * mat_transform;		  
	}
	mat_transform = mat4f::Translation(center) * mat_transform;			      
	mat_transform = mat4f::Translation(translation) * mat_transform;
}
