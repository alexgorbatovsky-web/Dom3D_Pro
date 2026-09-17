#pragma once

#include "Solid.h"
#include "SurfaceFace.h"
#include <Standard_Failure.hxx>
#include <algorithm>
#include <vector>

namespace lowpoly {
struct Completion {
    int retained = 0;
    std::vector<int> triangulated;
    std::vector<int> missing;
    bool complete() const { return missing.empty() && retained + triangulated.size() > 0; }
};

inline bool HasMesh(const CSurfaceFace* surface)
{
    return surface && surface->IsInitMesh && surface->pMesh3D
        && !surface->pMesh3D->GetVertices().empty()
        && std::any_of(surface->pMesh3D->GetFaces().begin(),
            surface->pMesh3D->GetFaces().end(), [](const auto& face) {
                return !face.deleted && face.corners.size() >= 3;
            });
}

// Called only after the requested quadrangulation attempt. Successful face
// meshes are never rebuilt: even a failed face can contain provisional cells.
inline Completion CompleteWithTriangles(CSolid& solid, float step)
{
    Completion result;
    for (int i = 0; i < solid.GetNumSurfaces(); ++i) {
        auto* surface = solid.GetSurfaceFace(i);
        if (HasMesh(surface)) {
            ++result.retained;
            continue;
        }
        bool built = false;
        try {
            built = surface && !surface->m_Face.IsNull() && surface->BuldMeshTriangle(
                std::max(step * 0.1f, 0.0001f), solid.AngDeflection);
        } catch (const Standard_Failure&) {
        }
        if (built && HasMesh(surface)) result.triangulated.push_back(i);
        else result.missing.push_back(i);
    }
    return result;
}
}
