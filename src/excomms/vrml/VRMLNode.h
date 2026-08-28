/* 
 * File	    : VRMLNode.h
 * Purpouse : VRML nodes definitions
 * Data	    : 04/04/2013
 */

#ifndef __VRML_NODE_H__
#define __VRML_NODE_H__

class VRMLContext;
class VRMLMaterial;
class VRMLTexture;
class VRMLAppearance;
class VRMLNode;
class VRMLColor;
class VRMLGroupingBase;
class VRMLChild;
class VRMLGeometry;
class VRMLNormal;
class VRMLCoordinate;
class VRMLTextureCoordinate;
class VRMLTextureTransform;
class VRMLTransform;
class VRMLFaceSet;
class VRMLShape;
class VRMLGroup;

typedef VRMLNode sfnode;
typedef VRMLNode* sfpnode;

class VRMLNode {
public:
	typedef
	enum node_type {
		none,
		appearance_node,
		back_node,
		bounded_volume_node,
		child_node,
		color_node,
		color_rgba_node,
		coordinate_node,
		normal_node,
		geometry_node,
		material_node,
		texture_node,
		texture_coordinate_node,
		texture_transform_node,
		grouping_node,
		group_node,
		transform_node
   	} node_type;
	VRMLNode(const node_type _type)
	{ 
		refcount = 1;
		type = _type;
		parent = NULL;
	}

	virtual ~VRMLNode(){;}

	const node_type& TypeID() const { return type; }

	const cStr& DefID() const { return defID;}
	const cStr& NodeName() const { return name;}
	
	void SetDefID (const char *_defID) { defID = _defID;}
	void SetNodeName(const char *_name) { name = _name;}
	
	int GetRefcount() { return refcount;}
	
	VRMLNode* GetParent(){ return parent; }
	const VRMLNode* GetParent() const { return parent; }

	void SetParent(VRMLNode* _parent) { parent = _parent; }

	void Retain() { refcount++;}
	void Release() { if(!(--refcount)) delete this; }
	
	virtual VRMLAppearance* to_appearance();
	virtual VRMLMaterial* to_material();
    virtual VRMLTexture* to_texture();
	virtual VRMLColor* to_color();
	virtual VRMLGroupingBase* to_grouping();
	virtual VRMLChild* to_child();
	virtual VRMLGeometry* to_geometry();
	virtual VRMLNormal* to_normal();
	virtual VRMLCoordinate* to_coordinate();
	virtual VRMLTextureCoordinate* to_tex_coordinate();
	virtual VRMLTextureTransform* to_tex_transform();
	virtual VRMLTransform* to_transform();
	virtual VRMLGroup* to_group();

	virtual void AddChild(VRMLNode* child) {}

	virtual bool render(VRMLContext &context){ return false; }
protected:
	VRMLNode* parent;
private:
	node_type type;
	cStr defID;
	cStr name;
	int refcount;
};

class VRMLMaterial:public VRMLNode {
public:
	VRMLMaterial():VRMLNode(material_node)
	,ambient(0)
	,transparency(0)
	,shininess(0)
	,diffuse(sfcolor::Zero)
	,specular(sfcolor::Zero)
	,emissive(sfcolor::Zero)
	{}
	
	virtual VRMLMaterial* to_material() { return this;}
    
	void SetDiffuse(sfcolor &_diffuse) { diffuse = _diffuse;}
	void SetEmissive(sfcolor &_emissive) { emissive=_emissive;}
	void SetSpecular(sfcolor &_specular) { specular = _specular;}
	void SetAmbient(sffloat _ambient) { ambient=_ambient;}
	void SetShininess(sffloat _shininess) { shininess = _shininess; }
	void SetTransparency(sffloat _transparency) { transparency=_transparency;}
	
	sfcolor& GetDiffuse() { return diffuse; }
	sfcolor& GetEmissive(){ return emissive; }
	sfcolor& GetSpecular(){ return specular; }
	sffloat GetAmbient() { return ambient; }
	sffloat GetShininess() { return shininess; }
	sffloat GetTransparency() { return transparency;}
	
private:
    sfcolor diffuse;
    sfcolor emissive;
    sfcolor specular;
    sffloat ambient;
    sffloat shininess;
	sffloat transparency;
};

class VRMLTexture:public VRMLNode {
public:
	VRMLTexture():VRMLNode(texture_node){}
	virtual ~VRMLTexture() {;}
	enum texture_type {
		none,
		image,
		pixel
	};
	virtual VRMLTexture* to_texture() { return this;}
	
	virtual const texture_type TexType() const = 0;
	void SetTexType(texture_type _type) { type = _type; }
private:
   texture_type type;
};


class VRMLTextureTransform:public VRMLNode {
public:
	VRMLTextureTransform():VRMLNode(texture_transform_node){}
	~VRMLTextureTransform(){}
	virtual VRMLTextureTransform* to_tex_transform() { return this;}
	void Center(sfvec2f c) {  center = c;}
	void Rotation(sffloat r) { rotation = r; }
	void Scale(sfvec2f s) { scale = s; }
	void Translation(sfvec2f t) { translation = t; }
	
	const sfvec2f& Center() const { return center; }

	comms::cMat3& GetMatrix() { return matrix;}
	void BuildMatrix();
private:
	sfvec2f center;
	sffloat rotation;
	sfvec2f scale;
	sfvec2f translation;
	mat3f matrix;
};

class VRMLAppearance:public VRMLNode {
public:
	VRMLAppearance():VRMLNode(appearance_node)
	,material(NULL)
	,texture(NULL)
	,texture_transform(NULL){}
	virtual ~VRMLAppearance();
	VRMLAppearance* to_appearance() { return this;}
	
	VRMLMaterial* GetMaterial() { return material ? (material->to_material()):0;} 
	VRMLTexture* GetTexture() { return texture ? (texture->to_texture()):0; }
	VRMLTextureTransform* GetTextureTransform() { return texture ? (texture->to_tex_transform()):0; }
	
	void SetMaterial(VRMLMaterial* _material) { material =  STCAST_OBJ(_material,sfpnode); }
	void SetTexture(VRMLTexture* _texture) { texture =  STCAST_OBJ(_texture,sfpnode); }
	void SetTextureTransform(VRMLTextureTransform* _textransform) { texture_transform =  STCAST_OBJ(_textransform,sfpnode); }

private:
	VRMLNode* material;
	VRMLNode* texture;
	VRMLNode* texture_transform;
};


class VRMLColor:public VRMLNode {
public:
	VRMLColor():VRMLNode(color_node){}
	~VRMLColor(){}
	virtual VRMLColor* to_color() { return this;}
	cList<rgb> &GetColors() { return colors;}
private:
   mfcolor colors;
};

class VRMLGeometry:public VRMLNode {
public:
	VRMLGeometry():VRMLNode(geometry_node){}
	enum geometry_type {
	   box,
	   cone,
	   cylinder,
	   sphere,
	   faceset
	};

	virtual ~VRMLGeometry(){}

	virtual VRMLGeometry* to_geometry() { return this;}
	virtual VRMLFaceSet* to_faceset() = 0;

	void SetGeoType(geometry_type _type) { type = _type; }
	const geometry_type GetGeoType() const { return type; }
private:
	geometry_type type;
};

class VRMLChild:public VRMLNode {
public:
	VRMLChild():VRMLNode(child_node){}
	virtual ~VRMLChild(){}
	enum child_type{
		none,
		shape
	};
	virtual VRMLChild* to_child() { return this;}
	virtual VRMLShape* to_shape();
	
	void ChildType(child_type _type) { c_type = _type;}
	const child_type ChildType() const { return c_type; }
private:
	child_type c_type;
};

class  VRMLShape: public VRMLChild {
public:
	VRMLShape():VRMLChild()
	,appearance(0)
	,geometry(0) { 
	  ChildType(shape);
	}
	virtual ~VRMLShape();

	virtual VRMLShape* to_shape() { return this;}

	void SetAppearance(VRMLAppearance* _appearance) { 
		appearance = STCAST_OBJ(_appearance,sfpnode); 
	}
	
	void SetGeometry(VRMLGeometry* _geometry) { 
		geometry = STCAST_OBJ(_geometry,sfpnode); 
	}

	VRMLAppearance* GetAppearance() { 
		return appearance?(appearance->to_appearance()):0; 
	}
	VRMLGeometry* GetGeometry() { 
		return geometry?(geometry->to_geometry()):0; 
	}

private:
	sfpnode appearance;
	sfpnode geometry;
	sfvec3f bbox_center;
    sfvec3f bbox_size;
};

class VRMLCoordinate:public VRMLNode {
public:
	VRMLCoordinate():VRMLNode(coordinate_node){}
	~VRMLCoordinate(){}
	virtual VRMLCoordinate* to_coordinate() {return this; }
	mfvec3f& GetPoints() { return points; }
private:
	mfvec3f points;
};
class VRMLNormal:public VRMLNode{
public:
	VRMLNormal():VRMLNode(normal_node){}
	virtual VRMLNormal* to_normal() { return this; }
	mfvec3f& GetPoints() { return points; }
private:
	mfvec3f points;
};


class VRMLTextureCoordinate:public VRMLNode {
public:
	VRMLTextureCoordinate():VRMLNode(texture_coordinate_node){
	}
	virtual VRMLTextureCoordinate* to_tex_coordinate() { return this; }
	mfvec2f& GetPoints() { return points; }
private:
	mfvec2f points;
};

class VRMLFaceSet:public VRMLGeometry {
public:
	VRMLFaceSet():VRMLGeometry()
	,coord(NULL)
	,normal(NULL)
	,color(NULL)
	,tex_coord(NULL)
	,ccw(false)
	,convex(false)
	,crease_angle(0)
	,normal_per_vertex(true)
	,solid(true) {
		SetGeoType(faceset); 
	}
	
	virtual ~VRMLFaceSet();

	VRMLFaceSet* to_faceset() { return this; }

	VRMLCoordinate* Coord() { return coord ? (coord->to_coordinate()):0;}
	VRMLNormal* Normal() { return normal ? (normal->to_normal()):0;}
	VRMLTextureCoordinate* TexCoord() { return tex_coord ? (tex_coord->to_tex_coordinate()):0;}
	VRMLColor *Color(){ return color ? (color->to_color()):0;}

	mfint32 & CoordIndex() { return coord_index; }
	mfint32 & NormIndex() { return normal_index; }
	mfint32 & TexCoordIndex() { return tex_coord_index; }
	mfint32 & ColorIndex() { return color_index; }

	sfbool NormalPerVertex() { return normal_per_vertex;}
	sfbool ColorPerVertex() { return color_per_vertex; }
		
	void SetCoord(VRMLCoordinate* _coord)	{ coord = STCAST_OBJ(_coord,sfpnode);  }
	void SetNormal(VRMLNormal* _normal)		{ normal = STCAST_OBJ(_normal,sfpnode); }
	void SetTexCoord(VRMLTextureCoordinate* _texcoord) { tex_coord = STCAST_OBJ(_texcoord,sfpnode);}
	void SetColor(VRMLColor* _color) { color = STCAST_OBJ(_color,sfpnode); }
	void SetNormalPerVertex(sfbool value)	{ normal_per_vertex=value; }
	void SetColorPerVertex(sfbool value)	{ color_per_vertex = value; }	
	void SetCCW(sfbool _ccw) { ccw = _ccw; }
	void SetSolid(sfbool _solid) { solid = _solid; }
	void SetCreaseAngle(sffloat value) { crease_angle = value;}
	void SetConvex(sfbool _convex) { convex = _convex;}

private:
	sfpnode coord;				// vertex coord
	sfpnode normal;				// normal coord
    sfpnode tex_coord;			// tex coord
	sfpnode color;				// color
    mfint32 coord_index;		// coordIndex
	mfint32 normal_index;		// normalIndex
    mfint32 tex_coord_index;	// texCoordIndex
	mfint32 color_index;		// colorIndex
	sfbool color_per_vertex;	// colorPerVertex
	sfbool normal_per_vertex;	// normalPerVertex
    sfbool ccw;					// ccw
    sfbool convex;				// convex
    sffloat crease_angle;		// creaseAngle
   	sfbool solid;				// solid
};

class VRMLImageTexture:public VRMLTexture {
public: 
	VRMLImageTexture():VRMLTexture(){
	 SetTexType(image);
	}
	const cStr& GetUrl() const { return url;}
	void SetUrl(const cStr & _url) { url=_url; }
	
	void SetRepeatS(const sfbool repS) { repeatS = repS;}
	void SetRepeatT(const sfbool repT) { repeatT = repT;}

	virtual const texture_type TexType() const { return image;}

	sfbool GetRepeatS() { return repeatS;}
	sfbool GetRepeatT() { return repeatT;}

private:
	cStr url;
	sfbool repeatS;
	sfbool repeatT;
};

class VRMLPixelTexture:public VRMLTexture {
public: 
	VRMLPixelTexture():VRMLTexture(){
	 SetTexType(pixel);
	}
	sfimage &GetPixelImage() { return pix_image; }

	virtual const texture_type TexType() const { return pixel; }

private:
	sfimage pix_image;
};

class VRMLGroupingBase:public VRMLNode {
public:
	VRMLGroupingBase(const node_type _type):VRMLNode(_type){}
	virtual ~VRMLGroupingBase();
	
	virtual VRMLGroupingBase* to_grouping() { return this;}
	
	virtual VRMLGroup* to_group() { return NULL; }
	virtual VRMLTransform* to_transform() { return NULL;}

	virtual void AddChild(VRMLNode *node) { children.Add(node);}
	cList<VRMLNode*>& Children() { return children; }
private:
	cList<VRMLNode*> children;
};


class VRMLGroup:public VRMLGroupingBase {
public:
	VRMLGroup():VRMLGroupingBase(group_node){}
	virtual ~VRMLGroup(){}
	virtual VRMLGroup* to_group() { return this; }
};

class VRMLTransform:public VRMLGroupingBase {
public:
	VRMLTransform():VRMLGroupingBase(transform_node){
		mat_transform=mat4f::Identity; 
		center.SetZero();
		rotation.Set(0,0,1,0);
		scale.SetOne();
		scale_orient.Set(0,0,1,0);
		translation.SetZero();
	}
	~VRMLTransform(){}
	virtual VRMLTransform* to_transform() { return this; }
	mat4f& GetMatrix() { return mat_transform; }
	
	void Rotation(sfvec4f r) { rotation = r;}
	void Scale(sfvec3f s) { scale = s; }
	void Translation(sfvec3f t) { translation = t; }
	void ScaleOrient(sfvec4f s_orient) { scale_orient = s_orient; }
	void Center(sfvec3f c) { center = c; }

	const sfvec3f& Center() const  { return center; }
	const sfvec4f& Rotation() const { return  rotation;}
	const sfvec3f& Scale() const { return  scale; }
	const sfvec3f& Translation() const { return  translation; }
	const sfvec4f& ScaleOrient() const { return scale_orient; }

	void BuildMatrix();
	
private:
	sfvec3f center;
	sfvec4f rotation;
	sfvec3f scale;
	sfvec4f scale_orient;
	sfvec3f translation;
	mat4f mat_transform;
};

#endif