/*
 * File         : VRMLParser.cpp
 * Purpouse     : VRML Parser class implement
 * Data         : 05/04/2013
 */

#include "stdafx.h"
#include "vrml.h"
#include "VRMLNode.h"
#include "VRMLScene.h"
#include "VRMLParser.h"
#include "VRMLSyntax.h"
#include "VRMLStringConversion.h"
#include "VRMLTokenTable.h"

char VRMLParser::get_char(int pos) {
	return ((pos<0)?m_pos:pos)<m_buffer.Length()?m_buffer[m_pos]:0;
}

void VRMLParser::set_char(char ch, int pos) {
	if(pos<m_buffer.Length()) m_buffer[pos] = ch; 
}

void VRMLParser::set_pos(int pos){
	m_pos = pos<m_buffer.Length()?pos:m_buffer.Length();
}


void VRMLParser::set_defname(sfpnode pnode){
	if(pnode) pnode->SetDefID(m_defName);
	m_defName.Clear();
}

int VRMLParser::skip_spaces(int iStart, int iEnd){
	const char *p=m_buffer.ToCharPtr();
	int iSymbol = -1;
	int Len = m_buffer.Length();
	if(iStart > Len-1 || iStart > iEnd ) return -1;
	if(iEnd > Len-1) iEnd = Len - 1;
	for(int i=iStart;i<iEnd;i++) 
		if(p[i]==V_CS || p[i]==V_TAB) {
			continue;
		} else {
			iSymbol=i;
			break;
		}
    if(iSymbol==-1) m_pos = Len;
	else 			m_pos = iSymbol;
	return iSymbol;
}

void VRMLParser::AddNode(sfpnode parent, sfpnode node){
	if(node) {
		if(parent) { 
			parent->AddChild(node); 
			node->SetParent(parent);
		}
		else m_scene->AddNode(node);
		m_scene->SetDefsNode(node);
	}
}

int VRMLParser::GetLine(){

	if(/*m_pos < m_buffer.Length() && */m_pos < m_len) return 1;
	else return -1;

	if(!m_data.ReadString(&m_buffer)) {
		return -1; // EOF
	}
	return 0;
}

bool VRMLParser::IsEON(cStr &str) {
	if(IsEqual(str,VRML_EON_STR)) return true;
	else {
		if(str[0]==V_CBCR){ 
			set_pos(m_pos-str.Length()+1);
			return true;
		}
	}
	return false;
}

int VRMLParser::GetBufferData()
{
	m_buffer.Clear();
	cStr temp;
	int length=0;
	while(true)
	{
		if(!m_data.ReadString(&temp)) 
		{ 
			break; 
		}
		// Cut off comments:
		const int iSharp = temp.IndexOf('#');
		if(iSharp > -1) {
			temp.Remove(iSharp);
		}
		if(!temp.IsEmpty()){	
			int Len = temp.Length();
			if(temp[Len-1]!=V_CS){
				temp.Append(V_CS);
			    Len++;
			}
			int j=0;
			while((temp[j]!='\0') && (temp[j]==V_CS)) { j++;}
			if(temp[j]=='\0') { j--; }
			if (j <= Len) {
				cStr tmp(temp, j, Len - j);
				m_buffer += tmp;
				length = m_buffer.Length();
			}
		}
	}
	m_len = length;
#ifdef _DEBUG
#if TEST_WRL
	cData file;
	file.SetFilePn("temp.wrl",true);
	file.WriteString(m_buffer);
#endif
#endif
	return length;
}

int VRMLParser::GetToken(cStr &str) {
	m_tokens.Split(str, " \t\r\n");
	// Comment or empty string.
	if(m_tokens.Count() < 1) { 
		return 0;
	}
	return 1;
}

int VRMLParser::extract_def_name(cStr &str){
	extract_word(str);
	int bUseDef=0;
	if(IsEqual(str,VRML_DEF_KEY)) {
		bUseDef = DEF;
	} else if(IsEqual(str,VRML_USE_KEY)) {
		bUseDef = USE;
	} 
	if (bUseDef) {
		char *p = TO_CURR_PTR();
		char c;
		// skip spaces
		while ((c = *p) != 0 && (c == ' ' || c == '\t')) { p++; m_pos++; }
	    //extract_word(str);
		extract_by_seps(str, "{"); // may be contains s space symbol and tabs
		
		p = TO_CURR_PTR();
		char *q = p - 1;
		
		while ((c = *q) != 0 && (c == ' ' || c == '\t')) q--; // skip spaces
		while ((c = *q) != 0 && (c != ' ' && c != '\t')) q--; // back to spaces
		while ((c = *q) != 0 && (c == ' ' || c == '\t')) q--; // skip spaces
		q++;
		int ncount = (int)(p - q);
		str.Remove(str.Length() - ncount, ncount);
		m_defName = str;
		m_pos -= (ncount - 1);
	}
	return bUseDef;
}

int VRMLParser::check_braces(char br){
	cStr str; 
	extract_by_seps(str," \t");
	if(str[0]!=br) return VRML_ERR_BAD_SYMBOL;
	else {
		int Len = str.Length();
		if(Len>1) set_pos(m_pos-Len-1);
	} return 0;
}

int VRMLParser::check_start_node(const char* name) {
	cStr str;
	int idef=0;
	idef=extract_def_name(str);
	if(idef==DEF){
		extract_word(str);
	}else if(idef==USE) return idef;
	if(!IsEqualNoCase(str,name))
		return VRML_ERR_BAD_WORD;
	if(get_char()!='{'){	
		if(check_braces('{')<0) 
			return VRML_ERR_BAD_SYMBOL;
	}
	return 0;
}

void VRMLParser::extract_by_seps(cStr &dst, char* seps){
	dst.Clear();	
	const char *p = m_buffer.ToCharPtr();
	int istart = 0;
	int icount = 0;
	int ns = strlen(seps);
	const char* c = p + m_pos;
	bool issep = true;
	if(*c != '\0') {
		while(issep) {
			issep=false;
			for(int i=0;i<ns;i++) {
				issep=(seps[i] == *c); 
				if(issep) break;
			}
			if(issep) {
				c++;
				istart++;
			}
			if('\0' == *c) break;
		}
		issep=false;
		const char *cc = c;
		while(*cc != '\0' && !issep) {
			cc++;
			icount++;
			for(int i=0;i<ns;i++) {
				issep = (seps[i] == *cc); 
				if(issep) break;
			}
		}
	}
	if('\0' != *c) {
		dst.Copy(c,0,icount);
		m_pos += (int)(istart+icount);
	}
}

void VRMLParser::extract_word(cStr &dst){
	extract_by_seps(dst," \t{[");
}


bool VRMLParser::extract_url_name(cStr &str) {
	bool iret = false;
	char *s = TO_CURR_PTR();
	char c;
	bool brEnd = false;
	while ((c = *s) != 0 && 
			(c == ' ' || 
			c == '\t' ||
			c == '[')) 
	{ ++s; 
	  if(c=='[') brEnd = true;
	}
	if (*s == '"') {
		char *p = s;
		char *q = s;
		while ((c = *p) != 0 && (c == ' ' || c == '\t' )) 
		{ p++; } 
		if (*p == '"') p++; 
		s = p;
		while ((c = *p) != 0 && /*c != ' ' && */c != '\t' && c != ']' && c != '"' ) 
		{ p++; }
		
		if (*p == '"') {
			q = p; 
			iret = true;
		}

		if (*(p-1) == '"') {
			p--;
			q = p;
			iret = true;
		} 
		else {
			q = p;
			while ( (c = *q) != 0 && (/*c == ' ' ||*/ c == '\t' ) && c != '"') 
			{ q++; } 
			if(*q != '"') iret = false;
			else iret = true;
		}
		if (iret) {
			if(brEnd) {
				q = p+1;
				while ( (c = *q) != 0 && (c == ' ' || c == '\t' )) 
				{ q++; }
				if(*q != ']') return false;
			}
			if(*s == '"') s++;
			str.Clear();
			while (s < p) { str.Append(*s); s++; }
			p = q + 1;
			m_pos += (p - TO_CURR_PTR());
		}
	} 
	return iret;
}

void VRMLParser::skip_floats(int n){
	int count = 0;
	char *sz = TO_CURR_PTR();
	if (sz != NULL && *sz > 0) {
		while (*sz != 0 && count < n)
		{
			while (*sz != 0    && 
				  (*sz == ' '  || 
				   *sz == '\t' || 
				   *sz == '\r' || 
				   *sz == '\n' || 
				   *sz == ',')) ++sz;
			if (*sz == 0) break;
			++count;
			while (*sz != 0    && 
				   *sz != ' '  && 
				   *sz != '\t' && 
				   *sz != '\r' && 
				   *sz != '\n' &&
				   *sz != ',') ++sz;
		}
	}
	m_pos += (sz - TO_CURR_PTR()); 
}

int VRMLParser::skip_nodes() {
	int  ibrackets = 0;
	int  iret = 0;
	bool quit = false;
	bool bopen = true;
	bool bstart = true;
	int index_end = m_buffer.IndexOf('}',m_pos);
	if (index_end==-1) return VRML_ERR_PARSE;		
	while(!quit) {
		iret = GetLine();
		if(iret==VRML_EOF) { 
			if (ibrackets>0) return VRML_ERR_PARSE;
			else return -1;
		}
		if(iret > 0) {
			int index = -1;
			if (bopen) {
				index = m_buffer.IndexOf('{',m_pos);
				if(index == -1) { 
					m_pos = index_end+1; 
					bopen=false;
				} else if (index > index_end) {
						if (ibrackets > 0) {
							bopen = false;
							m_pos = index_end+1;
							index_end = index;
							if (ibrackets==1 ) { break;}
							if(!bstart) ibrackets++;
							bstart=false;
						}
						else return VRML_ERR_PARSE;
				} else {
				   ibrackets++;
				   m_pos=index+1;
				}
			} else {
				if (ibrackets) {
					index = m_buffer.IndexOf('}',m_pos);
				} 
				if(index > index_end){
					if(ibrackets>0) {
						m_pos = index_end + 1; 
						index_end = index;
						ibrackets--;
						if(ibrackets > 0) 
						   bopen=true; 

					}
				} else if(index!=-1) { 
					ibrackets--; 
					m_pos=index+1; 
				} else {
					if(ibrackets>0) return VRML_ERR_PARSE;
				}
			}
		} 
		if(!bopen && ibrackets == 0) { 
			quit = true; 
			iret=VRML_EON;
			m_pos = index_end;
		} 
	} 
	return iret;
}

int VRMLParser::calc_end_index_child(){
	int  iret = 0;
	bool bstart= true;
	bool bopen = true;
	bool quit = false;
	int ibrackets = 0;
	int ipos = m_pos;
	int index_end = m_buffer.IndexOf(']',ipos);
	if (index_end==-1) return VRML_ERR_PARSE;		
	while(!quit) {
		iret = GetLine();
		if(iret==VRML_EOF) { 
			if (ibrackets>0) return VRML_ERR_PARSE;
			else return -1;
		}
		if(iret > 0) {
			int index = -1;
			if (bopen) {
				index = m_buffer.IndexOf('[',ipos);
				if(index == -1) { 
					ipos = index_end+1; 
					bopen=false;
				} else if (index > index_end) {
						if (ibrackets > 0) {
							bopen = false;
							int ie = m_buffer.IndexOf(']',index_end+1);
							while(ie < index) {
								index_end = ie;
								ie = m_buffer.IndexOf(']',ie+1);
								ibrackets--;
							}
							if(bstart && ibrackets==1){
							  	ibrackets--;
							}
							else if(ibrackets>0) {
								ipos = index_end+1;
								index_end = index;
								if(!bstart) ibrackets++;
							}
							bstart=false;
						}
						else return VRML_ERR_PARSE;
				} else {
				   ibrackets++;
				   ipos=index+1;
				}
			} else {
				if (ibrackets) {
					index = m_buffer.IndexOf(']',ipos);
				} 
				if(index > index_end){
					if(ibrackets>0) {
						ipos = index_end + 1; 
						index_end = index;
						ibrackets--;
						if(ibrackets > 0) 
						   bopen=true; 

					}
				} else if(index!=-1) { 
					ibrackets--; 
					ipos=index+1; 
				} else {
					if(ibrackets>0) return VRML_ERR_PARSE;
				}
			}
		} 
		if(!bopen && ibrackets == 0) { 
			quit = true;
			ipos = index_end;
		} 
	} 
	return ipos;
}


bool VRMLParser::IsEqual(const cStr &s1, const char* s2){
	return cStr::Equals(s1.ToCharPtr(),s2);
}

bool VRMLParser::IsEqualNoCase(const cStr &s1, const char* s2) {
	return cStr::EqualsNoCase(s1.ToCharPtr(),s2);
}

int VRMLParser::parse_group(sfpnode pnode) {
  
	if(get_char()!='{') {
		if(check_braces('{')<0) 
		return VRML_ERR_BAD_SYMBOL;
	} else set_pos(m_pos+1);
  
	VRMLGroup *pgroup_node = new VRMLGroup();
	set_defname(pgroup_node);
	int iret = 0;
	bool quit = false;
	bool expectedEnd = false;
	cStr str;
	while(!quit) {
	iret = GetLine();
	if(iret==VRML_EOF) break;
	if(iret > 0){
		if(expectedEnd) 
		{
			while((iret = GetLine())!=-1) {
				extract_by_seps(str," ,\t");
				if(str[0]!='}') {
					iret = VRML_ERR_BAD_SYMBOL;
					break;
				} else break;
			}
			if(iret < 0){
			   iret = VRML_ERR_PARSE; 
			   quit=true;
			} else {
			   expectedEnd = false;
			   quit=true;
			}	
		}  // expectedEnd
		else {
			while((iret = GetLine())!=-1) {
				extract_by_seps(str," [\t");
				if(IsEqual(str,VRML_CHILD_NODE_TYPE)){
					iret = parse_children(pgroup_node);
					expectedEnd = true;
					break;
				} else if(IsEON(str)){
					quit = true; 
					break;
				}
			}
		}
		if(iret == VRML_EOF && expectedEnd) { 
			iret = VRML_ERR_PARSE; 
			quit = true; 
		}
	} // iret
	} // while
  ADD_NODE_BY_CODE(iret,pnode,pgroup_node);
  return iret;
}

int VRMLParser::parse_transform(sfpnode pnode) {
	
	if(get_char()!='{') {
		if(check_braces('{')<0) 
			return VRML_ERR_BAD_SYMBOL;
	} else set_pos(m_pos+1);

	VRMLTransform *ptransform = new VRMLTransform();
	set_defname(ptransform);

	int iret = -1;
	bool quit=false;
	while(!quit) {
		iret = GetLine();
		if(iret==VRML_EOF) break;
		if(iret>0){
			cStr str;
			extract_word(str);
			char *sz = TO_CURR_PTR();
			char *pz = sz;
			int offs_pos = 0;
			if(IsEqual(str,VRML_TRANSLATION_PROPERTY)){
				sfvec3f t =  VRMLStringConversion::ToVector3((CPCH*)&sz);
				ptransform->Translation(t);
				offs_pos = sz-pz;
			}
			else if(IsEqual(str,VRML_ROTATION_PROPERTY)){
				sfvec4f r =  VRMLStringConversion::ToVector4((CPCH*)&sz);
				ptransform->Rotation(r);
				offs_pos = sz-pz;
			}
			else if(IsEqual(str,VRML_SCALE_PROPERTY)){
				sfvec3f s =  VRMLStringConversion::ToVector3((CPCH*)&sz);
				ptransform->Scale(s);
				offs_pos = sz-pz;
			}
			else if(IsEqual(str,VRML_SCALEORIENTATION_PROPERTY)){
				sfvec4f o =  VRMLStringConversion::ToVector4((CPCH*)&sz);
				ptransform->ScaleOrient(o);
				offs_pos = sz-pz;
			}
			else if(IsEqual(str,VRML_CENTER_PROPERTY)){
				sfvec3f c =  VRMLStringConversion::ToVector3((CPCH*)&sz);
				ptransform->Center(c);
				offs_pos = sz-pz;
			}
			else if(IsEqual(str,VRML_BBOXCENTER_PROPERTY)){
				skip_floats(3);
			}
			else if(IsEqual(str,VRML_BBOXSIZE_PROPERTY)){
				skip_floats(3);		
			} 
			else if(IsEqual(str,VRML_CHILD_NODE_TYPE)){
				iret = parse_children(ptransform);
				if(iret < 0) quit = true;
			}
			else if(IsEON(str)) {
				quit = true;
			} else {
				iret = VRML_ERR_BAD_SYMBOL;
				quit = true;
			}
			if(iret >=0 ){
		  		set_pos(m_pos+offs_pos);
			}
		}
  	}
	ADD_NODE_BY_CODE(iret,pnode,ptransform);
	return iret;
}

template<class T>
int VRMLParser::parse_points(const char* key, mfarray<T> &points){
	int iret = -1;
	bool quit=false;
	while(!quit) {
		iret = GetLine();
		if(iret==VRML_EOF) break;
		if(iret>0) {
			cStr str;
			extract_word(str);
			if(IsEqual(str,key)) {
				if(get_char()!='[') {
					if(check_braces('[')<0) {
						iret =  VRML_ERR_BAD_SYMBOL; 
						quit = true;
						break;
					}
				} else { m_pos++; }
				int index = m_buffer.IndexOf(']',m_pos);
				//set_char('\0',index);
				const char *sz = TO_CURR_PTR();
				VRMLStringConversion::ToVecList<T>(sz,points);
				//set_char(']',index);
				set_pos(index+1);
			} 
			else if(IsEON(str)){
				quit = true;		
			}
			else iret = VRML_ERR_BAD_WORD;
			if(iret<0) quit = true;
		}
	}
	return iret;
}
int VRMLParser::parse_coord(sfpnode pnode) {
	cStr str;
	int idef = 0;
	if((idef = extract_def_name(str))==DEF){
		extract_word(str);
	} else if (idef==USE) {
		sfpnode node = m_scene->GetDefsNode(m_defName);	
		m_defName.Clear();
		VRMLFaceSet *faceset = pnode->to_geometry()->to_faceset();
		faceset->SetCoord(node->to_coordinate());
		node->Retain();
		return 0;
	}
	if(IsEqual(str,VRML_COORDINATE_NODE_NAME)) {
		if(get_char()!='{') {
			if(check_braces('{')<0) 
				return VRML_ERR_BAD_SYMBOL;
		}
	} else return VRML_ERR_BAD_WORD;
  
	VRMLCoordinate *coord = new VRMLCoordinate();
	set_defname(coord);
	mfvec3f &vec = coord->GetPoints();
	int iret = parse_points<cVec3>(VRML_POINT_PROPERTY,vec);
	if(iret>=0){
		if(pnode){
			VRMLFaceSet *faceset = pnode->to_geometry()->to_faceset();
			faceset->SetCoord(coord);
		}
	}
	ADD_NODE_BY_CODE(iret,pnode,coord);
	return iret;	  
}

int VRMLParser::parse_normal(sfpnode pnode){
	cStr str;
	int idef = 0;
	if((idef = extract_def_name(str))==DEF){
		extract_word(str);
	} else if (idef==USE) {
		sfpnode node = m_scene->GetDefsNode(m_defName);	
		m_defName.Clear();
		VRMLFaceSet *faceset = pnode->to_geometry()->to_faceset();
		faceset->SetNormal(node->to_normal());
		node->Retain();
		return 0;
	}
	if(IsEqualNoCase(str,VRML_NORMAL_NODE_TYPE)) {
		if(get_char()!='{') {
			if(check_braces('{')<0) 
				return VRML_ERR_BAD_SYMBOL;
		}
	} else return VRML_ERR_BAD_WORD;
  
	VRMLNormal *normal = new VRMLNormal();
	set_defname(normal);
	mfvec3f &vec = normal->GetPoints();
	int iret = parse_points<cVec3>(VRML_VECTOR_PROPERTY,vec);
	if(iret>=0){
		if(pnode){
			VRMLFaceSet *faceset = pnode->to_geometry()->to_faceset();
			faceset->SetNormal(normal);
		}
	}
	ADD_NODE_BY_CODE(iret,pnode,normal);
	return iret;	  
}

int VRMLParser::parse_texcoord(sfpnode pnode){
	cStr str;
	int idef=0;
	if((idef=extract_def_name(str))==DEF){
		extract_word(str);
	} else if (idef==USE) {
		sfpnode node = m_scene->GetDefsNode(m_defName);	
		m_defName.Clear();
		if(node){ 
			VRMLFaceSet *faceset = pnode->to_geometry()->to_faceset();
			faceset->SetTexCoord(node->to_tex_coordinate());
			node->Retain();
		}
		return 0;
	}
	if(IsEqual(str,VRML_TEX_Coordinate_TYPE)) {
	  if(get_char()!='{') {
		 if(check_braces('{')<0) 
			return VRML_ERR_BAD_SYMBOL;
	  }
	} else return VRML_ERR_BAD_WORD;

	VRMLTextureCoordinate *texcoord = new VRMLTextureCoordinate();
	set_defname(texcoord);
	int iret = -1;
	bool quit=false;
	while(!quit) {
		iret = GetLine();
		if(iret==VRML_EOF) break;
		if(iret>0) {
			extract_word(str);
			if(IsEqual(str,VRML_POINT_PROPERTY)) {
				if(get_char()!='[') {
					if(check_braces('[')<0) { 
						iret =  VRML_ERR_BAD_SYMBOL; 
						quit = true;
						break;
					}
				} else { m_pos++; }
				int index = m_buffer.IndexOf(']',m_pos);
				//set_char('\0',index);
				mfvec2f &vec = texcoord->GetPoints();
				const char *sz = TO_CURR_PTR();
				VRMLStringConversion::ToVec2List(sz,vec);
				//set_char(']',index);
				set_pos(index+1);
				if(pnode) {
					VRMLFaceSet *faceset = pnode->to_geometry()->to_faceset();
					faceset->SetTexCoord(texcoord);
				}
			} 
			else if(IsEON(str)){ 
				quit = true;
			}
			else { 
				iret = VRML_ERR_BAD_WORD; 
				quit = true;
			}
		}
	}
	ADD_NODE_BY_CODE(iret,pnode,texcoord);
	return iret;
}


int VRMLParser::parse_color(sfpnode pnode){
	cStr str;
	int idef = 0;
	if((idef = extract_def_name(str))==DEF){
		extract_word(str);
	} else if (idef==USE) {
		sfpnode node = m_scene->GetDefsNode(m_defName);	
		m_defName.Clear();
		VRMLFaceSet *faceset = pnode->to_geometry()->to_faceset();
		faceset->SetColor(node->to_color());
		node->Retain();
		return 0;
	}
	if(IsEqual(str,VRML_COLOR_NODE_NAME)) {
		if(get_char()!='{') {
			if(check_braces('{')<0) 
				return VRML_ERR_BAD_SYMBOL;
		}
	} else return VRML_ERR_BAD_WORD;
  
	VRMLColor *color = new VRMLColor();
	set_defname(color);
	mfcolor &rgbColors = color->GetColors();
	int iret = parse_points<rgb>(VRML_COLOR_PROPERTY,rgbColors);
	if(iret>=0){
		if(pnode){
			VRMLFaceSet *faceset = pnode->to_geometry()->to_faceset();
			faceset->SetColor(color);
		}
	}
	ADD_NODE_BY_CODE(iret,pnode,color);
	return iret;
}

int VRMLParser::parse_indecies(mfint32 &idxList){
	if(get_char()!='[') {
		if(check_braces('[')<0) { 
			return VRML_ERR_BAD_SYMBOL;
		}
	} else { m_pos++; }
  
	int index = m_buffer.IndexOf(']',m_pos);
  
	if(index!=-1) {
		const char *sz = TO_CURR_PTR();
	//	set_char('\0',index);
		VRMLStringConversion::ToInt32List(sz,idxList);
	//	set_char(']',index);
		set_pos(index+1);
	} else return VRML_ERR_BAD_SYMBOL; 
  
	return 0;
}

int VRMLParser::parse_coord_index(sfpnode pnode) {
	VRMLGeometry *geometry = pnode->to_geometry();
	if(geometry){
		VRMLFaceSet *faceset = 	geometry->to_faceset();
		mfint32 &coordIdx = faceset->CoordIndex();
		return parse_indecies(coordIdx);
	}
	return -1;
}

int VRMLParser::parse_norm_index(sfpnode pnode){
	VRMLGeometry *geometry = pnode->to_geometry();
	if(geometry){
		VRMLFaceSet *faceset = 	geometry->to_faceset();
		mfint32 &normIdx = faceset->NormIndex();
		return parse_indecies(normIdx);
	}
	return -1;
}

int VRMLParser::parse_texcoord_index(sfpnode pnode){
	VRMLGeometry *geometry = pnode->to_geometry();
	if(geometry) {
		VRMLFaceSet *faceset = 	geometry->to_faceset();
		mfint32 &coordIdx = faceset->TexCoordIndex();
		return parse_indecies(coordIdx);
	}
	return -1;
}

int VRMLParser::parse_color_index(sfpnode pnode){
 	VRMLGeometry *geometry = pnode->to_geometry();
	if(geometry){
		VRMLFaceSet *faceset = 	geometry->to_faceset();
		mfint32 &colorIdx = faceset->ColorIndex();
		return parse_indecies(colorIdx);
	}
	return -1;
}

int VRMLParser::parse_faceset(sfpnode pnode){
	if(get_char()!='{') {
		if(check_braces('{')<0) { 
			return VRML_ERR_BAD_SYMBOL;
		}
	 } 
	VRMLFaceSet *faceset = new VRMLFaceSet();
	set_defname(faceset);
 
	int iret=-1;
	bool quit=false;
	cStr str;
	while(!quit) {
		iret = GetLine();
		if (iret==VRML_EOF) break;
		if (iret>0) {
			extract_word(str);
			if(IsEON(str)){
				quit = true;
			}
			else {
				int tag = tag_token(str);
				const char* sz = TO_CURR_PTR();
				const char* pz = sz;
				switch(tag){
				case COORD:		
					 iret=parse_coord(faceset);
					 break;
				case NORMAL:	
					 iret=parse_normal(faceset);
					 break;
				case TEXCOORD:	
					 iret=parse_texcoord(faceset);
					 break;
				case COLORINDEX: 
					 iret=parse_color_index(faceset); 
					 break;
				case COORDINDEX: 
					 iret=parse_coord_index(faceset);
					 break;
				case NORMINDEX:  
					 iret=parse_norm_index(faceset);
					 break;
				case TEXCOORDINDEX:
					 iret=parse_texcoord_index(faceset); 
					 break;
				case COLOR:
					 iret=parse_color(faceset);
					 break;
				case SOLID:
					 {
						sfbool val = VRMLStringConversion::ToBoolean((CPCH*)&sz);
						faceset->SetSolid(val);
					 }
					 break;
				case CREASEANGLE:
					 {
						sffloat val = VRMLStringConversion::ToFloat((CPCH*)&sz);
						faceset->SetCreaseAngle(val);
					 }		
					 break;
				case COLORPERVERT:
					 {
						sfbool val = VRMLStringConversion::ToBoolean((CPCH*)&sz);
						faceset->SetColorPerVertex(val);
					 } 
					 break;
				case NORMPERVERT:
					 {
						sfbool val = VRMLStringConversion::ToBoolean((CPCH*)&sz);
						faceset->SetNormalPerVertex(val);
					 }
					 break;
				case CONVEX:
					 {
						sfbool val = VRMLStringConversion::ToBoolean((CPCH*)&sz);
						faceset->SetConvex(val);
					 } 
					 break;
				case CCW: 
					 {
						sfbool val = VRMLStringConversion::ToBoolean((CPCH*)&sz);
						faceset->SetCCW(val);
					 }
					 break;
				case NONE: 
					 {
						iret = VRML_ERR_BAD_SYMBOL; 
					 }
					 break;
				} //switch
				if(iret < 0) quit = true;
				else set_pos(m_pos + sz - pz);
			} //else
		}
	} // while
	if(iret>=0){
		VRMLChild* child = pnode->to_child();
		if(child) { 
			VRMLShape *shape = child->to_shape();
			if(shape) shape->SetGeometry(faceset);
		}
	} else {
		delete faceset;
		faceset=NULL;
	}
	ADD_NODE_BY_CODE(iret,pnode,faceset);
	return iret;
}

int VRMLParser::parse_geometry(sfpnode pnode) {
	cStr str;
	int idef=0;
	if((idef=extract_def_name(str))==DEF){
		extract_word(str);
	} else if(idef==USE) {
		sfpnode node = m_scene->GetDefsNode(m_defName);	
		m_defName.Clear();
		if(node){
			VRMLShape* shape = pnode->to_child()->to_shape(); 
			shape->SetGeometry(node->to_geometry());
			node->Retain();
		}
		return 0;
	}
	int iret = -1;
	if(IsEqual(str,VRML_GEO_IndexedFaceSet_TYPE)) {
		iret = parse_faceset(pnode);
	} else {
		iret = skip_nodes();
	}
	return iret;
}

int VRMLParser::parse_material(sfpnode pnode)
{
	int iret = -1;
	iret = check_start_node(VRML_MATERIAL_NODE_TYPE);
	if(iret<0) {
	   return iret;
	}else if(iret==USE){
	    sfpnode node = m_scene->GetDefsNode(m_defName);	
		m_defName.Clear();
		if(node){
			VRMLAppearance *appearance = pnode->to_appearance();
			appearance->SetMaterial(node->to_material());
			node->Retain();
		}
		return iret;
	} 
	VRMLMaterial *material = new VRMLMaterial();
	set_defname(material);
	bool quit=false;
	while(!quit) {
		iret = GetLine();
		if(iret==VRML_EOF) break;
		if(iret>0) 
		{
			int offs_pos = 0;
			cStr str;
			extract_word(str);
			char *sz = TO_CURR_PTR();
			char *pz = sz;
			if(IsEON(str)){
				quit = true;
	    	}
			else if(IsEqual(str,VRML_EMISSIVE_COLOR_PROPERTY))
			{
				sfvec3f e =  VRMLStringConversion::ToVector3((CPCH*)&sz);
				material->SetEmissive(e);
			} 
			else if(IsEqual(str,VRML_SPECULAR_COLOR_PROPERTY))
			{
				sfvec3f s =  VRMLStringConversion::ToVector3((CPCH*)&sz);
				material->SetSpecular(s);
			} 
			else if(IsEqual(str,VRML_DIFFUSIVE_COLOR_PROPERTY))
			{
				sfvec3f d =  VRMLStringConversion::ToVector3((CPCH*)&sz);
				material->SetDiffuse(d);
			}
			else if(IsEqual(str,VRML_SHININESS_PROPERTY ))
			{
				sffloat val = VRMLStringConversion::ToFloat((CPCH*)&sz);
				material->SetShininess(val);
			}
			else if(IsEqual(str,VRML_TRANSPARENCY_PROPERTY))
			{
				sffloat val = VRMLStringConversion::ToFloat((CPCH*)&sz);
				material->SetTransparency(val);
			}
			else if(IsEqual(str,VRML_AMBIENT_INTENSITY_PROPERTY))
			{
				sffloat val = VRMLStringConversion::ToFloat((CPCH*)&sz);
				material->SetAmbient(val);
			}
			else 
			{ 
				iret = VRML_ERR_BAD_WORD; 
				quit=true;
			}
			if(iret >=0 ) set_pos(m_pos+sz-pz);
	 	}
	}
  	if(iret>=0){
	  	pnode->to_appearance()->SetMaterial(material);
  	} else { 
	 	 delete material; 
	 	 material=NULL;
  	}
	ADD_NODE_BY_CODE(iret,pnode,material);
	return iret;
}

int VRMLParser::parse_image_tex(sfpnode pnode){
 
	if(get_char()!='{') {
		if(check_braces('{')<0) { 
			return VRML_ERR_BAD_SYMBOL;
		} 
	}
 
 	VRMLImageTexture *texture = new VRMLImageTexture();
	set_defname(texture);
 
	int iret=-1;
	bool quit=false;
 
	while(!quit) {
		iret = GetLine();
		if(iret==VRML_EOF) break;
		if(iret>0){
	   		cStr str;
 	   		extract_word(str);
	   		if(IsEqual(str,VRML_URL_PROPERTY)) {
		   		if (extract_url_name(str)) {
			   		texture->SetUrl(str);
		   		} else iret = VRML_ERR_BAD_SYMBOL;
	   		} else if (IsEON(str)){
		   		quit = true;
	   		} if(iret < 0) quit = true;
		}
 	}
 	if(iret>=0){
	  	pnode->to_appearance()->SetTexture(texture);
  	} else { 
	  	delete texture; 
	  	texture=NULL;
  	}
	ADD_NODE_BY_CODE(iret,pnode,texture);
	return iret;
}

int VRMLParser::parse_pixel_tex(sfpnode pnode){
	if(get_char()!='{') {
		if(check_braces('{')<0) { 
			return VRML_ERR_BAD_SYMBOL;
		}
 	}
 	VRMLPixelTexture *texture = new VRMLPixelTexture();
 	set_defname(texture);
 	int iret=-1;
 	bool quit=false;
 	while(!quit) {
		iret = GetLine();
		if(iret==VRML_EOF) break;
		if(iret>0){
	   		cStr str;
 	   		extract_word(str);
	   		char *sz = TO_CURR_PTR();
	   		char *pz = sz;
	   		if(IsEqual(str,VRML_IMAGE_PROPERTY)) {
				sfimage &image = texture->GetPixelImage();
		   		sfint32 width = VRMLStringConversion::ToInt32((CPCH*)&sz);
		   		image.SetWidth((uint32)width);
		   		sfint32 height = VRMLStringConversion::ToInt32((CPCH*)&sz);
		   		image.SetHeight((uint32)height);
		   		sfint32 channel = VRMLStringConversion::ToInt32((CPCH*)&sz);
		   		image.SetChannel((uint32)channel);
		   		// read pixel data
		   		for(int32 j=0;j<height;j++)
			  		for(int32 i=0;i<width;i++){
				 		image.SetPixel(i,j,VRMLStringConversion::HexToUInt32((CPCH*)&sz));
		   			}
	   		} else if (IsEON(str)){
		   		quit = true;
	   		} 
			if(iret < 0) quit = true;
	   		else  { set_pos(m_pos+sz-pz); }
		}
	}
 	if(iret>=0){
	  	pnode->to_appearance()->SetTexture(texture);
  	} else { 
	  	delete texture; 
	  	texture=NULL;
  	}
	ADD_NODE_BY_CODE(iret,pnode,texture);
	return iret;
}

int VRMLParser::parse_texture(sfpnode pnode){
    int iret = -1;
    cStr str;
    int idef=0;
    if((idef=extract_def_name(str))==DEF){
  	    extract_word(str);
    } else if(idef==USE) {
  		sfpnode node = m_scene->GetDefsNode(m_defName);	
  		m_defName.Clear();
  		if(node){
  			VRMLAppearance *appearance = pnode->to_appearance();
  			appearance->SetTexture(node->to_texture());
  			node->Retain();
  		}
  		return 0;
    }
    if(IsEqual(str,VRML_TEX_ImageTexture_TYPE)){
       iret = parse_image_tex(pnode);
    }
    else if (IsEqual(str,VRML_TEX_PixelTexture_TYPE))
    {
  	  iret = parse_pixel_tex(pnode);
    }
    else if (IsEqual(str,VRML_TEX_MovieTexture_TYPE))
    {
  	  iret = skip_nodes();
	}
    else { iret = VRML_ERR_BAD_WORD; }
    return iret;	
}

int VRMLParser::parse_appearance(sfpnode pnode) 
{
    int iret = -1;
    iret = check_start_node(VRML_APPEARANCE_NODE_TYPE);
    if(iret<0) {
  	  return iret;
    } else if(iret==USE) {
  		sfpnode node = m_scene->GetDefsNode(m_defName);	
  		m_defName.Clear();
  		if(node){
  			VRMLShape* shape = pnode->to_child()->to_shape(); 
  			shape->SetAppearance(node->to_appearance());
  			node->Retain();
  		}
  		return iret;
    }
    VRMLAppearance *appearance = new VRMLAppearance();
    set_defname(appearance);
    bool quit=false;
    while(!quit) {
  	iret = GetLine();
  	if(iret==VRML_EOF) break;
  	if(iret>0){
  		cStr str;
  		extract_word(str);
  		if(IsEqual(str,VRML_MATERIAL_NODE_TYPE)){
  			iret = parse_material(appearance);
  		}
  		else if(IsEqual(str,VRML_TEXTURE_NODE_TYPE)){
  			iret = parse_texture(appearance);
  		} else if(IsEqual(str,VRML_TEXTURE_TRANSFORM_NODE_TYPE)){
  			iret = skip_nodes();
  		} else if(IsEON(str)) {
  		   quit = true;
  	    } else {
  			iret = VRML_ERR_BAD_WORD;
  		}
  		if(iret < 0) quit = true;
  	 }
    }
    if(iret>=0){
  		VRMLChild* child = pnode->to_child();
  		if(child) { 
  			VRMLShape *shape = child->to_shape();
  			if(shape) shape->SetAppearance(appearance);
  		}
  	} else {
  		delete appearance;
  		appearance=NULL;
  	}
    ADD_NODE_BY_CODE(iret,pnode,appearance);
    return iret;
}

int VRMLParser::parse_shape(sfpnode pnode) {
   
	if(get_char()!='{') {
 		if(check_braces('{')<0) { 
 	   		return VRML_ERR_BAD_SYMBOL;
 		}
   	} else set_pos(m_pos+1);

    VRMLShape *pshape = new VRMLShape();
    set_defname(pshape);
    
    int iret = -1;
    bool quit=false;
    cStr str;
    while(!quit) {
  		iret = GetLine();
  		if(iret==VRML_EOF) break;
  		if(iret>0){
  			extract_word(str);
  			if(IsEqual(str,VRML_GEOMETRY_NODE_TYPE)){
  				iret = parse_geometry(pshape);
  			}
    		else if(IsEqual(str,VRML_APPEARANCE_NODE_TYPE)){
    			iret = parse_appearance(pshape);
    		} 
    		else if(IsEON(str)) {
    		   quit = true;
    	    }
    		else {
    			iret = VRML_ERR_BAD_WORD; quit = true;
    		}
    	}
    }
    ADD_NODE_BY_CODE(iret,pnode,pshape);
    return iret;
}

int VRMLParser::parse_children(sfpnode pnode){
    bool quit = false;
    bool expectedEnd = false;
    int iret = VRML_ERR_NO;
    int end_child = -1;
    while(!quit) {
  		iret = GetLine();
  		if(iret==VRML_EOF) { 
  			if(expectedEnd) return VRML_ERR_BAD_SYMBOL;
  			else			return -1;
  		}
  		if(iret > 0) {
  			if(expectedEnd){
  				int Len = m_buffer.Length();
  				int ipos = skip_spaces(m_pos,Len);
  				if (ipos!=-1) { // symbol
  					if (get_char()==']') {
  						set_pos(m_pos+1);
  				    	expectedEnd = false;
  				    	quit=true;
  					}
  				}
  			}
  			else {
  				int Len = m_buffer.Length();
  				int ipos = skip_spaces(m_pos,Len);
  				if (ipos!=-1) { // symbol
  					m_pos = ipos;
  					if (get_char()=='[') {
  						expectedEnd = true;
  						end_child = calc_end_index_child();
  						m_pos++;
  					}
  					if(!expectedEnd) {
  						end_child = 0;
  					}
  					iret  = parse_nodes(pnode,end_child);
  					if(iret < 0)		  { quit = true; break;}
  					else if(!expectedEnd) { quit = true; break;}
  				} else {
  					m_pos = Len;
  				}
  			}
  		}
    } 
	return iret;
}


int VRMLParser::parse_nodes(sfpnode pnode, int end_pos ) {
	bool quit=false;
	int iret = -1;
	while(!quit) {
		iret = GetLine();
		if(iret==VRML_EOF) return 0;
		if(iret>0){
			int inode = 0;
			cStr str;
			int index = m_buffer.IndexOf('{',m_pos);
			if(index == -1){
				int ipos = m_pos;
				while((iret = GetLine())!=-1) {
					index = m_buffer.IndexOf('{',m_pos);
					if(index>=0) break;
					else m_pos = m_buffer.Length();
				}
				if(index == -1){
					if(end_pos > 0) { 
						m_pos = ipos;
				   		return 0;
					} else return 0;
				}
			}
			if(index >= 0){
				if(end_pos > 0){
			   		if(index > end_pos) return iret;
				}
				str.Clear();
				str = m_buffer.Substring(m_pos,index-m_pos);
				m_pos = index;
				GetToken(str);
			} 
			if(iret < 0) { 
				quit = true; 
				break; 
			}
    		if(IsEqual(m_tokens[inode],VRML_DEF_KEY)){
    		    inode = 2;
    		    m_defName = m_tokens[1];
    		} 
    		else if(IsEqual(m_tokens[inode],VRML_USE_KEY)){
    		    inode = 1;
    		    cStr useName = m_tokens[1];
    		    sfpnode node = m_scene->GetDefsNode(useName);
    		    AddNode(pnode,node);
    		    node->Retain();
    		    break;
    		}
    		else if(IsEqual(m_tokens[inode],VRML_CHILD_NODE_TYPE)){
    		    inode = 1;
    		}
    		if(IsEqual(m_tokens[inode],VRML_GROUP_NODE_TYPE)){
    		    iret = parse_group(pnode);
    		}
    		else if(IsEqual(m_tokens[inode],VRML_TRANSFORM_NODE_TYPE)){
    		    iret = parse_transform(pnode);
    		}
    		else if(IsEqual(m_tokens[inode],VRML_SHAPE_NODE_TYPE)){
    		    iret = parse_shape(pnode);
    		} else {
    		    // todo... check correctly nodes
    		    iret = skip_nodes(); 
    		}
    		if(iret < 0 || end_pos==0){
    			// only one node in child		
    			quit = true;
    		}
		}
	}
	return iret; // iret < 0 - error else OK
}

int VRMLParser::Run() {
	if(m_scene==NULL) return -1;
  
	if(GetBufferData())
	{
		return parse_nodes(NULL);
	} 	return -1;
}

void VRMLParser::init_token(){
    
	for(int i=0;i < HashSize;i++){ 
		HashTab[i]=NULL;
		TokHash[i]=NULL;
    }
    int j = 0;
    TokTab *t = TokTable;
    while(t->TokName) {
		j = hash(t->TokName);
		t->next = TokHash[j];
		TokHash[j] =t;
		t++;
    }
}

int VRMLParser::hash(const char *s) {
	int j = 0;
	while(*s!='\0') { j =(j + (*s++)) & (HashSize-1);}
	return j;
}

int VRMLParser::tag_token(cStr &s){
    TokTab *t;
    t = TokHash[hash(s.ToCharPtr())];
    while( t ) { 
  	  if (IsEqualNoCase(s,t->TokName))
           return t->TokValue;
           t = t->next;
	} 
	return 0;
}
