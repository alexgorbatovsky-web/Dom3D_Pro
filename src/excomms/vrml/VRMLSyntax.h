/* File: VRMLSyntax.h
 * Purpose: VRML enumarates types
 * Data: 03/04/2013
 */
#ifndef _VRMLSyntax__H_
#define _VRMLSyntax__H_

// keys
#define VRML_DEF_KEY "DEF"
#define VRML_USE_KEY "USE"

// IDs
#define DEF								 1
#define USE								 2

// letter strings
#define VRML_EON_STR 					"}"
#define VRML_EOFD_STR 					"]"

// node type names
#define VRML_SHAPE_NODE_TYPE			 "Shape"
#define VRML_APPEARANCE_NODE_TYPE		 "appearance"
#define VRML_MATERIAL_NODE_TYPE			 "material"
#define VRML_COLOR_NODE_TYPE 			 "color"
#define VRML_GROUP_NODE_TYPE 			 "Group"
#define VRML_CHILD_NODE_TYPE 			 "children"
#define VRML_TRANSFORM_NODE_TYPE 		 "Transform"
#define VRML_TEXTURE_NODE_TYPE 			 "texture"
#define VRML_TEXTURE_TRANSFORM_NODE_TYPE         "textureTransform"
#define VRML_GEOMETRY_NODE_TYPE 		 "geometry"
#define VRML_COORDINATE_NODE_TYPE 		 "coord"
#define VRML_NORMAL_NODE_TYPE 			 "normal"
#define VRML_TEXTURE_COORD_NODE_TYPE 	         "texCoord"

#define VRML_CCW_PROPERTY                        "ccw"
#define VRML_COLORINDEX_PROPERTY 		 "colorIndex"
#define VRML_COLORPERVERT_PROPERTY 		 "colorPerVertex"
#define VRML_CONVEX_PROPERTY 			 "convex"
#define VRML_COORDINDEX_PROPERTY 		 "coordIndex"
#define VRML_CREASEANGLE_PROPERTY 		 "creaseAngle"
#define VRML_NORMALINDEX_PROPERTY 		 "normalIndex"
#define VRML_NORMALPERVERT_PROPERTY              "normalPerVertex"
#define VRML_SOLID_PROPERTY                      "solid"
#define VRML_TEXCOORDINDEX_PROPERTY              "texCoordIndex"
// node names
#define VRML_GEO_IndexedFaceSet_TYPE             "IndexedFaceSet"
#define VRML_TEX_ImageTexture_TYPE 		 "ImageTexture"
#define VRML_TEX_PixelTexture_TYPE 		 "PixelTexture"
#define VRML_TEX_MovieTexture_TYPE 		 "MovieTexture"
#define VRML_TEX_Coordinate_TYPE 		 "TextureCoordinate"
#define VRML_COORDINATE_NODE_NAME 		 "Coordinate"
#define VRML_COLOR_NODE_NAME 			 "Color"



// property names
#define VRML_URL_PROPERTY			"url"
#define VRML_STRING_PROPERTY			"string"
#define VRML_TRANSLATION_PROPERTY		"translation"
#define VRML_ROTATION_PROPERTY			"rotation"
#define VRML_SCALE_PROPERTY			"scale"
#define VRML_SCALEORIENTATION_PROPERTY          "scaleOrientation"
#define VRML_CENTER_PROPERTY			"center"
#define VRML_BBOXCENTER_PROPERTY		"bboxCenter"
#define VRML_BBOXSIZE_PROPERTY			"bboxSize"
#define VRML_REPEAT_S_PROPERTY			"repeatS"
#define VRML_REPEAT_T_PROPERTY			"repeatT"
#define VRML_POINT_PROPERTY                     "point"
#define VRML_VECTOR_PROPERTY			"vector"
#define VRML_COLOR_PROPERTY			"color"
#define VRML_IMAGE_PROPERTY			"image"


#define VRML_EMISSIVE_COLOR_PROPERTY            "emissiveColor"
#define VRML_DIFFUSIVE_COLOR_PROPERTY           "diffuseColor"
#define VRML_AMBIENT_INTENSITY_PROPERTY         "ambientIntensity"
#define VRML_SPECULAR_COLOR_PROPERTY            "specularColor"
#define VRML_TRANSPARENCY_PROPERTY 		"transparency"
#define VRML_SHININESS_PROPERTY 		"shininess"

// Identifiers word
#define NONE			0
#define COORD			1
#define NORMAL			2
#define TEXCOORD		3
#define CCW			4		 
#define COLORINDEX		5
#define COLORPERVERT            6
#define CONVEX			7
#define COORDINDEX		8
#define CREASEANGLE		9
#define NORMINDEX		10
#define NORMPERVERT		11
#define SOLID			12
#define TEXCOORDINDEX           13
#define COLOR			14

#endif