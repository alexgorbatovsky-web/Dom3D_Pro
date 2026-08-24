#include "stdafx.h"
#include "CurveGuides/Curves.h"
#include "CurveGuides/CurvesMathOperations.h"
#include "FillContour.h"
#include "BigMatrix.h"

//#define coqu_debug

///returns angle between vectors, 0..2*pi
float ContourToFill::Angle(const Vector3D& s, const Vector3D& v1, const Vector3D& v2, const Vector3D& N){
	Vector3D a = v1 - s;
	Vector3D b = v2 - s;
	Vector3D r = (Vector3D::Cross(b, a));
	float sin_alpha = r.Length();
	float val = atan2(sin_alpha, a.dot(b));
	if (r.dot(N)<0)  return 2 * M_PI - val;
	return val;
}
void ContourToFill::FillByQuads(comms::cMeshContainer& mesh){

}
bool ContourToFill::PlacePoint(Vector3D& pt, Vector3D& n){
	if (NeedToSnap){
		DWORD B;
		pt = PMS().PutPointOnSurface(pt, n, B);
		return true;
	}else
	if (UsePlane){
		pt = pl.ProjectPoint(pt);
		n = pl.GetNormal();
		return true;
	}else
	if (UseHB){
		pt = HB.GetFastSmoothedPosition(pt, &n);
		return true;
	}
	return false;
}
std::mutex snap_mut_In;
std::mutex snap_mut_Out;
struct PtToSnap{
	int PointIndex;
	Vector3D Pos;
	Vector3D Nrm;
};
///for future: multithreaded update of the mesh, not works now
//#define MULTISNAP
class SnapQueue{
public:
	ContourToFill* Contour;
	FillContour* cf;
	bool StopIt;
	int maxqueue;
	cList<PtToSnap> sIn;
	cList<PtToSnap> sOut;
	cList<cThread::ThreadHandle> Handles;

	SnapQueue(ContourToFill* co, FillContour* cfill){
		StopIt = false;
		Contour = co;
		cf = cfill;
		maxqueue = 0;
	}
	static void process(void* p){
		SnapQueue* sq = (SnapQueue*)p;
		bool morejob;
		do{
			morejob = false;
			snap_mut_In.lock();
			int n = sq->sIn.Count();
			PtToSnap p;
			p.PointIndex = -1;
			if (n){
				p = sq->sIn.GetLast();
				sq->sIn.RemoveLast();
				morejob = sq->sIn.Count() > 0;
				snap_mut_In.unlock();
			}
			else snap_mut_In.unlock();
			if (p.PointIndex != -1){
				sq->Contour->PlacePoint(p.Pos, p.Nrm);
				snap_mut_Out.lock();
				sq->sOut.Add(p);
				snap_mut_Out.unlock();
			}
		} while (morejob || !sq->StopIt);
	}
	void AddPoint(int idx, Vector3D pt, Vector3D nrm){
#ifdef MULTISNAP
		PtToSnap p;
		p.Pos = pt;
		p.PointIndex = idx;
		p.Nrm = nrm;
		snap_mut_In.lock();
		sIn.Add(p);
		if (sIn.Count() > maxqueue)maxqueue = sIn.Count();
		snap_mut_In.unlock();
#endif
	}
	bool GetPoint(PtToSnap& pt){
#ifdef MULTISNAP
		pt.PointIndex = -1;
		snap_mut_Out.lock();
		if (sOut.Count()){
			pt = sOut.GetLast();
			sOut.RemoveLast();
		}
		snap_mut_Out.unlock();
		return pt.PointIndex != -1;
#else
		return false;
#endif
	}
	void run(){
#ifdef MULTISNAP
		int np = std::thread::hardware_concurrency() - 1;
		if (np < 1)np = 1;
		for (int i = 0; i < np; i++){
			Handles.Add(comms::cThread::CreateThread(&process,this));
		}
#endif
	}
	void handle(){
		PtToSnap sp;
		while (GetPoint(sp)){
			cf->Vrtx[sp.PointIndex] = sp.Pos;
			cf->Nrm[sp.PointIndex] = sp.Nrm;
			AddDbgLine(sp.Pos, sp.Pos + sp.Nrm * 4, 0xFFFFFFFF, 0xFF000000);
		}
	}
	void stop(){
		StopIt = true;
		for (int i = 0; i < Handles.Count(); i++){
			cThread::WaitAndDeleteThread(&Handles[i]);
		}
		handle();
	}
	void SimpleSnap(Vector3D& p,Vector3D& n){
#ifndef MULTISNAP
		Contour->PlacePoint(p, n);
#endif
	}
};
///snap edge center, used with subdivision
void ContourToFill::snap_edge_pt(void* context, cVec3& pt, cVec3& n, int vc, int v1, int v2, int weight){
	ContourToFill* C = (ContourToFill*)context;
	int np = C->Points.Count();
	if ((v2==v1+1 || (v1==0 && v2==np-1)) && v2 < np && v1 < np){
		Vector3D p0 = pt;
		if (v1 == 0 && v2 == np - 1)pt = C->WholeSet[v2*(C->nsub + 1) + C->nsub - weight].Pos;
		else pt = C->WholeSet[v1*(C->nsub + 1) + weight + 1].Pos;
		C->Pin.set(vc, true);
		//AddDbgLine(p0, pt, 0xFFFF0000, 0xFF00FF00);
	}
	else{
		//snap
		C->PlacePoint(pt, n);
	}
}
///snap face center, used with subdivision
void ContourToFill::snap_fac_pt(void* context, cVec3& pt, cVec3& n, int vc, int v1, int v2, int v3, float w1, float w2){
	ContourToFill* C = (ContourToFill*)context;
	C->PlacePoint(pt, n);
}
extern bool DebugFillCurve;
///selfcheck routine
bool FillContour::check(){
	return true;
	for (int i = 0; i < LivePoints.Count(); i++){
		int vc = LivePoints[i];
		ContourQueuePoint& cp = Contour[vc];
		if (Contour[cp.PrevIndex].NextIndex != vc || Contour[cp.NextIndex].PrevIndex != vc){
			return false;
		}
	}
	DebugDrawIdxs();
	return true;
}
///Draw debug info
void FillContour::DebugDrawIdxs(){
#ifdef coqu_debug
	if (DebugFillCurve){
		ClearDbg();
		int A = LivePoints.GetAmount();
		DbgLayer("VertexIndex");
		for (int j = 0; j < A; j++){
			ContourQueuePoint& cpt = Contour[LivePoints[j]];
			AddDbgNumber(Vrtx[cpt.VertexIndex], LivePoints[j], 0xFFFFFF00);
		}
		DbgLayer("VertexNormal");
		for (int j = 0; j < A; j++){
			ContourQueuePoint& cpt = Contour[LivePoints[j]];
			AddDbgLine(Vrtx[cpt.VertexIndex], Vrtx[cpt.VertexIndex] + Nrm[cpt.VertexIndex] * 40.0, 0xFFFF0000, 0xFF00FF00);
		}
	}
#endif //coqu_debug
}
inline bool isdiff(int a, int b, int c){
	return a != b && a != c && b != c;

}
///fill contour by triangles, place result into the mesh, nSubd is additional subdivision, use nSubd = 0, subdivision not tested well
void ContourToFill::FillByTriangles(comms::cMeshContainer& mesh, int nSubd) {
	int time0 = GetTickCount();
	nsub = nSubd;
	Prepare();
	auto& fraw = mesh.GetRaw();
	auto& fpos = mesh.GetPositions();
	int typesmooth = 1;
	float angle_thresh[2] = { M_PI * 0.5, 0.9 * M_PI };//{M_PI*0.5, 0.9*M_PI} {M_PI*5.f/12.f, 3*M_PI/4.f}; //{M_PI*0.5, M_PI}; 
	int prevkey = -1;
	if (!Points.GetAmount()) return;
	int counter = 0;
	int step = 0;
	float Length = 0.f;

	FillContour FCon;
	SnapQueue SQ(this, &FCon);

	FCon.Vrtx.SetCapacity(Points.Count());
	FCon.Nrm.SetCapacity(Points.Count());
	int nc = Points.Count();
	AABoundBox AB;
	AB.SetEmpty();
	int np = Points.Count();
	int start = 0;
	int idx = 0;
	for (int h = 0; h <= MaxHoleID; h++) {
		int num = 0;
		for (int i = 0; i < np; i++) {
			if (Points[i].HoleID == h) {
				FCon.Vrtx.Add(Points[i].Pos);
				FCon.Nrm.Add(Points[i].Normal);
				ContourQueuePoint cp;
				cp.Angle = 0;
				cp.hole = h;
				cp.VertexIndex = idx;
				FCon.Contour.Add(cp);
				FCon.LivePoints.Add(idx);
				AB.AddPoint(Points[i].Pos);
				num++;
				idx++;
			}
		}
		for (int p = 0; p < num; p++) {
			ContourQueuePoint& cp = FCon.Contour[start + p];
			cp.NextIndex = start + (p + 1) % num;
			cp.PrevIndex = start + (p - 1 + num) % num;
		}
		for (int i = 0; i < num; i++) {
			int si = start + i;
			ContourQueuePoint& cp = FCon.Contour[si];
			int vp = FCon.Contour[cp.PrevIndex].VertexIndex;
			int vn = FCon.Contour[cp.NextIndex].VertexIndex;
			Vector3D d = (FCon.Vrtx[vp] - FCon.Vrtx[vn]).ToNormal();
			FCon.Nrm[si] -= d * FCon.Nrm[si].dot(d);
			FCon.Nrm[si].Normalize();
		}
		start += num;
	}
	AB.Inflate(AB.GetDiagonal() / 10.0);
	int nmax = Points.Count() * Points.Count();
	if (DebugFillCurve) {
		//set DebugFillCurve = true, then use UP and DOWN keys to show mor or less triangles 
		AllowDebug(true);
		ClearDbg();
		static float nmax1 = 20;
		static float spd = 0.3;
		if (_KEY(VK_UP)) {
			nmax1 += spd;
			if (spd < 1)spd *= 1.1;
			else spd *= 1.05;
		}
		else
			if (_KEY(VK_DOWN)) {
				nmax1 -= spd;
				if (spd < 1)spd *= 1.1;
				else spd *= 1.05;
			}
			else {
				spd = 0.3;
			}
		if (nmax1 < nmax)nmax = nmax1;
	}
	float Len0 = -1;
	static int dd = 0;
	dd++;
	bool simp = _CTRL();
	static float dircoef = 1.0 / 10.0;
	Vector3D pt0(0);
	float maxdst = 0;
	bool firstdone = false;
	int nt = 1;
	static float angs[] = { c_PI / 3.0f * 1.1f,c_PI / 2,c_PI / 2.0f * 1.5f,c_PI };
	SQ.run();
	cList<cList<int> > bkContours;
	float SnapL = 0;
	bool FinishItFast = false;
	while (FCon.LivePoints.Count() > 2 && counter < nmax) {
		//ClearDbg();
		SQ.handle();
		counter++;
		int delpos = -1;
		float min = FLT_MAX;
		float anl = 0;
		int i1 = 0;
		int i2 = 0;
		int i3 = 0;
		Vector3D V1, V2, V3;
		int A = FCon.LivePoints.GetAmount();
		//------------------------------------------
		int na = 0;
		bool allbound = false;
		for (int j = 0; j < A; j++) {
			ContourQueuePoint& cpt = FCon.Contour[FCon.LivePoints[j]];
			if (!cpt.hole) {
				int vpr = FCon.Contour[cpt.PrevIndex].VertexIndex;
				int vnx = FCon.Contour[cpt.NextIndex].VertexIndex;
				Vector3D N0 = (FCon.Nrm[vpr] + FCon.Nrm[vnx] + FCon.Nrm[cpt.VertexIndex]).ToNormal();
				float& angle = cpt.Angle;
				if (simp)angle = 0;
				Vector3D cp = FCon.Vrtx[cpt.VertexIndex];
				if (__abs(angle) < 0.0001)angle = Angle(cp, FCon.Vrtx[vpr], FCon.Vrtx[vnx], N0);
				float a1 = angle;
				if (a1 > c_PI * 1.999)a1 -= 2 * c_PI;

				//a1 = angle;
				if (min >= a1) { //angle
					min = a1;
					anl = angle;
					i1 = vpr;
					i2 = cpt.VertexIndex;
					i3 = vnx;
					delpos = j;
					allbound = FCon.Contour[cpt.PrevIndex].PrevIndex == cpt.NextIndex;
					if (FCon.Contour[FCon.Contour[cpt.PrevIndex].PrevIndex].PrevIndex == cpt.NextIndex) {
						allbound = true;
					}
				}
				//Find avg of edge length
				if (step == 0) {
					Length += FCon.Vrtx[vpr].Distance(FCon.Vrtx[cpt.VertexIndex]);
					na++;
				}
			}
		}//for j
		if (step == 0 && na) {
			Length /= na;
			SnapL = Length * 0.95;
			pt0 = FCon.Vrtx[i2];
			for (int k = 0; k < A; k++) {
				float d = pt0.distance(FCon.Vrtx[FCon.Contour[FCon.LivePoints[k]].VertexIndex]);
				if (d > maxdst) {
					maxdst = d;
				}
			}
			nt = 4;
			step++;
		}
		if (!FinishItFast) {
			if (GetTickCount() - time0 > MaxAllowedTime)FinishItFast = true;
		}
		if (delpos >= 0) {
			//check here the magnitude of the minimun angle, and create or triangulate the new vertices in the contour
			//and update the contour accordingly
			V1 = FCon.Vrtx[i1];
			V2 = FCon.Vrtx[i2];
			V3 = FCon.Vrtx[i3];

			if (Len0 < 0)Len0 = Length;
			//-------------------------
			min = anl;
			if (min <= angle_thresh[0] || min >= c_PI || A == 3 || allbound || FinishItFast) {
				//just connecting points, sharp angle case
				if (i1 != i2 && i2 != i3 && i3 != i1) {
					FCon.Indx.Add(i1);
					FCon.Indx.Add(i2);
					FCon.Indx.Add(i3);
				}
				ContourQueuePoint& cpt = FCon.Contour[FCon.LivePoints[delpos]];
				FCon.LivePoints.RemoveAt(delpos);
				FCon.Contour[cpt.PrevIndex].NextIndex = cpt.NextIndex;
				FCon.Contour[cpt.NextIndex].PrevIndex = cpt.PrevIndex;

				FCon.Contour[cpt.PrevIndex].Angle = 0;
				FCon.Contour[cpt.NextIndex].Angle = 0;

				FCon.Contour[cpt.PrevIndex].hole = 0;
				FCon.Contour[cpt.NextIndex].hole = 0;
				A--;
				if (DebugFillCurve)FCon.check();
			}
			else {// add 2 triangles
				ContourQueuePoint& cpt = FCon.Contour[FCon.LivePoints[delpos]];

				Vector3D N = (FCon.Nrm[i1] + FCon.Nrm[i2] + FCon.Nrm[i3]).ToNormal(); // .ToNormal(); // 3.f; 
				Vector3D N0 = FCon.Nrm[i2];
				Vector3D pos = V1 + V3 - 2.0f * V2;
				if (min > c_PI)pos = V2 * 2.0 - pos;
				//pos -= N0*pos.dot(N0);
				float l = pos.Length();
				float l20 = std::max(V1.distance(V2), V3.distance(V2));
				float l2 = l20;// std::min(V1.distance(V2), V3.distance(V2))*0.5 + l20*0.5;
				pos = Vector3D::Normalize((V1 - V2).ToNormal() + (V3 - V2).ToNormal());
				//pos.Normalize();
				float fac = Len0;// std::min(l2, Len0);// *(__rand() + 260000) / (260000 + 16384.0);
				pos = V2 + fac * pos;
				int g = -1;
				SQ.SimpleSnap(pos, N);
				g = FCon.Vrtx.Add(pos);
				SQ.AddPoint(g, pos, N);

				cpt.Angle = 0;
				cpt.hole = 0;
				cpt.VertexIndex = g;
				FCon.Contour[cpt.PrevIndex].Angle = 0;
				FCon.Contour[cpt.NextIndex].Angle = 0;
				FCon.Contour[cpt.PrevIndex].hole = 0;
				FCon.Contour[cpt.NextIndex].hole = 0;
				if (DebugFillCurve) {
					if (counter == nmax) {
						DbgLayer("LivePoints");
						for (int j = 0;j< FCon.LivePoints.GetAmount();j++) {
							int p = FCon.LivePoints[j];
							ContourQueuePoint& cp = FCon.Contour[p];
							Vector3D v0 = FCon.Vrtx[cp.VertexIndex];
							Vector3D v1 = FCon.Vrtx[FCon.Contour[cp.NextIndex].VertexIndex];
							AddDbgLine(v0, v1, 0xFF000000, 0xFFFFFFFF);
						}
						DbgLayer("");
						
						AddDbgPoint(V2, 0xFFFF0000);
						AddDbgLine(V2, V1, 0xFFFF0000, 0xFF00FF00);
						AddDbgPoint(V1, 0xFF00FF00);
						AddDbgLine(V2, V3, 0xFFFF0000, 0xFF0000FF);
						AddDbgPoint(V3, 0xFF0000FF);
						AddDbgLine(V2, pos, 0xFFFF0000, 0xFFFFFFFF);
						AddDbgPoint(pos, 0xFFFFFFFF);
					}
				}
				int vd = FCon.SnapSomewhere(delpos, SnapL, V2);
				if (vd != -1) {
					g = FCon.Contour[vd].VertexIndex;
				}
				Vector3D nn1 = Vector3D::Cross(FCon.Vrtx[i1] - FCon.Vrtx[g], FCon.Vrtx[i2] - FCon.Vrtx[g]).ToNormal();
				Vector3D nn2 = Vector3D::Cross(FCon.Vrtx[i2] - FCon.Vrtx[g], FCon.Vrtx[i3] - FCon.Vrtx[g]).ToNormal();
				if (nn1.dot(nn2) > 0) {
					if (isdiff(i1, i2, g)) {
						FCon.Indx.Add(i1);
						FCon.Indx.Add(i2);
						FCon.Indx.Add(g);
					}
					if (isdiff(g, i2, i3)) {
						FCon.Indx.Add(g);
						FCon.Indx.Add(i2);
						FCon.Indx.Add(i3);
					}
				}
				else {
					if (isdiff(i1, i2, i3)) {
						FCon.Indx.Add(i1);
						FCon.Indx.Add(i2);
						FCon.Indx.Add(i3);
					}
					if (isdiff(i1, i3, g)) {
						FCon.Indx.Add(g);
						FCon.Indx.Add(i1);
						FCon.Indx.Add(i3);
					}
				}
				//else assert(0);
				nn1 = (nn1 + nn2).ToNormal();
				N = (N * 0.1 + nn1).ToNormal();
				FCon.Nrm.Add(N);

				if (DebugFillCurve)FCon.check();
			}
		}
		else break;
	}
	SQ.stop();
	int offset = fpos.Count();
	for (int i = 0; i < FCon.Vrtx.GetAmount(); ++i) {
		fpos.Add(FCon.Vrtx[i]);
	}
	for (int i = 0; i < FCon.Indx.GetAmount(); i += 3) {
		cVec3i v(3, 0, 0);
		fraw.Add(v);
		cVec3i f(FCon.Indx[i] + offset, -1, -1);
		fraw.Add(f);
		f[0] = FCon.Indx[i + 1] + offset;
		fraw.Add(f);
		f[0] = FCon.Indx[i + 2] + offset;
		fraw.Add(f);
	}
	mesh.TriSubd(nSubd, &snap_edge_pt, &snap_fac_pt, this);
}

float GetTriQuality(const Vector3D& p1, const Vector3D& p2, const Vector3D& p3){
	float L1 = p1.distance(p2);
	float L2 = p2.distance(p3);
	float L3 = p3.distance(p1);

	float dp1 = 1.0 - cVec3::Dot(p2 - p1, p3 - p1) / L1 / L3;
	float dp2 = 1.0 - cVec3::Dot(p1 - p2, p3 - p2) / L1 / L2;
	float dp3 = 1.0 - cVec3::Dot(p2 - p3, p1 - p3) / L2 / L3;

	if (dp1 < 0.0001 || dp2 < 0.0001 || dp3 < 0.0001)return 1000.0;

	float m = std::min(std::min(dp1, dp2), dp3);

	float Lmax = std::max(std::max(L1, L2), L3) - 1.0;
	float Lmin = std::min(std::min(L1, L2), L3) - 1.0;

	return __abs(m - 0.5)*4.0 + Lmin*Lmin + Lmax*Lmax;
}
float GetTriQuality1(const Vector3D& p1, const Vector3D& p2, const Vector3D& p3){
	float L1 = p1.distance(p2);
	float L2 = p2.distance(p3);
	float L3 = p3.distance(p1);
	float Lmax = std::max(std::max(L1, L2), L3);
	float Lmin = std::min(std::min(L1, L2), L3);
	return __abs(Lmin-1)+__abs(Lmax-1);
}
void ContourToFill::FillByTrianglesChunked(comms::cMeshContainer& mesh, int nSubd, int stages){
	int st = 0;
	nsub = nSubd;
	Prepare();
	auto& fraw = mesh.GetRaw();
	auto& fpos = mesh.GetPositions();
	int typesmooth = 1;
	float angle_thresh[2] = { M_PI*0.5, 0.9*M_PI };//{M_PI*0.5, 0.9*M_PI} {M_PI*5.f/12.f, 3*M_PI/4.f}; //{M_PI*0.5, M_PI}; 
	int prevkey = -1;
	if (!Points.GetAmount()) return;
	int counter = 0;
	int step = 0;
	float Length = 0.f;

	FillContour FCon;
	SnapQueue SQ(this, &FCon);

	FCon.Vrtx.SetCapacity(Points.Count());
	FCon.Nrm.SetCapacity(Points.Count());
	int nc = Points.Count();
	AABoundBox AB;
	AB.SetEmpty();
	int np = Points.Count();
	int start = 0;
	int idx = 0;
	for (int h = 0; h <= MaxHoleID; h++){
		int num = 0;
		for (int i = 0; i < np; i++){
			if (Points[i].HoleID == h){
				FCon.Vrtx.Add(Points[i].Pos);
				FCon.Nrm.Add(Points[i].Normal);
				ContourQueuePoint cp;
				cp.Angle = 0;
				cp.hole = h;
				cp.VertexIndex = idx;
				FCon.Contour.Add(cp);
				FCon.LivePoints.Add(idx);
				AB.AddPoint(Points[i].Pos);
				num++;
				idx++;
			}
		}
		for (int p = 0; p < num; p++){
			ContourQueuePoint& cp = FCon.Contour[start + p];
			cp.NextIndex = start + (p + 1) % num;
			cp.PrevIndex = start + (p - 1 + num) % num;
		}
		for (int i = 0; i < num; i++){
			int si = start + i;
			ContourQueuePoint& cp = FCon.Contour[si];
			int vp = FCon.Contour[cp.PrevIndex].VertexIndex;
			int vn = FCon.Contour[cp.NextIndex].VertexIndex;
			Vector3D d = (FCon.Vrtx[vp] - FCon.Vrtx[vn]).ToNormal();
			FCon.Nrm[si] -= d*FCon.Nrm[si].dot(d);
			FCon.Nrm[si].Normalize();
		}
		start += num;
	}
	AB.Inflate(AB.GetDiagonal() / 10.0);
	int nmax = Points.Count()*Points.Count();
	if (DebugFillCurve){
		//set DebugFillCurve = true, then use UP and DOWN keys to show mor or less triangles 
		AllowDebug(true);
		ClearDbg();
		static float nmax1 = 20;
		static float spd = 0.3;
		if (_KEY(VK_UP)){
			nmax1 += spd;
			if (spd<1)spd *= 1.1;
			else spd *= 1.05;
		}
		else
			if (_KEY(VK_DOWN)){
				nmax1 -= spd;
				if (spd<1)spd *= 1.1;
				else spd *= 1.05;
			}
			else{
				spd = 0.3;
			}
			if (nmax1 < nmax)nmax = nmax1;
	}
	float Len0 = -1;
	static int dd = 0;
	dd++;
	bool simp = _CTRL();
	static float dircoef = 1.0 / 10.0;
	Vector3D pt0(0);
	float maxdst = 0;
	bool firstdone = false;
	int nt = 1;
	static float angs[] = { c_PI / 3.0f*1.1f, c_PI / 2, c_PI / 2.0f*1.5f, c_PI };
	SQ.run();
	cList<cList<int> > bkContours;
	float SnapL = 0;
	bool SeekStrightChunks = true;
	while (FCon.LivePoints.Count()>2 && counter <nmax){
		//ClearDbg();
		SQ.handle();
		counter++;
		//------------------------------------------
		//first, seeking for the chunk between sharp edges
		if (SeekStrightChunks){
			float dmin = 7.75;
			int bestpoint = -1;
			for (int stage = 0; stage < 2; stage++){
				int B = FCon.LivePoints.GetAmount();
				int A = B;
				if (stage == 1 && bestpoint == -1){
					SeekStrightChunks = false;
					break;
				}	
				if (stage == 1) B = 1;
				for (int j = 0; j < B; j++){
					if (stage == 1)j = bestpoint;
					ContourQueuePoint& cpt = FCon.Contour[FCon.LivePoints[j]];
					if (!cpt.hole){
						int startidx = FCon.LivePoints[j];
						int endidx = -1;
						int vpr = FCon.Contour[cpt.PrevIndex].VertexIndex;
						int vnx = FCon.Contour[cpt.NextIndex].VertexIndex;
						Vector3D N0 = (FCon.Nrm[vpr] + FCon.Nrm[vnx] + FCon.Nrm[cpt.VertexIndex]).ToNormal();
						float &angle0 = cpt.Angle;
						if (simp)angle0 = 0;
						Vector3D cp = FCon.Vrtx[cpt.VertexIndex];
						if (__abs(angle0) < 0.0001)angle0 = Angle(cp, FCon.Vrtx[vpr], FCon.Vrtx[vnx], N0);
						if (angle0 < c_PI*0.99){//at least little bending, counting it as start point of the chunk
							///running to the tale of the chunk
							ContourQueuePoint* pc = &cpt;
							ContourQueuePoint* pc0 = pc;
							endidx = pc->NextIndex;
							pc = &FCon.Contour[pc->NextIndex];
							ContourQueuePoint* pp = pc0;
							float L = 0;
							for (int k = 0;; k++){
								int vpr1 = FCon.Contour[pc->PrevIndex].VertexIndex;
								int vnx1 = FCon.Contour[pc->NextIndex].VertexIndex;
								L += FCon.Vrtx[pp->VertexIndex].distance(FCon.Vrtx[pc->VertexIndex]);
								Vector3D N1 = (FCon.Nrm[vpr1] + FCon.Nrm[vnx1] + FCon.Nrm[pc->VertexIndex]).ToNormal();
								float &angle1 = pc->Angle;
								if (simp)angle1 = 0;
								Vector3D cp1 = FCon.Vrtx[pc->VertexIndex];
								if (__abs(angle1) < 0.0001)angle1 = Angle(cp1, FCon.Vrtx[vpr1], FCon.Vrtx[vnx1], N1);
								int ncur = k + 1;
								if (angle1 < c_PI*0.99){///end of chunk
									int id0 = FCon.Contour[pc->NextIndex].VertexIndex;
									int id1 = FCon.Contour[pc0->PrevIndex].VertexIndex;
									int n = ncur - 1;
									bool Ang0Sharp = angle0 <= c_PI / 2;
									bool Ang1Sharp = angle1 <= c_PI / 2;
									if (pc != pc0 && id0 != pc0->VertexIndex && id1 != pc->VertexIndex){
										if (!Ang0Sharp){
											n++;
										}
										if (!Ang1Sharp){
											n++;
										}
									}
									L /= ncur;
									Vector3D PS = FCon.Vrtx[FCon.Contour[pc0->PrevIndex].VertexIndex];
									Vector3D PE = FCon.Vrtx[FCon.Contour[pc->NextIndex].VertexIndex];
									float L1 = PS.distance(PE) / n;
									if (stage == 0){///seeking best chunk stage
										///estimate quality of triangles
										cList<Vector3D> newv;
										cList<Vector3D> orig;	

										Vector3D d1 = (FCon.Vrtx[FCon.Contour[startidx].VertexIndex] - FCon.Vrtx[FCon.Contour[FCon.Contour[startidx].PrevIndex].VertexIndex]).ToNormal();
										Vector3D d2 = (FCon.Vrtx[FCon.Contour[FCon.Contour[endidx].NextIndex].VertexIndex] - FCon.Vrtx[FCon.Contour[endidx].VertexIndex]).ToNormal();
										Vector3D d0 = (PS - PE).ToNormal();
										if (d0.dot(d1) > 0.999 || d0.dot(d2) > 0.999){
											break;
										}
										
										orig.Add(FCon.Vrtx[FCon.Contour[startidx].VertexIndex]);
										ContourQueuePoint* pcc = pc0;
										ContourQueuePoint* ppc = pcc;
										for (int p = 1; p <= ncur; p++){
											orig.Add(FCon.Vrtx[FCon.Contour[pcc->NextIndex].VertexIndex]);
											pcc = &FCon.Contour[pcc->NextIndex];
										}
										newv.Add(FCon.Vrtx[FCon.Contour[pc0->PrevIndex].VertexIndex]);
										for (int p = 1; p < n; p++){
											Vector3D p1 = PS + p*(PE - PS) / n;										
											newv.Add(p1);
										}										
										newv.Add(FCon.Vrtx[FCon.Contour[pc->NextIndex].VertexIndex]);		
										if (newv.Count() + orig.Count() >= A || orig.Count() <= 2){
											break;
										}
										float worstQ = 0;
										if (Ang0Sharp){
											for (int r = 0; r < orig.Count() - 1; r++){
												float Q = GetTriQuality(newv[r], orig[r], orig[r + 1]);
												worstQ = std::max(worstQ, Q);
											}
											for (int r = 1; r < newv.Count(); r++){
												float Q = GetTriQuality(newv[r - 1], orig[r], newv[r]);												
												worstQ = std::max(worstQ, Q);
											}
										}
										else{
											for (int r = 0; r < newv.Count() - 1; r++){
												float Q = GetTriQuality(newv[r], orig[r], orig[r + 1]);
												worstQ = std::max(worstQ, Q);												
											}
											for (int r = 1; r < newv.Count() && r<orig.Count(); r++){
												float Q = GetTriQuality(newv[r], orig[r - 1], orig[r]);
												worstQ = std::max(worstQ, Q);												
											}
										}
										float d = worstQ;
										if (d < dmin){
											dmin = d;
											bestpoint = j;
										}
									}
									else{///create actual geometry
										cList<int> newv;
										cList<int> orig;
										///create vertices
										FCon.Contour[pc0->PrevIndex].Angle = 0;
										FCon.Contour[pc0->NextIndex].Angle = 0;
										Vector3D NS = FCon.Nrm[FCon.Contour[pc0->PrevIndex].VertexIndex];
										Vector3D NE = FCon.Nrm[FCon.Contour[pc->NextIndex].VertexIndex];
										int idx0 = FCon.Contour.Count();
										int lastidx = -1;
										
										orig.Add(startidx);
										ContourQueuePoint* pcc = pc0;
										ContourQueuePoint* ppc = pcc;
										for (int p = 1; p <= ncur; p++){
											orig.Add(pcc->NextIndex);
											pcc = &FCon.Contour[pcc->NextIndex];
										}
										
										newv.Add(pc0->PrevIndex);
										
										for (int p = 1; p < n; p++){
											Vector3D p1 = PS + p*(PE - PS) / n;
											Vector3D n1 = (NS + p*(NE - NS) / n).ToNormal();
											int v = FCon.Vrtx.Add(p1);
											FCon.Nrm.Add(p1);
											ContourQueuePoint qp;
											qp.VertexIndex = v;
											qp.Angle = 0;
											qp.hole = false;											
											lastidx = FCon.Contour.Add(qp);
											FCon.LivePoints.Add(lastidx);
											newv.Add(lastidx);
										}

										pc = &FCon.Contour[endidx];
										pc0 = &FCon.Contour[startidx];

										newv.Add(pc->NextIndex);
										
										///create triangles and remove initial points from the queue										
										
										if (Ang0Sharp){
											for (int r = 0; r < orig.Count() - 1; r++){
												FCon.Indx.Add(FCon.Contour[newv[r]].VertexIndex);
												FCon.Indx.Add(FCon.Contour[orig[r]].VertexIndex);
												FCon.Indx.Add(FCon.Contour[orig[r + 1]].VertexIndex);
											}
											for (int r = 1; r < newv.Count(); r++){
												FCon.Indx.Add(FCon.Contour[newv[r - 1]].VertexIndex);
												FCon.Indx.Add(FCon.Contour[orig[r]].VertexIndex);
												FCon.Indx.Add(FCon.Contour[newv[r]].VertexIndex);
											}
										}
										else{
											for (int r = 0; r < newv.Count() - 1; r++){
												FCon.Indx.Add(FCon.Contour[newv[r]].VertexIndex);
												FCon.Indx.Add(FCon.Contour[orig[r]].VertexIndex);
												FCon.Indx.Add(FCon.Contour[newv[r + 1]].VertexIndex);
											}
											for (int r = 1; r < newv.Count() && r<orig.Count(); r++){
												FCon.Indx.Add(FCon.Contour[newv[r]].VertexIndex);
												FCon.Indx.Add(FCon.Contour[orig[r - 1]].VertexIndex);
												FCon.Indx.Add(FCon.Contour[orig[r]].VertexIndex);
											}
										}
										///set correct connectivity for new elements
										for (int r = 0; r < newv.Count(); r++){
											pcc = &FCon.Contour[newv[r]];
											pcc->Angle = 0;
											if (r > 0)pcc->PrevIndex = newv[r - 1];
											if (r<newv.Count() - 1)pcc->NextIndex = newv[r + 1];
										}
										///eliminate unused queue elements										
										for (int r = 0; r <= ncur; r++){
											pcc = &FCon.Contour[orig[r]];											
											pcc->NextIndex = pcc->PrevIndex = -1;											
										}
										for (int r = 0; r < FCon.LivePoints.Count(); r++){
											int id = FCon.LivePoints[r];
											if (FCon.Contour[id].NextIndex == -1 && FCon.Contour[id].PrevIndex == -1){
												FCon.LivePoints.RemoveAt(r, 1);
												r--;
											}
										}
									}
									break;
								}
								pp = pc;
								endidx = pc->NextIndex;
								pc = &FCon.Contour[pc->NextIndex];
							}
						}
					}
				}
			}	
			st++;
			if (st == stages)break;
			if (bestpoint != -1)continue;
		}
		int delpos = -1;
		float min = FLT_MAX;
		float anl = 0;
		int i1 = -1;
		int i2 = -1;
		int i3 = -1;
		Vector3D V1, V2, V3;
		int A = FCon.LivePoints.GetAmount();
		int na = 0;
		bool allbound = false;
		for (int j = 0; j < A; j++){
			ContourQueuePoint& cpt = FCon.Contour[FCon.LivePoints[j]];
			if (!cpt.hole){
				int vpr = FCon.Contour[cpt.PrevIndex].VertexIndex;
				int vnx = FCon.Contour[cpt.NextIndex].VertexIndex;
				Vector3D N0 = (FCon.Nrm[vpr] + FCon.Nrm[vnx] + FCon.Nrm[cpt.VertexIndex]).ToNormal();
				float &angle = cpt.Angle;
				if (simp)angle = 0;
				Vector3D cp = FCon.Vrtx[cpt.VertexIndex];
				if (__abs(angle) < 0.0001)angle = Angle(cp, FCon.Vrtx[vpr], FCon.Vrtx[vnx], N0);
				float a1 = angle;
				if (a1 > c_PI*1.999)a1 -= 2 * c_PI;

				//a1 = angle;
				if (a1>0.001 && a1<c_PI*0.999 && min >= a1){ //angle
					///parallel test
					if (FCon.Contour[cpt.PrevIndex].PrevIndex != cpt.NextIndex){///not last triangle
						Vector3D d = (FCon.Vrtx[vpr] - FCon.Vrtx[vnx]).ToNormal();
						Vector3D d1 = -(FCon.Vrtx[FCon.Contour[FCon.Contour[cpt.PrevIndex].PrevIndex].VertexIndex] - FCon.Vrtx[vpr]).ToNormal();
						Vector3D d2 = -(FCon.Vrtx[vnx] - FCon.Vrtx[FCon.Contour[FCon.Contour[cpt.NextIndex].NextIndex].VertexIndex] - FCon.Vrtx[vpr]).ToNormal();
						if (d.dot(d1)>0.999 || d.dot(d2) > 0.999){
							continue;
						}
					}
					min = a1;
					anl = angle;
					i1 = vpr;
					i2 = cpt.VertexIndex;
					i3 = vnx;
					delpos = j;
					allbound = FCon.Contour[cpt.PrevIndex].PrevIndex == cpt.NextIndex;
					if (FCon.Contour[FCon.Contour[cpt.PrevIndex].PrevIndex].PrevIndex == cpt.NextIndex){
						allbound = true;
					}
				}
				//Find avg of edge length
				if (step == 0){
					Length += FCon.Vrtx[vpr].Distance(FCon.Vrtx[cpt.VertexIndex]);
					na++;
				}
			}
		}//for j
		if (step == 0 && na && i2 != -1){
			Length /= na;
			SnapL = Length*0.0001;
			pt0 = FCon.Vrtx[i2];
			for (int k = 0; k < A; k++){
				float d = pt0.distance(FCon.Vrtx[FCon.Contour[FCon.LivePoints[k]].VertexIndex]);
				if (d>maxdst){
					maxdst = d;
				}
			}
			nt = 4;
			step++;
		}
		if (delpos >= 0){
			//check here the magnitude of the minimun angle, and create or triangulate the new vertices in the contour
			//and update the contour accordingly
			V1 = FCon.Vrtx[i1];
			V2 = FCon.Vrtx[i2];
			V3 = FCon.Vrtx[i3];

			if (Len0 < 0)Len0 = Length;
			//-------------------------
			min = anl;
			if (min <= angle_thresh[0] || min >= c_PI || A == 3 || allbound){
				//just connecting points, sharp angle case
				if (i1 != i2 && i2 != i3 && i3 != i1){
					FCon.Indx.Add(i1);
					FCon.Indx.Add(i2);
					FCon.Indx.Add(i3);
				}
				ContourQueuePoint& cpt = FCon.Contour[FCon.LivePoints[delpos]];
				FCon.LivePoints.RemoveAt(delpos);
				FCon.Contour[cpt.PrevIndex].NextIndex = cpt.NextIndex;
				FCon.Contour[cpt.NextIndex].PrevIndex = cpt.PrevIndex;

				FCon.Contour[cpt.PrevIndex].Angle = 0;
				FCon.Contour[cpt.NextIndex].Angle = 0;

				FCon.Contour[cpt.PrevIndex].hole = 0;
				FCon.Contour[cpt.NextIndex].hole = 0;
				A--;
				if (DebugFillCurve)FCon.check();
			}
			else{// add 2 triangles
				ContourQueuePoint& cpt = FCon.Contour[FCon.LivePoints[delpos]];

				Vector3D N = (FCon.Nrm[i1] + FCon.Nrm[i2] + FCon.Nrm[i3]).ToNormal(); // .ToNormal(); // 3.f; 
				Vector3D N0 = FCon.Nrm[i2];
				Vector3D pos = V1 + V3 - 2.0f * V2;
				if (min > c_PI)pos = V2*2.0 - pos;
				//pos -= N0*pos.dot(N0);
				float l = pos.Length();
				float l20 = std::max(V1.distance(V2), V3.distance(V2));
				float l2 = l20;// std::min(V1.distance(V2), V3.distance(V2))*0.5 + l20*0.5;
				pos = Vector3D::Normalize((V1 - V2).ToNormal() + (V3 - V2).ToNormal());
				//pos.Normalize();
				float fac = Len0;// std::min(l2, Len0);// *(__rand() + 260000) / (260000 + 16384.0);
				pos = V2 + fac*pos;
				int g = -1;
				SQ.SimpleSnap(pos, N);
				g = FCon.Vrtx.Add(pos);
				SQ.AddPoint(g, pos, N);

				cpt.Angle = 0;
				cpt.hole = 0;
				cpt.VertexIndex = g;
				FCon.Contour[cpt.PrevIndex].Angle = 0;
				FCon.Contour[cpt.NextIndex].Angle = 0;
				FCon.Contour[cpt.PrevIndex].hole = 0;
				FCon.Contour[cpt.NextIndex].hole = 0;
				if (DebugFillCurve){
					if (counter == nmax){
						AddDbgPoint(V2, 0xFFFF0000);
						AddDbgLine(V2, V1, 0xFFFF0000, 0xFF00FF00);
						AddDbgPoint(V1, 0xFF00FF00);
						AddDbgLine(V2, V3, 0xFFFF0000, 0xFF0000FF);
						AddDbgPoint(V3, 0xFF0000FF);
						AddDbgLine(V2, pos, 0xFFFF0000, 0xFFFFFFFF);
						AddDbgPoint(pos, 0xFFFFFFFF);
					}
				}
				int vd = FCon.SnapSomewhere(delpos, SnapL, V2);
				if (vd != -1){
					g = FCon.Contour[vd].VertexIndex;
				}
				Vector3D nn1 = Vector3D::Cross(FCon.Vrtx[i1] - FCon.Vrtx[g], FCon.Vrtx[i2] - FCon.Vrtx[g]).ToNormal();
				Vector3D nn2 = Vector3D::Cross(FCon.Vrtx[i2] - FCon.Vrtx[g], FCon.Vrtx[i3] - FCon.Vrtx[g]).ToNormal();
				if (nn1.dot(nn2) > 0){
					if (isdiff(i1, i2, g)){
						FCon.Indx.Add(i1);
						FCon.Indx.Add(i2);
						FCon.Indx.Add(g);
					}
					if (isdiff(g, i2, i3)){
						FCon.Indx.Add(g);
						FCon.Indx.Add(i2);
						FCon.Indx.Add(i3);
					}
				}
				else{
					if (isdiff(i1, i2, i3)){
						FCon.Indx.Add(i1);
						FCon.Indx.Add(i2);
						FCon.Indx.Add(i3);
					}
					if (isdiff(i1, i3, g)){
						FCon.Indx.Add(g);
						FCon.Indx.Add(i1);
						FCon.Indx.Add(i3);
					}
				}
				//else assert(0);
				nn1 = (nn1 + nn2).ToNormal();
				N = (N*0.1 + nn1).ToNormal();
				FCon.Nrm.Add(N);

				if (DebugFillCurve)FCon.check();
			}
		}
		else break;
	}
	SQ.stop();
	int offset = fpos.Count();
	for (int i = 0; i<FCon.Vrtx.GetAmount(); ++i){
		fpos.Add(FCon.Vrtx[i]);
	}
	for (int i = 0; i<FCon.Indx.GetAmount(); i += 3){
		cVec3i v(3, 0, 0);
		fraw.Add(v);
		cVec3i f(FCon.Indx[i] + offset, -1, -1);
		fraw.Add(f);
		f[0] = FCon.Indx[i + 1] + offset;
		fraw.Add(f);
		f[0] = FCon.Indx[i + 2] + offset;
		fraw.Add(f);
	}
	mesh.TriSubd(nSubd, &snap_edge_pt, &snap_fac_pt, this);
}
void ContourToFill::Clear(){

}
///debig info about normals
void ContourToFill::DbgNormals(){
	if (DebugFillCurve){
		DbgLayer("DbgNormals");
		for (int i = 0; i < Points.Count(); i++){
			AddDbgLine(Points[i].Pos, Points[i].Pos + Points[i].Normal * 5, 0xFFFFFFFF, 0xFF00FF00);
		}
	}
}
///prepare to filling, refine normals etc
void ContourToFill::Prepare(){
	//ClearDbg();
	int np = Points.Count();
	AABoundBox ab;
	ab.SetEmpty();
	for (int k = 0; k < np; k++) {
		ab.AddPoint(Points[k].Pos);
	}
	float Epsl = ab.GetDiagonal() / 10000.0;
	float ddd = Epsl;
	if (Points.Count() > 1){
		if (Points[0].Pos.distance(Points.GetLast().Pos) < Epsl){
			Points.RemoveLast();
		}
	}
	np = Points.Count();
	int nhole = 0;
	for (int k = 0; k < np; k++){
		if (!Points[k].HoleID){
			nhole++;
		}
	}
	if (np > 2){
		//check direction of links, maybe need to revert
		int nn = 0;
		float L = 0;
		float L1 = 0;

		float SummAngle = 0;
		int NumAngles = 0;

		cList<Vector3D> PtEx;
		for (int k = 0; k < np; k++) {
			int h = Points[k].HoleID;
			int p = (k - 1 + np) % np;
			if (Points[p].HoleID != h) {
				int ni = 0;
				while (Points[p].HoleID != h) {
					p--;
					if (p < 0)p += np;
					if (ni++ > np) {
						p = k;
						break;
					}
				}
			};
			int n = (k + 1) % np;
			if (Points[n].HoleID != h) {
				int ni = 0;
				while (Points[n].HoleID != h) {
					n++;
					if (n >= np)n -= np;
					if (ni++ > np) {
						p = k;
						break;
					}
				}
			}
			Vector3D c = Vector3D::Zero;
			if(n>p) {
				Vector3D pc = Points[k].Pos;
				Vector3D dn = (Points[n].Pos - pc).ToNormal();
				Vector3D dp = (Points[p].Pos - pc).ToNormal();
				float L = sqrt(2.0f/(1.01f-dn.dot(dp)));
				Vector3D d = (dn - dp);
				d.Normalize();
				c += Vector3D::Cross(Points[k].Normal, d).ToNormal() * L;
			}
			PtEx.Add(c);
		}
		/*
		AllowDebug(true);
		ClearDbg();
		DbgLayer("OffsPoints");
		for (int k = 1; k < np; k++) {
			if (Points[k].HoleID == Points[k - 1].HoleID) {
				Vector3D dv = Points[k].Pos - Points[k - 1].Pos;
				float L0 = dv.Length();
				dv += (PtEx[k] - PtEx[k - 1]) * ddd;
				L0 = dv.Length() - L0;
				L1 += L0;
				AddDbgLine(Points[k].Pos + PtEx[k] * 0.1, Points[k - 1].Pos + PtEx[k - 1] * 0.1, 0xFF000000, 0xFFFFFFFF);
			}
		}
		*/
		
			
		for (int i = 0; i < nhole; i++){
			int vp = (i + nhole - 1) % nhole;
			int vn = (i + 1) % nhole;
			float ang = Angle(Points[i].Pos, Points[vp].Pos, Points[vn].Pos, Points[i].Normal);
			Points[i].Angle = ang;
			if (Points[i].HoleID == 0){
				nn++;
				SummAngle += ang;
			}
		}
		if (nn){
			SummAngle /= nn;
			if (LockContourOrder==false && (SummAngle > c_PI) ^ InvertOrder){//revert order
				WasFlipped = true;
				int phole = -1;
				for (int k = 0; k < np; k++){
					if (phole != Points[k].HoleID){
						phole = Points[k].HoleID;
						int last = k;
						for (; last < np && Points[last].HoleID == phole; last++);
						last--;
						for (int i = k, j = last; i < j; i++, j--){
							std::swap(Points[i], Points[j]);
						}
						k = last;
					}
				}
			}
		}
		//check, is it plane?
		AABoundBox AB;
		AB.SetEmpty();
		cList<Vector3D> pts;
		Vector3D avn(0);
		for (int i = 0; i < np; i++){
			pts.Add(Points[i].Pos);
			AB.AddPoint(pts[i]);
			if (!Points[i].HoleID)avn += Points[i].Normal;
		}
		float ds = 0;
		pl = OneCurveObject::GetAveragePlane(pts, &ds);
		float dmin = AB.GetDiagonal() / 50.0;
		UsePlane = ds < dmin / 8.0;
		NeedToSnap = ForceSnaping;
		UseHB = false;
		if (UsePlane){
			if (avn.dot(pl.GetNormal())<0){
				pl.FlipNormal();
			}
			for (int i = 0; i < np; i++){
				Points[i].Normal = pl.GetNormal();
			}
		}
		if (ForceSnaping){
			NeedToSnap = true;
			float ns = 0;
			float nns = 0;
			for (int i = 0; i < np; i++){
				DWORD B = -2;
				Vector3D np = PMS().PutPointOnSurface(Points[i].Pos, Points[i].Normal, B);
				float ons = 1.0 - np.distance(Points[i].Pos) / dmin / 5.0;
				if (B == -2 && ons>0.999)ons = 0;
				clamp01(ons);
				ns += ons;
				nns += 1.0 - ons;
			}
			if (ns*2 < nns){
				NeedToSnap = false;
			}
		}

		WholeSet = Points;
		if (nsub){
			Points.Clear();
			for (int i = 0; i < WholeSet.Count(); i += nsub + 1){
				Points.Add(WholeSet[i]);
			}
		}

		if (!(NeedToSnap || UsePlane)){
			//cleanup normals
			int np1 = Points.Count();
			std_for(i, np1){
				Vector3D nn(0);
				Vector3D c = Points[i].Pos;
				for (int j = 0; j < nhole; j++){
					int nj = (j + 1) % nhole;
					Vector3D p1 = Points[j].Pos;
					Vector3D p2 = Points[nj].Pos;
					float d = c.distance((p1 + p2)*0.5);
					if (d > 0){
						d *= d;
						nn += Vector3D::Cross(c - p1, c - p2) / d;
					}
				}
				Vector3D vn = Points[(i + 1) % nhole].Pos - Points[(i + nhole - 1) % nhole].Pos;
				vn.Normalize();
				nn -= vn*vn.dot(nn);
				nn.Normalize();

				Points[i].Normal = nn;
			}std_for_end;
		}
		RelaxNormals(4);
		if (! (NeedToSnap || UsePlane)){
			NeedToSnap = false;
			UseHB = true;
			for (int i = 0; i < Points.Count(); i++){
				HB.AddNode(Points[i].Pos, Points[i].Normal);
			}
		}
	}
}
void ContourToFill::RelaxNormals(int times){
	int np = Points.Count();
	for (int k = 0; k < times; k++){
		for (int i = 0; i < np; i++){
			int ic = k & 1 ? i : np - i - 1;
			if (Points[ic].Angle < 0.001 && Points[ic].HoleID==0){
				int in = (ic + 1) % np;
				int ip = (ic + np - 1) % np;
				Vector3D na = Points[ip].Normal + Points[in].Normal + Points[ic].Normal;
				Vector3D d1 = Points[in].Pos - Points[ic].Pos;
				Vector3D d2 = Points[ip].Pos - Points[ic].Pos;
				d1.Normalize();
				d2.Normalize();
				na -= d1*na.dot(d1);
				na -= d2*na.dot(d2);
				na.Normalize();
				Points[ic].Normal = na;
			}
		}
	}
}
void ContourToFill::StartHole(){
	MaxHoleID++;
	HoleID = MaxHoleID;
}
void ContourToFill::FinishHole(){
	HoleID = 0;
}
void ContourToFill::RevertHole(){
	for (int i = 0; i < Points.Count(); i++){
		if (Points[i].HoleID == HoleID){
			for (int k = i, p = Points.Count() - 1; k < p; k++, p--){
				std::swap(Points[k], Points[p]);
			}
			break;
		}
	}
}
void ContourToFill::InvertHoleNormal(){
	for (int i = 0; i < Points.Count(); i++){
		if (Points[i].HoleID == HoleID){
			for (int k = i; k < Points.Count(); k++){
				Points[k].Normal *= -1;
			}
			break;
		}
	}
}
void ContourToFill::AddPoint(const Vector3D& Pos, const Vector3D& Normal){
	ContourPoint cp;
	cp.Pos = Pos;
	cp.Normal = Normal;
	cp.HoleID = HoleID;
	Points.Add(cp);
}
int FillContour::SnapSomewhere(int LiveIndex, float MaxDistance, Vector3D Origin){
	float MaxDistance2 = MaxDistance*MaxDistance;
	int dest = -1;
	float dist1 = FLT_MAX;
	for (int i = 0; i < LivePoints.Count(); i++){
		Contour[LivePoints[i]].Pin = 0;
	}
	Contour[LivePoints[LiveIndex]].Pin = 2;
	
	int idxp = Contour[LivePoints[LiveIndex]].PrevIndex;
	int idxpp = Contour[idxp].PrevIndex;
	int idxn = Contour[LivePoints[LiveIndex]].NextIndex;
	int idxnn = Contour[idxn].NextIndex;

	Vector3D np[2] = { Vrtx[Contour[idxp].VertexIndex],Vrtx[Contour[idxn].VertexIndex] };
	Vector3D np1[2] = { Vrtx[Contour[idxpp].VertexIndex],Vrtx[Contour[idxnn].VertexIndex] };
	Vector3D ds[2] = {(np1[0]-np[0]).ToNormal(), (np1[1] - np[1]).ToNormal() };
	
	int v00 = LivePoints[LiveIndex];
	int vcoor = Contour[LivePoints[LiveIndex]].VertexIndex;
	ContourQueuePoint* cp1 = &Contour[v00];
	ContourQueuePoint* cp2 = cp1;
	ContourQueuePoint* cp0 = cp1;
	Vector3D p0 = Vrtx[cp0->VertexIndex];

	Vector3D Dir = p0 - Origin;
	float LD = Dir.Length();
	Dir.Normalize();

	for (int k = 0; k < 1; k++){
		cp1->Pin = 2;
		cp2->Pin = 2;
		cp1->hole = 0;
		cp2->hole = 0;
		cp1 = &Contour[cp1->PrevIndex];
		cp2 = &Contour[cp2->NextIndex];
	}
	int k = 0;
	for (int k = 0; k < LivePoints.Count(); k++){
		cp2 = &Contour[cp2->NextIndex];
		if (cp2->Pin == 2)break;
		cp2->Pin = 1;
		cp2->hole = 0;
	}
	ContourQueuePoint* cpd = NULL;
	int vdst = -1;
	for (int i = 0; i < LivePoints.Count(); i++){
		ContourQueuePoint* cp = &Contour[LivePoints[i]];
		if (cp->VertexIndex != vcoor){
			if (cp->Pin == 1 || (cp->hole && cp->Pin != 2)){//only within current contour, holes allowed as well
				Vector3D v3 = Vrtx[cp->VertexIndex];
				bool fail = false;
				for (int k = 0; k < 2; k++) {
					Vector3D v1 = (v3 - np[k]).ToNormal();
					if(v1.dot(ds[k]) > 0.999) {
						fail = true;
						break;
					}
				}
				if (!fail) {
					float p = (v3 - Origin).dot(Dir) * 0.9;
					if (p >= -0.1) {
						p += 0.1;
						if (p > LD)p = LD;
						Vector3D pd = Origin + p * Dir;
						float d = v3.distance(pd);
						if (d < MaxDistance * p / LD) {
							float d2 = v3.DistanceSq(Origin, v3);
							if (d2 < dist1) {
								dist1 = d2;
								cpd = cp;
								vdst = LivePoints[i];
							}
						}
					}
				}
			}
		}
	}
	if (cpd){
		//LivePoints.RemoveAt(LiveIndex, 1);
		ContourQueuePoint cpbk = *cp0;
		cp0->VertexIndex = cpd->VertexIndex;
		cp0->PrevIndex = cpd->PrevIndex;
		cpd->PrevIndex = cpbk.PrevIndex;
		Contour[cp0->PrevIndex].NextIndex = v00;
		Contour[cp0->NextIndex].PrevIndex = v00;
		Contour[cpd->PrevIndex].NextIndex = vdst;
		Contour[cpd->NextIndex].PrevIndex = vdst;
		//check();
		return vdst;
	}
	return -1;
}
void ContourToFill::InfiniteSmooth(comms::cMeshContainer& mc){
	auto& raw = mc.GetRaw();
	auto& pos = mc.GetPositions();
	int nr = raw.Count();
	sparse::MatrixD sm;
	int nv = mc.GetPositions().Count();
	sm.Init(nv, 3);
	for (int k = 0; k < nr; k++){
		int n = raw[k][0];
		for (int j = 0; j < n; j++){
			int v = raw[k + 1 + j][0];
			int v1 = raw[k + 1 + (j + 1) % n][0];
			sm.elem(v, v)++;
			sm.elem(v1, v1)++;
			sm.elem(v, v1)--;
			sm.elem(v1, v)--;
		}
		k += n;
	}
	for (int i = 0; i<nv; i++){
		sm.rhs(i, 0) = 0;
		sm.rhs(i, 1) = 0;
		sm.rhs(i, 2) = 0;
		sm.SetVar(i, 0, pos[i].x);
		sm.SetVar(i, 1, pos[i].y);
		sm.SetVar(i, 2, pos[i].z);
	}
	for (int i = 0; i<nv; i++){
		if (i < Points.Count() || Pin.get(i))sm.LockVar(i, true);
	}
	sm.solve_symm();
	for (int i = 0; i<pos.Count(); i++){
		pos[i].x = sm.GetVar(i, 0);
		pos[i].y = sm.GetVar(i, 1);
		pos[i].z = sm.GetVar(i, 2);
	}
}
void ContourToFill::AddContous(cList<ContourOnSurf>& Contours, OneCurveObject* cu)
{
	for (int j = 0; j < Contours.Count(); j++) {
		ContourOnSurf& c = Contours[j];
		if (j)StartHole();
		float summ = 0;
		Vector3D hc(0);
		int nc = 0;
		int np = c.Points.Count();
		comms::cPlane pl = pl = cu->GetAveragePlane(true);
		if (np > 3) {
			//adding point to contour
			Vector3D nsumm(0);
			for (int k = 0; k < c.Points.Count(); k++) {
				Vector3D pt = c.Points[k];
				Vector3D nm = c.Normals[k];
				if (cu->KeepInPlane) {
					pl.ProjectPoint(pt);
					nm = pl.GetNormal();
				}
				AddPoint(pt, nm);
				nsumm += nm;
				hc += pt;
				nc++;
			}
			hc /= nc;
			nsumm.Normalize();
			//estimate - are curve and hole cycling oppisite direction?
			if (j) {
				float sr = 0;
				Vector3D avn(0);
				Vector3D avp(0);
				for (int k = 0; k < c.Points.Count(); k++) {
					Vector3D pt = c.Points[k];
					avp += pt;
					sr += pt.distance(hc);
					avn += c.Normals[k];
				}
				avn.Normalize();
				sr /= nc;
				avp /= nc;
				if (sr < 0.0001)sr = 0.0001;
				comms::cPlane P = pl;
				if (!cu->KeepInPlane) {
					P.SetFromPointAndNormal(avp, avn);
					//P = cu->GetAverageLocalPlane(hc, sr);
				}
				float s0 = cu->GetSquareInProjection(avp, P.GetNormal(), sr);
				float s1 = 0;
				for (int k = 1; k < c.Points.Count(); k++) {
					s1 += Vector3D::Cross(c.Points[k - 1] - hc, c.Points[k] - hc).dot(P.GetNormal());
				}
				if (nsumm.dot(P.GetNormal()) < 0) {
					if (!_ALT())InvertHoleNormal();
				}
				if (s1 * s0 < 0) {
					//need to revert contour because they cycle same direction
					RevertHole();
				}
			}
		}
		FinishHole();
	}
}


//======================================================
void ContourQuadrangulator::CreateFromMesh(comms::cMeshContainer* mesh) {
	mc = mesh;
	mesh->CreateFone();
	mesh->CalcNormals();
	cList<int> enc;
	enc.Add(-1, mesh->GetPositions().Count());
	auto& raw = mesh->GetRaw();
	cList<fAB> _ab;
	for (int i = 0; i < raw.Count(); i++) {
		int n = raw[i][0];
		fAB abb;
		abb.ab.SetEmpty();
		abb.idx = i;
		for (int j = 0; j < n; j++) {
			int v1 = raw[i + j + 1][0];
			abb.ab.AddPoint(mc->GetPosition(v1));
			int v2 = raw[i + (j + 1) % n + 1][0];
			if (mesh->GetEdgeFaceCount(v1, v2) == 1) {
				if (enc[v1] == -1) {
					qContourPoint cp;
					cp.IsInitial = true;
					cp.NextPoint = cp.PrevPoint = -1;
					cp.pos = mesh->GetPosition(v1);
					cp.Normal = mesh->GetNormal(v1);
					enc[v1] = Contour.Add(cp);
				}
				if (enc[v2] == -1) {
					qContourPoint cp;
					cp.IsInitial = true;
					cp.NextPoint = cp.PrevPoint = -1;
					cp.pos = mesh->GetPosition(v2);
					cp.Normal = mesh->GetNormal(v2);
					enc[v2] = Contour.Add(cp);
				}
				Contour[enc[v2]].PrevPoint = enc[v1];
				Contour[enc[v1]].NextPoint = enc[v2];
			}
		}
		_ab.Add(abb);
		i += n;
	}
	UpdateDirections(Contour, true);
	mcPicker.Init(_ab.ToPtr(), _ab.Count(), 0, fpool);
}

void ContourQuadrangulator::CreateBgFromMesh(comms::cMeshContainer* mesh) {
	mc = mesh;
	mesh->CalcNormals();
	auto& raw = mesh->GetRaw();
	cList<fAB> _ab;
	for (int i = 0; i < raw.Count(); i++) {
		int n = raw[i][0];
		fAB abb;
		abb.ab.SetEmpty();
		abb.idx = i;
		for (int j = 0; j < n; j++) {
			int v1 = raw[i + j + 1][0];
			abb.ab.AddPoint(mc->GetPosition(v1));
		}
		_ab.Add(abb);
		i += n;
	}
	
	mcPicker.Init(_ab.ToPtr(), _ab.Count(), 0, fpool);
}

void ContourQuadrangulator::CreateFromList(cList<int>& edges) {
	cList<int> enc;
	enc.Add(-1, mc->GetPositions().Count());
	for (int i = 0; i < edges.Count(); i += 2) {
		int v1 = edges[i];
		int v2 = edges[i + 1];
		if (enc[v1] == -1) {
			qContourPoint cp;
			cp.IsInitial = true;
			cp.NextPoint = cp.PrevPoint = -1;
			cp.pos = mc->GetPosition(v1);
			cp.Normal = mc->GetNormal(v1);
			enc[v1] = Contour.Add(cp);
		}
		if (enc[v2] == -1) {
			qContourPoint cp;
			cp.IsInitial = true;
			cp.NextPoint = cp.PrevPoint = -1;
			cp.pos = mc->GetPosition(v2);
			cp.Normal = mc->GetNormal(v2);
			enc[v2] = Contour.Add(cp);
		}
		Contour[enc[v2]].PrevPoint = enc[v1];
		Contour[enc[v1]].NextPoint = enc[v2];
	}
	UpdateDirections(Contour, true);
}

void ContourQuadrangulator::Clear() {
	Contour.Clear();
	Faces.Clear();
	vnv.reset();
}

double qangle(const comms::dVec3& v2, const comms::dVec3& v1, const comms::dVec3& v3) {
	comms::dVec3 d1 = (v2 - v1).ToNormal();
	comms::dVec3 d2 = (v3 - v1).ToNormal();
	double dp = d1.dot(d2);
	if (dp > 0.999999)dp = 0.999999;
	double ang = acos(dp) * 180 / c_PI;
	if (ang < 0)ang += 360.0;
	return ang;
}
double qquality(const comms::dVec3 & v1, const comms::dVec3 & v2, const comms::dVec3 & v3, const comms::dVec3 & v4) {
	double L1 = v1.distance(v2);
	double L2 = v2.distance(v3);
	double L3 = v3.distance(v4);
	double L4 = v4.distance(v1);
	double a1 = qangle(v1, v2, v3);
	double a2 = qangle(v2, v3, v4);
	double a3 = qangle(v3, v4, v1);
	double a4 = qangle(v4, v1, v2);
	a1 = std::min(a1, a2);
	a2 = std::min(a3, a4);
	return std::min(a1, a2);
}
double ContourQuadrangulator::TryToExpandChunk(int start_point, cList<qContourPoint> & new_contour, cList<int> & newfaces)
{
	///find end vertex
	int end_point = start_point;
	double summ_L = 0;
	double derived_L = 0;
	double edgedist = 0;
	double minang = 90;
	int num_chunks = 0;
	double price = 0;
	double worstq = 0;
	cList<comms::dVec3> old_chunks;
	cList<int> old_ids;
	new_contour = Contour;
	qContourPoint* cp = &new_contour[end_point];
	if (cp->Angle < 15) {
		int p = cp->PrevPoint;
		int pp = new_contour[p].PrevPoint;
		int n = cp->NextPoint;
		int nn = new_contour[n].NextPoint;
		double pnn = new_contour[p].pos.distance(new_contour[nn].pos);
		double ppn = new_contour[pp].pos.distance(new_contour[n].pos);
		if (pnn < ppn) {
			newfaces.Add(4);
			newfaces.Add(start_point);
			newfaces.Add(n);
			newfaces.Add(nn);
			newfaces.Add(p);

			new_contour[start_point].PrevPoint = new_contour[start_point].NextPoint = -1;
			new_contour[n].PrevPoint = new_contour[n].NextPoint = -1;
			new_contour[nn].PrevPoint = p;
			new_contour[p].NextPoint = nn;
		}
		else {
			newfaces.Add(4);
			newfaces.Add(start_point);
			newfaces.Add(n);
			newfaces.Add(pp);
			newfaces.Add(p);

			new_contour[start_point].PrevPoint = new_contour[start_point].NextPoint = -1;
			new_contour[p].PrevPoint = new_contour[p].NextPoint = -1;
			new_contour[n].PrevPoint = pp;
			new_contour[pp].NextPoint = n;
		}
		selfcheck(new_contour);
		return 0;
	}
	do {
		num_chunks++;
		end_point = cp->NextPoint;
		qContourPoint* cpn = &new_contour[end_point];
		summ_L += cp->pos.distance(cpn->pos);
		derived_L += (cp->DerivedEdgeLength + cpn->DerivedEdgeLength) / 2.0;
		edgedist += cp->avdist;
		if (end_point == start_point || new_contour[end_point].EdgeType)break;
		cp = cpn;
	} while (true);
	///average length
	summ_L /= num_chunks;
	derived_L /= num_chunks;
	edgedist /= num_chunks;
	if (start_point != end_point) {
		int pre_start = new_contour[start_point].PrevPoint;
		int post_end = new_contour[end_point].NextPoint;
		if (pre_start == -1 || post_end == -1)return DBL_MAX;
		int stype = new_contour[start_point].EdgeType;
		int etype = new_contour[end_point].EdgeType;
		//forward step
		comms::dVec3 pepos = etype != -1 ? new_contour[post_end].pos : new_contour[end_point].pos * 2 - new_contour[post_end].pos;
		comms::dVec3 pspos = stype != -1 ? new_contour[pre_start].pos : new_contour[start_point].pos * 2 - new_contour[pre_start].pos;
		double start_F = new_contour[start_point].front2.dot(pspos - new_contour[start_point].pos);
		double end_F = new_contour[end_point].front1.dot(pepos - new_contour[end_point].pos);
		//step to the right
		double start_T = new_contour[start_point].tangent2.dot(pspos - new_contour[start_point].pos);
		double end_T = new_contour[end_point].tangent1.dot(new_contour[end_point].pos - pepos);
		int num_chunks1 = num_chunks;
		int s = start_point;
		int p = 1;
		double new_sum_L = 0;
		cList<comms::dVec3> chunks;
		cList<comms::dVec3> chunksN;
		cList<int> chunkIds;
		chunks.Add(pspos);
		chunkIds.Add(stype == -1 ? -1 : pre_start);
		chunksN.Add(new_contour[start_point].Normal);
		old_chunks.Add(new_contour[start_point].pos);
		old_ids.Add(start_point);

		comms::dVec3 pt = pspos;
		do {
			s = new_contour[s].NextPoint;
			qContourPoint* sp = &new_contour[s];
			old_chunks.Add(sp->pos);
			old_ids.Add(s);
			float t = double(p++) / num_chunks;
			double Fc = start_F * (1.0 - t) + end_F * t;
			double Tc = start_T * (1.0 - t) + end_T * t;
			comms::dVec3 npt;
			comms::dVec3 nrm1 = sp->Normal;
			if (s == end_point)npt = pepos;
			else npt = sp->pos + Fc * sp->front2 + Tc * sp->tangent2;
			chunks.Add(npt);
			chunkIds.Add(-1);
			chunksN.Add(nrm1);
			new_sum_L += pt.distance(npt);
			pt = npt;
		} while (s != end_point);
		int nn = chunks.Count();
		if (nn > 3) {
			std_for(k, nn) {
				SnapToMesh(chunks[k], chunksN[k]);
			}std_for_end;
		}
		else {
			for (int k = 0; k < nn; k++) {
				SnapToMesh(chunks[k], chunksN[k]);
			}
		}
		if (etype != -1) {
			chunkIds.RemoveLast();
			chunkIds.Add(post_end);
		}
		//smooth
		for (int k = 0; k < 5; k++) {
			for (int p = 1; p < chunks.Count() - 1; p++) {
				comms::dVec3 T = (chunks[p - 1] - chunks[p + 1]).ToNormal();
				comms::dVec3 d = (chunks[p - 1] + chunks[p + 1]) / 2.0 - chunks[p];
				chunks[p] += T * T.dot(d) * 0.35 + d * 0.15;
			}
			for (int p = chunks.Count() - 2; p > 0; p--) {
				comms::dVec3 T = (chunks[p - 1] - chunks[p + 1]).ToNormal();
				comms::dVec3 d = (chunks[p - 1] + chunks[p + 1]) / 2.0 - chunks[p];
				chunks[p] += T * T.dot(d) * 0.35 + d * 0.15;
			}
		}
		double new_chunk_len = new_sum_L / num_chunks;
		double proportion = new_chunk_len / (derived_L + 0.000001);
		num_chunks1 = chunks.Count() - 1;
		///estimate quality of quads
		for (int i = 0; i < chunks.Count() - 1; i++) {
			comms::dVec3 v1 = chunks[i];
			comms::dVec3 v2 = chunks[i + 1];
			comms::dVec3 v3 = old_chunks[i + 1];
			comms::dVec3 v4 = old_chunks[i];
			double q = qquality(v1, v2, v3, v4);
			minang = std::min(minang, q);
		}
		///update the contour
		int starterase = stype == 1 ? start_point : new_contour[start_point].NextPoint;
		int lasterase = etype == 1 ? end_point : new_contour[end_point].PrevPoint;
		if (starterase == -1 || lasterase == -1)return DBL_MAX;
		for (int i = 0; i < chunkIds.Count(); i++) {
			if (chunkIds[i] == -1) {
				qContourPoint cpt;
				cpt.pos = chunks[i];
				cpt.Normal = chunksN[i];
				chunkIds[i] = new_contour.Add(cpt);
			}
		}
		for (int i = 0; i < chunkIds.Count(); i++) {
			qContourPoint& cp = new_contour[chunkIds[i]];
			if (i < chunkIds.Count() - 1)cp.NextPoint = chunkIds[i + 1];
			if (i)cp.PrevPoint = chunkIds[i - 1];
			cp.DerivedEdgeLength = derived_L;
			cp.ParentVertex = old_ids[i];
			cp.avdist = 0;// edgedist + 1;
		}
		qContourPoint& cpFirst = new_contour[chunkIds[0]];
		if (stype == -1) {
			cpFirst.PrevPoint = start_point;
			new_contour[start_point].NextPoint = chunkIds[0];
		}
		qContourPoint& cpLast = new_contour[chunkIds.GetLast()];
		if (etype == -1) {
			cpLast.NextPoint = end_point;
			new_contour[end_point].PrevPoint = chunkIds.GetLast();
		}
		for (int i = 0; i < 10000; i++) {
			int p1 = new_contour[starterase].NextPoint;
			new_contour[starterase].NextPoint = new_contour[starterase].PrevPoint = -1;
			if (starterase == lasterase)break;
			starterase = p1;
			if (starterase == -1)return DBL_MAX;
		}
		for (int i = 0; i < chunks.Count() - 1; i++) {
			newfaces.Add(4);
			newfaces.Add(old_ids[i]);
			newfaces.Add(old_ids[i + 1]);
			newfaces.Add(chunkIds[i + 1]);
			newfaces.Add(chunkIds[i]);
		}
		worstq = proportion > 1 ? proportion - 1.0 : 1.0 / proportion - 1.0;
		if (stype == -1)worstq += 4;
		if (etype == -1)worstq += 4;
		if (old_chunks.Count() <= 2 && (stype == -1 || etype == -1))worstq += 5;
		if (old_chunks.Count() == 2 && (stype == 1 || etype == 1))worstq -= 5;
	} else {
		//closed curve
		int s = start_point;
		bool sharpcase = Contour[s].Angle < 110.0;
		cList<comms::dVec3> chunks;
		cList<comms::dVec3> chunksN;
		cList<int> chunkIds;
		cList<comms::dVec3> new_chunks;
		cList<comms::dVec3> new_chunksN;
		cList<int> new_chunkIds;
		double proportion = 1.0;
		do {
			comms::dVec3 cs = new_contour[s].pos;
			int& p = new_contour[s].PrevPoint;
			int& n = new_contour[s].NextPoint;
			comms::dVec3 cn = new_contour[n].pos;
			comms::dVec3 cp = new_contour[p].pos;
			comms::dVec3 d = (new_contour[s].front1 + new_contour[s].front2).ToNormal() * (cs.distance(cn) + cs.distance(cp)) * 0.5;
			chunks.Add(cs);
			new_chunks.Add(cs + d);
			chunksN.Add(new_contour[s].Normal);
			new_chunksN.Add(new_contour[s].Normal);
			chunkIds.Add(s);
			new_chunkIds.Add(-1);
			s = n;
			p = n = -1;
		} while (s != start_point);
		int nn = new_chunks.Count();
		if (nn < 3)return DBL_MAX;
		//smooth
		int nc = chunks.Count();
		cList< comms::dVec3> temp;
		for (int k = 0; k < 5; k++) {
			temp = new_chunks;
			for (int p = 0; p < nc; p++) {
				int pp = (p + nc - 1) % nc;
				int nn = (p + 1) % nc;
				comms::dVec3 T = (temp[pp] - temp[nn]).ToNormal();
				comms::dVec3 d = (temp[pp] + temp[nn]) / 2.0 - temp[p];
				new_chunks[p] += T * T.dot(d) * 0.15 + d * 0.05;
			}
			if (sharpcase) {
				//chunks[0] = (chunks[1] + chunks[nc - 1]) / 2.0;
				chunks[1] = chunks[nc - 1] = chunks[0];
			}
			chunksN[0] = (chunksN[1] + chunksN.GetLast()).ToNormal();
			chunksN[1] = chunksN.GetLast() = chunksN[0];
		}
		std_for(k, nn) {
			SnapToMesh(new_chunks[k], new_chunksN[k]);
		}std_for_end;
		if (sharpcase) {
			for (int i = 0; i < new_chunkIds.Count() - 1; i++) {
				if (new_chunkIds[i] == -1 && i != 1) {
					qContourPoint cpt;
					cpt.pos = new_chunks[i];
					cpt.Normal = new_chunksN[i];
					cpt.ParentVertex = chunkIds[i];
					new_chunkIds[i] = new_contour.Add(cpt);
				}
			}
			new_chunkIds[1] = new_chunkIds.GetLast() = new_chunkIds[0];
			double sl = 0;
			double sln = 0;
			for (int i = 0; i < new_chunkIds.Count() - 1; i++) {
				if (i != 1) {
					qContourPoint& cp = new_contour[new_chunkIds[i]];
					int in = (i + 1) % new_chunkIds.Count();
					int ip = (i + new_chunkIds.Count() - 1) % new_chunkIds.Count();
					if (i == 0) {
						in = 2;
						ip= (i + new_chunkIds.Count() - 2) % new_chunkIds.Count();
					}
					cp.NextPoint = new_chunkIds[in];
					cp.PrevPoint = new_chunkIds[ip];
					cp.DerivedEdgeLength = new_contour[chunkIds[i]].DerivedEdgeLength;
					sl += chunks[i].distance(chunks[in]);
					sln += new_chunks[i].distance(new_chunks[in]);
				}
			}
			proportion = (sl + 0.000001) / (sln + 0.000001);
			for (int i = 1; i < chunks.Count() - 1; i++) {
				int i1 = (i + 1) % chunks.Count();
				newfaces.Add(4);
				newfaces.Add(chunkIds[i]);
				newfaces.Add(chunkIds[i1]);
				newfaces.Add(new_chunkIds[i1]);
				newfaces.Add(new_chunkIds[i]);
			}
			newfaces.Add(4);
			newfaces.Add(chunkIds[0]);
			newfaces.Add(chunkIds[1]);
			newfaces.Add(new_chunkIds[0]);
			newfaces.Add(chunkIds.GetLast());
		}
		else {
			for (int i = 0; i < new_chunkIds.Count(); i++) {
				if (new_chunkIds[i] == -1) {
					qContourPoint cpt;
					cpt.pos = new_chunks[i];
					cpt.Normal = new_chunksN[i];
					cpt.ParentVertex = chunkIds[i];
					new_chunkIds[i] = new_contour.Add(cpt);
				}
			}
			double sl = 0;
			double sln = 0;
			for (int i = 0; i < new_chunkIds.Count(); i++) {
				qContourPoint& cp = new_contour[new_chunkIds[i]];
				int in = (i + 1) % new_chunkIds.Count();
				int ip = (i + new_chunkIds.Count() - 1) % new_chunkIds.Count();
				cp.NextPoint = new_chunkIds[in];
				cp.PrevPoint = new_chunkIds[ip];
				cp.DerivedEdgeLength = new_contour[chunkIds[i]].DerivedEdgeLength;
				sl += chunks[i].distance(chunks[in]);
				sln += new_chunks[i].distance(new_chunks[in]);
			}
			proportion = (sl + 0.000001) / (sln + 0.000001);
			for (int i = 0; i < chunks.Count(); i++) {
				int i1 = (i + 1) % chunks.Count();
				newfaces.Add(4);
				newfaces.Add(chunkIds[i]);
				newfaces.Add(chunkIds[i1]);
				newfaces.Add(new_chunkIds[i1]);
				newfaces.Add(new_chunkIds[i]);
			}
		}
		worstq = proportion > 1 ? proportion - 1.0 : 1.0 / proportion - 1.0;		
	}
	selfcheck(new_contour);
	return worstq + old_chunks.Count() / 3.0 + (95.0 / (minang + 5.0) - 1.0) * 0.25 + edgedist / 5.0;
}
void ContourQuadrangulator::UpdateDirections(cList<qContourPoint> & contour, bool derived, double creaseangle) {
	for (int i = 0; i < contour.Count(); i++) {
		qContourPoint& cp = contour[i];
		cp.Angle = 0;
		if (cp.PrevPoint != -1 && cp.NextPoint != -1) {
			cp.tangent2 = (contour[cp.NextPoint].pos - cp.pos).ToNormal();
			cp.tangent1 = (contour[cp.PrevPoint].pos - cp.pos).ToNormal();
			cp.front1 = comms::dVec3::Cross(cp.tangent1, cp.Normal);
			cp.front2 = comms::dVec3::Cross(cp.Normal, cp.tangent2);
			double angle = atan2(cp.tangent2.dot(cp.front1), cp.tangent1.dot(cp.tangent2)) * 180 / c_PI;
			cp.EdgeType = 0;
			while (angle < 0)angle += 360;
			if (angle < 180 - creaseangle)cp.EdgeType = 1;
			if (angle > 180.0 + 50.0)cp.EdgeType = -1;
			cp.Angle = angle;
			if (derived)cp.DerivedEdgeLength = (cp.pos.distance(contour[cp.PrevPoint].pos) + cp.pos.distance(contour[cp.NextPoint].pos)) / 2.0;
		}
	}
	MarkAsOld();
}

void ContourQuadrangulator::DbgDrawContour(cList<qContourPoint> & contour, const char* layer, bool directions, bool points, bool ids) {
#ifdef coqu_debug
	DbgLayer(layer);
	for (int i = 0; i < contour.Count(); i++) {
		qContourPoint& cp = contour[i];
		assert(cp.NextPoint == -1 || cp.NextPoint < contour.Count());
		assert(cp.PrevPoint == -1 || cp.PrevPoint < contour.Count());
		if (cp.NextPoint != -1 && cp.PrevPoint != -1) {
			comms::dVec3 pp = contour[cp.PrevPoint].pos;
			comms::dVec3 np = contour[cp.NextPoint].pos;
			AddDbgLine((pp + cp.pos) / 2, cp.pos, 0xFF0000FF, 0xFF0000FF);
			AddDbgLine((np + cp.pos) / 2, cp.pos, 0xFFFF0000, 0xFFFF0000);
			if (cp.ParentVertex != -1) {
				AddDbgLine(cp.pos, contour[cp.ParentVertex].pos, 0xFF80FF8080, 0xFF8080FF);
			}
			if (points) {
				DWORD CL = 0xFF00FF00;
				if (cp.EdgeType == 1)CL = 0xFFFFFFFF;
				if (cp.EdgeType == -1)CL = 0xFFFFFF00;
				AddDbgPoint(cp.pos, CL);
				char cc[8];
				sprintf(cc, "%d", i);
				if (ids)AddDbgText(cp.pos, 0xFFFFFFFF, cc);
			}
			if (directions) {
				double L1 = pp.distance(cp.pos) / 3.0;
				double L2 = np.distance(cp.pos) / 3.0;
				comms::dVec3 pc1 = (pp + cp.pos) / 2.0;
				comms::dVec3 pc2 = (np + cp.pos) / 2.0;
				AddDbgLine(pc1, pc1 + L1 * cp.front1, 0xFF808080, 0xFF808080);
				AddDbgLine(pc2, pc2 + L2 * cp.front2, 0xFF808080, 0xFF808080);
			}
		}
		else {
			assert(cp.NextPoint == -1);
			assert(cp.PrevPoint == -1);
		}
	}
#endif //coqu_debug
}
void ContourQuadrangulator::Quadrangulate(comms::cMeshContainer * res) {
#ifdef coqu_debug
	void ClearDbgLayers();
	ClearDbgLayers();
#endif
	int pass = 1;
	static int numericpass = -1;
	static int skippass = -1;
	double crease = 50;
	bool contourfound = false;
	std::mutex aq;
	cList<int> starts;
	double InitialSquare = 0;
	if (mc) {
		for (int i = 0; i < mc->GetRaw().Count(); i++) {
			int n = mc->GetRaw()[i][0];
			if (n == 3) {
				comms::dVec3 vs[3];
				for (int j = 0; j < 3; j++) {
					vs[j] = mc->GetPosition(mc->GetRaw()[i + j + 1][0]);
				}
				InitialSquare += comms::dVec3::Cross(vs[2] - vs[0], vs[1] - vs[0]).Length() / 2.0;
			}
			i += n;
		}
	}
	InitialSquare *= 1.1;
	double CurrentSquare = 0;
	do {
		starts.Clear();
		DWORD CL = GetRandomColor();
		cStr s = "layer";
		s += cStr::ToString(pass++);
		double minw = DBL_MAX;
		cList<qContourPoint> bestc;
		cList<int> bestf;
		contourfound = false;
		for (int i = 0; i < Contour.Count(); i++) {
			if (Contour[i].NextPoint != -1) {
				contourfound = true;
				if (Contour[i].EdgeType) {
					int qs = ContourSize(i);
					if (qs > 4) {
						starts.Add(i);
					}
					else {
						bestf.Clear();
						bestc.Clear();
						bestf.Add(qs);
						minw = 0;
						int start = i;
						for (int k = 0; k < qs; k++) {
							bestf.Add(start);
							int next = Contour[start].NextPoint;
							Contour[start].NextPoint = Contour[start].PrevPoint = -1;
							if (next == i)break;
							start = next;
						}
						starts.Clear();
						break;
					}
				}
			}
		}
		if (starts.Count()) {
			_std_for(p, starts.Count()) {
				int i = starts[p];
				cList<qContourPoint> temp;
				cList<int> tempf;
				double v = TryToExpandChunk(i, temp, tempf);
				std_scoped_lock lk(aq);
				if (v < minw) {
					lk.release();
					if (CheckFlips(temp, tempf)) {
						if (!CheckSelfIntersections(temp)) {
							if (v < minw) {
								std_scoped_lock lk(aq);
								minw = v;
								bestc = temp;
								bestf = tempf;
							}
						}
					}
				}
			}_std_for_end;
		}
		if (minw < 10000000) {
			crease = 50;
			if (bestc.Count())Contour = bestc;
			Faces.AddRange(bestf);
			UpdateDirections(Contour);
			AddVnv(bestf);
			CreateDistField();
			//RelaxVnv(1,0.2,true);
#ifdef coqu_debug
			DbgViewVnv(s, pass == numericpass);
#endif //coqu_debug
			//DbgDrawContour(Contour, s, false, false, false);
		}
		else {
			crease *= 0.9;
			UpdateDirections(Contour, false, crease);
			//if (crease < 25) {
			//	crease = 50;
			//	RelaxVnv(1);
			//}
		}
		CurrentSquare = 0;
		for (int i = 0; i < Faces.Count(); i++) {
			int n = Faces[i];
			comms::dVec3 p[4];
			if (n <= 4) {
				for (int k = 0; k < n; k++) {
					p[k] = Contour[Faces[i + k + 1]].pos;
				}
				if (n == 4) {
					CurrentSquare += comms::dVec3::Cross(p[1] - p[0], p[2] - p[0]).Length() / 2.0;
					CurrentSquare += comms::dVec3::Cross(p[3] - p[0], p[2] - p[0]).Length() / 2.0;
				}
				if (n == 3) {
					CurrentSquare += comms::dVec3::Cross(p[1] - p[0], p[2] - p[0]).Length() / 2.0;
				}
			}
			i += n;
		}
		if (InitialSquare > 0) {
			if (CurrentSquare > InitialSquare)break;
		}
	} while (contourfound && pass < 5000);
	if (res) {
		RelaxVnv(20);
		res->Clear();
		res->SetDefaultObjMtl();
		for (int i = 0; i < Faces.Count(); i++) {
			int n = Faces[i];
			cVec3i nn(n, 0, 0);
			res->GetRaw().Add(nn);
			for (int j = 0; j < n; j++) {
				cVec3i nn(Faces[i + j + 1], -1, -1);
				res->GetRaw().Add(nn);
			}
			i += n;
		}
		for (int i = 0; i < Contour.Count(); i++) {
			res->GetPositions().Add(Contour[i].pos);
		}
		//res->Weld(0.01);
		//res->RemoveUnusedVerts();
		//res->Smooth(1.0, true, 4, -1, 0, true);
		//res->ImproveQuadsTopology();
	}

}
void ContourQuadrangulator::AddVnv(cList<int>& newfaces) {
	for (int i = 0; i < newfaces.Count(); i++) {
		int n = newfaces[i];
		for (int j = 0; j < n; j++) {
			int v1 = newfaces[i + j + 1];
			int v2 = newfaces[i + (j + 1) % n + 1];
			vnv.add_uniq(v1, v2);
			vnv.add_uniq(v2, v1);
		}
		i+=n;
	}
}
void ContourQuadrangulator::CreateDistField() {
	for (int i = 0; i < Contour.Count(); i++) {
		qContourPoint& cp = Contour[i];
		if (cp.IsInitial)cp.avdist = 0;
		else cp.avdist = -1;
	}
	bool ch;
	do {
		ch = false;
		scan(vnv, int* p1, int* p2) {
			qContourPoint& c1 = Contour[*p1];
			qContourPoint& c2 = Contour[*p2];
			if (c1.avdist != -1) {
				int dd = c1.avdist + 1;
				if (c2.avdist == -1 || c2.avdist > dd) {
					c2.avdist = dd;
					ch = true;
				}
			}
			if (c2.avdist != -1) {
				int dd = c2.avdist + 1;
				if (c1.avdist == -1 || c1.avdist > dd) {
					c1.avdist = dd;
					ch = true;
				}
			}
		}scan_end;
	} while (ch);
}
void ContourQuadrangulator::RelaxVnv(int ntimes, double degree, bool DistanceDependent) {
	cList<comms::dVec3> pos1;
	pos1.Add(comms::dVec3::Zero, Contour.Count());
	for (int k = 0; k < ntimes; k++) {
		std_for(i, Contour.Count()) {
			comms::dVec3 p = Contour[i].pos;
			double w = 1.0;
			if (!Contour[i].IsInitial) {
				scan_key(vnv, i, int* pv) {
					p += Contour[*pv].pos;
					w++;
				}scan_end;
			}
			pos1[i] = p / w;
		}std_for_end;
		std_for(i, pos1.Count()) {
			double deg = degree;
			if (DistanceDependent) {
				double d = Contour[i].avdist - 1;
				if (d > 6.0)d = 6.0;
				if (d < 0)d = 0;
				deg *= d / 6.0;
			}
			Contour[i].pos = pos1[i] * deg + Contour[i].pos * (1.0 - deg);
			SnapToMesh(Contour[i].pos, Contour[i].Normal);
		}std_for_end;
	}
}
void ContourQuadrangulator::DbgViewVnv(const char* name, bool indices) {
#ifdef coqu_debug
	DbgLayer(name);
	scan(vnv, int* v1, int* v2) {
		AddDbgLine(Contour[*v1].pos, Contour[*v2].pos, 0xFFFFFFFF, 0xFFFFFFFF);
		if (indices) {
			char cc[8];
			sprintf(cc, "%d", *v1);
			AddDbgText(Contour[*v1].pos, 0xFFFFFFFF, cc);
			sprintf(cc, "%d", *v2);
			AddDbgText(Contour[*v2].pos, 0xFFFFFFFF, cc);
		}
		char cc[8];
		DbgModifier(1, 0, 0);
		
		AddDbgLine(Contour[*v1].pos, Contour[*v2].pos, 0xFFFFFFFF, 0xFFFFFFFF);
		sprintf(cc, "%d", *v1);
		AddDbgText(Contour[*v1].pos, 0xFFFFFFFF, cc);
		sprintf(cc, "%d", *v2);
		AddDbgText(Contour[*v2].pos, 0xFFFFFFFF, cc);
		
		DbgModifier(0, 0, 0);
		if (Contour[*v1].NextPoint != -1) {
			DbgModifier(1, 0, 0);
			//sprintf(cc, "%d", int(Contour[*v1].Angle));
			//AddDbgText(Contour[*v1].pos, 0xFFFFFFFF, cc);
			//sprintf(cc, "%d", int(Contour[*v2].Angle));
			//AddDbgText(Contour[*v2].pos, 0xFFFFFFFF, cc);
			DbgModifier(0, 0, 1);
			if (Contour[*v1].EdgeType) {
				cc[1] = 0;
				if (Contour[*v1].EdgeType == 1)cc[0] = '+';
				if (Contour[*v1].EdgeType == -1)cc[0] = '-';
				AddDbgText(Contour[*v1].pos, 0xFFFFFFFF, cc);
			}
			DbgModifier(0, 0, 0);
		}
	}scan_end;
	for (int i = 0; i < Faces.Count(); i++) {
		int n = Faces[i];
		if (n == 4) {
			comms::dVec3 v1 = Contour[Faces[i + 1]].pos;
			comms::dVec3 v2 = Contour[Faces[i + 2]].pos;
			comms::dVec3 v3 = Contour[Faces[i + 3]].pos;
			comms::dVec3 v4 = Contour[Faces[i + 4]].pos;
			AddDbgTri(v1, v2, v3, 0x20FFFF00);
			AddDbgTri(v1, v3, v4, 0x20FFFF00);
		}
		i += n;
	}
#endif //coqu_debug
}
int ContourQuadrangulator::ContourSize(int start) {
	int n = 1;
	int s = start;
	do {
		int cp = Contour[s].NextPoint;
		if (cp == -1 || cp == start)break;
		n++;
		s = cp;
	} while (true);
	return n;
}
bool ContourQuadrangulator::CheckFlips(cList<qContourPoint> & contour, cList<int> & newfaces) {
	for (int i = 0; i < newfaces.Count(); i++) {
		int n = newfaces[i];
		int nn = 0;
		for (int k = 0; k < n; k++) {
			int vp = newfaces[i + (k + 3) % n + 1];
			int vc = newfaces[i + k + 1];
			int vn = newfaces[i + (k + 1) % n + 1];
			comms::dVec3 tn = comms::dVec3::Cross(contour[vn].pos - contour[vc].pos, contour[vp].pos - contour[vc].pos).ToNormal();
			if (tn.dot(contour[vc].Normal) < 0)nn++;
		}
		if (nn > 1)return false;
		i += n;
	}
	return true;
}
bool ContourQuadrangulator::CheckSelfIntersections(cList<qContourPoint> & contour)
{
	const double percent = 1.0 / 10.0;
	for (int i = 0; i < contour.Count(); i++) {
		int npt = contour[i].NextPoint;
		if (npt != -1) {
			qContourPoint* p2 = &contour[npt];
			qContourPoint* p1 = &contour[i];
			//if (p2->IsNew || p1->IsNew) {
			qContourIntersector is(p1->pos, p2->pos);
			int c = p2->NextPoint;
			if (c != -1) {
				qContourPoint* pc = &contour[c];
				if (c != i) {
					for (int j = 0; j < 10000; j++) {
						pc = &contour[c];
						int n = pc->NextPoint;
						if (n == i || n == -1)break;
						qContourPoint * pn = &contour[n];
						if (is.IntersectsWith(pn->pos, pc->pos, percent)) {
							return true;
						}
						if (pc->ParentVertex != -1) {
							qContourPoint* pp = &contour[pc->ParentVertex];
							if (is.IntersectsWith(pp->pos, pc->pos, percent)) {
								return true;
							}
						}
						c = n;
					}
				}
			}
			//}
		}
	}
	return false;
}
void ContourQuadrangulator::MarkAsOld() {
	for (int i = 0; i < Contour.Count(); i++) {
		Contour[i].IsNew = false;
	}
}
float RayTri(const Vector3D& RayOrig1, const Vector3D& RayDir, const Vector3D & t0, const Vector3D & t1, const Vector3D & t2, float& u, float& v);
void ContourQuadrangulator::SnapToMesh(comms::dVec3 & pos, comms::dVec3 & normal)
{
	comms::cSeg s;
	s.SetFromRay(pos, normal);
	StackArray<int> res;
	mcPicker.Pick(s, res, 0);
	Vector3D bestp = pos;
	Vector3D bestn = normal;
	float minds = FLT_MAX;
	if (mc && res.Count()) {
		Vector3D pt = s.GetFm();
		Vector3D nrm = s.GetNormal();
		auto& raw = mc->GetRaw();
		auto& pos = mc->GetPositions();
		for (int p = 0; p < res.Count(); p++) {
			int i = res[p];
			int n = raw[i][0];
			if (n == 3) {
				Vector3D p1 = pos[raw[i + 1][0]];
				Vector3D p2 = pos[raw[i + 2][0]];
				Vector3D p3 = pos[raw[i + 3][0]];
				float u, v;
				float dst = __abs(RayTri(pt, nrm, p1, p2, p3, u, v));
				if (dst < minds) {
					minds = dst;
					bestp = p1 + (p2 - p1) * u + (p3 - p1) * v;
				}
			}
		}
	}
	if (minds < FLT_MAX) {
		pos = bestp;
		normal = bestn;
		normal.Normalize();
	}
	else {
#ifdef coqu_debug
		DbgLayer("miss");
		AddDbgLine(pos - normal * 20, pos + normal * 20, 0xFFFF0000, 0xFF0000FF);
		//assert(0);
#endif //coqu_debug
	}
}
void ContourQuadrangulator::selfcheck(cList<qContourPoint> & contour)
{
	for (int i = 0; i < contour.Count(); i++) {
		qContourPoint* cp = &contour[i];
		assert((cp->NextPoint == -1 && cp->PrevPoint == -1) || (cp->NextPoint != -1 && cp->PrevPoint != -1));
	}
}
void TestQuad(comms::cMeshContainer * mc) {
	ContourQuadrangulator cq;
	cq.CreateFromMesh(mc);
	//cq.DbgDrawContour(cq.Contour,"contour",true,true);
	cq.Quadrangulate(NULL);
	cq.RelaxVnv(20);
	cq.DbgViewVnv();
}

qContourIntersector::qContourIntersector(const comms::dVec3 & pt1, const comms::dVec3 & pt2) {
	Prepare(pt1, pt2);
}

void qContourIntersector::Prepare(const comms::dVec3 & pt1, const comms::dVec3 & pt2)
{
	v1 = pt1;
	v2 = pt2;
	c = (v1 + v2) / 2.0;
	len12 = v1.distance(v2);
	L1.SetFromEnds(v1, v2);
}

bool qContourIntersector::IntersectsWith(const comms::dVec3 & pt1, const comms::dVec3 & pt2, double distpercent)
{
	const double _eps = 1e-12;
	comms::dVec3 c3 = (pt1 + pt2) / 2.0;
	double dis3 = pt1.distance(pt2) / 2.0;
	if (c.distance(c3) < len12 + dis3) {
		double L = std::min(len12, dis3) * distpercent;
		comms::dSeg L2;
		L2.SetFromEnds(pt1, pt2);
		comms::dVec3 p1, p2;
		L2.ClosestPoints(comms::dSeg::SegSeg, L1, L2, p1, p2);
		double d = p1.distance(p2);
		if (d < L) {
			if (p1.distanceSq(v1) > _eps && p1.distanceSq(v2) > _eps && p2.distanceSq(pt1) > _eps && p2.distanceSq(pt2) > _eps) {
				return true;
			}
		}
	}
	return false;
}
