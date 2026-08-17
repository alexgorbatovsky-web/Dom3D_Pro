// LineStyleBox.cpp : implementation file
//
//	This is a part of the CAD/CAM/CAE "Alpha".
//	Copyright (C) 1994-2000 A.Gorbatovsky 
//	All rights reserved.
//	Ukraine, Kiev
//////////////////////////////////////////////////////////////////////


#include "LineStyleBox.h"
#include "CAlfaDoc.h"
#include "CView3d.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


LINE_STYLE_STRUCT Line_style[]={
    0xFFFF,"CONTINUOUS","Solid line",0,0,0,0,0,0,0,0,
    0xFFC0,"HIDDEN","__ __ __ __ __ __ __ __ __ __ __ __ __ __ __ __",2,8.0,5.0,-3.0,0,0,0,0,
    0xFFFA,"CENTER","____ _ ____ _ ____ _ ____ _ ____ _ ____ _ ____",4,20.0,15.0,-2.0,1.0,-2.0,0,0,
    0xFFEA,"PHANTOM","______ _ _ ______ _ _ ______ _ _ ______ _ _ __",6,23.0,15.0,-2.0,1.0,-2.0,1.0,-2.0,
    0xFFE4,"DASHDOT","__ . __ . __ . __ . __ . __ . __ . __ . __ . __",4,10.0,5.0,-2.5,0.0,-2.50,0,0,
    0x1111,"DOT",". . . . . . . . . . . . . . . . . . . . . . . .",2,2.5,0.5,-2.0,0,0,0,0,
    0xFFEA,"DIVIDE2","__..__..__..__..__..__..__..__..__..__..__..__.",6,6.25,2.5,-1.25,0.0,-1.25,0.0,-1.25,
    0xAAAA,"THINK","Solid line",0,0,0,0,0,0,0,0,
   };

GLint CLineStyleBox::GetFactor(GLushort style)
{
GLint factor=1;
	if(style==Line_style[STYLE_PHANTOM].style)
		factor=4;
	if(style==Line_style[STYLE_CENTER].style)
		factor=3;
	if(style==Line_style[STYLE_DASHDOT].style)
		factor=2;
	if(style==Line_style[STYLE_DIVIDE2].style)
		factor=2;


	return factor;
}

int CLineStyleBox::GetPenStyle(GLushort style)
{
int PenStyle=PS_SOLID;
	if(style==Line_style[STYLE_HIDDEN].style)
		PenStyle=PS_DASH;
	if(style==Line_style[STYLE_CENTER].style)
		PenStyle=PS_DASHDOT;
	if(style==Line_style[STYLE_DASHDOT].style)
		PenStyle=PS_DASHDOT;
	if(style==Line_style[STYLE_DIVIDE2].style)
		PenStyle=PS_DASHDOTDOT;
	if(style==Line_style[STYLE_DOT].style)
		PenStyle=PS_DOT;
	if(style==Line_style[STYLE_THINK].style)
		PenStyle=-1;

	return PenStyle;
}

GLushort CLineStyleBox::GetLineStyle(String name)
{
int i=0;
int pr=1;
    while(i<Num_el(Line_style)&&(pr=strcmp(name,Line_style[i].name))!=0)
		i++;
    if(pr!=0)
		i=0;
    return Line_style[i].style;

}

String CLineStyleBox::GetLineStyleName(GLushort style)
{
	for(int i=0; i<Num_el(Line_style); i++){
		if(Line_style[i].style==style)
			return Line_style[i].name;
	}
    return Line_style[0].name;
}


/////////////////////////////////////////////////////////////////////////////
// CLineStyleBox dialog


CLineStyleBox::CLineStyleBox)
{
	m_pDoc=NULL;

}


