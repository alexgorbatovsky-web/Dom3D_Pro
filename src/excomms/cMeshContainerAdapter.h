#pragma once

#include "comms/comms.h"

#include <map>
#include <utility>

namespace comms {

using SubdSnapEdgeCallback = void(
    void *context, cVec3 &position, cVec3 &normal,
    int vertex, int edgeVertex1, int edgeVertex2, int weight);
using SubdSnapMiddlePointCallback = void(
    void *context, cVec3 &position, cVec3 &normal,
    int vertex, int faceVertex1, int faceVertex2, int faceVertex3,
    float weight1, float weight2);

// Lightweight mesh storage for the imported ContourToFill algorithms.
// Raw faces use the 3DCoat layout: a (vertexCount, 0, 0) record followed by
// vertexCount records whose x component contains a position index.
class cMeshContainer {
public:
    using Positions = cList<cVec3>;
    using RawFaces = cList<cVec3i>;

    void Clear();

    const Positions &GetPositions() const { return m_Positions; }
    Positions &GetPositions() { return m_Positions; }
    const RawFaces &GetRaw() const { return m_Raw; }
    RawFaces &GetRaw() { return m_Raw; }

    const cVec3 &GetPosition(int vertex) const { return m_Positions[vertex]; }
    const cVec3 &GetNormal(int vertex) const { return m_Normals[vertex]; }

    void CalcNormals();
    void CreateFone();
    int GetEdgeFaceCount(int vertex1, int vertex2) const;
    void SetDefaultObjMtl() {}
    void TriSubd(
        int subdivisionCount,
        SubdSnapEdgeCallback *edgeCallback = nullptr,
        SubdSnapMiddlePointCallback *middleCallback = nullptr,
        void *context = nullptr);

private:
    void Triangulate();

    Positions m_Positions;
    Positions m_Normals;
    RawFaces m_Raw;
    std::map<std::pair<int, int>, int> m_EdgeFaceCounts;
};

} // namespace comms
