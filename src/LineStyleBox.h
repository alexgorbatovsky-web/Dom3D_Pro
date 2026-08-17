#if !defined(AFX_LINESTYLEBOX_H__8C3DBC45_CB98_11D3_9E03_E33B50EBB22B__INCLUDED_)
#define AFX_LINESTYLEBOX_H__8C3DBC45_CB98_11D3_9E03_E33B50EBB22B__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// LineStyleBox.h : header file
//


/////////////////////////////////////////////////////////////////////////////
// CLineStyleBox dialog

class CLineStyleBox 
{
// Construction
public:
	CLineStyleBox();   // standard constructor


static GLint GetFactor(GLushort style);
static GLushort GetLineStyle(String name);
static String GetLineStyleName(GLushort style);
static int GetPenStyle(GLushort style);
static void CreatePen(CDC* pDC, CPen* pen, unsigned short style, short m_width,\
					  Pixel color, int selected);

// Implementation
protected:
	CAlfaDoc* m_pDoc;

};


//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.




#define STYLE_CONTINUOUS    0
#define STYLE_HIDDEN	    1	    
#define STYLE_CENTER	    2	    
#define STYLE_PHANTOM	    3	    
#define STYLE_DASHDOT	    4	    
#define STYLE_DOT			5	    
#define STYLE_DIVIDE2	    6    
#define STYLE_THINK	    7    
typedef struct{
	unsigned short style;
	String name;
	String label;
	short num;
	Coord com_len;
	Coord len[6];
	}LINE_STYLE_STRUCT;

extern LINE_STYLE_STRUCT Line_style[8];

#endif // !defined(AFX_LINESTYLEBOX_H__8C3DBC45_CB98_11D3_9E03_E33B50EBB22B__INCLUDED_)
