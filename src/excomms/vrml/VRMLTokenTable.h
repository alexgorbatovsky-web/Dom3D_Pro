/*
 * File		: VRMLTokenTable.h
 * Purpouse	: Token table of the VRML names
 * Data		: 12/04/2013
 *
 */

#ifndef __VRMLTokenTable__H
#define __VRMLTokenTable__H

typedef 
struct Token
{
	struct Token* next;
	char*         spell;	
} Token;

typedef
struct TokTab
{
	struct TokTab* next;
	char*          TokName;
	int            TokValue;
} TokTab;

#include "VRMLSyntax.h"

static TokTab TokTable[] = {
					{NULL,VRML_COORDINATE_NODE_TYPE,COORD},
					{NULL,VRML_NORMAL_NODE_TYPE,NORMAL},
					{NULL,VRML_TEXTURE_COORD_NODE_TYPE,TEXCOORD},
					{NULL,VRML_CCW_PROPERTY,CCW},
					{NULL,VRML_COLORINDEX_PROPERTY,COLORINDEX},
					{NULL,VRML_COLORPERVERT_PROPERTY,COLORPERVERT},
					{NULL,VRML_CONVEX_PROPERTY,CONVEX},
					{NULL,VRML_COORDINDEX_PROPERTY,COORDINDEX},
					{NULL,VRML_CREASEANGLE_PROPERTY,CREASEANGLE},
					{NULL,VRML_NORMALINDEX_PROPERTY,NORMINDEX},
					{NULL,VRML_NORMALPERVERT_PROPERTY,NORMPERVERT},
					{NULL,VRML_SOLID_PROPERTY,SOLID},
					{NULL,VRML_TEXCOORDINDEX_PROPERTY,TEXCOORDINDEX},
					{NULL,VRML_COLOR_NODE_TYPE,COLOR},
					{NULL,NULL,NONE}};

#endif
