class OneCurveObject;
struct ContourPoint{
	Vector3D Pos;
	Vector3D Normal;
	float Angle;
	bool Sharp;
	int HoleID;
};
struct ContourQueuePoint{
	ContourQueuePoint(){
		hole = 0;
		Pin = 0;
		Angle = 0;
		NextOpposite = -1;
	}
	int VertexIndex;
	int PrevIndex;
	int NextIndex;
	int NextOpposite;
	float Angle;
	int Pin;
	int hole;
};
/** \brief The class ContourToFill intended to fill contours by triangles or quads

Pass set of points with normals and get triangular/quad fillled surface
Derive from this class to redefine PlacePoint method for custom points snapping/placing
*/
struct FillContour{
	cList<Vector3D> Vrtx, Nrm;
	cList<int> Indx;

	cList<ContourQueuePoint> Contour;
	cList<int> LivePoints;

	Vector3D MainDir;
	float Base;
	float Max;
	int SnapSomewhere(int LiveIndex, float MaxDistance, Vector3D Origin);
	///draw debug info
	void DebugDrawIdxs();
	///integrity check
	bool check();
};

class ContourOnSurf {
public:
	cList<Vector3D> Points;
	cList<Vector3D> Normals;
};

class ContourToFill{
	cList<ContourPoint> Points;
	cList<ContourPoint> WholeSet;
	UnlimitedBitset Pin;
	int nsub;
	int HoleID;
	int MaxHoleID;
	virtual void Prepare();
	static float Angle(const Vector3D& s, const Vector3D& v1, const Vector3D& v2, const Vector3D& N);
	HyperBezier HB;
	comms::cPlane pl;

	static void snap_edge_pt(void* context, cVec3& pt, cVec3& n, int vc, int v1, int v2, int weight);
	static void snap_fac_pt(void* context, cVec3& pt, cVec3& n, int vc, int v1, int v2, int v3, float w1, float w2);

	void RelaxNormals(int times);
public:

	bool NeedToSnap;
	bool UseHB;
	bool UsePlane;
	bool LockDirection;
	int  MaxAllowedTime;
	bool WasFlipped;

	ContourToFill(){
		ForceSnaping = false;
		InvertOrder = false;
		HoleID = 0;
		MaxHoleID = 0;
		MaxAllowedTime = 20000;
		LockContourOrder = false;
		LockDirection = false;
		WasFlipped = false;
	}
	bool ForceSnaping;
	bool InvertOrder;
	bool LockContourOrder;
	virtual bool PlacePoint(Vector3D& pt, Vector3D& n);
	///not implemented yet
	void FillByQuads(comms::cMeshContainer& mesh);
	///fill contour by triangles, place result into the mesh
	void FillByTriangles(comms::cMeshContainer& mesh,int nSubd);
	///this is only suitable for filling N-gonal contours with stright edges. It adds stripes of triangles instead of adding triangles one by one.
	void FillByTrianglesChunked(comms::cMeshContainer& mesh, int nSubd, int stages);
	void Clear();
	///first, add points of the main contour
	void AddPoint(const Vector3D& Pos, const Vector3D& Normal);
	///then call this to start hole
	void StartHole();
	///then use AddPoint to add hole points, at the end use EndHole. Pay attention, nomals should be approx. same direction in contour and in hole, order of vertices should be opposite in contour and in hole.
	void FinishHole();
	///invert vertices order for the current hole, use before FinishHole called.
	void RevertHole();
	///invert normals for the current hole, use before FinishHole called.
	void InvertHoleNormal();
	void DbgNormals();
	///smooth infinitely, keep borders
	void InfiniteSmooth(comms::cMeshContainer& mesh);
	void AddContous(cList<ContourOnSurf>& Contours, OneCurveObject* cu);
};

struct qContourPoint {
	qContourPoint() {
		PrevPoint = NextPoint = -1;
		pos = comms::dVec3::Zero;
		DerivedEdgeLength = 0;
		avdist = 0;
		ParentVertex = -1;
		IsInitial = false;
		IsNew = true;
		Angle = 0;
	}
	comms::dVec3	pos;
	comms::dVec3	tangent1;
	comms::dVec3	tangent2;
	comms::dVec3	front1;
	comms::dVec3	front2;
	comms::dVec3	Normal;
	double	DerivedEdgeLength;
	double  Angle;
	int		avdist;
	int		ParentVertex;
	int		EdgeType;//0-straight, 1 - inner corner, -1 - outer coner
	int		PrevPoint;
	int		NextPoint;
	bool	IsInitial;
	bool    IsNew;
};
class qContourIntersector {
	comms::dVec3 v1;
	comms::dVec3 v2;
	comms::dVec3 c;
	double len12;
	comms::dSeg L1;
public:
	qContourIntersector(const comms::dVec3& pt1, const comms::dVec3& pt2);
	void Prepare(const comms::dVec3& pt1, const comms::dVec3& pt2);
	bool IntersectsWith(const comms::dVec3& pt1, const comms::dVec3& pt2, double distpercent);
};
class ContourQuadrangulator {
	fAABBNodePool fpool;
	fAABBNode mcPicker;
	comms::cMeshContainer* mc;
public:

	cList<qContourPoint> Contour;
	cList<int> Faces;
	uni_hash<int, int> vnv;
	///create contour from open edges of the triangular mesh, mesh pinter used during calculations, so keep it till the class used
	void CreateFromMesh(comms::cMeshContainer* mesh);
	void CreateBgFromMesh(comms::cMeshContainer* mesh);
	void CreateFromList(cList<int>& edges);
	void Clear();
	///returns the cost of expansion, designed for the multicore usage
	double TryToExpandChunk(int start_point, cList<qContourPoint>& new_contour, cList<int>& newfaces);
	void UpdateDirections(cList<qContourPoint>& contour, bool derived = false, double creaseangle = 50.0);
	void DbgDrawContour(cList<qContourPoint>& contour, const char* layer, bool directions, bool points, bool ids);
	void Quadrangulate(comms::cMeshContainer* res);
	void AddVnv(cList<int>& newfaces);
	void CreateDistField();
	void RelaxVnv(int ntimes, double degree = 1.0, bool DistanceDependent = false);
	void DbgViewVnv(const char* name = "final", bool indices = false);
	int  ContourSize(int start);
	bool CheckFlips(cList<qContourPoint>& contour, cList<int>& newfaces);
	bool CheckSelfIntersections(cList<qContourPoint>& contour);
	void MarkAsOld();
	void SnapToMesh(comms::dVec3& pos, comms::dVec3& normal);
	void selfcheck(cList<qContourPoint>& contour);
};