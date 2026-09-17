#include "CAlfaDoc.h"
#include "Dom3DProjectSerializer.h"
#include "solid/Solid.h"
#include "CMesh3D.h"
#include "SurfaceUVMapping.h"
#include <BRepGProp.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt2d.hxx>
#include <cmath>
#include <iostream>
#include <map>

static int TestFilletContours(const char* path, bool drafts)
{
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    if (!serializer.Load(QString::fromLocal8Bit(path), document, room, view, error)) {
        std::cerr << error.toStdString();
        return 1;
    }
    bool valid = true;
    int bodies = 0;
    for (const auto& object : document.GetObjects()) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid) continue;
        ++bodies;
        valid &= solid->GetNumSurfaces() == (drafts ? 50 : 69) && BRepCheck_Analyzer(solid->m_Shape).IsValid();
        solid->MeshQuadro = true;
        const auto cases = drafts
            ? std::vector<std::pair<float,bool>>{{.6f,false},{.4f,false},{.8f,false},{.6f,true},{.6f,false}}
            : std::vector<std::pair<float,bool>>{{.5f,false},{.35f,false},{.65f,false},{.5f,true},{.5f,false}};
        for (const auto [density, slx] : cases) {
            solid->MeshQuadroHoleSLX = slx;
            const bool built = solid->ReBuldMesh(1.f / density);
            valid &= built;
            std::cout << "density=" << density << " slx=" << slx << " built=" << built << '\n';
            std::vector<const CMesh3D*> meshes;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                auto* surface = solid->GetSurfaceFace(i);
                if (!surface || !surface->pMesh3D || !surface->IsInitMesh) {
                    std::cerr << "Missing surface " << i << '\n';
                    valid = false;
                    continue;
                }
                meshes.push_back(surface->pMesh3D);
                const auto& vertices = surface->pMesh3D->GetVertices();
                SurfaceUVMapping mapping(surface);
                size_t cells = 0, quads = 0, outside = 0;
                double area = 0;
                for (const auto& cell : surface->pMesh3D->GetFaces()) {
                    if (cell.deleted) continue;
                    ++cells;
                    quads += cell.corners.size() == 4;
                    if (cell.corners.size() < 3 || cell.corners.size() > 4) { valid = false; continue; }
                    Vec3 center{};
                    const auto a = vertices.at(cell.corners[0].v);
                    for (const auto& corner : cell.corners) center = center + vertices.at(corner.v);
                    center = center * (1.f / cell.corners.size());
                    SurfaceUVPoint uv;
                    if (!mapping.Project(center, uv)) valid = false;
                    else {
                        BRepClass_FaceClassifier classifier(TopoDS::Face(surface->m_Face),
                            gp_Pnt2d(uv.u, uv.v), 1.e-5);
                        outside += classifier.State() == TopAbs_OUT;
                    }
                    for (size_t k = 1; k + 1 < cell.corners.size(); ++k) {
                        const auto normal = cross(vertices.at(cell.corners[k].v)-a,
                            vertices.at(cell.corners[k+1].v)-a);
                        area += .5 * std::sqrt(double(dot(normal, normal)));
                    }
                }
                GProp_GProps properties;
                BRepGProp::SurfaceProperties(surface->m_Face, properties);
                const double ratio = area / properties.Mass();
                if (drafts ? i == 44 : (i == 0 || i == 1 || i == 2 || i == 4)) {
                    std::cout << " face=" << i << " cells=" << cells << " quads=" << quads
                              << " outside=" << outside << " areaRatio=" << ratio << '\n';
                    // Radius-one corner patches are deliberately coarse at
                    // these densities; chord area underestimates their CAD area.
                    const double minimumRatio = (i == 1 || i == 4) ? .80 : .97;
                    valid &= cells > 0 && quads > 0 && outside == 0
                        && ratio > minimumRatio && ratio < 1.03;
                }
                valid &= cells > 0;
            }
            auto welded = CMesh3D::CreateWelded(meshes);
            size_t open = 0, nonManifold = 0;
            if (!welded) valid = false;
            else {
                std::map<std::pair<size_t, size_t>, int> edges;
                for (const auto& cell : welded->GetFaces()) {
                    if (cell.deleted) continue;
                    for (size_t k = 0; k < cell.corners.size(); ++k) {
                        const auto a = cell.corners[k].v, b = cell.corners[(k+1)%cell.corners.size()].v;
                        // Singular CAD corners contain a collapsed quad side.
                        // After welding it is a vertex, not a topological edge.
                        if (a == b) continue;
                        ++edges[{std::min(a,b), std::max(a,b)}];
                    }
                }
                for (const auto& [edge, uses] : edges) {
                    open += uses == 1;
                    nonManifold += uses > 2;
                }
            }
            std::cout << " open=" << open << " nonManifold=" << nonManifold << std::endl;
            valid &= open == 0 && nonManifold == 0;
        }
    }
    return valid && bodies == 1 ? 0 : 1;
}

int TestBoolAndFill(const char* path) { return TestFilletContours(path, false); }
int TestBoxDraftsMesh(const char* path) { return TestFilletContours(path, true); }
