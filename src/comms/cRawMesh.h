#pragma once

//-----------------------------------------------------------------------------
// cRawSplit
//-----------------------------------------------------------------------------
template<class VERTEX>
class cRawSplit {
public:
	const cStr & GetMaterial() const {
		return m_Material;
	}
	void SetMaterial(const char *Material) {
		m_Material = Material;
	}

	const cList<VERTEX> & GetVertices() const {
		return m_Vertices;
	}
	cList<VERTEX> & GetVertices() {
		return m_Vertices;
	}

	const cList<int> & GetIndices() const {
		return m_Indices;
	}
	cList<int> & GetIndices() {
		return m_Indices;
	}

	void Clear() {
		m_Material.Clear();
		m_Vertices.Clear();
		m_Indices.Clear();
	}
private:
	cStr m_Material;
	
	cList<VERTEX> m_Vertices;
	cList<int> m_Indices; // 3 references to vertices for each triangle
}; // cRawSplit

//*****************************************************************************
// cRawMesh
//*****************************************************************************
class cRawMesh {
public:
	void Clear();
	
	const cList<cVec3> & GetPositions() const {
		return m_Positions;
	}
	cList<cVec3> & GetPositions() {
		return m_Positions;
	}

	const cList<cVec4> & GetIndices() const {
		return m_Indices;
	}
	cList<cVec4> & GetIndices() {
		return m_Indices;
	}

	const cList<cVec4> & GetWeights() const {
		return m_Weights;
	}
	cList<cVec4> & GetWeights() {
		return m_Weights;
	}

	const cList<cVec2> & GetTexCoords() const {
		return m_TexCoords;
	}
	cList<cVec2> & GetTexCoords() {
		return m_TexCoords;
	}
	
	const cList<cVec3> & GetNormals() const {
		return m_Normals;
	}
	cList<cVec3> & GetNormals() {
		return m_Normals;
	}

	const cList<cVec<cVec3, 2> > & GetTangents() const {
		return m_Tangents;
	}
	cList<cVec<cVec3, 2> > & GetTangents() {
		return m_Tangents;
	}

	const cList<cVec3i> & GetRaw() const {
		return m_Raw;
	}
	cList<cVec3i> & GetRaw() {
		return m_Raw;
	}

	const cList<cStr> & GetMaterials() const {
		return m_Materials;
	}
	cList<cStr> & GetMaterials() {
		return m_Materials;
	}

	int GetPolyCount() const;

	bool IsTriangulated() const;
	void Triangulate(cList<cVec3i> *TriRaw) const;
	void Triangulate(cList<int> *Indices) const;
	void Triangulate();

	void CalcNormals(const bool Flat = false);
	void CalcTangents();

	void InvertRaw();
	void ClearMaterials(); // Set "idMtl" to 0 for all faces and clear materials list
	
	//-------------------------------------------------------------------------
	// Split
	//-------------------------------------------------------------------------
	template<class VERTEX>
	void Split(cList<cRawSplit<VERTEX> *> *Splits) const {
		cAssert(Splits != nullptr);
		
		Splits->Clear();
		
		// Select source raw
		const cList<cVec3i> *Raw = &m_Raw;
		cList<cVec3i> TriRaw;
		if(!IsTriangulated()) {
			Triangulate(&TriRaw);
			Raw = &TriRaw;
		}
		
		// Prepare to split by material (at least by one)
		const int NMtls = cMath::Max(1, m_Materials.Count());
		m_HashTables.SetCount(NMtls);
		int m;
		for(m = 0; m < NMtls; m++) {
			cRawSplit<VERTEX> *M = new cRawSplit<VERTEX>;
			if(m_Materials.IsEmpty()) {
				M->SetMaterial("default");
			} else {
				M->SetMaterial(m_Materials[m]);
			}
			Splits->Add(M);
		}
		
		VERTEX u;
		memset(&u, 0, sizeof(VERTEX)); // Clear "Custom" or simply unused vertex fields.

		// Vertex format:
		const cVertex::Format Format = VERTEX::GetFormat();
		int Offset, Dim;
		const bool HasPosition = Format.HasUsage(cVertexUsage::Position, &Offset, &Dim);
		cVec3 *Position = nullptr;
		cVec4 *PositionRhw = nullptr;
		if(HasPosition) {
			if(3 == Dim) {
				Position = (cVec3 *)((byte *)&u + Offset);
			} else if(4 == Dim) {
				PositionRhw = (cVec4 *)((byte *)&u + Offset);
			}
		}
		cVec3 *Normal = Format.HasUsage(cVertexUsage::Normal, &Offset) ? (cVec3 *)((byte *)&u + Offset) : nullptr;
		cVec2 *TexCoord = Format.HasUsage(cVertexUsage::TexCoord, &Offset) ? (cVec2 *)((byte *)&u + Offset) : nullptr;
		cVec3 *Tangent = Format.HasUsage(cVertexUsage::Tangent, &Offset) ? (cVec3 *)((byte *)&u + Offset) : nullptr;
		cVec3 *BiTangent = Format.HasUsage(cVertexUsage::BiTangent, &Offset) ? (cVec3 *)((byte *)&u + Offset) : nullptr;
		cVec4 *Indices = Format.HasUsage(cVertexUsage::Indices, &Offset) ? (cVec4 *)((byte *)&u + Offset) : nullptr;
		cVec4 *Weights = Format.HasUsage(cVertexUsage::Weights, &Offset) ? (cVec4 *)((byte *)&u + Offset) : nullptr;
		
		// Enuming tris and filling splits:
		int index = 0, k;
		
		while(index < Raw->Count()) {
			const cVec3i &Tri = Raw->GetAt(index++);
			const int idMtl = Tri[1];
			for(k = 0; k < 3; k++) {
				const cVec3i &i0 = Raw->GetAt(index++);
				const int iPosition = i0[0];
				const int iTexCoord = i0[1];
				const int iNormal = i0[2];
				
				// Position || PositionRhw
				if(Position != nullptr) {
					*Position = m_Positions[iPosition];
				} else if(PositionRhw != nullptr) {
					PositionRhw->Set(m_Positions[iPosition], 1.0f);
				}
				
				// Normal
				if(!m_Normals.IsEmpty() && Normal != nullptr) {
					if(iNormal >= 0 && iNormal < m_Normals.Count()) {
						*Normal = m_Normals[iNormal];
					}
				}
				
				// TexCoord
				if(!m_TexCoords.IsEmpty() && TexCoord != nullptr) {
					if(iTexCoord >= 0 && iTexCoord < m_TexCoords.Count()) {
						*TexCoord = m_TexCoords[iTexCoord];
					}
				}
				
				// Tangent
				if(!m_Tangents.IsEmpty() && Tangent != nullptr) {
					*Tangent = m_Tangents[iNormal][0];
				}

				// BiTangent
				if(!m_Tangents.IsEmpty() && BiTangent != nullptr) {
					*BiTangent = m_Tangents[iNormal][1];
				}

				// Indices
				if(!m_Indices.IsEmpty() && Indices != nullptr) {
					*Indices = m_Indices[iPosition];
				}

				// Weights
				if(!m_Weights.IsEmpty() && Weights != nullptr) {
					*Weights = m_Weights[iPosition];
				}
				
				const int i = AddVertexToHashTable(idMtl, Splits->GetAt(idMtl)->GetVertices(), &u, iPosition);
				Splits->GetAt(idMtl)->GetIndices().Add(i);
			}
		}

		FreeHashTables();
	} // Split

protected:
	cList<cVec3> m_Positions;
	cList<cVec2> m_TexCoords;
	cList<cVec3> m_Normals;
	cList<cVec<cVec3, 2> > m_Tangents; // Tangent and BiTangent.
	// Polygon info or 3 references to the arrays of positions, texture coordinates,
	// and normals with tangents (optional) respectively.
	cList<cVec3i> m_Raw;	// Info0 = { Count0, idMtl, idObject }
							// Indices_0 = { iPosition, iTexCoord, iNormal (iTangent) }
							// ...
							// Indices_(Count0 - 1)
							// Info1
							// ...
	cList<cStr> m_Materials;
	cList<cVec4> m_Indices; // m_Indices.Count() == m_Weights.Count() == m_Positions.Count()
	cList<cVec4> m_Weights;
private:
	// Split hashtable
	struct HashEntry {
		int Index;
		HashEntry *pNext;
	};
	typedef cList<HashEntry *> HashTable;
	mutable cList<HashTable> m_HashTables;

	//-------------------------------------------------------------------------
	// AddVertexToHashTable
	//-------------------------------------------------------------------------
	template<class VERTEX>
	int AddVertexToHashTable(const int nHashTable, cList<VERTEX> &Vertices, const VERTEX *pVertex, const int HashValue) const {
		bool IsFound = false;
		int Index = 0;

		HashTable &H = m_HashTables[nHashTable];
		if(H.Count() > HashValue) {
			HashEntry *pEntry = H[HashValue];
			while(pEntry != nullptr) {
				const VERTEX *pCacheVertex = &Vertices[pEntry->Index];
				// If this vertex equals the vertex already in the vertex buffer,
				// simply point the index buffer to the existing vertex.
				if(memcmp(pVertex, pCacheVertex, sizeof(VERTEX)) == 0) {
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
			pNewEntry->pNext = nullptr;

			// Grow the hashtable cash if needed:
			if(H.Count() <= HashValue) {
				H.Add(nullptr, HashValue - H.Count() + 1);
			}

			// Add to the end of the linked list:
			HashEntry *pCurEntry = H[HashValue];
			if(nullptr == pCurEntry) {
				// This is the head element:
				H[HashValue] = pNewEntry;
			} else {
				// Find the tail:
				while(pCurEntry->pNext != nullptr) {
					pCurEntry = pCurEntry->pNext;
				}
				pCurEntry->pNext = pNewEntry;
			}
		}
		return Index;
	} // AddVertexToHashTable

	void FreeHashTables() const;
};
