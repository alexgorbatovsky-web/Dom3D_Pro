/*
 * File     : VRMLParser.h
 * Purpouse : VRML Parser class definition 
 * Data		: 05/04/2013
 */

#ifndef __VRMLParser_H__
#define __VRMLParser_H__

#define ADD_NODE_BY_CODE(code,p,n) if(code >= 0) AddNode(p,n);  else { if (n) delete n; n = NULL; }
#define TO_CURR_PTR() (char*)(m_buffer.ToCharPtr()+m_pos)
#define TO_PTR() (char*)(m_buffer.ToCharPtr())

#define HashSize 32

struct TokTab;
struct Token;

class VRMLScene;
class VRMLParser {
public:
	VRMLParser(VRMLScene* toScene, cData& inData):
	m_scene(toScene)
	,m_data(inData)
	,m_pos(0)
	,m_len(0) { init_token();}
	~VRMLParser(){}
	
	int Run();
	
	bool IsEqual(const cStr &s1, const char* s2);
	bool IsEqualNoCase(const cStr &s1, const char* s2);
	bool IsEON(cStr &str);
	
	int GetLine();
	int GetBufferData();
	int GetToken(cStr &str);
		
	void AddNode(sfpnode p, sfpnode n);


protected:
	int parse_group(sfpnode pnode);
	int parse_shape(sfpnode pnode);
	int parse_transform(sfpnode pnode);
	int parse_children(sfpnode pnode);
	int parse_appearance(sfpnode pnode);
	int parse_geometry(sfpnode pnode);
	int parse_faceset(sfpnode pnode);
	int parse_material(sfpnode pnode);
	int parse_texture(sfpnode pnode);
	int parse_image_tex(sfpnode pnode);
	int parse_pixel_tex(sfpnode pnode);
	int parse_coord(sfpnode pnode); 
	int parse_normal(sfpnode pnode);
	int parse_texcoord(sfpnode pnode);
	int parse_color_index(sfpnode pnode);
	int parse_coord_index(sfpnode pnode);
	int parse_norm_index(sfpnode pnode);
	int parse_texcoord_index(sfpnode pnode);
	int parse_color(sfpnode pnode);
	int parse_indecies(mfint32 &idxList);
	int parse_nodes(sfpnode pnode, int endpos=-1);
	template<class T>
	int parse_points(const char* key, mfarray<T> &points);
	
private:
	void extract_by_seps(cStr &dst, char* seps);
	void extract_word(cStr &dst);
	int  extract_def_name(cStr &str);
	bool extract_url_name(cStr &str);
	
	int calc_end_index_child();

	char get_char(int pos=-1); 
	void set_char(char ch, int pos); 
	void set_defname(sfpnode pnode);
	int  check_start_node(const char* name);
	int  check_braces(char chbr);
	int  skip_spaces(int iStart, int iCount);
	int  skip_nodes();
	void skip_floats(int n);
	void set_pos(int pos);
	void init_token();
	int tag_token(cStr &name);
	int hash(const char* name);
private:	
	TokTab* TokHash[HashSize];
    Token*  HashTab[HashSize];
   	VRMLScene *m_scene;
	StringsList m_tokens;
	cData &m_data;
	cStr m_buffer;
	cStr m_defName;
	int  m_pos;
	int  m_len;
	bool m_bexpected;
};
#endif
