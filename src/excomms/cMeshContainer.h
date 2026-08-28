#pragma once

//*****************************************************************************
// cMeshContainer
//*****************************************************************************

class cMeshContainer;

class cVMapElm{
public:
    DWORD   Vertex;
    DWORD   Type;
    int     Dim;
    float   Value[4];
};
class cVMapType{
public:
    cStr  Name;
    DWORD Type;
};
struct OneMorph{
	cStr Name;
	cList<cVec3> Pos;
};
struct PtexQuad{	
	int Nx;
	int Ny;
	cList<DWORD> Color;
};

struct PIndex {
	int idx;
	cList<int> childs;
};

namespace GM {
	class ObjectParam;
	class GM_Composer;
	class GM_PrimitiveObject;
	struct GM_Rect;
};

class MeshCutter;



template< typename V >
class MeshDivider;

typedef void SubdSnapEdgeCallback(void* context, cVec3& pt, cVec3& n, int vc,  int v1, int v2, int weight);
typedef void SubdSnapMiddlePointCallback(void* context, cVec3& pt, cVec3& n, int vc, int v1, int v2, int v3, float w1, float w2);

typedef BigDynArray<cVec3i, 4096> RawArray;
typedef BigDynArray<cVec3, 4096> VecArray;
typedef BigDynArray<cVec2, 4096> UvArray;
typedef BigDynArray<DWORD, 4096> DwordsArray;


/**
\brief General class for work with mesh.
*/
class APICALL cMeshContainer {
public:
	typedef VecArray			figure_t;
	typedef cList< int >        figureI_t;
	typedef cList< figure_t >   figures_t;
	typedef cList< figureI_t >  figuresI_t;
	typedef DWORDS2  edgeI_t;


public:
	typedef void MeshOpHook(cMeshContainer* mc,const char* filename);
	/// \warning Call `CreateDefaultObjMtl()` manually for create a defaul object and material.
	cMeshContainer();
    /**
	\brief Build a correct mesh by set of figures.
	\param fusionDistance  See `MergeFaces()` for details.
	\see Insert( figures_t )Edge
    */
	cMeshContainer( const figures_t&, float fusionDistance );

	~cMeshContainer();

	cMeshContainer( const cMeshContainer& );
	cMeshContainer& operator=( const cMeshContainer& );

    bool operator==( const cMeshContainer& ) const;

    /// \return true when mesh is correct.
	/// \todo fine  Move `ShowWarning` to configure of project or set always `true` when a debug-mode.
	bool IsValid(const bool ShowWarning = false) const;

    /**
	\brief Verify the error (from Blender by import OBJ): "the same vertex of face used multiple times".
	\see cMeshContainerTest::CorrectRawSequence*()
	\see CorrectRawSequence()
    */
	bool IsValidRawSequence() const;
	bool CorrectRawQuadSelfIntersection();
	bool CheckQuadSelfIntersection(cList<PIndex>& polyIntersec);

	void Clear();

	void Copy(const cMeshContainer& src);

	const cStr & GetName() const { return m_Name; }
	void SetName(const char *Name) { m_Name = Name; }

	const VecArray & GetPositions() const { return m_Positions; }
	VecArray & GetPositions() { return m_Positions; }

    /**
	\return Coord (position) by vertex (index of position).
	\details First vertex of mesh is 0.
	@see GetVertex()
    */
	const cVec3 & GetPosition( int vertex ) const;
	void SetPosition( int vertex, const cVec3& );

	/// \return Vertex (index of position) by coord (position).
	/// \see GetPosition()
	int GetVertex( const cVec3& ) const;

	const cList<DWORD> & GetVertexColor() const { return m_VertexColor; }
	cList<DWORD> & GetVertexColor() { return m_VertexColor; }

	const cList<DWORD> & GetVertexSpecular() const { return m_VertexSpecular; }
	cList<DWORD> & GetVertexSpecular() { return m_VertexSpecular; }

	const cList<DWORD> & GetVertexGlossMetallEmissive() const { return m_GlossMetallEmissive; }
	cList<DWORD> & GetVertexGlossMetallEmissive() { return m_GlossMetallEmissive; }

	const cList<DWORD> & GetVertexEmissive() const { return m_VertexEmissive; }
	cList<DWORD> & GetVertexEmissive() { return m_VertexEmissive; }

	const UvArray & GetTexCoords() const { return m_TexCoords; }
	UvArray & GetTexCoords() { return m_TexCoords; }

	const cList<float> & GetNormalDisplacement() const { return m_NormalDisplacement; }
	cList<float> & GetNormalDisplacement() { return m_NormalDisplacement; }
	
	/// \see GetNormal(), GetFaceNormal(), GetFaceCoord()
	const VecArray & GetNormals() const { return m_Normals; }
	VecArray & GetNormals() { return m_Normals; }

    /**
	\return Normal by vertex (index of normal).
	\warning Use CalcNormals() for correct result.
	\see GetFaceNormal(), GetFaceCoord()
    */
	const cVec3 & GetNormal( int vertex ) const;
	void SetNormal( int vertex, const cVec3& );

	const cList<cVec<cVec3, 2> > & GetTangents() const { return m_Tangents; }
	cList<cVec<cVec3, 2> > & GetTangents() { return m_Tangents; }

	const RawArray& GetRaw() const { return m_Raw; }
	RawArray& GetRaw() { return m_Raw; }

	const cList<cSurface> & GetMaterials() const { return m_Materials; }
	cList<cSurface> & GetMaterials() { return m_Materials; }

	const cList<PtexQuad> & GetPtex() const { return m_Ptex; }
	cList<PtexQuad> & GetPtex() { return m_Ptex; }

	const cList<cUVSet> & GetUVSets() const { return m_UVSets; }
	cList<cUVSet> & GetUVSets() { return m_UVSets; }

    const cList<cObject> & GetObjects() const { return m_Objects; }
	cList<cObject> & GetObjects() { return m_Objects; }

    const cList<cVMapElm> & GetVMaps() const { return m_VMaps; }
	cList<cVMapElm> & GetVMaps() { return m_VMaps; }

    const cList<cVMapType> & GetVMTypes() const { return m_VmTypes; }
	cList<cVMapType> & GetVMTypes() { return m_VmTypes; }

	int GetPolyCount() const;
	int GetTrisCount() const;

    /**
	\return List of 'begin raw block' which blocks be adjacent to 'beginRawFigure'.\n
	        Also returns average normal and coord of center of neighbours of face.
	\see Find()
    */
	cList< int >  GetNeighboursForFace(
		int beginRawFigure,
		cVec3* avgNormal = NULL,
		cVec3* avgCenter = NULL
	) const;

	/// \return Point has in the mesh.
	bool Contains( const cVec3& ) const;
	bool Contains( int a ) const;
	/// \return Edge `ab` has in the mesh.
	bool Contains( const cVec3& a, const cVec3& b ) const;
	bool Contains( int a, int b ) const;
	/// \return Figure has in the mesh. Verify by one direction only.
	bool Contains( const VecArray& figure ) const;
	bool Contains( const cList< int >& figure ) const;

	bool IsTriangulated() const;
	void Triangulate(RawArray &TriRaw) const;
	void Triangulate();
	void ToTriQuads();

	void CalcNormals();
	void CalcSplitNormals(cList<int>& SeamsList);

	/// \see Invert()
	void InvertRaw();

	void ConcateWith(const cMeshContainer* mc);
	void Textures2VColor();
	bool Texture2VNormals(VecArray& wnormals, int options);
	void CalcTangents();

	void RemoveUnusedObjMtl();
	void RemoveUnusedVerts(cList<int>* encoding=NULL);
	void RemoveNonManifoldFaces();
	void RemoveZeroEdgesFaces();
	void RemoveEmptyFaces();
	void Rasterize(const std::function<void(int, int, float)>& fn);
	void Rasterize(const cMat4& transform, const std::function<void(int, int, float)>& fn);
	void Rasterize(const cMat4& transform, int Lx, int Ly, float* zbuffer);

    /**
	\brief Divide a figure on two parts with declared line.
	\return Coords for found points.
	\tparam withErase  True, when need delete a figure which divided.
	\param a, b, c, d  Vertices of figure.
	\param tAB, tCD  Neppers of new vertices on the edges `ab` and `cd`.
	\details
    When `ab`, `cd` are not of part of 1 (one!) figure, exception is throw.
    */
	template< bool withErase >
	::std::pair< cVec3, cVec3 >  Divide(
		int a, int b, float tAB,
		int c, int d, float tCD,
		int* beginRemovedRawBlock = NULL,
		int* sizeRemovedRawBlock = NULL,
		::std::pair< int, int >* changedBeginRawBlock = NULL
	);
    /**
	\param m  Point for insert between points `ab`.
	\param n  Point for insert between points `cd`.
    */
	template< bool withErase >
	void Divide(
		int a, int b, const cVec3& m,
		int c, int d, const cVec3& n,
		int* beginRemovedRawBlock = NULL,
		int* sizeRemovedRawBlock = NULL,
		::std::pair< int, int >* changedBeginRawBlock = NULL
	);

    /**
	\brief Remove a point / line / polygon (represent as raw-block) from this mesh.
	\param beginRawFigure  Start position of block in the GetRaw().
	\param withOptimize   Unused coords of the mesh removed from the GetPositions().
	\see Find()
    For example
	\code
    Erase( Find( a, b ).GetFirst() );
    \endcode
    */
	void Erase( int beginRawFigure, bool withOptimize = false );
	void Erase( const cList< int >& beginRawFigures, bool withOptimize = false );

	/// \brief Remove raw-blocks which a count of vertices is equal `n`.
	void EraseByCount( int n );

	/// \brief Remove raw-blocks which a count of vertices in the diapason [a; b].
	void EraseByCount( int a, int b );

	/// \brief Remove figures which all vertices are confluent to line.
	/// \see FindClearConfluent()
	void EraseClearConfluent( float tolerance );
	/// \brief Verify only indices of vertices.
	void EraseClearConfluentByIndexOnly();

	/// \brief Invert a figure.
	/// \see Find()
	void Invert( int beginRawFigure );

    /**
	\return Shifts in the GetRaw() of this mesh. This pointer
	        consist of declared node or edge.\n
	        If node or edge is not found, when return an empty list.
	\see GetNeighboursForFace(), Erase()
    */
	cList< int >  Find( const cVec3& nodeCoord ) const;
	cList< int >  Find( int nodeIndex ) const;
	cList< int >  Find( const cVec3& edgeCoordA, const cVec3& edgeCoordB ) const;
	cList< int >  Find( int ai, int bi ) const;
	cList< int >  Find( const edgeI_t& ) const;
	/// \warning All defined edges must be parts of figure.
	cList< int >  Find( const edgeI_t&, const edgeI_t& ) const;

    /**
	\return See `cList< int > Find()`.
	        If node or edge is not found, when return -1.
    */
	int Find( const VecArray& figure ) const;
	int Find( const cList< int >& figure ) const;

    /**
	\return List of raw-blocks which contains all fantom-figures (all vertices are confluent to line).
	\param tolerance  Value at angle for include a figure.
	\warning All lines and points are confluent figure.
	\see EraseClearConfluent()
    */
	cList< int > FindClearConfluent( float tolerance ) const;

    /**
	\brief Fixed a direction for every face by neighbours.
	\return True, when any faces was corrected.
	\param beginRawBlockEtalon  Start of block for faces which was as etalon for "direction of all other face".
	\param fixed  List of 'beginRawFigure' when already fixed.
    */
	void CorrectFaces(
		int  beginRawBlockEtalon,
        uni_hash< bool, int >*  fixed = NULL
	);

    /**
	\brief Fixed the error (from Blender by import OBJ): "the same vertex of face used multiple times".
	\see cMeshContainerTest::CorrectRawSequence*()
	\see IsValidRawSequence()
    */
	void CorrectRawSequence();

    /**
	\brief Merge faces of mesh which side by side.
	\param fusionDistance  Distance between vertex and edge when initiate a merge.
	\see cMeshContainerTest.jpg, "K" or "H"
    */
	void MergeFaces( float fusionDistance );

    /**
	\brief Insert vertext on the line 'ab' (as verticies).
	\return Added vertex.
	\param t  Distance on the line as a nepper.\n
	          [ 0.0 (a) .. 0.5 (half of length 'ab') .. 1.0 (b) ]
	\param beginRawFigure  Starts of blocks when point or figure was inserted.
	\param beginRawForRemoved  Starts of blocks which clear confluented.
	\todo fine  Change 'int' to 'typedef int vertex_t'.
    */
	int Insert(
		int a, int b, float t,
		cList< int >* beginRawFigures = NULL,
		cList< int >* beginRawForRemoved = NULL
	);
	/// \param m  Point for insert between points `ab`.
	int Insert(
		int a, int b, const cVec3& m,
		cList< int >* beginRawFigures = NULL,
		cList< int >* beginRawForRemoved = NULL
	);

	/// Insert a figure (index of verticies) into the mesh.
	void Insert( const figureI_t&, int* beginRawFigure = NULL );
	/// Insert a point.
	figureI_t Insert( int a, int* beginRawFigure = NULL );
	/// Insert a line.
	figureI_t Insert( int a, int b, int* beginRawFigure = NULL );
	/// Insert a triangle.
	figureI_t Insert( int a, int b, int c, int* beginRawFigure = NULL );
	/// Insert a polygon with 4 vertices.
	figureI_t Insert( int a, int b, int c, int d, int* beginRawFigure = NULL );

    /**
	\brief Insert a set of figures (positions of verticies) into the mesh.
	\param fusionDistance  Call MergeFaces() when this param is positive or zero.
	\see construct( figures_t )
    */
	void Insert( const figures_t&, float fusionDistance = -FLT_MAX );

    /**
	\brief Insert a figure (positions of verticies) into the mesh.
    \details
	Added coords are not containes dublicates.\n
	Use MergeFaces() for fixed faces after insert positions.
	\param figureI_t  Accumulte indices of vertices when defined.
	\param ai, bi, ...  By analogy of 'figureI_t'.
    */
	void Insert( const figure_t&, figureI_t* = NULL );
	/// Insert a point.
	figure_t Insert( const cVec3& a,
		int* ai = NULL );
	/// Insert a line.
	figure_t Insert( const cVec3& a, const cVec3& b,
		int* ai = NULL, int* bi = NULL );
	/// Insert a triangle.
	figure_t Insert( const cVec3& a, const cVec3& b, const cVec3& c,
		int* ai = NULL, int* bi = NULL, int* ci = NULL );
	/// Insert a polygon with 4 vertices.
	figure_t Insert( const cVec3& a, const cVec3& b, const cVec3& c, const cVec3& d,
		int* ai = NULL, int* bi = NULL, int* ci = NULL, int* di = NULL );

    /**
	\brief Insert a mesh to this mesh.
    \details
    Vertices are not dublicate.
	\see ConcateWith()
    */
	void Insert( const cMeshContainer& );

    /**
	\return True when 'beginRawFigure' contains a figure which all vertices are confluent to line.
	\param tolerance  Value at angle for detect figure as confluent to line.
	\warning All lines and points are confluent figure.
	\see FindClearConfluent(), EraseClearConfluent()
    */
	bool IsClearConfluent( const figure_t&, float tolerance ) const;
	bool IsClearConfluent( int beginRawFigure, float tolerance ) const;
	/// \brief Verify only indices of vertices.
    /// \see IsClearConfluent()
	bool IsClearConfluentByIndexOnly( int beginRawFigure ) const;

    /**
	\return `true` when line / face is clockwise order.\n
	        `false` when 'beginRawFigure' aim to point.
    */
	bool IsClockwiseOrder( int beginRawFigure, const cVec3& observer ) const;

    /**
	\brief Build mesh from 'src1' and 'src2' to 'this'.
	\param operation Variants:\n
	      - 0 - add
	      - 1 - subtract src1 - src2
	      - 2 - intersect src1 & src2
    */
    bool PerformBooleanOp( cMeshContainer& src1, cMeshContainer& src2,
        int operation,
        cList< ::std::pair< comms::cVec3, comms::cVec3 > >* dividers = NULL );

	/*
	//--------------------------------------------------------------------------
	// bool InitPrimObj(ObjectParam) CreatePrimObj(ObjectParam), ChangePrimObj(ObjectParam)
	//---------------------------------------------------------------------------
	bool InitPrimObj(GM::ObjectParam* param);
	bool CreatePrimObj(GM::ObjectParam* param, GM::GM_Rect *part=NULL);
	int ChangePrimObj(GM::ObjectParam* param, GM::GM_Rect *part=NULL);
	
	GM::GM_PrimitiveObject* GetPrimObj() const;
	*/
	
	void CreateCube(const cVec3& Sides);
	void CreateQuadCylinder(const cMat4& M,float quant,float rtop=1.0,float rbottom=1.0,float Height=2.0);
	void CreateQuadCylinder(float quant,cVec3 PTop,cVec3 PBottom,float rTop,float rBottom);
	void MakeShell(float Out, float In, float OutEdge, float InEdge, int ndiv = 1, cList<cVec2>* EdgeShape = NULL);
	void MakeShellDir(float Out,float In,cVec3 Dir);
	void LeaveBiggestPiece();

	/// Try to divide all quads in approx equal sub-quads.
	void QuadQuantSubd(const cMat4& M,float quant,float dotp, ::std::function<int(int, int)>* divider = nullptr);
	/// Divide triangular mesh on N^2 triangles.
	void TriSubd(int N, SubdSnapEdgeCallback* dive = NULL, SubdSnapMiddlePointCallback* divm = NULL, void* context = NULL);

	void Transform(const cMat4& M);
	void SetDefaultObjMtl();
	void Symmetry(cVec3 pos, cVec3 n, bool Quads, bool RemoveOpp, float ToleranceCoef = 0.0);
	void WeldedSymmetry(cVec3 pos, cVec3 n, bool RemoveOpp, float ToleranceCoef = 0.0, float WeldCoef = 0.0);
	void Smooth(float degree,bool tangent,int count=1,float sharpdot=0.3,UnlimitedBitset* pins=NULL,bool smoothedges=false);
	void RelaxNormals(VecArray& ResultNormals, int Count);
	void SmoothFast(float degree,int count,UnlimitedBitset* pins,bool tangent);
	void SmoothPins(float degree, int count, UnlimitedBitset* pins, bool tangent);
	void SmoothFastAlongNormals(int count, UnlimitedBitset* pins);
	void PinEdgesAndSharp(UnlimitedBitset& bs);
	void SmoothSubsetFast(float degree, int count_regular, int count_tangent, int series_repeats, UnlimitedBitset& pins);
	void GetSharpEdges(cList<DWORDS2>& List,float dotp);
	void GetOpenEdges(cList<DWORDS2>& List);
	void GetOpenEdges(cList<bi_int>& List);
	void GetOpenEdges(cList<bi_int>& List,int sub_object);
	// Snaps the current mesh toi the other mesh, snap distance is equal to the average neighbour edge length multiplied with the dstMod
	void SnapToOtherMc(comms::cMeshContainer* other, float dstMod = 1.0, bool CheckNormalsDirections = true);
	// Snaps the points set to the current mesh. The current mesh should be truangular.
	// The points array contains point position (that will be changed if snapping occured), point normal and snap max distance
	void SnapPointsSet(cList<::std::tuple<cVec3, cVec3, float>>& points, bool CheckNormalsDirections = true);
	void RemoveDisconnectedPieces(float percent_of_polycount, float percent_of_square);

    /**
	\brief Creation for rendering.
	\details
    Create mesh with smoothed normals. Shader `mpreview1` used.
    */
	StaticMesh* CreateStaticMesh();
	/// Create mesh with smoothed normals and mcubes - like shader.
	StaticMesh* CreateStaticMeshMC();
	StaticMesh* CreateStaticMeshMC_Shell(float In, float Out);
	/// Create faceted mesh. Shader `mpreview1` used.
	StaticMesh* CreateHardsurfaceStaticMesh();
	/// Create mstatic mesh with UV, skypreview shader used.
	StaticMesh* CreateStaticMeshUV();


	/// \see GetNormal()
	cVec3 GetFaceCoord(int pos) const;
	cVec3 GetFaceNormal(int pos) const;
	
	void SplitHardsurface(float angcos);
	void SplitPositionsByNormals();
	void SplitDisconnected(cList<cMeshContainer*>& res);
	void CreateFone() const;
	void ClearFone() const;
	void CreateVnv(uni_hash<int, int>& vnv);
	void FindCorners(cList<int>& corners);

	/// \warning If mesh is not triangulated when DrawDbg() is incorrect.
	void DrawDbg(const cMat4& T,DWORD Color,DWORD FillColor=0,DWORD DetailColor=0);
	/// use this function within __thumbnail(...) section to visualuse mesh with UV, seek "__thumbnail(test1)" as example.
	void DbgDrawMeshWithUVGrid();
	DWORD CalcGUID();

	float CalcVolume(const cVec3& start = cVec3::Zero, const cVec3& dir = cVec3::AxisZ ) const ;
	float CalcSquare() const ;
	cBounds CalcBoundBox() const ;
	float CalcSquareMC() const ;
	void CloseHoles(int MaxHoleSize,bool SkipMaximalHole);
	void RemoveRedundantUV();
	void Weld(float distance, UnlimitedBitset* Selected = NULL, cList<int>* rawencode = nullptr);
	void WeldEdges(float distance);
	void WeldNormals();
	void AutoWeldOpenEdges();
	void CloseHolesBetweenDistinctIslands(cList<cStr>* involved_objects);
	void TriangulateAndDecimate(int destinationTris);
	void GradualSubdivision(const std::function<bool(int, int)>& test_split_edge, const std::function<comms::cVec3(int, int)>& get_middle_position);
	

	// keeps sharp details but makes triangles to be regularly shaped
	void OptimizeMesh(int MinDestinationTriangles);
	void AutodetectSymmetry(cList<cPlane>& planes);

	void CutMesh(MeshCutter& mcut);
	void CutMesh(const std::function<float(cVec3, float&)>& sign, cList<bi_int>& new_edges, cList<bi_int>* sources = nullptr, float div_mod = 1.0f);
	void FindCutEdges(const std::function<float(cVec3)>& getter, cList<std::pair<cVec3, cVec3>>& edges);
	void CutByPlane(const cPlane& pl,VecArray* cutverts=NULL);
	void SemiBoolean(const std::function<float(cVec3, float&)>& this_sign, cMeshContainer& with, const std::function<float(cVec3, float&)>& with_sign);
	void PlainSubdiv();
	void PatchedDivision(int dstTris, bool uniform, bool Flat = false, int GpuNormalsRelax = 0);
	float PickObject(cSeg& Ray, const cMat4& M);
	///returns (xyz = nearest_edge_point_pos,w = distance_from_the_ray)
	void PickObjectEdges(cSeg& Ray, const cMat4& M, cVec3& RayPt, cVec3& ObjPt);
	cVec3 CalcMeshProjSquare();
	cVec3 CalcMeshProjSquare(const cMat4& M);
	void ConvertPicToPolygones(cImage& Img, float Thickness, bool Normalize);
	void ConvertPicToGridPolygones(cImage& Img, float Thickness, int nDiv, bool Normalize, bool DefAlign);
	void ConvertPicToGridPolygonesWithTapering(cImage& ImgT, cImage& ImgB, float Thickness, int nDiv, bool Normalize, float Angle, float Weight);

	/// Divide the mesh on two meshes (harvest vertices).
	void DivideMesh(MeshDivider<int>&) const;

	///flip edge, requires fone, requires 2 faces over the edge.
	bool FlipEdge(int V1, int V2, int* valence = NULL);
	///tries to set valence 6 whenever possible unsing edge-flip
	bool OptimizeTriangularMesh();
	void PutOnGround(cMat4& M,cVec3& p,cVec3& c);
	void PutOnGround(cMat4& M);
	void LayOnGround(cMat4& M);
	static void SetLoadMeshHook(MeshOpHook* h);
	static void SetSaveMeshHook(MeshOpHook* h);
	void SeparateObject(int ObjectIndex, cMeshContainer* dest);
	void ImproveQuadsTopology();

	bool IntersectionTwoFace(int f1, cMeshContainer* m2, int f2, cVec3* p1, cVec3* p2);
	bool GetFaceVertex(int nf, cList<int>& Vertx);
	bool GetNormalsOfFace(int nf, cList<int>& Norm);

	int AddNewVert(cVec3& pnt, double delta);
	int	InsertVertexToEdge(int nf, cVec3& p, double delta);
	void InsertVertex(int f, int v, DWORDS2 ed);
	/**
	\brief Max indx can Get by GetPolyCount()
	see GetFaceNormal(int pos)
	*/
	cVec3 GetFaceNormalByNubmer(int indx);
	cVec3 GetFaceCenterByNubmer(int indx);
	int GetFacePos(int indx);
	int FindSecondFace(DWORD F, DWORDS2& edge);
	bool EditFace(int f, cList<DWORD>& Vert);
	int AddFace(cList<DWORD>& Vert);
	void print();
	int GetNearVertex(cVec3& pm);
	int FindFirstFace3d(DWORDS2& ed);
	int FindSecondCFace3d(int nf, DWORDS2& ed);
	bool IntersectionFacebyPlane(int nf, cPlane& pl, cList<cVec3>& rez);
	bool RemoveFace(int indx);
	void SelectConnectedFaces(int f, cList<int>& facesFrom, cList<int>& facesTo);
	void DrawWire(DWORD Color, float Thickness, bool DrawNormal=false);
	float GetLengthEdgeMin();
	float GetLengthEdgeMidle();
	void IntersectionRay(cVec3& p, cVec3& dir, cList<cVec3>& rez);
	bool IntersectionFaceRay(int nf, cVec3& p, cVec3& dir, cVec3& pc);
	void OptimizeVoxelMesh(BaseClass* vo);
	void FindTopologyCorrespondence(cMeshContainer& other, cList<int>& encoding);
	void SnapingToSculpt();
	cMeshContainer* QuadrangulateByEdges(cList<DWORDS2>& edges, float ApproxEdgeLength);
	bool TrimmingByPolyline(cList<cVec3>& poly);
	bool IntersectedFaceByPolyline(int f, cList<cVec3>& poly);

    /// \brief See `GenPlane`.
	struct PlaneArgs {
		float Width;
		float Height;
		int wSubDivs;
		int hSubDivs;
		cVec3 Axis;
		bool TwoSided;
		
		PlaneArgs() { SetDefaults(); }
		void SetDefaults();
		void Validate();
	};    

	static cMeshContainer * GenPlane(PlaneArgs = PlaneArgs());

	static cMeshContainer * GenPlaneHexagonal(const float Lx, const float Ly, const float Cell, const bool Noisy);		

	cList<OneMorph*> Morphs;


private:
	/// \see cMeshContainer()
	void init();


protected:
	cStr m_Name;
	VecArray m_Positions;
	UvArray m_TexCoords;
	VecArray m_Normals;
	cList<cUVSet> m_UVSets;	
    /// Tangent and BiTangent.
	cList<cVec<cVec3, 2> > m_Tangents;
    /**
    \details
	Polygon info or 3 references to the arrays of positions, texture coordinates,
	and normals with tangents (optional) respectively.
    \code
    Info0 = { Count0, idMtl, idObj } idMtl = 
    Indices_0 = { iPosition, iTexCoord, iNormal (iTangent) }
    ...
    Indices_(Count0 - 1)
    Info1
    ...
    \endcode
    */
	RawArray m_Raw;
	cList<cSurface> m_Materials;
    cList<cObject> m_Objects;
    cList<cVMapElm> m_VMaps;
    cList<cVMapType> m_VmTypes;
	cList<PtexQuad> m_Ptex;
	cList<DWORD> m_VertexColor;
    /// RGB - specular color, A - glossness.
	cList<DWORD> m_VertexSpecular;
	cList<DWORD> m_VertexEmissive;
	cList<DWORD> m_GlossMetallEmissive;
	cList<float> m_NormalDisplacement;


public:
	mutable uni_hash<int,DWORDS2> fone;
	static MeshOpHook* ImportHook;
	static MeshOpHook* ExportHook;
	cStr FirstLineComment;



private:
	mutable int fone_hash;
	/// Hashtable for ToDrawMesh().
	struct HashEntry {
		int Index;
		HashEntry *pNext;
	};
	mutable cList<HashEntry *> m_HashTable;
	template<typename Vertex> int AddVertexToHashTable(cList<Vertex> &Vertices, const Vertex *pVertex, const int HashValue) const;
	void FreeHashTable() const;


private:
	//GM::GM_Composer *composer;
};




//--------------------------------------------------------------------------------------------------------------------
// cMeshContainer::AddVertexToHashTable
//--------------------------------------------------------------------------------------------------------------------
template<typename Vertex>
inline int cMeshContainer::AddVertexToHashTable(cList<Vertex> &Vertices, const Vertex *pVertex, const int HashValue) const {
	bool IsFound = false;
	int Index = 0;

	if(m_HashTable.Count() > HashValue) {
		HashEntry *pEntry = m_HashTable[HashValue];
		while(pEntry != NULL) {
			const Vertex *pCacheVertex = &Vertices[pEntry->Index];
			// If this vertex equals the vertex already in the vertex buffer,
			// simply point the index buffer to the existing vertex.
			if(memcmp(pVertex, pCacheVertex, sizeof(Vertex)) == 0) {
				IsFound = true;
				Index = pEntry->Index;
				break;
			}
			pEntry = pEntry->pNext;
		}
	}

	// Vertex was not found. Create a new entry, both within the Vertices list
	// and also within hashtable cache.
	if(!IsFound) {
		// Add to the Vertices list.
		Index = Vertices.Add(*pVertex);

		// Add to the hashtable:
		HashEntry *pNewEntry = new HashEntry;
		pNewEntry->Index = Index;
		pNewEntry->pNext = NULL;

		// Grow the hashtable cash if needed:
		if(m_HashTable.Count() <= HashValue) {
			m_HashTable.Add(NULL, HashValue - m_HashTable.Count() + 1);
		}

		// Add to the end of the linked list:
		HashEntry *pCurEntry = m_HashTable[HashValue];
		if(NULL == pCurEntry) {
			// This is the head element:
			m_HashTable[HashValue] = pNewEntry;
		} else {
			// Find the tail:
			while(pCurEntry->pNext != NULL) {
				pCurEntry = pCurEntry->pNext;
			}
			pCurEntry->pNext = pNewEntry;
		}
	}
	return Index;
} // cMeshContainer::AddVertexToHash




/**
\brief Output a structore of cMeshContainer to stream.
\details Use it for debug.
*/
::std::ostream& operator<<( ::std::ostream&,  const cMeshContainer& );
