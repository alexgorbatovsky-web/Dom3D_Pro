#include "stdafx.h"
//namespace comms 
//{
#include "../3D-Coat/MPC/Primitive/mpc.h"
#include "../3D-Coat/MPC/Composite/GM_CompositeFactory.h"
#include "../3D-Coat/MPC/Composer/GM_Director.h"
#include "../3D-Coat/MPC/Primitive/GM_Tools.h"
//}
#include "../ClassEngine/stackarray.h"
#include "../3D-Coat/MC/dVector.h"
#include "../3D-Coat/MC/iVector.h"
#include "../3D-Coat/MC/MeshOperations.h"
#include "MeshCutter.h"
#include "MeshDivider.h"
#include "../3D-Coat/Exception.h"
#include "../3D-Coat/TextTool.h"
#include <iomanip>
#include <sstream>
#include <iostream>

#include "FillContour.h"
#include "../3D-Coat/IGES/BezierCurve.h"
#include "MC/TriangularPatch.h"
#include "MC/normals2geometry.h"

void BM2MC(BasicMesh& bm,comms::cMeshContainer& mc,bool _swap,float scale);
void MC2BM(comms::cMeshContainer& mc, BasicMesh& bm, bool SkipWeld, float Scale);
DWORD GetRandomColor();
void IntersectionTwoPoligons(cList<cVec3>& pol1, cList<cVec3>& pol2, cList<cVec3>& pts);
void Step(char* text);
bool crossing_line_triangle(CVertex3d* p0, CVector3d* v, CVertex3d* p1, CVertex3d* p2, CVertex3d* p3, CVertex3d* pc);

namespace comms{

	cMeshContainer::MeshOpHook* cMeshContainer::ImportHook = NULL;
	cMeshContainer::MeshOpHook* cMeshContainer::ExportHook = NULL;

//-----------------------------------------------------------------------------
// cMeshContainer::ctor
//-----------------------------------------------------------------------------
cMeshContainer::cMeshContainer(){
}


//-----------------------------------------------------------------------------
// cMeshContainer::ctor
//-----------------------------------------------------------------------------
cMeshContainer::cMeshContainer( const cMeshContainer& src )
{
	Copy( src );
}


//-----------------------------------------------------------------------------
// cMeshContainer.ctor
//-----------------------------------------------------------------------------
cMeshContainer::cMeshContainer(
	const figures_t&  figures,
	float             fusionDistance
)
{
	Insert( figures, fusionDistance );
}


//-----------------------------------------------------------------------------
// cMeshContainer::operator=
//-----------------------------------------------------------------------------
cMeshContainer& cMeshContainer::operator=( const cMeshContainer& src ) {
	if (this != &src) {
		Copy( src );
	}
	return *this;
}


//-----------------------------------------------------------------------------
// cMeshContainer::operator==
//-----------------------------------------------------------------------------
bool cMeshContainer::operator==( const cMeshContainer& b ) const {
    return (m_Raw == b.m_Raw) &&
        (m_Positions == b.m_Positions) &&
        (m_Name == b.m_Name) &&
        (m_TexCoords == b.m_TexCoords) &&
        (m_Normals == b.m_Normals);
        // @todo (m_Tangents == b.m_Tangents) &&
        // @todo (m_Materials == b.m_Materials) &&
        // @todo (m_Objects == b.m_Objects);
}


//-----------------------------------------------------------------------------
// cMeshContainer::IsValid
//-----------------------------------------------------------------------------
bool cMeshContainer::IsValid(const bool ShowWarning) const {
	if(ShowWarning) {
		cLog::Message("Validating mesh \"%s\"...", GetName().ToCharPtr());
	}
	int i;
	// Positions:
	cBounds Inf(-cVec3::Infinity, cVec3::Infinity);
	for(i = 0; i < m_Positions.Count(); i++) {
		const cVec3 &u = m_Positions[i];
		// W/o NaN?
		if(!u.IsValid()) {
			if(ShowWarning) {
				cLog::Warning("Position at index %d = { %s } has NaN.", i, u.ToString().ToCharPtr());
			}
			return false;
		}
		// Within (-Infinity, +Infinity)?
		if(!Inf.ContainsPoint(u)) {
			if(ShowWarning) {
				cLog::Warning("Position at index %d = { %s } is out of infinity.", i, u.ToString().ToCharPtr());
			}
			return false;
		}
	}
	// TexCoords:
	for(i = 0; i < GetTexCoords().Count(); i++) {
		const cVec2 &t = GetTexCoords()[i];
		// W/o NaN?
		if(!t.IsValid()) {
			if(ShowWarning) {
				cLog::Warning("Texture coordinate at index %d = { %s } has NaN.", i, t.ToString().ToCharPtr());
			}
			return false;
		}
		// Within (-Infinity, +Infinity)?
		if(!Inf.ContainsPoint(t)) {
			if(ShowWarning) {
				cLog::Warning("Texture coordinate at index %d = { %s } is out of infinity.", i, t.ToString().ToCharPtr());
			}
			return false;
		}
	}
	//-------------------------------------------------------------------------
	// Normals && Tangents:
	//-------------------------------------------------------------------------
	// Count of normals equals count of tangents (if both are filled)?
	/*if(!GetNormals().IsEmpty() && !GetTangents().IsEmpty() && GetNormals().Count() != GetTangents().Count()) {
		if(ShowWarning) {
			cLog::Warning("Number of normals = %d is not equal number of tangents = %d.", GetNormals().Count(), GetTangents().Count());
		}
		return false;
	}*/
	// Normals:
	for(i = 0; i < GetNormals().Count(); i++) {
		const cVec3 &N = GetNormals()[i];
		// W/o NaN?
		if(!N.IsValid()) {
			if(ShowWarning) {
				cLog::Warning("Normal at index %d = { %s } has NaN.", i, N.ToString().ToCharPtr());
			}
			return false;
		}
		// Within (-Infinity, +Infinity)?
		if(!Inf.ContainsPoint(N)) {
			if(ShowWarning) {
				cLog::Warning("Normal at index %d = { %s } is out of infinity.", i, N.ToString().ToCharPtr());
			}
			return false;
		}
	}
	// Tangents:
	for(i = 0; i < GetTangents().Count(); i++) {
		const cVec<cVec3, 2> &Tangents = GetTangents()[i];
		// W/o NaN?
		if(!Tangents[0].IsValid()) {
			if(ShowWarning) {
				cLog::Warning("Tangent at index %d = { %s } has NaN.", i, Tangents[0].ToString().ToCharPtr());
			}
			return false;
		}
		if(!Tangents[1].IsValid()) {
			if(ShowWarning) {
				cLog::Warning("BiTangent at index %d = { %s } has NaN.", i, Tangents[1].ToString().ToCharPtr());
			}
			return false;
		}
		// Within (-Infinity, +Infinity)?
		if(!Inf.ContainsPoint(Tangents[0])) {
			if(ShowWarning) {
				cLog::Warning("Tangent at index %d = { %s } is out of infinity.", i, Tangents[0].ToString().ToCharPtr());
			}
			return false;
		}
		if(!Inf.ContainsPoint(Tangents[1])) {
			if(ShowWarning) {
				cLog::Warning("BiTangent at index %d = { %s } is out of infinity.", i, Tangents[1].ToString().ToCharPtr());
			}
			return false;
		}
	}
	//-------------------------------------------------------------------------
	// Raw:
	//-------------------------------------------------------------------------
	if(!IsValidRawSequence()) {
		if(ShowWarning) {
			cLog::Warning("The same vertex of polygon used multiple times.");
		}
		return false;
	}
	int nPoly = 0;
	int index = 0;
	while(index < GetRaw().Count()) {
		const cVec3i &Info = GetRaw()[index];
		const int Deg = Info[0];
		const int idMtl = Info[1];
		// Deg >= 3?
		if(Deg < 3) {
			if(ShowWarning) {
				cLog::Warning("Polygon %d = { %s } has degree less than 3.", nPoly, cStr::ToString(Info.ToPtr(), Info.GetDimension()).ToCharPtr());
			}
			return false;
		}
		// idMtl is valid?
		if(idMtl != 0 && (idMtl < 0 || idMtl >= GetMaterials().Count())) {
			if(ShowWarning) {
				cLog::Warning("Polygon %d = { %s } has material index out of available %d.", nPoly, cStr::ToString(Info.ToPtr(), 3).ToCharPtr(), GetMaterials().Count());
			}
			return false;
		}
		// Indices withing arrays:
		for(int k = 0; k < Deg; k++) {
			const int s = index + 1 + k;
			// Face has declared number of indices?
			if(s >= GetRaw().Count()) {
				if(ShowWarning) {
					cLog::Warning("Polygon %d has less than declared number of indices = %d.", nPoly, Info[0]);
				}
				return false;
			}
			const cVec3i &i0 = GetRaw()[s];
			// Is position index valid?
			if(i0[0] < 0 || i0[0] >= GetPositions().Count()) {
				if(ShowWarning) {
					cLog::Warning("Polygon %d has position index %d that is out of available %d.", nPoly, i0[0], GetPositions().Count());
				}
				return false;
			}
			// Is texcoord index valid?
			if(i0[1] < 0 || i0[1] >= GetTexCoords().Count()) {
				// May be zero, if texcoords is empty.
				if(!GetTexCoords().IsEmpty() || i0[1] >= 0) {
					if(ShowWarning) {
						cLog::Warning("Polygon %d has texture coordinate index %d that is out of available %d.", nPoly, i0[1], GetTexCoords().Count());
					}
					return false;
				}
			}
			// Is normal index valid?
			const int NormalsCount = cMath::Max(GetNormals().Count(), GetTangents().Count());
			if(i0[2] < 0 || i0[2] >= NormalsCount) {
				// May be zero, if normals is empty.
				if(NormalsCount != 0 || i0[2] >= 0) {
					if(ShowWarning) {
						cLog::Warning("Polygon %d has normal index %d that is out of available %d.", nPoly, i0[2], NormalsCount);
					}
					return false;
				}
			}
		}
		index += Deg + 1;
		nPoly++;
	}

	return true;
} // cMeshContainer::IsValid

bool cMeshContainer::CorrectRawQuadSelfIntersection() {
	uni_hash<int, quad_int> hashIds;
	for (int i = 0, k = 0; i < m_Raw.Count(); ++i) {
		int  n = m_Raw[i][0];
		// skip points
		if (n > 1 && n < QuadSh + 1) {
			quad_int Q(-1, -1, -1, -1);
			for (int j = 0; j < n; ++j) {
				const int  iRaw = i + j + 1;
				const int  ip = m_Raw[iRaw][0];
				if (j == 0)		 Q.V1 = ip;
				else if (j == 1) Q.V2 = ip;
				else if (j == 2) Q.V3 = ip;
				else		     Q.V4 = ip;
			} // for (int j = 0; j < n; ++j)
			int* ind = hashIds.get(Q);
			if (ind) {
				int iRaw = *ind;
				m_Raw.RemoveAt(i, n + 1);
				i -= n;
			}
			else {
				hashIds.add(Q, k);
			}
			k++;
		} // if (n > 1)
		i += n;
	}
	return true;
}

bool cMeshContainer::CheckQuadSelfIntersection(cList<PIndex>& polyIntersec) {
	uni_hash<int, quad_int> hashIds;
	for (int i = 0, k = 0; i < m_Raw.Count(); ++i) {
		const int  n = m_Raw[i][0];
		// skip points
		if (n > 1 && n < QuadSh + 1) {
			quad_int Q(-1, -1, -1, -1);
			for (int j = 0; j < n; ++j) {
				const int  iRaw = i + j + 1;
				const int  ip = m_Raw[iRaw][0];
				if (j == 0)	   Q.V1 = ip;
				else if (j == 1) Q.V2 = ip;
				else if (j == 2) Q.V3 = ip;
				else		   Q.V4 = ip;
			} // for (int j = 0; j < n; ++j)
			int* ind = hashIds.get(Q);
			if (ind) {
				int iRaw = *ind;
				int j = 0;
				bool noExist = true;
				while (j < polyIntersec.Count()) {
					if (polyIntersec[j].idx == iRaw) {
						polyIntersec[j].childs.Add(k);
						noExist = false;
						break;
					}
					j++;
				}
				if (noExist) {
					PIndex pidx;
					pidx.idx = iRaw;
					polyIntersec.Add(pidx);
				}
			}
			else {
				hashIds.add(Q, k);
			}
			k++;
		} // if (n > 1)
		i += n;
	}
	return polyIntersec.Count() > 0;
}

//-----------------------------------------------------------------------------
// cMeshContainer::IsValidRawSequence
//-----------------------------------------------------------------------------
bool cMeshContainer::IsValidRawSequence() const {
	for (int i = 0;  i < m_Raw.Count();  ++i) {
		const int  n = m_Raw[ i ][ 0 ];
		// skip points
		if (n > 1) {
			UnlimitedBitset  has;
			for (int j = 0; j < n; ++j) {
				const int  iRaw = i + j + 1;
				const int  ip = m_Raw[ iRaw ][ 0 ];
				if ( has.get( ip ) ) {
					return false;
				}
				has.set( ip, true );
			} // for (int j = 0; j < n; ++j)
		} // if (n > 1)
		i += n;
	} // for (int i = 0;  i < m_Raw.Count();  ++i)

	return true;
} // cMeshContainer::IsValidRawSequence


//-----------------------------------------------------------------------------
// cMeshContainer::Clear
//-----------------------------------------------------------------------------
void cMeshContainer::Clear() {
	m_Positions.Clear();
	m_TexCoords.Clear();
	m_Normals.Clear();
	m_Tangents.Clear();
	m_Raw.Clear();
	m_Name.Clear();
	m_Materials.Clear();
	m_Objects.Clear();
} // cMeshContainer::Clear

//-----------------------------------------------------------------------------
// cMeshContainer::GetPosition
//-----------------------------------------------------------------------------
const cVec3 & cMeshContainer::GetPosition( int vertex ) const {
	if ( (vertex < 0) || (vertex >= m_Positions.Count()) ) {
		throw comms::Exception( "The 'vertex' is out of mesh for position." );
	}
	return m_Positions[ vertex ];
} // cMeshContainer::GetPosition

//-----------------------------------------------------------------------------
// cMeshContainer::SetPosition
//-----------------------------------------------------------------------------
void cMeshContainer::SetPosition( int vertex, const cVec3& c ) {
	if ( (vertex < 0) || (vertex >= m_Positions.Count()) ) {
		throw ::std::runtime_error( "The 'vertex' is out of mesh for position." );
	}
	m_Positions[ vertex ] = c;
} // cMeshContainer::SetPosition

//-----------------------------------------------------------------------------
// cMeshContainer::GetVertex
//-----------------------------------------------------------------------------
int cMeshContainer::GetVertex( const cVec3& c ) const {
	for (int i = 0; i < m_Positions.Count(); ++i) {
		const cVec3&  p = m_Positions[ i ];
		if (p == c) { return i; }
	}
	return -1;
} // cMeshContainer::GetVertex

//-----------------------------------------------------------------------------
// cMeshContainer::GetNormal
//-----------------------------------------------------------------------------
const cVec3 & cMeshContainer::GetNormal( int vertex ) const {
	if ( (vertex < 0) || (vertex >= m_Normals.Count()) ) {
		throw comms::Exception( "The 'vertex' is out of mesh for normal." );
	}
	return m_Normals[ vertex ];
} // cMeshContainer::GetNormal

//-----------------------------------------------------------------------------
// cMeshContainer::SetNormal
//-----------------------------------------------------------------------------
void cMeshContainer::SetNormal( int vertex, const cVec3& c ) {
	if ( (vertex < 0) || (vertex >= m_Positions.Count()) ) {
		throw ::std::runtime_error( "The 'vertex' is out of mesh for position." );
	}
	m_Normals[ vertex ] = c;
} // cMeshContainer::SetNormal

//-----------------------------------------------------------------------------
// cMeshContainer::GetPolyCount
//-----------------------------------------------------------------------------
int cMeshContainer::GetPolyCount() const {
	int Count = 0;
	int index = 0;
	while(index < m_Raw.Count()) {
		index += m_Raw[index][0] + 1;
		Count++;
	}
	return Count;
} // cMeshContainer::GetPolyCount

//-----------------------------------------------------------------------------
// cMeshContainer::GetTrisCount
//-----------------------------------------------------------------------------
int cMeshContainer::GetTrisCount() const {
	int Count = 0;
	int index = 0;
	while(index < m_Raw.Count()) {
		int nn=m_Raw[index][0];
		index += nn + 1;
		if(nn>2){
			Count+=nn-2;
		}
	}
	return Count;
} // cMeshContainer::GetTrisCount

//-----------------------------------------------------------------------------
// cMeshContainer::GetNeighboursForFace
//-----------------------------------------------------------------------------
cList< int >  cMeshContainer::GetNeighboursForFace(
	int     beginRawFigure,
	cVec3*  avgNormal,
	cVec3*  avgCenter
) const {
	cList< int >  r;
	const int  i = beginRawFigure;
	const int  n = m_Raw[ i ][ 0 ];
	UnlimitedBitset  harvested;
	for (int j = 0; j < n; ++j) {
		const int  iRaw = i + j + 1;
		const int  ip = m_Raw[ iRaw ][ 0 ];
		const cList< int >  ftr = Find( ip );
		for (int k = 0; k < ftr.Count(); ++k) {
			const int  brb = ftr[ k ];
			if ( !harvested.get( brb ) ) {
				r.Add( brb );
				harvested.set( brb, true );
			}
		} // for (int k = 0; k < ftr.Count(); ++k)
	} // for (int j = 0; j < n; ++j)

	if ( avgNormal ) {
		*avgNormal = cVec3::Zero;
		if ( avgCenter ) { *avgCenter = cVec3::Zero; }
		for (int k = 0; k < r.Count(); ++k) {
			*avgNormal += GetFaceNormal( r[ k ] );
			if ( avgCenter ) { *avgCenter += GetFaceCoord( r[ k ] ); }
		}
		avgNormal->Normalize();
		if ( avgCenter ) { *avgCenter /= r.Count(); }
	}

	return r;
} // cMeshContainer::GetNeighboursForFace

//-----------------------------------------------------------------------------
// cMeshContainer::Contains( node by position )
//-----------------------------------------------------------------------------
bool cMeshContainer::Contains( const cVec3& a ) const {
	return !Find( a ).IsEmpty();
} // cMeshContainer::Contains( node by position )

//-----------------------------------------------------------------------------
// cMeshContainer::Contains( node by index )
//-----------------------------------------------------------------------------
bool cMeshContainer::Contains( const int ai ) const {
	return !Find( ai ).IsEmpty();
} // cMeshContainer::Contains( node by index )

//-----------------------------------------------------------------------------
// cMeshContainer::Contains( edge by position )
//-----------------------------------------------------------------------------
bool cMeshContainer::Contains( const cVec3& a, const cVec3& b ) const {
	return !Find( a, b ).IsEmpty();
} // cMeshContainer::Contains( edge by position )

//-----------------------------------------------------------------------------
// cMeshContainer::Contains( edge by index )
//-----------------------------------------------------------------------------
bool cMeshContainer::Contains( int ai, int bi ) const {
	return !Find( ai, bi ).IsEmpty();
} // cMeshContainer::Contains( edge by index )

//-----------------------------------------------------------------------------
// cMeshContainer::Contains( figure by positions )
//-----------------------------------------------------------------------------
bool cMeshContainer::Contains( const VecArray& figure ) const {
	return (Find( figure ) != -1);
} // cMeshContainer::Contains( figure by positions )

//-----------------------------------------------------------------------------
// cMeshContainer::Contains( figure by indices )
//-----------------------------------------------------------------------------
bool cMeshContainer::Contains( const cList< int >& figure ) const {
	return (Find( figure ) != -1);
} // cMeshContainer::Contains( figure by indices )

//-----------------------------------------------------------------------------
// cMeshContainer::IsTriangulated
//-----------------------------------------------------------------------------
bool cMeshContainer::IsTriangulated() const {
	int index = 0;
	while(index < m_Raw.Count()) {
		const int D = m_Raw[index][0];
		if(D != 3) {
			return false;
		}
		index += D + 1;
	}
	return true;
} // cMeshContainer::IsTriangulated

//-----------------------------------------------------------------------------
// cMeshContainer::Triangulate
//-----------------------------------------------------------------------------
void cMeshContainer::Triangulate(RawArray &TriRaw) const {
	TriRaw.Clear();
	int index = 0;
	cMat3 screwmat = cMat3::Scaling(0.99, 0.98, 0.97);
	
	auto screw_dist = [&](const cVec3& p1, const cVec3& p2) {
		auto diff = p2 - p1;
		diff *= screwmat;
		return diff.Length();
	};
	while (index < m_Raw.Count()) {
		int nDeg = m_Raw[index][0];
		const int idMtl = m_Raw[index][1];
		const int idOb = m_Raw[index][2];
		if (nDeg < 3) {
			// Skip lines and points.
			index += nDeg + 1;
		}
		else {
			if (nDeg == 4) {
				cVec3i v0 = m_Raw[index + 1];
				cVec3i v1 = m_Raw[index + 2];
				cVec3i v2 = m_Raw[index + 3];
				cVec3i v3 = m_Raw[index + 4];
				float L0 = screw_dist(m_Positions[v0[0]], m_Positions[v2[0]]);
				float L1 = screw_dist(m_Positions[v1[0]], m_Positions[v3[0]]);
				if (L0 < L1 * 1.000001) {
					TriRaw.Add(cVec3i(3, idMtl, idOb));
					TriRaw.Add(v0);
					TriRaw.Add(v1);
					TriRaw.Add(v2);
					TriRaw.Add(cVec3i(3, idMtl, idOb));
					TriRaw.Add(v2);
					TriRaw.Add(v3);
					TriRaw.Add(v0);
				}
				else {
					TriRaw.Add(cVec3i(3, idMtl, idOb));
					TriRaw.Add(v1);
					TriRaw.Add(v2);
					TriRaw.Add(v3);
					TriRaw.Add(cVec3i(3, idMtl, idOb));
					TriRaw.Add(v3);
					TriRaw.Add(v0);
					TriRaw.Add(v1);
				}
				index += 3;
			}
			else {
				const cVec3i i0 = m_Raw[++index];
				nDeg -= 2;
				while (nDeg--) {
					index++;
					TriRaw.Add(cVec3i(3, idMtl, idOb));
					TriRaw.Add(i0);
					TriRaw.Add(m_Raw[index]);
					TriRaw.Add(m_Raw[index + 1]);
				}
			}
			index += 2;
		}
	}
} // cMeshContainer::Triangulate

// cMeshContainer::Triangulate

//-----------------------------------------------------------------------------
// cMeshContainer::Triangulate
//-----------------------------------------------------------------------------
void cMeshContainer::Triangulate() {
	if(IsTriangulated()) {
		return;
	}
	RawArray TriRaw;
	Triangulate(TriRaw);
	m_Raw.Copy(TriRaw);
} // cMeshContainer::Triangulate
	
void cMeshContainer::ToTriQuads() {
	RawArray TriRaw;
	int index = 0;
	while (index < m_Raw.Count()) {
		int nDeg = m_Raw[index][0];
		const int idMtl = m_Raw[index][1];
		const int idOb = m_Raw[index][2];		
		if (nDeg > 4) {
			PlaneTriangulator pl;
			for (int i = 0; i < nDeg; i++) {
				pl.AddPoint(m_Positions[m_Raw[i + index + 1][0]]);
			}
			pl.Triangulate();
			for (int i = 0; i < pl.Result.Count(); i += 3) {
				TriRaw.Add(cVec3i(3, idMtl, idOb));
				TriRaw.Add(m_Raw[index + 1 + pl.Result[i]]);
				TriRaw.Add(m_Raw[index + 1 + pl.Result[i + 1]]);
				TriRaw.Add(m_Raw[index + 1 + pl.Result[i + 2]]);
			}
		} else {
			for (int i = 0; i <= nDeg; i++) {
				TriRaw.Add(m_Raw[index + i]);
			}
		}
		index += nDeg + 1;
	}
	m_Raw = TriRaw;
}

	//-----------------------------------------------------------------------------
// cMeshContainer::InvertRaw
//-----------------------------------------------------------------------------
void cMeshContainer::InvertRaw() {
	int index = 0;
	while(index < m_Raw.Count()) {
		const int Deg = m_Raw[index][0];
		int i = 0;
		while(i < Deg / 2) {
			cMath::Swap(m_Raw[index + 1 + i], m_Raw[index + Deg - i]);
			i++;
		}
		index += Deg + 1;
	}
} // cMeshContainer::InvertRaw

//-----------------------------------------------------------------------------
// cMeshContainer::CalcNormals
//-----------------------------------------------------------------------------
void cMeshContainer::CalcNormals() {
	
	if(m_Positions.Count() < 1) { // No vertices.
		return;
	}
	m_Normals.Clear();

	if (m_Normals.Count() == 0) {
		m_Normals.SetCount(m_Positions.Count(), cVec3::Zero);

		// Pointing raw to newly created normals:
		int index = 0;
		while (index < m_Raw.Count()) {
			int nDeg = m_Raw[index][0];
			while (nDeg--) {
				index++;
				m_Raw[index][2] = m_Raw[index][0];
			}
			index++;
		}
	} else {
		for(int i=0;i<m_Normals.Count();i++) {
			m_Normals[i] = Vector3D::Zero;
		}
	}

	// Calcing normals:
	int index = 0;
	while(index < m_Raw.Count()) {
		const int nDeg = m_Raw[index][0];
		if(nDeg >= 3) { // Skip lines and points.
			cVec3 c(0,0,0);
			for(int i=0;i<nDeg;i++){
				c+=m_Positions[m_Raw[index+i+1][0]];
			}
			c/=nDeg;
			for(int i=0;i<nDeg;i++){
				cVec3 pc=m_Positions[m_Raw[index+i+1][0]];
				cVec3 pn=m_Positions[m_Raw[index+((i+1)%nDeg)+1][0]];
				pc-=c;
				pn-=c;
				const cVec3 Normal = cVec3::Cross(pc,pn);
				int nidx = m_Raw[index + i + 1][2];
				if (nidx >= 0 && nidx < m_Normals.Count()) {
					m_Normals[nidx] += Normal;
				}
			}
			
		}
		index += nDeg + 1;
	}

	// Normalizing normals:
	for(int i = 0; i < m_Normals.Count(); i++) {
		m_Normals[i].NormalizeSafe();
	}
} // cMeshContainer::CalcNormals

void cMeshContainer::CalcSplitNormals(cList<int>& SeamsList){
	if (SeamsList.Count() == 0){
		CalcNormals();
		return;
	}
	cList<int> idsref;
	uni_hash<bi_DWORD,bi_DWORD> fon;
	fon.set_table_size(m_Raw.Count() / 2 + 1);
	int nidx = 0;
	for (int i = 0; i<m_Raw.Count(); i++){
		int n = m_Raw[i][0];
		for (int j = 0; j<n; j++){
			m_Raw[i + j + 1][2] = nidx++;
			int v1 = m_Raw[i + j + 1][0];
			int v2 = m_Raw[i + 1 + (j + 1) % n][0];
			fon.add_quick(bi_DWORD(v1, v2), bi_DWORD(i,j));
		}
		i += n;
	}
	for (int i = 0; i < SeamsList.Count(); i += 2){
		fon.del(bi_DWORD(SeamsList[i], SeamsList[i + 1]));
		fon.del(bi_DWORD(SeamsList[i + 1], SeamsList[i]));
	}
	bool ch;
	do{
		ch = false;
		for (int i = 0; i<m_Raw.Count(); i++){
			int n = m_Raw[i][0];
			for (int j = 0; j<n; j++){
				int v1 = m_Raw[i + j + 1][0];
				int v2 = m_Raw[i + 1 + (j + 1) % n][0];
				scan_key(fon, bi_DWORD(v2, v1), bi_DWORD* fi){
					int f = fi->V1;
					int nf = m_Raw[f][0];
					int& n1 = m_Raw[i + j + 1][2];
					int& n2 = m_Raw[i + 1 + (j + 1) % n][2];
					int& n21 = m_Raw[f + fi->V2 + 1][2];
					int& n11 = m_Raw[f + (fi->V2 + 1) % nf + 1][2];
					int nn1 = ::std::min(n1, n11);
					int nn2 = ::std::min(n2, n21);
					if (nn1 != n1){
						n1 = nn1;
						ch = true;
					}
					if (nn1 != n11){
						n11 = nn1;
						ch = true;
					}
					if (nn2 != n2){
						n2 = nn2;
						ch = true;
					}
					if (nn2 != n21){
						n21 = nn2;
						ch = true;
					}
				}scan_end;
			}
			i += n;
		}
	} while (ch);
	UnlimitedBitset used;
	cList<int> enc;
	for (int i = 0; i<m_Raw.Count(); i++){
		int n = m_Raw[i][0];
		for (int j = 0; j<n; j++){
			used.set(m_Raw[i + j + 1][2], true);
		}
		i += n;
	}
	enc.Add(-1, nidx);
	int pos = 0;
	for (int i = 0; i < nidx; i++){
		if (used.get(i)){
			enc[i] = pos++;
		}
	}
	m_Normals.Clear();
	m_Normals.Add(Vector3D::Zero, pos);
	for (int i = 0; i<m_Raw.Count(); i++){
		int n = m_Raw[i][0];
		for (int j = 0; j<n; j++){
			m_Raw[i + j + 1][2] = enc[m_Raw[i + j + 1][2]];
			assert(m_Raw[i + j + 1][2] != -1);
		}
		i += n;
	}
	for (int i = 0; i<m_Raw.Count(); i++){
		int n = m_Raw[i][0];
		if (n > 2){
			cVec3 c(0, 0, 0);
			for (int j = 0; j < n; j++){
				c += m_Positions[m_Raw[i + j + 1][0]];
			}
			c /= n;
			for (int j = 0; j < n; j++){
				cVec3 pc = m_Positions[m_Raw[i + j + 1][0]];
				cVec3 pn = m_Positions[m_Raw[i + ((j + 1) % n) + 1][0]];
				pc -= c;
				pn -= c;
				const cVec3 Normal = cVec3::Cross(pc, pn);
				m_Normals[m_Raw[i + j + 1][2]] += Normal;
			}
		}
		i += n;
	}
	// Normalizing normals:
	for (int i = 0; i < m_Normals.Count(); i++) {
		m_Normals[i].NormalizeSafe();
	}
}
//*****************************************************************************
// GenPlane
//*****************************************************************************

//-----------------------------------------------------------------------------
// cMeshContainer::PlaneArgs::SetDefaults
//-----------------------------------------------------------------------------
void cMeshContainer::PlaneArgs::SetDefaults() {
	Width = 1.0f;
	Height = 1.0f;
	wSubDivs = 10;
	hSubDivs = 10;
	Axis = cVec3::AxisY;
	TwoSided = false;
} // cMeshContainer::PlaneArgs::SetDefaults

//-----------------------------------------------------------------------------
// cMeshContainer::PlaneArgs::Validate
//-----------------------------------------------------------------------------
void cMeshContainer::PlaneArgs::Validate() {
	const PlaneArgs Defs;
	if(Width < cMath::SpaceEpsilon) {
		Width = Defs.Width;
	}
	if(Height < cMath::SpaceEpsilon) {
		Height = Defs.Height;
	}
	if(wSubDivs < 1) {
		wSubDivs = Defs.wSubDivs;
	}
	if(hSubDivs < 1) {
		hSubDivs = Defs.hSubDivs;
	}
	Axis.Normalize();
	if(!Axis.IsNormalized()) {
		Axis = Defs.Axis;
	}
} // cMeshContainer::PlaneArgs::Validate

//-----------------------------------------------------------------------------
// cMeshContainer::GenPlane
//-----------------------------------------------------------------------------
cMeshContainer * cMeshContainer::GenPlane(PlaneArgs Args) {
	Args.Validate();
	
	const int NPositionsTexCoords = (Args.wSubDivs + 1) * (Args.hSubDivs + 1);
	const int NNormals = (Args.TwoSided ? 2 : 1) * NPositionsTexCoords;
	const int NPolys = Args.wSubDivs * Args.hSubDivs * (Args.TwoSided ? 2 : 1);

	cVec3 W, H;
	Args.Axis.ToPerps(W, H);
	W.FixDenormals();
	H.FixDenormals();
	
	cMeshContainer *Mesh = new cMeshContainer;
	Mesh->SetName("Plane");

	// Positions && TexCoords:
	const float iwSubDivs = 1.0f / (float)Args.wSubDivs;
	const float ihSubDivs = 1.0f / (float)Args.hSubDivs;
	
	float w0 = - 0.5f * Args.Width;
	float wCur = w0;
	float hCur = 0.5f * Args.Height;
	float dw = Args.Width / (float)Args.wSubDivs;
	float dh = Args.Height / (float)Args.hSubDivs;
	int iw, ih;
	for(ih = 0; ih <= Args.hSubDivs; ih++) {
		for(iw = 0; iw <= Args.wSubDivs; iw++) {
			Mesh->GetPositions().Add(wCur * W + hCur * H);
			Mesh->GetTexCoords().Add(cVec2((float)iw * iwSubDivs, (float)ih * ihSubDivs));

			wCur += dw;
		}
		hCur -= dh;
		wCur = w0;
	}

	cAssert(Mesh->GetPositions().Count() == NPositionsTexCoords);
	cAssert(Mesh->GetTexCoords().Count() == NPositionsTexCoords);

	// Normals && Tangents:
	for(int iNormal = 0; iNormal < NNormals; iNormal++) {
		if(iNormal < NPositionsTexCoords) {
			Mesh->GetNormals().Add(Args.Axis);
			Mesh->GetTangents().Add(cVec<cVec3, 2>(W, H));
		} else { // TwoSided
			Mesh->GetNormals().Add(-Args.Axis);
			Mesh->GetTangents().Add(cVec<cVec3, 2>(W, -H));
		}
	}

	cAssert(Mesh->GetNormals().Count() == NNormals);
	cAssert(Mesh->GetTangents().Count() == NNormals);
	
	// Raw:
	for(ih = 0; ih < Args.hSubDivs; ih++) {
		for(iw = 0; iw < Args.wSubDivs; iw++) {
			const int i0 = (Args.wSubDivs + 1) * ih + iw;
			const int i1 = (Args.wSubDivs + 1) * (ih + 1) + iw;

			Mesh->GetRaw().Add(cVec3i(4, 0, 0));
			Mesh->GetRaw().Add(cVec3i(i0));
			Mesh->GetRaw().Add(cVec3i(i1));
			Mesh->GetRaw().Add(cVec3i(i1 + 1));
			Mesh->GetRaw().Add(cVec3i(i0 + 1));

			if(Args.TwoSided) {
				Mesh->GetRaw().Add(cVec3i(4, 0, 0));
				Mesh->GetRaw().Add(cVec3i(i0, i0, NPositionsTexCoords + i0));
				Mesh->GetRaw().Add(cVec3i(i0 + 1, i0 + 1, NNormals / 2 + i0 + 1));
				Mesh->GetRaw().Add(cVec3i(i1 + 1, i1 + 1, NNormals / 2 + i1 + 1));
				Mesh->GetRaw().Add(cVec3i(i1, i1, NNormals / 2 + i1));
			}
		}
	}

	cAssert(Mesh->GetPolyCount() == NPolys);

	return Mesh;
} // cMeshContainer::GenPlane

//----------------------------------------------------------------------------------------------------------
// cMeshContainer::GenPlaneHexagonal
//----------------------------------------------------------------------------------------------------------
cMeshContainer * cMeshContainer::GenPlaneHexagonal(const float Lx, const float Ly, const float Cell, const bool Noisy) {
	cAssert(Cell > 0.0f);

	cMeshContainer *Mesh = new cMeshContainer;
	Mesh->SetName("HexagonalPlane");

	// Positions:
	const int xCells = int(Lx / Cell), yCells = int(Ly / Cell);
	int ix, iy;
	for(iy = 0; iy <= yCells; iy++) {
		for(ix = 0; ix <= xCells; ix++) {
			Mesh->GetPositions().Add(cVec3((float)ix * Cell, float(iy) * Cell, 0.0f));
		}
	}
	for(iy = 0; iy < yCells; iy++) {
		for(ix = 0; ix < xCells; ix++) {
			Mesh->GetPositions().Add(cVec3((float)ix * Cell + Cell / 2.0f, float(iy) * Cell + Cell / 2.0f, 0.0f));
		}
	}
	
	int Shift = (xCells + 1) * (yCells + 1);
	for(int yt = 0; yt < yCells; yt++)
		for(int xt = 0; xt < xCells; xt++) {
			// Left:
			int i0 = yt * xCells + xt + Shift;
			int i1 = yt * (xCells + 1) + xt;
			int i2 = i1 + (xCells + 1);

			Mesh->GetRaw().Add(cVec3i(3, 0, 0));
			Mesh->GetRaw().Add(cVec3i(i0, 0, 0));
			Mesh->GetRaw().Add(cVec3i(i2, 0, 0));
			Mesh->GetRaw().Add(cVec3i(i1, 0, 0));
			// Right:
			Mesh->GetRaw().Add(cVec3i(3, 0, 0));
			Mesh->GetRaw().Add(cVec3i(i0, 0, 0));
			Mesh->GetRaw().Add(cVec3i(i1 + 1, 0, 0));
			Mesh->GetRaw().Add(cVec3i(i2 + 1, 0, 0));
			if(0 == yt) { // Top cover:
				Mesh->GetRaw().Add(cVec3i(3, 0, 0));
				Mesh->GetRaw().Add(cVec3i(i0, 0, 0));
				Mesh->GetRaw().Add(cVec3i(i1, 0, 0));
				Mesh->GetRaw().Add(cVec3i(i1 + 1, 0, 0));
			} else {
				int i4 = i0 - xCells;
				// Left - top:
				Mesh->GetRaw().Add(cVec3i(3, 0, 0));
				Mesh->GetRaw().Add(cVec3i(i0, 0, 0));
				Mesh->GetRaw().Add(cVec3i(i1, 0, 0));
				Mesh->GetRaw().Add(cVec3i(i4, 0, 0));
				// Right - top:
				Mesh->GetRaw().Add(cVec3i(3, 0, 0));
				Mesh->GetRaw().Add(cVec3i(i0, 0, 0));
				Mesh->GetRaw().Add(cVec3i(i4, 0, 0));
				Mesh->GetRaw().Add(cVec3i(i1 + 1, 0, 0));
				if(yCells - 1 == yt) { // Bottom cover:
					Mesh->GetRaw().Add(cVec3i(3, 0, 0));
					Mesh->GetRaw().Add(cVec3i(i0, 0, 0));
					Mesh->GetRaw().Add(cVec3i(i2 + 1, 0, 0));
					Mesh->GetRaw().Add(cVec3i(i2, 0, 0));
				}
			}
	}

	if(Noisy) {
		const float h = 0.18f * Cell;
		for(int i = 0; i < Mesh->GetPositions().Count(); i++) {
			cVec3 &u = Mesh->GetPositions()[i];
			u.x += float(cMath::RandRange1() * h);
			u.y += float(cMath::RandRange1() * h);
			u.z += float(cMath::RandRange1() * h);
		}
	}

	return Mesh;
} // cMeshContainer::GenPlaneHexagonal

//-----------------------------------------------------------------------------
// cMeshContainer::FreeHashTable
//-----------------------------------------------------------------------------
void cMeshContainer::FreeHashTable() const {
	for(int i = 0; i < m_HashTable.Count(); i++) {
		HashEntry *pEntry = m_HashTable[i];
		while(pEntry != NULL) {
			HashEntry *pNext = pEntry->pNext;
			delete pEntry;
			pEntry = pNext;
		}
	}
	m_HashTable.Free();
} // cMeshContainer::FreeHashTable

cMeshContainer::~cMeshContainer(){
	for(int i=0;i<Morphs.Count();i++)delete(Morphs[i]);
}

void cMeshContainer::ConcateWith(const cMeshContainer* mc) {
	int np0 = m_Positions.Count();
	int nn0 = m_Normals.Count();
	int nu0 = m_TexCoords.Count();
	int no0 = m_Objects.Count();
	int nm0 = m_Materials.Count();
	int ns0 = m_UVSets.Count();
	int nr0 = m_Raw.Count();

	cList<int> objMap;
	cList<int> matMap;
	cList<int> uvMap;

	for (int i = 0; i < mc->GetObjects().Count(); i++) {
		auto& ob = mc->GetObjects()[i];
		int idx = -1;
		for (int k = 0; k < GetObjects().Count(); k++) {
			if (ob.Name == GetObjects()[k].Name) {
				idx = k;
				break;
			}
		}
		if (idx == -1) {
			idx = GetObjects().Add(ob);
		}
		objMap.Add(idx);
	}

	for (int i = 0; i < mc->GetMaterials().Count(); i++) {
		auto& ob = mc->GetMaterials()[i];
		int idx = -1;
		for (int k = 0; k < GetMaterials().Count(); k++) {
			if (ob.Name == GetMaterials()[k].Name) {
				idx = k;
				break;
			}
		}
		if (idx == -1) {
			idx = GetMaterials().Add(ob);
		}
		matMap.Add(idx);
	}

	for (int i = 0; i < mc->GetUVSets().Count(); i++) {
		auto& ob = mc->GetUVSets()[i];
		int idx = -1;
		for (int k = 0; k < GetUVSets().Count(); k++) {
			if (ob.Name == GetUVSets()[k].Name) {
				idx = k;
				break;
			}
		}
		if (idx == -1) {
			idx = GetUVSets().Add(ob);
		}
		uvMap.Add(idx);
	}
	if (uvMap.Count() == 0)uvMap.Add(0);
	if (matMap.Count() == 0)matMap.Add(0);
	if (objMap.Count() == 0)objMap.Add(0);

	m_Positions.AddRange(mc->m_Positions);
	m_Normals.AddRange(mc->m_Normals);
	m_TexCoords.AddRange(mc->m_TexCoords);
	//m_Objects.AddRange(mc->m_Objects.ToPtr(),mc->m_Objects.Count());
	//m_Materials.AddRange(mc->m_Materials.ToPtr(),mc->m_Materials.Count());
	//m_UVSets.AddRange(mc->m_UVSets.ToPtr(),mc->m_UVSets.Count());
	m_Raw.AddRange(mc->m_Raw);
	m_VertexColor.AddRange(mc->m_VertexColor.ToPtr(), mc->m_VertexColor.Count());
	for (int i = nr0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		unsigned int r = m_Raw[i][1];
		m_Raw[i][1] = matMap.uGet(r & 65535, 0) + (uvMap.uGet(r >> 16, 0) << 16);
		m_Raw[i][2] = objMap.uGet(m_Raw[i][2], 0);
		for (int j = 0; j < n; j++) {
			m_Raw[i + 1 + j][0] += np0;
			if (m_Raw[i + 1 + j][1] != -1)m_Raw[i + 1 + j][1] += nu0;
			if (m_Raw[i + 1 + j][2] != -1)m_Raw[i + 1 + j][2] += nn0;
		}
		i += n;
	}
}

void cMeshContainer::Textures2VColor(){
	if (m_VertexColor.Count() != 0){
		bool allgrey = true;
		for (int i = 0; i < m_VertexColor.Count(); i++){
			DWORD C = m_VertexColor[i];
			if (C != 0xFFFFFFFF && C != 0xFF808080){
				allgrey = false;
				break;
			}
		}
		if (allgrey)m_VertexColor.Clear();
	}
	if(m_VertexColor.Count()==0){
		bool tex=false;
		for(int i=0;i<m_Materials.Count();i++){
			if(!m_Materials[i].Tex[0].FileName.IsEmpty())tex=true;
		}
		if(tex){
			WeldEdges(CalcBoundBox().GetDiagonal() / 10000.0f);
			cList<TexturePattern*> tps;
			cList<TexturePattern*> tpm;
			cList<TexturePattern*> tpg;
			cList<TexturePattern*> tpd;
			cList<TexturePattern*> tpwn;
			
			bool gmet = false;
			bool dep = false;
			bool wmap = false;

			for(int i=0;i<m_Materials.Count();i++){
				
				TexturePattern* tp = NULL;
				TexturePattern* tp_met = NULL;
				TexturePattern* tp_gloss = NULL;
				TexturePattern* tp_depth = NULL;
				TexturePattern* tp_wnormal = NULL;
				
				if (!m_Materials[i].Tex[0].FileName.IsEmpty()) { // color
					tp = new TexturePattern;
					tp->LoadTexture(m_Materials[i].Tex[0].FileName.ToCharPtr());
					cStr wn = m_Materials[i].Tex[0].FileName;
					cStr ext = wn.GetFileExtension();
					wn.RemoveFileExtension();
					int under = wn.LastIndexOfAny("_-.~ ");
					if (under != -1) {
						cStr wnm = wn.Substring(0, under);
						wnm += "_worldmap." + ext;
						if (CheckIfFileExists(wnm)) {
							tp_wnormal = new TexturePattern;
							tp_wnormal->LoadTexture(wnm);
							if(tp_wnormal->SizeX) {
								wmap = true;
							}
						}
					}
				}				
				if (!m_Materials[i].Tex[1].FileName.IsEmpty()) { // gloss
					tp_gloss = new TexturePattern;
					tp_gloss->LoadTexture(m_Materials[i].Tex[1].FileName.ToCharPtr());
					// check for metall
					cStr fn = m_Materials[i].Tex[1].FileName;
					FileList FL;
					cStr fp = fn.GetFileName();
					fp.RemoveFileExtension();
					int under = fp.LastIndexOfAny("_-.~ ");
					if(under!=-1){
						cStr alias = fp.Substring(under);
						if(alias.Contains("rough", true)) {
							int n = tp_gloss->SizeX * tp_gloss->SizeY;
							if(tp_gloss->Data) {
								for(int i=0;i<n;i++) {
									tp_gloss->Data[i] ^= 0xFFFFFF;
								}
							}
						}
						fp.Remove(under);
						fp = fp.GetFileBase();
						CreateSortedFileList(fn.GetFilePath(), fp + "*.*", FL, false);
						for (int i = 0; i < FL.Count(); i++) {
							cStr tfn = FL[i];
							cStr ext = tfn.GetFileExtension();
							ext.ToLower();
							if (ext == "tga" || ext == "png" || ext == "jpg") {
								cStr fnm = tfn.GetFileName();
								fnm.RemoveFileExtension();
								fnm.ToLower();
								if (fnm.StartsWith(fp) && fnm.EndsWith("metalness") || fnm.EndsWith("metal") || fnm.EndsWith("metall") || fnm.EndsWith("met") || fnm.EndsWith("metallicity")) {
									if (!tp_met) {
										tp_met = new TexturePattern;
									}
									tp_met->LoadTexture(tfn);
								}
							}
						}
					}
				}				
				
				if (!m_Materials[i].Tex[2].FileName.IsEmpty()) { // displacement
					tp_depth = new TexturePattern;
					if (!tp_depth->LoadTexture(m_Materials[i].Tex[2].FileName.ToCharPtr())) {
						delete(tp_depth);
						tp_depth = nullptr;
					}
				}					
				
				tps.Add(tp);
				tpm.Add(tp_met);
				tpg.Add(tp_gloss);
				tpd.Add(tp_depth);
				tpwn.Add(tp_wnormal);
				
				if(tp_met || tp_gloss) {
					gmet = true;
				}
				if (tp_depth)dep = true;
			}
			m_VertexColor.Add(0xFF808080,m_Positions.Count());			
			for(int i=0;i<m_Raw.Count();i++){
				int n=m_Raw[i][0];
				int m=m_Raw[i][1]&0xFFFF;
				if(n && m>=0 && m<tps.Count() && tps[m]){
					for(int j=0;j<n;j++){
						int vp=m_Raw[i+j+1][0];
						int uvv=m_Raw[i+j+1][1];
						if(uvv!=-1){
							float u=m_TexCoords[uvv][0];
							float v=m_TexCoords[uvv][1];
							Vector4D CL = tps[m] ? tps[m]->getPixelUV(u, 1 - v) : Vector4D::Zero;
							DWORD C=V4D2DW(CL);
							m_VertexColor[vp]=C;
						}
					}
				}
				i+=n;
			}
			if (gmet) {
				m_GlossMetallEmissive.Add(0, m_Positions.Count());
				for (int i = 0; i < m_Raw.Count(); i++) {
					int n = m_Raw[i][0];
					int m = m_Raw[i][1] & 0xFFFF;
					if (n && m >= 0 && m < tps.Count()) {
						for (int j = 0; j < n; j++) {
							int vp = m_Raw[i + j + 1][0];
							int uvv = m_Raw[i + j + 1][1];
							if (uvv != -1) {
								float u = m_TexCoords[uvv][0];
								float v = m_TexCoords[uvv][1];
								Vector4D CL(0);
								if (tpg[m])CL.x = tpg[m]->getPixelUV(u, 1 - v).x;
								if (tpm[m])CL.y = tpm[m]->getPixelUV(u, 1 - v).x;
								clamp4D(CL);
								DWORD C = V4D2DW(CL);
								m_GlossMetallEmissive[vp] = C;
							}
						}
					}
					i += n;
				}
			}
			if(dep) {
				m_NormalDisplacement.Add(0, m_Positions.Count());
				for (int i = 0; i < m_Raw.Count(); i++) {
					int n = m_Raw[i][0];
					int m = m_Raw[i][1] & 0xFFFF;
					if (n && m >= 0 && m < tps.Count()) {
						for (int j = 0; j < n; j++) {
							int vp = m_Raw[i + j + 1][0];
							int uvv = m_Raw[i + j + 1][1];
							if (uvv != -1) {
								float u = m_TexCoords[uvv][0];
								float v = m_TexCoords[uvv][1];
								float d = 0;
								if (tpd[m])d = tpd[m]->getPixelUV(u, 1 - v).x;
								m_NormalDisplacement[vp] = d;
							}
						}
					}
					i += n;
				}
			}
			if (wmap) {
				cList<DWORD> world_map;
				cList<Vector4D> poss;
				world_map.Add(0, m_Positions.Count());
				poss.Add(Vector4D::Zero, m_Positions.Count());
				for (int i = 0; i < m_Raw.Count(); i++) {
					int n = m_Raw[i][0];
					int m = m_Raw[i][1] & 0xFFFF;
					if (n && m >= 0 && m < tps.Count()) {
						for (int j = 0; j < n; j++) {
							int vp = m_Raw[i + j + 1][0];
							int uvv = m_Raw[i + j + 1][1];
							if (uvv != -1) {
								float u = m_TexCoords[uvv][0];
								float v = m_TexCoords[uvv][1];
								Vector4D C(0);
								if (tpwn[m])C = tpwn[m]->getPixelUV(u, 1 - v);
								std::swap(C.x, C.z);
								world_map[vp] = V4D2DW(C);
								poss[vp] = m_Positions[vp];
							}
						}
					}
					i += n;
				}
				cList<int> links;
				cList<DWORD> nrm;
				nrm.SetCount(m_Positions.Count(), 0);
				for (int i = 0; i < m_Raw.Count(); i++) {
					int n = m_Raw[i][0];
					for (int j = 0; j < n; j++) {
						int jn = (j + 1) % n;
						auto& v1 = m_Raw[i + j + 1];
						auto& v2 = m_Raw[i + jn + 1];
						int pv1 = v1[0];
						int pv2 = v2[0];
						if (world_map[pv1] && world_map[pv2]) {
							links.Add(pv1);
							links.Add(pv2);
						}
					}
					i += n;
				}
				Normals2Geometry NG;
				NG.setup(poss, world_map, links);
				if (!NG.failed()) {
					NG.calculate(poss);
					if (!NG.failed()) {
						for (int i = 0; i < m_Positions.Count(); i++) {
							m_Positions[i] = poss[i].ToVec3();
						}
					}
				}
			}
			for(int i=0;i<tps.Count();i++){
				if(tps[i])delete(tps[i]);
			}
			for (int i = 0; i < tpg.Count(); i++) {
				if (tpg[i])delete(tpg[i]);
			}
			for (int i = 0; i < tpm.Count(); i++) {
				if (tpm[i])delete(tpm[i]);
			}
			for (int i = 0; i < tpd.Count(); i++) {
				if (tpd[i])delete(tpd[i]);
			}
			for (int i = 0; i < tpwn.Count(); i++) {
				if (tpwn[i])delete(tpwn[i]);
			}
		}
	}
	//RemoveUnusedVerts();
}

bool cMeshContainer::Texture2VNormals(VecArray& wnormals, int options) {
	bool tex = false;
	for (int i = 0; i < m_Materials.Count(); i++) {
		if (!m_Materials[i].Tex[3].FileName.IsEmpty())tex = true;
	}
	bool nm = false;
	if (tex) {
		cList<TexturePattern*> tpn;
		for (int i = 0; i < m_Materials.Count(); i++) {
			TexturePattern* tp_n = NULL;
			if (!m_Materials[i].Tex[3].FileName.IsEmpty()) { // normalmap
				tp_n = new TexturePattern;
				tp_n->LoadTexture(m_Materials[i].Tex[3].FileName.ToCharPtr());
				if (!tp_n->CheckIfNormalmap()) {
					delete(tp_n);
					tp_n = nullptr;
				}
			}
			tpn.Add(tp_n);
			if (tp_n)nm = true;
		}
		if (nm) {
			CalcTangents();
			wnormals.SetCount(m_Positions.Count(), cVec3::Zero);
			for (int i = 0; i < m_Raw.Count(); i++) {
				int n = m_Raw[i][0];
				int m = m_Raw[i][1] & 0xFFFF;
				if (n && m >= 0 && m < tpn.Count()) {
					for (int j = 0; j < n; j++) {
						int vp = m_Raw[i + j + 1][0];
						int uvv = m_Raw[i + j + 1][1];
						int nv = m_Raw[i + j + 1][2];
						if (uvv != -1 && nv != -1) {
							float u = m_TexCoords[uvv][0];
							float v = m_TexCoords[uvv][1];
							cVec3 C(128,128,255);
							if (tpn[m])C = tpn[m]->getPixelUV(u, 1 - v).ToVec3();
							C -= cVec3(128);
							C.Normalize();
							auto& tan = m_Tangents[nv];
							if(options & 2)std::swap(C.z, C.y);
							if(options & 4)C.z = -C.z;
							if(options & 8)C.y = -C.y;
							wnormals[vp] += C.z*tan[0] + C.y*tan[1]+C.x*m_Normals[nv];
						}
					}
				}
				i += n;
			}
			for (auto& n : wnormals) {
				n.Normalize();
			}
		}
	}
	return false;
}

void cMeshContainer::CalcTangents() {
	m_Tangents.Clear();
	if (m_Normals.IsEmpty()) {
		return; // No normals -> no tangents
	}

	if (m_TexCoords.IsEmpty()) {
		return; // No texcoords -> no tangents
	}

	m_Tangents.SetCount(m_Normals.Count(), cVec<cVec3, 2>(cVec3::Zero, cVec3::Zero));

	// Calcing tanspace
	int Index = 0, Deg;
	cVec3 Normal, Tangent, BiTangent;
	int i, j;
	cMat3 T;
	while (Index < m_Raw.Count()) {
		Deg = m_Raw[Index][0];
		if (Deg >= 3) { // Skip lines and points
			const cVec3i& i0 = m_Raw[Index + 1];
			const cVec3i& i1 = m_Raw[Index + 2];
			const cVec3i& i2 = m_Raw[Index + 3];

			const cVec3 u = m_Positions[i1[0]] - m_Positions[i0[0]];
			const cVec3 v = m_Positions[i2[0]] - m_Positions[i0[0]];
			const cVec2 s = m_TexCoords.IsEmpty() ? cVec2::Zero : m_TexCoords[i1[1]] - m_TexCoords[i0[1]];
			const cVec2 t = m_TexCoords.IsEmpty() ? cVec2::Zero : m_TexCoords[i2[1]] - m_TexCoords[i0[1]];

			Normal = cVec3::Cross(u, v);

			T.SetCol0(u);
			T.SetCol1(v);
			T.SetCol2(Normal);
			T.Invert();

			Tangent = cVec3::Transform(cVec3(s.x, t.x, 0.0f), T);
			BiTangent = cVec3::Transform(cVec3(s.y, t.y, 0.0f), T);

			// Reduces influence of small polygon to resulting smooth tanspace
			Tangent *= cMath::Square(1.0f / Tangent.Length());
			BiTangent *= cMath::Square(1.0f / BiTangent.Length());

			for (j = 0; j < Deg; j++) {
				const cVec3i& r = m_Raw[Index + 1 + j];
				m_Tangents[r[2]] += cVec<cVec3, 2>(Tangent, BiTangent);
			}
		}
		Index += Deg + 1;
	}

	// Normalizing tanspace
	for (i = 0; i < m_Tangents.Count(); i++) {
		m_Tangents[i][0].NormalizeSafe();
		m_Tangents[i][1].NormalizeSafe();
	}
}


template< bool withErase >
::std::pair< cVec3, cVec3 >  cMeshContainer::Divide(
	int a, int b, float tAB,
	int c, int d, float tCD,
	int* beginRemovedRawBlock,
	int* sizeRemovedRawBlock,
	::std::pair< int, int >* changedBeginRawBlock
) {
	assert( a >= 0 );
	assert( b >= 0 );
	assert( c >= 0 );
	assert( d >= 0 );
	assert( (tAB >= 0.0f) && (tAB <= 1.0f) );
	assert( (tCD >= 0.0f) && (tCD <= 1.0f) );

	if ((a == c) && (b == d)) {
		throw comms::Exception( "The edges 'ab' and 'cd' are same." );
	}

	// calc points for sides 'ab' & 'cd'
	const cVec3&  ca = GetPosition( a );
	const cVec3&  cb = GetPosition( b );
	const cVec3&  cc = GetPosition( c );
	const cVec3&  cd = GetPosition( d );
	const ::std::pair< cVec3, cVec3 >  r = ::std::make_pair(
		cLineF3::Point( ca, cb, tAB ),
		cLineF3::Point( cc, cd, tCD )
	);
	if (r.first == r.second) {
		throw comms::Exception( "The points for divide are equals." );
	}

	Divide< withErase >(
		a, b, r.first,
		c, d, r.second,
		beginRemovedRawBlock,
		sizeRemovedRawBlock,
		changedBeginRawBlock
	);

	return r;
}

template
::std::pair< cVec3, cVec3 >  cMeshContainer::Divide< true >(
	int a, int b, float tAB,
	int c, int d, float tCD,
	int* beginRemovedRawBlock,
	int* sizeRemovedRawBlock,
	::std::pair< int, int >* changedBeginRawBlock
);

template
::std::pair< cVec3, cVec3 >  cMeshContainer::Divide< false >(
	int a, int b, float tAB,
	int c, int d, float tCD,
	int* beginRemovedRawBlock,
	int* sizeRemovedRawBlock,
	::std::pair< int, int >* changedBeginRawBlock
);



template< bool withErase >
void cMeshContainer::Divide(
	int a, int b, const cVec3& m,
	int c, int d, const cVec3& n,
	int* beginRemovedRawBlock,
	int* sizeRemovedRawBlock,
	::std::pair< int, int >* changedBeginRawBlock
) {
	assert( a >= 0 );
	assert( b >= 0 );
	assert( c >= 0 );
	assert( d >= 0 );

	if ((a == c) && (b == d)) {
		throw comms::Exception( "The edges 'ab' and 'cd' are same." );
	}

	// # Start of block is remain.
	// # All points must be in the same block.

	// divide a figure (block) on the two parts by line 'mn'
	figureI_t  figure1;
	figureI_t  figure2;
	const edgeI_t  ab( a, b );
	const edgeI_t  cd( c, d );
	const cList< int >  brl = Find( ab, cd );
	if ( brl.Count() != 1 ) {
		throw comms::Exception( "Must be one figure." );
	}
	const int  br = brl.GetFirst();
	const int  i = br;
	int sizeBR = m_Raw[ i ][ 0 ];
	// @todo fine  Release AddPosition( pos ) for add unique.
	VecArray&  pos = GetPositions();
	int  im = GetVertex( m );
	if (im == -1) {
		im = pos.Add( m );
	}
	int  in = GetVertex( n );
	if (in == -1) {
		in = pos.Add( n );
	}

	figureI_t* figure = &figure1;
	for (int j = 0; j < sizeBR; ++j) {
		const int  iRaw1 = i + 1 + j;
		const int  iRaw2 = i + 1 + (j + 1) % sizeBR;
		const int  ip1 = m_Raw[ iRaw1 ][ 0 ];
		const int  ip2 = m_Raw[ iRaw2 ][ 0 ];
		if ( figure->IsEmpty() || (figure->GetLast() != ip1) ) {
			figure->Add( ip1 );
		}
		const edgeI_t  edge( ip1, ip2 );
		const bool  hasAB = (edge == ab);
		const bool  hasCD = (edge == cd);
		if ( hasAB || hasCD ) {
			const int  ip = hasAB ? im : in;
			if ( figure->GetLast() != ip ) {
				figure->Add( ip );
			}
			// switch to work with other figure
			figure = (figure == &figure1) ? &figure2 : &figure1;
			if ( figure->IsEmpty() || (figure->GetLast() != ip) ) {
				figure->Add( ip );
			}
		}
	} // for (int j = 0; j < sizeBR; ++j)


	// change the divided figure by the two calculated figure
	if ( withErase ) {
		Erase( br, true );
	} else {
		// do confluent the figure
		// # Must call EraseClearConfluentByIndexOnly() for correct mesh.
		for (int j = 0; j < sizeBR; ++j) {
			const int  iRaw1 = i + 1 + j;
			m_Raw[ iRaw1 ][ 0 ] = -1;
		}
	}

	/* @test
	std::cout << "  mesh  " << m_Raw << std::endl;
	std::cout << "  figure1  " << figure1 << std::endl;
	std::cout << "  figure2  " << figure2 << std::endl;
	*/

	Insert( figure1, changedBeginRawBlock ? &changedBeginRawBlock->first  : NULL );
	//std::cout << "  First insert  " << *this << std::endl;
	Insert( figure2, changedBeginRawBlock ? &changedBeginRawBlock->second : NULL );
	//std::cout << "  Second insert  " << *this << std::endl;

	if ( beginRemovedRawBlock ) { *beginRemovedRawBlock = br; }
	if ( sizeRemovedRawBlock )  { *sizeRemovedRawBlock  = sizeBR; }
}

template
void cMeshContainer::Divide< true >(
	int a, int b, const cVec3& m,
	int c, int d, const cVec3& n,
	int* beginRemovedRawBlock,
	int* sizeRemovedRawBlock,
	::std::pair< int, int >* changedBeginRawBlock
);

template
void cMeshContainer::Divide< false >(
	int a, int b, const cVec3& m,
	int c, int d, const cVec3& n,
	int* beginRemovedRawBlock,
	int* sizeRemovedRawBlock,
	::std::pair< int, int >* changedBeginRawBlock
);


void cMeshContainer::Erase( int beginRawFigure, bool withOptimize ) {
	assert( beginRawFigure >= 0 );
	const int  n = m_Raw[ beginRawFigure ][ 0 ];
	m_Raw.RemoveAt( beginRawFigure, n + 1 );
	if ( withOptimize ) {
		RemoveUnusedVerts();
	}
}


void cMeshContainer::Erase( const cList< int >& beginRawFigures, bool withOptimize ) {
	assert( !beginRawFigures.IsEmpty() );
	for (int i = 0; i < beginRawFigures.Count(); ++i) {
		Erase( beginRawFigures[ i ], withOptimize );
	}
}


void cMeshContainer::EraseByCount( int n ) {
	return EraseByCount( n, n );
}


void cMeshContainer::EraseByCount( int a, int b ) {
	if (a > b) {
		throw comms::Exception( "Incorrect diapason." );
	}
	for (int i = 0;  i < m_Raw.Count();  /**/) {
		const int  n = m_Raw[ i ][ 0 ];
		if ((n >= a) && (n <= b)) {
			// our client, destroy
			m_Raw.RemoveAt( i, n + 1 );
		} else {
			i += (n + 1);
		}
	} // for (int i = 0;  i < m_Raw.Count();  ++i)
}


void cMeshContainer::EraseClearConfluent( float tolerance ) {
	assert( tolerance >= 0.0f );

	for (int i = 0;  i < m_Raw.Count();  /* */) {
		if ( IsClearConfluent( i, tolerance ) ) {
			Erase( i, false );
		} else {
			const int  n = m_Raw[ i ][ 0 ];
			i += (n + 1);
		}
	} // for (int i = 0;  i < m_Raw.Count();  ++i)
}


void cMeshContainer::EraseClearConfluentByIndexOnly() {

	for (int i = 0;  i < m_Raw.Count();  /* */) {
		if ( IsClearConfluentByIndexOnly( i ) ) {
			Erase( i, false );
		} else {
			const int  n = m_Raw[ i ][ 0 ];
			i += (n + 1);
		}
	} // for (int i = 0;  i < m_Raw.Count();  ++i)
}


void cMeshContainer::Invert( int beginRawFigure ) {
	// @todo Refactoring InvertRaw()
	const int  i = beginRawFigure;
	const int  n = m_Raw[ i ][ 0 ];
	for (int j = 0; j < (n / 2); ++j) {
		cMath::Swap( m_Raw[ i + 1 + j ], m_Raw[ i + n - j ] );
	}
}


cList< int > cMeshContainer::Find( const cVec3& a ) const {
	const int  ai = GetVertex( a );
	if (ai == -1) {
		return cList< int >();
	}
	return Find( ai );
}


cList< int > cMeshContainer::Find( int a ) const {
	cList< int >  r;
	for (int i = 0;  i < m_Raw.Count();  ++i) {
		const int  n = m_Raw[ i ][ 0 ];
		for (int j = 0; j < n; ++j) {
			const int  iRaw = i + j + 1;
			const int  ip = m_Raw[ iRaw ][ 0 ];
			if (a == ip) {
				// node 'a' is found!
				r.Add( i );
				break;
			}
		} // for (int j = 0; j < n; ++j)
		i += n;
	} // for (int i = 0;  i < m_Raw.Count();  ++i)

	return r;
}


cList< int > cMeshContainer::Find( const cVec3& a, const cVec3& b ) const {
	const int  ai = GetVertex( a );
	const int  bi = GetVertex( b );
	if ((ai == -1) || (bi == -1)) {
		return cList< int >();
	}
	return Find( edgeI_t( ai, bi ) );
}


cList< int > cMeshContainer::Find( int ai, int bi ) const {
	return Find( edgeI_t( ai, bi ) );
}


cList< int > cMeshContainer::Find( const edgeI_t& e ) const {
	cList< int >  r;
	for (int i = 0;  i < m_Raw.Count();  ++i) {
		const int  n = m_Raw[ i ][ 0 ];
		// skip points
		if (n > 1) {
			for (int j = 0; j < n; ++j) {
				const int  iRaw1 = i + j + 1;
				const int  iRaw2 = i + 1 + (j + 1) % n;
				const int  ai = m_Raw[ iRaw1 ][ 0 ];
				const int  bi = m_Raw[ iRaw2 ][ 0 ];
				const edgeI_t  edge( ai, bi );
				if ( edge == e ) {
					// edge is found!
					r.Add( i );
					break;
				}
			} // for (int j = 0; j < n; ++j)
		} // if (n > 1)
		i += n;
	} // for (int i = 0;  i < m_Raw.Count();  ++i)

	return r;
}


cList< int > cMeshContainer::Find( const edgeI_t& ea, const edgeI_t& eb ) const {
	cList< int >  r;
	for (int i = 0;  i < m_Raw.Count();  ++i) {
		const int  n = m_Raw[ i ][ 0 ];
		// skip points
		if (n > 1) {
			bool  found = false;
			for (int j = 0; j < n; ++j) {
				const int  iRaw1 = i + j + 1;
				const int  ai = m_Raw[ iRaw1 ][ 0 ];
				// skip dirt vertices
				// @see cMeshContainerTest::DivideM() for details
				if ( ai == -1 ) { break; }

				const int  iRaw2 = i + 1 + (j + 1) % n;
				const int  bi = m_Raw[ iRaw2 ][ 0 ];
				const edgeI_t  edge( ai, bi );
				const bool  f = (edge == ea) || (edge == eb);
				if ( !found && f ) {
					found = true;
				} else if ( found && f ) {
					// edges are found in this figure!
					r.Add( i );
					break;
				}
			} // for (int j = 0; j < n; ++j)
		} // if (n > 1)
		i += n;
	} // for (int i = 0;  i < m_Raw.Count();  ++i)

	return r;
}


int cMeshContainer::Find( const VecArray& figure ) const {
	cList< int >  fi;
	for (int i = 0; i < figure.Count(); ++i) {
		const int  vi = GetVertex( figure[ i ] );
		if (vi == -1) {
			throw comms::Exception( "Edge by this coord is absent." );
		}
		fi.Add( vi );
	}
	return Find( fi );
}


int cMeshContainer::Find( const cList< int >& figure ) const {
	if ( figure.IsEmpty() ) {
		throw comms::Exception( "Figure is empty." );
	}

	for (int i = 0;  i < m_Raw.Count();  ++i) {
		const int  n = m_Raw[ i ][ 0 ];
		if (figure.Count() == n) {
			bool  equal = true;
			for (int j = 0; j < n; ++j) {
				const int  iRaw = i + j + 1;
				const int  ip = m_Raw[ iRaw ][ 0 ];
				const int  ipf = figure[ j ];
				if (ip != ipf) {
					equal = false;
					break;
				}
			} // for (int j = 0; j < n; ++j)
			if ( equal ) {
				return i;
			}
		} // if (figure.Count() == n)
		i += n;
	} // for (int i = 0;  i < m_Raw.Count();  ++i)

	return -1;
}


cList< int > cMeshContainer::FindClearConfluent( float tolerance ) const {
	assert( tolerance >= 0.0f );

	cList< int >  r;
	for (int i = 0;  i < m_Raw.Count();  ++i) {
		if ( IsClearConfluent( i, tolerance ) ) {
			r.Add( i );
		}
		const int  n = m_Raw[ i ][ 0 ];
		i += n;
	} // for (int i = 0;  i < m_Raw.Count();  ++i)

	return r;
}


void cMeshContainer::CorrectFaces(
	int  beginRawBlockEtalon,
	uni_hash< bool, int >*  fixed
) {
	// @todo potential error  Verify when 'fixed' is present and contains
	//       raw-blocks around a set of blocks which must be fixed.

	if ( !fixed ) {
		fixed = new uni_hash< bool, int >;
		CorrectFaces( beginRawBlockEtalon, fixed );
		delete fixed;
		return;
	}

	fixed->add_quick( beginRawBlockEtalon, true );

	const int  i = beginRawBlockEtalon;
	const int  n = m_Raw[ i ][ 0 ];
	// skip empty blocks, points and lines
	if (n < 3) { return; }

	for (int j = 0; j < n; ++j) {
		// look side-by-side
		const int  iRaw1 = i + j + 1;
		const int  iRaw2 = i + 1 + (j + 1) % n;
		const int  a = m_Raw[ iRaw1 ][ 0 ];
		const int  b = m_Raw[ iRaw2 ][ 0 ];
		const cList< int >  ftr = Find( a, b );
		for (int k = 0; k < ftr.Count(); ++k) {
			const int  io = ftr[ k ];

			// skip the same raw-block
			if (io == i) { continue; }
			// skip the fixed raw-block
			if ( fixed->get( io ) ) { continue; }

			fixed->add_quick( io, true );

			// process the other raw-block
			const int  no = m_Raw[ io ][ 0 ];
			// skip empty blocks, points and lines
			if (no < 3) { continue; }

			for (int jo = 0; jo < no; ++jo) {
				// search the side 'ab' and detect order it
				const int  ioRaw1 = io + jo + 1;
				const int  ioRaw2 = io + 1 + (jo + 1) % no;
				const int  ao = m_Raw[ ioRaw1 ][ 0 ];
				const int  bo = m_Raw[ ioRaw2 ][ 0 ];
				const bool  foundOtherOrder = ((a == ao) && (b == bo));
				const bool  foundSameOrder  = ((a == bo) && (b == ao));
				if ( !foundSameOrder && !foundOtherOrder) { continue; }

				if ( foundOtherOrder ) {
					Invert( io );
					break;
				}
			} // for (int jo = 0; jo < no; ++jo)

			CorrectFaces( io, fixed );
			break;

		} // for (int k = 0; k < ftr.Count(); ++k)
	} // for (int j = 0; j < n; ++j)
}


void cMeshContainer::CorrectRawSequence() {
	for (int i = 0;  i < m_Raw.Count();  ++i) {
		int*  n = &m_Raw[ i ][ 0 ];
		// skip points
		if (*n > 1) {
			UnlimitedBitset  has;
			for (int j = 0; j < *n; /**/) {
				const int  iRaw = i + j + 1;
				const int  ip = m_Raw[ iRaw ][ 0 ];
				if ( has.get( ip ) ) {
					m_Raw.RemoveAt( iRaw, 1 );
					--(*n);
				} else {
					++j;
					has.set( ip, true );
				}
			} // for (int j = 0; j < *n; ++j)
		} // if (n > 1)
		i += *n;
	} // for (int i = 0;  i < m_Raw.Count();  ++i)
}

void cMeshContainer::RemoveUnusedObjMtl(){
	UnlimitedBitset usm;
	UnlimitedBitset usu;
	UnlimitedBitset usob;
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		usm.set(m_Raw[i][1]&0xFFFF,true);
		usu.set(m_Raw[i][1]>>16, true);
		usob.set(m_Raw[i][2],true);
		i+=n;
	}
	cList<int> encobj;
	cList<int> encuv;
	cList<int> encmtl;
	encobj.Add(-1,m_Objects.Count());
	encmtl.Add(-1,m_Materials.Count());
	encuv.Add(-1, m_UVSets.Count());
	int ob=0;
	for(int i=0;i<m_Objects.Count();i++){
		if(usob.get(i))encobj[i]=ob++;
	}
	ob=0;
	for(int i=0;i<m_Objects.Count();i++){
		if(!usob.get(ob)){
			m_Objects[i].Name.Clear();
			m_Objects.RemoveAt(i);
			i--;
		}
		ob++;
	}
	int mtl=0;
	for(int i=0;i<m_Materials.Count();i++){
		if(usm.get(i))encmtl[i]=mtl++;
	}
	mtl=0;
	for(int i=0;i<m_Materials.Count();i++){
		if(!usm.get(mtl)){
			m_Materials[i].Name.Clear();
			m_Materials.RemoveAt(i);
			i--;
		}
		mtl++;
	}
	int uu = 0;
	for (int i = 0; i<m_UVSets.Count(); i++){
		if (usu.get(i))encuv[i] = uu++;
	}
	uu = 0;
	for (int i = 0; i<m_UVSets.Count(); i++){
		if (!usu.get(uu)){
			m_UVSets[i].Name.Clear();
			m_UVSets.RemoveAt(i);
			i--;
		}
		uu++;
	}
	for(int i=0;i<m_Raw.Count();i++){
		int m=m_Raw[i][1]&0xFFFF;
		int u = m_Raw[i][1] >> 16;
		if (u < encuv.Count())u = encuv[u];
		else u = 0;
		// stable for the dirt mesh
		if ((m_Raw[i][2]>=0 && m_Raw[i][2] < encobj.Count()) && (m < encmtl.Count())) {
			m_Raw[i][2]=encobj[m_Raw[i][2]];
			m_Raw[i][1]=encmtl[m] + (u<<16);
		}
		int n=m_Raw[i][0];
		i+=n;
	}
}
void cMeshContainer::MergeFaces( float fusionDistance ) {
	// verify all edges of mesh
	for (int vi = 0; vi < m_Positions.Count(); ++vi) {
		const cVec3&  v = m_Positions[ vi ];
		// verify: New vertex on the some edge? Divide the edge!
		uni_hash< bool, DWORDS2 >  visitedOnce;
		for (int countRawPrev = m_Raw.Count();
			/* see below */;
			countRawPrev = m_Raw.Count()
		) {
			// verify all edges of mesh
			for (int i = 0; i < m_Raw.Count(); ++i) {
				const int  n = m_Raw[ i ][ 0 ];
				if (n > 1) {
					for (int j = 0; j < n; ++j) {
						const int  iRaw1 = i + j + 1;
						const int  iRaw2 = i + 1 + (j + 1) % n;
						const int  ip1 = m_Raw[ iRaw1 ][ 0 ];
						const int  ip2 = m_Raw[ iRaw2 ][ 0 ];
						if ((ip1 == vi) || (ip2 == vi)) {
							continue;
						}
						const DWORDS2  ab( (DWORD)ip1, (DWORD)ip2 );
						if ( visitedOnce.get( ab ) ) {
							continue;
						}
						visitedOnce.add_quick( ab, true );
						const cVec3&  p1 = m_Positions[ ip1 ];
						const cVec3&  p2 = m_Positions[ ip2 ];
						// @todo optimize  Without cast.
						typedef cVec< float, 3 >  point_t;
						const float  distance =
							static_cast< point_t >( v ).DistanceToLineSegSq( p1, p2 );
						if (distance < fusionDistance) {
							// insert vertex to the edge 'ip1--ip2'
							Insert( ip1, ip2, v );
							// #! After call Insert() the mesh is changed. Break loops!
							i = m_Raw.Count();
							break;
						}
					} // for (int j = 0; j < n; ++j)
				} // if (n > 1)
				i += n;
			} // for (int i = 0; i < m_Raw.Count(); ++i)
			if (countRawPrev == m_Raw.Count()) {
				break;
			}
		} // for (int countRawPrev = ...
	} // for (int vi = 0; vi < m_Positions.Count(); ++vi)

	// fixed empty raw-blocks...
	EraseByCount( 0 );
	// ...and situation when the same vertex of face used multiple times
	CorrectRawSequence();
	EraseClearConfluent( fusionDistance );
}


int cMeshContainer::Insert(
	int a, int b, float t,
	cList< int >* beginRawFigures,
	cList< int >* beginRawForRemoved
) {
	const cVec3&  ac = GetPosition( a );
	const cVec3&  bc = GetPosition( b );
	const cVec3  m3 = cLineF3::Point( ac, bc, t );

	return Insert( a, b, m3, beginRawFigures, beginRawForRemoved );
}


int cMeshContainer::Insert(
	int a, int b, const cVec3& m,
	cList< int >* beginRawFigures,
	cList< int >* beginRawForRemoved
) {
	// search the edge 'ab' into the mesh and insert point 'm'
	const int  found = m_Positions.IndexOf( m );
	const int  added = (found == -1) ? m_Positions.Add( m ) : found;
	bool  inserted = false;
	const int  count = m_Raw.Count();
	for (int i = 0; i < count; ++i) {
		const int  n = m_Raw[ i ][ 0 ];
		// skip points
		if (n > 1) {
			for (int j = 0; j < n; ++j) {
				const int  iRaw1 = i + 1 + j;
				const int  iRaw2 = i + 1 + (j + 1) % n;
				const int  ip1 = m_Raw[ iRaw1 ][ 0 ];
				const int  ip2 = m_Raw[ iRaw2 ][ 0 ];
				if ( ((a == ip1) && (b == ip2))
				  || ((b == ip1) && (a == ip2))
				  || ((a == ip1) && (added == ip2))
				  || ((b == ip1) && (added == ip2))
				  || ((a == ip2) && (added == ip1))
				  || ((b == ip2) && (added == ip1))
				) {
					// don't touch when point is present
					if ( (added == ip1) || (added == ip2) ) {
						continue;
					}

					// edge 'ab' is found!
					// # Don't remove a figure: add new with inserted point to
					//   the end of raw-mesh and set up numbers of verticies
					//   to 0 for old figure.
					const cVec3i  ip( added, m_Raw[ iRaw1 ][ 1 ], m_Raw[ iRaw1 ][ 2 ] );
					const int  beginRawFigure = m_Raw.Count();
					if ( beginRawFigures ) {
						beginRawFigures->Add( beginRawFigure );
					}
					const cVec3i  firstN = m_Raw[ i ] + cVec3i( 1, 0, 0 );
					const int  iFirstN = m_Raw.Add( firstN );
					for (int u = 0; u < n; ++u) {
						const int  iRawX1 = i + 1 + u;
						const int  iRawX2 = i + 1 + (u + 1) % n;
						const cVec3i&  source = m_Raw[ iRawX1 ];
						m_Raw.Add( source );
						if (iRaw2 == iRawX2) {
							if (source[ 0 ] != ip[ 0 ]) {
								m_Raw.Add( ip );
							} else {
								// we don't add a dublicates: fix count
								m_Raw[ iFirstN ][ 0 ]--;
							}
						}
					} // for (int u = 0; ...

					// fake-remove a figure: clear confluent
					const cVec3i&  firstV = m_Raw[ i + 1 ];
					for (int u = 1; u < n; ++u) {
						const int  iRawX1 = i + 1 + u;
						m_Raw[ iRawX1 ] = firstV;
					}
					if ( beginRawForRemoved ) {
						beginRawForRemoved->Add( i );
					}

					inserted = true;

					// shift for added 'i'
					// #! Don't skip 'for (i)', because a mesh consist of
					//    separate edges by every faces.
					//    See cMeshContainerTest.InsertA_X() for example.
					// skip 'j' loops
					break;
				} // if ( ((a == ip1) && ...
			} // for (int j = 0; j < n; ++j)
		} // if (n > 1)
		i += n;
	} // for (int i = 0; i < count; ++i)

	if ( !inserted && (found == -1) ) {
		m_Positions.RemoveLast();
		return -1;
	}

	return added;
}


void cMeshContainer::Insert( const figureI_t& figure, int* beginRawFigure ) {
	if ( figure.IsEmpty() ) {
		throw comms::Exception( "Attempt add an empty figure (set of vertices)." );
	}

	cList< cVec3i >  iraw;
	iraw.Add( cVec3i( figure.Count(), 0, 0 ) );
	const int  posCount = GetPositions().Count();
	for (int k = 0; k < figure.Count(); ++k) {
		const int  vi = figure[ k ];
		if (vi < 0) {
			throw comms::Exception( "Index of vertex is incorrect." );
		}
		if (vi >= posCount) {
			throw comms::Exception( "Index of vertex is out of bound." );
		}
		iraw.Add( cVec3i( vi, 0, 0 ) );
	} // for (int k = 0; k < figure.Count(); ++k)

	if ( beginRawFigure ) {
		*beginRawFigure = GetRaw().Count();
	}
	GetRaw().AddRange( iraw );
}


cMeshContainer::figureI_t cMeshContainer::Insert( int a, int* beginRawFigure ) {
	figureI_t  figure;
	figure.Add( a );
	Insert( figure, beginRawFigure );
	return figure;
}


cMeshContainer::figureI_t cMeshContainer::Insert( int a, int b, int* beginRawFigure ) {
	figureI_t  figure;
	figure.Add( a );
	figure.Add( b );
	Insert( figure, beginRawFigure );
	return figure;
}


cMeshContainer::figureI_t cMeshContainer::Insert( int a, int b, int c, int* beginRawFigure ) {
	figureI_t  figure;
	figure.Add( a );
	figure.Add( b );
	figure.Add( c );
	Insert( figure, beginRawFigure );
	return figure;
}


cMeshContainer::figureI_t cMeshContainer::Insert( int a, int b, int c, int d, int* beginRawFigure ) {
	figureI_t  figure;
	figure.Add( a );
	figure.Add( b );
	figure.Add( c );
	figure.Add( d );
	Insert( figure, beginRawFigure );
	return figure;
}


void cMeshContainer::Insert( const figures_t& figures, float fusionDistance ) {
	for (int k = 0; k < figures.Count(); ++k) {
		Insert( figures[ k ] );
	}
	if ( fusionDistance >= 0.0f ) {
		MergeFaces( fusionDistance );
	}
}


void cMeshContainer::Insert( const figure_t& figure, figureI_t* figureI ) {
	if ( figure.IsEmpty() ) {
		throw comms::Exception( "Attempt add an empty figure (set of positions)." );
	}

	figureI_t* const  fi = figureI ? figureI : new figureI_t;
	for (int k = 0; k < figure.Count(); ++k) {
		const cVec3&  v = figure[ k ];
		int  vi = GetVertex( v );
		if (vi == -1) {
			vi = GetPositions().Add( v );
		}
		fi->Add( vi );
	} // for (int k = 0; k < figure.Count(); ++k)

	Insert( *fi );
	if ( !figureI ) {
		// @see Declaration of 'fi'.
		delete fi;
	}
}


cMeshContainer::figure_t cMeshContainer::Insert( const cVec3& a, int* ai ) {
	figure_t  figure;
	figure.Add( a );
	figureI_t  figureI;
	Insert( figure, &figureI );
	if ( ai ) { *ai = figureI[ 0 ]; }
	return figure;
}


cMeshContainer::figure_t cMeshContainer::Insert(
	const cVec3& a, const cVec3& b,
	int* ai, int* bi
) {
	figure_t  figure;
	figure.Add( a );
	figure.Add( b );
	figureI_t  figureI;
	Insert( figure, &figureI );
	if ( ai ) { *ai = figureI[ 0 ]; }
	if ( bi ) { *bi = figureI[ 1 ]; }
	return figure;
}


cMeshContainer::figure_t cMeshContainer::Insert(
	const cVec3& a, const cVec3& b, const cVec3& c,
	int* ai, int* bi, int* ci
) {
	figure_t  figure;
	figure.Add( a );
	figure.Add( b );
	figure.Add( c );
	figureI_t  figureI;
	Insert( figure, &figureI );
	if ( ai ) { *ai = figureI[ 0 ]; }
	if ( bi ) { *bi = figureI[ 1 ]; }
	if ( ci ) { *ci = figureI[ 2 ]; }
	return figure;
}


cMeshContainer::figure_t cMeshContainer::Insert(
	const cVec3& a, const cVec3& b, const cVec3& c, const cVec3& d,
	int* ai, int* bi, int* ci, int* di
) {
	figure_t  figure;
	figure.Add( a );
	figure.Add( b );
	figure.Add( c );
	figure.Add( d );
	figureI_t  figureI;
	Insert( figure, &figureI );
	if ( ai ) { *ai = figureI[ 0 ]; }
	if ( bi ) { *bi = figureI[ 1 ]; }
	if ( ci ) { *ci = figureI[ 2 ]; }
	if ( di ) { *di = figureI[ 3 ]; }
	return figure;
}


void cMeshContainer::Insert( const cMeshContainer& b ) {
	const auto&  raw = b.GetRaw();
	const VecArray& positions = b.GetPositions();
	for (int i = 0;  i < raw.Count();  ++i) {
		const int  n = raw[ i ][ 0 ];
		figure_t  figure;
		for (int j = 0; j < n; ++j) {
			const int  iRaw = i + j + 1;
			const int  ip = raw[ iRaw ][ 0 ];
			const cVec3&  p = positions[ ip ];
			figure.Add( p );
		} // for (int j = 0; j < n; ++j)
		Insert( figure );
		i += n;
	} // for (int i = 0;  i < raw.Count();  ++i)
}


bool cMeshContainer::IsClearConfluent( const figure_t& figure, float tolerance ) const {
	const int  beginRawFigure = Find( figure );
	return IsClearConfluent( beginRawFigure, tolerance );
}


bool cMeshContainer::IsClearConfluent( int beginRawFigure, float tolerance ) const {
	assert( beginRawFigure >= 0 );
	assert( tolerance >= 0.0f );

	const int  i = beginRawFigure;
	const int  n = m_Raw[ i ][ 0 ];
	if ((n == 1) || (n == 2)) {
		// # All lines and points are confluent figure.
		return true;
	}
	if (n > 2) {
		for (int j = 0; j < n; ++j) {
			const int  aiRaw = i + j + 1;
			const int  biRaw = i + 1 + (j + 1) % n;
			const int  ciRaw = i + 1 + (j + 2) % n;
			const int  ai = m_Raw[ aiRaw ][ 0 ];
			const int  bi = m_Raw[ biRaw ][ 0 ];
			const int  ci = m_Raw[ ciRaw ][ 0 ];
			const cVec3&  a = m_Positions[ ai ];
			const cVec3&  b = m_Positions[ bi ];
			const cVec3&  c = m_Positions[ ci ];
			const cVec3  ac = cVec3::Normalize2( c - a );
			const cVec3  bc = cVec3::Normalize2( b - a );
			const cVec3  cross = cVec3::Abs( cVec3::Cross( ac, bc ) );
			static const float  P = ::std::numeric_limits< float >::epsilon();
			if ( (cross.x <= (tolerance + P))
			  && (cross.y <= (tolerance + P))
			  && (cross.z <= (tolerance + P))
			) {
				continue;
			}
			return false;
		} // for (int j = 0; j < n; ++j)
		return true;
	} // if (n > 2)

	return false;
}


bool cMeshContainer::IsClearConfluentByIndexOnly( int beginRawFigure ) const {
	assert( beginRawFigure >= 0 );

	const int  i = beginRawFigure;
	const int  n = m_Raw[ i ][ 0 ];
	if ((n == 1) || (n == 2)) {
		// # All lines and points are confluent figure.
		return true;
	}
	if (n > 2) {
		for (int j = 0; j < n; ++j) {
			const int  aiRaw = i + 1 + j;
			const int  biRaw = i + 1 + (j + 1) % n;
			const int  ai = m_Raw[ aiRaw ][ 0 ];
			const int  bi = m_Raw[ biRaw ][ 0 ];
			if (ai == bi) {
				continue;
			}
			return false;
		} // for (int j = 0; j < n; ++j)
		return true;
	} // if (n > 2)

	return false;
}


bool cMeshContainer::IsClockwiseOrder( int beginRawFigure, const cVec3& observer ) const {
	const int  i = beginRawFigure;
	if ((i < 0) || (i >= GetRaw().Count())) {
		throw comms::Exception( "'beginRawFigure' must be in the diapason [ 0; GetRaw().Count() )." );
	}
	const int  n = m_Raw[ i ][ 0 ];
	// points
	if (n == 1) { return false; }

	// calc normal of face
	const cVec3  normal = GetFaceNormal( i );
	bool  r = false;
	for (int j = 0; j < n; ++j) {
		const int  iRaw = i + j + 1;
		const int  ai = m_Raw[ iRaw ][ 0 ];
		const cVec3&  a = m_Positions[ ai ];
		const float  dot = cVec3::Dot( normal, a - observer );
		r = (dot < 0);
		break;
	} // for (int j = 0; j < n; ++j)

	return r;
}


void cMeshContainer::RemoveUnusedVerts(cList<int>* Encoding){
	UnlimitedBitset bs;
	int pos = 0;
	for (int i = 0; i<m_Raw.Count(); i++){
		int n = m_Raw[i][0];
		int ob = m_Raw[i][2];
		if (ob != -1){
			if (i != pos){
				for (int k = 0; k <= n; k++){
					m_Raw[pos + k] = m_Raw[i + k];
				}
			}
			pos += n + 1;
		}
		i += n;
	}
	if (pos < m_Raw.Count()){
		m_Raw.RemoveAt(pos, m_Raw.Count() - pos);
	}
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		for(int j=0;j<n;j++){
			bs.set(m_Raw[i+j+1][0],true);
		}
		i+=n;
	}
	cList<int> cod;
	cod.Add(-1,m_Positions.Count());
	int j=0;
	bool col = m_VertexColor.Count() > 0;
	for (int i = 0; i < m_Positions.Count(); i++) {
		if (bs.get(i)) {
			if (i != j) {
				m_Positions[j] = m_Positions[i];
				if (i < m_VertexColor.Count()) {
					m_VertexColor[j] = m_VertexColor[i];
				}
				if (i < m_VertexSpecular.Count()) {
					m_VertexSpecular[j] = m_VertexSpecular[i];
				}
				if (i < m_VertexEmissive.Count()) {
					m_VertexEmissive[j] = m_VertexEmissive[i];
				}
			}
			cod[i] = j++;
		}
	}
	if(j<m_Positions.Count()){
		m_Positions.RemoveAt(j,m_Positions.Count()-j);
	}
	if(j<m_VertexColor.Count()){
		m_VertexColor.RemoveAt(j,m_VertexColor.Count()-j);
	}
	if(j<m_VertexSpecular.Count()){
		m_VertexSpecular.RemoveAt(j,m_VertexSpecular.Count()-j);
	}
	if(j<m_VertexEmissive.Count()){
		m_VertexEmissive.RemoveAt(j,m_VertexEmissive.Count()-j);
	}
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		for(int j=0;j<n;j++){
			m_Raw[i+j+1][0]=cod[m_Raw[i+j+1][0]];
		}
		if(n==3){
			int v1=m_Raw[i+1][0];
			int v2=m_Raw[i+2][0];
			int v3=m_Raw[i+3][0];
			assert(v1!=v2 && v2!=v3 && v3!=v1);
		}
		if (n == 0) {
			m_Raw.RemoveAt(i, 1);
			i--;
		}
		i+=n;
	}
	if (Encoding){
		Encoding->Copy(cod);
	}
}

void cMeshContainer::RemoveNonManifoldFaces() {
	CreateFone();
	int p = 0;
	for(int i=0;i<m_Raw.Count();i++) {
		int n = m_Raw[i][0];
		bool del = false;
		for (int k = 1; k < n; k++) {
			DWORDS2 D(m_Raw[i + 1 + k][0], m_Raw[i + 1 + (k + 1) % n][0]);
			if (fone.size(D) > 2) del = true;
		}
		if (!del) {
			for (int k = 0; k <= n; k++) {
				m_Raw[p++] = m_Raw[i + k];
			}			
		}
		i += n;
	}
	if(p<m_Raw.Count()) {
		m_Raw.RemoveAt(p, m_Raw.Count() - p);
		fone.reset();
	}
}

void cMeshContainer::RemoveZeroEdgesFaces() {
	cList<int> cod;
	cod.Add(-1, m_Positions.Count());
	int p = 0;
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		bool del = false;
		for (int k = 1; k < n; k++) {
			DWORDS2 D(m_Raw[i + 1 + k][0], m_Raw[i + 1 + (k + 1) % n][0]);
			if ((m_Positions[D.V1] - m_Positions[D.V2]).IsZero(1e-16f)) {
				if (D.V1 != D.V2)cod[D.V2] = D.V1;
				del = true;
			}
		}
		if (!del) {
			for (int k = 0; k <= n; k++) {
				m_Raw[p++] = m_Raw[i + k];
			}
		}
		i += n;
	}
	if (p < m_Raw.Count()) {
		m_Raw.RemoveAt(p, m_Raw.Count() - p);
		fone.reset();
		int p = 0;
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			for (int k = 0; k < n; k++) {
				int& v = m_Raw[i + 1 + k][0];
				do {
					const int v1=cod[v];
					if (v1 != -1)v = v1;
					else break;
				} while (true);
			}
			i += n;
		}
		RemoveUnusedVerts();
	}
}

void cMeshContainer::RemoveEmptyFaces() {
	int p = 0;
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		if(n>0) {
			for (int k = 0; k <= n; k++) {
				m_Raw[p++] = m_Raw[i + k];
			}
		}
		i += n;
	}
	if(p< m_Raw.Count()) {
		m_Raw.RemoveAt(p, m_Raw.Count() - p);
	}
}

void cMeshContainer::Rasterize(const std::function<void(int, int, float)>& fn) {
	RawArray r;
	Triangulate(r);
	for(int i=0;i<r.Count();i++) {
		int n = r[i][0];
		if (n == 3) {
			rasterizeGeo(m_Positions[r[i + 1][0]], m_Positions[r[i + 2][0]], m_Positions[r[i + 3][0]], fn);
		}
		i += n;
	}
}

void cMeshContainer::Rasterize(const cMat4& transform, const std::function<void(int, int, float)>& fn) {
	auto p = m_Positions;
	std_for(i, p.Count()) {
		p[i].TransformCoordinate(transform);
	}std_for_end;
	RawArray r;
	Triangulate(r);
	for (int i = 0; i < r.Count(); i++) {
		int n = r[i][0];
		if (n == 3) {
			rasterizeGeo(p[r[i + 1][0]], p[r[i + 2][0]], p[r[i + 3][0]], fn);
		}
		i += n;
	}
}

void cMeshContainer::Rasterize(const cMat4& transform, int Lx, int Ly, float* zbuffer) {
	int n = Lx * Ly;
	std_for(i, n) {
		zbuffer[i] = -FLT_MAX;
	}std_for_end;
	Rasterize(transform,
		[&](int x, int y, float z) {
			if (x >= 0 && y >= 0 && x < Lx && y < Ly) {
				float& zb = zbuffer[x + y * Lx];
				zb = std::max(zb, z);
			}
		}
	);
}

bool cMeshContainer::PerformBooleanOp( cMeshContainer& src1, cMeshContainer& src2,
                                       int operation,
                                       cList< ::std::pair< comms::cVec3, comms::cVec3 > >* dividers
) {
	AABoundBox ab;
	ab.SetEmpty();
	int np=src1.GetPositions().Count();
	for(int i=0;i<np;i++)ab.AddPoint(src1.GetPositions()[i]);
	np=src2.GetPositions().Count();
	for(int i=0;i<np;i++)ab.AddPoint(src2.GetPositions()[i]);
	float scl = ab.GetDiagonal();
	if(scl>0){
		scl=1024/scl;
		BasicMesh m1;
		BasicMesh m2;
		MC2BM(src1,m1,false,scl);
		MC2BM(src2,m2,false,scl);
		if(operation==0){
			m1.InvertFaces();
			m2.InvertFaces();
		}
		if(operation==1){
			m2.InvertFaces();
		}
		BasicMesh m3;
        // TODO Oh, see BM2MC() below.
        static const float  kk = 2048.0f * scl;
        m3.boolean( m1, m2, false, operation != 0, false, dividers, kk );
		if(operation==0)m3.InvertFaces();
		Clear();
		BM2MC(m3,*this,false,1.0/scl);
	}
	return true;
}

Vector3D cupts[8] = { Vector3D(-1, -1, 1), Vector3D(-1, 1, 1), Vector3D(1, 1, 1), Vector3D(1, -1, 1), Vector3D(-1, -1, -1), Vector3D(-1, 1, -1), Vector3D(1, 1, -1), Vector3D(1, -1, -1) };
int curaw[24] = { 1, 4, 3, 2, 1, 5, 8, 4, 2, 6, 5, 1, 3, 7, 6, 2, 4, 8, 7, 3, 6, 7, 8, 5 };

void cMeshContainer::CreateCube(const Vector3D& Sides){
	int p0=GetPositions().Count();
	for(int i=0;i<8;i++){
		Vector3D p=cupts[i];
		p*=Sides/2.0;
		GetPositions().Add(p);
	}
	auto& raw=GetRaw();
	for(int i=0;i<6;i++){
		cVec3i v(4,0,0);
		raw.Add(v);
		for(int k=0;k<4;k++){
			cVec3i v(curaw[i*4+3-k]-1+p0,-1,-1);
			raw.Add(v);
		}
	}		
	SetDefaultObjMtl();
}
void cMeshContainer::SetDefaultObjMtl(){
	if(GetObjects().Count()==0){
		comms::cObject ob;
		GetObjects().Add(ob);
		GetObjects().GetLast().Name="Object";
	}
	if(GetMaterials().Count()==0){
		comms::cSurface mt;
		GetMaterials().Add(mt);
		GetMaterials().GetLast().Name="Surface";
	}
}
Vector3D clpts[14]={
	cVec3(1.0,1.0,0.0),
	cVec3(0.5,1.0,0.86602540),
	cVec3(-0.5,1.0,0.86602540),
	cVec3(-1.0,1.0,0.0),
	cVec3(-0.5,1.0,-0.86602540),
	cVec3(0.5,1.0,-0.86602540),
	cVec3(1.0,-1.0,0.0),
	cVec3(0.5,-1.0,0.86602540),
	cVec3(-0.5,-1.0,0.86602540),
	cVec3(-1.0,-1.0,0.0),
	cVec3(-0.5,-1.0,-0.86602540),
	cVec3(0.5,-1.0,-0.86602540),
	cVec3(0.0,1.0,0),
	cVec3(0.0,-1.0,0.0)
};

int clraw[48]={1,6,13,2, 1,7,12,6, 2,8,7,1, 2,13,4,3, 3,9,8,2, 4,10,9,3, 4,13,6,5, 5,11,10,4, 6,12,11,5, 8,14,12,7, 9,10,14,8, 11,12,14,10};
void cMeshContainer::CreateQuadCylinder(const Matrix4D& M1,float quant,float rtop,float rbottom,float Height){
	Matrix4D M=M1;
	int p0=GetPositions().Count();
	for(int i=0;i<14;i++){
		Vector3D p=clpts[i];
		/*if(p.y<0){
			p.x*=rtop;
			p.y*=rtop;
		}
		if(p.y>0){
			p.x*=rbottom;
			p.y*=rbottom;
		}
		p.y*=Height/2.0;*/
		GetPositions().Add(p);
	}
	auto& raw=GetRaw();
	for(int i=0;i<12;i++){
		cVec3i v(4,0,0);
		raw.Add(v);
		for(int k=0;k<4;k++){
			cVec3i v(clraw[i*4+3-k]-1+p0,-1,-1);
			raw.Add(v);
		}
	}		
	SetDefaultObjMtl();
	float sxz=(rtop+rbottom)/2.0;
	float sy=Height;
	Height=1.0;
	rtop/=sxz;
	rbottom/=sxz;
	Matrix4D M0=Matrix4D::Scaling(Vector3D(sxz,sy,sxz));
	M0*=M;
	M=M0;
	QuadQuantSubd(M,quant,0.2);
	Matrix4D mi=M;
	mi.Invert();
	VecArray& pos=GetPositions();
	int np=pos.Count();
	float rm=0;
	float ymi=FLT_MAX;
	float yma=-FLT_MAX;
	for(int j=0;j<np;j++){
		Vector3D p=pos[j];
		float dx=__abs(p.x);
		float dy=__abs(p.z);
		float k=sqrt(dx*dx+dy*dy);
		if(k>0){
			dx/=k;
			dy/=k;
			float a0;
			float a=atan2(dy,dx);
			if(a<c_PI/3){
				a0=c_PI/6;
			}else{
				a0=c_PI/2;
			}
			k=sqrt(3.0f)/2.0/cos(a-a0);
			p.x/=k;
			p.z/=k;
			float t=(p.y+1)/2.0;
			float k=rbottom+t*(rtop-rbottom);
			p.x*=k;
			p.z*=k;
		}
		p.y*=Height/2;
		p.TransformCoordinate(M);
		pos[j]=p;
		//AddDbgPoint(p,0xFF00FF00);
	}
	//cMeshIO::SaveMesh(*this,"cyl.obj");
}
void cMeshContainer::CreateQuadCylinder(float quant,Vector3D PTop,Vector3D PBottom,float rTop,float rBottom){
	Vector3D Y=(PTop-PBottom).ToNormal();
	Vector3D X=Vector3D::Cross(Y,Vector3D::AxisX);
	if(X.Length()<0.1)X=Vector3D::Cross(Y,Vector3D::AxisZ);
	X.Normalize();
	Vector3D Z=Vector3D::Cross(X,Y);
	Z.Normalize();
	float L=PTop.distance(PBottom);
	Matrix4D Tr=Matrix4D::Translation(Vector3D(0.0,L/2.0,0.0));
	Matrix4D R=Matrix4D::Identity;
	R.SetRow0(Vector4D(X,0));
	R.SetRow1(Vector4D(Y,0));
	R.SetRow2(Vector4D(Z,0));
	Tr*=R;
	Tr*=Matrix4D::Translation(PBottom);
	CreateQuadCylinder(Tr,quant,rTop,rBottom,L);
	//AddDbgLine(PTop,PBottom,0xFFFF0000,0xFF00FF00);
}
struct _bi_int{
	_bi_int(){
		V1 = V2 = -1;
	}
	int V1, V2;
};
bool cMeshContainer::OptimizeTriangularMesh(){
	cList<int> valence;
	_bi_int bb;
	cList<_bi_int> edge;
	CreateFone();
	CalcNormals();
	int nv = m_Positions.Count();
	valence.Add(0, nv);
	edge.Add(bb, nv);
	for (int i = 0; i < m_Raw.Count(); i++){
		int n = m_Raw[i][0];
		if (n == 3){
			for (int j = 0; j < n; j++){
				int v1 = m_Raw[i + 1 + j][0];
				int v2 = m_Raw[i + 1 + (j + 1) % 3][0];
				valence[v1]++;
				if (fone.size(DWORDS2(v1, v2)) == 1){
					if (edge[v1].V1 == -1)edge[v1].V1 = v2;
					else edge[v1].V2 = v2;
					if (edge[v2].V1 == -1)edge[v2].V1 = v1;
					else edge[v2].V2 = v1;
				}
			}
		}
		i += n;
	}
	for (int i = 0; i < nv; i++){
		if (edge[i].V1 != -1 && edge[i].V2 != -1){
			Vector3D vc = m_Positions[i];
			Vector3D v1 = m_Positions[edge[i].V1];
			Vector3D v2 = m_Positions[edge[i].V2];
			v1 -= vc;
			v2 -= vc;
			v1.Normalize();
			v2.Normalize();
			float dp = v1.dot(v2)*0.9999;
			fclampM11(dp);
			float a = acosf(dp) * 180 / c_PI;
			if (a > 170)valence[i] += 3;
			else if (a > 90)valence[i] += 4;
			else valence[i] += 5;
		}
	}
	bool something = false;
	bool ch;
	do{
		ch = false;
		scan(fone, int* pv, DWORDS2* d){
			DWORDS2 D = *d;
			if (FlipEdge(D.V1, D.V2, valence.ToPtr())){
				ch = true;
				something = true;
			}
		}scan_end;
	} while (ch);
	return something;
}
float square(Vector3D v1,Vector3D v2, Vector3D v3){
	return Vector3D::Cross(v2 - v1, v3 - v1).Length() / 2.0;
}
static int nz(int x){
	return x == 0 ? 1 : 0;
}
bool cMeshContainer::FlipEdge(int V1, int V2, int* valence){
	int ts[2];
	int nt=0;
	cVec3i opp[2];
	cVec3i v12[2];
	scan_key(fone, DWORDS2(V1, V2), int* pf){
		if (nt < 2){
			if (m_Raw[*pf][0] == 3){
				for (int k = 0; k < 3; k++){
					int v = m_Raw[*pf + 1 + k][0];
					if (v != V1 && v != V2){
						opp[nt] = m_Raw[*pf + 1 + k];
						v12[nt] = m_Raw[*pf + 1 + (k + 1) % 3];
						break;
					}
				}
				ts[nt++] = *pf;
			}
			else return false;
		}
		else return false;
	}scan_end;
	if (nt == 2){
		if (valence){
			int vo1 = valence[opp[0][0]] - 6;
			int vo2 = valence[opp[1][0]] - 6;
			int vn1 = valence[v12[0][0]] - 6;
			int vn2 = valence[v12[1][0]] - 6;
			int s1 = nz(vo1) + nz(vo2) + nz(vn1) + nz(vn2);
			vo1++;
			vo2++;
			vn1--;
			vn2--;
			int s2 = nz(vo1) + nz(vo2) + nz(vn1) + nz(vn2);

			Vector3D po1 = m_Positions[opp[0][0]];
			Vector3D po2 = m_Positions[opp[1][0]];
			Vector3D pn1 = m_Positions[v12[0][0]];
			Vector3D pn2 = m_Positions[v12[1][0]];

			if (s1 == s2){
				float L1 = po1.distance(po2);
				float L2 = pn1.distance(pn2);
				if (L2 <= L1)return false;
			}else
			if (s2 <= s1)return false;
			
			float sq1 = square(po1, pn1, po2);
			float sq2 = square(po1, pn2, po2);
			float sm = (sq1 + sq2)*0.001;
			if (sq1 < sm)return false;
			if (sq2 < sm)return false;
		}
		m_Raw[ts[0] + 1] = opp[0];
		m_Raw[ts[0] + 2] = v12[0];
		m_Raw[ts[0] + 3] = opp[1];

		m_Raw[ts[1] + 1] = opp[1];
		m_Raw[ts[1] + 2] = v12[1];
		m_Raw[ts[1] + 3] = opp[0];

		fone.del(DWORDS2(V1, V2));
		fone.del(DWORDS2(V1, V2));
		fone.del_elm(DWORDS2(opp[0][0],v12[1][0]),ts[0]);
		fone.del_elm(DWORDS2(opp[1][0], v12[0][0]), ts[0]);

		fone.add_quick(DWORDS2(opp[0][0], opp[1][0]), ts[0]);
		fone.add_quick(DWORDS2(opp[0][0], opp[1][0]), ts[1]);

		fone.add_quick(DWORDS2(opp[0][0], v12[1][0]), ts[1]);
		fone.add_quick(DWORDS2(opp[1][0], v12[0][0]), ts[0]);
		valence[opp[0][0]]++;
		valence[opp[1][0]]++;
		valence[v12[0][0]]--;
		valence[v12[1][0]]--;
		return true;
	}
	return false;
}

void cMeshContainer::QuadQuantSubd(const cMat4& M,float quant,float dotp, ::std::function<int(int, int)>* divider){
	::QuadQuantSubd(*this,M,quant,dotp, divider);
}

void cMeshContainer::TriSubd(int N, SubdSnapEdgeCallback* dive, SubdSnapMiddlePointCallback* divm, void* context){
	Triangulate();
	CalcNormals();
	uni_hash<int, tri_DWORD> hash;
	for (int i = 0; i < m_Raw.Count(); i++){
		int n = m_Raw[i][0];
		for (int j = 0; j < n; j++){
			int vc = m_Raw[i + j + 1][0];
			int vn = m_Raw[i + (j + 1) % n + 1][0];
			if (vc > vn){
				cMath::Swap(vc, vn);
			}
			Vector3D pc = m_Positions[vc];
			Vector3D pn = m_Positions[vn];
			Vector3D nc = m_Normals[vc];
			Vector3D nn = m_Normals[vn];
			//divide edges
			for (int k = 0; k < N; k++){
				float t = float(k + 1) / (N + 1);
				Vector3D P = pc + (pn - pc)*t;
				Vector3D N = (nc + (nn - nc)*t).ToNormal();
				tri_DWORD tt(vc, vn, k);
				if (!hash.get(tt)){
					int vm = m_Positions.Add(P);
					m_Normals.Add(N);
					hash.add_quick(tt, vm);
					if (dive){
						dive(context, P, N, vm, vc, vn, k);
						m_Positions.GetLast() = P;
					}
				}
			}
		}
		i += n;
	}
	cList<int> tids;
	for (int i = 0; i <= N + 1; i++){
		for (int j = 0; j <= i; j++){
			tids.Add(0);
		}
	}
	comms::RawArray raw1;
	for (int i = 0; i < m_Raw.Count(); i++){
		int n = m_Raw[i][0];
		if (n==3){
			int v[3] = { m_Raw[i + 1][0], m_Raw[i + 2][0], m_Raw[i + 3][0] };
			Vector3D p[3] = { m_Positions[v[0]], m_Positions[v[1]], m_Positions[v[2]] };
			Vector3D n[3] = { m_Normals[v[0]], m_Normals[v[1]], m_Normals[v[2]] };
			//filling corvers
			tids[0] = v[0];
			tids[N + 1] = v[1];
			tids.GetLast() = v[2];
			//filling edges
			for (int j = 1; j <= N; j++){
				int k = j;
				tri_DWORD T(v[0], v[1], k - 1);
				if (T.V1 > T.V2){
					T.V3 = N - 1 - T.V3;
					cMath::Swap(T.V1, T.V2);
				}
				tids[j] = *hash.get(T);

				int jL = j*N + (5 - j)*j / 2;
				int jR = jL + N + 1 - j;

				tri_DWORD T1(v[1], v[2], k - 1);
				if (T1.V1 > T1.V2){
					T1.V3 = N - 1 - T1.V3;
					cMath::Swap(T1.V1, T1.V2);
				}
				tids[jR] = *hash.get(T1);

				tri_DWORD T2(v[2], v[0], N - k);
				if (T2.V1 > T2.V2){
					T2.V3 = N - 1 - T2.V3;
					cMath::Swap(T2.V1, T2.V2);
				}
				tids[jL] = *hash.get(T2);

			}
			//filling inner space
			p[1] -= p[0];
			p[2] -= p[0];
			n[1] -= n[0];
			n[2] -= n[0];
			for (int j = 1; j < N; j++){
				for (int q = 1; q < N + 1 - j; q++){
					int jin = j*N + (5 - j)*j / 2 + q;
					float uu = float(q) / (N + 1);
					float vv = float(j) / (N + 1);
					Vector3D cp = p[0] + p[1] * uu + p[2] * vv;
					Vector3D cn = n[0] + n[1] * uu + n[2] * vv;
					tids[jin] = m_Positions.Add(cp);
					m_Normals.Add(cn);
					if (divm){
						divm(context, cp, cn, tids[jin], v[0], v[1], v[2], uu, vv);
						m_Positions.GetLast() = cp;
					}
				}
			}
			//adding triangles
			for (int j = 0; j <= N; j++){
				int nm = N - j;
				for (int k = 0; k <= nm; k++){
					int j0 = j*N + (5 - j)*j / 2 + k;
					int j1 = j0 + 1;
					int j2 = (j + 1)*N + (4 - j)*(j + 1) / 2 + k;
					int j3 = j2 + 1;
					raw1.Add(m_Raw[i]);
					raw1.Add(cVec3i(tids[j0], -1, -1));
					raw1.Add(cVec3i(tids[j1], -1, -1));
					raw1.Add(cVec3i(tids[j2], -1, -1));
					if (k < nm){
						raw1.Add(m_Raw[i]);
						raw1.Add(cVec3i(tids[j1], -1, -1));
						raw1.Add(cVec3i(tids[j3], -1, -1));
						raw1.Add(cVec3i(tids[j2], -1, -1));
					}
				}
			}
		}
		i += n;
	}
	m_Raw = raw1;
}


void cMeshContainer::Transform(const cMat4& M){
	for(int i=0;i<m_Positions.Count();i++){
		m_Positions[i].TransformCoordinate(M);
	}
}


cVec3 cMeshContainer::GetFaceCoord( int pos ) const{
	const int  i = pos;
	if ((i < 0) || (i >= GetRaw().Count())) {
		throw comms::Exception( "'pos' must be in the diapason [ 0; GetRaw().Count() )." );
	}

	cVec3  r = cVec3::Zero;
	const int  n = m_Raw[ i ][ 0 ];
	if ( n < 1) {
		throw comms::Exception( "Incorrect face in this mesh." );
	}

	// calc middle of face
	for (int j = 0; j < n; ++j) {
		const int  iRaw = i + j + 1;
		const int  ai = m_Raw[ iRaw ][ 0 ];
		const cVec3&  a = m_Positions[ ai ];
		r += a;
	} // for (int j = 0; j < n; ++j)
	r /= n;

	return r;
}


cVec3 cMeshContainer::GetFaceNormal(int pos) const{
	// @todo Optimize this!
	int n=m_Raw[pos][0];
	if(n==3){
		Vector3D v=m_Positions[m_Raw[pos+1][0]];
		Vector3D vn=m_Positions[m_Raw[pos+2][0]];
		Vector3D vnn=m_Positions[m_Raw[pos+3][0]];
		Vector3D N=Vector3D::Cross(v-vn,vnn-vn);
		return N.ToNormal();
	}else{
		cVec3 N(0);
		for(int i=0;i<n;i++){
			Vector3D v=m_Positions[m_Raw[i+pos+1][0]];
			Vector3D vn=m_Positions[m_Raw[(i+1)%n+pos+1][0]];
			Vector3D vnn=m_Positions[m_Raw[(i+2)%n+pos+1][0]];
			N+=Vector3D::Cross(v-vn,vnn-vn);
		}
		N.Normalize();
		return N;
	}
}


void cMeshContainer::SplitHardsurface(float angcos){
	uni_hash<int,DWORDS2> fone;
	fone.set_table_size(m_Raw.Count()/2+1);
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		for(int j=0;j<n;j++){
			int v1=m_Raw[i+j+1][0];
			int v2=m_Raw[i+1+(j+1)%n][0];
			fone.add_quick(DWORDS2(v1,v2),i);
		}
		i+=n;
	}
	uni_hash<int,DWORDS2> split;
	scan(fone,int* pf,DWORDS2* d){
		if(fone.size(*d)==2){
			//detect sharp edges
			scan_key(fone,*d,int* nf){
				if(*nf>*pf){
					Vector3D n1=GetFaceNormal(*nf);
					Vector3D n2=GetFaceNormal(*pf);
					if(n1.dot(n2)<angcos){
						split.add_quick(*d,*nf);
					}
				}
			}scan_end;
		}
	}scan_end;
	//splitting on separate clusters
	int nr=m_Raw.Count();
	UnlimitedBitset touched;
	cList<int> order;
	int np0=m_Positions.Count();
	VecArray pos0;
	pos0.Copy(m_Positions);
	for(int i=0;i<nr;i++){
		int n=m_Raw[i][0];
		if(!touched.get(i)){
			int pt0=m_Positions.Count();
			m_Positions.AddRange(pos0);
			order.Clear();
			order.Add(i);
			touched.set(i,true);
			for(int k=0;k<order.Count();k++){
				int f=order[k];
				int n1=m_Raw[f][0];
				cVec3i rr=m_Raw[f];
				m_Raw.Add(rr);
				for(int j=0;j<n1;j++){
					cVec3i r1=m_Raw[f+j+1];
					r1[0]+=pt0;
					m_Raw.Add(r1);
					int v1=m_Raw[f+j+1][0];
					int v2=m_Raw[f+1+(j+1)%n][0];
					DWORDS2 d(v1,v2);
					if(split.get(d)==NULL){
						scan_key(fone,d,int* nf){
							if(*nf!=f && touched.get(*nf)==false){
								order.Add(*nf);
								touched.set(*nf,true);
							}
						}scan_end;
					}
				}
			}
		}
		i+=n;
	}
	m_Raw.RemoveAt(0,nr);
	RemoveUnusedVerts();
}
struct _opposite
{
	_opposite() {}
	_opposite(int m , int o , int v1 , int v2, int uv1, int uv2) : material(m) , object(o) , v1(v1) , v2(v2), uv1(uv1), uv2(uv2) {}
	int material;
	int object;
	int v1;
	int v2;
	int uv1;
	int uv2;
};
void cMeshContainer::SplitPositionsByNormals() {
	if (m_Normals.Count() > m_Positions.Count()) {
		WeldNormals();
		uni_hash<int, int> vnonv;
		uni_hash<_opposite, int> open;
		uni_hash<_opposite, bi_int> edlinks;
		BigDynArray<int> occupied;
		occupied.Add(-1, m_Positions.Count());
		VecArray newpos;
		newpos.Add(cVec3::Zero, m_Normals.Count());
		UnlimitedBitset pins;
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			for (int j = 0; j < n; j++) {
				int v = m_Raw[i + j + 1][0];
				int vn = m_Raw[i + j + 1][2];
				if (vn != -1) {
					pins.set(vn, true);
					auto& pv = occupied[v];
					if (pv == -1)pv = vn;
					else if (pv != vn) {
						vnonv.add_once(v, pv);
						vnonv.add_once(v, vn);
					}
				}
			}
			i += n;
		}
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			int mt = m_Raw[i][1];
			int ob = m_Raw[i][2];
			for (int j = 0; j < n; j++) {
				int v1 = m_Raw[i + j + 1][0];
				int v2 = m_Raw[i + (j + 1) % n + 1][0];
				int nv1 = m_Raw[i + j + 1][2];
				int nv2 = m_Raw[i + (j + 1) % n + 1][2];
				int uv1 = m_Raw[i + j + 1][1];
				int uv2 = m_Raw[i + (j + 1) % n + 1][1];
				bool s1 = vnonv.get(v1) != nullptr;
				bool s2 = vnonv.get(v2) != nullptr;
				if (s1 || s2) {
					edlinks.add_once(bi_int(v1, v2), _opposite(mt, ob, nv1, nv2, uv1, uv2));
					if (s1)pins.set(nv1, false);
					if (s2)pins.set(nv2, false);
				}
			}
			i += n;
		}
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			for (int j = 0; j < n; j++) {
				int& v1 = m_Raw[i + j + 1][0];
				int nv1 = m_Raw[i + j + 1][2];
				if (nv1 != -1) {
					newpos[nv1] = m_Positions[v1];
					v1 = nv1;
				}
			}
			i += n;
		}
		do {
			scan(edlinks, auto * po, auto * ped) {
				bi_int inv = bi_int(ped->V2, ped->V1);
				auto* ipo = edlinks.get(inv);
				if (ipo) {
					if (po->v2 == ipo->v1) {
						m_Raw.Add(cVec3i(3, ipo->material, ipo->object));
						m_Raw.Add(cVec3i(po->v2, po->uv2, po->v2));
						m_Raw.Add(cVec3i(po->v1, po->uv1, po->v1));
						m_Raw.Add(cVec3i(ipo->v2, ipo->uv2, ipo->v2));

						open.add_quick(ipo->v2, _opposite(ipo->material, ipo->object, ipo->v2, po->v1, ipo->uv2, po->uv1));
					}
					else
						if (po->v1 == ipo->v2) {
							m_Raw.Add(cVec3i(3, ipo->material, ipo->object));
							m_Raw.Add(cVec3i(po->v2, po->uv2, po->v2));
							m_Raw.Add(cVec3i(po->v1, po->uv1, po->v1));
							m_Raw.Add(cVec3i(ipo->v1, ipo->uv1, ipo->v1));

							open.add_quick(po->v2, _opposite(ipo->material, ipo->object, po->v2, ipo->v1, po->uv2, ipo->uv1));
						}
						else {
							m_Raw.Add(cVec3i(4, ipo->material, ipo->object));
							m_Raw.Add(cVec3i(po->v2, po->uv2, po->v2));
							m_Raw.Add(cVec3i(po->v1, po->uv1, po->v1));
							m_Raw.Add(cVec3i(ipo->v2, ipo->uv2, ipo->v2));
							m_Raw.Add(cVec3i(ipo->v1, ipo->uv1, ipo->v1));

							open.add_quick(ipo->v2, _opposite(ipo->material, ipo->object, ipo->v2, po->v1, ipo->uv2, po->uv1));
							open.add_quick(po->v2, _opposite(ipo->material, ipo->object, po->v2, ipo->v1, po->uv2, po->uv1));
						}
					edlinks.del(inv);
				}
				edlinks.del(*ped);
			}scan_end;
		} while (edlinks.size());
		
		do {
			cList<int> temp;
			scan(open, auto * po, auto * ped) {
				temp.Clear();
				temp.Add(*ped);
				int next = po->v2;
				temp.Add(next);
				int n = 0;
				do {
					auto* pnx = open.get(next);
					if (pnx) {
						temp.Add(pnx->v2);
						next = pnx->v2;
					}
				} while (n++ < 128 && temp[0] != temp.GetLast());
				if(temp[0] == temp.GetLast()){
					m_Raw.Add(cVec3i(temp.Count() - 1, po->material, po->object));
					for (int i = 0; i < temp.Count() - 1; i++){
						m_Raw.Add(cVec3i(temp[i], po->uv1, temp[i]));
					}
				}
				for(int i = 0; i < temp.Count(); i++) {
					open.del(temp[i]);
				}
			}scan_end;				
		} while (open.size());		
		newpos.moveTo(m_Positions);
		SmoothFast(0.01, 1, &pins, false);
	}
}

void cMeshContainer::CreateVnv(uni_hash<int, int>& vnv){
	vnv.set_table_size(m_Positions.Count() * 7);
	for (int i = 0; i < m_Raw.Count(); i++){
		int n = m_Raw[i][0];
		for (int j = 0; j < n; j++){
			int k = (j + 1) % n;
			int v1 = m_Raw[i + 1 + j][0];
			int v2 = m_Raw[i + 1 + k][0];
			vnv.add_uniq(v1, v2);
			vnv.add_uniq(v2, v1);
		}
		i += n;
	}
}
void cMeshContainer::CreateFone() const{
	int h=m_Raw.Count()+m_Positions.Count()*10;
	if(fone.size()==0 || h!=fone_hash){
		fone.reset();
		fone.set_table_size(m_Raw.Count()/2+1);
		for(int i=0;i<m_Raw.Count();i++){
			int n=m_Raw[i][0];
			for(int j=0;j<n;j++){
				int v1=m_Raw[i+j+1][0];
				int v2=m_Raw[i+1+(j+1)%n][0];
				fone.add_quick(DWORDS2(v1,v2),i);
			}
			i+=n;
		}
		fone_hash=h;
	}
}
void cMeshContainer::ClearFone() const{
	fone.reset();
	fone_hash=0;
}
void cMeshContainer::FindCorners(cList<int>& corners){
	CreateFone();
	uni_hash<DWORD,DWORD> vnv;
	scan(fone,int* pf,DWORDS2* d){
		if(fone.size(*d)==1){
			vnv.add_uniq(d->V1,d->V2);
			vnv.add_uniq(d->V2,d->V1);
		}
	}scan_end;
	scan(vnv,DWORD* v1,DWORD* v2){
		Vector3D D1[2];
		int p=0;
		scan_key(vnv,*v1,DWORD* p2){
			D1[p++]=(m_Positions[*p2]-m_Positions[*v1]).ToNormal();
			if(p>1)break;
		}scan_end;
		float dp = D1[0].dot(D1[1]);
		if(dp >- 0.8){
			if(corners.IndexOf(*v1)==-1)corners.Add(*v1);
		}
	}scan_end;
}
void cMeshContainer::GetSharpEdges(cList<DWORDS2>& List,float dotp){
	CreateFone();
	scan(fone,int* f,DWORDS2* ed){
		if(fone.get(*ed)==f){
			scan_key(fone,*ed,int* pf){
				if(*pf!=*f){
					if(GetFaceNormal(*pf).dot(GetFaceNormal(*f))<dotp){
						List.Add(*ed);
					}
				}
			}scan_end;
		}
	}scan_end;
}
void cMeshContainer::GetOpenEdges(cList<DWORDS2>& List){
	CreateFone();
	scan(fone,int* f,DWORDS2* ed){
		if(fone.size(*ed)==1){
			List.Add(*ed);
		}
	}scan_end;
}

void cMeshContainer::GetOpenEdges(cList<bi_int>& List) {
	int h = m_Raw.Count() + m_Positions.Count() * 10;
	void_hash<bi_int> edges;
	edges.set_table_size(m_Raw.Count() + 1);
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		for (int j = 0; j < n; j++) {
			int v1 = m_Raw[i + j + 1][0];
			int v2 = m_Raw[i + 1 + (j + 1) % n][0];
			edges.add_quick(bi_int(v1, v2));
		}
		i += n;
	}
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		for (int j = 0; j < n; j++) {
			int v1 = m_Raw[i + j + 1][0];
			int v2 = m_Raw[i + 1 + (j + 1) % n][0];
			if(!edges.get(bi_int(v2, v1))) {
				List.Add(bi_int(v1, v2));
			}
		}
		i += n;
	}	
}

void cMeshContainer::GetOpenEdges(cList<bi_int>& List, int sub_object) {
	int np = 0;
	for (int i = 0; i < m_Raw.Count(); i++) {
		auto& r = m_Raw[i];
		int n = r[0];
		if (r[2] == sub_object)np += n;
		i += n;
	}
	int h = np;
	void_hash<bi_int> edges;
	edges.set_table_size(m_Raw.Count() + 1);
	for (int i = 0; i < m_Raw.Count(); i++) {
		auto& r = m_Raw[i];
		int n = r[0];
		if (r[2] == sub_object) {
			for (int j = 0; j < n; j++) {
				int v1 = m_Raw[i + j + 1][0];
				int v2 = m_Raw[i + 1 + (j + 1) % n][0];
				edges.add_quick(bi_int(v1, v2));
			}
		}
		i += n;
	}
	for (int i = 0; i < m_Raw.Count(); i++) {
		auto& r = m_Raw[i];
		int n = r[0];
		if (r[2] == sub_object) {
			for (int j = 0; j < n; j++) {
				int v1 = m_Raw[i + j + 1][0];
				int v2 = m_Raw[i + 1 + (j + 1) % n][0];
				if (!edges.get(bi_int(v2, v1))) {
					List.Add(bi_int(v1, v2));
				}
			}
		}
		i += n;
	}
}

void cMeshContainer::SnapToOtherMc(comms::cMeshContainer* other, float dstMod, bool CheckNormals) {
	comms::cMeshContainer mo = *other;
	mo.Triangulate();

	auto& rawc = GetRaw();
	cList<cVec2> len;
	int n = GetPositions().Count();
	len.Add(cVec2::Zero, n);
	for (int i = 0; i < rawc.Count(); i++) {
		int n = rawc[i][0];
		for (int k = 0; k < n; k++) {
			int v1 = rawc[i + 1 + k][0];
			int v2 = rawc[i + 1 + (k + 1) % n][0];
			float L = GetPosition(v1).distance(GetPosition(v2)) * dstMod;
			len[v1] += cVec2(L, 1);
			len[v2] += cVec2(L, 1);
		}
		i += n;
	}
	m_Normals.Clear();
	CalcNormals();
	cList<::std::tuple<cVec3, cVec3, float>> list;
	for(int i=0;i<GetPositions().Count();i++) {
		list.Add(::std::tuple(GetPosition(i), GetNormal(i), len[i][1] >0 ? len[i][0] / len[i][1] : 0));
	}
	mo.SnapPointsSet(list, CheckNormals);
	for (int i = 0; i < GetPositions().Count(); i++) {
		SetPosition(i, ::std::get<0>(list[i]));
	}
}

void cMeshContainer::SmoothFast(float degree,int count,UnlimitedBitset* pins,bool tangent){
	uni_hash<int, int> vnv;
	vnv.set_table_size(m_Positions.Count()+1);
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		for(int j=0;j<n;j++){
			int v1=m_Raw[i+j+1][0];
			int v2=m_Raw[i+1+(j+1)%n][0];
			if(pins==NULL || pins->get(v1)==false)vnv.add_uniq(v1,v2);
			if(pins==NULL || pins->get(v2)==false)vnv.add_uniq(v2,v1);
		}
		i+=n;
	}
	VecArray src;
	for(int p=0;p<count;p++){
		src.Copy(m_Positions);
		if(tangent){
			if(p || m_Normals.Count()!=m_Positions.Count()){
				CalcNormals();
			}
		}
		
		std_parallel_for(0, m_Positions.Count(),
			[&](int i) {
				int nn = 0;
				if (pins && pins->get(i))return;
				Vector3D s(0);
				scan_key(vnv, i, int* pv) {
					s += src[*pv];
					nn++;
				}scan_end;
				if (nn > 1) {
					s /= nn;
					if (tangent) {
						Vector3D n = m_Normals[i];
						s -= n * n.dot(s - src[i]);
					}
					m_Positions[i] = s * degree + src[i] * (1 - degree);
				}
			}
		);
	}
}

void cMeshContainer::SmoothPins(float degree, int count, UnlimitedBitset* pins, bool tangent) {
	if (!pins)return;
	uni_hash<int, int> vnv;
	vnv.set_table_size(m_Positions.Count() + 1);
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		for (int j = 0; j < n; j++) {
			int v1 = m_Raw[i + j + 1][0];
			int v2 = m_Raw[i + 1 + (j + 1) % n][0];
			if (pins->get(v1) && pins->get(v2)) {
				vnv.add_uniq(v2, v1);
				vnv.add_uniq(v1, v2);
			}
		}
		i += n;
	}
	VecArray src;
	for (int p = 0; p < count; p++) {
		src.Copy(m_Positions);
		if (tangent) {
			if (p || m_Normals.Count() != m_Positions.Count()) {
				CalcNormals();
			}
		}

		std_parallel_for(0, m_Positions.Count(),
			[&](int i) {
				int nn = 0;
				if (!pins->get(i))return;
				Vector3D s(0);
				scan_key(vnv, i, int* pv) {
					s += src[*pv];
					nn++;
				}scan_end;
				if (nn == 2) {
					s /= nn;
					if (tangent) {
						Vector3D n = m_Normals[i];
						s -= n * n.dot(s - src[i]);
					}
					m_Positions[i] = s * degree + src[i] * (1 - degree);
				}
			}
		);
	}
}

	void cMeshContainer::SmoothFastAlongNormals(int count, UnlimitedBitset* pins) {
		uni_hash<int, int> vnv;
		vnv.set_table_size(m_Positions.Count() + 1);
	
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			for (int j = 0; j < n; j++) {
				int v1 = m_Raw[i + j + 1][0];
				int v2 = m_Raw[i + 1 + (j + 1) % n][0];
				if (pins == NULL || pins->get(v1) == false)vnv.add_uniq(v1, v2);
				if (pins == NULL || pins->get(v2) == false)vnv.add_uniq(v2, v1);
			}
			i += n;
		}
		VecArray src;
		for (int p = 0; p < count; p++) {
			src.Copy(m_Positions);
			auto& res = m_Positions;
			auto& pos = src;
			auto& nrm = m_Normals;
			
			std_parallel_for(0, m_Positions.Count(),
				[&](int i) {
					int nn = 0;
					if (pins && pins->get(i))return;
					Vector3D s(0);
					scan_key(vnv, i, int* pv) {
						s += pos[*pv];
						nn++;
					}scan_end;
					if (nn) {
						s /= nn;
						if (nrm.Count()) {
							Vector3D n = nrm[i];
							res[i] = pos[i] + n * n.dot(s - pos[i]);
						}
						else res[i] = s;
					}
				}
			);
			if ((p % 16) == 15)CalcNormals();
		}
	}

	void cMeshContainer::PinEdgesAndSharp(UnlimitedBitset& bs) {
		uni_hash<DWORDS2, DWORDS2> ed;
		int h = m_Raw.Count() + 1;
		ed.set_table_size(h);
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			for (int j = 0; j < n; j++) {
				int vp1 = m_Raw[i + j + 1][0];
				int vp2 = m_Raw[i + 1 + (j + 1) % n][0];
				int v1 = m_Raw[i + j + 1][2];
				int v2 = m_Raw[i + 1 + (j + 1) % n][2];
				ed.add_quick(DWORDS2(v1, v2), DWORDS2(vp1, vp2));
			}
			i += n;
		}
		scan(ed, DWORDS2 * f, DWORDS2 * pe) {
			if (ed.size(*pe) == 1) {
				bs.set(f->V1, true);
				bs.set(f->V2, true);
			}
		} scan_end;
	}
	

	void cMeshContainer::SmoothSubsetFast(float degree, int count_regular, int count_tangent, int series_repeats, UnlimitedBitset& pins) {
	uni_hash<int, int> vnv;
	uni_hash<bi_int, int> ednv;
	vnv.set_table_size(m_Positions.Count() + 1);
	ednv.set_table_size(m_Positions.Count() + 1);
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		for (int j = 0; j < n; j++) {
			int v1 = m_Raw[i + j + 1][0];
			int v2 = m_Raw[i + 1 + (j + 1) % n][0];
			if (!pins.get(v1))vnv.add_uniq(v1, v2);
			if (!pins.get(v2)) {
				vnv.add_uniq(v2, v1);
				int v3 = m_Raw[i + 1 + (j + 2) % n][0];
				ednv.add_quick(v2, bi_int(v1, v3));
			}
		}
		i += n;
	}
	VecArray src;
	int npos = m_Positions.Count();
	src.Add(Vector3D::Zero, npos);
	CalcNormals();
	int nn = series_repeats * (count_regular + count_tangent);
	for (int p = 0; p < nn; p++) {
		src.FastCopyFrom(m_Positions);
		int sub = p % (count_regular + count_tangent);
		bool tan = sub >= count_regular;
		if(tan) {
			std_parallel_for(0, npos,
				[&](int i) {
					int nn = 0;
					if (!pins.get(i)) {
						Vector3D s(0);
						scan_key(ednv, i, bi_int * bi) {
							Vector3D p0 = src[i];
							s += Vector3D::Cross(src[bi->V2] - p0, src[bi->V1] - p0);
						}scan_end;
						m_Normals[i] = s.ToNormal();
					}
				}
			);
		}		
		std_parallel_for(0, npos,
			[&](int i) {
				int nn = 0;
				if (!pins.get(i)) {
					Vector3D s(0);
					scan_key(vnv, i, int* pv) {
						s += src[*pv];
						nn++;
					}scan_end;
					if (nn) {
						s /= nn;
						if (tan) {
							Vector3D n = m_Normals[i];
							s -= n * n.dot(s - src[i]);
						}
						m_Positions[i] = s;
					}
				}
			}
		);
	}
	CalcNormals();
}
void cMeshContainer::Smooth(float degree,bool tangent,int count,float sharpdot,UnlimitedBitset* ppin,bool smoothedges){
	if (degree > 1.0){
		int n = ffloorf(degree + 1);
		Smooth(degree/n,tangent,n*count,sharpdot,ppin,smoothedges);
		return;
	}
	const bool delpin = (ppin == NULL);
	if(!ppin)ppin=new UnlimitedBitset;
	UnlimitedBitset& pin=*ppin;
	UnlimitedBitset edge;
	cList<cVec4> pos1;
	uni_hash<DWORD,DWORD> ene;
	CalcNormals();
	pos1.Add(cVec4::Zero,m_Positions.Count());
	CreateFone();
	scan(fone,int* pf,DWORDS2* d){
		if(pin.get(d->V1) && pin.get(d->V2)){
			ene.add_uniq(d->V1,d->V2);
			ene.add_uniq(d->V2,d->V1);
		}
		if(fone.size(*d)==1){
			pin.set(d->V1,true);
			pin.set(d->V2,true);
			edge.set(d->V1,true);
			edge.set(d->V2,true);
		}else{
			//detect sharp edges
			scan_key(fone,*d,int* nf){
				if(*nf>*pf){
					Vector3D n1=GetFaceNormal(*nf);
					Vector3D n2=GetFaceNormal(*pf);
					float dp=n1.dot(n2);
					if(dp<sharpdot && dp>-0.99){
						pin.set(d->V1,true);
						pin.set(d->V2,true);
						ene.add_uniq(d->V1,d->V2);
						ene.add_uniq(d->V2,d->V1);
					}
				}
			}scan_end;
		}
	}scan_end;
	scan(ene,DWORD* v1,DWORD* v2){
		if(ene.size(*v1)!=2)edge.set(*v1,true);
	}scan_end;
	for(int i=0;i<count;i++){
		for(int i=0;i<m_Raw.Count();i++){
			int n=m_Raw[i][0];
			for(int j=0;j<n;j++){
				int v1=m_Raw[i+j+1][0];
				int v2=m_Raw[i+1+(j+1)%n][0];
				Vector3D p1=m_Positions[v1];
				Vector3D p2=m_Positions[v2];
				bool pp1=pin.get(v1);
				bool pp2=pin.get(v2);
				int ed1 = edge.get(v1);
				int ed2 = edge.get(v2);
				if (ed1 <= ed2 && pp1 <= pp2)pos1[v1] += cVec4(p2, 1);
				if (ed2 <= ed1 && pp2 <= pp1)pos1[v2] += cVec4(p1, 1);
			}
			i+=n;
		}
		for(int i=0;i<m_Positions.Count();i++){
			if(pos1[i].w>0){
				if(smoothedges && ene.size(i)==2){
					cVec3 pt1=m_Positions[*ene.get(i,0)];
					cVec3 pt2=m_Positions[*ene.get(i,1)];
					cVec3 d=(pt2-pt1).ToNormal();
					cVec3 p1=pos1[i].ToVec3()/pos1[i].w;
					Vector3D p0=m_Positions[i];
					p1-=p0;
					p1=d*d.dot(p1);
					m_Positions[i]+=p1*degree*0.5;
				}else{
					if(smoothedges || !pin.get(i)){
						float w1 = pos1[i].w;
						cVec3 p1=pos1[i].ToVec3()/w1;
						float deg = degree;
						if (w1 < 2.01){
							deg *= 0.5;
						}
						if(tangent)p1-=m_Normals[i]*m_Normals[i].dot(p1-m_Positions[i]);
						m_Positions[i]=m_Positions[i]*(1.0-deg)+p1*deg;
					}
				}
				/*if(!pin.get(i))*/
			}
			pos1[i]=Vector4D::Zero;
		}
		if(count>1 && tangent){
			CalcNormals();
		}
	}
	if(delpin)delete(ppin);
}
void cMeshContainer::CloseHoles(int MaxHoleSize,bool SkipMaximalHole){
	fone.reset();
	CreateFone();
	CalcNormals();
	cList<bi_DWORD> mob;
	mob.Add(bi_DWORD(0,0),m_Positions.Count());
	uni_hash<int,int> links;
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		for(int p=0;p<n;p++){
			int np=(p+1)%n;
			DWORDS2 D(m_Raw[i+p+1][0],m_Raw[i+np+1][0]);
			if(fone.size(D)==1){
				links.add_quick(m_Raw[i+np+1][0],m_Raw[i+p+1][0]);
				mob[m_Raw[i+p+1][0]]=bi_DWORD(m_Raw[i][1],m_Raw[i][2]);
			}
		}
		i+=n;
	}
	links.refine_table();
	int q=0;
	int MaxHoleSize1=0;
	cList<cList<int>*> AL;

	while(links.size() && q++<1000000){
		cList<int>* li=new cList<int>;
		int p0=-1;
		scan(links,int* e,int* k){
			p0=*k;
			break;
		}scan_end;
		int nc=0;
		int pn=-1;
		int pc=p0;
		int p00=p0;
		int pp=-1;
		do{
			int sz=links.size(pc);
			if(sz==1 || pp==-1){
				scan_key(links,pc,int* pp){
					pn=*pp;
					break;
				}scan_end;
			}else{
				if(pp!=-1){
					Vector3D n=m_Normals[pc].ToNormal();
					Vector3D PP=m_Positions[pp];
					Vector3D PC=m_Positions[pc];
					PP-=PC;
					PP-=n*n.dot(PP);
					float bestw=10000;
					scan_key(links,pc,int* pp){
						int pppn=*pp;
						Vector3D PN=m_Positions[pppn]-PC;
						PN-=n*n.dot(PN);
						float co=PN.dot(PP);
						float si=n.dot(Vector3D::Cross(PP,PN));
						float a=atan2(si,co);
						if(a<0)a+=c_PI*2;
						if(a<bestw){
							bestw=a;
							pn=pppn;
						}
					}scan_end;
				}
			}
			if(pc==pn)break;
			li->Add(pn);
			pp=pc;
			pc=pn;
			nc++;
		}while(pn!=p0 && nc<100000);
		for (int k = 0; k < li->Count(); k++)links.del((*li)[k]);
		if(nc<100000){
			AL.Add(li);
			if(SkipMaximalHole && nc>MaxHoleSize1)MaxHoleSize1=nc;
		}
		else {			
			delete(li);
		}
		links.del(p00);
	}
	for(int i=0;i<AL.Count();i++){
		cList<int> lin=*AL[i];
		int NL=lin.Count();
		DWORD CL=GetRandomColor();
		if(NL!=MaxHoleSize1 && NL<MaxHoleSize){
			
			do{
				float mina=10000;
				int best=-1;
				for(int k=0;k<NL;k++){
					Vector3D pv=m_Positions[lin[(k+NL-1)%NL]];
					Vector3D cv=m_Positions[lin[k]];
					Vector3D nv=m_Positions[lin[(k+1)%NL]];
					Vector3D nn1=m_Normals[lin[k]];
					pv-=cv;
					nv-=cv;
					//pv-=nn*pv.dot(nn);
					//nv-=nn*nv.dot(nn);
					pv.Normalize();
					nv.Normalize();
					Vector3D nn=Vector3D::Cross(pv,nv).ToNormal();
					if(nn.dot(nn1)<0)nn*=-1;
					float co=pv.dot(nv);
					float si=-nn.dot(Vector3D::Cross(pv,nv));
					float ang=atan2(si,co);
					if(ang<0)ang+=2*c_PI;
					if(ang<mina){
						mina=ang;
						best=k;
					}
				}
				if(best!=-1){
					int pv=lin[(best+NL-1)%NL];
					int cv=lin[best];
					int nv=lin[(best+1)%NL];
					lin.RemoveAt(best,1);
					cVec3i nn(3,mob[cv].V1,mob[cv].V2);
					m_Raw.Add(nn);
					cVec3i v1(pv,-1,pv);
					m_Raw.Add(v1);
					cVec3i v2(cv,-1,cv);
					m_Raw.Add(v2);
					cVec3i v3(nv,-1,nv);
					m_Raw.Add(v3);
					NL--;
				}else break;
			}while(NL);
		}
		delete(AL[i]);
	}
}
void cMeshContainer::LeaveBiggestPiece(){
	CreateFone();
	cList<int> clu;
	clu.Add(-1,m_Raw.Count());
	int bestclu=-1;
	int maxsize=0;
	int cclu=0;
	cList<int> order;
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		if(clu[i]==-1){
			order.Clear();
			cclu++;
			int ccsize=1;
			order.Add(i);
			clu[i]=cclu;
			for(int w=0;w<order.Count();w++){
				int q=order[w];
				int nq=m_Raw[q][0];
				for(int j=0;j<nq;j++){
					int c=m_Raw[q+j+1][0];
					int n=m_Raw[q+((j+1)%nq)+1][0];
					DWORDS2 D(c,n);
					scan_key(fone,D,int* pnf){
						int nf=*pnf;
						if(nf!=q){
							if(clu[nf]==-1){
								clu[nf]=cclu;
								ccsize++;
								order.Add(nf);
							}
						}
					}scan_end;
				}
			}
			if(ccsize>maxsize){
				maxsize=ccsize;
				bestclu=cclu;
			}
		}
		i+=n;
	}
	int p=0;
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		if(clu[i]==bestclu){
			for(int j=0;j<=n;j++){
				if(p!=i)m_Raw[p+j]=m_Raw[i+j];
			}
			p+=n+1;
		}
		i+=n;
	}
	if(p<m_Raw.Count()){
		m_Raw.RemoveAt(p,m_Raw.Count()-p);
	}
}
void cMeshContainer::MakeShellDir(float Out,float In,cVec3 Dir){
	int np=m_Positions.Count();
	if(m_Normals.Count()!=m_Positions.Count()){
		CalcNormals();
	}
	CreateFone();
	int nr=m_Raw.Count();
	for(int i=0;i<np;i++){
		Vector3D pp=m_Positions[i]-Dir*In;
		m_Positions.Add(pp);
		m_Positions[i]+=Dir*Out;
		Vector3D nn=m_Normals[i];
		m_Normals.Add(-nn);
	}
	for(int i=0;i<nr;i++){
		cVec3i nn=m_Raw[i];
		m_Raw.Add(nn);
		int n=nn[0];
		for(int j=0;j<n;j++){
			cVec3i rr=m_Raw[i+n-j];
			rr[0]+=np;
			m_Raw.Add(rr);
		}
		i+=n;
	}
	for(int i=0;i<nr;i++){
		cVec3i nn=m_Raw[i];
		int n=nn[0];
		for(int j=0;j<n;j++){
			int v1=m_Raw[i+j+1][0];
			int v2=m_Raw[i+((j+1)%n)+1][0];
			if(fone.size(DWORDS2(v1,v2))==1){
				nn[0]=4;
				m_Raw.Add(nn);
				cVec3i mm;
				mm[0]=v2;
				mm[1]=-1;
				mm[2]=-1;
				m_Raw.Add(mm);
				mm[0]=v1;
				m_Raw.Add(mm);
				mm[0]=v1+np;
				m_Raw.Add(mm);
				mm[0]=v2+np;
				m_Raw.Add(mm);
			}
		}
		i+=n;
	}
}
void cMeshContainer::RelaxNormals(VecArray& ResultNormals, int Count){
	CreateFone();
	UnlimitedBitset pin;
	uni_hash<int, int> vnv;
	CreateVnv(vnv);
	VecArray temp;
	temp = m_Normals;
	ResultNormals = temp;
	for (int i = 0; i < m_Positions.Count(); i++){
		scan_key(vnv, i, int* nv){
			if (fone.size(DWORDS2(i, *nv)) == 1){
				pin.set(i, true);
				pin.set(*nv, true);
			}
		}scan_end;
	}
	for (int i = 0; i < Count; i++){
		std_parallel_for(0, m_Positions.Count(),
			[&](int p) {
				if (pin.get(p) == false) {
					scan_key(vnv, p, int* nv) {
						float L = m_Positions[p].distanceSq(m_Positions[*nv]) + 0.00001;
						temp[p] += ResultNormals[*nv] / L;
					}scan_end;
					temp[p].Normalize();
				}
			}
		);
		for (int p = 0; p < m_Positions.Count(); p++){
			ResultNormals[p] = temp[p];
		}
	}
}
void cMeshContainer::MakeShell(float Out, float In, float OutEdge, float InEdge, int ndiv, cList<cVec2>* EdgeShape){
	int np=m_Positions.Count();
	if(m_Normals.Count()!=m_Positions.Count()){
		CalcNormals();
	}
	VecArray temp;
	CreateFone();
	RelaxNormals(temp, 300);
	UnlimitedBitset bse;
	int nr=m_Raw.Count();
	float L = (InEdge + OutEdge) / 3.0;
	for(int i=0;i<nr;i++){
		cVec3i nn=m_Raw[i];
		int n=nn[0];
		for(int j=0;j<n;j++){
			int v1=m_Raw[i+j+1][0];
			int v2=m_Raw[i+((j+1)%n)+1][0];
			if(fone.size(DWORDS2(v1,v2))==1){
				bse.set(v1,true);
				bse.set(v2,true);
			}
		}
		i+=n;
	}
	for(int i=0;i<np;i++){
		float dp = temp[i].dot(m_Normals[i]);
		if (dp < 0.5)dp = 0.5;
		if(bse.get(i)){
			Vector3D pp = m_Positions[i] - temp[i] * InEdge;
			m_Positions.Add(pp);
			m_Positions[i] += temp[i] * OutEdge;
		}else{
			Vector3D pp = m_Positions[i] - temp[i] * In / dp;
			m_Positions.Add(pp);
			m_Positions[i] += temp[i] * Out / dp;
		}
		//AddDbgLine(m_Positions[i], m_Positions[i] + temp[i] * 8.0, 0xFFFFFFFF, 0xFFFF0000);
		Vector3D nn=m_Normals[i];
		m_Normals.Add(-nn);
	}
	uni_hash<int, int> divs;
	if (EdgeShape && EdgeShape->Count()){
		cList<cVec3> tangents;
		tangents.Add(cVec3::Zero, m_Positions.Count());
		for (int i = 0; i < nr; i++){
			cVec3i nn = m_Raw[i];
			int n = nn[0];
			for (int j = 0; j < n; j++){
				int v1 = m_Raw[i + j + 1][0];
				int v2 = m_Raw[i + ((j + 1) % n) + 1][0];
				if (fone.size(DWORDS2(v1, v2)) == 1){
					cVec3 p1 = m_Positions[v1] - m_Positions[v2];
					tangents[v1] += p1;
					tangents[v2] += p1;
				}
			}
			i += n;
		}
		//relax tangents a bit
		cList<cVec3> tan1;
		for (int i = 0; i < tangents.Count(); i++) {
			if (tangents[i].LengthSq() > 0) {
				tangents[i].Normalize();
			}
		}
		cList<cVec3> tan0 = tangents;
		cList<int> TanSmooth;
		for (int i = 0; i < nr; i++) {
			cVec3i nn = m_Raw[i];
			int n = nn[0];
			for (int j = 0; j < n; j++) {
				int v1 = m_Raw[i + j + 1][0];
				int v2 = m_Raw[i + ((j + 1) % n) + 1][0];
				if (fone.size(DWORDS2(v1, v2)) == 1) {
					TanSmooth.Add(v1);
					TanSmooth.Add(v2);
				}
			}
			i += n;
		}
		for (int i = 0; i < 15; i++){
			tan1 = tangents;
			for (int i = 0; i < TanSmooth.Count(); i += 2){
				int v1 = TanSmooth[i];
				int v2 = TanSmooth[i + 1];
				cVec3 p1 = (tan1[v1] + tan1[v2]).ToNormal();
				tangents[v1] += p1;
				tangents[v2] += p1;
			}
			for (int i = 0; i < tangents.Count(); i++){
				if (tangents[i].LengthSq()>0){
					tangents[i].Normalize();
				}
			}
		}
		ndiv = EdgeShape->Count() - 1;
		float th = OutEdge+InEdge;
		for (int i = 0; i<np; i++){
			if (bse.get(i)){
				Vector3D pp1 = m_Positions[i];
				Vector3D pp2 = m_Positions[i + np];
				Vector3D tan = tangents[i];
				Vector3D tang0 = tan0[i];
				float L = tan.dot(tang0);
				if (L < 0.66)L = 0.66;
				Vector3D Y = (pp2 - pp1).ToNormal();
				float LY = th;
				Vector3D X = Vector3D::Cross(tan, Y)/L;
				for (int k = 0; k < ndiv + 1 ; k++){
					cVec2 pt = (*EdgeShape)[k];
					Vector3D ps = pp1 + (pp2 - pp1)*pt.y + LY*pt.x*X;
					if (k == 0)m_Positions[i] = ps;
					else if (k == ndiv)m_Positions[i + np] = ps;
					else{
						int v = m_Positions.Add(ps);
						divs.add(i, v);
						m_Normals.Add(Vector3D::Zero);
					}
				}
			}
		}
	}
	else
	if (ndiv > 1){
		float mi = 0.05 / ndiv;
		float ma = 1.0 - 2 * mi;
		for (int i = 0; i<np; i++){
			if (bse.get(i)){
				Vector3D pp1 = m_Positions[i];
				Vector3D pp2 = m_Positions[i+np];
				for (int k = 1; k < ndiv; k++){
					float t = mi + ma*(k - 1) / (ndiv - 2);
					Vector3D ps = pp1 + (pp2 - pp1)*t;
					int v = m_Positions.Add(ps);
					divs.add(i, v);
					m_Normals.Add(Vector3D::Zero);
				}
			}
		}
	}
	for(int i=0;i<nr;i++){
		cVec3i nn=m_Raw[i];
		m_Raw.Add(nn);
		int n=nn[0];
		for(int j=0;j<n;j++){
			cVec3i rr=m_Raw[i+n-j];
			rr[0]+=np;
			m_Raw.Add(rr);
		}
		i+=n;
	}
	for(int i=0;i<nr;i++){
		cVec3i nn=m_Raw[i];
		int n=nn[0];
		for(int j=0;j<n;j++){
			int v1=m_Raw[i+j+1][0];
			int v2=m_Raw[i+((j+1)%n)+1][0];
			if(fone.size(DWORDS2(v1,v2))==1){
				if (ndiv > 1){
					for (int p = 0; p < ndiv; p++){
						int v11 = -1;
						int v12 = -1;
						int v21 = -1;
						int v22 = -1;
						if (p == 0){
							v11 = v1;
							v21 = v2;
						}
						else{
							int* pv11 = divs.get(v1, p-1);
							if (pv11)v11 = *pv11;
							int* pv21 = divs.get(v2, p-1);
							if (pv21)v21 = *pv21;
						}
						if (p == ndiv - 1){
							v12 = v1 + np;
							v22 = v2 + np;
						}
						else{
							int* pv12 = divs.get(v1, p);
							if (pv12)v12 = *pv12;
							int* pv22 = divs.get(v2, p);
							if (pv22)v22 = *pv22;
						}
						if (v11 != -1 && v12 != -1 && v21 != -1 && v22 != -1){
							nn[0] = 4;
							m_Raw.Add(nn);
							cVec3i mm;
							mm[0] = v21;
							mm[1] = -1;
							mm[2] = -1;
							m_Raw.Add(mm);
							mm[0] = v11;
							m_Raw.Add(mm);
							mm[0] = v12;
							m_Raw.Add(mm);
							mm[0] = v22;
							m_Raw.Add(mm);
						}						
					}
				}
				else{
					nn[0] = 4;
					m_Raw.Add(nn);
					cVec3i mm;
					mm[0] = v2;
					mm[1] = -1;
					mm[2] = -1;
					m_Raw.Add(mm);
					mm[0] = v1;
					m_Raw.Add(mm);
					mm[0] = v1 + np;
					m_Raw.Add(mm);
					mm[0] = v2 + np;
					m_Raw.Add(mm);
				}
			}
		}
		i+=n;
	}
}
void cMeshContainer::DrawDbg(const cMat4& T,DWORD Color,DWORD FillColor,DWORD DetailColor){
	if(FillColor){
		for(int i=0;i<m_Raw.Count();i++){
			int n=m_Raw[i][0];
			for(int j=1;j<n-1;j++){
				int v1=m_Raw[i+j+1][0];
				int v2=m_Raw[i+((j+1)%n)+1][0];
				int v3=m_Raw[i+1][0];
				Vector3D pp1=m_Positions[v1];
				Vector3D pp2=m_Positions[v2];
				Vector3D pp3=m_Positions[v3];
				pp1.TransformCoordinate(T);
				pp2.TransformCoordinate(T);
				pp3.TransformCoordinate(T);
				AddDbgTri(pp1,pp2,pp3,FillColor);
			}
			i+=n;
		}
	}
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		for(int j=0;j<n;j++){
			int v1=m_Raw[i+j+1][0];
			int v2=m_Raw[i+((j+1)%n)+1][0];
			Vector3D pp1=m_Positions[v1];
			Vector3D pp2=m_Positions[v2];
			pp1.TransformCoordinate(T);
			pp2.TransformCoordinate(T);
			AddDbgLine(pp1,pp2,Color,Color);
		}
		i+=n;
	}
	if ( DetailColor ) {
		for (int i = 0, q = 0;  i < m_Raw.Count();  i++, q++) {
			const int  n = m_Raw[ i ][ 0 ];
			const cVec3&  pos = GetFaceCoord( i );
			::std::ostringstream  ss;
			ss << q << " #" << i;
			AddDbgText( pos, DetailColor, ss.str().c_str() );
			i += n;
		}
	}
}
void cMeshContainer::Symmetry(cVec3 p, cVec3 nr, bool quads, bool RemoveNeg, float ToleranceCoef){
	bool tria=!quads;
	quads=true;
	auto& raw=GetRaw();
	auto& pos=GetPositions();
	cList<bi_DWORD> MidEdges;
	float mdist = 0.001;
	if (ToleranceCoef > 0){
		mdist = 0.0000001;
		uni_hash<int, DWORDS2> openedges;
		float avl = 0;
		int ne = 0;
		for (int i = 0; i<raw.Count(); i++){
			int n = raw[i][0];
			float l = 0;
			for (int j = 0; j<n; j++){
				int v1 = raw[i + j + 1][0];
				int v2 = raw[i + (j + 1) % n + 1][0];
				cVec3 p0 = pos[v1];
				cVec3 p1 = pos[v2];
				openedges.add_quick(DWORDS2(v1, v2), i);
				l += p0.distance(p1);
			}
			if (n){
				l /= n;
				avl += l;
				ne++;
			}
			i += n;
			openedges.refine_table();
		}
		if (ne){
			ToleranceCoef *= avl / ne;
			mdist = ToleranceCoef;
		}
		
		// snap points that are close to plane
		for (int i = 0; i<raw.Count(); i++){
			int n = raw[i][0];
			for (int j = 0; j<n; j++){
				for (int j = 0; j < n; j++){
					int v1 = raw[i + j + 1][0];
					int v2 = raw[i + (j + 1) % n + 1][0];
					
					cVec3& p1 = pos[v1];
					float d = nr.dot(p1 - p);
					if (__abs(d) < ToleranceCoef){
						p1 -= nr*d;
					}
					cVec3& p2 = pos[v2];
					d = nr.dot(p2 - p);
					if (__abs(d) < ToleranceCoef){
						p2 -= nr*d;
					}						
				}
			}
			i += n;
		}
	}
	if(quads){
		//estimate quality
		int NGL=0;
		int NGR=0;
		int NL = 0;
		int NR = 0;
		for(int i=0;i<raw.Count();i++){
			int n=raw[i][0];
			Vector3D c(0);
			for(int j=0;j<n;j++){
				cVec3 p0=pos[raw[i+j+1][0]];
				c+=p0;
			}
			if(n){
				c/=n;
				float dp = nr.dot(c - p);
				if (n > 4 && n & 1){
					if (dp > 0)NGR++;
					else NGL++;
				}
				if (dp > 0)NR++;
				else NL++;
			}
			i+=n;
		}
		if (NGR > NGL){
			nr *= -1;
			cMath::Swap(NL, NR);
		}
		if (NL > NR * 10){
			nr *= -1;
		}
	}
	int nv=pos.Count();
	for(int i=0;i<nv;i++){
		cVec3 pt=pos[i];
		pt=pt-nr*nr.dot(pt-p)*2.0;
		pos.Add(pt);
	}
	if(quads){
		for(int i=0;i<nv;i++){
			cVec3 pt=pos[i];
			pt=pt-nr*nr.dot(pt-p);
			pos.Add(pt);
		}
	}
	cList<cVec3i> rn;
	m_Normals.Clear();
	cList<int> Mirror;
	for(int i=0;i<raw.Count();i++){
		int n=raw[i][0];
		bool allbad=true;
		bool allgood=true;
		int ngood=0;
		for(int j=0;j<n;j++){
			cVec3 p0=pos[raw[i+j+1][0]];
			if (nr.dot(p0 - p) <= mdist){
				allgood=false;
			}else{
				allbad=false;
				ngood++;
			}	
		}
		if(!allbad){
			if(quads && !allgood){
				if(ngood>1){
					cVec3i vc=raw[i];
					vc[0]=4;
					int vp=-1;					
					for(int j=0;j<=n;j++){
						int vt=raw[i+1+(j%n)][0];
						Vector3D p0=pos[vt];
						if (nr.dot(p0 - p) > mdist){
							if(vp!=-1){
								bi_DWORD b(vp,vt);
								MidEdges.Add(b);														
							}
							vp=vt;
						}else vp=-1;
					}
				}
			}else{
				cVec3i v=raw[i];
				rn.Add(v);
				for(int j=0;j<n;j++){
					cVec3i v1=raw[i+j+1];
					v1[1]=v1[2]=-1;
					rn.Add(v1);
				}
				if(!RemoveNeg){
					rn.Add(v);
					v[1]=v[2]=-1;
					for(int j=0;j<n;j++){
						Vector3D p0=pos[raw[i+n-j][0]];
						v=raw[i+1+(n+1-j)%n];
						v[1]=v[2]=-1;
						if (nr.dot(p0 - p) >= mdist){
							v[0]+=nv;
						}	
						rn.Add(v);
					}
				}
			}
		}
		i+=n;
	}	
	uni_hash<int,bi_DWORD> fone;
	fone.set_table_size(rn.Count());
	for(int i=0;i<rn.Count();i++){
		int n=rn[i][0];
		for(int j=0;j<n;j++){
			int jn=(j+1)%n;
			int v1=rn[i+j+1][0];
			int v2=rn[i+jn+1][0];
			bi_DWORD b(v2,v1);
			fone.add_quick(b,i);
		}
		i+=n;
	}
	for(int i=0;i<MidEdges.Count();i++){
		bi_DWORD b=MidEdges[i];
		if(fone.size(b)==1){
			int vp=b.V1;
			int vt=b.V2;
			cVec3i vc;
			if(tria){
				rn.Add(cVec3i(3, 0, 0));
				rn.Add(cVec3i(vp, -1, -1));
				rn.Add(cVec3i(vt, -1, -1));
				rn.Add(cVec3i(vt + nv + nv, -1, -1));				

				if(!RemoveNeg){
					rn.Add(cVec3i(3, 0, 0));
					rn.Add(cVec3i(vt + nv, -1, -1));
					rn.Add(cVec3i(vp + nv, -1, -1));
					rn.Add(cVec3i(vt + nv + nv, -1, -1));					
				}

				rn.Add(cVec3i(3, 0, 0));
				rn.Add(cVec3i(vp, -1, -1));
				rn.Add(cVec3i(vt + nv + nv, -1, -1));
				rn.Add(cVec3i(vp + nv + nv, -1, -1));				

				if(!RemoveNeg){
					rn.Add(cVec3i(3, 0, 0));
					rn.Add(cVec3i(vt + nv + nv, -1, -1));
					rn.Add(cVec3i(vp + nv, -1, -1));
					rn.Add(cVec3i(vp + nv + nv, -1, -1));					
				}
			}else{
				rn.Add(cVec3i(4, 0, 0));
				rn.Add(cVec3i(vp, -1, -1));
				rn.Add(cVec3i(vt, -1, -1));
				rn.Add(cVec3i(vt + nv + nv, -1, -1));
				rn.Add(cVec3i(vp + nv + nv, -1, -1));				

				if(!RemoveNeg){
					rn.Add(cVec3i(4, 0, 0));
					rn.Add(cVec3i(vp + nv + nv, -1, -1));
					rn.Add(cVec3i(vt + nv + nv, -1, -1));
					rn.Add(cVec3i(vt + nv, -1, -1));
					rn.Add(cVec3i(vp + nv, -1, -1));					
				}
			}
		}
	}
	for(int i=0;i<nv;i++){
		Vector3D pt=pos[i];
		float d=nr.dot(pt-p);
		if(d<0){
			pt=pt-d*nr;
			pos[i]=pt;
		}
	}
	m_Raw.Copy(rn);
	RemoveUnusedVerts();
}
void cMeshContainer::WeldedSymmetry(cVec3 p, cVec3 nr, bool RemoveNeg, float ToleranceCoef, float WeldCoef){
	auto& raw = GetRaw();
	auto& pos = GetPositions();
	cList<bi_DWORD> MidEdges;
	float mdist = 0.001;
	if (ToleranceCoef > 0){
		mdist = 0.0000001;
		uni_hash<int, DWORDS2> openedges;
		float avl = 0;
		int ne = 0;
		for (int i = 0; i<raw.Count(); i++){
			int n = raw[i][0];
			float l = 0;
			for (int j = 0; j<n; j++){
				int v1 = raw[i + j + 1][0];
				int v2 = raw[i + (j + 1) % n + 1][0];
				cVec3 p0 = pos[v1];
				cVec3 p1 = pos[v2];
				openedges.add_quick(DWORDS2(v1, v2), i);
				l += p0.distance(p1);
			}
			if (n){
				l /= n;
				avl += l;
				ne++;
			}
			i += n;
			openedges.refine_table();
		}
		if (ne){
			ToleranceCoef *= avl / ne;
		}
		for (int i = 0; i<raw.Count(); i++){
			int n = raw[i][0];
			for (int j = 0; j<n; j++){
				for (int j = 0; j < n; j++){
					int v1 = raw[i + j + 1][0];
					int v2 = raw[i + (j + 1) % n + 1][0];
					if (openedges.size(DWORDS2(v1, v2)) == 1){//ony open edges
						cVec3& p1 = pos[v1];
						float d = nr.dot(p1 - p);
						if (__abs(d) < ToleranceCoef){
							p1 -= nr*d;
						}
						cVec3& p2 = pos[v2];
						d = nr.dot(p2 - p);
						if (__abs(d) < ToleranceCoef){
							p2 -= nr*d;
						}
					}
				}
			}
			i += n;
		}
	}
	//estimate quality
	int NGL = 0;
	int NGR = 0;
	int NL = 0;
	int NR = 0;
	for (int i = 0; i<raw.Count(); i++){
		int n = raw[i][0];
		Vector3D c(0);
		for (int j = 0; j<n; j++){
			cVec3 p0 = pos[raw[i + j + 1][0]];
			c += p0;
		}
		if (n){
			c /= n;
			float dp = nr.dot(c - p);
			if (n > 4 && n & 1){
				if (dp > 0)NGR++;
				else NGL++;
			}
			if (dp > 0)NR++;
			else NL++;
		}
		i += n;
	}
	if (NGR > NGL){
		nr *= -1;
		cMath::Swap(NL, NR);
	}
	if (NL > NR * 10){
		nr *= -1;
	}
	int nv = pos.Count();
	for (int i = 0; i<nv; i++){
		cVec3 pt = pos[i];
		pt = pt - nr*nr.dot(pt - p)*2.0;
		pos.Add(pt);
	}
	cList<cVec3i> rn;
	m_Normals.Clear();
	cList<int> Mirror;
	for (int i = 0; i<raw.Count(); i++){
		int n = raw[i][0];
		bool allbad = true;
		bool allgood = true;
		int ngood = 0;
		for (int j = 0; j<n; j++){
			cVec3 p0 = pos[raw[i + j + 1][0]];
			float d = nr.dot(p0 - p);
			if (d <= -mdist){
				allgood = false;
			}
			if (d >= mdist){
				allbad = false;
				ngood++;
			}
		}
		if (allgood){
			cVec3i v = raw[i];
			rn.Add(v);
			for (int j = 0; j<n; j++){
				cVec3i v1 = raw[i + j + 1];
				v1[1] = v1[2] = -1;
				rn.Add(v1);
			}
			if (!RemoveNeg){
				rn.Add(v);
				v[1] = v[2] = -1;
				for (int j = 0; j < n; j++){
					v = raw[i + 1 + (n + 1 - j) % n];
					v[1] = v[2] = -1;
					Vector3D vop = pos[v[0] + nv];
					Vector3D vc = pos[v[0]];
					if (vop.distance(vc) > 0.0001){
						v[0] += nv;
					}
					rn.Add(v);
				}
			}
		}
		i += n;
	}
	m_Raw.Copy(rn);
	RemoveUnusedVerts();
	if (WeldCoef > 0)Weld(ToleranceCoef * WeldCoef);
}
void cMeshContainer::AutoWeldOpenEdges() {
	CreateFone();
	UnlimitedBitset bs;
	float minlen = FLT_MAX;
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		for (int j = 0; j < n; j++) {
			DWORDS2 D(m_Raw[i + j + 1][0], m_Raw[i + (j + 1) % n + 1][0]);
			if (fone.size(D) == 1) {
				float L = m_Positions[D.V1].distance(m_Positions[D.V2]);
				if (L > 0) {
					minlen = ::std::min(L, minlen);
					bs.set(D.V1, true);
					bs.set(D.V2, true);
				}
			}
		}
		i += n;
	}
	Weld(minlen / 8.0, &bs);
	fone.reset();
}

void cMeshContainer::CloseHolesBetweenDistinctIslands(cList<cStr>* involved_objects) {
	cList<bi_int> edges;
	if(involved_objects) {
		for (int i = 0; i < m_Objects.Count(); i++) {
			for(auto& s:*involved_objects){
				if (s == m_Objects[i].Name) {
					GetOpenEdges(edges, i);
					break;
				}
			}
		}
	}else {
		GetOpenEdges(edges);
	}
	if(edges.Count()) {
		uni_hash<int, int> links;
		for (auto& e : edges) {
			links.add_quick(e.V2, e.V1);
		}
		cList<unsigned short> recode_objects;
		recode_objects.Add(0xFFFF, m_Objects.Count());
		BigDynArray<unsigned short, 4096> vertex_object;
		vertex_object.SetCount(m_Positions.Count(), 0);
		BigDynArray<unsigned short, 4096> vertex_material;
		vertex_material.SetCount(m_Positions.Count(), 0);
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			unsigned short ob = m_Raw[i][2];
			unsigned short mt = m_Raw[i][1];
			for (int j = 0; j < n; j++) {
				int v = m_Raw[i + j + 1][0];
				vertex_object[v] = ob;
				vertex_material[v] = mt;
			}
			i += n;
		}

		fAABBNode seek;
		cList<fAB> boxes;
		fAABBNodePool pool;
		int idx = 0;
		for (auto& e : edges) {
			cVec3 v1 = m_Positions[e.V1];
			cVec3 v2 = m_Positions[e.V2];
			float R = v1.distance(v2) * 0.75;
			fAB ab;
			ab.ab.SetEmpty();
			ab.ab.AddPoint((v1 + v2) * 0.5);
			ab.ab.Inflate(R);
			ab.idx = idx++;
			boxes.Add(ab);
		}
		seek.Init(boxes, pool);
		cList<bi_DWORD> res;
		seek.IntersectWith(seek, res);
		cList<bi_DWORD> allowed;
		cList<float> dist;
		for(auto& e:res) {
			auto e1 = edges[e.V1];
			auto e2 = edges[e.V2];
			if (e1.V1 != e2.V1 && e1.V1 != e2.V2 && e1.V2 != e2.V2 && e1.V2 != e1.V1) {
				auto d1 = m_Positions[e1.V2] - m_Positions[e1.V1];
				auto d2 = m_Positions[e2.V2] - m_Positions[e2.V1];
				if (d1.dot(d2) < 0) {
					float w;
					float L = GetPointToLineDV(m_Positions[e1.V1], m_Positions[e2.V1], m_Positions[e2.V2], w).Length();
					L = std::min(L, GetPointToLineDV(m_Positions[e1.V2], m_Positions[e2.V1], m_Positions[e2.V2], w).Length());
					L = std::min(L, GetPointToLineDV(m_Positions[e2.V1], m_Positions[e1.V1], m_Positions[e1.V2], w).Length());
					L = std::min(L, GetPointToLineDV(m_Positions[e2.V2], m_Positions[e1.V1], m_Positions[e1.V2], w).Length());
					if (L < d1.Length() && L < d2.Length()) {
						allowed.Add(e);
						dist.Add(L);
					}
				}
			}
		}
		psort(allowed.ToPtr(), dist.ToPtr(), allowed.Count(), sizeof(bi_DWORD));
		UnlimitedBitset used;
		used.set(m_Positions.Count(), false);

		auto add_tri = [&](int v1, int v2, int v3) {
			auto m = std::min(std::min(vertex_material[v1], vertex_material[v2]), vertex_material[v3]);
			auto o1 = vertex_object[v1];
			auto o2 = vertex_object[v2];
			auto o3 = vertex_object[v3];
			auto o = std::min(std::min(o1, o2), o3);
			auto om = std::max(std::max(o1, o2), o3);
			m_Raw.Add({ 3,m,o });
			m_Raw.Add({ v1,-1,-1 });
			m_Raw.Add({ v2,-1,-1 });
			m_Raw.Add({ v3,-1,-1 });
			used.set(v1, true);
			used.set(v2, true);
			used.set(v3, true);
			if (o != om)recode_objects[om] = std::min(recode_objects[om], o);
			if (!links.del_elm(v1, v2)) {
				links.add_quick(v2, v1);
			}
			if (!links.del_elm(v2, v3)) {
				links.add_quick(v3, v2);
			}
			if (!links.del_elm(v3, v1)) {
				links.add_quick(v1, v3);
			}
		};
		for (int i = 0; i < allowed.Count(); i++) {
			auto a = allowed[i];
			bi_int ed1 = edges[a.V1];
			bi_int ed2 = edges[a.V2];
			if (!(used.get(ed1.V1) || used.get(ed1.V2) || used.get(ed2.V1) || used.get(ed2.V2))) {
				float L_12_22 = m_Positions[ed1.V2].distance(m_Positions[ed2.V2]);
				float L_11_21 = m_Positions[ed1.V1].distance(m_Positions[ed2.V1]);
				if(L_11_21 < L_12_22) {
					add_tri(ed1.V2, ed1.V1, ed2.V1);
					add_tri(ed2.V2, ed2.V1, ed1.V1);
				}else {
					add_tri(ed1.V2, ed1.V1, ed2.V2);
					add_tri(ed2.V2, ed2.V1, ed1.V2);
				}
			}
		}
		// fast closing holes
		bool ch = false;
		for (int k = 0; k < 4; k++) {
			float dpmin = 1.0f - k * k * 0.25;
			do {
				ch = false;
				scan(links, auto * to, auto * from) {
					auto* next = links.get(*to);
					if (next) {
						auto v1 = *from;
						auto v2 = *to;
						auto v3 = *next;
						bool add = false;
						if (k == 0 && vertex_object[v1] != vertex_object[v3]) add = true;
						else{
							auto n1 = (m_Positions[v1] - m_Positions[v2]).ToNormal();
							auto n2 = (m_Positions[v3] - m_Positions[v2]).ToNormal();
							float dp = n1.dot(n2);
							add = n1.dot(n2) > dpmin;
						}
						if (add) {
							add_tri(v1, v2, v3);
							ch = true;
						}
					}
				}scan_end;
			} while (ch);
		}
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			int& ob = m_Raw[i][2];
			while (recode_objects[ob] != 0xFFFF && recode_objects[ob] != ob) {
				ob = recode_objects[ob];
			}
			i += n;
		}
	}
}

void cMeshContainer::TriangulateAndDecimate(int destinationTris) {
	Triangulate();
	int pol = GetPolyCount();
	if(pol > destinationTris * 1.1) {
		TriangulationContext TC;
		TC.FromRawMesh(this);
		HalfEdgeMesh he;
		he.AddGeometry(&TC, GetObjects().GetLast().Name);
		he.FinalizeCreation();
		float p = 1.0 - float(destinationTris) / pol;
		he.CollapsePercent(p, false);
		Clear();
		he.ToRawMesh(this);
	}
}

void cMeshContainer::GradualSubdivision(const std::function<bool(int, int)>& test_split_edge, const std::function<comms::cVec3(int, int)>& get_middle_position) {
	Triangulate();
	if (!m_Raw.Count())return;
	bool changes = false;
	uni_hash<int, DWORDS2> middle;
	cList<int> mid_list;
	BigDynArray<byte,16384> subdv;
	UnlimitedBytes allowedFaces;
	int nr = m_Raw.Count() / 4;
	allowedFaces.set(nr, 3);
	for (int i = 0; i < nr; i++) {
		allowedFaces.set(i, 1);
	}
	RemoveZeroEdgesFaces();
	bool KeepNormals = m_Normals.Count() == m_Positions.Count();
	BigDynArray<int> live;
	for (int i = 0; i < m_Raw.Count(); i++) {
		live.Add(i);
		i += m_Raw[i][0];
	}
	do {
		int nr = live.Count();
		std_for(p, subdv.Count()) {
			subdv[p] = 0;
		}std_for_end;
		if(m_Positions.Count() > subdv.Count()) {
			subdv.Add(0, m_Positions.Count() - subdv.Count());
		}
		int nsubd = 0;
		std_for(q, live.Count()) {
			int i = live[q];
			int n = m_Raw[i][0];
			for (int j = 0; j < n; j++) {
				int v1 = m_Raw[i + j + 1][0];
				int v2 = m_Raw[i + (j + 1) % n + 1][0];
				bool split = test_split_edge(v1, v2);
				if (split) {
					nsubd++;
					for (int j = 0; j < n; j++) {
						subdv[m_Raw[i + j + 1][0]] = 1;
					}
					break;
				}
			}
		}std_for_end;
		nsubd *= 5;
		if (middle.get_table_size() < nsubd)middle.set_table_size(nsubd);
		do {
			changes = false;
			for (int q = 0; q < nr; q++) {
				int i = live[q];
				int n = m_Raw[i][0];
				int ndiv = 0;
				int div[3] = { -1,-1,-1 };
				for (int j = 0; j < n; j++) {
					int v1 = m_Raw[i + j + 1][0];
					int v2 = m_Raw[i + (j + 1) % n + 1][0];
					DWORDS2 m(v1, v2);
					auto* pm = middle.get(m);
					if (!pm) {
						if (subdv[v1] || subdv[v2]) {
							int mv = m_Positions.Add(get_middle_position(v1, v2));
							if (KeepNormals)m_Normals.Add((m_Normals[v1] + m_Normals[v2]).ToNormal());
							div[j] = mv;
							middle.add_quick(m, mv);
							changes = true;
							ndiv++;
						}
					}
					else {
						div[j] = *pm;
						ndiv++;
					}
				}
				if(ndiv==2) {
					for (int j = 0; j < n; j++) {
						if (div[j] == -1) {
							int v1 = m_Raw[i + j + 1][0];
							int v2 = m_Raw[i + (j + 1) % n + 1][0];
							DWORDS2 m(v1, v2);
							int mv = m_Positions.Add(get_middle_position(v1, v2));
							if (KeepNormals)m_Normals.Add((m_Normals[v1] + m_Normals[v2]).ToNormal());
							middle.add_quick(m, mv);
							changes = true;
							ndiv++;
						}
					}
				}
			}
		} while (changes);
		changes = false;
		if (middle.size()) {
			for (int q = 0; q < nr; q++) {
				int i = live[q];
				int n = m_Raw[i][0];
				int ndiv = 0;
				int div[3] = { -1,-1,-1 };
				for (int j = 0; j < n; j++) {
					int v1 = m_Raw[i + j + 1][0];
					int v2 = m_Raw[i + (j + 1) % n + 1][0];
					DWORDS2 m(v1, v2);
					auto* pm = middle.get(m);
					if (pm) {
						div[j] = *pm;
						ndiv++;
					}
				}
				if (ndiv == 1) {
					for (int j = 0; j < n; j++) {
						if (div[j] != -1) {
							live.Add(m_Raw.Count());
							m_Raw.Add(m_Raw[i]);
							allowedFaces.set(m_Raw.Count() / 4, true);
							m_Raw.Add({ div[j],-1,-1 });
							m_Raw.Add({ m_Raw[i + (j + 1) % n + 1][0],-1,-1 });
							m_Raw.Add({ m_Raw[i + (j + 2) % n + 1][0],-1,-1 });
							m_Raw[i + (j + 1) % n + 1][0] = div[j];
							changes = true;
						}
					}
				} else
				if (ndiv == 3) {
					for (int j = 0; j < n; j++) {
						live.Add(m_Raw.Count());
						m_Raw.Add(m_Raw[i]);
						allowedFaces.set(m_Raw.Count() / 4, true);
						m_Raw.Add({ m_Raw[i + j + 1][0],-1,-1 });
						m_Raw.Add({ div[j],-1,-1 });
						m_Raw.Add({ div[(j + 2) % n],-1,-1 });
						changes = true;
					}
					for (int j = 0; j < n; j++) {
						m_Raw[i + j + 1][0] = div[j];
					}
				}
				else if(ndiv==0){
					allowedFaces.set(i / 4, false);
				}
			}
			middle.reset();
		}
		int j = 0;
		for(int i=0;i<live.Count();i++) {
			if(allowedFaces.get(live[i]>>2)) {
				live[j] = live[i];
				j++;
			}
		}
		if (j < live.Count())live.Del(j, live.Count() - j);
	}while(changes);	
}

/*
void cMeshContainer::GradualSubdivision(
	const std::function<bool(int, int)>& testEdgeSplit,
	const std::function<comms::cVec3(int, int)>& getMiddlePosition) {

	Triangulate();

	if (!m_Raw.Count()) {
		return;
	}

	bool isChanged = false;
	uni_hash<int, DWORDS2> middlePoints;
	cList<int> midList;
	BigDynArray<byte, 16384> subdivisionBuffer;
	UnlimitedBytes allowedFaces;
	int numElements = m_Raw.Count() / 4;

	// Initialize allowedFaces to 1
	for (int i = 0; i < numElements; i++) {
		allowedFaces.set(i, 1);
	}

	RemoveZeroEdgesFaces();

	do {
		int numElements = m_Raw.Count();
		// Set each element in subdivisionBuffer to 0
		std_for(p, subdivisionBuffer.Count()) {
			subdivisionBuffer[p] = 0;
		}std_for_end;

		if (m_Positions.Count() > subdivisionBuffer.Count()) {
			subdivisionBuffer.Add(0, m_Positions.Count() - subdivisionBuffer.Count());
		}

		int numSubdivisions = 0;
		// Loop to determine if a subdivision is needed
		std_for(p, numElements / 4) {
			if (allowedFaces.get(p)) {
				int i = p << 2;
				int n = m_Raw[i][0];
				for (int j = 0; j < n; j++) {
					int v1 = m_Raw[i + j + 1][0];
					int v2 = m_Raw[i + (j + 1) % n + 1][0];
					bool shouldSplit = testEdgeSplit(v1, v2);
					if (shouldSplit) {
						numSubdivisions++;
						for (int j = 0; j < n; j++) {
							subdivisionBuffer[m_Raw[i + j + 1][0]] = 1;
						}
						break;
					}
				}
			}
		}std_for_end;

		numSubdivisions *= 5;
		if (middlePoints.get_table_size() < numSubdivisions) {
			middlePoints.set_table_size(numSubdivisions);
		}

		do {
			isChanged = false;
			for (int i = 0; i < numElements; i++) {
				int n = m_Raw[i][0];
				int numDivisions = 0;
				int divisions[3] = { -1,-1,-1 };
				for (int j = 0; j < n; j++) {
					int v1 = m_Raw[i + j + 1][0];
					int v2 = m_Raw[i + (j + 1) % n + 1][0];
					DWORDS2 m(v1, v2);
					auto* midpoint = middlePoints.get(m);
					if (!midpoint) {
						if (subdivisionBuffer[v1] || subdivisionBuffer[v2]) {
							int midpointIndex = m_Positions.Add(getMiddlePosition(v1, v2));
							divisions[j] = midpointIndex;
							middlePoints.add_quick(m, midpointIndex);
							isChanged = true;
							numDivisions++;
						}
					}
					else {
						divisions[j] = *midpoint;
						numDivisions++;
					}
				}
				// Additional logic for case when numDivisions equals 2
				i += n;
			}
		} while (isChanged);

		// Additional logic for post-processing of middlePoints
	} while (isChanged);
}
*/

void cMeshContainer::OptimizeMesh(int MinDestinationTriangles) {
	float sq = CalcSquare();
	int p = GetPolyCount();
	int p1 = p * 1.5;
	if (p1 < MinDestinationTriangles*1.5)p1 = MinDestinationTriangles*1.5;
	p1 /= 1.5;

	float l = sqrt(sq / p1 * 2);
	auto& raw = GetRaw();
	auto& pos = GetPositions();
	RawArray res;
	uni_hash<int, tri_DWORD> hash;
	hash.set_table_size(pos.Count() + 1);
	int nt0 = GetPolyCount();
	for (int i = 0; i < raw.Count(); i++) {
		int n = raw[i][0];
		StackArray<int> vtemp;
		StackArray<int> nsides;
		StackArray<Vector3D> pts;
		for (int k = 0; k < n; k++) {
			int v = raw[i + k + 1][0];
			vtemp.Add(v);
			pts.Add(pos[v]);
			int vn = raw[i + 1 + ((k + 1) % n)][0];
			float L = pos[v].distance(pos[vn]);
			if (L > l * 1.6) {
				int n = ffloorf(L / l);
				if (n > 15)n = 15;
				nsides.Add(n);
				for (int k = 0; k < n; k++) {
					tri_DWORD T(vn, v, n - k);
					int* pp = hash.get(T);
					if (pp) {
						vtemp.Add(*pp);
					}
					else {
						Vector3D pt = pos[v] + (pos[vn] - pos[v]) * float(k + 1) / (n + 1);
						int np = pos.Add(pt);
						::std::swap(T.V1, T.V2);
						T.V3 = k + 1;
						hash.add_quick(T, np);
						vtemp.Add(np);
					}
				}
			}
			else nsides.Add(0);
		}
		if (vtemp.Count() > 3) {
			tMiniMesh* m = TPatch.TriangulatePatch(nsides[0], nsides[1], nsides[2]);
			pts[1] -= pts[0];
			pts[2] -= pts[0];
			for (int k = vtemp.Count(); k < m->uvw.Count(); k++) {
				Vector3D p = pts[0] + pts[1] * m->uvw[k].x + pts[2] * m->uvw[k].y;
				vtemp.Add(pos.Add(p));
			}
			for (int k = 0; k < m->Indices.Count(); k += 3) {
				res.Add(cVec3i(3, 0, 0));
				for (int p = 0; p < 3; p++) {
					res.Add(cVec3i(vtemp[m->Indices[k + p]], -1, -1));
				}
			}
		}
		else {
			res.Add(cVec3i(vtemp.Count(), 0, 0));
			for (int k = 0; k < vtemp.Count(); k++) {
				res.Add(cVec3i(vtemp[k], -1, -1));
			}
		}
		i += n;
	}
	raw = res;
	int nt1 = GetPolyCount();
	TriangulateAndDecimate(p1);
}

void cMeshContainer::AutodetectSymmetry(cList<cPlane>& planes) {
	Triangulate();
	std::mutex m;
	_std_for(symm, 3) {
		Vector3D p(0);
		p[symm] = 1;
		Vector3D zz(0);
		zz[symm] = 1;
		Vector3D x, y;
		CreateBasis(zz, x, y);
		Matrix3D tr(Matrix3D::ColsCtor, x, y, zz);
		AABoundBox ab;
		ab.SetEmpty();
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			for(int k=0;k<n;k++){
				Vector3D p0 = m_Positions[m_Raw[i + k + 1][0]];
				p0 *= tr;					
				ab.AddPoint(p0);
			}
			i += n;
		}
		Vector3D p0 = ab.GetMin();
		p0.z = 0;
		float Lx = ab.GetSizeX();
		float Ly = ab.GetSizeY();
		float L = std::max(Lx, Ly);

		Matrix4D T = Matrix4D(tr, Vector3D::Zero);
		T *= Matrix4D::Translation(-p0);
		T *= Matrix4D::Scaling(512 / L);
		ValuesField vf;
		int szx = int(512 * Lx / L + 1);
		int szy = int(512 * Ly / L + 1);
		vf.create(szx, szy, 2);
		vf.operate(
			[&](int x, int y) {
				vf.set(FLT_MAX, x, y, 0);
				vf.set(-FLT_MAX, x, y, 1);
			}, false);
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			if (n == 3) {
				Vector3D p0 = m_Positions[m_Raw[i + 1][0]];
				Vector3D p1 = m_Positions[m_Raw[i + 2][0]];
				Vector3D p2 = m_Positions[m_Raw[i + 3][0]];
				p0.TransformCoordinate(T);
				p1.TransformCoordinate(T);
				p2.TransformCoordinate(T);
				rasterizeGeo(p0, p1, p2,
					[&](int x, int y, float z) {
						float& _m1 = vf.value(x, y, 0);
						float& _m2 = vf.value(x, y, 1);
						_m1 = std::min(_m1, z);
						_m2 = std::max(_m2, z);
					});
			}
			i += n;
		}

		float average = 0;
		float aL = 0;
		int ns = 0;
		vf.operate(
			[&](int x, int y) {
				auto& a = vf.getV2(x, y);
				if (a.x <= a.y) {
					average += (a.x + a.y) / 2;
					aL += a.y - a.x;
					ns++;
				}
			}, false);
		if (ns > 0) {
			average /= ns;
			aL /= ns;
			aL /= 300.0;
			int fail = 0;
			int nfail = 0;
			vf.operate(
				[&](int x, int y) {
					cVec2 mm1(FLT_MAX, -FLT_MAX);
					cVec2 mm2(FLT_MAX, -FLT_MAX);
					for (int dy = -1; dy <= 1; dy++) {
						for (int dx = -1; dx <= 1; dx++) {
							int xx = x + dx;
							int yy = y + dy;
							cVec2& m = vf.getV2(xx, yy);
							if (m.y > m.x) {
								float m1 = average - m.x;
								float m2 = m.y - average;
								mm1.x = std::min(mm1.x, m1);
								mm2.x = std::min(mm2.x, m2);
								mm1.y = std::max(mm1.y, m1);
								mm2.y = std::max(mm2.y, m2);
							}
						}
					}
					if (mm1.x < mm1.y && mm2.x < mm2.y) {
						if (mm1.y + aL < mm2.x - aL || mm2.y + aL < mm1.x - aL) {
							fail++;
						}
						else nfail++;
					}
				}, false);
			if (fail < nfail / 500) {
				std_scoped_lock l(m);
				Vector3D cm(0);
				cm[symm] = average * L / 512;
				planes.Add(comms::cPlane(cm, p));
			}
		}
	}_std_for_end;
}

void cMeshContainer::Weld(float distance, UnlimitedBitset* Selected, cList<int>* rawencode) {
	cList<int> replace;
	replace.Add(0, m_Positions.Count());
	cList<int> ids;
	FloatPointHash fph;
	if (rawencode) {
		rawencode->SetCount(m_Raw.Count());
		for (int i = 0; i < m_Raw.Count(); i++) {
			(*rawencode)[i] = i;
		}
	}
	fph.SetHashParam(m_Positions.Count() + 1, distance);
	for (int i = 0; i < m_Positions.Count(); i++) {
		int v1 = i;
		if (Selected == NULL || Selected->get(i)) {
			int k = fph.AddPoint(m_Positions[i], 0);
			if (k < ids.Count()) {
				v1 = ids[k];
			}
			else {
				ids.Add(0, k + 1 - ids.Count());
				ids[k] = i;
			}
		}
		replace[i] = v1;
	}
	for (int k = 0; k < m_Raw.Count(); k++) {
		int n = m_Raw[k][0];
		for (int j = 0; j < n; j++) {
			m_Raw[k + j + 1][0] = replace[m_Raw[k + j + 1][0]];
		}
		k += n;
	}
	for (int k = 0; k < m_Raw.Count(); k++) {
		int& n = m_Raw[k][0];
		bool fail = false;
		bool changes = false;
		do {
			changes = false;
			for (int j = 0; j < n; j++) {
				for (int p = j + 1; p < n; p++) {
					if (m_Raw[k + j + 1][0] == m_Raw[k + p + 1][0]) {
						n--;
						m_Raw.RemoveAt(k + p + 1, 1);
						if(rawencode)rawencode->RemoveAt(k + p + 1, 1);
						changes = true;
						break;
					}
				}
				if (n == 0)fail = true;
				if (fail)break;
			}
		} while (changes);
		if (fail) {			
			if (rawencode)rawencode->RemoveAt(k, n + 1);
			m_Raw.RemoveAt(k, n + 1);
			k--;
		}
		else {
			k += n;
		}
	}
}

void cMeshContainer::WeldEdges(float distance) {
	// first, let us find open edges
	void_hash<bi_int> eds;
	eds.set_table_size(m_Positions.Count());
	cList<std::tuple<bi_int, int, tri_int, int>> open;
	for (int k = 0; k < m_Raw.Count(); k++) {
		int n = m_Raw[k][0];
		for (int j = 0; j < n; j++) {
			int jn = (j + 1) % n;
			int v1 = m_Raw[k + j + 1][0];
			int v2 = m_Raw[k + jn + 1][0];
			eds.add_quick(bi_int(v1, v2));
		}
		k += n;
	}
	for (int k = 0; k < m_Raw.Count(); k++) {
		int n = m_Raw[k][0];
		for (int j = 0; j < n; j++) {
			int jn = (j + 1) % n;
			int v1 = m_Raw[k + j + 1][0];
			int v2 = m_Raw[k + jn + 1][0];
			if(!eds.get(bi_int(v2, v1))) {
				Vector3D pv1 = m_Positions[v1];
				Vector3D pv2 = m_Positions[v2];
				Vector3D c = m_Positions[v1] + m_Positions[v2];
				c /= distance;
				open.Add(std::tuple(bi_int(v1, v2), k, tri_int(ffloorf(c.x), ffloorf(c.y), ffloorf(c.z)), -1));
			}
		}
		k += n;
	}
	eds.clear();
	if(open.Count() == 0)return;
	cList<int> replace;
	replace.Add(-1, m_Positions.Count());
	uni_hash<int, tri_int> hash;
	hash.set_table_size(open.Count());

	for (int q = 0; q < open.Count(); q++) {
		auto& o= open[q];
		int k = std::get<1>(o);
		if (k != -1) {
			bi_int ed = std::get<0>(o);
			int n = m_Raw[k][0];
			for (int j = 0; j < n; j++) {
				int jn = (j + 1) % n;
				int v1 = m_Raw[k + j + 1][0];
				int v2 = m_Raw[k + jn + 1][0];
				bi_int e2(v1, v2);
				if (e2 == ed) {
					hash.add_quick(std::get<2>(o), q);
				}
			}
		}
	}

	bool changes = true;
	int steps = 0;

	auto repl = [&](int& v) {
		if (replace[v] == -1) return;
		do {
			int r = replace[v];
			if (r == -1 || r == v)return;
			v = r;
		} while (true);
	};

	auto setrepl =
		[&](int& v1, int& v2, int r) {
		if (v1 == -1) v1 = r;
		if (v2 == -1) v2 = r;
		r = std::min(v1, r);
		r = std::min(v2, r);
		if (v1 > r) {
			replace[v1] = r;
			v1 = r;
		}
		if (v2 > r) {
			replace[v2] = r;
			v2 = r;
		}
		};

	for (int q = 0; q < open.Count(); q++) {
		auto& o = open[q];
		int k= std::get<1>(o);
		if (k != -1) {
			bi_int ed = std::get<0>(o);
			int n = m_Raw[k][0];
			for (int j = 0; j < n; j++) {
				int jn = (j + 1) % n;
				int v1 = m_Raw[k + j + 1][0];
				int v2 = m_Raw[k + jn + 1][0];
				bi_int e2(v1, v2);
				if (e2 == ed) {
					Vector3D pv1 = m_Positions[v1];
					Vector3D pv2 = m_Positions[v2];
					Vector3D d12 = pv2 - pv1;

					Vector3D c = m_Positions[v1] + m_Positions[v2];
					c /= distance;

					int x = ffloorf(c.x - 0.5f);
					int y = ffloorf(c.y - 0.5f);
					int z = ffloorf(c.z - 0.5f);

					float min_dist = distance;
					bi_int opp(-1, -1);
					tri_int t;
					int idx = -1;

					for (int dx = 0; dx < 2; dx++) {
						for (int dy = 0; dy < 2; dy++) {
							for (int dz = 0; dz < 2; dz++) {
								tri_int tc(x + dx, y + dy, z + dz);
								scan_key(hash, tc, int * pb) {
									int f = std::get<1>(open[*pb]);
									if (f != -1) {
										auto& b = std::get<0>(open[*pb]);
										if (b.V1 != v2 || b.V2 != v1) {
											Vector3D pb1 = m_Positions[b.V1];
											Vector3D pb2 = m_Positions[b.V2];
											Vector3D n12 = pb2 - pb1;
											float dd = std::max(pv1.distance(pb2), pv2.distance(pb1));
											if (dd < min_dist && n12.dot(d12) < 0) {
												min_dist = dd;
												t = tc;
												opp = b;
												idx = *pb;
												changes = true;
											}
										}
									}
								}scan_end
							}
						}
					}
					if(idx != -1) {
						int vv1 = std::min(opp.V2, v1);
						setrepl(replace[opp.V2], replace[v1],vv1);
						int vv2 = std::min(opp.V1, v2);
						setrepl(replace[opp.V1], replace[v2], vv2);
						hash.del_elm(t, idx);
						hash.del_elm(std::get<2>(o), q);
						std::get<1>(o) = -1;
						std::get<1>(open[idx]) = -1;

						std::get<3>(open[idx]) = q;
						std::get<3>(open[q]) = idx;
					}
				}
			}
		}
	}
	for (int k = 0; k < m_Raw.Count(); k++) {
		int n = m_Raw[k][0];
		for (int j = 0; j < n; j++) {
			repl(m_Raw[k + j + 1][0]);
		}
		k += n;
	}
	//RemoveUnusedVerts(); 
}

void cMeshContainer::WeldNormals() {
	if (m_Normals.Count() > m_Positions.Count()) {
		uni_hash<bi_int, int> hash;
		hash.set_table_size(m_Positions.Count());
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			if (n > 0) {
				for (int j = 0; j < n; j++) {
					int v = m_Raw[i + j + 1][0];
					int vn = m_Raw[i + j + 1][2];
					if (vn != -1) {
						hash.add_uniq(v, bi_int(vn, vn));
					}
				}
			}
			i += n;
		}
		std_for(i, m_Positions.Count()) {
			if (hash.size(i) > 1) {
				scan_key(hash, i, auto * p1) {
					scan_key(hash, i, auto * p2) {
						if (p1 != p2 && p1->V2 < p2->V2) {
							Vector3D n1 = m_Normals[p1->V2];
							Vector3D n2 = m_Normals[p2->V2];
							if (n1.dot(n2) > 0.999f) {
								p2->V2 = p1->V1;
							}
						}
					} scan_end;
				}scan_end;
			}
		}std_for_end;
		BigDynArray<int> encode;
		encode.Add(-1, m_Normals.Count());
		for (int i = 0; i < m_Positions.Count(); i++) {
			scan_key(hash, i, auto * p) {
				if (p->V2 < p->V1)encode[p->V1] = p->V2;
			}scan_end;
		}
		BigDynArray<int> replace;
		replace.Add(-1, m_Normals.Count());
		int p = 0;
		for (int i = 0; i < m_Normals.Count(); i++) {
			if (encode[i] == -1) {
				replace[i] = p;
				if (p != i)m_Normals[p] = m_Normals[i];
				p++;
			}
		}
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			if (n > 0) {
				for (int j = 0; j < n; j++) {
					int& vn = m_Raw[i + j + 1][2];
					if (vn != -1) {
						while (encode[vn] != -1) {
							vn = encode[vn];
						}
						vn = replace[vn];
					}
				}
			}
			i += n;
		}
		if (p < m_Normals.Count())m_Normals.RemoveAt(p, m_Normals.Count() - p);
	}
}

float cMeshContainer::CalcVolume(const cVec3& start, const cVec3& dir) const {
	float v = 0;
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		if (n > 0) {
			float avh = 0;
			float s = 0;
			Vector3D p0 = m_Positions[m_Raw[i + 1][0]];
			for (int j = 0; j < n; j++) {
				Vector3D p = m_Positions[m_Raw[i + j + 1][0]];
				if (j > 1) {
					Vector3D pp = m_Positions[m_Raw[i + j][0]];
					Vector3D d1 = pp - p0;
					Vector3D d2 = p - p0;
					s += Vector3D::Cross(d2, d1).dot(dir) / 2.0;
				}
				avh += dir.dot(p - start);
			}
			v += s * avh / n;
		}
		i += n;
	}
	return v;
}
cBounds cMeshContainer::CalcBoundBox() const {
	cBounds b;
	b.SetEmpty();
	for(int i=0;i<m_Positions.Count();i++){
		b.AddPoint(m_Positions[i]);
	}
	return b;
}
float cMeshContainer::CalcSquare() const {
	float s=0;
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		if(n>0){
			Vector3D p0=m_Positions[m_Raw[i+1][0]];
			for(int j=0;j<n;j++){
				Vector3D p=m_Positions[m_Raw[i+j+1][0]];
				if(j>1){
					Vector3D pp=m_Positions[m_Raw[i+j][0]];
					Vector3D d1=pp-p0;
					Vector3D d2=p-p0;
					s+=Vector3D::Cross(d2,d1).Length()/2.0;
				}
			}
		}
		i+=n;
	}
	return s;
}
float cMeshContainer::CalcSquareMC() const {
	float s=0;
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		if(n>0){
			Vector3D p0=m_Positions[m_Raw[i+1][0]];
			for(int j=0;j<n;j++){
				Vector3D p=m_Positions[m_Raw[i+j+1][0]];
				if(j>1){
					Vector3D pp=m_Positions[m_Raw[i+j][0]];
					Vector3D d1=pp-p0;
					Vector3D d2=p-p0;
					Vector3D d=Vector3D::Cross(d2,d1)/2.0;
					s+=__abs(d.x)+__abs(d.y)+__abs(d.z);				
				}
			}
		}
		i+=n;
	}
	return s;
}
struct ed_sequence;
typedef ed_sequence* lp_ed_sequence;
struct ed_sequence {
	int vertex:31;
	bool first_object:1;
	lp_ed_sequence next;
	lp_ed_sequence prev;
};
//#define dbg_chunks

void cMeshContainer::SemiBoolean(const std::function<float(cVec3, float&)>& this_sign, cMeshContainer& with,
	const std::function<float(cVec3, float&)>& with_sign) {
	auto m1 = with;
	cList<bi_int> edges1;
	cList<bi_int> edges2;
	m1.CutMesh(this_sign, edges1);
	CutMesh(with_sign, edges2);
	int nv1=m_Positions.Count();
	ConcateWith(&m1);

	//return;

	BigDynArray<ed_sequence> edges;
	uni_hash<int, int> verts_on_edges;
	uni_hash<int, int> side1_verts;
	uni_hash<int, int> side2_verts;
	uni_hash<int, int> all_verts;
	UnlimitedBitset side0;

	auto get_existing_vertex = [&](int vertex)->ed_sequence* {
		int* p = verts_on_edges.get(vertex);
		if (p) {
			return &edges[*p];
		}
		return nullptr;
	};

	auto get_vertex = [&](int vertex, bool side)->ed_sequence* {
		int* p = verts_on_edges.get(vertex);
		if (!p) {
			ed_sequence s;
			s.vertex = vertex;
			s.first_object = side;
			s.next = nullptr;
			s.prev = nullptr;
			if (side)side1_verts.add_quick(vertex, 0);
			else side2_verts.add_quick(vertex, 0);
			all_verts.add_quick(vertex, 0);
			if (side)side0.set(vertex, side);
			int pos = edges.Add(s);
			p = verts_on_edges.add_quick(vertex, pos);
		}
		return &edges[*p];
	};

	auto add_link = [&](int v1, int v2, bool side) {
		auto* e1 = get_vertex(v1, side);
		auto* e2 = get_vertex(v2, side);
		if(e1->next && e1->next != e2) {
			e1->next->prev = nullptr;
		}
		if(e2->prev && e2->prev != e1) {
			e2->prev->next = nullptr;
		}
		e1->next = e2;
		e2->prev = e1;
	};

	auto shrink = [&](lp_ed_sequence s) {
		if(s->next && s->next == s->prev) {
			auto n = s->next;
			if(n->prev == s) {
				s->next = s->prev = nullptr;
				n->next = n->prev = nullptr;

				if (s->first_object)side1_verts.del(s->vertex);
				else side2_verts.del(s->vertex);
				all_verts.del(s->vertex);

				if (n->first_object)side1_verts.del(n->vertex);
				else side2_verts.del(n->vertex);
				all_verts.del(n->vertex);
			}
		}
	};

	auto remove_vertex = [&](int vertex) {
		auto* ed = get_existing_vertex(vertex);
		if (ed && (ed->next || ed->prev)) {
			auto* n = ed->next;
			auto* p = ed->prev;
			if (ed->prev) {
				ed->prev->next = ed->next;
			}
			if (ed->next) {
				ed->next->prev = ed->prev;
			}
			ed->next = ed->prev = nullptr;
			if (ed->first_object)side1_verts.del(vertex);
			else side2_verts.del(vertex);
			all_verts.del(vertex);
			if (n)shrink(n);
			if (p)shrink(p);
		}
	};

	float av_len1 = 0;
	float av_len2 = 0;
	int n1 = 0;
	int n2 = 0;
	bool changes = false;

	for(auto& a : edges1) {
		a.V1 += nv1;
		a.V2 += nv1;
		if (a.V1 != a.V2) {
			side0.set(a.V1, true);
			side0.set(a.V2, true);
			add_link(a.V1, a.V2, true);
			av_len1 += (GetPosition(a.V1) - GetPosition(a.V2)).Length();
			n1++;
		}
	}
	for (auto& a : edges2) {
		if (a.V1 != a.V2) {
			add_link(a.V1, a.V2, false);
			av_len2 += (GetPosition(a.V1) - GetPosition(a.V2)).Length();
			n2++;
		}
	}
#ifdef dbg_chunks
	//check if all closed
	int n_open = 0;
	for(auto& e:edges) {
		if(!e.prev ||!e.next) {
			n_open++;
		}
	}
#endif //dbg_chunks
	
	if (n1)av_len1 /= n1;
	if (n2)av_len2 /= n2;
	float max_len = std::max(av_len1, av_len2) * 6;
#ifdef dbg_chunks
	auto oneassert = [&](bool r, int vc)
	{
		static bool one = true;
		if (!r) {
			//	assert(r);
			AllowDebug(true);
			if (one) {
				one = false;
				static int L = 0;
				DbgLayer("L" + cStr::ToString(L));
				AddDbgPoint(GetPosition(vc), 0xFFFFFFFF);
				auto* ed = get_existing_vertex(vc);
				if(ed) {
					auto* e1 = ed;
					do {
						if (e1->next) {
							AddDbgLine(GetPosition(e1->vertex), GetPosition(e1->next->vertex), 0xFFFF0000, 0xFF0000FF);
							if(e1->next->prev != ed) {
								break;
							}
						}
						e1 = ed->next;
					} while (e1 && e1 != ed);
				}
			}
		}
	};
	auto test_if_closed = [&](int v)	{
		auto* ed = get_existing_vertex(v);
		auto* e1 = ed;
		do {
			if (e1->next && e1->next->prev != e1) {
				oneassert(false, v);
			}
			e1 = e1->next;
		} while (e1 && e1 != ed);
		oneassert(e1 != nullptr, v);
	};
#endif //dbg_chunks
	cList<std::pair<lp_ed_sequence, lp_ed_sequence>> hot_edges;
	int tick = 0;
	
	do {
		changes = false;
		scan(side1_verts, auto* pe, auto* pk) {
			auto* ed = get_existing_vertex(*pk);
			if(ed && ed->next && ed->next->first_object == ed->first_object) {
				bi_int a(ed->vertex, ed->next->vertex);
				Vector3D p1 = GetPosition(a.V1);
				Vector3D p2 = GetPosition(a.V2);

				float _min = max_len;
				lp_ed_sequence this_side = nullptr;
				lp_ed_sequence opp_side = nullptr;
				scan(side2_verts, auto * nx, auto * st) {
					auto* edo = get_existing_vertex(*st);
					if(edo && edo->prev && edo->first_object == edo->prev->first_object && edo->first_object != ed->first_object) {
						Vector3D o1 = GetPosition(edo->vertex);
						Vector3D o2 = GetPosition(edo->prev->vertex);
						float d = p1.distance(o2) + p2.distance(o1);
						if (d < _min && edo->prev->prev) {
							_min = d;
							this_side = ed;
							opp_side = edo->prev;
						}
					}
				}scan_end;
				if (this_side && opp_side) {

					Vector3D o1 = GetPosition(opp_side->vertex);
					Vector3D o2 = GetPosition(opp_side->prev->vertex);
					float d1 = p2.distance(o1);
					float d2 = p1.distance(o2);

					if (d1 < d2) {
						//join p2 - o1

						m_Raw.Add({ 3,0,0 });
						m_Raw.Add({ a.V2, -1, -1 });
						m_Raw.Add({ a.V1, -1,-1 });
						m_Raw.Add({ opp_side->vertex, -1, -1 });

						m_Raw.Add({ 3,0,0 });
						m_Raw.Add({ opp_side->vertex, -1, -1 });
						m_Raw.Add({ opp_side->prev->vertex, -1,-1 });
						m_Raw.Add({ a.V2, -1, -1 });

					}
					else {
						//join p1 - o2

						m_Raw.Add({ 3,0,0 });
						m_Raw.Add({ a.V2, -1, -1 });
						m_Raw.Add({ a.V1, -1,-1 });
						m_Raw.Add({ opp_side->prev->vertex, -1, -1 });

						m_Raw.Add({ 3,0,0 });
						m_Raw.Add({ opp_side->vertex, -1, -1 });
						m_Raw.Add({ opp_side->prev->vertex, -1,-1 });
						m_Raw.Add({ a.V1, -1, -1 });

					}
					this_side->next->prev = opp_side->prev;
					opp_side->prev->next = this_side->next;
					this_side->next = opp_side;
					opp_side->prev = this_side;

					hot_edges.Add({ this_side, opp_side });
					hot_edges.Add({ opp_side->prev, this_side->next });
					changes = true;

					break;
				}
				if (hot_edges.Count())break;
			}
		}scan_end;
		while (hot_edges.Count()) {
			float min_dst = max_len * 3;
			int hot_idx = -1;
			int sub_idx = -1;
			int next_v = -1;
			int rem_v = -1;
			int next_v_opp = -1;
			int nfail = 0;
			for (int i = 0; i < hot_edges.Count(); i++) {
				auto* v1 = hot_edges[i].first;
				auto* v2 = hot_edges[i].second;
				if (v2->next && v2->next->first_object == v2->first_object) {
					float d = GetPosition(v2->next->vertex).distance(GetPosition(v1->vertex));
					if (d < min_dst) {
						min_dst = d;
						hot_idx = i;
						sub_idx = 1;
						next_v = v2->next->vertex;
						rem_v = v2->vertex;
					}
				}
				if (v1->prev && v1->prev->first_object == v1->first_object) {
					float d = GetPosition(v1->prev->vertex).distance(GetPosition(v2->vertex));
					if (d < min_dst) {
						min_dst = d;
						hot_idx = i;
						sub_idx = 0;
						next_v = v1->prev->vertex;
						rem_v = v1->vertex;
					}
				}
			}
			if (hot_idx != -1) {
				auto& e = hot_edges[hot_idx];
								
				m_Raw.Add(cVec3i(3, 0, 0));
				m_Raw.Add(cVec3i(e.second->vertex, -1, -1));
				m_Raw.Add(cVec3i(e.first->vertex, -1, -1));
				m_Raw.Add(cVec3i(next_v, -1, -1));
				if (!sub_idx)e.first = e.first->prev;
				else e.second = e.second->next;
				remove_vertex(rem_v);
				changes = true;
			} else {
				hot_edges.Clear();
				break;
			}
		}
		if (!changes)tick++;
		else tick = 0;
	} while (tick < 16);
	if(all_verts.size()) {
		do {
			changes = false;
			float best_w = FLT_MAX;
			int v = -1;
			int vp = -1;
			int vn = -1;
			scan(all_verts, auto* e, auto* k) {
				auto* ed = get_existing_vertex(*k);
				if(ed && ed->next && ed->prev) {
					float L = GetPosition(ed->prev->vertex).distance(GetPosition(ed->next->vertex));
					if (L < best_w) {
						best_w = L;
						v = ed->vertex;
						vn = ed->next->vertex;
						vp = ed->prev->vertex;
					}
				}
			}scan_end;
			if (v != -1) {
				m_Raw.Add(cVec3i(3, 0, 0));
				m_Raw.Add(cVec3i(vn, -1, -1));
				m_Raw.Add(cVec3i(v, -1, -1));
				m_Raw.Add(cVec3i(vp, -1, -1));
				remove_vertex(v);
				changes = true;
			}
		} while (changes);
	}
}

void cMeshContainer::PlainSubdiv(){
	uni_hash<int,DWORDS2> ds;
	//ds.set_table_size();
}

typedef ::std::pair<Vector3D, Vector3D> PosNorm;
class BezierLine {
	Vector3D p[4];
	Vector3D pt01(const PosNorm& pt0, const PosNorm& pt1) {
		Vector3D d = pt1.first - pt0.first;
		Vector3D n = pt0.second;
		float L = d.Length();
		d.Normalize();
		Vector3D d0 = d;
		d -= n * n.dot(d);
		d.Normalize();
		float m = d.dot(d0);
		return pt0.first + d * L * m / 3.0f;
	}
public:
	BezierLine(const PosNorm& p0, const PosNorm& p1) {
		p[0] = p0.first;
		p[3] = p1.first;
		p[1] = pt01(p0, p1);
		p[2] = pt01(p1, p0);
	}
	BezierLine(const Vector3D& p0, const Vector3D& p1, const Vector3D& p2, const Vector3D& p3) {
		p[0] = p0;
		p[1] = p1;
		p[2] = p2;
		p[3] = p3;
	}
	BezierLine(Vector3D* pp) {
		for (int i = 0; i < 4; i++)p[i] = pp[i];
	}
	BezierLine() {
		p[0] = p[1] = p[2] = p[3] = Vector3D::Zero;
	}
	Vector3D get(float t) {
		float mt = 1.0f - t;
		float mt2 = mt * mt;
		float t2 = t * t;
		return mt * mt2 * p[0] + 3.0f * mt2 * t * p[1] + 3.0f * mt * t2 * p[2] + t2 * t * p[3];
	}
	Vector3D half() {
		return (p[0] + 3.0f * (p[1] + p[2]) + p[3]) / 8.0f;
	}
	Vector3D dir(float t) {
		float mt = 1.0f - t;
		return 3.0f * mt * mt * (p[1] - p[0]) + 6.0f * mt * t * (p[2] - p[1]) + 3.0f * t * t * (p[3] - p[2]);
	}
	Vector3D half_dir() {
		return 0.75f * (p[3] + p[2] - p[1] - p[0]);
	}
};
class TriangularPatch {
public:
	TriangularPatch(const Vector3D* poss, const Vector3D* norms, float L) :
		TriangularPatch(poss[0], norms[0], poss[1], norms[1], poss[2], norms[2], L) {};
	
	TriangularPatch(const PosNorm& np0, const PosNorm& np1, const PosNorm& np2, float L) :
		TriangularPatch(np0.first, np0.second, np1.first, np1.second, np2.first, np2.second, L) {};
	
	TriangularPatch(const Vector3D& p0, const Vector3D& n0, const Vector3D& p1, const Vector3D& n1, const Vector3D& p2, const Vector3D& n2, float L) {
		pos[0] = p0; pos[1] = p1; pos[2] = p2;
		Vector3D nrm[] = { n0, n1, n2 };
		
		if (L > 1.5) {			
			auto p01 = middle(0, 1, nrm);
			auto p12 = middle(1, 2, nrm);
			auto p20 = middle(2, 0, nrm);
			
			float L2 = L / 2.0f;
			subp[0] = new TriangularPatch(PosNorm(p0, n0), p01, p20, L2);
			subp[1] = new TriangularPatch(p01, PosNorm(p1, n1), p12, L2);
			subp[2] = new TriangularPatch(p20, p12, PosNorm(p2, n2), L2);
			subp[3] = new TriangularPatch(p12, p20, p01, L2);
		}
		else {
			subp[0] = subp[1] = subp[2] = subp[3] = nullptr;
		}		
	}
	~TriangularPatch() {
		for (int i = 0; i < 4; i++)if(subp[i])delete(subp[i]);
	}
	Vector3D get(float u, float v) {
		if (subp[0]) {
			u *= 2.0f;
			v *= 2.0f;
			if (u + v <= 1.0f)return subp[0]->get(u, v);
			if (u > 1.0f)return subp[1]->get(u - 1.0f, v);
			if (v > 1.0f)return subp[2]->get(u, v - 1.0f);
			return subp[3]->get(1.0f - u, 1.0f - v);
		}
		return pos[0] * (1.0f - u - v) + pos[1] * u + pos[2] * v;
	}
private:
	Vector3D pos[3];
	TriangularPatch* subp[4];
	
	PosNorm middle(int v0, int v1, Vector3D* nrm) {
		BezierLine BL(PosNorm(pos[v0],nrm[v0]), PosNorm(pos[v1], nrm[v1]));
		Vector3D p = BL.half();
		Vector3D d = BL.half_dir().ToNormal();
		Vector3D n = nrm[v0] + nrm[v1];
		n -= d * d.dot(n);
		return ::std::pair(p, n.ToNormal());
	}
	void dbg(Vector3D* pos, Vector3D* nrm) {
		//AllowDebug(true);
		//for (int k = 0; k < 3; k++) {
		// 	DbgLayer(cStr("P") + cStr::ToString(int(L * 4)));
		//	AddDbgPoint(pos[k], 0xFFFF0000);
		//	DbgLayer(cStr("N") + cStr::ToString(int(L * 4)));
		//	AddDbgLine(pos[k], pos[k] + nrm[k]*1, 0xFF00FF00, 0xFF00FF00);
		//	DbgLayer(cStr("E") + cStr::ToString(int(L * 4)));
		//	AddDbgLine(pos[k], pos[(k+1)%3], 0xFFFFFF00, 0xFFFFFF00);
		//}
	}	
};

class QuadPatch {
	Vector3D pos[4];
	QuadPatch* subp[4];
	PosNorm middle(const PosNorm& p0, const PosNorm& p1) {
		BezierLine BL(p0, p1);
		Vector3D p = BL.half();
		Vector3D d = BL.half_dir().ToNormal();
		Vector3D n = p0.second + p1.second;
		n -= d * d.dot(n);
		return ::std::pair(p, n.ToNormal());
	}
	PosNorm middle(const PosNorm& p0, const PosNorm& p1, const PosNorm& p2, const PosNorm& p3) {
		PosNorm c0 = middle(p0, p1);
		PosNorm c1 = middle(p2, p3);
		return PosNorm((c0.first + c1.first) * 0.5f, (c0.second + c1.second).ToNormal());
	}
public:
	/// 0----1
	/// |    |
	/// |    |
	/// 2----3
	QuadPatch(const PosNorm& p0, const PosNorm& p1, const PosNorm& p2, const PosNorm& p3, float L) {
		pos[0] = p0.first;
		pos[1] = p1.first;
		pos[2] = p2.first;
		pos[3] = p3.first;
		if (L > 1.5) {
			auto p01 = middle(p0, p1);
			auto p13 = middle(p1, p3);
			auto p32 = middle(p3, p2);
			auto p20 = middle(p2, p0);
			auto pc = middle(p01, p32, p20, p13);

			float L2 = L / 2.0f;
			subp[0] = new QuadPatch(p0, p01, p20, pc, L2);
			subp[1] = new QuadPatch(p01, p1, pc, p13, L2);
			subp[2] = new QuadPatch(p20, pc, p2, p32, L2);
			subp[3] = new QuadPatch(pc, p13, p32, p3, L2);
		}
		else {
			subp[0] = subp[1] = subp[2] = subp[3] = nullptr;
		}
	}
	Vector3D get(float u, float v) {
		if (subp[0]) {
			u *= 2.0f;
			v *= 2.0f;
			int idx = 0;
			if (v > 1.0f) {
				v--;
				idx += 2;
			}
			if (u > 1.0f) {
				u--;
				idx++;
			}
			return subp[idx]->get(u, v);
		}
		Vector3D du = pos[1] - pos[0];
		Vector3D dv = pos[2] - pos[0];
		Vector3D duv = pos[3] - pos[0] - du - dv;
		return pos[0] + du * u + dv * v + duv * u * v;
	}
	~QuadPatch() {
		for (int i = 0; i < 4; i++) {
			if (subp[i])delete(subp[i]);
		}
	}
};
	
void cMeshContainer::PatchedDivision(int dstTris, bool uniform, bool flat, int GpuNormalsRelax) {
	int n_initial = m_Positions.Count();
	auto correctDiv = [](int* nn) {
		int n[3] = { nn[0],nn[1],nn[2] };
		int id[3] = { 0, 1, 2 };
		bool c = false;
		do {
			c = false;
			for(int k=1;k<3;k++) {
				if(n[k]<n[k-1]) {
					::std::swap(n[k - 1], n[k]);
					::std::swap(id[k], id[k - 1]);
					c = true;
				}
			}
		} while (c);
		nn[id[0]] = nn[id[1]] = nn[id[2]];
	};
	if (!uniform) {
		Triangulate();
	}
	float L = 0;
	int nL = 0;
	int nT = 0;
	if (!m_Normals.Count())CalcNormals();
	int globaldiv = 0;
	uni_hash<int, tri_DWORD> div[3];
	uni_hash<int, DWORDS2> n_div;
	int nr0 = m_Raw.Count();
	int maxdiv = 12;
	if(uniform) {
		ToTriQuads();
		nr0 = m_Raw.Count();
		maxdiv = 100;
		int nt0 = 0;
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			nt0 += n - 2;
			i += n;
		}
		if (nt0 > dstTris / 1.2)return;
		do {
			globaldiv++;
			int nt = nt0 * (globaldiv + 1) * (globaldiv + 1);
			if (nt > dstTris / 1.2)break;
		} while (globaldiv < 200);
		int nt = nt0 * 3;
		for (int k = 0; k < 3; k++) {
			div[k].set_table_size(nt);
		}
		n_div.set_table_size(nt);
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			if (n == 3 || n == 4) {
				cVec3i vs[4];
				int ns[4];
				for (int k = 0; k < n; k++) {
					vs[k] = m_Raw[i + k + 1];					
				}
				for (int k = 0; k < n; k++) {
					DWORDS2 dd(vs[k][0], vs[(k + 1) % n][0]);
					n_div.add_once(dd, globaldiv);
				}
			}
		}
	}
	else {
		Triangulate();
		nr0 = m_Raw.Count();
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			if (n == 3) {
				Vector3D pos[3] = { m_Positions[m_Raw[i + 1][0]], m_Positions[m_Raw[i + 2][0]], m_Positions[m_Raw[i + 2][0]] };
				L += pos[0].distance(pos[1]) + pos[1].distance(pos[2]) + pos[2].distance(pos[0]);
				nL += 3;
				nT++;
			}
			i += n;
		}
		if (nL) {
			L /= nL;
			float lq = L / sqrt(dstTris / nT);
			for (int k = 0; k < 3; k++) {
				int realT = 0;
				for (int i = 0; i < m_Raw.Count(); i++) {
					int n = m_Raw[i][0];
					if (n == 3) {
						Vector3D pos[3] = { m_Positions[m_Raw[i + 1][0]], m_Positions[m_Raw[i + 2][0]], m_Positions[m_Raw[i + 2][0]] };
						float L[3] = { pos[0].distance(pos[1]), pos[1].distance(pos[2]), pos[2].distance(pos[0]) };
						int ns[3];
						for (int k = 0; k < 3; k++) {
							ns[k] = ::std::max(ffloorf(L[k] / lq + 0.5), 1);
							if (ns[k] > 12)ns[k] = 12;
						}
						correctDiv(ns);
						tMiniMesh* m = TPatch.TriangulatePatch(ns[0], ns[1], ns[2]);
						realT += m->Indices.Count() / 3;
						nL += 3;
					}
					i += n;
				}
				float dd = float(realT) / float(dstTris);
				if (dd > 1.1)lq *= 1.1;
				else if (dd < 0.9)lq /= 1.1;
				else break;
			}

			int nt = nT * 3;
			for (int k = 0; k < 3; k++) {
				div[k].set_table_size(nt);
			}
			n_div.set_table_size(nt);
			/// set the divisions count for each edge
			for (int k = 0; k < 3; k++) {
				for (int i = 0; i < nr0; i++) {
					int n = m_Raw[i][0];
					if (n == 3) {
						cVec3i vs[3];
						Vector3D pos[3];
						int ns[3];
						for (int k = 0; k < 3; k++) {
							vs[k] = m_Raw[i + k + 1];
							pos[k] = m_Positions[vs[k][0]];
						}
						for (int k = 0; k < 3; k++) {
							float L = pos[k].distance(pos[(k + 1) % 3]);
							int ndiv = ::std::max(ffloorf(L / lq + 0.5), 1) - 1;
							if (ndiv > maxdiv)ndiv = maxdiv;
							ns[k] = ndiv;
						}
						correctDiv(ns);
						int ns1[3];
						int* pns[3] = { nullptr, nullptr, nullptr };
						for (int k = 0; k < 3; k++) {
							DWORDS2 dd(vs[k][0], vs[(k + 1) % 3][0]);
							int* nd = n_div.get(dd);
							int ndiv = ns[k];
							if (!nd) {
								nd = n_div.add_quick(dd, ndiv);
							}
							else ndiv = *nd;
							ns1[k] = ndiv;
							pns[k] = nd;
						}
						correctDiv(ns1);
						for (int k = 0; k < 3; k++) {
							*pns[k] = ns1[k];
						}
					}
				}
			}
		}
	}
	int cpol = 0;
	for (int i = 0; i < nr0; i++) {
		int n = m_Raw[i][0];
		cpol++;
		if (n == 3 || n == 4) {
			cVec3i vs[4];
			Vector3D pos[4];
			Vector3D nrm[4];
			cVec2 uvs[4];
			bool hasn = true;
			bool hasu = true;
			for (int k = 0; k < n; k++) {
				vs[k] = m_Raw[i + k + 1];
				pos[k] = m_Positions[vs[k][0]];
				if (vs[k][2] != -1) {
					nrm[k] = m_Normals[vs[k][2]];
				}
				else hasu = false;
				if (vs[k][1] != -1) {
					uvs[k] = m_TexCoords[vs[k][1]];
				}
				else hasu = false;
			}
			
			int ns[4];
			int nn;
			
			if(uniform) {
				for (int k = 0; k < n; k++)ns[k] = globaldiv;
				nn = globaldiv;
			}
			else {
				for (int k = 0; k < n; k++) {
					DWORDS2 dd(vs[k][0], vs[(k + 1) % n][0]);
					int ndiv = 0;
					ns[k] = ndiv;
					int* nd = n_div.get(dd);
					if (!nd) {
						n_div.add_quick(dd, ndiv);
					}
					else ndiv = *nd;
					ns[k] = ndiv;
				}
				nn = ::std::max(::std::max(ns[0], ns[1]), ns[2]);
			}

			if (n == 3) {
				TriangularPatch NP(pos[0], nrm[0], pos[1], nrm[1], pos[2], nrm[2], flat ? 1 : nn + 1);

				pos[1] -= pos[0];
				pos[2] -= pos[0];
				nrm[1] -= nrm[0];
				nrm[2] -= nrm[0];
				uvs[1] -= uvs[0];
				uvs[2] -= uvs[0];

				tMiniMesh* m = TPatch.TriangulatePatch(ns[0], ns[1], ns[2]);
				int vv[4] = { 0, 1 + ns[0], 2 + ns[0] + ns[1], 3 + ns[0] + ns[1] + ns[2] };
				auto dovtx = [&](int idx, int q) -> int {
					Vector3D u = m->uvw[idx];
					if (q == 0) {
						return m_Positions.Add(NP.get(u.x, u.y));
					}
					if (q == 2) {
						return hasn ? m_Normals.Add((nrm[0] + nrm[1] * u.x + nrm[2] * u.y).ToNormal()) : -1;
					}
					if (q == 1) {
						return hasu ? m_TexCoords.Add(uvs[0] + uvs[1] * u.x + uvs[2] * u.y) : -1;
					}
					return -1;
				};
				if (m) {
					StackArray<cVec3i, 2048> posV;
					int n = m->uvw.Count();
					posV.Add(cVec3i(-1, -1, -1), n);
					for (int k = 0; k < n; k++) {
						for (int p = 0; p < 3; p++) {
							for (int q = 0; q < 3; q++) {
								if (vv[p] == k) {
									posV[k][q] = vs[p][q];
								}
								if (k > vv[p] && k < vv[p + 1]) {// in the middle between p and p+1
									int idx = k - vv[p] - 1;
									int maxi = vv[p + 1] - vv[p] - 2;
									tri_DWORD ddp(vs[(p + 1) % 3][q], vs[p][q], maxi - idx);
									tri_DWORD dd(ddp.V2, ddp.V1, idx);
									if (ddp.V1 != -1 && ddp.V2 != -1) {										
										int* pd = div[q].get(ddp);
										if (pd) {
											posV[k][q] = *pd;
										}
										else {
											posV[k][q] = dovtx(k, q);
											div[q].add(dd, posV[k][q]);
										}
									}
								}
								if (posV[k][q] == -1) {
									posV[k][q] = dovtx(k, q);
								}
							}
						}
					}
					for (int k = 0; k < m->Indices.Count(); k += 3) {
						m_Raw.Add(m_Raw[i]);
						for (int p = 0; p < 3; p++) {
							cVec3i vv = posV[m->Indices[k + p]];
							m_Raw.Add(vv);
						}
					}
				}
			}
			else if (n == 4) {				
				int ng = globaldiv + 1;
				QuadPatch QP(
					PosNorm(m_Positions[vs[0][0]], m_Normals[vs[0][2]]),
					PosNorm(m_Positions[vs[1][0]], m_Normals[vs[1][2]]),
					PosNorm(m_Positions[vs[3][0]], m_Normals[vs[3][2]]),
					PosNorm(m_Positions[vs[2][0]], m_Normals[vs[2][2]]),
					flat ?  1 : nn + 1);

				nrm[1] -= nrm[0];
				nrm[3] -= nrm[0];
				uvs[1] -= uvs[0];
				uvs[3] -= uvs[0];
				nrm[2] -= nrm[0] + nrm[1] + nrm[3];
				uvs[2] -= uvs[0] + uvs[1] + uvs[3];
				
				auto dovtx = [&](int x, int y, int q) -> int {
					float u = float(x) / ng;
					float v = float(y) / ng;
					if (q == 0) {
						return m_Positions.Add(QP.get(u, v));
					}
					if (q == 2) {
						return hasn ? m_Normals.Add((nrm[0] + nrm[1] * u + nrm[3] * v + nrm[2] * u * v).ToNormal()) : -1;
					}
					if (q == 1) {
						return hasu ? m_TexCoords.Add(uvs[0] + uvs[1] * u + uvs[3] * v + uvs[2] * u * v) : -1;
					}
					return -1;
				};
				
				StackArray<cVec3i, 2048> posV;
				for (int y = 0; y <= ng; y++) {
					for (int x = 0; x <= ng; x++) {
						cVec3i v(-1, -1, -1);
						for (int q = 0; q < 3; q++) {
							tri_DWORD b(-1, -1, -1);
							if (y == 0) {
								if (x == 0) v[q] = vs[0][q];
								else if (x == ng)v[q] = vs[1][q];
								else  b = tri_DWORD(vs[1][q], vs[0][q], ng - x - 1);
							}
							else
								if (y == ng) {
									if (x == 0) v[q] = vs[3][q];
									else if (x == ng)v[q] = vs[2][q];
									else b = tri_DWORD(vs[3][q], vs[2][q], x - 1);
								}
								else
									if (x == 0) b = tri_DWORD(vs[0][q], vs[3][q], y - 1);
									else if (x == ng) b = tri_DWORD(vs[2][q], vs[1][q], ng - y - 1);

							if (v[q] == -1) {
								if (b.V1 != -1) {
									int* pd = div[q].get(b);
									if (pd) {
										v[q] = *pd;
									}
									else {
										v[q] = dovtx(x, y, q);
										div[q].add(tri_DWORD(b.V2, b.V1, ng - b.V3 - 2), v[q]);
									}
								}
								else {
									v[q] = dovtx(x, y, q);
								}
							}
						}
						posV.Add(v);
					}
				}
				int dd[4] = { 0, 1, ng + 2, ng + 1 };
				for (int y = 0; y < ng; y++) {
					for (int x = 0; x < ng; x++) {
						m_Raw.Add(m_Raw[i]);
						int p0 = x + y * (ng + 1);
						for (int p = 0; p < 4; p++) {
							m_Raw.Add(posV[p0 + dd[p]]);
						}
					}
				}
			}
		}
		i += n;
	}
	m_Raw.RemoveAt(0, nr0);

	if (m_Normals.Count() && GpuNormalsRelax) {
		VecArray wnorm;		
		Texture2VNormals(wnorm, GpuNormalsRelax);
		cList<Vector4D> pos;
		cList<int> links;
		cList<DWORD> nrm;
		nrm.SetCount(m_Positions.Count(), 0);
		pos.SetCount(m_Positions.Count(), Vector4D::Zero);
		for (int i = 0; i < m_Positions.Count(); i++) {
			pos[i] = comms::cVec4(m_Positions[i], 0);
		}
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			for(int j = 0; j < n; j++) {
				int jn = (j + 1) % n;
				auto& v1 = m_Raw[i + j + 1];
				auto& v2 = m_Raw[i + jn + 1];
				int pv1 = v1[0];
				int pv2 = v2[0];
				if(!nrm[pv1]) {
					if(wnorm.Count())nrm[pv1] = V4D2DW(Vector4D(wnorm[v1[0]] * 126.0f + Vector3D(128), 0));
					else nrm[pv1 ] =V4D2DW(Vector4D(m_Normals[v1[2]]*126.0f + Vector3D(128), 0));
				}
				links.Add(pv1);
				links.Add(pv2);
			}
			i += n;
		}
		Normals2Geometry NG;
		NG.setup(pos, nrm, links);
		if (!NG.failed()) {
			NG.calculate(pos);
			if (!NG.failed()) {
				for (int i = 0; i < m_Positions.Count(); i++) {
					m_Positions[i] = pos[i].ToVec3();
				}
			}
		}
	}
}

Vector3D cMeshContainer::CalcMeshProjSquare(){
	Vector3D s(0);
	VecArray& Pos = GetPositions();
	auto& raw = GetRaw();
	int nr = raw.Count();
	for (int i = 0; i<nr-1; i++){
		int n = raw[i][0];
		if (n>2){
			int v0 = raw[i + 1][0];
			Vector3D p0 = Pos[v0];
			for (int j = 1; j<n; j++){
				int pv = raw[i + j][0];
				int cv = raw[i + j + 1][0];
				Vector3D p1 = Pos[pv];
				Vector3D p2 = Pos[cv];
				p1 -= p0;
				p2 -= p0;
				float sx = __abs(p1.y*p2.z - p1.z*p2.y) / 2.0;
				float sy = __abs(p1.x*p2.z - p1.z*p2.x) / 2.0;
				float sz = __abs(p1.y*p2.x - p1.x*p2.y) / 2.0;
				s += Vector3D(sx, sy, sz);
			}
		}
		i += n;
	}
	return s;
}
Vector3D cMeshContainer::CalcMeshProjSquare(const cMat4& M){
	Vector3D s(0);
	VecArray& Pos = GetPositions();
	auto& raw = GetRaw();
	int nr = raw.Count();
	for (int i = 0; i<nr; i++){
		int n = raw[i][0];
		if (n>2){
			int v0 = raw[i + 1][0];
			Vector3D p0 = Pos[v0];
			p0.TransformCoordinate(M);
			for (int j = 1; j<n; j++){
				int pv = raw[i + j][0];
				int cv = raw[i + j + 1][0];
				Vector3D p1 = Pos[pv];
				Vector3D p2 = Pos[cv];
				p1.TransformCoordinate(M);
				p2.TransformCoordinate(M);
				p1 -= p0;
				p2 -= p0;
				float sx = __abs(p1.y*p2.z - p1.z*p2.y) / 2.0;
				float sy = __abs(p1.x*p2.z - p1.z*p2.x) / 2.0;
				float sz = __abs(p1.y*p2.x - p1.x*p2.y) / 2.0;
				s += Vector3D(sx, sy, sz);
			}
		}
		i += n;
	}
	return s;
}
BYTE mc2d_num[16] = { 0, 1, 1, 1, 1, 1, 2, 1, 1, 2, 1, 1, 1, 1, 1, 0 };
BYTE mc2d_ed[] = {
	0, 0, 0, 0, 0, 0, 0, 0,//0
	2, 0, 0, 1, 0, 0, 0, 0,//1
	0, 1, 1, 3, 0, 0, 0, 0,//2
	2, 0, 1, 3, 0, 0, 0, 0,//3
	3, 2, 2, 0, 0, 0, 0, 0,//4
	3, 2, 1, 0, 0, 0, 0, 0,//5
	0, 1, 1, 3, 3, 2, 2, 0,//6
	3, 2, 1, 3, 0, 0, 0, 0,//7
	1, 3, 3, 2, 0, 0, 0, 0,//8
	2, 0, 0, 1, 1, 3, 3, 2,//9
	0, 1, 3, 2, 0, 0, 0, 0,//10
	2, 0, 3, 2, 0, 0, 0, 0,//11
	1, 3, 2, 0, 0, 0, 0, 0,//12
	1, 3, 0, 1, 0, 0, 0, 0,//13
	0, 1, 2, 0, 0, 0, 0, 0,//14
	0, 0, 0, 0, 0, 0, 0, 0 //15
};
bool eqf(float a, float b){
	return __abs(a - b)<0.001;
}
void cMeshContainer::ConvertPicToPolygones(comms::cImage& Img, float Thickness, bool Normalize){
	Thickness /= 2;
	int Lx = Img.GetWidth();
	int Ly = Img.GetHeight();
	if (Lx&&Ly){
		DWORD* pic = (DWORD*)Img.GetPixels();
		float* ptr = new float[(Lx + 4)*(Ly + 4)];
		memset(ptr, 0, (Lx + 4)*(Ly + 4) * 4);
		int ofs = 0;
		for (int dy = 0; dy<Ly; dy++){
			for (int dx = 0; dx<Lx; dx++){
				float V = 0;
				if (dx>0 && dy>0 && dx<Lx - 1 && dy<Ly - 1){
					float w = 0;
					for (int ddy = -1; ddy <= 1; ddy++){
						for (int ddx = -1; ddx <= 1; ddx++){
							int of2 = ofs + ddx*Lx + ddy;
							DWORD V2 = pic[of2];
							float wt = ddx*ddx + ddy*ddy + 1;
							wt = 1 / wt;
							V2 = (V2 >> 8) & 255;
							V += V2*wt;
							w += wt;
						}
					}
					V /= w;
				}
				if (V<127.5){
					if (V>125)V = 125;
				}
				else{
					if (V<130)V = 130;
				}
				ptr[(dy + 2)*(Lx + 4) + (dx + 2)] = V;
				ofs++;
			}
		}
		Lx += 4;
		Ly += 4;
		uni_hash<int, DWORDS2> VHash;
		uni_hash<bi_DWORD, int> EdHash;
		VecArray& pos = GetPositions();
		cList<DWORDS2> ipos;
		//creating contour
		for (int dx = 0; dx<Lx - 1; dx++){
			for (int dy = 0; dy<Ly - 1; dy++){
				float ed[4];
				int ofs = dx + dy*Lx;
				ed[0] = ptr[ofs];
				ed[1] = ptr[ofs + 1];
				ed[2] = ptr[ofs + Lx];
				ed[3] = ptr[ofs + Lx + 1];
				BYTE ms = int(ed[0] >= 128) + int(ed[1] >= 128) * 2 + int(ed[2] >= 128) * 4 + int(ed[3] >= 128) * 8;
				if (ms != 0 && ms != 15){
					int ned = mc2d_num[ms];
					int p = ms << 3;
					for (int i = 0; i<ned; i++){
						int va[4];
						cVec2 vps[4];
						float wt[4];
						for (int q = 0; q<4; q++){
							int vt = mc2d_ed[p + q];
							int ddx = vt & 1;
							int ddy = vt >> 1;
							int x1 = dx + ddx;
							int y1 = dy + ddy;
							int vv = x1 + y1*Lx;
							va[q] = vv;
							vps[q] = cVec2(x1, y1);
							float f = ed[vt];
							//if(f>124 && f<127.5)f=124;
							//if(f<132 && f>127.5)f=132;
							wt[q] = f - 127.5;
						}
						int v2[2];
						for (int q = 0; q<2; q++){
							int q2 = q * 2;
							DWORDS2 D(va[q2], va[q2 + 1]);
							int* vt = VHash.get(D);
							if (vt){
								v2[q] = *vt;
							}
							else{
								cVec2 vt = (wt[q2] * vps[q2 + 1] - wt[q2 + 1] * vps[q2]) / (wt[q2] - wt[q2 + 1]);
								Vector3D V = Vector3D(vt.y, vt.x, Thickness);
								int id = pos.Add(V);
								ipos.Add(D);
								V.z = -Thickness;
								pos.Add(V);
								//assert(V.x>0.1 && V.y>0.1);

								ipos.Add(D);
								VHash.add(D, id);
								v2[q] = id;
							}
						}
						bi_DWORD B(v2[1], v2[0]);
						EdHash.add(dx, B);
						assert(B.V1<pos.Count());
						assert(B.V2<pos.Count());
						p += 4;
					}
				}
			}
		}
		delete[]ptr;
		auto& raw = GetRaw();
		cList<int> Poly;
		cList<bi_DWORD> Used;
		for (int dx = 0; dx<Lx - 1; dx++){
			bool change = false;
			int na = 0;
			do{
				change = false;
				float x0 = dx;
				cVec3i h;
				if (EdHash.size(dx)){
					Poly.Clear();
					Used.Clear();
					int start = -1;
					int next = -1;
					scan_key(EdHash, dx, bi_DWORD* B){
						Vector3D V0 = pos[B->V1];
						if (eqf(V0.y, x0)){
							start = B->V1;
							next = B->V2;
							Used.Add(*B);
							break;
						}
					}scan_end;
					if (start == -1){
						scan_key(EdHash, dx, bi_DWORD* B){
							Vector3D V0 = pos[B->V2];
							if (eqf(V0.y, x0 + 1)){
								start = B->V1;
								next = B->V2;
								Used.Add(*B);
								break;
							}
						}scan_end;
					}
					if (start != -1){
						int start0 = start;
						Poly.Add(start);
						Poly.Add(next);
						bool error = false;
						bool closed = false;
						int natt = 0;
						do{
							Vector3D v1 = pos[next];
							DWORDS2 d1 = ipos[next];
							bool B1 = eqf(v1.y, x0 + 1);
							bool B2 = eqf(v1.y, x0);
							bi_DWORD BF;
							int n2 = -1;
							if (B1 || B2){
								float d = FLT_MAX;
								scan_key(EdHash, dx, bi_DWORD* B){
									Vector3D V1 = pos[B->V1];
									if ((B1 && V1.x >= v1.x && eqf(V1.y, x0 + 1)) || (B2 && V1.x <= v1.x && eqf(V1.y, x0))){
										float dd = B2 ? v1.x - V1.x : V1.x - v1.x;
										if (dd<d){
											d = dd;
											n2 = B->V1;
											BF = *B;
										}
									}
								}scan_end;
								if (n2 != -1){
									if (n2 == start0){
										n2 = -1;
										closed = true;
									}
									else{
										Poly.Add(n2);
										n2 = BF.V2;

									}
								}
							}
							else{
								scan_key(EdHash, dx, bi_DWORD* B){
									if (B->V1 == next){
										n2 = B->V2;
										BF = *B;
										break;
									}
								}scan_end;
							}
							if (n2 == -1){
								if (!closed)error = true;
							}
							else{
								Used.Add(BF);
								start = next;
								next = n2;
								if (next == start0){
									closed = true;
								}
								else{
									Poly.Add(next);
								}
							}
							natt++;
						} while (!(error || closed || natt>10000));
						//assert(closed);
						if (/*closed && */Poly.Count()){
							h[0] = Poly.Count();
							h[1] = h[2] = 0;
							raw.Add(h);
							cVec3i v;
							v[1] = v[2] = -1;
							int np = Poly.Count();
							for (int i = 0; i<np; i++){
								v[0] = Poly[np - i - 1];
								assert(v[0]<pos.Count());
								//assert(pos[v[0]].x>0.1 && pos[v[0]].y>0.1);
								raw.Add(v);
							}
							raw.Add(h);
							for (int i = 0; i<np; i++){
								v[0] = Poly[i] + 1;
								assert(v[0]<pos.Count());
								raw.Add(v);
							}
							h[0] = 4;
							for (int i = 0; i<Used.Count(); i++){
								raw.Add(h);
								bi_DWORD B = Used[i];
								v[0] = B.V2 + 1;
								assert(v[0]<pos.Count());
								raw.Add(v);
								v[0] = B.V1 + 1;
								assert(v[0]<pos.Count());
								raw.Add(v);
								v[0] = B.V1;
								assert(v[0]<pos.Count());
								raw.Add(v);
								v[0] = B.V2;
								assert(v[0]<pos.Count());
								raw.Add(v);
							}
						}
					}
					if (Used.Count()){
						change = true;
						for (int i = 0; i<Used.Count(); i++){
							EdHash.del_elm(dx, Used[i]);
						}
					}
				}
			} while (change && na++<10000);
		}
		if (Normalize){
			AABoundBox AB;
			AB.SetEmpty();
			for (int i = 0; i<pos.Count(); i++){
				AB.AddPoint(pos[i]);
			}
			float LY = AB.GetSizeY();
			Vector3D C = AB.GetCenter();
			C.z = 0;
			if (LY>0){
				LY = 1.0 / LY;
				for (int i = 0; i<pos.Count(); i++){
					Vector3D P = pos[i];
					pos[i] = (pos[i] - C)*LY + Vector3D(0.0012, 0.50013, 0.0011);
				}
			}
		}
	}
	cSurface s;
	GetMaterials().Add(s);
	cObject ob;
	GetObjects().Add(ob);
}
int _ddx[4] = { 0, 1, 1, 0 };
int _ddy[4] = { 0, 0, 1, 1 };
BYTE _msk[4] = { 1, 2, 8, 4 };
BYTE mc2d_tnum[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 2 };
BYTE mc2d_vnum[16] = { 0, 3, 3, 4, 3, 4, 6, 5, 3, 6, 4, 5, 4, 5, 5, 4 };
BYTE mc2d_vert[192] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //0
	0, 0, 0, 1, 0, 3, 0, 0, 0, 0, 0, 0, //1
	0, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, //2
	0, 0, 1, 1, 0, 3, 1, 2, 0, 0, 0, 0, //3
	3, 3, 0, 3, 2, 3, 0, 0, 0, 0, 0, 0, //4
	0, 0, 0, 1, 3, 3, 2, 3, 0, 0, 0, 0, //5
	0, 1, 1, 1, 1, 2, 0, 3, 2, 3, 3, 3, //6
	0, 0, 1, 1, 3, 3, 1, 2, 2, 3, 0, 0, //7
	2, 2, 2, 3, 1, 2, 0, 0, 0, 0, 0, 0, //8
	0, 0, 0, 1, 0, 3, 1, 2, 2, 2, 2, 3, //9
	0, 1, 1, 1, 2, 3, 2, 2, 0, 0, 0, 0, //10
	0, 0, 1, 1, 2, 2, 0, 3, 2, 3, 0, 0, //11
	0, 3, 1, 2, 3, 3, 2, 2, 0, 0, 0, 0, //12
	0, 0, 3, 3, 2, 2, 0, 1, 1, 2, 0, 0, //13
	1, 1, 2, 2, 3, 3, 0, 1, 0, 3, 0, 0, //14
	0, 0, 1, 1, 2, 2, 3, 3, 0, 0, 0, 0  //15
};
BYTE mc2d_tri[144] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, //0
	0, 1, 2, 0, 0, 0, 0, 0, 0, //1
	0, 1, 2, 0, 0, 0, 0, 0, 0, //2
	0, 1, 3, 0, 3, 2, 0, 0, 0, //3
	0, 1, 2, 0, 0, 0, 0, 0, 0, //4
	0, 1, 2, 2, 1, 3, 0, 0, 0, //5
	0, 1, 2, 3, 4, 5, 0, 0, 0, //6
	0, 1, 3, 0, 3, 4, 0, 4, 2, //7
	0, 1, 2, 0, 0, 0, 0, 0, 0, //8
	0, 1, 2, 3, 4, 5, 0, 0, 0, //9
	0, 1, 3, 0, 3, 2, 0, 0, 0, //10
	0, 1, 3, 3, 1, 4, 4, 1, 2, //11
	0, 1, 3, 0, 3, 2, 0, 0, 0, //12
	0, 3, 1, 1, 3, 4, 1, 4, 2, //13
	3, 0, 1, 3, 1, 4, 4, 1, 2, //14
	0, 1, 3, 3, 1, 2, 0, 0, 0  //15
};
void cMeshContainer::ConvertPicToGridPolygones(comms::cImage& Img, float Thickness, int nDiv, bool Normalize, bool DefAlign){
	Thickness /= 2;
	if (nDiv<1)nDiv = 1;
	int Lx = Img.GetWidth();
	int Ly = Img.GetHeight();
	if (Lx&&Ly){
		VecArray& pos = GetPositions();
		auto& raw = GetRaw();
		uni_hash<int, DWORDS2> eds;
		uni_hash<int, bi_DWORD> mide;
		eds.set_table_size(Lx*Ly + 1);
		mide.set_table_size((Lx + Ly) * 2 * nDiv + 1);
		for (int dy = -1; dy <= Ly; dy++){
			for (int dx = -1; dx <= Lx; dx++){
				int val[4];
				cVec2i vs[4];
				BYTE sg = 0;
				for (int k = 0; k<4; k++){
					vs[k] = cVec2i(dx + _ddx[k], dy + _ddy[k]);
					comms::cImage::PixelRgba8 px = Img.GetPixelRgba8(vs[k][0], vs[k][1]);
					val[k] = int(px.r + px.g + px.b)*px.a / 255 / 3;
					if (val[k]>127)sg |= _msk[k];
				}
				if (sg){
					int vid[6];
					bool ved[6];
					int nv = mc2d_vnum[sg];
					int s12 = sg * 12;
					for (int j = 0; j<nv; j++){
						int id1 = mc2d_vert[s12 + j + j];
						int id2 = mc2d_vert[s12 + j + j + 1];
						ved[j] = id1 != id2;
						int g1 = vs[id1][0] + vs[id1][1] * (Lx + 2);
						int g2 = vs[id2][0] + vs[id2][1] * (Lx + 2);
						DWORDS2 b(g1, g2);
						int* pid = eds.get(b);
						if (!pid){
							cVec2 v1(vs[id1][0], vs[id1][1]);
							cVec2 v2(vs[id2][0], vs[id2][1]);
							cVec2 vc = v1;
							if (id1 != id2){
								float w1 = float(val[id1]) / 255.0 - 0.5;
								float w2 = float(val[id2]) / 255.0 - 0.5;
								vc += (v2 - v1)*w1 / (w1 - w2);
							}
							if (Normalize && !DefAlign){
								vc.x = vc.x / Lx - 0.5;
								vc.y = -vc.y / Ly + 0.5;
							}
							int pp = pos.Add(Vector3D(vc.x, -Thickness, vc.y));
							pos.Add(Vector3D(vc.x, Thickness, vc.y));
							pid = eds.add_quick(b, pp);
						}
						vid[j] = *pid;
					}
					int nt = mc2d_tnum[sg] * 3;
					for (int j = 0; j<nt; j += 3){
						cVec3i v1(3, 0, 0);
						raw.Add(v1);
						cVec3i v2(0, -1, -1);
						int s0 = sg * 9;
						for (int k = 0; k<3; k++){
							int vc = mc2d_tri[s0 + j + k];
							v2[0] = vid[vc];
							raw.Add(v2);
						}
						raw.Add(v1);
						for (int k = 2; k >= 0; k--){
							v2[0] = vid[mc2d_tri[s0 + j + k]] + 1;
							raw.Add(v2);
						}
						for (int k = 0; k<3; k++){
							int vc = mc2d_tri[s0 + j + k];
							int vn = mc2d_tri[s0 + j + (k + 1) % 3];
							if (ved[vc] && ved[vn]){
								for (int q = 0; q<nDiv + 2; q++){
									int vs[4];
									int vt[4] = { 0, 0, 0, 0 };
									int v1[4];
									int v2[4];

									if (q == 0){
										vs[0] = vid[vn];
										vs[1] = vid[vc];
									}
									else{
										vt[1] = vt[0] = q;
										v1[0] = vid[vn];
										v2[0] = vid[vn] + 1;
										v1[1] = vid[vc];
										v2[1] = vid[vc] + 1;
									}
									if (q == nDiv + 1){
										vs[2] = vid[vc] + 1;
										vs[3] = vid[vn] + 1;
									}
									else{
										vt[3] = vt[2] = q + 1;
										v1[2] = vid[vc];
										v2[2] = vid[vc] + 1;
										v1[3] = vid[vn];
										v2[3] = vid[vn] + 1;
									}
									for (int p = 0; p<4; p++){
										if (vt[p] != 0){
											bi_DWORD b(v1[p], vt[p]);
											int* pt = mide.get(b);
											if (!pt){
												float t = float(vt[p] - 1) / nDiv;
												if (vt[p] == 1)t = 0.05 / nDiv;
												if (vt[p] == nDiv + 1)t = 1.0 - 0.05 / nDiv;
												Vector3D ps = pos[v1[p]] * (1.0 - t) + pos[v2[p]] * t;
												int pp = pos.Add(ps);
												pt = mide.add_quick(b, pp);
											}
											vs[p] = *pt;
										}
										assert(vs[p] >= 0 && vs[p]<pos.Count());
									}
									cVec3i v4(4, 0, 0);
									raw.Add(v4);
									v4[0] = vs[0];
									v4[1] = v4[2] = -1;
									raw.Add(v4);
									v4[0] = vs[1];
									raw.Add(v4);
									v4[0] = vs[2];
									raw.Add(v4);
									v4[0] = vs[3];
									raw.Add(v4);
								}
							}
						}
					}
				}
			}
		}
		if (Normalize && DefAlign){
			AABoundBox AB;
			AB.SetEmpty();
			for (int i = 0; i<pos.Count(); i++){
				::std::swap(pos[i].x, pos[i].y);
				::std::swap(pos[i].z, pos[i].x);
				//pos[i].y*=-1;
				AB.AddPoint(pos[i]);
			}
			float LY = AB.GetSizeY();
			Vector3D C = AB.GetCenter();
			C.z = 0;
			if (LY>0){
				LY = 1.0 / LY;
				for (int i = 0; i<pos.Count(); i++){
					Vector3D P = pos[i];
					pos[i] = (pos[i] - C)*LY + Vector3D(0.0012, 0.50013, 0.0011);
				}
			}
		}
		SetDefaultObjMtl();
	}
}
void cMeshContainer::ConvertPicToGridPolygonesWithTapering(comms::cImage& ImgT, comms::cImage& ImgB, float Thickness, int nDiv, bool Normalize, float Angle, float Weight){
	Thickness /= 2;
	if (nDiv<1)nDiv = 1;
	int Lx = ::std::max(ImgT.GetWidth(), ImgB.GetWidth());
	int Ly = ::std::max(ImgT.GetHeight(), ImgB.GetHeight());
	if (Weight>0.99)Weight = 0.99;
	if (Weight<0.01)Weight = 0.01;
	Weight = 1 - Weight;
	if (Lx&&Ly){
		CurveBuffer cbt;
		CurveBuffer cbb;
		cList<OneSelPoint> top;
		cList<OneSelPoint> btm;
		Rct R(0, 0, 0, 0);
		ConvertPicToContour(ImgT, top, R);
		ConvertPicToContour(ImgB, btm, R);
		cbt.CreateSignedCurveBuffer(top);
		cbb.CreateSignedCurveBuffer(btm);
		VolumeObject vo;
		int Nx = (Lx + 7) / 8 + 1;
		int Nz = (Ly + 7) / 8 + 1;
		int Ny = (nDiv + 2 + 7) / 8 + 1;
		for (int cx = -3; cx <= Nx + 2; cx++){
			for (int cy = -1; cy <= Ny; cy++){
				for (int cz = -3; cz <= Nz + 2; cz++){
					VolumeCell* vc = vo.GetCell(cx, cy, cz, true);
					if (vc){
						if (!vc->Values)vc->Values = vo.AllocVox();
						int ofs = 0;
						vo.MarkDirtyCell(cx, cy, cz);
						for (int dz = 0; dz<9; dz++){
							for (int dy = 0; dy<9; dy++){
								for (int dx = 0; dx<9; dx++){
									int x = cx * 8 + dx;
									int y = cy * 8 + dy;
									int z = cz * 8 + dz;
									float d1 = cbt.GetIDist(x, z);
									float d2 = cbb.GetIDist(x, z);
									float tz = float(y + 0.05) / (nDiv + 0.1);
									float tt = tz;
									if (tt<0)tt = 0;
									if (tt>1)tt = 1;
									float tw = tt;
									if (Weight != 0.5)tw = pow(tw, Weight / (1.0f - Weight));
									float d = d1*tw + d2*(1 - tw) + (1 - tt)*Angle*(nDiv - 4);
									d = 32767.999 + d*1024.0;
									//if(tz<-0.0001 || tz>1.0001)d=0;
									if (y<-1 || y>nDiv + 1)d = 0;
									if (d<0)d = 0;
									if (d>65535)d = 65535;
									vc->Values[ofs++] = ffloorf(d);
									vc->HaveFilled = vc->HaveEmpty = true;
								}
							}
						}
					}
				}
			}
		}
		vo.UpdateMesh(true);
		vo.SetTransform(Matrix4D::Identity, false);
		TriangulationContext TC;
		vo.PerformGlobalTriangulation(&TC);
		TC.ToRawMesh(this);
		VecArray& pos = GetPositions();
		auto& raw = GetRaw();
		static float du = -2.0;
		static float dv = -1.0;
		for (int i = 0; i<pos.Count(); i++){
			pos[i].x = ((pos[i].x + du) / Lx)*2.0 - 1.0;
			pos[i].z = 1.0 - ((pos[i].z + dv) / Ly)*2.0;
			pos[i].y = ((pos[i].y + 0.05)*2.0 / (nDiv + 0.1)) - 1.0;
			if (pos[i].y<-1)pos[i].y = -1;
			if (pos[i].y>1)pos[i].y = 1;
			pos[i] /= 2.0;
		}
		SetDefaultObjMtl();
	}
}
void cMeshContainer::SplitDisconnected(cList<cMeshContainer*>& res){
	uni_hash<int,DWORDS2> fone;
	fone.set_table_size(m_Raw.Count()/2+1);
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		for(int k=0;k<n;k++){
			int vc=m_Raw[i+1+k][0];
			int vn=m_Raw[i+1+(k+1)%n][0];
			fone.add_quick(DWORDS2(vc,vn),i);
		}
		i+=n;
	}
	UnlimitedBitset used;
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		if(!used.get(i)){
			cList<cVec3i> rr;
			cList<int> cluster;
			cluster.Add(i);
			used.set(i,true);
			for(int j=0;j<cluster.Count();j++){
				int f=cluster[j];
				int nn=m_Raw[f][0];
				for(int k=0;k<nn;k++){
					int vc=m_Raw[f+1+k][0];
					int vn=m_Raw[f+1+(k+1)%nn][0];
					scan_key(fone,DWORDS2(vc,vn),int* pf){
						if(used.get(*pf)==false){
							used.set(*pf,true);
							cluster.Add(*pf);
						}
					}scan_end;
				}
			}
			cMeshContainer* mm=new cMeshContainer;
			mm->Copy(*this);
			mm->m_Raw.Clear();
			for(int j=0;j<cluster.Count();j++){
				int f=cluster[j];
				int nn=m_Raw[f][0];
				for(int k=0;k<=nn;k++){
					mm->m_Raw.Add(m_Raw[f+k]);
				}
			}
			mm->RemoveUnusedVerts();
			mm->RemoveUnusedObjMtl();
			res.Add(mm);
		}
		i+=n;
	}	
}
struct pattr{
	cVec2 uv;
	int tIndex;
	int ob;
	int mtl;
};
void cMeshContainer::RemoveRedundantUV(){
	uni_hash<pattr,int> vattr;
	vattr.set_table_size(m_Positions.Count()+1);
	UnlimitedBitset UsedUV;
	cList<int> enc;
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		int mt=m_Raw[i][1];
		int ob=m_Raw[i][2];
		for(int j=0;j<n;j++){
			int u=m_Raw[i+1+j][1];
			int p=m_Raw[i+1+j][0];
			if(u!=-1){
				cVec2 tc=m_TexCoords[u];
				int du=-1;
				scan_key(vattr,p,pattr* ppa){
					if(cVec2::Distance(tc,ppa->uv)<0.0001){
						if(ppa->ob==ob && ppa->mtl==mt){
							du=ppa->tIndex;
						}
					}
				}scan_end;
				if(du==-1){
					pattr pp;
					pp.mtl=mt;
					pp.ob=ob;
					pp.tIndex=u;
					pp.uv=tc;
					vattr.add_quick(p,pp);
					UsedUV.set(u,true);
				}else{
					m_Raw[i+1+j][1]=du;
					//AddDbgPoint((m_Positions[p]-Vector3D(-0.61837620,-0.44581380,0.70066398))*248.33548,0xFFFF0000);
				}
			}
		}
		i+=n;
	}
	int p=0;
	enc.Add(-1,m_TexCoords.Count());
	for(int i=0;i<m_TexCoords.Count();i++){
		if(UsedUV.get(i)){
			m_TexCoords[p]=m_TexCoords[i];
			enc[i]=p++;
		}
	}
	if(p<m_TexCoords.Count())m_TexCoords.RemoveAt(p,m_TexCoords.Count()-p);
	for(int i=0;i<m_Raw.Count();i++){
		int n=m_Raw[i][0];
		int mt=m_Raw[i][1];
		int ob=m_Raw[i][2];
		for(int j=0;j<n;j++){
			int& u=m_Raw[i+1+j][1];
			if(u!=-1){
				u=enc[u];
			}
		}
		i+=n;
	}	
}  
void cMeshContainer::Copy(const cMeshContainer& src) {
	if(this != &src )
	{
		Clear();
		m_Positions.Copy(src.GetPositions());
		m_TexCoords.Copy(src.GetTexCoords());
		m_Normals.Copy(src.GetNormals());
		m_Tangents.Copy(src.GetTangents());
		m_Raw.Copy(src.GetRaw());
		m_Name.Copy(src.GetName());
		m_Objects.Copy(src.GetObjects());
		m_Materials.Copy(src.GetMaterials());
//		m_UVSets.Copy(src.GetUVSets());
	}
}

void cMeshContainer::PutOnGround(Matrix4D& M,Vector3D& mp,Vector3D& cm){
	float minY=FLT_MAX;
	int nvt=0;
	cm=Vector3D::Zero;
	int n=m_Positions.Count();
	mp=Vector3D::Zero;
	float miny=FLT_MAX;
	Vector4D s=M.GetCol1();
	for(int k=0;k<n;k++){
		Vector3D p=m_Positions[k];
		cm+=p;
		float p1=p.dot(s.ToVec3())+s.w;
		if(p1<miny){
			miny=p1;
			mp=p;
		}
	}
	if(n){
		cm/=n;
		cm.TransformCoordinate(M);
		mp.TransformCoordinate(M);
		cm.y-=minY;
		mp.y=0;
		Matrix4D m=Matrix4D::Translation(Vector3D(0,-minY,0));
		M*=m;
	}
}
void cMeshContainer::PutOnGround(Matrix4D& M){
	Vector3D c,p;
	PutOnGround(M,c,p);
}
void cMeshContainer::LayOnGround(Matrix4D& M){
	int n=180;
	Vector3D C0;
	for(int p=0;p<n;p++){
		Vector3D r,c;
		PutOnGround(M,r,c);
		if(p==0)C0=c;
		Vector3D R=Vector3D::Cross(c-r,Vector3D::AxisY);
		R.Normalize();
		float k=float(n-p)/n;
		float a=k*k*3.0;
		cMat4 m=cMat4::RotationAt(r,R,-a);
		cVec3 pp=c;
		pp.TransformCoordinate(m);
		if(p==n-1){
			m*=cMat4::Translation(C0.x-c.x,0,C0.z-c.z);
		}
		cVec3 pp1=c;
		pp1.TransformCoordinate(m);
		pp1-=c;
		M*=m;
	}
}
float PickTri(comms::cSeg& Ray, Vector3D pos1, Vector3D pos2, Vector3D pos3){
	float u, v, t;
	Vector3D Fm = Ray.GetFm();
	Vector3D Nm = Ray.GetNormal();
	float d = FLT_MAX;
	cVec3::RayTri(Fm, Nm, pos1, pos2, pos3, u, v, d);
	return d;
}
float cMeshContainer::PickObject(cSeg& Ray, const cMat4& M){
	auto& raw = GetRaw();
	VecArray& pos = GetPositions();
	float mind = FLT_MAX;
	for (int i = 0; i<raw.Count(); i++){
		int n = raw[i][0];
		if (n>2){
			Vector3D p0 = pos[raw[i + 1][0]];
			p0.TransformCoordinate(M);
			Vector3D p1 = pos[raw[i + 2][0]];
			p1.TransformCoordinate(M);
			for (int j = 2; j<n; j++){
				Vector3D pc = pos[raw[i + j + 1][0]];
				pc.TransformCoordinate(M);
				float d = PickTri(Ray, p0, p1, pc);
				if (d<mind)mind = d;
				p1 = pc;
			}
		}
		i += n;
	}
	return mind;
}

void cMeshContainer::PickObjectEdges(cSeg& Ray, const cMat4& M, cVec3& RayPt, cVec3& ObjPt) {
	auto& raw = GetRaw();
	VecArray& pos = GetPositions();
	float mind = FLT_MAX;

	for (int i = 0; i < raw.Count(); i++) {
		int n = raw[i][0];
		if (n > 2) {
			Vector3D p0 = pos[raw[i + 1][0]];
			p0.TransformCoordinate(M);
			Vector3D p1 = pos[raw[i + 2][0]];
			p1.TransformCoordinate(M);
			for (int j = 2; j < n; j++) {
				Vector3D pc = pos[raw[i + j + 1][0]];
				pc.TransformCoordinate(M);
				float d = PickTri(Ray, p0, p1, pc);
				if (d < mind) {
					mind = d;
					RayPt = ObjPt = Ray.GetFm()+d*Ray.GetNormal();
				}
				p1 = pc;
			}
		}
		i += n;
	}
	if(mind<FLT_MAX)return;
	dSeg dRay;
	dRay.SetFromRay(Ray.GetFm(), Ray.GetNormal());
	for (int i = 0; i < raw.Count(); i++) {
		int n = raw[i][0];
		if (n > 2) {
			for (int j = 0; j < n; j++) {
				Vector3D p1 = pos[raw[i + j + 1][0]];
				p1.TransformCoordinate(M);
				Vector3D p2 = pos[raw[i + (j + 1) % n + 1][0]];
				p2.TransformCoordinate(M);
				cVec3 p, q;
				if (p1.distance(p2) > 0) {
					comms::cSeg s;
					s.SetFromEnds(p1, p2);
					s.ClosestPoints(comms::cSeg::SegRay, s, Ray, p, q);
				} else {
					p = p1;
					q = Ray.ProjectPoint(comms::cSeg::Ray, p1).Point;
				}
				float d = p.distance(q);
				if (d < mind) {
					ObjPt = p;
					RayPt = q;
					mind = d;
				}
			}
		}
		i += n;
	}
}
/*
__postprocess(xcv) {
	if(_CTRL()) {
		ClearDbg();
		Vector3D p1 = Vector3D::RandNormal() * 200.0;
		Vector3D p2 = Vector3D::RandNormal() * 200.0;
		AddDbgLine(p1, p2, 0xFFFF0000, 0xFF00FF00);

		comms::cSeg Ray = IRS->GetPickRay(cVec2(Widgets::aMouseX, Widgets::aMouseY));
		AddDbgLine(Ray.GetFm(), Ray.GetFm() + Ray.GetNormal()*2000, 0xFFFFFFFF, 0xFFFFFFFF);

		comms::cSeg s;
		comms::cSeg dRay;
		dRay.SetFromRay(Ray.GetFm(), Ray.GetNormal());
		s.SetFromEnds(p1, p2);
		cVec3 p, q;
		s.ClosestPoints(comms::cSeg::SegRay,s, Ray, p, q);
		AddDbgLine(p,q,0xFF000000,0xFF0000FF);
	}
}
*/
void cMeshContainer::CutMesh(MeshCutter& cut){
    uni_hash<int,DWORDS2> vone;
    int nr=m_Raw.Count();
    int s=0;
    bool notify=cut.NeedToNotifyAboutOpenVerts();
    UnlimitedBitset newvrt;
    for(int i=0;i<nr;i++){
    	int n=m_Raw[i][0];
    	assert(n==3);
    	if(n==3){
#if 1
    		Vector3D p1=m_Positions[m_Raw[i+1][0]];
    		Vector3D p2=m_Positions[m_Raw[i+2][0]];
    		Vector3D p3=m_Positions[m_Raw[i+3][0]];
    		float ws[3]={cut.GetCutWeight(p1),cut.GetCutWeight(p2),cut.GetCutWeight(p3)};
#ifdef _DEBUG
			{
				static const DWORD  blue = 0xFF0000AA;
				static const DWORD  red  = 0xFFAA0000;
				DbgLayer( "Points from cMeshContainer::CutMesh()" );
				AddDbgPoint( p1,  (ws[ 0 ] > 0) ? blue : red );
				AddDbgPoint( p2,  (ws[ 1 ] > 0) ? blue : red );
				AddDbgPoint( p3,  (ws[ 2 ] > 0) ? blue : red );
				DbgLayer( NULL );
			}
#endif
    		if(ws[0]<=0 && ws[1]<=0 && ws[2]<=0){
    			m_Raw[i]=m_Raw[i+1]=m_Raw[i+2]=m_Raw[i+3]=cVec3i(0,0,0);
    		}else
    		if(!(ws[0]>=0 && ws[1]>=0 && ws[2]>=0)){
    			int n=int(ws[0]>=0)+int(ws[1]>=0)+int(ws[2]>=0);
    			int pm[3]={-1,-1,-1};
    			int nw=0;
    			for(int k=0;k<3;k++){
    				int k1=(k+1)%3;
    				float w1=ws[k ];
    				float w2=ws[k1];
    				if((w1<0 && w2>0) || (w1>0 && w2<0)){
    					int v1=m_Raw[i+1+k][0];
    					int v2=m_Raw[i+1+k1][0];
    					Vector3D P1=m_Positions[v1];
    					Vector3D P2=m_Positions[v2];
    					P1=P1+(P2-P1)*w1/(w1-w2);
    					DWORDS2 D(v1,v2);
    					int* pv=vone.get(D);
    					if(!pv){
    						int p=m_Positions.Add(P1);
    						if(notify)newvrt.set(p,true);
    						pv=vone.add_quick(D,p);
    						s++;
    						if(s>500){
    							vone.refine_table();
    							s=0;
    						}
    					} // if(!pv)
						// @todo optimize  Why 'if(pv)' is needed?
    					if(pv){
    						pm[k]=*pv;
    						nw++;
    					} // if(pv)
    				} // if((w1<0 && w2>0) || (w1>0 && w2<0))
    			} // for(int k=0;k<3;k++)
    			if(nw==1){
    				int p=0;
    				if(pm[2]!=-1)p=1;
    				if(pm[0]!=-1)p=2;
    				int p2=(p+1)%3;
    				int p3=(p+2)%3;
    				if(ws[p2]>0){
    					m_Raw[i+1+p3][0]=pm[p2];
    					assert(pm[p2]!=-1);
    				}else{
    					m_Raw[i+1+p2][0]=pm[p2];
    					assert(pm[p2]!=-1);
    				}
    				if(notify)newvrt.set(m_Raw[i+1+p][0],true);
    			}else  // if(nw==1)
    			if(nw==2){
    				if(n==1){
    					int p=0;
    					if(pm[0]!=-1 && pm[1]!=-1)p=1;
    					if(pm[1]!=-1 && pm[2]!=-1)p=2;
    					int p2=(p+1)%3;
    					int p3=(p+2)%3;
    					m_Raw[i+1+p2][0]=pm[p];
    					assert(pm[p]!=-1);
    					m_Raw[i+1+p3][0]=pm[p3];
    					assert(pm[p3]!=-1);
    				} // if(n==1)
    				if(n==2){
    					int p=0;
    					if(pm[0]!=-1 && pm[1]!=-1)p=1;
    					if(pm[1]!=-1 && pm[2]!=-1)p=2;
    					int p2=(p+1)%3;
    					int p3=(p+2)%3;
    					m_Raw[i+1+p][0]=pm[p3];
    					assert(pm[p3]!=-1);
    					cVec3i vv=m_Raw[i];
    					m_Raw.Add(vv);
    					cVec3i vv1=m_Raw[i+1+p2];
    					m_Raw.Add(vv1);
    					vv1=m_Raw[i+1+p];
    					m_Raw.Add(vv1);
    					vv1[0]=pm[p];
    					assert(pm[p]!=-1);
    					m_Raw.Add(vv1);
    				} // if(n==2)
    			} // if(nw==2)
    		} // else if(!(ws[0]>=0 && ws[1]>=0 && ws[2]>=0))
#endif
    	} // if(n==3)
    	i+=n;
    } // for(int i=0;i<nr;i++)

    int p=0;
    for(int i=0;i<m_Raw.Count();i++){
    	int n=m_Raw[i][0];
    	if(n>0){
    		if(p<i){
    			for(int j=0;j<=n;j++){
    				m_Raw[p+j]=m_Raw[i+j];
    			}
    		}
    		p+=n+1;
    	} // if(n>0)
    	i+=n;
    } // for(int i=0;i<m_Raw.Count();i++)

	if(p<m_Raw.Count()){
		m_Raw.RemoveAt(p,m_Raw.Count()-p);
	}
    cList<int> Enc;
    RemoveUnusedVerts(&Enc);
    if(notify){
    	for(int i=0;i<Enc.Count();i++){
    		if(Enc[i]!=-1 && newvrt.get(i)){
    			cut.OnOpenVertex(Enc[i],m_Positions[Enc[i]]);
    		}
    	}
    } // if(notify)
}

void cMeshContainer::CutMesh(const std::function<float(cVec3, float&)>& sign, cList<bi_int>& new_edges, cList<bi_int>* sources, float div_mod) {
	uni_hash<int,DWORDS2> vone;
       
    UnlimitedBitset newvrt;
	cList<tri_int> splits;
	std::mutex m;
	Triangulate();
	int nr = m_Raw.Count();
	BigDynArray<float, 8192> weights;
	weights.Add(0.0f, m_Positions.Count());

	std_for(i, weights.Count()) {
		float den = 1;
		float w = sign(m_Positions[i], den);
		weights[i] = w;
		//if (__abs(weights[i]) < 0.01)weights[i] = 0;
	}std_for_end;

	auto findmid = [&](Vector3D p1, float w1, Vector3D p2, float w2, float& m) ->Vector3D {
		Vector3D dd = p2 - p1;
		Vector3D c = p1 - dd * w1 / (w2 - w1);
		for (int p = 0; p < 10; p++) {
			float d = 1;
			float w = sign(c, d);
			if (__abs(w) < 1) return c;
			if (w * w1 < 0) {
				p2 = c;
				w2 = w;
			}
			else {
				if (w * w2 < 0) {
					p1 = c;
					w1 = w;
				}
				else {
					p2 = p1 + (p2 - p1) * 1.15f;
					w2 = w2 = sign(p2, d);
				}
			}
			m = d;
			dd = p2 - p1;
			c = p1 - dd * w1 / (w2 - w1);
		}
		return c;
	};

	cList<cVec3i> exRaw;
	//AllowDebug(true);
	
	std_for(i, nr/4){
		int r = i << 2;
		int n = m_Raw[r][0];
		if (n == 3) {
			int r1 = m_Raw[r][1];
			int r2 = m_Raw[r][2];
			int vs[3] = { m_Raw[r + 1][0],m_Raw[r + 2][0], m_Raw[r + 3][0] };
			int vmid[3] = { -1,-1,-1 };
			float ws[3] = { 0,0,0 };
			int nmid = 0;
			int last = 0;
			int nzero = 0;
			bool vis = false;
			float dmax = 1;
			for (int k = 0; k < 3; k++) {
				int kn = (k + 1) % 3;
				int v1 = vs[k];
				int v2 = vs[kn];
				float w1 = weights[v1];
				float w2 = weights[v2];
				if (w1 > 0)vis = true;
				if (w1 == 0)nzero++;
				ws[k] = w1;
				if (w1 * w2 < 0) {
					m.lock();
					int* mid = vone.get(DWORDS2(v1, v2));
					if (mid)vmid[k] = *mid;
					else {
						float d = 1;
						Vector3D p = findmid(m_Positions[v1], w1, m_Positions[v2], w2, d);
						dmax=std::max(dmax,d);
						vmid[k] = m_Positions.Add(p);
						vone.add_quick(DWORDS2(v1, v2), vmid[k]);
					}
					nmid++;
					last = k;
					m.unlock();
				}
			}
			if(vis && nzero >= 2) {
				for(int k=0;k<3;k++) {
					if(ws[k] == 0 && ws[(k+1)%3] == 0) {
						bi_int ed(vs[k], vs[(k + 1) % 3]);
						m.lock();
						new_edges.Add(ed);
						m.unlock();
					}
				}
			} else
			if (nmid == 1) {
				if (ws[last] < 0) {
					m_Raw[r + 1 + last][0] = vmid[last];
					m.lock();
					bi_int ed(m_Raw[r + 1 + (last + 2) % 3][0], vmid[last]);
					new_edges.Add(ed);
					m.unlock();
				}
				else {
					m_Raw[r + 1 + (last + 1) % 3][0] = vmid[last];
					m.lock();
					bi_int ed(vmid[last], m_Raw[r + 1 + (last + 2) % 3][0]);
					new_edges.Add(ed);
					m.unlock();
				}
			} else
				if(nmid==2) {
					for (int q = 0; q < 3; q++) {
						if (vmid[q] == -1) {
							int corner = (q + 2) % 3;
							int corner1 = q;
							int corner2 = (q + 1) % 3;

							int vv = vmid[corner];
							int vv2 = vmid[corner2];

							cVec3 pos1 = m_Positions[vv];
							cVec3 pos2 = m_Positions[vv2];

							int ndiv = int(dmax * pos1.distance(pos2));
							if(ndiv>0) {
								vis = false;
								int mids[8];
								if (ndiv > 3)ndiv = 3;
								if (ndiv < 1)ndiv = 1;
								ndiv = ndiv * 2 + 1;
								mids[0] = vv;
								mids[ndiv - 1] = vv2;
								cVec3 cpos = m_Positions[vs[corner]];
								cVec3 cpos1 = m_Positions[vs[corner1]];
								cVec3 cpos2 = m_Positions[vs[corner2]];
								float cw = ws[corner];
								float cw1 = ws[corner1];
								float cw2 = ws[corner2];

								for (int k = 1; k < ndiv - 1; k++) {
									float w = float(k) / (ndiv - 1);
									float wi = 1.0f - w;
									cVec3 mpos = cpos2 * w + cpos1 * wi;
									float dm = 1;
									mpos = findmid(mpos, cw2 * w + cw1 * wi, cpos, cw, dm);
									m.lock();
									mids[k] = m_Positions.Add(mpos);
									m.unlock();
								}
								m.lock();
								static int cnt = 0;
								int r = exRaw.Count();
								DWORD C = GetRandomColor();
								if (ws[corner] < 0) {
									//DbgLayer(cStr("faceA_") + cStr::ToString(cnt++));
									//C |= 0xFFFF0000;
									int vc1 = vs[corner1];
									int vc2 = vs[corner2];
									bool snap_to_1 = false;
									cVec3 n0 = cVec3::Cross(cpos1 - cpos, cpos2 - cpos).ToNormal();
									bool mid_done = false;
									for (int k = ndiv - 1; k > 0; k--) {
										exRaw.Add({ 3,r1,r2 });
										exRaw.Add({ mids[k],-1,-1 });
										exRaw.Add({ mids[k - 1],-1,-1 });
										bool mid = false;
										if (!snap_to_1) {
											cVec3 ep1 = m_Positions[mids[k]] - cpos1;
											cVec3 ep2 = m_Positions[mids[k - 1]] - cpos1;
											cVec3 d11 = (ep1 - cpos1).ToNormal();
											cVec3 d21 = (ep2 - cpos1).ToNormal();
											cVec3 d12 = (ep1 - cpos2).ToNormal();
											cVec3 d22 = (ep2 - cpos2).ToNormal();
											float cp1 = n0.dot(cVec3::Cross(d11, d21));
											float cp2 = n0.dot(cVec3::Cross(d12, d22));
											if (cp1 > cp2) {
												snap_to_1 = true;
												mid = true;
											}
										}

										exRaw.Add({ snap_to_1 ? vc1 : vc2,-1,-1 });
										if (mid) {
											mid_done = true;
											exRaw.Add({ 3,r1,r2 });
											exRaw.Add({ vc1,-1,-1 });
											exRaw.Add({ vc2,-1,-1 });
											exRaw.Add({ mids[k],-1,-1 });
										}
									}
									if (!mid_done) {
										snap_to_1 = true;
										exRaw.Add({ 3,r1,r2 });
										exRaw.Add({ vc1,-1,-1 });
										exRaw.Add({ vc2,-1,-1 });
										exRaw.Add({ mids[0],-1,-1 });
									}
								}else {
									//DbgLayer(cStr("faceB_") + cStr::ToString(cnt++));
									//C |= 0xFF0000FF;
									int vc = vs[corner];
									for(int k=1;k<ndiv;k++) {
										exRaw.Add({ 3,r1,r2 });
										exRaw.Add({ mids[k - 1],-1,-1 });
										exRaw.Add({ mids[k],-1,-1 });
										exRaw.Add({ vc,-1,-1 });
									}
								}
								/*
								for (int k = r; k < exRaw.Count(); k++) {
									int n= exRaw[k][0];
									for (int p = 0; p < n; p++) {
										Vector3D p1 = m_Positions[exRaw[k + p + 1][0]];
										Vector3D p2 = m_Positions[exRaw[k + (p + 1) % n + 1][0]];
										AddDbgPoint(p1, C);
										AddDbgLine(p1, p2, C, 0xFF000000);
									}
									k += n;
								}
								*/
								m.unlock();
								
							}
							else {
								if (ws[corner] < 0) {
									float L1 = pos1.distance(m_Positions[vs[corner2]]);
									float L2 = pos2.distance(m_Positions[vs[corner1]]);
									if (L1 < L2) {
										m_Raw[r + 1 + corner][0] = vv;
										m.lock();
										exRaw.Add(cVec3i(3, 0, 0));
										exRaw.Add(cVec3i(vv, -1, -1));
										exRaw.Add(cVec3i(vs[corner2], -1, -1));
										exRaw.Add(cVec3i(vv2, -1, -1));
										new_edges.Add(bi_int(vv2, vv));
										m.unlock();
									}
									else {
										m_Raw[r + 1 + corner][0] = vv2;
										m.lock();
										exRaw.Add(cVec3i(3, 0, 0));
										exRaw.Add(cVec3i(vv, -1, -1));
										exRaw.Add(cVec3i(vs[corner1], -1, -1));
										exRaw.Add(cVec3i(vv2, -1, -1));
										new_edges.Add(bi_int(vv2, vv));
										m.unlock();
									}
								}
								else {
									m_Raw[r + 1 + corner1][0] = vv;
									m_Raw[r + 1 + corner2][0] = vv2;
									m.lock();
									new_edges.Add(bi_int(vv, vv2));
									m.unlock();
								}
							}
							break;
						}
					}
				}
			if (!vis) {
				for (int p = 0; p < 4; p++) {
					m_Raw[r + p] = cVec3i(0, 0, 0);
				}
			}
		}
	}std_for_end;

	m_Raw.AddRange(exRaw);

    int p=0;
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		if (n > 0) {
			if (p < i) {
				for (int j = 0; j <= n; j++) {
					m_Raw[p + j] = m_Raw[i + j];
				}
			}
			p += n + 1;
		}
		i += n;
	}

	if (p < m_Raw.Count()) {
		m_Raw.RemoveAt(p, m_Raw.Count() - p);
	}
}

void cMeshContainer::FindCutEdges(const std::function<float(cVec3)>& getter, cList<std::pair<cVec3, cVec3>>& edges) {
	int nr = m_Raw.Count();
	cList<float> weights;
	weights.Add(0.0f, m_Positions.Count());
	std_for(i, weights.Count()) {
		weights[i] = getter(m_Positions[i]);
	}std_for_end;
	for (int i = 0; i < nr; i++) {
		int n = m_Raw[i][0];
		if (n == 3) {
			bool any_above = false;
			bool any_below = false;
			for (int j = 0; j < 3; j++) {
				float w = weights[m_Raw[1 + i + j][0]];
				if (w > 0)any_above = true;
				if (w <= 0)any_below = true;
			}
			if (any_above && any_below) {
				int vs[3] = { m_Raw[1 + i][0], m_Raw[2 + i][0], m_Raw[3 + i][0] };
				float ws[3] = { weights[vs[0]],weights[vs[1]],weights[vs[2]] };
				cVec3 pos[3] = { m_Positions[vs[0]],m_Positions[vs[1]],m_Positions[vs[2]] };						
				int nvec = 0;
				cVec3 vecs[2] = { cVec3(0),cVec3(0) };
				bool invert = false;
				for (int k = 0; k < 3; k++) {
					int kn = (k + 1) % 3;
					int v1 = vs[k];
					int v2 = vs[kn];
					float w1 = ws[k];
					float w2 = ws[kn];										
					ws[k] = w1;
					if (w1 * w2 <= 0) {
						Vector3D p = (pos[k] * w2 - pos[kn] * w1) / (w2 - w1);
						float wn = getter(p);
						if (wn * w1 < 0) {
							p = (pos[k] * wn - p * w1) / (wn - w1);
						}
						else
							if (wn * w2 < 0) {
								p = (pos[kn] * wn - p * w2) / (wn - w2);
							}
						if (nvec < 2) {
							if (nvec == 0 && w2 < w1)invert = true;
							vecs[nvec++] = p;
						}
					}
				}
				if (nvec == 2) {
					if (invert) {
						edges.Add(std::pair<Vector3D, Vector3D>(vecs[1], vecs[0]));
					}
					else {
						edges.Add(std::pair<Vector3D, Vector3D>(vecs[0], vecs[1]));
					}
				}
			}
		}
		i += n;
	}
}


void cMeshContainer::DivideMesh( MeshDivider< int >&  divider ) const {
	const VecArray&  pos = GetPositions();
	for (int i = 0;  i < pos.Count();  ++i) {
		const cVec3&  p = pos[ i ];
		const int  vertex = GetVertex( p );
		assert( vertex != -1 );
		divider( p, vertex );
	}
}


void cMeshContainer::init() {
	// Don't accepted: it's really dangerous change for the project.
	//CreateDefaultObjMtl( *this );
}




::std::ostream& operator<<(::std::ostream& out, const cMeshContainer& mc) {
	using namespace ::std;

	const ios::fmtflags  oldFlags = out.flags();
	const streamsize  oldPrecision = out.precision();
	const streamsize  oldWidth = out.width();
	out.precision(4);
	cout.setf(ios::fixed, ios::floatfield);

	out << "\tpositions\n";
	const VecArray&  pos = mc.GetPositions();
	for (int i = 0; i < pos.Count(); ++i) {
		out << "\t\t#" << i << "\t" << pos[i] << "\n";
	}

	out << "\traw\n";
	const auto&  raw = mc.GetRaw();
	int  q = 0;
	for (int i = 0; i < raw.Count(); ++i) {
		const int  n = raw[i][0];
		out << "\t\t#" << q << ":" << i << "\t" << raw[i] << "\t";
		ostringstream  ss;
		cVec3  ps = cVec3::Zero;
		bool  correctCenter = true;
		for (int j = 0; j < n; ++j) {
			const int  k = i + 1 + j;
			if (k < raw.Count()) {
				ss << raw[k];
				const int  p = raw[k][0];
				if ((p >= 0) && (p < pos.Count())) {
					ps += pos[p];
				}
				else {
					ss << '?';
					correctCenter = false;
				}
			}
			else {
				ss << '?';
				correctCenter = false;
			}
			ss << " ";
		}
		out << ss.str();
		if (correctCenter) {
			out << "\tcenter = " << (ps / n);
		}
		else {
			out << "\tundefined center";
		}
		out << "\n";
		i += n;
		++q;
	}

	out.flags(oldFlags);
	out.precision(oldPrecision);
	out.width(oldWidth);

	return out;
}

void cMeshContainer::SnapPointsSet(cList<::std::tuple<cVec3, cVec3, float>>& points, bool CheckNormalsDirections) {
	cList<fAB> abs;
	cList<int> tstart;
	auto& raw = GetRaw();
	for (int i = 0; i < raw.Count(); i++) {
		int n = raw[i][0];
		if (n == 3) {
			fAB fab;
			fab.ab.SetEmpty();
			fab.idx = tstart.Count();
			for (int k = 0; k < 3; k++) {
				fab.ab.AddPoint(GetPosition(raw[i + 1 + k][0]));
			}
			abs.Add(fab);
			tstart.Add(i);
		}
		i += n;
	}
	fAABBNodePool fp;
	fAABBNode root;
	root.AB.SetEmpty();
	root.Init(abs.ToPtr(), abs.Count(), 0, fp);
	int n = points.Count();
	::std::function<void(fAABBNode*, Vector3D&, Vector3D&, float&, Vector3D&)> intr = [&](fAABBNode* nd, Vector3D& p0, Vector3D& n, float& dmin, Vector3D& res) {
		float s = 0;
		if (nd->AB.RayIntersection(p0, n, s)) {
			if (__abs(s) <= dmin) {
				if (nd->TriIdx >= 0) {
					int s = tstart[nd->TriIdx];
					int nt = raw[s][0];
					if (nt == 3) {
						Vector3D p[3];
						for (int k = 0; k < 3; k++) {
							p[k] = GetPosition(raw[s + 1 + k][0]);
						}
						float u, v;
						float d = RayTri(p0, n, p[0], p[1], p[2], u, v);
						float da = __abs(d);
						if (da <= dmin) {
							if (!CheckNormalsDirections || n.dot(Vector3D::Cross(p[1] - p[0], p[2] - p[0])) > 0) {
								dmin = da;
								res = p0 + n * d;
							}
						}
					}
				}
				else {
					for (int k = 0; k < 2; k++) if (nd->m_children[k])intr(nd->m_children[k], p0, n, dmin, res);
				}
			}
		}
	};
	std_for(i, n) {
		auto& pt = points[i];
		float Ls = ::std::get<2>(pt);
		if (Ls > 0) {
			Vector3D& p0 = ::std::get<0>(pt);
			Vector3D nrm = ::std::get<1>(pt);
			float L = Ls;
			Vector3D npos = p0;
			intr(&root, p0, nrm, L, npos);
			if (L < Ls) {
				p0 = npos;
			}
		}
	} std_for_end;
}

void cMeshContainer::RemoveDisconnectedPieces(float percent_of_polycount, float percent_of_square) {
	CreateFone();
	const float max_island_square = CalcSquare() * percent_of_square / 100.0f;
	auto& raw = GetRaw();
	auto& pos = GetPositions();
	const int max_island = static_cast<int>(raw.Count() * percent_of_polycount / 400.0f);

	cList<int> order;
	UnlimitedBitset used;
	UnlimitedBitset remove;
	for (int i = 0; i < raw.Count(); i++) {
		int n = raw[i][0];
		if (!used.get(i)) {
			order.Clear();
			order.Add(i);
			used.set(i, true);
			float summ = 0;
			for (int j = 0; j < order.Count(); j++) {
				int f = order[j];
				int nv = raw[f][0];
				if (nv > 0) {
					Vector3D p0 = pos[raw[f + 1][0]];
					for (int k = 0; k < nv; k++) {
						Vector3D p = pos[raw[f + k + 1][0]];
						if (k > 1) {
							Vector3D pp = pos[raw[f + k][0]];
							Vector3D d1 = pp - p0;
							Vector3D d2 = p - p0;
							summ += Vector3D::Cross(d2, d1).Length() / 2.0;
						}
					}
				}
				for (int k = 0; k < nv; k++) {
					int v1 = raw[f + 1 + k][0];
					int v2 = raw[f + 1 + (k + 1) % nv][0];
					scan_key(fone, DWORDS2(v1, v2), auto * pf1) {
						if (!used.get(*pf1)) {
							used.set(*pf1, true);
							order.Add(*pf1);
						}
					}scan_end
				}
			}
			if (order.Count() < max_island && summ < max_island_square) {
				for (auto& r : order) {
					remove.set(r, true);
				}
			}
		}
		i += n;
	}
	RawArray raw1;
	raw1.EnsureCapacity(raw.Count());
	for (int i = 0; i < raw.Count(); i++) {
		int n = raw[i][0];
		if (!remove.get(i)) {
			for (int j = 0; j <= n; j++) {
				raw1.Add(raw[i + j]);
			}
		}
		i += n;
	}
	raw.Clear();
	raw.FastCopyFrom(raw1);
	RemoveUnusedVerts();
	fone.reset();
}

StaticMesh* cMeshContainer::CreateStaticMesh(){
	VecArray& Pos = GetPositions();
	CalcNormals();
	VecArray& N = GetNormals();
	auto& raw = GetRaw();
	int nt = 0;
	int np = Pos.Count();
	int nn = N.Count();
	int nr = raw.Count();
	for (int i = 0; i<nr; i++){
		int n = raw[i][0];
		if (n>2)nt += n - 2;
		i += n;
	}
	UnlimitedBitset b;
	cList<comms::cObject>& obs = GetObjects();
	for (int i = 0; i<obs.Count(); i++){
		if (obs[i].Name.Contains("_negative")){
			b.set(i, true);
		}
	}
	if (nt){
		StaticMesh* sm = new StaticMesh;
		sm->create(Pos.Count(), nt * 3, comms::cVertex::PositionNormalColored::FormatID);
		comms::cVertex::PositionNormalColored* vn = (comms::cVertex::PositionNormalColored*)sm->getVertexData();
		for (int i = 0; i<np; i++){
			vn[i].Pos = Pos[i];
			vn[i].Normal = N[i];
			Vector4D CL(N[i], 0);
			CL.ToVec3() *= 127.0;
			CL.ToVec3() += Vector3D(128);
			DWORD CDW = V4D2DW(CL);
			vn[i].Color = CDW;
		}
		DWORD* idx4 = NULL;
		WORD* idx2 = NULL;
		if (np<60000){
			idx2 = sm->getIndices();
		}
		else{
			idx4 = sm->getIndices4();
		}
		for (int i = 0; i<nr; i++){
			int p0 = 0;
			int pp = 0;
			int n = raw[i][0];
			bool neg = b.get(raw[i][2]);
			for (int j = 0; j<n; j++){
				int pc = raw[i + j + 1][0];
				if (neg){
					vn[pc].Color |= 0xFF000000;
				}
				if (j == 0)p0 = pc;
				if (j>1){
					if (idx2){
						idx2[0] = p0;
						idx2[1] = pp;
						idx2[2] = pc;
						idx2 += 3;
					}
					if (idx4){
						idx4[0] = p0;
						idx4[1] = pp;
						idx4[2] = pc;
						idx4 += 3;
					}
				}
				pp = pc;
			}
			i += n;
		}
		sm->setNInd(nt * 3);
		sm->setNVert(np);
		sm->unlock();
		static int sh = IRS->GetShaderID("mpreview1", comms::cVertex::PositionNormalColored::FormatID);
		sm->setShader(sh);
		return sm;
	}
	return NULL;
}
StaticMesh* cMeshContainer::CreateStaticMeshMC(){
	static DWORD CL2 = 0;
	static DWORD CL1 = 0x80FFFFFF;
	VecArray& Pos = GetPositions();
	//if (Pos.Count()>1000000)return CreateHardsurfaceStaticMesh();
	CalcNormals();
	VecArray& N = GetNormals();
	auto& raw = GetRaw();
	int nt = 0;
	int np = Pos.Count();
	int nn = N.Count();
	int nr = raw.Count();
	for (int i = 0; i<nr; i++){
		int n = raw[i][0];
		if (n>2)nt += n - 2;
		i += n;
	}
	if (nt){
		StaticMesh* sm = new StaticMesh;
		sm->create(Pos.Count(), nt * 3, MCVertex2::FormatID);
		MCVertex2* vn = (MCVertex2*)sm->getVertexData();
		for (int i = 0; i<np; i++){
			vn[i].Pos = Pos[i];
//#ifdef COLOR3
			//vn[i].Normal = N2DW(-N[i], 0);
			vn[i].Color = CL1;
			vn[i].Color2 = CL2;
//#else
			vn[i].Normal = -N[i];
//#endif
		}
		DWORD* idx4 = NULL;
		WORD* idx2 = NULL;
		if (np<60000){
			idx2 = sm->getIndices();
		}
		else{
			idx4 = sm->getIndices4();
		}
		for (int i = 0; i<nr; i++){
			int p0 = 0;
			int pp = 0;
			int n = raw[i][0];
			for (int j = 0; j<n; j++){
				int pc = raw[i + j + 1][0];
				if (j == 0)p0 = pc;
				if (j>1){
					if (idx2){
						idx2[0] = p0;
						idx2[1] = pp;
						idx2[2] = pc;
						idx2 += 3;
					}
					if (idx4){
						idx4[0] = p0;
						idx4[1] = pp;
						idx4[2] = pc;
						idx4 += 3;
					}
				}
				pp = pc;
			}
			i += n;
		}
		sm->setNInd(nt * 3);
		sm->setNVert(np);
		sm->unlock();
		return sm;
	}
	return NULL;
}
#define idx24(id,value) if(idx2)idx2[id]=value;if(idx4)idx4[id]=value;
	
StaticMesh* cMeshContainer::CreateStaticMeshMC_Shell(float In, float Out) {
	VecArray& Pos = GetPositions();
	CalcNormals();
	VecArray& N = GetNormals();
	auto& raw = GetRaw();
	int nt = 0;
	int np = Pos.Count();
	int nn = N.Count();
	int nr = raw.Count();
	for (int i = 0; i < nr; i++) {
		int n = raw[i][0];
		if (n > 2)nt += n - 2;
		i += n;
	}
	int nedges = 0;
	cList<int> Edges;
	static DWORD CL2 = 0;
	static DWORD CL1 = 0x80FFFFFF;
	scan(fone, int * pf, DWORDS2 * pe) {
		if(fone.size(*pe) == 1) {
			int nv = raw[*pf][0];
			for(int k=0;k<nv;k++) {
				int kn = (k + 1) % nv;
				int v1 = raw[(*pf) + k + 1][0];
				int v2 = raw[(*pf) + kn + 1][0];
				if(DWORDS2(v1,v2) == *pe) {
					Edges.Add(v2);
					Edges.Add(v1);
					nedges++;
					break;
				}
			}
		}
	}scan_end;
	if (nt) {
		StaticMesh* sm = new StaticMesh;
		sm->create(np*2 + nedges*4, (nt + nedges) * 18, MCVertex2::FormatID);
		MCVertex2* vn = (MCVertex2*)sm->getVertexData();
		for (int i = 0; i < np; i++) {
			MCVertex2& V1 = vn[i];
			V1.Pos = Pos[i] + N[i] * Out;
			V1.Color = CL1;
			V1.Color2 = CL2;
			V1.Normal = -N[i];
			MCVertex2& V2 = vn[i + np];
			V2.Pos = Pos[i] - N[i] * In;
			V2.Color = CL1;
			V2.Color2 = CL2;
			V2.Normal = N[i];
		}
		///edges
		int v0 = np * 2;
		float dd = (Out + In) * 0.25;
		for (int i = 0, v2 = 0; i < nedges; i++, v2 += 2) {
			MCVertex2* VV = vn + v0 + i * 4;
			int vv1 = Edges[v2];
			int vv2 = Edges[v2 + 1];
			Vector3D Nn = Vector3D::Cross(Pos[vv2] - Pos[vv1], N[vv1] + N[vv2]).ToNormal();
			VV->Pos = Pos[vv1] + N[vv1] * (Out - dd);
			VV->Color = CL1;
			VV->Color2 = CL2;
			VV->Normal = Nn;
			VV++;
			VV->Pos = Pos[vv2] + N[vv2] * (Out - dd);
			VV->Color = CL1;
			VV->Color2 = CL2;
			VV->Normal = Nn;
			VV++;
			VV->Pos = Pos[vv1] - N[vv1] * (In - dd);
			VV->Color = CL1;
			VV->Color2 = CL2;
			VV->Normal = Nn;
			VV++;
			VV->Pos = Pos[vv2] - N[vv2] * (In - dd);
			VV->Color = CL1;
			VV->Color2 = CL2;
			VV->Normal = Nn;
		}
		DWORD* idx4 = NULL;
		WORD* idx2 = NULL;
		if (np * 2 + nedges * 18 < 60000) {
			idx2 = sm->getIndices();
		}
		else {
			idx4 = sm->getIndices4();
		}
		int di = nt * 3;
		for (int i = 0; i < nr; i++) {
			int p0 = 0;
			int pp = 0;
			int n = raw[i][0];
			for (int j = 0; j < n; j++) {
				int pc = raw[i + j + 1][0];
				if (j == 0)p0 = pc;
				if (j > 1) {
					idx24(0, p0);
					idx24(1, pp);
					idx24(2, pc);
						 
					idx24(    di, pp + np);
					idx24(1 + di, p0 + np);
					idx24(2 + di, pc + np);

					if (idx2)idx2 += 3;
					if (idx4)idx4 += 3;
				}
				pp = pc;
			}
			i += n;
		}
		if (idx2)idx2 += nt * 3;
		if (idx4)idx4 += nt * 3;
		for (int i = 0, v4 = v0, v2 = 0; i < nedges; i++, v4 += 4, v2 += 2) {
			int v00 = Edges[v2];
			int v11 = Edges[v2 + 1];

			idx24(0, v00);
			idx24(1, v11);
			idx24(2, v4);

			idx24(3, v4);
			idx24(4, v11);
			idx24(5, v4 + 1);

			if (idx2)idx2 += 6;
			if (idx4)idx4 += 6;
			
			idx24(0, v4);
			idx24(1, v4 + 1);
			idx24(2, v4 + 2);

			idx24(3, v4 + 2);
			idx24(4, v4 + 1);
			idx24(5, v4 + 3);

			if (idx2)idx2 += 6;
			if (idx4)idx4 += 6;

			idx24(0, v4 + 2);
			idx24(1, v4 + 3);
			idx24(2, v00 + np);

			idx24(3, v00 + np);
			idx24(4, v4 + 3);
			idx24(5, v11 + np);

			if (idx2)idx2 += 6;
			if (idx4)idx4 += 6;
		}
		sm->setNInd(nt * 6 + nedges*18);
		sm->setNVert(np*2 + nedges*4);
		sm->unlock();
		return sm;
	}
	return NULL;
}

StaticMesh* cMeshContainer::CreateHardsurfaceStaticMesh(){
	VecArray& Pos = GetPositions();
	VecArray& Nrm = GetNormals();
	RawArray raw;
	Triangulate(raw);
	if (GetNormals().Count() == 0)CalcNormals();
	int nt = 0;
	int np = Pos.Count();
	int nr = raw.Count();
	int nvs = 0;
	int nf = 0;
	UnlimitedBitset b;
	cList<comms::cObject>& obs = GetObjects();
	for (int i = 0; i<obs.Count(); i++){
		if (obs[i].Name.Contains("_negative")){
			b.set(i, true);
		}
	}
	for (int i = 0; i<nr; i++){
		int n = raw[i][0];
		nf++;
	}
	int step = 1;//+nf/2800000;
	for (int i = 0; i<nr; i++){
		int n = raw[i][0];
		if ((i%step) == 0){
			if (n>2){
				nt += n - 2;
				nvs += n;
			}
		}
		i += n;
	}
	if (nt){
		StaticMesh* sm = new StaticMesh;
		sm->create(nvs, nt * 3, comms::cVertex::PositionNormalColored::FormatID);
		comms::cVertex::PositionNormalColored* vn = (comms::cVertex::PositionNormalColored*)sm->getVertexData();
		DWORD* idx4 = NULL;
		WORD* idx2 = NULL;
		if (nvs<60000){
			idx2 = sm->getIndices();
		}
		else{
			idx4 = sm->getIndices4();
		}
		int npc = 0;
		for (int i = 0; i<nr; i++){
			int n = raw[i][0];
			float neg = float(b.get(raw[i][2]));
			int p0 = npc;
			if ((i%step) == 0){
				if (n>2){
					Vector3D C(0, 0, 0);
					for (int j = 0; j<n; j++){
						int pc = raw[i + j + 1][0];
						C += Pos[pc];
					}
					C /= n;
					Vector3D N(0, 0, 0);
					for (int j = 0; j<n; j++){
						Vector3D pc = Pos[raw[i + j + 1][0]];
						Vector3D pn = Pos[raw[i + (j + 1) % n + 1][0]];
						pc -= C;
						pn -= C;
						N += Vector3D::Cross(pc, pn);
					}
					N.Normalize2();
					for (int j = 0; j<n; j++){
						int pc = raw[i + j + 1][0];
						int nc = raw[i + j + 1][2];
						vn->Pos = Pos[pc];
						vn->Normal = N;
//						Vector4D CL = nc != -1 ? Vector4D(Nrm[nc], neg) : Vector4D(0, 0, 0, neg);
						Vector4D CL = Vector4D(0, 0, 0, neg);
						if (nc != -1 && nc < Nrm.Count())
							CL = Vector4D(Nrm[nc], neg);
						CL.ToVec3() *= 127.0;
						CL.ToVec3() += Vector3D(128);
						vn->Color = V4D2DW(CL);
						vn++;
						if (j>1){
							if (idx2){
								idx2[0] = p0;
								idx2[1] = p0 + j - 1;
								idx2[2] = p0 + j;
								idx2 += 3;
							}
							if (idx4){
								idx4[0] = p0;
								idx4[1] = p0 + j - 1;
								idx4[2] = p0 + j;
								idx4 += 3;
							}
						}
					}
					npc += n;
				}
			}
			i += n;
		}
		sm->setNInd(nt * 3);
		sm->setNVert(nvs);
		sm->setNPri(nt);
		sm->unlock();
		static int sh = IRS->GetShaderID("mpreview1", comms::cVertex::PositionNormalColored::FormatID);
		sm->setShader(sh);
		return sm;
	}
	return NULL;
}
StaticMesh* cMeshContainer::CreateStaticMeshUV(){
	VecArray& Pos = GetPositions();
	auto& uv = GetTexCoords();
	CalcNormals();
	VecArray& N = GetNormals();
	auto& raw = GetRaw();
	int nt = 0;
	int np = uv.Count();
	//int nn=N.Count();
	int nr = raw.Count();
	for (int i = 0; i<nr; i++){
		int n = raw[i][0];
		if (n>2)nt += n - 2;
		i += n;
	}
	if (nt){
		StaticMesh* sm = new StaticMesh;
		sm->create(uv.Count(), nt * 3, vfVertexN);
		VertexN* vn = (VertexN*)sm->getVertexData();
		VertexN* vn0 = vn;
		for (int i = 0; i<np; i++){
			vn->Pos = Vector3D::Zero;
			vn->Normal = Vector3D::Zero;
			vn->TexCoord = uv[i];
			vn++;
		}
		DWORD* idx4 = NULL;
		WORD* idx2 = NULL;
		if (np<60000){
			idx2 = sm->getIndices();
		}
		else{
			idx4 = sm->getIndices4();
		}
		int nidx = 0;
		for (int i = 0; i<nr; i++){
			int p0 = 0;
			int pp = 0;
			int n = raw[i][0];
			for (int j = 0; j<n; j++){
				int vc = raw[i + j + 1][0];
				int pc = raw[i + j + 1][1];
				vn0[pc].Pos = Pos[vc];
				if (j == 0)p0 = pc;
				if (j>1){
					if (idx2){
						idx2[0] = p0;
						idx2[1] = pp;
						idx2[2] = pc;
						for (int k = 0; k<3; k++)assert(idx2[k]<np);
						idx2 += 3;
						nidx += 3;
					}
					if (idx4){
						idx4[0] = p0;
						idx4[1] = pp;
						idx4[2] = pc;
						idx4 += 3;
						nidx += 3;
					}
				}
				pp = pc;
			}
			i += n;
		}
		sm->setNInd(nt * 3);
		sm->setNVert(np);
		sm->setNPri(nt);
		sm->unlock();
		static int sh = IRS->GetShaderID("skypreview", vfVertexN);
		sm->setShader(sh);
		return sm;
	}
	return NULL;
}
void cMeshContainer::SeparateObject(int ObjectIndex, cMeshContainer* dest){
	if(dest)dest->Copy(*this);
	for (int i = 0; i < m_Raw.Count(); i++){
		int n = m_Raw[i][0];
		if (m_Raw[i][2] == ObjectIndex){
			m_Raw[i][2] = -1;
		}
		else{
			if(dest)dest->m_Raw[i][2] = -1;
		}
		i += n;
	}
	if(dest)dest->RemoveUnusedVerts();
	RemoveUnusedVerts();
	if(dest)dest->RemoveUnusedObjMtl();
	RemoveUnusedObjMtl();
}
void cMeshContainer::SetLoadMeshHook(MeshOpHook* h){
	ImportHook = h;
}
void cMeshContainer::SetSaveMeshHook(MeshOpHook* h){
	ExportHook = h;
}
DWORD cMeshContainer::CalcGUID() {
	DWORD H = 0;
	H += m_Positions.Count();
	H += m_Raw.Count();
	H += m_TexCoords.Count();
	H += m_Normals.Count();
	for (int i = 0; i < m_Positions.Count(); i++) {
		cVec3 p = m_Positions[i];
		H += p.x * 2048;
		H += p.y * 2048;
		H += p.y * 2048;
	}
	for (int i = 0; i < m_Normals.Count(); i++) {
		cVec3 p = m_Normals[i];
		H += p.x * 2048;
		H += p.y * 2048;
		H += p.y * 2048;
	}
	for (int i = 0; i < m_TexCoords.Count(); i++) {
		cVec2 p = m_TexCoords[i];
		H += p.x * 65535;
		H += p.y * 65535;
	}
	return H;
}
struct Meshef {
	cMeshContainer* mc;
	StaticMesh* sm;
	DWORD GUID;
};
cList<Meshef> MeshList;
void cMeshContainer::DbgDrawMeshWithUVGrid() {
	Meshef* ref = NULL;
	for (int i = 0; i < MeshList.Count(); i++) {
		if (MeshList[i].mc == this) {
			ref = &MeshList[i];
			break;
		}
	}
	if (!ref) {
		Meshef mr;
		MeshList.Add(mr);
		ref = &MeshList.GetLast();
		ref->mc = this;
		ref->sm = CreateStaticMeshUV();
		ref->GUID = CalcGUID();
	}
	if (ref) {
		 DWORD G = CalcGUID();
		 if (G != ref->GUID) {
			 ref->GUID = G;
			 delete(ref->sm);
			 ref->sm = CreateStaticMeshUV();
		 }
		 static int tex = IRS->GetTextureID("data/textures/grid64.png", false, true);
		 static int texn = IRS->GetTextureID("data/textures/nonorm.png", false, true);
		 static int texg = IRS->GetTextureID("data/textures/nogloss.png", false, true);
		 static int sh = IRS->GetShaderID("obj_sel", vfVertexN, "#define VIEW_COLOR\n#define VERT_NORMAL""obj_sel");
		 ref->sm->setTexture(tex, 0);
		 ref->sm->setTexture(texn, 1);
		 ref->sm->setTexture(texg, 3);
		 ref->sm->setTexture(AppOptions::EnvMap, 4);
		 ref->sm->setShader(sh);
		 ref->sm->render();
		 rsRestoreShader();
	}
}
void cMeshContainer::ImproveQuadsTopology() {
	uni_hash<int, int> vnv;
	bool ch = false;
	do {
		ch = false;
		for (int i = 0; i < m_Raw.Count(); i++) {
			int n = m_Raw[i][0];
			if (n == 4) {
				for (int j = 0; j < n; j++) {
					int v1 = m_Raw[i + j + 1][0];
					int v2 = m_Raw[i + (j + 1) % 4 + 1][0];
					int v3 = m_Raw[i + (j + 2) % 4 + 1][0];
					int v4 = m_Raw[i + (j + 3) % 4 + 1][0];
					if (vnv.size(v1) == 3 && vnv.size(v2) == 5 && vnv.size(v3) == 3 && vnv.size(v4) == 5) {
						ch = true;
						m_Raw.RemoveAt(i, 5);
						for (int p = 0; p < m_Raw.Count(); p++) {
							int nn = m_Raw[p][0];
							for (int k = 0; k < nn; k++) {
								if (m_Raw[k][0] == v1) {
									m_Raw[k][0] = v3;
								}
							}
							p += nn;
						}
						i = -1;
						n = 0;
						vnv.reset();
						CreateVnv(vnv);
						break;
					}
				}
			}
			i += n;
		}
	} while (ch);
}

//============================================
bool cMeshContainer::GetFaceVertex(int nf, cList<int>& Vertx)
{
	int nfm = GetPolyCount();
	if (nf<0 || nf>= nfm) {
		cLog::Warning("Bad index Face %d ", nf);
		return false;
	}

	int QurF = 0;
	for (int i = 0; i < m_Raw.Count(); i++) {
		int nv = m_Raw[i][0];

		if (QurF == nf) {
			for (int k = 0; k < nv; k++)
				Vertx.Add(m_Raw[i+k+1][0]);
			return true;
		}
		i += nv;
		QurF++;
	}

	return false;
}
bool cMeshContainer::GetNormalsOfFace(int nf, cList<int>& Norm)
{
	int nfm = GetPolyCount();
	if (nf < 0 || nf >= nfm) {
		cLog::Warning("Bad index Face %d ", nf);
		return false;
	}
	int QurF = 0;
	for (int i = 0; i < m_Raw.Count(); i++) {
		int nv = m_Raw[i][0];
		if (QurF == nf) {
			for (int k = 0; k < nv; k++)
				Norm.Add(m_Raw[i + k + 1][2]);
			return true;
		}
		i += nv;
		QurF++;
	}
	return false;
}

bool cMeshContainer::IntersectionTwoFace(int f1, cMeshContainer* m2, int f2, cVec3* p1, cVec3* p2)
{
	cList<cVec3> pol1;
	cList<cVec3> pol2;
	cList<int> Vertx1;
	if (!GetFaceVertex(f1, Vertx1))
		return false;
	for (int i = 0; i < Vertx1.Count(); i++)
		pol1.Add(m_Positions[Vertx1[i]]);

	cList<int> Vertx2;
	if (!m2->GetFaceVertex(f2, Vertx2))
		return false;
	for (int i = 0; i < Vertx2.Count(); i++)
		pol2.Add(m2->m_Positions[Vertx2[i]]);

	cList<cVec3> pts;
	IntersectionTwoPoligons(pol1, pol2, pts);
	if (pts.Count() < 2)
		return false;

	*p1 = pts[0];
	*p2 = pts[1];

	return true;
}

int cMeshContainer::AddNewVert(cVec3& pnt, double delta)
{
	for (int i = 0; i < m_Positions.Count(); i++) {
		double dist = pnt.Distance(m_Positions[i]);
		if (dist < delta)
			return i;
	}
	return m_Positions.Add(pnt);
}

int	cMeshContainer::InsertVertexToEdge(int nf, cVec3& p, double delta)
{
	int pos = GetFacePos(nf);
	if (pos == -1)
		return -1;
	cList<int> Verts;
	if (!GetFaceVertex(nf, Verts))
		return -1;
	for (int i = 0; i < Verts.Count(); i++)
	{
		double dist = p.Distance(m_Positions[Verts[i]]);
		if (dist < delta)
			return Verts[i];
	}

//	char buf[120];
//	sprintf(buf, "InsertVertexToEdge  f=%d p %6.1f %6.1f %6.1f ", nf, p.x, p.y, p.z);
//	Step2(buf);

	CVertex3d p3d(p);
	for (int i = 0; i < Verts.Count()-1; i++)
	{
		double dist = p3d.GetDistLineSegment(m_Positions[Verts[i]], m_Positions[Verts[i + 1]]);
		if (dist < delta)
		{
			int vn = AddNewVert(p, delta);

//			char buf[120];
//			sprintf(buf, "i=%d vn=%d ", i, vn);
//			Step2(buf);

			DWORDS2 ed(Verts[i], Verts[i + 1]);
			int f2 = FindSecondFace(nf, ed);
			cVec3i vi(vn, -1, -1);
			int np = m_Raw[pos][0];
			m_Raw[pos][0] = np + 1;
			m_Raw.Insert(pos+i+2, vi);
			if (f2 != -1)
				InsertVertex(f2, vn, ed);
			return vn;
		}
	}

	int nfm = GetPolyCount();

	double dist = p3d.GetDistLineSegment(m_Positions[Verts[0]], m_Positions[Verts[Verts.Count() - 1]]);
	if (dist < delta)
	{
		int vn = AddNewVert(p, delta);

	//	char buf[120];
	//	sprintf(buf, "Last Edge vn=%d ", vn);
	//	Step2(buf);

		DWORDS2 ed(Verts[0], Verts[Verts.Count() - 1]);
		int f2 = FindSecondFace(nf, ed);
		cVec3i vi(vn, -1, -1);
		int np = m_Raw[pos][0];
		m_Raw[pos][0] = np + 1;
		if (nf < nfm-1)
			m_Raw.Insert(pos + Verts.Count() + 1, vi);
		else
			m_Raw.Add( vi);

		if (f2 != -1)
			InsertVertex(f2, vn, ed);
		return vn;
	}

	return -1;
}

void cMeshContainer::InsertVertex(int f, int v, DWORDS2 ed)
{
	int pos = GetFacePos(f);
	if (pos == -1)
		return ;

	cList<int> Verts;
	if (!GetFaceVertex(f, Verts))
		return ;
	for (int i = 0; i < Verts.Count() - 1; i++)
	{
		DWORDS2 edi(Verts[i], Verts[i + 1]);
		if (ed == edi) {
			cVec3i vi(v, -1, -1);
			int np = m_Raw[pos][0];
			m_Raw[pos][0] = np + 1;
			m_Raw.Insert(pos + i+2, vi);
			return;
		}
	}
	DWORDS2 edi(Verts[0], Verts[Verts.Count() - 1]);
	int nfm = GetPolyCount();
	if (ed == edi)
	{
		cVec3i vi(v, -1, -1);
		int np = m_Raw[pos][0];
		m_Raw[pos][0] = np + 1;
		if (f < nfm - 1)
			m_Raw.Insert(pos + Verts.Count() + 1, vi);
		else
			m_Raw.Add(vi);
	}
}

cVec3 cMeshContainer::GetFaceNormalByNubmer(int indx)
{
	int pos = GetFacePos(indx);
	cVec3 norm = GetFaceNormal(pos);
	return norm;
}

cVec3 cMeshContainer::GetFaceCenterByNubmer(int indx)
{
	cVec3 vc(0,0,0);
	cList<int> Verts;
	if (!GetFaceVertex(indx, Verts))
		return vc;

	for (int i = 0; i < Verts.Count(); i++)
	{
		vc += m_Positions[Verts[i]];
	}
	vc /= (float)Verts.Count();
	return vc;
}

int cMeshContainer::GetFacePos(int indx)
{

	int nfm = GetPolyCount();
	if (indx < 0 || indx >= nfm) {
		cLog::Warning("Bad index Face %d ", indx);
		return -1;
	}

	int QurF = 0;
	for (int i = 0; i < m_Raw.Count(); i++) {
		int nv = m_Raw[i][0];
		if (QurF == indx) {
			return i;
		}
		i += nv;
		QurF++;
	}

	return -1;
}

int cMeshContainer::FindSecondFace(DWORD nF, DWORDS2& edge)
{
	int nfm = GetPolyCount();
	for (int j = 0; j < nfm; j++) {
		if (nF == j)
			continue;
		cList<int> Verts;
		if (!GetFaceVertex(j, Verts))
			return -1;
		for (int i = 0; i < Verts.Count() - 1; i++)
		{
			DWORDS2 edi(Verts[i], Verts[i + 1]);
			if (edge == edi)
				return j;
		}
		DWORDS2 edi(Verts[0], Verts[Verts.Count() - 1]);
		if (edge == edi)
			return j;
	}

	return -1;
}

void cMeshContainer::print()
{
	char buf[120];
	sprintf(buf, "----------  Qty Positions   = %d   ------", m_Positions.Count());
	Step(buf);

	int nfm = GetPolyCount();
	sprintf(buf, "----------  Qty  Polygons   = %d   ------", nfm);
	Step(buf);
	for (int j = 0; j < nfm; j++) {

		cList<int> Verts;
		if (!GetFaceVertex(j, Verts))
			return ;
		sprintf(buf, "------  Polygons %d   Qty  Vertex   = %d   ------", j, Verts.Count());
		Step(buf);
		for (int i = 0; i < Verts.Count() ; i++)
		{
			sprintf(buf, "----------  V%d  = %d   ------", i, Verts[i]);
			Step(buf);
		}

	}
	//m_Raw
}

bool cMeshContainer::EditFace(int f, cList<DWORD>& VertN)
{
	int pos = GetFacePos(f);
	if (pos == -1)
		return false;
	cList<int> Verts;
	if (!GetFaceVertex(f, Verts))
		return false;
	if (Verts.Count() == VertN.Count()-1)
	{
		for (int i = 0; i < VertN.Count()-1; i++)
			m_Raw[pos+i+1][0] = VertN[i];
		return true;
	}
	int nfm = GetPolyCount();
	m_Raw[pos][0] = VertN.Count()-1;
	if (Verts.Count() < VertN.Count()-1)
	{
		int dr = Verts.Count();
		int addrw = VertN.Count()-1 - Verts.Count();
		for (int i = 0; i < Verts.Count(); i++)
			m_Raw[pos + i + 1][0] = VertN[i];

		for (int k = 0; k < addrw; k++) {
			cVec3i vi(VertN[dr + k], 0, 0);
			if (f < nfm - 1)
				m_Raw.Insert(pos + dr + k + 1, vi);
			else
				m_Raw.Add(vi);
		}
		return true;
	}
	for (int i = 0; i < VertN.Count()-1; i++)
		m_Raw[pos + i + 1][0] = VertN[i];

	int addrw = Verts.Count() - (VertN.Count()-1);
	int dr = VertN.Count() - 1;
	for (int k = 0; k < addrw; k++)
		m_Raw.RemoveAt(pos + dr + 1);

	return true;
}

int cMeshContainer::AddFace(cList<DWORD>& VertN)
{
	int nf = -1;
	cVec3i vn(VertN.Count()-1, 0, 0);
	m_Raw.Add(vn);
	int nfm = GetPolyCount();

	for (int i = 0; i < VertN.Count()-1; i++) {
		cVec3i vni(VertN[i], -1, -1);
		m_Raw.Add(vni);
	}
	return  nfm+1;
}

int cMeshContainer::GetNearVertex(cVec3& pm)
{
	int i_min = -1;
	double min_dist = 1e15;
	for (int j = 0; j < m_Positions.Count(); j++)
	{
		double dist = m_Positions[j].Distance(pm);
		if (min_dist > dist) {
			min_dist = dist;
			i_min = j;
		}
	}
	return i_min;
}

void cMeshContainer::SnapingToSculpt()
{
	bool mDrawOnPlane = uvtool().mDrawOnPlane;
	uvtool().mDrawOnPlane = false;
	bool AutoSnap = UV_map_Tool::AutoSnap;
	UV_map_Tool::AutoSnap = true;
	ClusteredMesh* mesh = &clumesh();
	CalcNormals();
	for (int j = 0; j < m_Positions.Count(); j++)
	{
		Vector3D Pos = m_Positions[j];
		Vector3D Nrm = GetNormal(j);
		DWORD F;
		Vector3D Pn = mesh->Snap(Pos, Nrm, F);
		m_Positions[j] = Pn;
	}
	uvtool().mDrawOnPlane = mDrawOnPlane;
	UV_map_Tool::AutoSnap = AutoSnap;
}

int cMeshContainer::FindFirstFace3d(DWORDS2& ed)
{
	int nfm = GetPolyCount();
	for (int j = 0; j < nfm; j++) {
		int pos = GetFacePos(j);
		if (pos == -1)
			return -1;
		cList<int> Verts;
		if (!GetFaceVertex(j, Verts))
			return -1;
		for (int i = 0; i < Verts.Count() - 1; i++)
		{
			DWORDS2 edi(Verts[i], Verts[i + 1]);
			if (ed == edi)
				return j;
		}
		DWORDS2 edi(Verts[0], Verts[Verts.Count() - 1]);
		if (ed == edi)
			return j;
	}
	return -1;
}
int cMeshContainer::FindSecondCFace3d(int nf, DWORDS2& ed)
{
	int nfm = GetPolyCount();
	for (int j = 0; j < nfm; j++) {
		if (nf == j)
			continue;
		cList<int> Verts;
		if (!GetFaceVertex(j, Verts))
			return false;
		for (int i = 0; i < Verts.Count() - 1; i++)
		{
			DWORDS2 edi(Verts[i], Verts[i + 1]);
			if (ed == edi)
				return j;
		}
		DWORDS2 edi(Verts[0], Verts[Verts.Count() - 1]);
		if (ed == edi)
			return j;

	}
	// ed is boundary of Mesh and single
	return -1;
}
bool cMeshContainer::RemoveFace(int indx)
{
	int pos = GetFacePos(indx);
	if (pos == -1)
		return false;
	int QtyR = m_Raw[pos][0]+1;
	for (int i = 0; i < QtyR; i++)
		m_Raw.RemoveAt(pos);
	return true;
}

bool cMeshContainer::IntersectionFacebyPlane(int nf, cPlane& pl, cList<cVec3>& pcr)
{
	double delta = 0.001;

/*	CSizeBlock bound;
	for (int i = 0; i < m_Vert.GetCount(); i++) {
		CPoint3d p(pMesh->P(m_Vert[i].p).x, pMesh->P(m_Vert[i].p).y, pMesh->P(m_Vert[i].p).z);
		bound.AddPoint(&p);
	}
	bound.Update();
	if ((fabs(bound.m_pc.x * pl->a + bound.m_pc.y * pl->b + bound.m_pc.z * pl->c + pl->d) + delta) > bound.m_Rad)
		return; // ��������� ����� ������
*/
	cList<int> Verts;
	if (!GetFaceVertex(nf, Verts))
		return false;

	cList<double> KnotDelta;

	for (int i = 0; i < Verts.Count(); i++) {
		double de = m_Positions[Verts[i]].x * pl.a + m_Positions[Verts[i]].y * pl.b + m_Positions[Verts[i]].z * pl.c + pl.d;
		KnotDelta.Add(de);
	}
	bool IsParal = true;
	for (int i = 0; i < KnotDelta.Count(); i++)
	{
		if (fabs(KnotDelta[i]) > delta)
			IsParal = false;
	}
	if (IsParal)
		return false; // Plane para;e; Face

	cList<Vector3D> pnts;
	for (int i = 0; i < Verts.Count(); i++) {
		pnts.Add(m_Positions[Verts[i]]);
	}
	CPolyline line(pnts);
	line.AddPoint(line.P(0));

	int nc = line.IntersectionPlane(pl, pcr);
/*	if (pcr.Count()) {
		Step2("IntersectionPlane");
		char buf[120];
		sprintf(buf, "pl= %6.5f %6.5f %6.5f  %6.3f ", pl.a, pl.b, pl.c, pl.d);
		Step2(buf);
		line.print();
		for (int i = 0; i < pcr.Count(); i++) {
			sprintf(buf, "pcr= %6.2f %6.2f %6.2f ", pcr[i].x, pcr[i].y, pcr[i].z);
			Step2(buf);
		}
	}
*/

	return true;
}

void cMeshContainer::SelectConnectedFaces(int nf, cList<int>& facesFrom, cList<int>& facesTo)
{
	cList<int> Verts;
	if (!GetFaceVertex(nf, Verts))
		return;

	for (int i = 0; i < Verts.Count() - 1; i++)
	{
		DWORDS2 edi(Verts[i], Verts[i + 1]);
		int f2 = FindSecondCFace3d(nf, edi);
		if (f2 != -1) {
			for (int k = 0; k < facesFrom.Count(); k++)
			{
				if (f2 == facesFrom[k]) {
					facesFrom.RemoveAt(k);
					facesTo.Add(f2);
				}
			}
		}
	}
	DWORDS2 edi(Verts[0], Verts[Verts.Count() - 1]);
	int f2 = FindSecondCFace3d(nf, edi);
	if (f2 != -1) {
		for (int k = 0; k < facesFrom.Count(); k++)
		{
			if (f2 == facesFrom[k]) {
				facesFrom.RemoveAt(k);
				facesTo.Add(f2);
			}
		}
	}
}

void cMeshContainer::DrawWire( DWORD Color,float Thickness, bool DrawNormal)
{
	rsEnableZ(true);
	rsRestoreShader();
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		for (int j = 0; j < n; j++) {
			int v1 = m_Raw[i + j + 1][0];
			int v2 = m_Raw[i + ((j + 1) % n) + 1][0];
			Vector3D pp1 = m_Positions[v1];
			Vector3D pp2 = m_Positions[v2];

			DrawAALine(pp1, pp2, Color, Color, Thickness, Thickness, false);
		}
		i += n;
	}

	if (DrawNormal) {
		int nfm = GetPolyCount();
		for (int j = 0; j < nfm; j++) {
			cList<int> Verts;
			if (!GetFaceVertex(j, Verts))
				return;
			cList<int> norms;
			if(!GetNormalsOfFace(j, norms))
				return;
			if (Verts.Count() != norms.Count())
				return;
			for (int i = 0; i < Verts.Count(); i++) {
				Vector3D p1 = m_Positions[Verts[i]];
				Vector3D p2 = m_Positions[Verts[i]];
				const Vector3D norml = GetNormal(norms[i]);
				float Len = GetAbsoluteFromVisual(p1, 70);
				p2 += norml * Len;
				DrawAALine(p1, p2, Color, Color, Thickness, Thickness, false);
			}

		}
	}

	FlushAALines();
	rsFlush();
}

float cMeshContainer::GetLengthEdgeMin()
{
	float distMin = 1e15;
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		for (int j = 0; j < n; j++) {
			int v1 = m_Raw[i + j + 1][0];
			int v2 = m_Raw[i + ((j + 1) % n) + 1][0];
			Vector3D pp1 = m_Positions[v1];
			Vector3D pp2 = m_Positions[v2];
			float dist = pp1.Distance(pp2);
			if (distMin > dist)
				distMin = dist;
		}
		i += n;
	}
	return distMin;
}

float cMeshContainer::GetLengthEdgeMidle()
{
	float distSumSq = 0;
	int nd = 0;
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		for (int j = 0; j < n; j++) {
			int v1 = m_Raw[i + j + 1][0];
			int v2 = m_Raw[i + ((j + 1) % n) + 1][0];
			Vector3D pp1 = m_Positions[v1];
			Vector3D pp2 = m_Positions[v2];
			float dist = pp1.Distance(pp2);
			if (dist > 0.0000001) {
				distSumSq += sqrt(dist);
				nd++;
			}
		}
		i += n;
	}
	float ds = distSumSq / float(nd);
	return pow(ds, 2);
}

void cMeshContainer::IntersectionRay(cVec3& p, cVec3& dir, cList<cVec3>& rez)
{
	for (int i = 0; i < m_Raw.Count(); i++) {
		cVec3 pc;
		if (IntersectionFaceRay(i, p, dir, pc))
			rez.Add(pc);
	}
}

bool cMeshContainer::IntersectionFaceRay(int f, cVec3& p, cVec3& dir, cVec3& pc)
{
	cList<int> Verts;
	if (!GetFaceVertex(f, Verts))
		return false;
	if (Verts.Count() < 3)
		return false;
	comms::cBounds bound;
	for (int i = 0; i < Verts.Count(); i++)
		bound.AddPoint(m_Positions[Verts[i]]);
	const comms::cSphere ball = bound.ToSphere();

	CVertex3d pB(ball.GetCenter());
	CVertex3d pl1(p);
	CVertex3d pl2=pl1;

	CVertex3d p0d(p);
	CVector3d vd(dir.x, dir.y, dir.z);
	pl2.Move(&vd, 100);

	double ditpb = pB.GetDistLine(&pl1, &pl2);
	double delta = 0.001;
	if((ditpb+ delta) > ball.GetRadius())
		return false; // Line very far

	float u;
	float v;
	float Dist = FLT_MAX;
	float d = RayTri(p, dir, m_Positions[Verts[0]], m_Positions[Verts[1]], m_Positions[Verts[2]], u, v);
	if (d > -0.000001 && d < Dist) {
		pc = m_Positions[Verts[0]] + (m_Positions[Verts[1]] - m_Positions[Verts[0]]) * u + (m_Positions[Verts[2]] - m_Positions[Verts[0]]) * v;
		return true;
	}

	if (Verts.Count() < 4)
		return false;

	d = RayTri(p, dir, m_Positions[Verts[0]], m_Positions[Verts[2]], m_Positions[Verts[3]], u, v);
	if (d > -0.000001 && d < Dist) {
		pc = m_Positions[Verts[0]] + (m_Positions[Verts[2]] - m_Positions[Verts[0]]) * u + (m_Positions[Verts[3]] - m_Positions[Verts[0]]) * v;
		return true;
	}
	return false;

	/*
	CVertex3d p0(m_Positions[Verts[0]]);
	CVertex3d p1(m_Positions[Verts[1]]);
	CVertex3d p2(m_Positions[Verts[2]]);
	CVertex3d pcd;
	bool pr = crossing_line_triangle(&p0d, &vd, &p0, &p1, &p2, &pcd);
	if (pr == true) {
		pc.x = pcd.x;
		pc.y = pcd.y;
		pc.z = pcd.z;
		return pr;
	}
	if (Verts.Count() < 4)
		return pr;
	CVertex3d p3(m_Positions[Verts[3]]);
	pr = crossing_line_triangle(&p0d, &vd, &p2, &p3, &p0, &pcd);
	if (pr == true) {
		pc.x = pcd.x;
		pc.y = pcd.y;
		pc.z = pcd.z;
	}
	return pr;
*/	

}

void cMeshContainer::OptimizeVoxelMesh(BaseClass* _vo) {
	VolumeObject* vo = _vo ? dynamic_cast<VolumeObject*>(_vo) : nullptr;
	if (vo) {
		m_Normals.FastClear();
		m_Normals.Add(Vector3D::Zero, m_Positions.Count());
		std_for(i, m_Positions.Count()) {
			m_Normals[i] = vo->GetInterpNormal(m_Positions[i]);
		}std_for_end;
	}
	else {
		CalcNormals();
	}
	AllowDebug(true);
	
	UnlimitedBytes cluster;
	cluster.set(m_Normals.Count(), 0);
	std_for(i, m_Normals.Count()) {
		auto& n = m_Normals[i];
		float nx = __abs(n.x);
		float ny = __abs(n.y);
		float nz = __abs(n.z);
		BYTE b = 0;
		if (ny > nx && ny > nz) {
			b = 1;
		}
		else if (nz > nx && nz > ny) {
			b = 2;
		}
		if (ny < nx && ny < nx) {
			b |= 1 << 2;
		}
		else if (nz < nx && nz < ny) {
			b |= 2 << 2;
		}
		cluster.set(i, b);
	}std_for_end;
	/*
	for (int i = 0; i < 8; i++) {
		for (int p = 0; p < 3; p++) {
			for (int j = 0; j < m_Raw.Count(); j++) {
				int n = m_Raw[j][0];
				for (int k = 0; k < n; k++) {
					int q = (k + 1) % n;
					int v1 = m_Raw[j + k + 1][0];
					int v2 = m_Raw[j + q + 1][0];
					auto& c1 = cluster.ref(v1);
					auto& c2 = cluster.ref(v2);
					if ((c1 & 3) == p || (c2 & 3) == p) {
						if ((c1 >> 2) != p)c1 = (c1 & 12) + p;
						if ((c2 >> 2) != p)c2 = (c2 & 12) + p;
					}
				}
				j += n;
			}
		}
	}
	*/
	UnlimitedBytes fcluster;
	for (int j = 0; j < m_Raw.Count(); j++) {
		int n = m_Raw[j][0];
		Vector3D ns(0);
		for (int k = 0; k < n; k++) {
			ns += m_Normals[m_Raw[j + k + 1][0]];
		}
		ns.Normalize();
		int bestc = 0;
		float v = -1;
		for (int k = 0; k < 3; k++) {
			float val = __abs(ns[k]);
			if(val>=v) {
				bestc = k;
				v = val;
			}
		}
		fcluster.set(j, bestc);
		j += n;
	}
	for(int i=0;i<m_Positions.Count();i++) {
		cluster.set(i, 0);
	}
	for (int j = 0; j < m_Raw.Count(); j++) {
		int n = m_Raw[j][0];
		int cuc = fcluster.get(j);
		for (int k = 0; k < n; k++) {
			int v1 = m_Raw[j + k + 1][0];
			auto& c1 = cluster.ref(v1);
			if (c1 && c1 != cuc + 1) {
				c1 = 254;
			}
			else c1 = cuc + 1;
		}
		j += n;
	}
	for (int i = 0; i < m_Positions.Count(); i++) {
		auto& c = cluster.ref(i);
		if (c < 254)c--;
	}
	std_for(i, m_Positions.Count()) {
		auto& r = cluster.ref(i);
		if (r < 3) {
			r &= 3;
			cVec2 g(0);
			auto p = m_Positions[i];
			if (r == 0) {
				g = cVec2(p.y, p.z);
			}
			else
				if (r == 1) {
					g = cVec2(p.z, p.x);
				}
				else {
					g = cVec2(p.x, p.y);
				}
			cVec2 ig(floorf(g.x + 0.5), floorf(g.y + 0.5));
			ig -= g;
			if (ig.LengthSq() > 0.001)r = 255;
		}
	}std_for_end;

	DbgLayer("points");
	for (int i = 0; i < m_Positions.Count(); i++) {
		auto r = cluster.get(i);
		DWORD CL = (r < 3 ? 0 : (0xFF << r * 8)) | 0xFF000000;
		if (r == 254)CL = 0xFF00FF00;
		AddDbgPoint(m_Positions[i], CL);
	}
	DbgLayer("fclusters");
	for (int j = 0; j < m_Raw.Count(); j++) {
		int n = m_Raw[j][0];
		int cuc = fcluster.get(j);
		if(n==3){
			cVec3 p[3];
			for (int k = 0; k < n; k++) {
				int v1 = m_Raw[j + k + 1][0];
				p[k] = m_Positions[v1];
			}
			DWORD cl = (0xFF << (cuc * 8)) | 0xFF000000;
			AddDbgTri(p[0], p[1], p[2], cl);
		}
		j += n;
	}

	DbgLayer("faces");

	uni_hash<int, bi_int> raws_on_del;
	uni_hash<int, int> vnv;
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		for (int j = 0; j < n; j++) {
			int q = (j + 1) % n;
			int v1 = m_Raw[i + j + 1][0];
			int v2 = m_Raw[i + q + 1][0];
			auto c1 = cluster[v1];
			auto c2 = cluster[v2];
			auto d = m_Positions[v1] - m_Positions[v2];
			d.Normalize();
			bool good = false;
			if(c1<254) {
				bool zx = __abs(d[(c1 + 1) % 3]) < 0.001;
				bool zy = __abs(d[(c1 + 2) % 3]) < 0.001;
				good = zx ^ zy;
			}
			if (c2 < 254) {
				bool zx = __abs(d[(c2 + 1) % 3]) < 0.001;
				bool zy = __abs(d[(c2 + 2) % 3]) < 0.001;
				good |= zx ^ zy;
			}
			if ((c1 == 254 && c2 < 254) || (c2 == 254 && c1 < 254))good = true;
			if (!good) {
				raws_on_del.add_uniq(bi_int(v1, v2), i);
				AddDbgLine(m_Positions[v1], m_Positions[v2], 0xFF0000FF, 0xFF0000FF);
			} else AddDbgLine(m_Positions[v1], m_Positions[v2], 0xFFFF0000, 0xFFFF0000);
		}
		i += n;
	}
	DbgLayer("res");
	RawArray res;
	auto join = [&](StackArray<int, 64>& result, int v1, int v2, int raw2) -> bool {
		int n1 = result.Count();
		int n2 = m_Raw[raw2][0];
		for (int i = 0; i < n1; i++) {
			int q = (i + 1) % n1;
			int vi1 = result[i];
			int vi2 = result[q];
			if(vi1 == v1 && vi2 == v2) {
				for (int j = 0; j < n2; j++) {
					int t = (j + 1) % n2;
					int vj1 = m_Raw[raw2 + j + 1][0];
					int vj2 = m_Raw[raw2 + t + 1][0];
					if (vj1 == v2 && vj2 == v1) {
						if (i < n1 - 1) {
							result.reverse(0, i);
							result.reverse(i + 1, n1 - 1);
							result.reverse(0, n1 - 1);
						}
						for (int k = 2; k < n2; k++) {
							result.Add(m_Raw[raw2 + (j + k) % n2 + 1][0]);
						}
						for (int k = 0; k < n2; k++) {
							m_Raw[raw2 + k + 1] = cVec3i(0, 0, 0);
						}
						return true;
					}
				}
			}
		}
		return false;
	};
	auto to_array = [&](StackArray<int, 64>& result, int raw) {
		result.Clear();
		int n = m_Raw[raw][0];
		for (int i = 0; i < n; i++) {
			auto& r = m_Raw[i + raw + 1];
			result.Add(r[0]);
			r = cVec3i(0, 0, 0);
		}
		m_Raw[raw] = cVec3i(0, 0, 0);
	};
	StackArray<int, 64> temp;
	RawArray res_raw;
	for (int i = 0; i < m_Raw.Count(); i++) {
		int n = m_Raw[i][0];
		if(n) {
			auto r0 = m_Raw[i];
			to_array(temp, i);
			bool c = false;
			do {
				c = false;
				int nt = temp.Count();
				for (int j = 0; j < nt; j++) {
					int k = (j + 1) % nt;
					int v1 = temp[j];
					int v2 = temp[k];
					int* opp = raws_on_del.get(bi_int(v2, v1));
					if(opp) {
						if(join(temp, v1, v2, *opp)) {
							c = true;
							break;
						}
					}
				}
			} while (c);
			/*
			int j = 0;
			for(int k=0;k<temp.Count();k++) {
				if(cluster.get(temp[k])<255) {
					temp[j++] = temp[k];
				}
			}
			if(j<temp.Count()) {
				temp.RemoveAt(j, temp.Count() - j);
			}
			*/
			if (temp.Count() > 2) {
				r0[0] = temp.Count();
				res_raw.Add(r0);
				for (int k = 0; k < temp.Count(); k++) {
					res_raw.Add({ temp[k],-1,-1 });
				}


				//dbg
				Vector3D c(0);
				for(auto& e:temp) {
					c += m_Positions[e];
				}
				c /= temp.Count();
				for(int i=0;i<temp.Count();i++) {
					AddDbgLine(m_Positions[temp[i]], m_Positions[temp[(i + 1) % temp.Count()]], 0xFFFF0000, 0xFF0000FF);
				}
				//
			}
		}
		i += n;
	}
	m_Raw.Clear();
	//m_Raw = res_raw;
	//RemoveUnusedVerts();
}

void cMeshContainer::FindTopologyCorrespondence(cMeshContainer& other, cList<int>& encoding) {
	//CreateVnv();
	//other.CreateVnv();

}

struct HyperEdge {
	cList<int> verts;
	float length;
};
	struct ContourPoint {
		ContourPoint() {
			vertex = -1;
			state = 0;
			next = prev = next_cage = prev_cage = nullptr;
		}
		int vertex;
		/// 0-usual vertex, 1-part of quad mesh, 2-corner
		char state;
		/// linking
		ContourPoint* next;
		ContourPoint* prev;
		ContourPoint* next_cage;
		ContourPoint* prev_cage;
};
struct HyperFace {
	cList<int> innerFaces;
	BigDynArray<ContourPoint> points;
	cList<ContourPoint*> links_start;
};

#define qseams_debug

cMeshContainer* cMeshContainer::QuadrangulateByEdges(cList<DWORDS2>& edges, float ApproxEdgeLength) {
	cMeshContainer* summ = new cMeshContainer;
	if (edges.Count()) {
#ifdef qseams_debug
		AllowDebug(true);
		DbgLayer("sharp");
		for (auto& e : edges) {
			AddDbgLine(m_Positions[e.V1], m_Positions[e.V2], 0xFFFFFFFF, 0xFFFFFFFF);
		}
#endif
		Triangulate();
		auto& raw=m_Raw;
		void_hash<DWORDS2> splits;
		/// connectivity info for the vertices over initial edges
		uni_hash<int, int> vnv_on_edges;
		/// the list of seams between islands
		void_hash<DWORDS2> used;
		splits.set_table_size(edges.Count() + 1);
		used.set_table_size(edges.Count() + 1);
		for (auto& e : edges) {
			splits.add_quick(e);
			vnv_on_edges.add_uniq(e.V1, e.V2);
			vnv_on_edges.add_uniq(e.V2, e.V1);
		}
		float mindot = 0.66;
		/// list of relatively straight chunks between islands of faces
		cList<HyperEdge*> HyperEdges;
		/// islands of faces 
		cList<HyperFace*> hyperFaces;
		/// the vertices that are the part of the future quad mesh
		UnlimitedBitset HyperVertexLocked;
		/// temporary array used for expansion
		cList<int> order;
		cList<int> verts_start;
		UnlimitedBitset corners;
		UnlimitedBitset quad_points;
		CreateFone();
		/// find all corners over the initial edeges list
		for (auto& e : edges) {
			for (int p = 0; p < 2; p++) {
				int v = p ? e.V2 : e.V1;
				int sz = vnv_on_edges.size(v);
				if (!HyperVertexLocked.get(v)) {
					bool is_corner = false;
					if (sz != 2) {
						is_corner = true;
					}
					else {
						int v1 = *vnv_on_edges.get(v, 0);
						int v2 = *vnv_on_edges.get(v, 1);
						float dp = -(m_Positions[v1] - m_Positions[v]).ToNormal().dot((m_Positions[v2] - m_Positions[v]).ToNormal());
						if (dp < mindot) {
							is_corner = true;
						}
					}
					if (is_corner) {
						HyperVertexLocked.set(v, true);
						verts_start.Add(v);
						corners.set(v, true);
					}
				}
			}
		}
		if (!verts_start.Count()) {
			verts_start.Add(0);
			HyperVertexLocked.set(0, true);
		}
		/// create the list of hyper edges - edges with multiple inner verts that connect corner points
		for (int i = verts_start.Count() - 1; i >= 0; i--) {
			int v = verts_start[i];
			do {
				order.Clear();
				float L = 0;
				scan_key(vnv_on_edges, v, int* vn) {
					if (!used.get(DWORDS2(v, *vn))) {
						used.add(DWORDS2(v, *vn));
						order.Add(v);
						order.Add(*vn);
						L += m_Positions[v].distance(m_Positions[*vn]);
						break;
					}
				}scan_end;
				if (order.Count() == 0)break;
				do {
					int no = order.Count();
					int vc = order[no - 1];
					if (HyperVertexLocked.get(vc))break;
					scan_key(vnv_on_edges, vc, int* vn) {
						if (*vn != order[no - 2]) {
							used.add_quick(DWORDS2(vc, *vn));
							order.Add(*vn);
							L += m_Positions[vc].distance(m_Positions[*vn]);
						}
					}scan_end;
					if (no == order.Count())break;
				} while (true);
				auto* he = new HyperEdge;
				he->verts = order;
				he->length = L;
				HyperEdges.Add(he);
			} while (true);
		}
		/// divide each hyper edge onto even count of chunks of similarly equal length
		for(int i=0;i<HyperEdges.Count();i++) {
			auto* he = HyperEdges[i];
			int nd = ffloorf(he->length / ApproxEdgeLength / 2) * 2;
			if (nd < 2)nd = 2;
			float dL = he->length/nd;
			float Lc = 0;
			float eps = dL / 8.0f;
			for (int p = 1; p < nd; p++) {
				float Ld = dL * p;
				Lc = 0;
				for (int j = 1; j < he->verts.Count(); j++) {
					int v0 = he->verts[j - 1];
					int v1 = he->verts[j];
					const auto& p0 = m_Positions[v0];
					const auto& p1 = m_Positions[v1];
					float L1 = m_Positions[v0].distance(m_Positions[v1]);
					if (Lc + L1 > Ld) {
						float L2 = Ld - Lc;
						float L3 = L1 - L2;
						float t = L2 / L1;
						if (L2 < eps) {
							/// new vertex is clodse to beginning of the line, we don't add new vertex 
							if (!HyperVertexLocked.get(v0)) {
								HyperVertexLocked.set(v0, true);
								verts_start.Add(v0);
								quad_points.set(v0, true);
							}
						}
						else if (Lc + L1 - Ld < eps) {
							/// new vertex is close to end, we don't add the vertex
							if (!HyperVertexLocked.get(v1)) {
								HyperVertexLocked.set(v1, true);
								verts_start.Add(v1);
								quad_points.set(v1, true);
							}
						}
						else {
							/// split the line and insert point, add triangles, update fone, used
							auto pc = p0 + (p1 - p0) * t;
							int v = m_Positions.Add(pc);
							HyperVertexLocked.set(v, true);
							quad_points.set(v, true);

							used.del(DWORDS2(v0, v1));
							used.add_quick(DWORDS2(v0, v));
							used.add_quick(DWORDS2(v1, v));
							
							he->verts.Insert(j, v);
							verts_start.Add(v);
							StackArray<int> faces;
							StackArray<int> opp;
							StackArray<int> add;
							scan_key(fone, DWORDS2(v0, v1), auto * pf) {
								int vop = -1;
								int f1 = *pf;
								faces.Add(f1);
								add.Add(f1);
								int n = raw[f1][0];
								for (int j = 0; j < n; j++) {
									int vo = raw[f1 + 1 + j][0];
									if (vo != v0 && vo != v1) {
										vop = vo;
										break;
									}
								}
								opp.Add(vop);
							}scan_end;
							for (auto& rm : faces) {
								int n = raw[rm][0];
								for (int k = 0; k < n; k++) {
									int vv0 = raw[rm + 1 + k][0];
									int vv1 = raw[rm + 1 + (k + 1) % n][0];
									fone.del_elm(DWORDS2(vv0, vv1), rm);
								}
								int f2 = raw.Add(raw[rm]);
								for (int k = 0; k < n; k++) {
									auto r = raw[rm + 1 + k];
									int& vv = raw[rm + 1 + k][0];
									if (vv == v1)vv = v;
									if (r[0] == v0)r[0] = v;
									raw.Add(r);
								}
								add.Add(f2);
							}
							for (auto& a : add) {
								int n = raw[a][0];
								for (int k = 0; k < n; k++) {
									int vv0 = raw[a + 1 + k][0];
									int vv1 = raw[a + 1 + (k + 1) % n][0];
									fone.add_uniq(DWORDS2(vv0, vv1), a);
								}
							}
						}
						break;
					}
					Lc += L1;
				}
			}
			for (int j = 0; j < he->verts.Count(); j++) {
				int v = he->verts[j];
				if (!HyperVertexLocked.get(v)) {
					HyperVertexLocked.set(v, true);
					verts_start.Add(v);
				}
			}
		}
		UnlimitedBitset usedFace;
		/// create hyperfaces as clusters separated by the hyper-edges
		for (int i = 0; i < raw.Count(); i++) {
			int n = raw[i][0];
			if (!usedFace.get(i)) {
				order.Clear();
				order.Add(i);
				usedFace.set(i, true);
				for (int k = 0; k < order.Count(); k++) {
					int f = order[k];
					int n = raw[f][0];
					for (int j = 0; j < n; j++) {
						int v1 = raw[f + j + 1][0];
						int v2 = raw[f + 1 + (j + 1) % n][0];
						DWORDS2 D(v1, v2);
						if (!used.get(D)) {
							scan_key(fone, D, int* pf) {
								if (*pf != f && !usedFace.get(*pf)) {
									usedFace.set(*pf, true);
									order.Add(*pf);
								}
							}scan_end;
						}
					}
				}
				HyperFace* hf = new HyperFace;
				hf->innerFaces = order;
				hyperFaces.Add(hf);
			}
			i += n;
		}
		/// update the connectivity on edges
		vnv_on_edges.reset();
		scan(used, auto* e, auto* k) {
			vnv_on_edges.add_quick(k->V1, k->V2);
			vnv_on_edges.add_quick(k->V2, k->V1);
		}scan_end;
		/// create list of edges over each hyper-face, it always starts from corner if there it is at least one
		UnlimitedBitset tempUsed;
		uni_hash<ContourPoint*, int> cplink;
		cplink.set_table_size(m_Positions.Count());
		ContourQuadrangulator coqu;
		coqu.CreateBgFromMesh(this);
		int p = 0;
		for(auto* hf:hyperFaces) {
			tempUsed.clear();			
			for (auto& f : hf->innerFaces) {
				int n = raw[f][0];
				for (int i = 0; i < n; i++) {
					int v = raw[f + 1 + i][0];
					int v1 = raw[f + 1 + (i + 1) % n][0];
					if (used.get(DWORDS2(v, v1))) {
						ContourPoint cp;
						hf->points.Add(cp);
						auto& c = hf->points.GetLast();
						c.vertex = v;
						cplink.add_quick(v, &c);
					}
				}
			}
			for (auto& f : hf->innerFaces) {
				int n = raw[f][0];
				for (int i = 0; i < n; i++) {
					int v = raw[f + 1 + i][0];
					int v1 = raw[f + 1 + (i + 1) % n][0];
					if (used.get(DWORDS2(v, v1))) {
						scan_key(cplink, v, auto** pe) {
							auto* e = *pe;
							if(!e->next) {
								scan_key(cplink, v1, auto * *pe1) {
									auto* e1 = *pe1;
									if(!e1->prev) {
										e->next = e1;
										e1->prev = e;
										break;
									}
								}scan_end;
								break;
							}
						}scan_end;
					}
				}
			}
			tempUsed.clear();
			for (int i = 0; i < hf->points.Count(); i++) {
				auto& pt = hf->points[i];
				assert(pt.next);
				assert(pt.prev);
				pt.state = corners.get(pt.vertex) ? 2 : (quad_points.get(pt.vertex) ? 1 : 0);
				if (pt.state == 2 && !tempUsed.get(pt.vertex)) {
					hf->links_start.Add(&pt);
					tempUsed.set(pt.vertex, true);
					auto* cc = &pt;
					while(cc) {
						cc = cc->next;
						if (cc) {
							if (tempUsed.get(cc->vertex))break;
							tempUsed.set(cc->vertex, true);
						}
					}
				}
			}
			for (int i = 0; i < hf->points.Count(); i++) {
				auto& pt = hf->points[i];
				if (pt.state == 1 && !tempUsed.get(pt.vertex)) {
					hf->links_start.Add(&pt);
					tempUsed.set(pt.vertex, true);
					auto* cc = &pt;
					while (cc) {
						cc = cc->next;
						if (cc) {
							if (tempUsed.get(cc->vertex))break;
							tempUsed.set(cc->vertex, true);
						}
					}
				}
			}
			coqu.Clear();
			cList<int> edges;
			for (auto& e:hf->points) {
				if(e.state) {
					auto* ed = &e;
					edges.Add(e.vertex);
					ed = ed->next;
					while (!ed->state)ed = ed->next;
					edges.Add(ed->vertex);
				}
			}
			coqu.CreateFromList(edges);
			auto* mm = new cMeshContainer;
			coqu.Quadrangulate(mm);
			summ->ConcateWith(mm);
#ifdef qseams_debug
			cStr nm = "quads" + cStr::ToString(p++);
			DbgLayer(nm);
			mm->DrawDbg(Matrix4D::Identity,GetRandomColor());
#endif //qseams_debug
			delete(mm);
		}
#ifdef qseams_debug
		/// debug drawings
		DbgLayer("corners");
		for(auto& a:verts_start) {
			AddDbgSphere(m_Positions[a], 1, 0xFFFF0000);
		}
		for (auto* a : HyperEdges) {
			cStr nm = "edge" + cStr::ToString(p++);
			DbgLayer(nm);
			DWORD CL = GetRandomColor();
			for (int i = 1; i < a->verts.Count(); i++) {
				AddDbgLine(m_Positions[a->verts[i - 1]], m_Positions[a->verts[i]], CL, CL);
			}
			for (int i = 0; i < a->verts.Count(); i++) {
				if (HyperVertexLocked.get(a->verts[i])) {
					AddDbgSphere(m_Positions[a->verts[i]], 0.66, 0xFF00FF00);
				}
			}
		}
		p = 0;
		for (auto* f : hyperFaces) {
			cStr nm = "faces" + cStr::ToString(p++);
			DbgLayer(nm);
			DWORD CL = GetRandomColor();
			for (auto& f : f->innerFaces) {
				AddDbgTriangle(m_Positions[raw[f+1][0]], m_Positions[raw[f + 2][0]], m_Positions[raw[f + 3][0]], CL, CL, CL);
			}
			nm = "face_edges" + cStr::ToString(p++);
			DbgLayer(nm);
			CL = GetRandomColor();
			for (auto& l : f->links_start) {
				AddDbgSphere(m_Positions[l->vertex], 1.5, CL);
				auto* st = l->next;
				while (st && st != l) {
					if (st->state == 2)AddDbgSphere(m_Positions[st->vertex], 1, CL);
					if (st->state == 1)AddDbgSphere(m_Positions[st->vertex], 0.5, CL);
					AddDbgLine(m_Positions[st->prev->vertex], m_Positions[st->vertex], 0xFFFFFFFF, 0xFF000000);
					st = st->next;
				}
			}
		}
#endif //qseams_debug
	}
	return summ;
}


static bool IntersectedRectByPolyline(cList<cVec2>& rect, cList<cVec2>& poly, cVec2& pc1, cVec2& pc2)
{

}

bool cMeshContainer::IntersectedFaceByPolyline(int f, cList<cVec3>& poly)
{
	cList<int> Verts;
	if (!GetFaceVertex(f, Verts))
		return false;
	if (Verts.Count() < 3)
		return false;


	return false;
}

bool cMeshContainer::TrimmingByPolyline(cList<cVec3>& polyline)
{
	// @todo implement
	// - find all faces that are intersected by the polyline
	for (int i = 0; i < m_Raw.Count(); i++) {
		bool rez= IntersectedFaceByPolyline(i, polyline);

		if(rez) {
	// find fertexs that need to move on the point polyline
			cList<int> Verts;
			if (!GetFaceVertex(i, Verts))
				return false;
			double min_dist = 1e10;

		}
	}



	//

	return false;
}


static cMeshContainer* m = nullptr;
__postrender(r) {
	return;
	if (!devmode())return;
	if (!m) {
		m = cMeshIO::LoadMesh("C:\\Users\\andre\\Downloads\\Wall and Profile\\Furniture Profile\\Hilda.obj");//C:/test/sphere_strange.obj");
		if (m) {
			cList<DWORDS2> list;
			void_hash<DWORDS2> used;
			void_hash<bi_DWORD> nn;
			uni_hash<Vector3D, bi_DWORD> nedge;
			auto& raw = m->GetRaw();
			for (int i = 0; i < raw.Count(); i++) {
				int n = raw[i][0];
				for (int j = 0; j < n; j++) {
					int n1 = raw[i + j + 1][2];
					int n2 = raw[i + 1 + ((j + 1) % n)][2];

					int v1 = raw[i + j + 1][0];
					int v2 = raw[i + 1 + ((j + 1) % n)][0];

					nedge.add_quick(bi_DWORD(v1, v2), m->GetNormal(n1) + m->GetNormal(n2));
				}
				i += n;
			}
			for (int i = 0; i < raw.Count(); i++) {
				int n = raw[i][0];
				for (int j = 0; j < n; j++) {
					int v1 = raw[i + j + 1][0];
					int v2 = raw[i + 1 + ((j + 1) % n)][0];
					Vector3D* pn1 = nedge.get(bi_DWORD(v1, v2));
					Vector3D* pn2 = nedge.get(bi_DWORD(v2, v1));
					if(pn1 && pn2 && pn1->distance(*pn2) < 0.00001f) {
						continue;
					}
					DWORDS2 D(v1, v2);
					if (!used.get(D)) {
						list.Add(D);
						used.add_quick(D);
					}
				}
				i += n;
			}
			auto bb = m->CalcBoundBox();
			auto* res = m->QuadrangulateByEdges(list, bb.GetDiagonal()/20);
			AllowDebug(true);
			DbgLayer("Mesh");
			res->DrawDbg(Matrix4D::Identity, 0xFFFF0000, 0xFF00FF00);
		}
	}
}

} // namespace comms



// @todo Move to cMeshContainerTest.
void testbool(){
	return;
	comms::cMeshContainer* m1=comms::cMeshIO::LoadMesh("c:/test210/Samples/cube.obj");
	comms::cMeshContainer* m2=comms::cMeshIO::LoadMesh("c:/test210/Samples/sphere.obj");
	if(m1 && m2){
		comms::cMeshContainer m3;
		m3.PerformBooleanOp(*m1,*m2,2);
		comms::cMeshIO::SaveMesh(m3,"c:/out/ressumm.obj");
	}
}
//__thumbnail(test1) {
//	static comms::cMeshContainer* m1 = comms::cMeshIO::LoadMesh("C:/temp/untitled.obj");
//	if(m1)m1->DbgDrawMeshWithUVGrid();
//}

