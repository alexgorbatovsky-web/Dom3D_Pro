#include "comms.h"

namespace comms {

// cRawMesh::Clear
void cRawMesh::Clear() {
	m_Positions.Clear();
	m_Indices.Clear();
	m_Weights.Clear();
	m_TexCoords.Clear();
	m_Normals.Clear();
	m_Tangents.Clear();
	m_Raw.Clear();
	m_Materials.Clear();
}

// cRawMesh::GetPolyCount
int cRawMesh::GetPolyCount() const {
	int Count = 0;
	int index = 0;
	while(index < m_Raw.Count()) {
		index += m_Raw[index][0] + 1;
		Count++;
	}
	return Count;
}

// cRawMesh::IsTriangulated
bool cRawMesh::IsTriangulated() const {
	int D, index = 0;
	while(index < m_Raw.Count()) {
		D = m_Raw[index][0];
		if(D != 3) {
			return false;
		}
		index += D + 1;
	}
	return true;
}

//-----------------------------------------------------------------------------
// cRawMesh::Triangulate : (cList<cVec3i> *)
//-----------------------------------------------------------------------------
void cRawMesh::Triangulate(cList<cVec3i> *TriRaw) const {
	TriRaw->Clear();
	int index = 0;
	while(index < m_Raw.Count()) {
		int nDeg = m_Raw[index][0];
		const int idMtl = m_Raw[index][1];
		if(nDeg < 3) {
			// Skip lines and points.
			index += nDeg + 1;
		} else {
			const cVec3i i0 = m_Raw[++index];
			nDeg -= 2;
			while(nDeg--) {
				index++;
				TriRaw->Add(cVec3i(3, idMtl, 0));
				TriRaw->Add(i0);
				TriRaw->Add(m_Raw[index]);
				TriRaw->Add(m_Raw[index + 1]);
			}
			index += 2;
		}
	}
} // cRawMesh::Triangulate : (cList<cVec3i> *)

//-----------------------------------------------------------------------------
// cRawMesh::Triangulate : (cList<int> *)
//-----------------------------------------------------------------------------
void cRawMesh::Triangulate(cList<int> *Indices) const {
	Indices->Clear();
	
	// Select source raw
	const cList<cVec3i> *Raw = &m_Raw;
	cList<cVec3i> TriRaw;
	if(!IsTriangulated()) {
		Triangulate(&TriRaw);
		Raw = &TriRaw;
	}
	int Index = 0, i0, i1, i2;
	
	while(Index < Raw->Count()) {
		i0 = Raw->GetAt(Index++)[0];
		i1 = Raw->GetAt(Index++)[0];
		i2 = Raw->GetAt(Index++)[0];

		Indices->Add(i0);
		Indices->Add(i1);
		Indices->Add(i2);
	}
} // cRawMesh::Triangulate : (cList<int> *)

// cRawMesh::Triangulate
void cRawMesh::Triangulate() {
	if(IsTriangulated()) {
		return;
	}
	cList<cVec3i> TriRaw;
	Triangulate(&TriRaw);
	m_Raw.Copy(TriRaw);
}

// cRawMesh::InvertRaw
void cRawMesh::InvertRaw() {
	int index = 0, Deg, i;
	while(index < m_Raw.Count()) {
		Deg = m_Raw[index][0];
		i = 0;
		while(i < Deg / 2) {
			cMath::Swap(m_Raw[index + 1 + i], m_Raw[index + Deg - i]);
			i++;
		}
		index += Deg + 1;
	}
}

// cRawMesh::ClearMaterials
void cRawMesh::ClearMaterials() {
	int index = 0;
	while(index < m_Raw.Count()) {
		m_Raw[index][1] = 0;
		index += m_Raw[index][0] + 1;
	}
	m_Materials.Clear();
}

//-----------------------------------------------------------------------------
// cRawMesh::CalcNormals
//-----------------------------------------------------------------------------
void cRawMesh::CalcNormals(const bool Flat) {
	m_Normals.Clear();

	const int Count = Flat ? GetPolyCount() : m_Positions.Count();
	if(Count < 1) {
		return;
	}
	
	m_Normals.SetCount(Count, cVec3::Zero);

	// Indexing raw to created normals:
	int Index = 0, Poly = 0;
	while(Index < m_Raw.Count()) {
		int Deg = m_Raw[Index][0];
		while(Deg--) {
			Index++;
			if(Flat) {
				m_Raw[Index][2] = Poly;
			} else { // Smooth
				m_Raw[Index][2] = m_Raw[Index][0];
			}
		}
		Index++;
		Poly++;
	}

	// Calcing normals:
	Index = 0;
	cVec3 Normal;
	int i, j, Deg;
	while(Index < m_Raw.Count()) {
		Deg = m_Raw[Index][0];
		if(Deg >= 3) { // Skip lines and points.
			const cVec3i &i0 = m_Raw[Index + 1];
			const cVec3i &i1 = m_Raw[Index + 2];
			const cVec3i &i2 = m_Raw[Index + 3];
			cVec3 Normal = cVec3::Cross(m_Positions[i1[0]] - m_Positions[i0[0]], m_Positions[i2[0]] - m_Positions[i0[0]]);
			if(Flat) {
				m_Normals[i0[2]] = Normal;
				cAssert(i0[2] == i1[2] && i1[2] == i2[2]);
			} else {
				Normal *= Normal.LengthSq(); // Reduces influence of small polygon to resulting smooth normal.
				
				for(j = 0; j < Deg; j++) {
					const cVec3i &r = m_Raw[Index + 1 + j];
					m_Normals[r[2]] += Normal;
				}
			}
		}
		Index += Deg + 1;
	}

	// Normalizing normals
	for(i = 0; i < m_Normals.Count(); i++) {
		m_Normals[i].NormalizeSafe();
	}
} // cRawMesh::CalcNormals

//-----------------------------------------------------------------------------
// cRawMesh::CalcTangents
//-----------------------------------------------------------------------------
void cRawMesh::CalcTangents() {
	m_Tangents.Clear();
	if(m_Normals.IsEmpty()) {
		return; // No normals -> no tangents
	}

	if(m_TexCoords.IsEmpty()) {
		return; // No texcoords -> no tangents
	}

	m_Tangents.SetCount(m_Normals.Count(), cVec<cVec3, 2>(cVec3::Zero, cVec3::Zero));

	// Calcing tanspace
	int Index = 0, Deg;
	cVec3 Normal, Tangent, BiTangent;
	int i, j;
	cMat3 T;
	while(Index < m_Raw.Count()) {
		Deg = m_Raw[Index][0];
		if(Deg >= 3) { // Skip lines and points
			const cVec3i &i0 = m_Raw[Index + 1];
			const cVec3i &i1 = m_Raw[Index + 2];
			const cVec3i &i2 = m_Raw[Index + 3];

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
			
			for(j = 0; j < Deg; j++) {
				const cVec3i &r = m_Raw[Index + 1 + j];
				m_Tangents[r[2]] += cVec<cVec3, 2>(Tangent, BiTangent);
			}
		}
		Index += Deg + 1;
	}

	// Normalizing tanspace
	for(i = 0; i < m_Tangents.Count(); i++) {
		m_Tangents[i][0].NormalizeSafe();
		m_Tangents[i][1].NormalizeSafe();
	}
} // cRawMesh::CalcTangents

// cRawMesh::FreeHashTables
void cRawMesh::FreeHashTables() const {
	int n, i;
	for(n = 0; n < m_HashTables.Count(); n++) {
		HashTable &H = m_HashTables[n];
		for(i = 0; i < H.Count(); i++) {
			HashEntry *pEntry = H[i];
			while(pEntry != nullptr) {
				HashEntry *pNext = pEntry->pNext;
				delete pEntry;
				pEntry = pNext;
			}
		}
		H.Free();
	}
	m_HashTables.Free();
}

} // comms
