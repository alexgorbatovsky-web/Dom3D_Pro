#include "cMeshContainerAdapter.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

namespace comms {
namespace {

using Edge = std::pair<int, int>;

Edge OrderedEdge(int vertex1, int vertex2) {
    return std::minmax(vertex1, vertex2);
}

void AddTriangle(cMeshContainer::RawFaces &raw, int vertex1, int vertex2, int vertex3) {
    raw.Add(cVec3i(3, 0, 0));
    raw.Add(cVec3i(vertex1, -1, -1));
    raw.Add(cVec3i(vertex2, -1, -1));
    raw.Add(cVec3i(vertex3, -1, -1));
}

} // namespace

void cMeshContainer::Clear() {
    m_Positions.Clear();
    m_Normals.Clear();
    m_Raw.Clear();
    m_EdgeFaceCounts.clear();
}

void cMeshContainer::Triangulate() {
    RawFaces triangles;
    for (int face = 0; face < m_Raw.Count();) {
        const int count = m_Raw[face][0];
        if (count >= 3 && face + count < m_Raw.Count()) {
            const int first = m_Raw[face + 1][0];
            for (int corner = 1; corner + 1 < count; ++corner) {
                AddTriangle(triangles, first, m_Raw[face + corner + 1][0],
                            m_Raw[face + corner + 2][0]);
            }
        }
        face += std::max(count, 0) + 1;
    }
    m_Raw = triangles;
}

void cMeshContainer::CalcNormals() {
    m_Normals.SetCount(m_Positions.Count(), cVec3::Zero);
    for (int face = 0; face < m_Raw.Count();) {
        const int count = m_Raw[face][0];
        if (count >= 3 && face + count < m_Raw.Count()) {
            const int vertex0 = m_Raw[face + 1][0];
            for (int corner = 1; corner + 1 < count; ++corner) {
                const int vertex1 = m_Raw[face + corner + 1][0];
                const int vertex2 = m_Raw[face + corner + 2][0];
                const cVec3 normal = cVec3::Cross(
                    m_Positions[vertex1] - m_Positions[vertex0],
                    m_Positions[vertex2] - m_Positions[vertex0]);
                m_Normals[vertex0] += normal;
                m_Normals[vertex1] += normal;
                m_Normals[vertex2] += normal;
            }
        }
        face += std::max(count, 0) + 1;
    }
    for (int vertex = 0; vertex < m_Normals.Count(); ++vertex) {
        if (m_Normals[vertex].LengthSq() > 0.0f) {
            m_Normals[vertex].Normalize();
        }
    }
}

void cMeshContainer::CreateFone() {
    m_EdgeFaceCounts.clear();
    for (int face = 0; face < m_Raw.Count();) {
        const int count = m_Raw[face][0];
        if (count >= 2 && face + count < m_Raw.Count()) {
            for (int corner = 0; corner < count; ++corner) {
                const int vertex1 = m_Raw[face + corner + 1][0];
                const int vertex2 = m_Raw[face + (corner + 1) % count + 1][0];
                ++m_EdgeFaceCounts[OrderedEdge(vertex1, vertex2)];
            }
        }
        face += std::max(count, 0) + 1;
    }
}

int cMeshContainer::GetEdgeFaceCount(int vertex1, int vertex2) const {
    const auto found = m_EdgeFaceCounts.find(OrderedEdge(vertex1, vertex2));
    return found != m_EdgeFaceCounts.end() ? found->second : 0;
}

void cMeshContainer::TriSubd(
    int subdivisionCount,
    SubdSnapEdgeCallback *edgeCallback,
    SubdSnapMiddlePointCallback *middleCallback,
    void *context) {
    subdivisionCount = std::max(subdivisionCount, 0);
    Triangulate();
    CalcNormals();

    if (subdivisionCount == 0 || m_Raw.Count() == 0) {
        return;
    }

    const int segments = subdivisionCount + 1;
    std::map<Edge, std::vector<int>> edgeVertices;

    auto edgeVertex = [&](int from, int to, int step) {
        const Edge edge = OrderedEdge(from, to);
        auto &vertices = edgeVertices[edge];
        if (vertices.empty()) {
            vertices.resize(subdivisionCount, -1);
        }

        const bool reversed = from != edge.first;
        const int canonicalStep = reversed ? subdivisionCount - step - 1 : step;
        int &result = vertices[canonicalStep];
        if (result < 0) {
            const float weight = static_cast<float>(canonicalStep + 1) / segments;
            cVec3 position = m_Positions[edge.first] +
                (m_Positions[edge.second] - m_Positions[edge.first]) * weight;
            cVec3 normal = m_Normals[edge.first] +
                (m_Normals[edge.second] - m_Normals[edge.first]) * weight;
            normal.Normalize();
            result = m_Positions.Add(position);
            m_Normals.Add(normal);
            if (edgeCallback != nullptr) {
                edgeCallback(context, position, normal, result, edge.first, edge.second, canonicalStep);
                m_Positions[result] = position;
                m_Normals[result] = normal;
            }
        }
        return result;
    };

    RawFaces subdivided;
    for (int face = 0; face < m_Raw.Count(); face += 4) {
        const int corners[3] = {
            m_Raw[face + 1][0], m_Raw[face + 2][0], m_Raw[face + 3][0]};
        std::vector<std::vector<int>> lattice(segments + 1);

        for (int row = 0; row <= segments; ++row) {
            lattice[row].resize(segments - row + 1);
            for (int column = 0; column <= segments - row; ++column) {
                int vertex = -1;
                if (row == 0 && column == 0) {
                    vertex = corners[0];
                } else if (row == 0 && column == segments) {
                    vertex = corners[1];
                } else if (row == segments && column == 0) {
                    vertex = corners[2];
                } else if (row == 0) {
                    vertex = edgeVertex(corners[0], corners[1], column - 1);
                } else if (column == 0) {
                    vertex = edgeVertex(corners[0], corners[2], row - 1);
                } else if (row + column == segments) {
                    vertex = edgeVertex(corners[1], corners[2], row - 1);
                } else {
                    const float weight1 = static_cast<float>(column) / segments;
                    const float weight2 = static_cast<float>(row) / segments;
                    cVec3 position = m_Positions[corners[0]] +
                        (m_Positions[corners[1]] - m_Positions[corners[0]]) * weight1 +
                        (m_Positions[corners[2]] - m_Positions[corners[0]]) * weight2;
                    cVec3 normal = m_Normals[corners[0]] +
                        (m_Normals[corners[1]] - m_Normals[corners[0]]) * weight1 +
                        (m_Normals[corners[2]] - m_Normals[corners[0]]) * weight2;
                    normal.Normalize();
                    vertex = m_Positions.Add(position);
                    m_Normals.Add(normal);
                    if (middleCallback != nullptr) {
                        middleCallback(context, position, normal, vertex,
                                       corners[0], corners[1], corners[2], weight1, weight2);
                        m_Positions[vertex] = position;
                        m_Normals[vertex] = normal;
                    }
                }
                lattice[row][column] = vertex;
            }
        }

        for (int row = 0; row < segments; ++row) {
            for (int column = 0; column < segments - row; ++column) {
                AddTriangle(subdivided, lattice[row][column],
                            lattice[row][column + 1], lattice[row + 1][column]);
                if (column + 1 < segments - row) {
                    AddTriangle(subdivided, lattice[row][column + 1],
                                lattice[row + 1][column + 1], lattice[row + 1][column]);
                }
            }
        }
    }
    m_Raw = subdivided;
    CalcNormals();
    CreateFone();
}

} // namespace comms
