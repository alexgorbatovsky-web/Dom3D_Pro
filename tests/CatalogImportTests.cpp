#include "CAlfaDoc.h"
#include "CAssembled.h"
#include "CFacadeFurniture.h"
#include "CKitchenCabinet.h"
#include "CFurnitureDrawer.h"
#include "CFurnitureAssemblies.h"
#include "CGroup.h"
#include "CMesh3D.h"
#include "ContourQuadrangulator3DCoat.h"
#include "FillContour.h"
#include "SurfacePatchBuilder.h"
#include "SurfaceUVMapping.h"
#include "CPart.h"
#include "CPolyline.h"
#include "Conic.h"
#include "Dom3DProjectSerializer.h"
#include "LinkLine.h"
#include "Net.h"
#include "Plane.h"
#include "Point3d.h"
#include "SmartLine.h"
#include "SweptSolidBuilder.h"
#include "iges/SplineCurve.h"
#include "render/BlenderCyclesRenderer.h"
#include "render/NativeRaytraceRenderer.h"
#include "render/RenderScene.h"
#include "UndoRedo.h"
#include "Vector.h"
#include "ui/MaterialDrag.h"
#include "ui/ToolRegistry.h"
#include "solid/Solid.h"
#include "solid/SheetBendShapeBuilder.h"
#include "solid/SurfaceSet.h"
#include "StepIO.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepGProp.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRep_Tool.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>
#include <Standard_Failure.hxx>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QEventLoop>
#include <QTimer>
#include <QMimeData>
#include <QTemporaryDir>

#include <cmath>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <chrono>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

size_t ActiveFaceEdgeComponentCount(const CMesh3D& mesh) {
    const std::vector<CMesh3D::Face>& faces = mesh.GetFaces();
    std::vector<std::vector<size_t>> neighbours(faces.size());
    std::map<std::pair<size_t, size_t>, std::vector<size_t>> edge_faces;
    size_t active_face_count = 0;
    for (size_t face_index = 0; face_index < faces.size(); ++face_index) {
        const CMesh3D::Face& face = faces[face_index];
        if (face.deleted || face.corners.size() < 3)
            continue;
        ++active_face_count;
        for (size_t corner = 0; corner < face.corners.size(); ++corner) {
            const size_t first = face.corners[corner].v;
            const size_t second = face.corners[
                (corner + 1) % face.corners.size()].v;
            if (first != second)
                edge_faces[std::minmax(first, second)].push_back(face_index);
        }
    }
    for (const auto& entry : edge_faces) {
        const std::vector<size_t>& owners = entry.second;
        for (size_t first = 0; first < owners.size(); ++first) {
            for (size_t second = first + 1; second < owners.size(); ++second) {
                neighbours[owners[first]].push_back(owners[second]);
                neighbours[owners[second]].push_back(owners[first]);
            }
        }
    }
    size_t component_count = 0;
    std::vector<bool> visited(faces.size(), false);
    for (size_t start = 0; start < faces.size(); ++start) {
        if (visited[start] || faces[start].deleted
            || faces[start].corners.size() < 3) {
            continue;
        }
        ++component_count;
        std::vector<size_t> pending{start};
        visited[start] = true;
        while (!pending.empty()) {
            const size_t current = pending.back();
            pending.pop_back();
            for (size_t neighbour : neighbours[current]) {
                if (!visited[neighbour]) {
                    visited[neighbour] = true;
                    pending.push_back(neighbour);
                }
            }
        }
    }
    return active_face_count == 0 ? 0 : component_count;
}

size_t ClosedMeshBoundaryLoopCount(const CMesh3D& mesh, bool& manifold) {
    manifold = true;
    std::map<std::pair<size_t, size_t>, size_t> edge_use;
    for (const CMesh3D::Face& face : mesh.GetFaces()) {
        if (face.deleted || face.corners.size() < 3)
            continue;
        for (size_t corner = 0; corner < face.corners.size(); ++corner) {
            const size_t first = face.corners[corner].v;
            const size_t second = face.corners[
                (corner + 1) % face.corners.size()].v;
            if (first != second)
                ++edge_use[std::minmax(first, second)];
        }
    }
    std::map<size_t, std::vector<size_t>> boundary_neighbours;
    for (const auto& entry : edge_use) {
        if (entry.second > 2)
            manifold = false;
        if (entry.second == 1) {
            boundary_neighbours[entry.first.first].push_back(entry.first.second);
            boundary_neighbours[entry.first.second].push_back(entry.first.first);
        }
    }
    for (const auto& entry : boundary_neighbours) {
        if (entry.second.size() != 2)
            manifold = false;
    }
    size_t loops = 0;
    std::set<size_t> visited;
    for (const auto& entry : boundary_neighbours) {
        if (visited.count(entry.first) != 0)
            continue;
        ++loops;
        std::vector<size_t> pending{entry.first};
        visited.insert(entry.first);
        while (!pending.empty()) {
            const size_t current = pending.back();
            pending.pop_back();
            for (size_t neighbour : boundary_neighbours[current]) {
                if (visited.insert(neighbour).second)
                    pending.push_back(neighbour);
            }
        }
    }
    return loops;
}

void TestDenseNotchBoundaryQuadrangulation() {
    std::vector<Vec3> contour{
        {239.1999f, 22.1790f, 0.0f}, {240.2272f, 28.6650f, 0.0f},
        {243.2084f, 34.5161f, 0.0f}, {247.8519f, 39.1595f, 0.0f},
        {253.7029f, 42.1408f, 0.0f}, {260.1889f, 43.1680f, 0.0f},
        {294.9043f, 43.1680f, 0.0f}, {329.6198f, 43.1680f, 0.0f},
        {329.6198f, 112.6858f, 0.0f}, {329.6198f, 182.2036f, 0.0f},
        {329.6198f, 251.7214f, 0.0f}, {329.6198f, 321.2392f, 0.0f},
        {263.6958f, 321.2392f, 0.0f}, {197.7719f, 321.2392f, 0.0f},
        {131.8479f, 321.2392f, 0.0f}, {65.9240f, 321.2392f, 0.0f},
        {59.3363f, 279.9455f, 0.0f}, {52.7487f, 238.6518f, 0.0f},
        {148.0188f, 223.4995f, 0.0f}, {168.0394f, 116.5712f, 0.0f},
        {148.0188f, 9.6429f, 0.0f}, {193.6093f, 15.9110f, 0.0f}};
    CSurfaceFace fill_surface;
    CMesh3D result;
    CMesh3D triangles;
    std::string error;
    std::string rejection;
    require(fill_surface.MakeFilledContour(
                contour, {0.0f, 0.0f, 1.0f}, &result, false, &error,
                &triangles, &rejection),
            error.c_str());
    CMesh3D raw_quads;
    require(Build3DCoatQuadrangulation(
                triangles.GetVertices(), triangles.GetFaces(), &raw_quads,
                true),
            "Dense notch raw quadrangulator returned no mesh.");
    size_t quads = 0;
    size_t other_faces = 0;
    double covered_area = 0.0;
    for (const CMesh3D::Face& face : raw_quads.GetFaces()) {
        if (face.deleted || face.corners.size() < 3)
            continue;
        if (face.corners.size() == 4)
            ++quads;
        else
            ++other_faces;
        const Vec3 origin = raw_quads.GetVertices()[face.corners[0].v];
        for (size_t corner = 1; corner + 1 < face.corners.size(); ++corner) {
            const Vec3 first = raw_quads.GetVertices()[face.corners[corner].v];
            const Vec3 second = raw_quads.GetVertices()[face.corners[corner + 1].v];
            const Vec3 area_vector = cross(first - origin, second - origin);
            covered_area += 0.5 * std::sqrt(
                static_cast<double>(dot(area_vector, area_vector)));
        }
    }
    require(quads == 16 && other_faces == 0,
            "Dense notch topology differs from the 3DCoat reference mesh.");
    require(std::fabs(covered_area - 58648.4852) < 0.1,
            "Dense notch area differs from the 3DCoat reference mesh.");
}

void TestFusionCylinderFrontGuard() {
    // UV boundary captured from Extrude-Fusion at Density 0.40. The reference
    // advancing front used to grow from these 10 nodes to thousands of points
    // and never closed. A rejected candidate must return promptly and the
    // deterministic triangle pairing must recover the four strip quads.
    const std::vector<Vec3> contour{
        {0.000000594f, 0.000002228f, 0.0f},
        {-0.000000666f, 3.749998842f, 0.0f},
        {0.000000100f, 7.500000191f, 0.0f},
        {-0.000000386f, 11.249999771f, 0.0f},
        {0.000000380f, 15.000001120f, 0.0f},
        {-0.868399696f, 13.902750723f, 0.0f},
        {-1.570796450f, 11.899998957f, 0.0f},
        {-1.570796450f, 7.500002292f, 0.0f},
        {-1.570796450f, 3.100002661f, 0.0f},
        {-0.868399231f, 1.097250952f, 0.0f}};
    CSurfaceFace fill_surface;
    CMesh3D result;
    CMesh3D triangles;
    std::string error;
    std::string rejection;
    require(fill_surface.MakeFilledContour(contour, {0.0f, 0.0f, 1.0f},
                &result, false, &error, &triangles, &rejection),
            error.c_str());
    size_t quads = 0;
    size_t other_faces = 0;
    for (const CMesh3D::Face& face : result.GetFaces()) {
        if (face.deleted)
            continue;
        if (face.corners.size() == 4)
            ++quads;
        else
            ++other_faces;
    }
    require(!rejection.empty() && quads == 4 && other_faces == 0,
            "Fusion cylinder fallback did not recover four local quads.");

    const std::vector<Vec3> double_fillet_contour{
        {0.0f, 0.000000514f, 0.0f},
        {0.785398146f, 0.000000514f, 0.0f},
        {1.570796421f, 0.000000514f, 0.0f},
        {1.570796421f, 7.642077006f, 0.0f},
        {1.570796421f, 15.284153499f, 0.0f},
        {1.570796421f, 22.926229991f, 0.0f},
        {1.570796421f, 30.568307437f, 0.0f},
        {0.785432706f, 30.709334888f, 0.0f},
        {0.0f, 30.767757930f, 0.0f},
        {0.0f, 23.075818576f, 0.0f},
        {0.0f, 15.383879222f, 0.0f},
        {0.0f, 7.691938914f, 0.0f}};
    result.Clear();
    triangles.Clear();
    error.clear();
    rejection.clear();
    require(fill_surface.MakeFilledContour(double_fillet_contour,
                {0.0f, 0.0f, 1.0f}, &result, false, &error,
                &triangles, &rejection), error.c_str());
    quads = 0;
    size_t result_triangles = 0;
    for (const CMesh3D::Face& face : result.GetFaces()) {
        if (face.deleted)
            continue;
        quads += face.corners.size() == 4;
        result_triangles += face.corners.size() == 3;
        for (const MeshCorner& corner : face.corners) {
            require(corner.v < result.GetVertices().size(),
                    "Double-fillet fallback contains an invalid vertex.");
            const Vec3 point = result.GetVertices()[corner.v];
            require(point.x >= -1.0e-5f && point.x <= 1.57081f
                        && point.y >= -1.0e-5f && point.y <= 30.76777f,
                    "Double-fillet quadrangulation escaped its UV contour.");
        }
    }
    require(!rejection.empty() && quads == 4 && result_triangles == 2,
            "Double-fillet fallback did not recover its bounded local mesh.");
    std::vector<std::unique_ptr<CPolyline>> saved_boundaries;
    require(fill_surface.CreateLastQuadrangulationBoundaryPolylines(
                saved_boundaries)
                && saved_boundaries.size() == 2
                && saved_boundaries.back()->IsClosed()
                && saved_boundaries.back()->GetPointCount()
                    == double_fillet_contour.size(),
            "Rejected quadrangulator input was not retained as an XY boundary line.");

    // The same UV contour belongs to a radius-12 cylindrical fillet. Its U
    // coordinate is angular, so the quadrangulator must see the exact metric
    // development X=12*U rather than the misleading 1.57 x 30.7 UV strip.
    CSurfaceFace metric_cylinder_surface;
    const TopoDS_Shape cylinder =
        BRepPrimAPI_MakeCylinder(12.0, 31.0).Shape();
    for (TopExp_Explorer face(cylinder, TopAbs_FACE); face.More(); face.Next()) {
        const TopoDS_Face candidate = TopoDS::Face(face.Current());
        if (BRepAdaptor_Surface(candidate).GetType() == GeomAbs_Cylinder) {
            metric_cylinder_surface.m_Face = candidate;
            break;
        }
    }
    require(!metric_cylinder_surface.m_Face.IsNull(),
            "Could not construct the metric cylinder regression face.");
    result.Clear();
    triangles.Clear();
    error.clear();
    rejection.clear();
    require(metric_cylinder_surface.MakeFilledContour(double_fillet_contour,
                {0.0f, 0.0f, 1.0f}, &result, false, &error,
                &triangles, &rejection), error.c_str());
    quads = 0;
    result_triangles = 0;
    for (const CMesh3D::Face& face : result.GetFaces()) {
        if (face.deleted)
            continue;
        quads += face.corners.size() == 4;
        result_triangles += face.corners.size() == 3;
    }
    require(rejection.empty() && quads == 8 && result_triangles == 0,
            "Metric cylinder development did not produce eight bounded quads.");
}

void TestSpherePoleQuadroProjection() {
    CSurfaceFace sphere_surface;
    const TopoDS_Shape sphere = BRepPrimAPI_MakeSphere(10.0).Shape();
    for (TopExp_Explorer face(sphere, TopAbs_FACE); face.More(); face.Next()) {
        const TopoDS_Face candidate = TopoDS::Face(face.Current());
        if (BRepAdaptor_Surface(candidate).GetType() == GeomAbs_Sphere) {
            sphere_surface.m_Face = candidate;
            break;
        }
    }
    require(!sphere_surface.m_Face.IsNull(),
            "Could not construct the sphere-pole regression face.");

    // UV boundary captured from the pole-bearing spherical face in Ball-Filed.
    // In the native angular plane it crosses U=0 and has a misleading long
    // closing chord. In the sphere's polar metric it is one ordinary loop.
    const std::vector<Vec3> contour{
        {1.534514f, 1.252845f, 0.0f}, {1.184406f, 1.226769f, 0.0f},
        {0.913796f, 1.165219f, 0.0f}, {0.726968f, 1.081420f, 0.0f},
        {0.600404f, 0.984870f, 0.0f}, {0.513108f, 0.880935f, 0.0f},
        {0.451423f, 0.772534f, 0.0f}, {0.407014f, 0.661308f, 0.0f},
        {0.374774f, 0.548231f, 0.0f}, {0.351520f, 0.433913f, 0.0f},
        {0.335233f, 0.318766f, 0.0f}, {0.324643f, 0.203078f, 0.0f},
        {0.318986f, 0.087075f, 0.0f}, {0.159665f, 0.087067f, 0.0f},
        {0.000000f, 0.087066f, 0.0f}, {6.169028f, 0.087066f, 0.0f},
        {6.056111f, 0.087067f, 0.0f}, {5.943671f, 0.087067f, 0.0f},
        {5.831147f, 0.087067f, 0.0f}, {5.718157f, 0.087067f, 0.0f},
        {5.604464f, 0.087067f, 0.0f}, {5.489953f, 0.087067f, 0.0f},
        {5.374596f, 0.087067f, 0.0f}, {5.258427f, 0.087067f, 0.0f},
        {5.141520f, 0.087067f, 0.0f}, {5.023967f, 0.087067f, 0.0f},
        {4.905871f, 0.087067f, 0.0f}, {4.787337f, 0.087066f, 0.0f},
        {4.668469f, 0.087066f, 0.0f}, {4.549376f, 0.087067f, 0.0f},
        {4.430180f, 0.087067f, 0.0f}, {4.311020f, 0.087067f, 0.0f},
        {4.192060f, 0.087067f, 0.0f}, {4.073488f, 0.087067f, 0.0f},
        {3.955509f, 0.087068f, 0.0f}, {3.838339f, 0.087068f, 0.0f},
        {3.722178f, 0.087069f, 0.0f}, {3.607193f, 0.087069f, 0.0f},
        {3.493475f, 0.087069f, 0.0f}, {3.381004f, 0.087068f, 0.0f},
        {3.269600f, 0.087067f, 0.0f}, {3.158868f, 0.087066f, 0.0f},
        {3.048152f, 0.087067f, 0.0f}, {2.936491f, 0.087070f, 0.0f},
        {2.822605f, 0.087075f, 0.0f}, {2.816819f, 0.204964f, 0.0f},
        {2.805925f, 0.322523f, 0.0f}, {2.789112f, 0.439517f, 0.0f},
        {2.765020f, 0.555635f, 0.0f}, {2.731474f, 0.670438f, 0.0f},
        {2.685012f, 0.783263f, 0.0f}, {2.620032f, 0.893041f, 0.0f},
        {2.527305f, 0.997942f, 0.0f}, {2.391624f, 1.094664f, 0.0f},
        {2.189955f, 1.177073f, 0.0f}, {1.899250f, 1.234429f, 0.0f}};

    CMesh3D mesh;
    std::string error;
    require(sphere_surface.MakeFilledContour(contour,
                {0.0f, 0.0f, 1.0f}, &mesh, false, &error),
            error.c_str());
    require(mesh.RestoreTo3DFromUVSurface(&sphere_surface),
            "Could not restore the sphere-pole quad mesh to 3D.");
    size_t quads = 0;
    size_t other_faces = 0;
    double maximum_edge = 0.0;
    const auto& vertices = mesh.GetVertices();
    for (const CMesh3D::Face& face : mesh.GetFaces()) {
        if (face.deleted || face.corners.size() < 3)
            continue;
        quads += face.corners.size() == 4;
        other_faces += face.corners.size() != 4;
        for (size_t corner = 0; corner < face.corners.size(); ++corner) {
            const size_t first = face.corners[corner].v;
            const size_t second = face.corners[(corner + 1) % face.corners.size()].v;
            require(first < vertices.size() && second < vertices.size(),
                    "Sphere-pole mesh contains an invalid vertex index.");
            const Vec3 edge = vertices[second] - vertices[first];
            maximum_edge = std::max(maximum_edge,
                std::sqrt(static_cast<double>(dot(edge, edge))));
        }
    }
    require(quads > 100 && other_faces == 0,
            "Sphere-pole projection did not produce an all-quad mesh.");
    require(ActiveFaceEdgeComponentCount(mesh) == 1,
            "Sphere-pole projection split the mesh into islands.");
    require(maximum_edge < 5.0,
            "Sphere-pole projection retained an angular seam chord.");
}

void TestCylinderSeamTrimDirection() {
    constexpr double radius = 5.0;
    constexpr double height = 13.2;
    constexpr double cut_x = 1.372295;
    constexpr double cut_z_min = 2.053276;
    constexpr double cut_z_max = 10.553276;
    const TopoDS_Shape cylinder =
        BRepPrimAPI_MakeCylinder(radius, height).Shape();
    const TopoDS_Shape cutter = BRepPrimAPI_MakeBox(
        gp_Pnt(cut_x, -6.0, cut_z_min),
        gp_Pnt(6.0, 6.0, cut_z_max)).Shape();
    TopoDS_Shape cut_shape =
        BRepAlgoAPI_Cut(cylinder, cutter).Shape();

    CSolid solid(cut_shape);
    solid.MeshQuadro = true;
    require(solid.InitSurfaces() && solid.InitEdges()
                && solid.ReBuldMesh(1.0f / 0.50f),
            "Could not build the cylinder seam-trim regression solid.");

    bool verified_cylinder = false;
    for (int surface_index = 0;
         surface_index < solid.GetNumSurfaces(); ++surface_index) {
        CSurfaceFace* surface = solid.GetSurfaceFace(surface_index);
        if (!surface || !surface->pMesh3D
            || BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                != GeomAbs_Cylinder
            || surface->GetPreparedPolylineCount() < 8) {
            continue;
        }
        bool kept_back_side = false;
        size_t quads = 0;
        size_t other_faces = 0;
        double maximum_edge = 0.0;
        const auto& vertices = surface->pMesh3D->GetVertices();
        for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
            if (face.deleted || face.corners.size() < 3)
                continue;
            quads += face.corners.size() == 4;
            other_faces += face.corners.size() != 4;
            Vec3 center{};
            for (size_t corner = 0; corner < face.corners.size(); ++corner) {
                const size_t first = face.corners[corner].v;
                const size_t second = face.corners[
                    (corner + 1) % face.corners.size()].v;
                require(first < vertices.size() && second < vertices.size(),
                        "Cylinder seam trim produced an invalid mesh index.");
                center = center + vertices[first];
                const Vec3 edge = vertices[second] - vertices[first];
                maximum_edge = std::max(maximum_edge,
                    std::sqrt(static_cast<double>(dot(edge, edge))));
            }
            center = center * (1.0f / static_cast<float>(face.corners.size()));
            if (center.z > cut_z_min + 0.05
                && center.z < cut_z_max - 0.05) {
                require(center.x <= cut_x + 0.05,
                        "Cylinder seam trim retained the OCCT OUT side.");
                kept_back_side = kept_back_side || center.x < -3.0f;
            }
        }
        require(quads > 0 && other_faces == 0 && kept_back_side,
                "Cylinder seam trim did not retain the connected back side.");
        require(ActiveFaceEdgeComponentCount(*surface->pMesh3D) == 1,
                "Cylinder seam trim split the surface mesh into islands.");
        require(maximum_edge < 3.0,
                "Cylinder seam relocation left an undersampled long edge.");
        verified_cylinder = true;
    }
    require(verified_cylinder,
            "The seam-trim regression did not inspect its cylinder face.");
}

double PatchSignedArea(const std::vector<SurfacePatchPoint>& polygon) {
    double area = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const SurfacePatchPoint& a = polygon[i];
        const SurfacePatchPoint& b = polygon[(i + 1) % polygon.size()];
        area += a.u * b.v - b.u * a.v;
    }
    return area * 0.5;
}

bool SamePatchPoint(SurfacePatchPoint a, SurfacePatchPoint b) {
    return std::hypot(a.u - b.u, a.v - b.v) <= 1.0e-8;
}

void TestIslandBoundaryRemoval() {
    // Deliberately mix long and short boundary edges and use three holes.  The
    // patch builder may add bridge samples, but it must neither erase an input
    // boundary edge nor change the represented surface area.
    const std::vector<std::vector<SurfacePatchPoint>> contours{
        {{0.0, 0.0}, {17.0, 0.0}, {83.0, 0.0}, {120.0, 0.0},
         {120.0, 75.0}, {93.0, 75.0}, {70.0, 75.0}, {0.0, 75.0},
         {0.0, 58.0}, {0.0, 11.0}},
        // Its vertex average lies in the missing upper-right part of this L.
        {{12.0, 14.0}, {42.0, 14.0}, {42.0, 24.0},
         {22.0, 24.0}, {22.0, 44.0}, {12.0, 44.0}},
        {{48.0, 9.0}, {76.0, 9.0}, {76.0, 18.0}, {48.0, 18.0}},
        {{83.0, 38.0}, {108.0, 38.0}, {108.0, 64.0}, {83.0, 64.0}}};

    std::vector<std::vector<SurfacePatchPoint>> patches;
    std::string error;
    require(BuildSurfacePatchesWithoutHoles(contours, patches, &error),
            error.c_str());
    require(patches.size() >= 4,
            "Removing three holes did not create independent islands.");

    double expected_area = std::fabs(PatchSignedArea(contours.front()));
    for (size_t i = 1; i < contours.size(); ++i)
        expected_area -= std::fabs(PatchSignedArea(contours[i]));
    double patch_area = 0.0;
    for (const auto& patch : patches) {
        require(patch.size() >= 4,
                "Hole removal produced a degenerate island boundary.");
        patch_area += std::fabs(PatchSignedArea(patch));
    }
    require(std::fabs(patch_area - expected_area)
                <= std::max(1.0, expected_area) * 1.0e-9,
            "Hole removal changed the represented surface area.");

    for (const auto& contour : contours) {
        for (size_t edge = 0; edge < contour.size(); ++edge) {
            const SurfacePatchPoint a = contour[edge];
            const SurfacePatchPoint b = contour[(edge + 1) % contour.size()];
            size_t occurrences = 0;
            for (const auto& patch : patches) {
                for (size_t candidate = 0; candidate < patch.size(); ++candidate) {
                    const SurfacePatchPoint c = patch[candidate];
                    const SurfacePatchPoint d = patch[(candidate + 1) % patch.size()];
                    if ((SamePatchPoint(a, c) && SamePatchPoint(b, d))
                        || (SamePatchPoint(a, d) && SamePatchPoint(b, c))) {
                        ++occurrences;
                    }
                }
            }
            require(occurrences == 1,
                    "Hole removal lost or duplicated an original boundary edge.");
        }
    }
}

void DiagnoseQuadrangulatorObj(const std::string& path) {
    std::ifstream stream(path);
    require(static_cast<bool>(stream),
            "Could not open quadrangulator diagnostic OBJ.");
    std::vector<Vec3> vertices;
    std::vector<CMesh3D::Face> faces;
    std::string line;
    while (std::getline(stream, line)) {
        std::istringstream values(line);
        std::string keyword;
        values >> keyword;
        if (keyword == "v") {
            Vec3 vertex{};
            values >> vertex.x >> vertex.y >> vertex.z;
            require(static_cast<bool>(values),
                    "Invalid vertex in quadrangulator diagnostic OBJ.");
            vertices.push_back(vertex);
        } else if (keyword == "f") {
            CMesh3D::Face face;
            std::string token;
            while (values >> token) {
                const size_t separator = token.find('/');
                const int index = std::stoi(token.substr(0, separator));
                require(index > 0 && static_cast<size_t>(index) <= vertices.size(),
                        "Invalid face index in quadrangulator diagnostic OBJ.");
                const size_t vertex = static_cast<size_t>(index - 1);
                face.corners.push_back({vertex, vertex, vertex});
            }
            faces.push_back(std::move(face));
        }
    }
    require(!vertices.empty() && !faces.empty(),
            "Quadrangulator diagnostic OBJ contains no triangle mesh.");
    if (std::any_of(faces.begin(), faces.end(), [](const CMesh3D::Face& face) {
            return face.corners.size() != 3;
        })) {
        std::map<std::pair<size_t, size_t>, size_t> edge_use;
        std::map<size_t, size_t> next;
        for (const CMesh3D::Face& face : faces) {
            for (size_t i = 0; i < face.corners.size(); ++i) {
                const size_t a = face.corners[i].v;
                const size_t b = face.corners[(i + 1) % face.corners.size()].v;
                ++edge_use[std::minmax(a, b)];
            }
        }
        for (const CMesh3D::Face& face : faces) {
            for (size_t i = 0; i < face.corners.size(); ++i) {
                const size_t a = face.corners[i].v;
                const size_t b = face.corners[(i + 1) % face.corners.size()].v;
                if (edge_use[std::minmax(a, b)] == 1)
                    next[a] = b;
            }
        }
        require(!next.empty(), "Diagnostic quad OBJ has no open boundary.");
        std::vector<Vec3> contour;
        size_t current = next.begin()->first;
        const size_t start = current;
        do {
            contour.push_back(vertices[current]);
            const auto following = next.find(current);
            require(following != next.end(),
                    "Diagnostic quad OBJ has a broken boundary.");
            current = following->second;
        } while (current != start && contour.size() <= next.size());
        require(current == start && contour.size() == next.size(),
                "Diagnostic quad OBJ has more than one boundary.");
        CMesh3D triangle_mesh;
        require(FillContorByTriangles(
                    &triangle_mesh, contour, {0.0f, 0.0f, 1.0f}),
                "Could not reconstruct ContourToFill input from quad boundary.");
        vertices = triangle_mesh.GetVertices();
        faces = triangle_mesh.GetFaces();
    }
    CMesh3D result;
    require(Build3DCoatQuadrangulation(
                vertices, faces, &result, true),
            "The exact-reference quadrangulator returned no mesh.");
    size_t quads = 0;
    size_t triangles = 0;
    double signed_area = 0.0;
    for (const CMesh3D::Face& face : result.GetFaces()) {
        if (face.deleted || face.corners.size() < 3)
            continue;
        quads += face.corners.size() == 4;
        triangles += face.corners.size() == 3;
        double twice_area = 0.0;
        for (size_t i = 0; i < face.corners.size(); ++i) {
            const Vec3& a = result.GetVertices()[face.corners[i].v];
            const Vec3& b = result.GetVertices()[
                face.corners[(i + 1) % face.corners.size()].v];
            twice_area += static_cast<double>(a.x) * b.y
                - static_cast<double>(b.x) * a.y;
        }
        signed_area += twice_area * 0.5;
    }
    std::cout << "vertices=" << result.GetVertices().size()
              << " quads=" << quads
              << " triangles=" << triangles
              << " signed_area=" << std::fabs(signed_area) << '\n';
    require(result.ExportToObj(path + ".DomExact.obj"),
            "Could not export the exact-reference diagnostic mesh.");
}

void TestLowPolyQuadroBranch(bool skip_seam_regression = false) {
    if (!skip_seam_regression) {
        CMesh3D seam_mesh;
        std::vector<Vec3> vertices{
            {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
            {2.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
            {1.0f, 1.0f, 0.0f}, {2.0f, 1.0f, 0.0f}};
        std::vector<CMesh3D::Face> faces{
            CMesh3D::Face({0, 1, 4, 3}),
            CMesh3D::Face({1, 2, 5, 4})};
        require(seam_mesh.SetGeometry(
                    std::move(vertices), std::move(faces), {}, {}),
                "Could not prepare the quad seam propagation mesh.");
        require(seam_mesh.SynchronizeBoundaryVertices(
                    {{0.0f, 0.0f, 0.0f},
                     {0.0f, 0.5f, 0.0f},
                     {0.0f, 1.0f, 0.0f}}, 0.01f) > 0,
                "A missing seam node was not inserted.");
        size_t active_face_count = 0;
        for (const CMesh3D::Face& face : seam_mesh.GetFaces()) {
            if (face.deleted)
                continue;
            ++active_face_count;
            require(face.corners.size() == 4,
                    "Seam propagation converted a quad into a triangle.");
        }
        require(active_face_count == 4,
                "The seam insertion did not propagate across the quad strip.");
        const auto has_vertex = [&](Vec3 expected) {
            return std::any_of(
                seam_mesh.GetVertices().begin(), seam_mesh.GetVertices().end(),
                [&](Vec3 vertex) {
                    return dot(vertex - expected, vertex - expected) < 1.0e-8f;
                });
        };
        require(has_vertex({0.0f, 0.5f, 0.0f})
                    && has_vertex({2.0f, 0.5f, 0.0f}),
                "The propagated seam strip did not reach both boundaries.");

        const auto require_trim_topology = [](double offset_x,
                                               double offset_y) {
            constexpr int columns = 12;
            constexpr int rows = 8;
            constexpr float step = 5.0f;
            std::vector<Vec3> grid_vertices;
            std::vector<CMesh3D::Face> grid_faces;
            for (int row = 0; row <= rows; ++row) {
                for (int column = 0; column <= columns; ++column)
                    grid_vertices.push_back({column * step, row * step, 0.0f});
            }
            const auto vertex_index = [](int column, int row) {
                return static_cast<size_t>(row * (columns + 1) + column);
            };
            for (int row = 0; row < rows; ++row) {
                for (int column = 0; column < columns; ++column) {
                    grid_faces.emplace_back(std::initializer_list<size_t>{
                        vertex_index(column, row),
                        vertex_index(column + 1, row),
                        vertex_index(column + 1, row + 1),
                        vertex_index(column, row + 1)});
                }
            }
            CMesh3D trim_mesh;
            require(trim_mesh.SetGeometry(
                        std::move(grid_vertices), std::move(grid_faces)),
                    "Could not prepare direct TrimByPline quad grid.");
            CPolyline trim_line;
            constexpr int samples = 16;
            constexpr double radius = 7.3;
            const double center_x = 30.0 + offset_x;
            const double center_y = 20.0 + offset_y;
            for (int sample = 0; sample < samples; ++sample) {
                const double angle = 2.0 * M_PI * sample / samples;
                trim_line.AddPoint({center_x + radius * std::cos(angle),
                                    center_y + radius * std::sin(angle), 0.0});
            }
            trim_line.SetClosed(true);
            require(trim_mesh.TrimByPline(&trim_line, {1.0, 1.0, 0.0}),
                    "TrimByPline rejected a closed contour over a quad grid.");

            const auto point_inside_trim = [&](double x, double y) {
                bool inside = false;
                for (int i = 0, j = samples - 1; i < samples; j = i++) {
                    const CPoint3d* a = trim_line.P(i);
                    const CPoint3d* b = trim_line.P(j);
                    if (((a->y > y) != (b->y > y))
                        && x < (b->x - a->x) * (y - a->y)
                                / (b->y - a->y) + a->x) {
                        inside = !inside;
                    }
                }
                return inside;
            };

            std::map<std::pair<size_t, size_t>, int> edge_use;
            for (const CMesh3D::Face& face : trim_mesh.GetFaces()) {
                if (face.deleted)
                    continue;
                require(face.corners.size() >= 3,
                        "TrimByPline produced a face with fewer than three corners.");
                std::set<size_t> unique_vertices;
                double twice_area = 0.0;
                double center_x_sum = 0.0;
                double center_y_sum = 0.0;
                for (size_t corner = 0; corner < face.corners.size(); ++corner) {
                    const size_t first = face.corners[corner].v;
                    const size_t second = face.corners[
                        (corner + 1) % face.corners.size()].v;
                    require(first < trim_mesh.GetVertices().size()
                                && second < trim_mesh.GetVertices().size(),
                            "TrimByPline produced an invalid vertex index.");
                    unique_vertices.insert(first);
                    ++edge_use[std::minmax(first, second)];
                    const Vec3& a = trim_mesh.GetVertices()[first];
                    const Vec3& b = trim_mesh.GetVertices()[second];
                    center_x_sum += a.x;
                    center_y_sum += a.y;
                    twice_area += static_cast<double>(a.x) * b.y
                        - static_cast<double>(b.x) * a.y;
                }
                require(unique_vertices.size() == face.corners.size(),
                        "TrimByPline produced a face with repeated vertices.");
                require(twice_area > 1.0e-7,
                        "TrimByPline produced a zero-area or inverted face.");
                const double inverse_corner_count =
                    1.0 / static_cast<double>(face.corners.size());
                require(!point_inside_trim(
                            center_x_sum * inverse_corner_count,
                            center_y_sum * inverse_corner_count),
                        "TrimByPline retained a face centre inside the hole.");
            }
            std::map<size_t, int> boundary_degree;
            std::map<size_t, std::vector<size_t>> boundary_neighbors;
            const auto point_segment_distance_sq = [](Vec3 point, Vec3 a, Vec3 b) {
                const Vec3 direction = b - a;
                const float length_sq = dot(direction, direction);
                const float alpha = length_sq > 1.0e-20f
                    ? std::clamp(dot(point - a, direction) / length_sq, 0.0f, 1.0f)
                    : 0.0f;
                const Vec3 delta = point - (a + direction * alpha);
                return dot(delta, delta);
            };
            for (const auto& edge : edge_use) {
                require(edge.second <= 2,
                        "TrimByPline produced a non-manifold edge.");
                if (edge.second == 1) {
                    ++boundary_degree[edge.first.first];
                    ++boundary_degree[edge.first.second];
                    boundary_neighbors[edge.first.first].push_back(edge.first.second);
                    boundary_neighbors[edge.first.second].push_back(edge.first.first);
                    const Vec3& a = trim_mesh.GetVertices()[edge.first.first];
                    const Vec3& b = trim_mesh.GetVertices()[edge.first.second];
                    const Vec3 midpoint = (a + b) * 0.5f;
                    const bool outer = std::fabs(midpoint.x) < 1.0e-5f
                        || std::fabs(midpoint.y) < 1.0e-5f
                        || std::fabs(midpoint.x - columns * step) < 1.0e-5f
                        || std::fabs(midpoint.y - rows * step) < 1.0e-5f;
                    float nearest_trim_sq = std::numeric_limits<float>::max();
                    for (int sample = 0; sample < samples; ++sample) {
                        const CPoint3d* first = trim_line.P(sample);
                        const CPoint3d* second = trim_line.P((sample + 1) % samples);
                        nearest_trim_sq = std::min(nearest_trim_sq,
                            point_segment_distance_sq(midpoint,
                                {static_cast<float>(first->x),
                                 static_cast<float>(first->y), 0.0f},
                                {static_cast<float>(second->x),
                                 static_cast<float>(second->y), 0.0f}));
                    }
                    if (!outer && nearest_trim_sq >= 1.0e-6f) {
                        std::cerr << "Trim offset " << offset_x << "," << offset_y
                                  << " bad boundary edge (" << a.x << "," << a.y
                                  << ")-(" << b.x << "," << b.y << ") distance2="
                                  << nearest_trim_sq << '\n';
                    }
                    require(outer || nearest_trim_sq < 1.0e-6f,
                            "TrimByPline left an open edge away from the trim contour.");
                }
            }
            require(!boundary_degree.empty(),
                    "TrimByPline produced no open boundary.");
            for (const auto& vertex : boundary_degree) {
                require(vertex.second == 2,
                        "TrimByPline left a broken boundary or T-junction.");
            }
            std::set<size_t> visited_boundary;
            int boundary_components = 0;
            for (const auto& vertex : boundary_neighbors) {
                if (!visited_boundary.insert(vertex.first).second)
                    continue;
                ++boundary_components;
                std::vector<size_t> pending{vertex.first};
                while (!pending.empty()) {
                    const size_t current = pending.back();
                    pending.pop_back();
                    for (size_t next : boundary_neighbors[current]) {
                        if (visited_boundary.insert(next).second)
                            pending.push_back(next);
                    }
                }
            }
            require(boundary_components == 2,
                    "TrimByPline did not produce exactly an outer and a hole boundary.");
        };
        for (const auto& offset : std::array<std::pair<double, double>, 8>{
                 std::pair{0.2, 0.3}, std::pair{0.7, 0.9},
                 std::pair{1.4, -0.8}, std::pair{2.4, -1.6},
                 std::pair{-1.1, 2.1}, std::pair{-2.3, -1.7},
                 std::pair{3.3, 2.7}, std::pair{-3.6, 3.4}}) {
            require_trim_topology(offset.first, offset.second);
        }
    }

    const auto require_rectangular_box_quads = [](double x, double y, double z,
                                                   float density) {
        TopoDS_Shape box_shape = BRepPrimAPI_MakeBox(x, y, z).Shape();
        CSolid box(box_shape);
        box.MeshQuadro = true;
        require(box.InitSurfaces() && box.InitEdges()
                    && box.ReBuldMesh(1.0f / density),
                "Rectangular Low Poly box was not built.");
        for (int surface_index = 0; surface_index < box.GetNumSurfaces(); ++surface_index) {
            const CSurfaceFace* surface = box.GetSurfaceFace(surface_index);
            require(surface && surface->pMesh3D,
                    "Rectangular Low Poly box skipped a surface.");
            for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
                if (!face.deleted)
                    require(face.corners.size() == 4,
                            "A rectangular Low Poly face retained a triangle.");
            }
        }
    };
    require_rectangular_box_quads(30.0, 16.0, 8.0, 0.45f);

    const auto cube_face_counts_at_density_02 = [](double size) {
        TopoDS_Shape shape = BRepPrimAPI_MakeBox(size, size, size).Shape();
        CSolid solid(shape);
        solid.MeshQuadro = true;
        // Low Poly passes Deflection = 1 / Density.
        require(solid.InitSurfaces() && solid.InitEdges()
                    && solid.ReBuldMesh(5.0f),
                "Density 0.2 Low Poly cube was not built.");
        std::vector<size_t> counts;
        for (int surface_index = 0;
             surface_index < solid.GetNumSurfaces(); ++surface_index) {
            const CSurfaceFace* surface = solid.GetSurfaceFace(surface_index);
            require(surface && surface->pMesh3D
                        && surface->m_TypeMesh == REGULAR_MESH,
                    "Density 0.2 cube contains an unexpected trimmed face.");
            size_t count = 0;
            for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
                if (!face.deleted)
                    ++count;
            }
            counts.push_back(count);
        }
        return counts;
    };

    const std::vector<size_t> small_cube_counts =
        cube_face_counts_at_density_02(40.0);
    const std::vector<size_t> large_cube_counts =
        cube_face_counts_at_density_02(160.0);
    require(small_cube_counts == large_cube_counts,
            "Density 0.2 depends on the absolute cube size.");
    require(std::all_of(small_cube_counts.begin(), small_cube_counts.end(),
                        [](size_t count) { return count == 144; }),
            "Density 0.2 did not create the calibrated 12 by 12 grid per cube face.");

    const TopoDS_Shape outer =
        BRepPrimAPI_MakeBox(40.0, 40.0, 10.0).Shape();
    const TopoDS_Shape hole = BRepPrimAPI_MakeBox(
        gp_Pnt(14.0, 14.0, -1.0), 12.0, 12.0, 12.0).Shape();
    TopoDS_Shape perforated_box = BRepAlgoAPI_Cut(outer, hole).Shape();

    CSolid hybrid_display(perforated_box);
    require(hybrid_display.InitSurfaces()
                && hybrid_display.InitEdges()
                && hybrid_display.ReBuldMesh(2.0f),
            "Hybrid display mesh for a trimmed solid was not built.");
    bool hybrid_has_regular_quads = false;
    bool hybrid_has_trimmed_triangles = false;
    for (int surface_index = 0;
         surface_index < hybrid_display.GetNumSurfaces(); ++surface_index) {
        const CSurfaceFace* surface =
            hybrid_display.GetSurfaceFace(surface_index);
        require(surface && surface->pMesh3D,
                "Hybrid display mesh skipped a surface.");
        for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
            if (face.deleted)
                continue;
            if (surface->m_TypeMesh == REGULAR_MESH)
                hybrid_has_regular_quads = hybrid_has_regular_quads
                    || face.corners.size() == 4;
            else
                hybrid_has_trimmed_triangles = hybrid_has_trimmed_triangles
                    || face.corners.size() == 3;
        }
    }
    require(hybrid_has_regular_quads,
            "Hybrid display did not use CNet for natural UV faces.");
    require(hybrid_has_trimmed_triangles,
            "Hybrid display did not use OCCT for trimmed faces.");

    CSolid quadro_low_poly(perforated_box);
    quadro_low_poly.MeshQuadro = true;
    require(quadro_low_poly.InitSurfaces()
                && quadro_low_poly.InitEdges()
                && quadro_low_poly.ReBuldMesh(2.0f),
            "Low Poly quad mesh for a trimmed solid was not built.");
    bool found_trimmed_surface = false;
    bool verified_new_hole_path = false;
    bool found_subdivided_regular_surface = false;
    size_t fine_regular_face_count = 0;
    for (int surface_index = 0;
         surface_index < quadro_low_poly.GetNumSurfaces(); ++surface_index) {
        CSurfaceFace* surface =
            quadro_low_poly.GetSurfaceFace(surface_index);
        require(surface && surface->pMesh3D,
                "Low Poly skipped a surface in Mesh Quadro mode.");
        bool has_active_face = false;
        bool has_quad = false;
        for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
            if (face.deleted)
                continue;
            has_active_face = true;
            has_quad = has_quad || face.corners.size() == 4;
            if (surface->m_TypeMesh == REGULAR_MESH)
                ++fine_regular_face_count;
        }
        require(has_active_face,
                "Low Poly produced an empty surface in Mesh Quadro mode.");
        if (surface->m_TypeMesh == REGULAR_MESH
            && surface->pMesh3D->GetFaces().size() > 1) {
            found_subdivided_regular_surface = true;
        }
        if (surface->m_TypeMesh != REGULAR_MESH) {
            found_trimmed_surface = true;
            require(surface->BuildFilledMeshWhithHoles(2.0f),
                    "The direct UV patch builder rejected a trimmed surface.");
            require(ActiveFaceEdgeComponentCount(*surface->pMesh3D) == 1,
                    "UV patches of one trimmed surface were not welded into one mesh.");
            require(std::fabs(surface->GetLastLowPolyDensity() - 0.5f)
                        < 1.0e-6f,
                    "Trimmed surface forgot its last Low Poly density.");
            std::vector<std::unique_ptr<CPolyline>> island_boundaries;
            require(surface->CreateLastIslandBoundaryPolylines(
                        island_boundaries)
                        && island_boundaries.size() >= 2,
                    "Trimmed surface did not retain its generated island boundaries.");
            for (const std::unique_ptr<CPolyline>& boundary : island_boundaries) {
                require(boundary && boundary->IsClosed()
                            && boundary->GetPointCount() >= 3,
                        "A cached island boundary is invalid.");
            }
            verified_new_hole_path = true;
            require(has_quad,
                    "A trimmed Low Poly surface fell back to OCCT triangles.");

            const std::vector<UV>& uvs = surface->pMesh3D->GetUVs();
            for (const CMesh3D::Face& mesh_face : surface->pMesh3D->GetFaces()) {
                if (mesh_face.deleted || mesh_face.corners.size() < 3)
                    continue;
                double u = 0.0;
                double v = 0.0;
                for (const MeshCorner& corner : mesh_face.corners) {
                    require(corner.uv < uvs.size(),
                            "UV patch mesh contains an invalid UV index.");
                    u += uvs[corner.uv].u;
                    v += uvs[corner.uv].v;
                }
                u /= static_cast<double>(mesh_face.corners.size());
                v /= static_cast<double>(mesh_face.corners.size());
                BRepClass_FaceClassifier classifier(
                    TopoDS::Face(surface->m_Face), gp_Pnt2d(u, v),
                    1.0e-7, Standard_False);
                require(classifier.State() != TopAbs_OUT,
                        "UV patch mesh filled a face hole.");
            }
        }
    }
    require(found_trimmed_surface,
            "Low Poly regression shape did not expose a trimmed surface.");
    require(verified_new_hole_path,
            "The new UV hole-to-islands path was not exercised.");
    require(found_subdivided_regular_surface,
            "Low Poly left a natural UV surface as one quad.");

	// An odd three-node contour cannot have an all-quad cover without changing
	// its boundary. It therefore exercises the requested safety path and must
	// return the original ContourToFill triangle instead of QuadEars/Grid.
	{
		CSurfaceFace fill_surface;
		CMesh3D fallback_mesh;
		std::string fill_error;
		require(fill_surface.MakeFilledContour(
			{{0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f},
			 {0.0f, 3.0f, 0.0f}},
			{0.0f, 0.0f, 1.0f}, &fallback_mesh, false, &fill_error),
			"ContourToFill triangle fallback was rejected.");
		require(fallback_mesh.GetFaces().size() == 1
				&& fallback_mesh.GetFaces().front().corners.size() == 3,
			"A failed quadrangulation did not preserve ContourToFill output.");
	}

    // Exercise the complete islands -> triangles -> Shpagin quadrangulator
    // path on a planar face containing several unlike holes.
    TopoDS_Shape complex_perforated =
        BRepPrimAPI_MakeBox(100.0, 80.0, 10.0).Shape();
    for (const auto& cylinder : std::array<std::array<double, 3>, 2>{
             std::array<double, 3>{25.0, 55.0, 8.0},
             std::array<double, 3>{68.0, 55.0, 11.0}}) {
        const TopoDS_Shape cutter = BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(cylinder[0], cylinder[1], -1.0),
                   gp_Dir(0.0, 0.0, 1.0)), cylinder[2], 12.0).Shape();
        complex_perforated = BRepAlgoAPI_Cut(
            complex_perforated, cutter).Shape();
    }
    complex_perforated = BRepAlgoAPI_Cut(complex_perforated,
        BRepPrimAPI_MakeBox(gp_Pnt(22.0, 14.0, -1.0),
                            58.0, 13.0, 12.0).Shape()).Shape();
    CSolid complex_quadro(complex_perforated);
    complex_quadro.MeshQuadro = true;
    require(complex_quadro.InitSurfaces() && complex_quadro.InitEdges()
                && complex_quadro.ReBuldMesh(1.25f),
            "Complex multi-hole Low Poly mesh was not built.");
    bool complex_trimmed_face_found = false;
    for (int surface_index = 0;
         surface_index < complex_quadro.GetNumSurfaces(); ++surface_index) {
        const CSurfaceFace* surface =
            complex_quadro.GetSurfaceFace(surface_index);
        if (!surface || !surface->pMesh3D
            || surface->m_TypeMesh == REGULAR_MESH) {
            continue;
        }
        complex_trimmed_face_found = true;
        for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
            if (face.deleted)
                continue;
            require(face.corners.size() == 4,
                    "Complex multi-hole Low Poly mesh retained a triangle.");
        }
    }
    require(complex_trimmed_face_found,
            "Complex multi-hole regression did not contain a trimmed face.");

    // Three long rectangular openings reproduce the large right-hand island
    // from the Low Poly report: its boundary is valid, but a failed island
    // fill used to make BuildTrimmingMesh silently replace the complete top
    // face with the legacy CNet triangulation.
    TopoDS_Shape long_slot_shape =
        BRepPrimAPI_MakeBox(100.0, 80.0, 10.0).Shape();
    for (const std::array<double, 4>& slot : {
             std::array<double, 4>{12.0, 48.0, 22.0, 8.0},
             std::array<double, 4>{52.0, 48.0, 22.0, 8.0},
             std::array<double, 4>{14.0, 15.0, 68.0, 8.0}}) {
        long_slot_shape = BRepAlgoAPI_Cut(
            long_slot_shape,
            BRepPrimAPI_MakeBox(gp_Pnt(slot[0], slot[1], -1.0),
                                slot[2], slot[3], 12.0).Shape()).Shape();
    }
    CSolid long_slot_quadro(long_slot_shape);
    long_slot_quadro.MeshQuadro = true;
    require(long_slot_quadro.InitSurfaces() && long_slot_quadro.InitEdges()
                && long_slot_quadro.ReBuldMesh(1.0f / 1.05f),
            "Low Poly failed for the three-long-slot regression.");
    bool long_slot_face_found = false;
    for (int surface_index = 0;
         surface_index < long_slot_quadro.GetNumSurfaces(); ++surface_index) {
        CSurfaceFace* surface = long_slot_quadro.GetSurfaceFace(surface_index);
        if (!surface || surface->m_TypeMesh == REGULAR_MESH
            || surface->GetPreparedPolylineCount() < 8
            || BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                != GeomAbs_Plane) {
            continue;
        }
        require(surface->BuildFilledMeshWhithHoles(1.0f / 1.05f),
                "The right-hand long-slot island was rejected.");
        for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
            if (!face.deleted)
                require(face.corners.size() == 4,
                        "The right-hand long-slot island retained a triangle.");
        }
        long_slot_face_found = true;
    }
    require(long_slot_face_found,
            "The long-slot regression did not inspect its planar top face.");

    bool verified_regular_to_trimmed_boundary = false;
    const auto point_distance_sq = [](const CPoint3d& a, const CPoint3d& b) {
        const double dx = a.x - b.x;
        const double dy = a.y - b.y;
        const double dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    };
    for (int regular_index = 0;
         regular_index < quadro_low_poly.GetNumSurfaces(); ++regular_index) {
        CSurfaceFace* regular = quadro_low_poly.GetSurfaceFace(regular_index);
        if (!regular || regular->m_TypeMesh != REGULAR_MESH)
            continue;
        for (int regular_edge = 0;
             regular_edge < regular->GetPreparedPolylineCount(); ++regular_edge) {
            TopoDS_Edge regular_topo_edge;
            std::vector<CPoint3d> regular_boundary;
            if (!regular->GetPreparedTopoEdge(regular_edge, regular_topo_edge)
                || !regular->GetRegularMeshBoundaryPoints(
                    regular_edge, regular_boundary)) {
                continue;
            }
            for (int trimmed_index = 0;
                 trimmed_index < quadro_low_poly.GetNumSurfaces(); ++trimmed_index) {
                CSurfaceFace* trimmed = quadro_low_poly.GetSurfaceFace(trimmed_index);
                if (!trimmed || trimmed->m_TypeMesh == REGULAR_MESH)
                    continue;
                for (int trimmed_edge = 0;
                     trimmed_edge < trimmed->GetPreparedPolylineCount(); ++trimmed_edge) {
                    TopoDS_Edge trimmed_topo_edge;
                    std::vector<CPoint3d> trimmed_boundary;
                    if (!trimmed->GetPreparedTopoEdge(trimmed_edge, trimmed_topo_edge)
                        || !trimmed_topo_edge.IsSame(regular_topo_edge)
                        || !trimmed->GetPreparedPolylinePoints(
                            trimmed_edge, trimmed_boundary)) {
                        continue;
                    }
                    require(trimmed_boundary.size() == regular_boundary.size(),
                            "Trimmed Boundary Line does not use the regular mesh node count.");
                    const bool same_direction = point_distance_sq(
                        trimmed_boundary.front(), regular_boundary.front())
                        <= point_distance_sq(
                            trimmed_boundary.front(), regular_boundary.back());
                    for (size_t point_index = 0;
                         point_index < regular_boundary.size(); ++point_index) {
                        const size_t regular_point = same_direction
                            ? point_index
                            : regular_boundary.size() - 1 - point_index;
                        require(point_distance_sq(
                                    trimmed_boundary[point_index],
                                    regular_boundary[regular_point]) < 1.0e-8,
                                "Trimmed Boundary Line node differs from the finished regular mesh boundary.");
                    }
                    verified_regular_to_trimmed_boundary = true;
                }
            }
        }
    }
    require(verified_regular_to_trimmed_boundary,
            "The Low Poly regression did not verify a regular-to-trimmed seam.");

    TopoDS_Shape two_hole_shape = BRepPrimAPI_MakeBox(60.0, 36.0, 8.0).Shape();
    const TopoDS_Shape first_cutter = BRepPrimAPI_MakeBox(
        gp_Pnt(10.0, 10.0, -1.0), 10.0, 10.0, 10.0).Shape();
    const TopoDS_Shape second_cutter = BRepPrimAPI_MakeBox(
        gp_Pnt(40.0, 10.0, -1.0), 10.0, 10.0, 10.0).Shape();
    two_hole_shape = BRepAlgoAPI_Cut(two_hole_shape, first_cutter).Shape();
    two_hole_shape = BRepAlgoAPI_Cut(two_hole_shape, second_cutter).Shape();
    CSolid two_hole_low_poly(two_hole_shape);
    two_hole_low_poly.MeshQuadro = true;
    require(two_hole_low_poly.InitSurfaces()
                && two_hole_low_poly.InitEdges()
                && two_hole_low_poly.ReBuldMesh(2.0f),
            "Low Poly could not build a solid with two holes.");
    bool found_two_hole_face = false;
    for (int surface_index = 0;
         surface_index < two_hole_low_poly.GetNumSurfaces(); ++surface_index) {
        CSurfaceFace* surface = two_hole_low_poly.GetSurfaceFace(surface_index);
        if (!surface || surface->m_TypeMesh == REGULAR_MESH)
            continue;
        if (surface->GetPreparedPolylineCount() < 8)
            continue;
        require(surface->BuildFilledMeshWhithHoles(2.0f),
                "The UV patch builder rejected a face with two holes.");
        require(ActiveFaceEdgeComponentCount(*surface->pMesh3D) == 1,
                "Two-hole UV patches were not welded into one mesh.");
        require(surface->BuildFilledMeshWhithHoles(2.0f, true),
                "Mesh Quadro Hole SLX rejected a face with two holes.");
        bool slx_has_active_face = false;
        const std::vector<UV>& slx_uvs = surface->pMesh3D->GetUVs();
        for (const CMesh3D::Face& mesh_face : surface->pMesh3D->GetFaces()) {
            if (mesh_face.deleted)
                continue;
            slx_has_active_face = true;
            double center_u = 0.0;
            double center_v = 0.0;
            for (const MeshCorner& corner : mesh_face.corners) {
                require(corner.uv < slx_uvs.size(),
                        "Mesh Quadro Hole SLX produced an invalid UV index.");
                center_u += slx_uvs[corner.uv].u;
                center_v += slx_uvs[corner.uv].v;
            }
            const double inverse_corner_count =
                1.0 / static_cast<double>(mesh_face.corners.size());
            BRepClass_FaceClassifier classifier(
                TopoDS::Face(surface->m_Face),
                gp_Pnt2d(center_u * inverse_corner_count,
                         center_v * inverse_corner_count),
                1.0e-7, Standard_False);
            require(classifier.State() != TopAbs_OUT,
                    "Mesh Quadro Hole SLX retained a face inside a hole.");
        }
        require(slx_has_active_face,
                "Mesh Quadro Hole SLX returned an empty mesh.");
        found_two_hole_face = true;
    }
    require(found_two_hole_face,
            "The two-hole regression solid did not expose a two-hole face.");

    two_hole_low_poly.MeshQuadroHoleSLX = true;
    require(two_hole_low_poly.ReBuldMesh(2.0f),
            "The complete two-hole solid rejected the SLX rebuild.");
    std::vector<const CMesh3D*> slx_surface_meshes;
    for (int surface_index = 0;
         surface_index < two_hole_low_poly.GetNumSurfaces(); ++surface_index) {
        CSurfaceFace* surface = two_hole_low_poly.GetSurfaceFace(surface_index);
        if (surface && surface->pMesh3D)
            slx_surface_meshes.push_back(surface->pMesh3D);
    }
    std::unique_ptr<CMesh3D> welded_slx = CMesh3D::CreateWelded(
        slx_surface_meshes);
    require(welded_slx != nullptr,
            "The complete SLX solid could not be welded for seam validation.");
    std::map<std::pair<size_t, size_t>, int> welded_edge_use;
    for (const CMesh3D::Face& face : welded_slx->GetFaces()) {
        if (face.deleted)
            continue;
        for (size_t corner = 0; corner < face.corners.size(); ++corner) {
            const size_t first = face.corners[corner].v;
            const size_t second = face.corners[
                (corner + 1) % face.corners.size()].v;
            ++welded_edge_use[std::minmax(first, second)];
        }
    }
    for (const auto& edge : welded_edge_use) {
        require(edge.second == 2,
                "The welded SLX solid retained an open or non-manifold seam.");
    }

    TopoDS_Shape coarse_round_holes = BRepPrimAPI_MakeBox(
        80.0, 60.0, 8.0).Shape();
    for (const gp_Pnt& center : {gp_Pnt(28.0, 30.0, -1.0),
                                 gp_Pnt(52.0, 30.0, -1.0)}) {
        coarse_round_holes = BRepAlgoAPI_Cut(
            coarse_round_holes,
            BRepPrimAPI_MakeCylinder(
                gp_Ax2(center, gp_Dir(0.0, 0.0, 1.0)),
                5.0, 10.0).Shape()).Shape();
    }
    for (const float density : {0.70f, 0.55f, 0.50f, 0.45f, 0.30f}) {
        CSolid coarse_round_holes_low_poly(coarse_round_holes);
        coarse_round_holes_low_poly.MeshQuadro = true;
        require(coarse_round_holes_low_poly.InitSurfaces()
                    && coarse_round_holes_low_poly.InitEdges()
                    && coarse_round_holes_low_poly.ReBuldMesh(1.0f / density),
                "Low Poly failed for two round holes at coarse density.");
        bool verified_coarse_round_holes = false;
        for (int surface_index = 0;
             surface_index < coarse_round_holes_low_poly.GetNumSurfaces();
             ++surface_index) {
            CSurfaceFace* surface =
                coarse_round_holes_low_poly.GetSurfaceFace(surface_index);
			if (surface && BRepAdaptor_Surface(
					TopoDS::Face(surface->m_Face)).GetType()
					== GeomAbs_Cylinder) {
				for (const CMesh3D::Face& cylinder_face :
					 surface->pMesh3D->GetFaces()) {
					if (!cylinder_face.deleted)
						require(cylinder_face.corners.size() == 4,
							"A round-hole cylinder wall retained a triangle.");
				}
			}
            if (!surface || surface->m_TypeMesh == REGULAR_MESH
                || surface->GetPreparedPolylineCount() < 6
                || BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                    != GeomAbs_Plane) {
                continue;
            }
            const std::string coarse_builder_error =
                "The coarse two-round-hole UV patch builder failed at density "
                + std::to_string(density) + ".";
            const bool coarse_builder_ok =
                surface->BuildFilledMeshWhithHoles(1.0f / density);
            require(coarse_builder_ok, coarse_builder_error.c_str());
            require(ActiveFaceEdgeComponentCount(*surface->pMesh3D) == 1,
                    "Coarse round-hole UV patches were not welded into one mesh.");
            const std::vector<UV>& uvs = surface->pMesh3D->GetUVs();
            double mesh_area = 0.0;
            for (const CMesh3D::Face& mesh_face : surface->pMesh3D->GetFaces()) {
                if (mesh_face.deleted)
                    continue;
				require(mesh_face.corners.size() == 3
						|| mesh_face.corners.size() == 4,
						"Coarse density produced an unsupported polygon.");
                double face_u = 0.0;
                double face_v = 0.0;
                double twice_face_area = 0.0;
                for (size_t corner_index = 0;
                     corner_index < mesh_face.corners.size(); ++corner_index) {
                    const MeshCorner& corner = mesh_face.corners[corner_index];
                    const MeshCorner& next = mesh_face.corners[
                        (corner_index + 1) % mesh_face.corners.size()];
                    require(corner.uv < uvs.size() && next.uv < uvs.size(),
                            "Coarse density produced an invalid UV index.");
                    face_u += uvs[corner.uv].u;
                    face_v += uvs[corner.uv].v;
                    twice_face_area += uvs[corner.uv].u * uvs[next.uv].v
                        - uvs[next.uv].u * uvs[corner.uv].v;
                }
                mesh_area += std::fabs(twice_face_area) * 0.5;
                const double inverse_corner_count =
                    1.0 / static_cast<double>(mesh_face.corners.size());
                BRepClass_FaceClassifier classifier(
                    TopoDS::Face(surface->m_Face),
                    gp_Pnt2d(face_u * inverse_corner_count,
                             face_v * inverse_corner_count),
                    1.0e-6, Standard_False);
                require(classifier.State() != TopAbs_OUT,
                        "Coarse density produced a face crossing a hole.");
            }
            GProp_GProps face_properties;
            BRepGProp::SurfaceProperties(
                TopoDS::Face(surface->m_Face), face_properties);
            require(std::fabs(mesh_area - face_properties.Mass())
                        <= face_properties.Mass() * 0.02,
                    "Coarse density lost part of the planar hole surface.");
            if (density >= 0.50f) {
            const std::string coarse_slx_error =
                "SLX collar failed for two round holes at density "
                + std::to_string(density) + ".";
            require(surface->BuildFilledMeshWhithHoles(
                        1.0f / density, true), coarse_slx_error.c_str());
            const std::vector<UV>& slx_uvs = surface->pMesh3D->GetUVs();
            double slx_area = 0.0;
            std::map<std::pair<size_t, size_t>, int> slx_edge_use;
            for (const CMesh3D::Face& mesh_face : surface->pMesh3D->GetFaces()) {
                if (mesh_face.deleted)
                    continue;
                require(mesh_face.corners.size() == 3
                            || mesh_face.corners.size() == 4,
                        "Raw SLX produced an unsupported polygon.");
                double center_u = 0.0;
                double center_v = 0.0;
                double twice_area = 0.0;
                for (size_t corner_index = 0;
                     corner_index < mesh_face.corners.size(); ++corner_index) {
                    const MeshCorner& corner = mesh_face.corners[corner_index];
                    const MeshCorner& next = mesh_face.corners[
                        (corner_index + 1) % mesh_face.corners.size()];
                    require(corner.uv < slx_uvs.size()
                                && next.uv < slx_uvs.size(),
                            "Raw SLX produced an invalid UV index.");
                    center_u += slx_uvs[corner.uv].u;
                    center_v += slx_uvs[corner.uv].v;
                    twice_area += slx_uvs[corner.uv].u * slx_uvs[next.uv].v
                        - slx_uvs[next.uv].u * slx_uvs[corner.uv].v;
                    ++slx_edge_use[std::minmax(corner.v, next.v)];
                }
                slx_area += std::fabs(twice_area) * 0.5;
                const double inverse_count =
                    1.0 / static_cast<double>(mesh_face.corners.size());
                BRepClass_FaceClassifier classifier(
                    TopoDS::Face(surface->m_Face),
                    gp_Pnt2d(center_u * inverse_count,
                             center_v * inverse_count),
                    1.0e-6, Standard_False);
                const std::string outside_slx_face =
                    "Raw SLX retained a face inside a round hole at density "
                    + std::to_string(density) + ".";
                require(classifier.State() != TopAbs_OUT,
                        outside_slx_face.c_str());
            }
            const std::string slx_area_error =
                "Raw SLX changed the trimmed surface area at density "
                + std::to_string(density) + ": mesh="
                + std::to_string(slx_area) + ", face="
                + std::to_string(face_properties.Mass()) + ".";
            require(std::fabs(slx_area - face_properties.Mass())
                        <= face_properties.Mass() * 0.02,
                    slx_area_error.c_str());
            for (const auto& edge : slx_edge_use) {
                require(edge.second <= 2,
                        "Raw SLX produced a non-manifold edge.");
            }
            size_t collar_node_count = 0;
            for (const UV& uv : slx_uvs) {
                for (const std::pair<double, double>& center : {
                         std::pair{28.0, 30.0}, std::pair{52.0, 30.0}}) {
                    const double radius = std::hypot(
                        uv.u - center.first, uv.v - center.second);
                    if (radius > 5.2 && radius < 9.5) {
                        ++collar_node_count;
                        break;
                    }
                }
            }
            require(collar_node_count >= 8,
                    "SLX did not preserve the transition collar nodes.");
            }
            verified_coarse_round_holes = true;
        }
        require(verified_coarse_round_holes,
                "The coarse-density regression did not inspect the top face.");
    }

    // A skew angular hole used to expose the SLX collar's centroid-ray
    // assumption: several rails converged near one corner instead of following
    // the two incident sides. Keep this deliberately non-radial contour small
    // compared with the background cells so the transition collar is required.
    BRepBuilderAPI_MakePolygon angular_hole_polygon;
    for (const gp_Pnt& point : {
             gp_Pnt(33.0, 28.0, -1.0), gp_Pnt(37.0, 26.0, -1.0),
             gp_Pnt(41.0, 29.0, -1.0), gp_Pnt(40.0, 34.0, -1.0),
             gp_Pnt(35.0, 36.0, -1.0), gp_Pnt(32.0, 33.0, -1.0)}) {
        angular_hole_polygon.Add(point);
    }
    angular_hole_polygon.Close();
    const TopoDS_Shape angular_hole = BRepPrimAPI_MakePrism(
        BRepBuilderAPI_MakeFace(angular_hole_polygon.Wire()).Face(),
        gp_Vec(0.0, 0.0, 10.0)).Shape();
    TopoDS_Shape angular_hole_shape = BRepAlgoAPI_Cut(
        BRepPrimAPI_MakeBox(80.0, 60.0, 8.0).Shape(),
        angular_hole).Shape();
    CSolid angular_hole_low_poly(angular_hole_shape);
    angular_hole_low_poly.MeshQuadro = true;
    angular_hole_low_poly.MeshQuadroHoleSLX = true;
    require(angular_hole_low_poly.InitSurfaces()
                && angular_hole_low_poly.InitEdges()
                && angular_hole_low_poly.ReBuldMesh(2.0f),
            "SLX failed to rebuild the angular-hole regression solid.");
    bool verified_angular_hole = false;
    for (int surface_index = 0;
         surface_index < angular_hole_low_poly.GetNumSurfaces(); ++surface_index) {
        CSurfaceFace* surface = angular_hole_low_poly.GetSurfaceFace(surface_index);
        if (!surface || surface->m_TypeMesh == REGULAR_MESH
            || BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                != GeomAbs_Plane) {
            continue;
        }
        require(surface->BuildFilledMeshWhithHoles(2.0f, true),
                "SLX collar failed on a skew angular hole.");
        const std::vector<UV>& uvs = surface->pMesh3D->GetUVs();
        const std::vector<Vec3>& vertices = surface->pMesh3D->GetVertices();
        std::map<std::pair<size_t, size_t>, int> edge_use;
        double mesh_area = 0.0;
        size_t local_face_count = 0;
        for (const CMesh3D::Face& mesh_face : surface->pMesh3D->GetFaces()) {
            if (mesh_face.deleted)
                continue;
            double center_u = 0.0;
            double center_v = 0.0;
            double twice_area = 0.0;
            double minimum_edge = std::numeric_limits<double>::max();
            double maximum_edge = 0.0;
            for (size_t corner_index = 0;
                 corner_index < mesh_face.corners.size(); ++corner_index) {
                const MeshCorner& corner = mesh_face.corners[corner_index];
                const MeshCorner& next = mesh_face.corners[
                    (corner_index + 1) % mesh_face.corners.size()];
                require(corner.uv < uvs.size() && next.uv < uvs.size()
                            && corner.v < vertices.size() && next.v < vertices.size(),
                        "Angular SLX produced an invalid mesh index.");
                center_u += uvs[corner.uv].u;
                center_v += uvs[corner.uv].v;
                twice_area += uvs[corner.uv].u * uvs[next.uv].v
                    - uvs[next.uv].u * uvs[corner.uv].v;
                const Vec3 delta = vertices[next.v] - vertices[corner.v];
                const double edge_length = std::sqrt(
                    static_cast<double>(dot(delta, delta)));
                minimum_edge = std::min(minimum_edge, edge_length);
                maximum_edge = std::max(maximum_edge, edge_length);
                ++edge_use[std::minmax(corner.v, next.v)];
            }
            mesh_area += std::fabs(twice_area) * 0.5;
            const double inverse_count =
                1.0 / static_cast<double>(mesh_face.corners.size());
            center_u *= inverse_count;
            center_v *= inverse_count;
            BRepClass_FaceClassifier classifier(
                TopoDS::Face(surface->m_Face), gp_Pnt2d(center_u, center_v),
                1.0e-6, Standard_False);
            require(classifier.State() != TopAbs_OUT,
                    "Angular SLX retained a face inside the hole.");
            if (center_u > 24.0 && center_u < 49.0
                && center_v > 18.0 && center_v < 44.0) {
                ++local_face_count;
                require(minimum_edge > 1.0e-8
                            && maximum_edge / minimum_edge < 20.0,
                        "Angular SLX reproduced a bunched collar rail.");
            }
        }
        for (const auto& edge : edge_use) {
            require(edge.second <= 2,
                    "Angular SLX produced a non-manifold edge.");
        }
        GProp_GProps angular_face_properties;
        BRepGProp::SurfaceProperties(
            TopoDS::Face(surface->m_Face), angular_face_properties);
        require(std::fabs(mesh_area - angular_face_properties.Mass())
                    <= angular_face_properties.Mass() * 0.02,
                "Angular SLX changed the trimmed surface area.");
        require(local_face_count >= 6,
                "Angular SLX regression did not inspect the collar area.");
        verified_angular_hole = true;
    }
    require(verified_angular_hole,
            "The angular-hole regression did not inspect a trimmed plane.");

    TopoDS_Shape edge_hole_shape = BRepAlgoAPI_Cut(
        BRepPrimAPI_MakeBox(80.0, 60.0, 8.0).Shape(),
        BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(12.0, 30.0, -1.0), gp_Dir(0.0, 0.0, 1.0)),
            5.0, 10.0).Shape()).Shape();
    CSolid edge_hole_low_poly(edge_hole_shape);
    edge_hole_low_poly.MeshQuadro = true;
    require(edge_hole_low_poly.InitSurfaces()
                && edge_hole_low_poly.InitEdges()
                && edge_hole_low_poly.ReBuldMesh(1.0f / 0.40f),
            "Low Poly failed for a small hole beside the outer boundary.");
    bool verified_edge_hole = false;
    for (int surface_index = 0;
         surface_index < edge_hole_low_poly.GetNumSurfaces(); ++surface_index) {
        CSurfaceFace* surface = edge_hole_low_poly.GetSurfaceFace(surface_index);
        if (!surface || surface->m_TypeMesh == REGULAR_MESH
            || surface->GetPreparedPolylineCount() < 5
            || BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                != GeomAbs_Plane) {
            continue;
        }
        require(surface->BuildFilledMeshWhithHoles(1.0f / 0.40f),
                "The collar failed beside the outer boundary.");
        const std::vector<UV>& uvs = surface->pMesh3D->GetUVs();
        double mesh_area = 0.0;
        for (const CMesh3D::Face& mesh_face : surface->pMesh3D->GetFaces()) {
            if (mesh_face.deleted)
                continue;
			require(mesh_face.corners.size() == 3
					|| mesh_face.corners.size() == 4,
					"The edge-hole collar produced an unsupported polygon.");
            double twice_area = 0.0;
            double center_u = 0.0;
            double center_v = 0.0;
            for (size_t i = 0; i < mesh_face.corners.size(); ++i) {
                const UV& a = uvs[mesh_face.corners[i].uv];
				const UV& b = uvs[mesh_face.corners[
					(i + 1) % mesh_face.corners.size()].uv];
                twice_area += a.u * b.v - b.u * a.v;
                center_u += a.u;
                center_v += a.v;
            }
            mesh_area += std::fabs(twice_area) * 0.5;
			const double inverse_corner_count =
				1.0 / static_cast<double>(mesh_face.corners.size());
            BRepClass_FaceClassifier classifier(
                TopoDS::Face(surface->m_Face),
				gp_Pnt2d(center_u * inverse_corner_count,
					center_v * inverse_corner_count),
                1.0e-6, Standard_False);
            require(classifier.State() != TopAbs_OUT,
                    "The edge-hole quad centre escaped the face.");
        }
        GProp_GProps face_properties;
        BRepGProp::SurfaceProperties(
            TopoDS::Face(surface->m_Face), face_properties);
        require(std::fabs(mesh_area - face_properties.Mass())
                    <= face_properties.Mass() * 0.02,
                "The edge-hole mesh lost part of the planar surface.");
        verified_edge_hole = true;
    }
    require(verified_edge_hole,
            "The edge-hole regression did not inspect the top face.");

    TopoDS_Shape many_hole_shape = BRepPrimAPI_MakeBox(
        100.0, 70.0, 8.0).Shape();
    for (const gp_Pnt& center : {
             gp_Pnt(18.0, 54.0, -1.0), gp_Pnt(39.0, 54.0, -1.0),
             gp_Pnt(14.0, 36.0, -1.0), gp_Pnt(31.0, 37.0, -1.0),
             gp_Pnt(24.0, 20.0, -1.0), gp_Pnt(65.0, 45.0, -1.0),
             gp_Pnt(55.0, 20.0, -1.0), gp_Pnt(76.0, 20.0, -1.0)}) {
        many_hole_shape = BRepAlgoAPI_Cut(
            many_hole_shape,
            BRepPrimAPI_MakeCylinder(
                gp_Ax2(center, gp_Dir(0.0, 0.0, 1.0)),
                4.5, 10.0).Shape()).Shape();
    }
    for (const float density : {0.80f, 0.90f}) {
        CSolid many_hole_low_poly(many_hole_shape);
        many_hole_low_poly.MeshQuadro = true;
        require(many_hole_low_poly.InitSurfaces()
                    && many_hole_low_poly.InitEdges()
                    && many_hole_low_poly.ReBuldMesh(1.0f / density),
                "Low Poly failed for the complex eight-hole topology.");
        bool verified_many_holes = false;
        for (int surface_index = 0;
             surface_index < many_hole_low_poly.GetNumSurfaces();
             ++surface_index) {
            CSurfaceFace* surface =
                many_hole_low_poly.GetSurfaceFace(surface_index);
            if (!surface || surface->m_TypeMesh == REGULAR_MESH
                || surface->GetPreparedPolylineCount() < 12
                || BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                    != GeomAbs_Plane) {
                continue;
            }
            for (const CMesh3D::Face& final_face : surface->pMesh3D->GetFaces()) {
                if (!final_face.deleted)
					require(final_face.corners.size() == 3
							|| final_face.corners.size() == 4,
							"Final seam synchronization produced an unsupported polygon.");
            }
            bool verified_final_seam = false;
            for (int trimmed_edge = 0;
                 trimmed_edge < surface->GetPreparedPolylineCount();
                 ++trimmed_edge) {
                TopoDS_Edge trimmed_topo_edge;
                if (!surface->GetPreparedTopoEdge(
                        trimmed_edge, trimmed_topo_edge)) {
                    continue;
                }
                for (int donor_index = 0;
                     donor_index < many_hole_low_poly.GetNumSurfaces();
                     ++donor_index) {
                    CSurfaceFace* donor =
                        many_hole_low_poly.GetSurfaceFace(donor_index);
                    if (!donor || donor->m_TypeMesh != REGULAR_MESH)
                        continue;
                    for (int donor_edge = 0;
                         donor_edge < donor->GetPreparedPolylineCount();
                         ++donor_edge) {
                        TopoDS_Edge donor_topo_edge;
                        std::vector<CPoint3d> donor_points;
                        if (!donor->GetPreparedTopoEdge(
                                donor_edge, donor_topo_edge)
                            || !donor_topo_edge.IsSame(trimmed_topo_edge)
                            || !donor->GetRegularMeshBoundaryPoints(
                                donor_edge, donor_points)) {
                            continue;
                        }
                        for (const CPoint3d& point : donor_points) {
                            bool found = false;
                            for (const Vec3& vertex :
                                 surface->pMesh3D->GetVertices()) {
                                const double dx = vertex.x - point.x;
                                const double dy = vertex.y - point.y;
                                const double dz = vertex.z - point.z;
                                if (dx * dx + dy * dy + dz * dz < 1.0e-8) {
                                    found = true;
                                    break;
                                }
                            }
                            require(found,
                                    "Trimmed final mesh omitted a regular-neighbour seam node.");
                        }
                        verified_final_seam = true;
                    }
                }
            }
            require(verified_final_seam,
                    "The complex topology did not verify its final outer seam.");
            require(surface->BuildFilledMeshWhithHoles(1.0f / density),
                    "The checked quad builder rejected the eight-hole face.");
            const std::vector<UV>& uvs = surface->pMesh3D->GetUVs();
            double mesh_area = 0.0;
            for (const CMesh3D::Face& mesh_face :
                 surface->pMesh3D->GetFaces()) {
                if (mesh_face.deleted)
                    continue;
				require(mesh_face.corners.size() == 3
						|| mesh_face.corners.size() == 4,
						"The complex topology produced an unsupported polygon.");
                double twice_area = 0.0;
                double center_u = 0.0;
                double center_v = 0.0;
				for (size_t i = 0; i < mesh_face.corners.size(); ++i) {
                    const UV& a = uvs[mesh_face.corners[i].uv];
					const UV& b = uvs[mesh_face.corners[
						(i + 1) % mesh_face.corners.size()].uv];
                    twice_area += a.u * b.v - b.u * a.v;
                    center_u += a.u;
                    center_v += a.v;
                }
                mesh_area += std::fabs(twice_area) * 0.5;
				const double inverse_corner_count =
					1.0 / static_cast<double>(mesh_face.corners.size());
                BRepClass_FaceClassifier classifier(
                    TopoDS::Face(surface->m_Face),
					gp_Pnt2d(center_u * inverse_corner_count,
						center_v * inverse_corner_count),
                    1.0e-6, Standard_False);
                require(classifier.State() != TopAbs_OUT,
                        "A complex-topology quad centre escaped the face.");
            }
            GProp_GProps face_properties;
            BRepGProp::SurfaceProperties(
                TopoDS::Face(surface->m_Face), face_properties);
            require(std::fabs(mesh_area - face_properties.Mass())
                        <= face_properties.Mass() * 0.02,
                    "The complex topology lost a surface sector.");
            verified_many_holes = true;
        }
        require(verified_many_holes,
                "The eight-hole regression did not inspect the top face.");
    }

    TopoDS_Shape narrow_outer_bridge = BRepAlgoAPI_Cut(
        BRepPrimAPI_MakeBox(60.0, 50.0, 8.0).Shape(),
        BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(14.0, 25.0, -1.0), gp_Dir(0.0, 0.0, 1.0)),
            12.0, 10.0).Shape()).Shape();
    CSolid narrow_bridge_low_poly(narrow_outer_bridge);
    narrow_bridge_low_poly.MeshQuadro = true;
    require(narrow_bridge_low_poly.InitSurfaces()
                && narrow_bridge_low_poly.InitEdges()
                && narrow_bridge_low_poly.ReBuldMesh(1.4f),
            "Low Poly could not preserve a narrow bridge beside a round hole.");
    bool verified_narrow_round_bridge = false;
    for (int surface_index = 0;
         surface_index < narrow_bridge_low_poly.GetNumSurfaces(); ++surface_index) {
        CSurfaceFace* surface = narrow_bridge_low_poly.GetSurfaceFace(surface_index);
        if (!surface || surface->GetPreparedPolylineCount() < 2
            || BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                != GeomAbs_Plane) {
            continue;
        }
        require(surface->BuildFilledMeshWhithHoles(1.4f),
                "The UV island builder removed the narrow round-hole bridge.");
        const std::vector<UV>& uvs = surface->pMesh3D->GetUVs();
        for (const CMesh3D::Face& mesh_face : surface->pMesh3D->GetFaces()) {
            if (mesh_face.deleted)
                continue;
			require(mesh_face.corners.size() == 3
					|| mesh_face.corners.size() == 4,
					"The narrow round-hole bridge produced an unsupported polygon.");
            double u = 0.0;
            double v = 0.0;
            for (const MeshCorner& corner : mesh_face.corners) {
                u += uvs[corner.uv].u;
                v += uvs[corner.uv].v;
            }
            const double inverse_count = 1.0 / mesh_face.corners.size();
            BRepClass_FaceClassifier classifier(
                TopoDS::Face(surface->m_Face),
                gp_Pnt2d(u * inverse_count, v * inverse_count),
                1.0e-5, Standard_False);
            require(classifier.State() != TopAbs_OUT,
                    "A quad crossed the narrow round-hole bridge.");
        }
        verified_narrow_round_bridge = true;
    }
    require(verified_narrow_round_bridge,
            "The narrow round-hole solid did not expose its planar island face.");

    TopoDS_Shape fillet_base = BRepPrimAPI_MakeBox(80.0, 50.0, 30.0).Shape();
    BRepFilletAPI_MakeFillet fillet_builder(fillet_base);
    for (TopExp_Explorer explorer(fillet_base, TopAbs_EDGE);
         explorer.More(); explorer.Next()) {
        fillet_builder.Add(6.0, TopoDS::Edge(explorer.Current()));
    }
    fillet_builder.Build();
    require(fillet_builder.IsDone(),
            "OCCT could not construct the small-radius regression solid.");
    TopoDS_Shape fillet_shape = fillet_builder.Shape();
    CSolid fillet_low_poly(fillet_shape);
    fillet_low_poly.MeshQuadro = true;
    require(fillet_low_poly.InitSurfaces() && fillet_low_poly.InitEdges()
                // Density 0.50 in the Low Poly dialog passes Deflection 2.0.
                && fillet_low_poly.ReBuldMesh(2.0f),
            "Low Poly failed on small-radius transition surfaces.");
    bool found_radius_surface = false;
    for (int surface_index = 0;
         surface_index < fillet_low_poly.GetNumSurfaces(); ++surface_index) {
        const CSurfaceFace* surface = fillet_low_poly.GetSurfaceFace(surface_index);
        require(surface && surface->pMesh3D,
                "Small-radius Low Poly skipped a surface.");
        const GeomAbs_SurfaceType type = BRepAdaptor_Surface(
            TopoDS::Face(surface->m_Face)).GetType();
        found_radius_surface = found_radius_surface
            || type == GeomAbs_Cylinder || type == GeomAbs_Sphere
            || type == GeomAbs_Torus;
        bool has_active_face = false;
        for (const CMesh3D::Face& mesh_face : surface->pMesh3D->GetFaces()) {
            if (mesh_face.deleted)
                continue;
            has_active_face = true;
            require(mesh_face.corners.size() == 4,
                    "Small-radius Low Poly fell back to a triangle fan.");
            if (type == GeomAbs_Sphere) {
                const std::vector<Vec3>& vertices =
                    surface->pMesh3D->GetVertices();
                const Vec3& first = vertices[mesh_face.corners[0].v];
                double area = 0.0;
                for (size_t corner = 1;
                     corner + 1 < mesh_face.corners.size(); ++corner) {
                    const Vec3& second =
                        vertices[mesh_face.corners[corner].v];
                    const Vec3& third =
                        vertices[mesh_face.corners[corner + 1].v];
                    const Vec3 normal = cross(second - first, third - first);
                    area += std::sqrt(static_cast<double>(dot(normal, normal)))
                        * 0.5;
                }
                require(area > 1.0e-8,
                        "A rounded box corner retained a collapsed pole quad.");
            }
        }
        require(has_active_face,
                "Small-radius Low Poly produced an empty surface.");
        if (type == GeomAbs_Sphere
            && surface->m_TypeMesh == REGULAR_MESH
            && surface->GetPreparedPolylineCount() == 3) {
            const size_t expected_quads = static_cast<size_t>(
                (surface->m_QtyU - 1) * (surface->m_QtyV - 1));
            require(surface->pMesh3D->GetFaces().size() == expected_quads,
                    "A spherical box corner no longer follows the structured 3DCoat UV grid.");

            for (int edge_index = 0;
                 edge_index < surface->GetPreparedPolylineCount();
                 ++edge_index) {
                std::vector<CPoint3d> prepared_points;
                std::vector<CPoint3d> boundary_points;
                require(surface->GetPreparedPolylinePoints(
                            edge_index, prepared_points)
                            && surface->GetRegularMeshBoundaryPoints(
                                edge_index, boundary_points),
                        "A spherical corner edge has no mesh boundary.");
                const auto polyline_length = [](const std::vector<CPoint3d>& points) {
                    double length = 0.0;
                    for (size_t point = 1; point < points.size(); ++point) {
                        const double dx = points[point].x - points[point - 1].x;
                        const double dy = points[point].y - points[point - 1].y;
                        const double dz = points[point].z - points[point - 1].z;
                        length += std::sqrt(dx * dx + dy * dy + dz * dz);
                    }
                    return length;
                };
                const double prepared_length = polyline_length(prepared_points);
                const double boundary_length = polyline_length(boundary_points);
                require(prepared_length > 1.0e-6
                            && boundary_length > prepared_length * 0.8,
                        "A spherical UV pole was mistaken for a rounded seam edge.");
            }
        }
    }
    require(found_radius_surface,
            "The fillet regression did not expose a radius surface.");

    // Rounded corners are the ambiguous case for seam synchronization: two
    // tangent trimming edges can be geometrically closer than the actual
    // shared edge.  Every prepared node of a topologically shared edge must
    // survive in both finished surface meshes.
    size_t verified_fillet_seams = 0;
    for (int first_surface_index = 0;
         first_surface_index < fillet_low_poly.GetNumSurfaces();
         ++first_surface_index) {
        const CSurfaceFace* first_surface =
            fillet_low_poly.GetSurfaceFace(first_surface_index);
        if (!first_surface || !first_surface->pMesh3D)
            continue;
        for (int first_edge_index = 0;
             first_edge_index < first_surface->GetPreparedPolylineCount();
             ++first_edge_index) {
            TopoDS_Edge first_edge;
            std::vector<CPoint3d> seam_points;
            if (!first_surface->GetPreparedTopoEdge(first_edge_index, first_edge)
                || !first_surface->GetPreparedPolylinePoints(
                    first_edge_index, seam_points)) {
                continue;
            }
            for (int second_surface_index = first_surface_index + 1;
                 second_surface_index < fillet_low_poly.GetNumSurfaces();
                 ++second_surface_index) {
                const CSurfaceFace* second_surface =
                    fillet_low_poly.GetSurfaceFace(second_surface_index);
                if (!second_surface || !second_surface->pMesh3D)
                    continue;
                bool shares_edge = false;
                for (int second_edge_index = 0;
                     second_edge_index < second_surface->GetPreparedPolylineCount();
                     ++second_edge_index) {
                    TopoDS_Edge second_edge;
                    if (second_surface->GetPreparedTopoEdge(
                            second_edge_index, second_edge)
                        && second_edge.IsSame(first_edge)) {
                        shares_edge = true;
                        break;
                    }
                }
                if (!shares_edge)
                    continue;
                const auto contains_point = [](const CMesh3D& mesh,
                                               const CPoint3d& point) {
                    return std::any_of(
                        mesh.GetVertices().begin(), mesh.GetVertices().end(),
                        [&point](Vec3 vertex) {
                            const double dx = vertex.x - point.x;
                            const double dy = vertex.y - point.y;
                            const double dz = vertex.z - point.z;
                            return dx * dx + dy * dy + dz * dz < 1.0e-6;
                        });
                };
                const auto contains_edge = [](const CMesh3D& mesh,
                                              const CPoint3d& first,
                                              const CPoint3d& second) {
                    const auto matches = [](Vec3 vertex, const CPoint3d& point) {
                        const double dx = vertex.x - point.x;
                        const double dy = vertex.y - point.y;
                        const double dz = vertex.z - point.z;
                        return dx * dx + dy * dy + dz * dz < 1.0e-6;
                    };
                    const std::vector<Vec3>& vertices = mesh.GetVertices();
                    for (const CMesh3D::Face& face : mesh.GetFaces()) {
                        if (face.deleted)
                            continue;
                        for (size_t corner = 0; corner < face.corners.size(); ++corner) {
                            const size_t next = (corner + 1) % face.corners.size();
                            const Vec3 a = vertices[face.corners[corner].v];
                            const Vec3 b = vertices[face.corners[next].v];
                            if ((matches(a, first) && matches(b, second))
                                || (matches(a, second) && matches(b, first))) {
                                return true;
                            }
                        }
                    }
                    return false;
                };
                for (const CPoint3d& point : seam_points) {
                    require(contains_point(*first_surface->pMesh3D, point)
                                && contains_point(*second_surface->pMesh3D, point),
                            "A rounded Low Poly seam lost a shared boundary node.");
                }
                for (size_t point = 1; point < seam_points.size(); ++point) {
                    require(contains_edge(*first_surface->pMesh3D,
                                seam_points[point - 1], seam_points[point])
                                && contains_edge(*second_surface->pMesh3D,
                                    seam_points[point - 1], seam_points[point]),
                            "A rounded Low Poly seam node exists but is not connected as a mesh edge.");
                }
                ++verified_fillet_seams;
            }
        }
    }
    require(verified_fillet_seams >= 4,
            "The fillet regression did not verify its rounded seams.");

    const auto sphere_corner_resolution = [&fillet_shape](float density) {
        CSolid solid(fillet_shape);
        solid.MeshQuadro = true;
        require(solid.InitSurfaces() && solid.InitEdges()
                    && solid.ReBuldMesh(1.0f / density),
                "Could not rebuild the rounded box at the requested density.");
        int resolution = 0;
        for (int surface_index = 0;
             surface_index < solid.GetNumSurfaces(); ++surface_index) {
            const CSurfaceFace* surface = solid.GetSurfaceFace(surface_index);
            if (!surface || BRepAdaptor_Surface(
                    TopoDS::Face(surface->m_Face)).GetType()
                    != GeomAbs_Sphere) {
                continue;
            }
            resolution = std::max(
                resolution, std::max(surface->m_QtyU, surface->m_QtyV));
        }
        return resolution;
    };
    const int coarse_corner_resolution = sphere_corner_resolution(0.10f);
    const int fine_corner_resolution = sphere_corner_resolution(1.85f);
    require(coarse_corner_resolution >= 3
                && fine_corner_resolution > coarse_corner_resolution,
            "Mesh Quadro corner resolution is pinned across Density values.");

    TopoDS_Shape blind_outer = BRepPrimAPI_MakeCylinder(30.0, 50.0).Shape();
    TopoDS_Shape blind_cutter = BRepPrimAPI_MakeCylinder(
        gp_Ax2(gp_Pnt(0.0, 0.0, 35.0), gp_Dir(0.0, 0.0, 1.0)),
        12.0, 20.0).Shape();
    TopoDS_Shape blind_shape = BRepAlgoAPI_Cut(blind_outer, blind_cutter).Shape();
    CSolid blind_cylinder(blind_shape);
    blind_cylinder.MeshQuadro = true;
    require(blind_cylinder.InitSurfaces()
                && blind_cylinder.InitEdges()
                && blind_cylinder.ReBuldMesh(2.0f),
            "Low Poly could not build a cylinder with a blind hole.");
    for (int surface_index = 0;
         surface_index < blind_cylinder.GetNumSurfaces(); ++surface_index) {
        CSurfaceFace* surface = blind_cylinder.GetSurfaceFace(surface_index);
        require(surface && surface->pMesh3D,
                "Blind-hole cylinder skipped a surface.");
        if (BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
            != GeomAbs_Plane) {
            continue;
        }
        const std::vector<UV>& uvs = surface->pMesh3D->GetUVs();
        for (const CMesh3D::Face& mesh_face : surface->pMesh3D->GetFaces()) {
            if (mesh_face.deleted || mesh_face.corners.size() < 3)
                continue;
            double u = 0.0;
            double v = 0.0;
            for (const MeshCorner& corner : mesh_face.corners) {
                require(corner.uv < uvs.size(),
                        "Blind-hole cylinder contains an invalid UV index.");
                u += uvs[corner.uv].u;
                v += uvs[corner.uv].v;
            }
            u /= static_cast<double>(mesh_face.corners.size());
            v /= static_cast<double>(mesh_face.corners.size());
            BRepClass_FaceClassifier classifier(
                TopoDS::Face(surface->m_Face), gp_Pnt2d(u, v),
                1.0e-7, Standard_False);
            require(classifier.State() != TopAbs_OUT,
                    "Blind-hole UV patch mesh escaped the surface boundary.");
            for (const MeshCorner& corner : mesh_face.corners) {
                const UV& sample = uvs[corner.uv];
                BRepClass_FaceClassifier boundary_classifier(
                    TopoDS::Face(surface->m_Face),
                    gp_Pnt2d(sample.u, sample.v), 1.0e-4, Standard_False);
                require(boundary_classifier.State() != TopAbs_OUT,
                        "Blind-hole quad has a vertex outside the surface boundary.");
            }
        }
    }

    TopoDS_Shape through_shape = BRepAlgoAPI_Cut(
        BRepPrimAPI_MakeCylinder(30.0, 50.0).Shape(),
        BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(0.0, 0.0, -5.0), gp_Dir(0.0, 0.0, 1.0)),
            12.0, 60.0).Shape()).Shape();
    CSolid through_cylinder(through_shape);
    through_cylinder.MeshQuadro = true;
    require(through_cylinder.InitSurfaces() && through_cylinder.InitEdges()
                && through_cylinder.ReBuldMesh(2.0f),
            "Could not initialize the through-hole cylinder.");
    bool found_annular_cylinder_face = false;
    bool found_full_circle_boundary = false;
    for (int surface_index = 0;
         surface_index < through_cylinder.GetNumSurfaces(); ++surface_index) {
        CSurfaceFace* surface = through_cylinder.GetSurfaceFace(surface_index);
        if (surface) {
            for (int edge_index = 0;
                 edge_index < surface->GetPreparedPolylineCount(); ++edge_index) {
                TopoDS_Edge edge;
                if (!surface->GetPreparedTopoEdge(edge_index, edge))
                    continue;
                BRepAdaptor_Curve curve(edge);
                if (curve.GetType() == GeomAbs_Circle
                    && curve.LastParameter() - curve.FirstParameter() > 6.0) {
                    require(surface->GetPreparedPolylinePointCount(edge_index) >= 21,
                            "Coarse Low Poly reduced a circular boundary to fewer than 20 sides.");
                    found_full_circle_boundary = true;
                }
            }
        }
        if (!surface || surface->GetPreparedPolylineCount() < 2
            || BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                != GeomAbs_Plane)
            continue;
        require(surface->BuildFilledMeshWhithHoles(2.0f),
                "The UV patch builder rejected an annular cylinder face.");
        const std::vector<UV>& annular_uvs = surface->pMesh3D->GetUVs();
        for (const CMesh3D::Face& mesh_face : surface->pMesh3D->GetFaces()) {
            if (mesh_face.deleted || mesh_face.corners.size() < 3)
                continue;
            double u = 0.0;
            double v = 0.0;
            for (const MeshCorner& corner : mesh_face.corners) {
                u += annular_uvs[corner.uv].u;
                v += annular_uvs[corner.uv].v;
            }
            const double inverse_count = 1.0 / mesh_face.corners.size();
            BRepClass_FaceClassifier classifier(
                TopoDS::Face(surface->m_Face),
                gp_Pnt2d(u * inverse_count, v * inverse_count),
                1.0e-4, Standard_False);
            require(classifier.State() != TopAbs_OUT,
                    "Annular cylinder quad filled the central hole.");
        }
        found_annular_cylinder_face = true;
    }
    require(found_annular_cylinder_face,
            "The through-hole cylinder did not expose an annular face.");
    require(found_full_circle_boundary,
            "The through-hole regression did not inspect a circular boundary.");

	// Two unlike blind holes with top chamfers reproduce the small left-hand
	// cylinder, its circular floor, and the concentric bevel rings from the
	// reported Solid Box.
	TopoDS_Shape chamfered_holes_shape = BRepPrimAPI_MakeBox(
		80.0, 60.0, 12.0).Shape();
	for (const std::array<double, 3>& hole_spec : {
			 std::array<double, 3>{24.0, 30.0, 5.0},
			 std::array<double, 3>{56.0, 30.0, 9.0}}) {
		chamfered_holes_shape = BRepAlgoAPI_Cut(
			chamfered_holes_shape,
			BRepPrimAPI_MakeCylinder(
				gp_Ax2(gp_Pnt(hole_spec[0], hole_spec[1], 3.0),
					gp_Dir(0.0, 0.0, 1.0)), hole_spec[2], 10.0).Shape()).Shape();
	}
	BRepFilletAPI_MakeChamfer hole_chamfers(chamfered_holes_shape);
	for (TopExp_Explorer edge(chamfered_holes_shape, TopAbs_EDGE);
		 edge.More(); edge.Next()) {
		const TopoDS_Edge top_edge = TopoDS::Edge(edge.Current());
		try {
			BRepAdaptor_Curve curve(top_edge);
			if (curve.GetType() == GeomAbs_Circle
				&& std::fabs(curve.Circle().Location().Z() - 12.0) < 1.0e-5) {
				hole_chamfers.Add(1.5, top_edge);
			}
		} catch (const Standard_Failure&) {
		}
	}
	hole_chamfers.Build();
	require(hole_chamfers.IsDone() && !hole_chamfers.Shape().IsNull(),
		"Could not create the two unlike chamfered holes regression.");
	TopoDS_Shape chamfered_holes_result = hole_chamfers.Shape();
	CSolid chamfered_holes(chamfered_holes_result);
	chamfered_holes.MeshQuadro = true;
	chamfered_holes.MeshQuadroHoleSLX = true;
	require(chamfered_holes.InitSurfaces() && chamfered_holes.InitEdges()
			&& chamfered_holes.ReBuldMesh(2.0f),
		"Low Poly failed for two unlike chamfered holes.");
	bool found_chamfer = false;
	bool found_small_cylinder = false;
	int circular_floor_count = 0;
	for (int surface_index = 0;
		 surface_index < chamfered_holes.GetNumSurfaces(); ++surface_index) {
		const CSurfaceFace* surface =
			chamfered_holes.GetSurfaceFace(surface_index);
		if (!surface || !surface->pMesh3D)
			continue;
		const GeomAbs_SurfaceType type = BRepAdaptor_Surface(
			TopoDS::Face(surface->m_Face)).GetType();
		found_chamfer = found_chamfer || type == GeomAbs_Cone;
		if (type == GeomAbs_Cylinder) {
			const double radius = BRepAdaptor_Surface(
				TopoDS::Face(surface->m_Face)).Cylinder().Radius();
			found_small_cylinder = found_small_cylinder
				|| std::fabs(radius - 5.0) < 1.0e-5;
		}
		bool circular_floor = false;
		if (type == GeomAbs_Plane
			&& surface->GetPreparedPolylineCount() == 1) {
			TopoDS_Edge floor_edge;
			if (surface->GetPreparedTopoEdge(0, floor_edge)) {
				BRepAdaptor_Curve floor_curve(floor_edge);
				circular_floor = floor_curve.GetType() == GeomAbs_Circle;
			}
		}
		if (circular_floor)
			++circular_floor_count;
		if (type != GeomAbs_Cone && type != GeomAbs_Cylinder
			&& !circular_floor) {
			continue;
		}
		for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
			if (!face.deleted) {
				require(face.corners.size() == 4,
					"A chamfer, cylinder, or circular floor retained a triangle.");
				std::vector<Vec3> unique_points;
				for (const MeshCorner& corner : face.corners) {
					require(corner.v < surface->pMesh3D->GetVertices().size(),
						"A chamfered-hole face has an invalid vertex.");
					const Vec3 point = surface->pMesh3D->GetVertices()[corner.v];
					const bool duplicate = std::any_of(
						unique_points.begin(), unique_points.end(),
						[&](Vec3 existing) {
							const Vec3 delta = existing - point;
							return dot(delta, delta) < 1.0e-10f;
						});
					if (!duplicate)
						unique_points.push_back(point);
				}
				require(unique_points.size() == 4,
					"A chamfered-hole quad collapsed into a visible wedge.");
			}
		}
	}
	require(found_chamfer && found_small_cylinder && circular_floor_count == 2,
		"The chamfered-hole regression missed its cone, cylinder, or floor.");
	std::vector<const CMesh3D*> chamfered_surface_meshes;
	for (int surface_index = 0;
		 surface_index < chamfered_holes.GetNumSurfaces(); ++surface_index) {
		const CSurfaceFace* surface = chamfered_holes.GetSurfaceFace(surface_index);
		if (surface && surface->pMesh3D)
			chamfered_surface_meshes.push_back(surface->pMesh3D);
	}
	std::unique_ptr<CMesh3D> welded_chamfered = CMesh3D::CreateWelded(
		chamfered_surface_meshes);
	require(welded_chamfered != nullptr,
		"The chamfered-hole surface meshes could not be welded.");
	std::map<std::pair<size_t, size_t>, int> chamfered_edge_use;
	for (const CMesh3D::Face& face : welded_chamfered->GetFaces()) {
		if (face.deleted)
			continue;
		for (size_t corner = 0; corner < face.corners.size(); ++corner) {
			const size_t first = face.corners[corner].v;
			const size_t second = face.corners[
				(corner + 1) % face.corners.size()].v;
			++chamfered_edge_use[std::minmax(first, second)];
		}
	}
	for (const auto& edge : chamfered_edge_use) {
		require(edge.second == 2,
			"A chamfer or small blind cylinder retained an open mesh seam.");
	}

    CSolid coarse_low_poly(perforated_box);
    coarse_low_poly.MeshQuadro = true;
    require(coarse_low_poly.InitSurfaces()
                && coarse_low_poly.InitEdges()
                && coarse_low_poly.ReBuldMesh(4.0f),
            "Coarse Low Poly quad mesh was not built.");
    size_t coarse_regular_face_count = 0;
    for (int surface_index = 0;
         surface_index < coarse_low_poly.GetNumSurfaces(); ++surface_index) {
        const CSurfaceFace* surface =
            coarse_low_poly.GetSurfaceFace(surface_index);
        if (!surface || !surface->pMesh3D
            || surface->m_TypeMesh != REGULAR_MESH) {
            continue;
        }
        for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
            if (!face.deleted)
                ++coarse_regular_face_count;
        }
    }
    require(fine_regular_face_count > coarse_regular_face_count,
            "Low Poly Density no longer controls the surface face count.");

    ToolRegistry hole_tools;
    CAlfaDoc hole_document;
    const ToolDefinition* box_definition = hole_tools.Find("SolidBox");
    const ToolDefinition* hole_definition = hole_tools.Find("SolidHole");
    require(box_definition && hole_definition,
            "Hole or Solid Box tool is not registered.");
    ActiveParametricObject hole_box = hole_tools.CreateParametricObject(
        "SolidBox", hole_document, box_definition->defaults);
    auto* hole_body = hole_box.object_index < hole_document.GetObjects().size()
        ? dynamic_cast<CSolid*>(
            hole_document.GetObjects()[hole_box.object_index].get()) : nullptr;
    require(hole_body != nullptr, "Hole regression box was not created.");
    hole_document.EnsureObjectId(*hole_body);
    Vec3 hole_min{};
    Vec3 hole_max{};
    require(hole_body->GetBounds(hole_min, hole_max),
            "Hole regression box has no bounds.");
    std::vector<ToolParameter> hole_parameters = hole_definition->defaults;
    const auto set_hole_parameter = [&](const char* id, double value) {
        for (ToolParameter& parameter : hole_parameters) {
            if (parameter.id == id) {
                parameter.value = value;
                return;
            }
        }
    };
    set_hole_parameter("diameter", 10.0);
    set_hole_parameter("hole_type", 0.0);
    set_hole_parameter("hole.center.x", (hole_min.x + hole_max.x) * 0.5);
    set_hole_parameter("hole.center.y", (hole_min.y + hole_max.y) * 0.5);
    set_hole_parameter("hole.center.z", hole_max.z);
    set_hole_parameter("hole.normal.x", 0.0);
    set_hole_parameter("hole.normal.y", 0.0);
    set_hole_parameter("hole.normal.z", 1.0);
    set_hole_parameter("hole.refs.valid", 1.0);
    set_hole_parameter("hole.distance1", (hole_max.y - hole_min.y) * 0.5);
    set_hole_parameter("hole.distance2", (hole_max.x - hole_min.x) * 0.5);
    set_hole_parameter("hole.edge1.start.x", hole_min.x);
    set_hole_parameter("hole.edge1.start.y", hole_min.y);
    set_hole_parameter("hole.edge1.start.z", hole_max.z);
    set_hole_parameter("hole.edge1.end.x", hole_max.x);
    set_hole_parameter("hole.edge1.end.y", hole_min.y);
    set_hole_parameter("hole.edge1.end.z", hole_max.z);
    set_hole_parameter("hole.edge1.side", 1.0);
    set_hole_parameter("hole.edge2.start.x", hole_min.x);
    set_hole_parameter("hole.edge2.start.y", hole_min.y);
    set_hole_parameter("hole.edge2.start.z", hole_max.z);
    set_hole_parameter("hole.edge2.end.x", hole_min.x);
    set_hole_parameter("hole.edge2.end.y", hole_max.y);
    set_hole_parameter("hole.edge2.end.z", hole_max.z);
    set_hole_parameter("hole.edge2.side", -1.0);
    GProp_GProps before_hole_properties;
    BRepGProp::VolumeProperties(hole_body->m_Shape, before_hole_properties);
    ActiveParametricObject hole_operation = hole_tools.ApplyHole(
        hole_document, hole_body->m_id, hole_parameters);
    require(!hole_operation.tool_id.empty()
                && hole_body->GetNumOperations() == 2,
            "Through Hole was not added to the operation history.");
    GProp_GProps after_hole_properties;
    BRepGProp::VolumeProperties(hole_body->m_Shape, after_hole_properties);
    require(after_hole_properties.Mass() < before_hole_properties.Mass(),
            "Through Hole did not remove solid volume.");

    const double moved_x = hole_min.x + (hole_max.x - hole_min.x) * 0.30;
    const double moved_y = hole_min.y + (hole_max.y - hole_min.y) * 0.40;
    for (ToolParameter& parameter : hole_operation.parameters) {
        if (parameter.id == "hole.distance1") {
            parameter.value = moved_y - hole_min.y;
        } else if (parameter.id == "hole.distance2") {
            parameter.value = moved_x - hole_min.x;
        }
    }
    hole_tools.Rebuild(hole_operation, hole_document);
    hole_body = dynamic_cast<CSolid*>(
        hole_document.GetObjects()[hole_box.object_index].get());
    bool found_moved_hole = false;
    if (hole_body) {
        for (TopExp_Explorer edge(hole_body->m_Shape, TopAbs_EDGE);
             edge.More(); edge.Next()) {
            try {
                BRepAdaptor_Curve curve(TopoDS::Edge(edge.Current()));
                if (curve.GetType() != GeomAbs_Circle
                    || std::abs(curve.Circle().Radius() - 5.0) > 0.01) {
                    continue;
                }
                const gp_Pnt location = curve.Circle().Location();
                if (std::abs(location.X() - moved_x) < 0.01
                    && std::abs(location.Y() - moved_y) < 0.01) {
                    found_moved_hole = true;
                    break;
                }
            } catch (const Standard_Failure&) {
            }
        }
    }
    require(found_moved_hole,
            "Editing Hole edge distances did not move the hole center.");

    CAlfaDoc blind_hole_document;
    ActiveParametricObject blind_box = hole_tools.CreateParametricObject(
        "SolidBox", blind_hole_document, box_definition->defaults);
    auto* blind_body = blind_box.object_index
            < blind_hole_document.GetObjects().size()
        ? dynamic_cast<CSolid*>(
            blind_hole_document.GetObjects()[blind_box.object_index].get())
        : nullptr;
    require(blind_body != nullptr, "Blind Hole regression box was not created.");
    blind_hole_document.EnsureObjectId(*blind_body);
    Vec3 blind_min{};
    Vec3 blind_max{};
    require(blind_body->GetBounds(blind_min, blind_max),
            "Blind Hole regression box has no bounds.");
    std::vector<ToolParameter> blind_parameters = hole_definition->defaults;
    const auto set_blind_parameter = [&](const char* id, double value) {
        for (ToolParameter& parameter : blind_parameters) {
            if (parameter.id == id) {
                parameter.value = value;
                return;
            }
        }
    };
    constexpr double blind_diameter = 8.0;
    const double blind_depth = std::min(
        5.0, static_cast<double>(blind_max.z - blind_min.z) * 0.25);
    set_blind_parameter("diameter", blind_diameter);
    set_blind_parameter("hole_type", 1.0);
    set_blind_parameter("depth", blind_depth);
    set_blind_parameter("hole.center.x", (blind_min.x + blind_max.x) * 0.5);
    set_blind_parameter("hole.center.y", (blind_min.y + blind_max.y) * 0.5);
    set_blind_parameter("hole.center.z", blind_max.z);
    set_blind_parameter("hole.normal.x", 0.0);
    set_blind_parameter("hole.normal.y", 0.0);
    set_blind_parameter("hole.normal.z", 1.0);
    GProp_GProps before_blind_properties;
    BRepGProp::VolumeProperties(blind_body->m_Shape, before_blind_properties);
    const ActiveParametricObject blind_operation = hole_tools.ApplyHole(
        blind_hole_document, blind_body->m_id, blind_parameters);
    require(!blind_operation.tool_id.empty(),
            "Blind Hole was not added to the operation history.");
    GProp_GProps after_blind_properties;
    BRepGProp::VolumeProperties(blind_body->m_Shape, after_blind_properties);
    const double removed_blind_volume = before_blind_properties.Mass()
        - after_blind_properties.Mass();
    const double expected_blind_volume = 3.14159265358979323846
        * blind_diameter * blind_diameter * 0.25 * blind_depth;
    require(removed_blind_volume > expected_blind_volume * 0.98
                && removed_blind_volume < expected_blind_volume * 1.02,
            "Blind Hole depth did not control the removed volume.");

    for (ToolParameter& parameter : hole_box.parameters) {
        if (parameter.id == "width") {
            parameter.value *= 1.2;
            break;
        }
    }
    hole_tools.Rebuild(hole_box, hole_document);
    hole_body = dynamic_cast<CSolid*>(
        hole_document.GetObjects()[hole_box.object_index].get());
    require(hole_body && hole_body->GetNumOperations() == 2,
            "Rebuilding the base body lost the Hole operation.");
}

void TestBossPocketRegression() {
    ToolRegistry registry;
    CAlfaDoc document;
    const ToolDefinition* box_definition = registry.Find("SolidBox");
    require(box_definition != nullptr, "Solid Box is not registered.");
    ActiveParametricObject box = registry.CreateParametricObject(
        "SolidBox", document, box_definition->defaults);
    auto* body = box.object_index < document.GetObjects().size()
        ? dynamic_cast<CSolid*>(document.GetObjects()[box.object_index].get())
        : nullptr;
    require(body != nullptr, "Boss/Pocket regression box was not created.");
    document.EnsureObjectId(*body);

    Vec3 bounds_min{};
    Vec3 bounds_max{};
    require(body->GetBounds(bounds_min, bounds_max),
            "Boss/Pocket regression box has no bounds.");
    int front_face = -1;
    for (int face_index = 0; face_index < body->GetNumSurfaces(); ++face_index) {
        Vec3 center{};
        Vec3 normal{};
        if (body->GetFaceCenterAndNormal(face_index, center, normal)
            && normal.z < -0.99f
            && std::fabs(center.z - bounds_min.z) < 1.0e-4f) {
            front_face = face_index;
            break;
        }
    }
    require(front_face >= 0, "Boss/Pocket host face was not found.");

    auto sketch = std::make_unique<CSmartLine>("Boss Pocket Profile");
    const CPoint3d sketch_origin(
        (bounds_min.x + bounds_max.x) * 0.5,
        (bounds_min.y + bounds_max.y) * 0.5,
        bounds_min.z);
    require(sketch->SetCoordinateSystem(
                sketch_origin, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0})
            && sketch->Add(new CLinkLine({-10.0, -10.0, 0.0},
                                         {10.0, -10.0, 0.0}))
            && sketch->Add(new CLinkLine({10.0, -10.0, 0.0},
                                         {10.0, 10.0, 0.0}))
            && sketch->Add(new CLinkLine({10.0, 10.0, 0.0},
                                         {-10.0, 10.0, 0.0}))
            && sketch->Add(new CLinkLine({-10.0, 10.0, 0.0},
                                         {-10.0, -10.0, 0.0}))
            && sketch->SetClosed(true),
            "Boss/Pocket regression profile was not created.");
    sketch->SetFaceAttachment(body->m_id, front_face);
    document.AddObject(std::move(sketch));
    const unsigned long profile_id = document.GetObjects().back()->m_id;

    const auto add_legacy_feature = [&](double operation, double depth) {
        const size_t operation_index = static_cast<size_t>(body->GetNumOperations());
        body->SetParametricOperation(
            operation_index, "SolidSketchFeature", "Boss / Pocket",
            {{"operation", operation}, {"depth", depth}, {"taper", 0.0},
             {"profile.id", static_cast<double>(profile_id)},
             {"face.index", static_cast<double>(front_face)}}, {});
    };
    add_legacy_feature(0.0, 30.0);
    add_legacy_feature(1.0, 24.0);
    add_legacy_feature(1.0, 10.0);

    GProp_GProps base_properties;
    BRepGProp::VolumeProperties(body->m_Shape, base_properties);
    require(registry.ReplayOperations(box.object_index, document),
            "Legacy Boss/Pocket history could not be replayed.");
    body = dynamic_cast<CSolid*>(document.GetObjects()[box.object_index].get());
    require(body && body->GetNumOperations() == 2,
            "Duplicate Boss/Pocket operations were not consolidated.");
    GProp_GProps depth10_properties;
    BRepGProp::VolumeProperties(body->m_Shape, depth10_properties);
    require(std::fabs((base_properties.Mass() - depth10_properties.Mass())
                      - 4000.0) < 1.0,
            "Pocket did not use the oriented host face or its saved depth.");
    const auto require_pocket_starts_at_host_face = [&]() {
        const auto* attached_sketch = dynamic_cast<const CSmartLine*>(
            document.FindObjectById(profile_id));
        require(attached_sketch != nullptr,
                "Pocket lost its attached sketch.");
        require(std::fabs(attached_sketch->GetCoordinateSystem().origin.z
                          - bounds_min.z) < 1.0e-5,
                "Pocket moved its sketch from the host face to the pocket floor.");
        bool host_face_has_opening = false;
        for (int face_index = 0; face_index < body->GetNumSurfaces(); ++face_index) {
            Vec3 center{};
            Vec3 normal{};
            if (!body->GetFaceCenterAndNormal(face_index, center, normal)
                || normal.z > -0.99f
                || std::fabs(center.z - bounds_min.z) > 1.0e-4f) {
                continue;
            }
            int wire_count = 0;
            for (TopExp_Explorer wire(
                     body->GetTopoFace(face_index), TopAbs_WIRE);
                 wire.More(); wire.Next()) {
                ++wire_count;
            }
            host_face_has_opening = host_face_has_opening || wire_count >= 2;
        }
        require(host_face_has_opening,
                "Pocket left the sketch host surface without an opening.");
    };
    require_pocket_starts_at_host_face();

    const auto rebuild_depth = [&](double depth) {
        ActiveParametricObject active = registry.ActiveObjectFromDocument(
            box.object_index, *body, 1, &document);
        for (ToolParameter& parameter : active.parameters) {
            if (parameter.id == "depth")
                parameter.value = depth;
        }
        registry.Rebuild(active, document);
        body = dynamic_cast<CSolid*>(document.GetObjects()[box.object_index].get());
        require(body != nullptr, "Pocket depth rebuild lost the body.");
        GProp_GProps properties;
        BRepGProp::VolumeProperties(body->m_Shape, properties);
        require_pocket_starts_at_host_face();
        return properties.Mass();
    };
    const double depth5_volume = rebuild_depth(5.0);
    const double depth20_volume = rebuild_depth(20.0);
    require(depth5_volume > depth10_properties.Mass()
                && depth20_volume < depth10_properties.Mass()
                && depth5_volume - depth20_volume > 5900.0,
            "Pocket Depth still does not control the removed volume.");

    ActiveParametricObject boss = registry.ActiveObjectFromDocument(
        box.object_index, *body, 1, &document);
    for (ToolParameter& parameter : boss.parameters) {
        if (parameter.id == "operation")
            parameter.value = 0.0;
        else if (parameter.id == "depth")
            parameter.value = 7.0;
    }
    registry.Rebuild(boss, document);
    body = dynamic_cast<CSolid*>(document.GetObjects()[box.object_index].get());
    require(body != nullptr, "Switching Pocket to Boss lost the body.");
    GProp_GProps boss_properties;
    BRepGProp::VolumeProperties(body->m_Shape, boss_properties);
    require(std::fabs((boss_properties.Mass() - base_properties.Mass())
                      - 2800.0) < 1.0,
            "Boss did not extrude outward from the oriented host face.");
}

void TestFaceBasedPrimitiveBooleanOverlap() {
	ToolRegistry registry;
	const auto verify = [&](const char* tool_id,
		const char* extrusion_parameter, double extrusion) {
		const ToolDefinition* definition = registry.Find(tool_id);
		require(definition != nullptr,
			"Face-overlap primitive tool is not registered.");
		std::vector<ToolParameter> parameters = definition->defaults;
		for (ToolParameter& parameter : parameters) {
			if (parameter.id == extrusion_parameter)
				parameter.value = extrusion;
		}
		parameters.push_back({
			"boolean.body_id", "Boolean Body", 1.0, 0.0,
			static_cast<double>(std::numeric_limits<unsigned long>::max()), 1.0});
		CAlfaDoc document;
		ActiveParametricObject object = registry.CreateParametricObject(
			tool_id, document, parameters);
		const CSolid* solid = object.object_index < document.GetObjects().size()
			? dynamic_cast<const CSolid*>(
				document.GetObjects()[object.object_index].get())
			: nullptr;
		require(solid != nullptr,
			"Face-overlap primitive was not created.");
		Vec3 minimum{};
		Vec3 maximum{};
		require(solid->GetBounds(minimum, maximum),
			"Face-overlap primitive has no bounds.");
		constexpr double delta = 0.01;
		constexpr double tolerance = 2.0e-4;
		const double expected_minimum = extrusion > 0.0
			? -delta : extrusion + delta;
		const double expected_maximum = extrusion > 0.0
			? extrusion - delta : delta;
		require(std::fabs(minimum.z - expected_minimum) <= tolerance
				&& std::fabs(maximum.z - expected_maximum) <= tolerance,
			"Face-based primitive does not overlap the base plane by Delta 0.01.");
	};

	verify("SolidBox", "depth", 5.0);
	verify("SolidBox", "depth", -5.0);
	verify("SolidCylinder", "height", 5.0);
	verify("SolidCylinder", "height", -5.0);

	// A Box created on a Solid face is committed into the host as a second
	// editable parametric operation, rather than remaining a separate body.
	CAlfaDoc document;
	document.GetObjects().clear();
	const ToolDefinition* box_definition = registry.Find("SolidBox");
	require(box_definition != nullptr, "SolidBox tool is not registered.");
	std::vector<ToolParameter> host_parameters = box_definition->defaults;
	for (ToolParameter& parameter : host_parameters) {
		if (parameter.id == "width" || parameter.id == "height")
			parameter.value = 20.0;
		else if (parameter.id == "depth")
			parameter.value = 10.0;
	}
	const ActiveParametricObject host = registry.CreateParametricObject(
		"SolidBox", document, host_parameters);
	require(host.object_index < document.GetObjects().size(),
		"Could not create the host Box.");
	const unsigned long host_id =
		document.GetObjects()[host.object_index]->m_id;

	std::vector<ToolParameter> cutter_parameters = box_definition->defaults;
	for (ToolParameter& parameter : cutter_parameters) {
		if (parameter.id == "width" || parameter.id == "height")
			parameter.value = 5.0;
		else if (parameter.id == "depth")
			parameter.value = -5.0;
		else if (parameter.id == "origin.z")
			parameter.value = 10.0;
	}
	cutter_parameters.push_back({
		"boolean.body_id", "Boolean Body", static_cast<double>(host_id),
		0.0, static_cast<double>(std::numeric_limits<unsigned long>::max()), 1.0});
	const ActiveParametricObject cutter = registry.CreateParametricObject(
		"SolidBox", document, cutter_parameters);
	require(cutter.object_index < document.GetObjects().size()
			&& document.ApplyBooleanToSolids(
				host.object_index, cutter.object_index, BooleanOperation::Cut),
		"Face-based Box was not committed as a Boolean Cut.");
	require(document.GetObjects().size() == 1,
		"Face-based Boolean left the cutter as a separate object.");
	auto* result = dynamic_cast<CSolid*>(document.GetObjects().front().get());
	require(result && result->GetNumOperations() == 2
			&& result->GetOperation(1)
			&& result->GetOperation(1)->ToolId == "boolean",
		"Face-based Box did not add the second parametric operation.");

	GProp_GProps deep_cut_properties;
	BRepGProp::VolumeProperties(result->m_Shape, deep_cut_properties);
	ActiveParametricObject editable = registry.ActiveObjectFromDocument(
		0, *result, 1, &document);
	require(editable.tool_id == "SolidBox",
		"The second Box operation cannot be reopened for editing.");
	for (ToolParameter& parameter : editable.parameters) {
		if (parameter.id == "depth")
			parameter.value = -2.0;
	}
	registry.Rebuild(editable, document);
	result = dynamic_cast<CSolid*>(document.GetObjects().front().get());
	GProp_GProps shallow_cut_properties;
	if (result)
		BRepGProp::VolumeProperties(result->m_Shape, shallow_cut_properties);
	require(result && result->GetNumOperations() == 2
			&& shallow_cut_properties.Mass() > deep_cut_properties.Mass() + 70.0,
		"Editing the second Box operation did not rebuild the host body.");

	// The same operation-history editing path must work for a face-based
	// Cylinder.  It used to reopen only Box boolean tools.
	CAlfaDoc cylinder_document;
	cylinder_document.GetObjects().clear();
	const ActiveParametricObject cylinder_host =
		registry.CreateParametricObject(
			"SolidBox", cylinder_document, host_parameters);
	require(cylinder_host.object_index
			< cylinder_document.GetObjects().size(),
		"Could not create the Cylinder test host.");
	const unsigned long cylinder_host_id =
		cylinder_document.GetObjects()[cylinder_host.object_index]->m_id;
	const ToolDefinition* cylinder_definition = registry.Find("SolidCylinder");
	require(cylinder_definition != nullptr,
		"SolidCylinder tool is not registered.");
	std::vector<ToolParameter> cylinder_parameters =
		cylinder_definition->defaults;
	for (ToolParameter& parameter : cylinder_parameters) {
		if (parameter.id == "diameter")
			parameter.value = 6.0;
		else if (parameter.id == "height")
			parameter.value = -5.0;
		else if (parameter.id == "origin.x"
				|| parameter.id == "origin.y")
			parameter.value = 10.0;
		else if (parameter.id == "origin.z")
			parameter.value = 10.0;
	}
	cylinder_parameters.push_back({
		"boolean.body_id", "Boolean Body",
		static_cast<double>(cylinder_host_id), 0.0,
		static_cast<double>(std::numeric_limits<unsigned long>::max()), 1.0});
	const ActiveParametricObject cylinder_cutter =
		registry.CreateParametricObject(
			"SolidCylinder", cylinder_document, cylinder_parameters);
	require(cylinder_cutter.object_index
				< cylinder_document.GetObjects().size()
			&& cylinder_document.ApplyBooleanToSolids(
				cylinder_host.object_index,
				cylinder_cutter.object_index,
				BooleanOperation::Cut),
		"Face-based Cylinder was not committed as a Boolean Cut.");
	auto* cylinder_result = dynamic_cast<CSolid*>(
		cylinder_document.GetObjects().front().get());
	require(cylinder_result && cylinder_result->GetNumOperations() == 2,
		"Face-based Cylinder did not add the second parametric operation.");
	GProp_GProps narrow_cylinder_properties;
	BRepGProp::VolumeProperties(
		cylinder_result->m_Shape, narrow_cylinder_properties);
	ActiveParametricObject editable_cylinder =
		registry.ActiveObjectFromDocument(
			0, *cylinder_result, 1, &cylinder_document);
	require(editable_cylinder.tool_id == "SolidCylinder",
		"The second Cylinder operation cannot be reopened for editing.");
	for (ToolParameter& parameter : editable_cylinder.parameters) {
		if (parameter.id == "diameter")
			parameter.value = 8.0;
	}
	registry.Rebuild(editable_cylinder, cylinder_document);
	cylinder_result = dynamic_cast<CSolid*>(
		cylinder_document.GetObjects().front().get());
	GProp_GProps wide_cylinder_properties;
	if (cylinder_result)
		BRepGProp::VolumeProperties(
			cylinder_result->m_Shape, wide_cylinder_properties);
	require(cylinder_result && cylinder_result->GetNumOperations() == 2
			&& wide_cylinder_properties.Mass()
				< narrow_cylinder_properties.Mass() - 100.0,
		"Editing the second Cylinder operation did not rebuild the host body.");

	// A pocket crossing the outer edge turns the top into one concave planar
	// face.  It must use its actual contour, not the rectangular UV bounds.
	TopoDS_Shape notched_shape = BRepAlgoAPI_Cut(
		BRepPrimAPI_MakeBox(100.0, 80.0, 30.0).Shape(),
		BRepPrimAPI_MakeBox(
			gp_Pnt(30.0, -1.0, 15.0), 25.0, 20.0, 15.01).Shape()).Shape();
	CSolid notched(notched_shape);
	notched.MeshQuadro = true;
	require(notched.InitSurfaces() && notched.InitEdges()
			&& notched.ReBuldMesh(2.0f),
		"Could not build the edge-pocket Quadro regression solid.");
	bool tested_top = false;
	for (int surface_index = 0;
		surface_index < notched.GetNumSurfaces(); ++surface_index) {
		const CSurfaceFace* surface = notched.GetSurfaceFace(surface_index);
		if (!surface || !surface->pMesh3D || !surface->IsPlanar())
			continue;
		Vec3 center{};
		Vec3 normal{};
		if (!surface->GetCenterAndNormal(center, normal)
			|| normal.z < 0.9f || center.z < 29.9f)
			continue;
		tested_top = true;
		double mesh_area = 0.0;
		for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
			if (face.deleted || face.corners.size() < 3)
				continue;
			const Vec3 first = surface->pMesh3D->GetVertices()[
				face.corners.front().v];
			for (size_t corner = 1; corner + 1 < face.corners.size(); ++corner) {
				const Vec3 second = surface->pMesh3D->GetVertices()[
					face.corners[corner].v];
				const Vec3 third = surface->pMesh3D->GetVertices()[
					face.corners[corner + 1].v];
				const Vec3 area_vector = cross(second - first, third - first);
				mesh_area += 0.5 * std::sqrt(
					static_cast<double>(dot(area_vector, area_vector)));
			}
		}
		const double expected_area = 100.0 * 80.0 - 25.0 * 19.0;
		require(std::fabs(mesh_area - expected_area) < 1.0,
			"Quadro filled the concave top face across the boolean edge pocket.");
	}
	require(tested_top,
		"The edge-pocket regression did not find the concave top face.");
}

void TestDraftingPersistence() {
    QTemporaryDir directory;
    require(directory.isValid(), "Could not create Drafting test directory.");
    const QString path = directory.filePath("drafting-roundtrip.dom3d");
    const std::string drafting =
        R"({"version":1,"sheets":[{"name":"A4 Test","format":"A4","width":210,"height":297,"landscape":false,"stamp":"1-й лист","stampVisible":true,"scale":1,"primitives":[{"type":"line","x1":20,"y1":30,"x2":120,"y2":30,"text":""}]}]})";
    CAlfaDoc source;
    source.SetDraftingData(drafting);
    Dom3DProjectSerializer serializer;
    ProjectViewState view;
    QString error;
    require(serializer.Save(path, source, "Drafting", view, {}, error),
            error.toLocal8Bit().constData());

    CAlfaDoc loaded;
    QString room;
    require(serializer.Load(path, loaded, room, view, error),
            error.toLocal8Bit().constData());
    require(room == "Drafting", "Drafting room was not restored.");
    require(loaded.GetDraftingData() == drafting,
            "Drafting sheets were not preserved by DOM3D serialization.");
}

void TestMeshVar7Regression() {
    const auto require_case = [](int touchedVertex,
                                 const std::array<std::array<size_t, 3>, 4>& expected) {
        CMesh3D mesh;
        require(mesh.SetGeometry(
                    {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                     {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                     {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f}},
                    {CMesh3D::Face{0, 1, 2, 3},
                     CMesh3D::Face{1, 4, 5, 2}}),
                "Could not prepare the Var-7 regression mesh.");

        // This field deliberately does not describe the shared edge.  Var-7
        // must derive the neighbour's local edge index from mesh topology.
        mesh.GetFaces()[1].edgeIndex = 0;
        require(mesh.SplitFaceByVar7(0, touchedVertex, 1),
                "SplitFaceByVar7 rejected a valid pair of quads.");
        require(mesh.GetFaces().size() == 4,
                "SplitFaceByVar7 did not create two replacement triangles.");

        for (size_t faceIndex = 0; faceIndex < expected.size(); ++faceIndex) {
            const CMesh3D::Face& face = mesh.GetFaces()[faceIndex];
            require(face.corners.size() == 3,
                    "SplitFaceByVar7 left a non-triangular result face.");
            for (size_t corner = 0; corner < 3; ++corner) {
                require(face.corners[corner].v == expected[faceIndex][corner],
                        "SplitFaceByVar7 produced the wrong Var-7 topology.");
            }
        }
        require(mesh.GetFaces()[0].m_Trimmed && mesh.GetFaces()[1].m_Trimmed,
                "SplitFaceByVar7 did not mark both source faces as processed.");
    };

    // Shared edge 1 of the first quad is edge 3 of the second quad.
    require_case(3, {{{0, 1, 3}, {1, 4, 5}, {1, 5, 3}, {5, 2, 3}}});
    require_case(0, {{{0, 2, 3}, {2, 4, 5}, {1, 4, 0}, {4, 2, 0}}});
}

void TestTrimClassificationDiagnostics() {
    CAlfaDoc document;
    const int originalWorkLayer = document.GetWorkLayerID();
    const size_t originalObjectCount = document.GetObjects().size();
    const CAlfaObject* originalSelection = document.GetSelectedObject();
    CMesh3D::SetTrimClassificationDiagnosticsEnabled(true);

    CMesh3D mesh;
    require(mesh.SetGeometry(
                {{0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f},
                 {10.0f, 10.0f, 0.0f}, {0.0f, 10.0f, 0.0f}},
                {CMesh3D::Face{0, 1, 2, 3}}),
            "Could not prepare the trim diagnostics mesh.");
    CPolyline trim;
    trim.AddPoint({0.0, 0.0, 0.0});
    trim.AddPoint({10.0, 0.0, 0.0});
    trim.AddPoint({10.0, 10.0, 0.0});
    trim.SetClosed(true);
    require(mesh.TrimByPline(&trim, {0.0, 10.0, 0.0}),
            "Trim diagnostics regression contour was not processed.");

    require(document.GetObjects().size() == originalObjectCount + 2,
            "Trim diagnostics did not create a mesh snapshot and face polyline.");
    const auto* snapshot = dynamic_cast<const CMesh3D*>(
        document.GetObjects()[originalObjectCount].get());
    require(snapshot
                && snapshot->GetName() == "Mesh3D - Before Trim"
                && snapshot->GetVertices().size() == 4
                && snapshot->GetFaces().size() == 1
                && snapshot->GetFaces().front().corners.size() == 4,
            "Trim diagnostics did not preserve the pre-trim mesh.");
    const auto* diagnostic = dynamic_cast<const CPolyline*>(
        document.GetObjects().back().get());
    require(diagnostic && diagnostic->GetName() == "VariantCut = 2"
                && diagnostic->IsClosed()
                && diagnostic->GetPointCount() == 4,
            "Trim diagnostics created an invalid Var-2 polyline.");
    const CLayer* diagnosticLayer = document.GetLayerByID(
        diagnostic->m_LayerID);
    require(diagnosticLayer
                && diagnosticLayer->Name == "Trim Classification Diagnostics",
            "Trim diagnostics polyline is not on its dedicated layer.");
    require(snapshot->m_LayerID == diagnostic->m_LayerID,
            "Pre-trim mesh snapshot is not on the diagnostic layer.");
    require(std::fabs(diagnostic->GetLineWidth() - 1.0) < 1.0e-8,
            "Trim diagnostic polyline does not use line width 1.");
    require(document.GetWorkLayerID() == originalWorkLayer,
            "Trim diagnostics changed the document work layer.");
    require(document.GetSelectedObject() == originalSelection,
            "Trim diagnostics changed the document selection.");
    const Color color = diagnostic->GetColor();
    require(color.b > 0.99f && color.r < 0.01f && color.g < 0.01f,
            "Var-2 diagnostic polyline is not blue.");

    CMesh3D::SetTrimClassificationDiagnosticsEnabled(false);
}

void TestCreateSurfaceFromSketch() {
    CAlfaDoc document;
    const size_t initial_object_count = document.GetObjects().size();
    auto sketch = std::make_unique<CSmartLine>("L-shaped sketch");
    require(sketch->Add(new CLinkLine(
                {0.0, 0.0, 0.0}, {10.0, 0.0, 0.0}))
            && sketch->Add(new CLinkLine(
                {10.0, 0.0, 0.0}, {10.0, 4.0, 0.0}))
            && sketch->Add(new CLinkLine(
                {10.0, 4.0, 0.0}, {4.0, 4.0, 0.0}))
            && sketch->Add(new CLinkLine(
                {4.0, 4.0, 0.0}, {4.0, 10.0, 0.0}))
            && sketch->Add(new CLinkLine(
                {4.0, 10.0, 0.0}, {0.0, 10.0, 0.0}))
            && sketch->Add(new CLinkLine(
                {0.0, 10.0, 0.0}, {0.0, 0.0, 0.0}))
            && sketch->SetClosed(true),
            "Could not prepare the trimmed surface sketch.");
    document.AddObject(std::move(sketch));

    std::string error;
    require(document.CreateSurfaceFromSelectedSketch(&error),
            error.empty() ? "Create Surface rejected a valid sketch."
                          : error.c_str());
    require(document.GetObjects().size() == initial_object_count + 2,
            "Create Surface did not add exactly one document object.");
    const auto* surface = dynamic_cast<const CSurfaceSet*>(
        document.GetObjects().back().get());
    require(surface && surface->GetName() == "L-shaped sketch Surface"
                && surface->GetNumSurfaces() == 1,
            "Create Surface did not create a named Surface Set.");

    GProp_GProps properties;
    BRepGProp::SurfaceProperties(surface->GetTopoFace(0), properties);
    require(std::abs(properties.Mass() - 64.0) < 1.0e-6,
            "Create Surface lost the sketch trimming boundary.");
    require(document.GetSelectedObject() == surface,
            "Create Surface did not select the new surface.");
}

void TestCurveToPolylineByLength() {
    CAlfaDoc document;
    const size_t initial_object_count = document.GetObjects().size();
    auto curve = std::make_unique<CBSpline>("Length curve");
    curve->AddPoint({0.0, 0.0, 0.0});
    curve->AddPoint({100.0, 0.0, 0.0});
    document.AddObject(std::move(curve));

    std::string error;
    require(document.CreatePolylineFromSelectedCurveByLength(30.0, &error),
            error.empty() ? "Curve To Polyline rejected a valid curve."
                          : error.c_str());
    require(document.GetObjects().size() == initial_object_count + 2,
            "Curve To Polyline did not add exactly one object.");
    const auto* polyline = dynamic_cast<const CPolyline*>(
        document.GetObjects().back().get());
    require(polyline && polyline->GetName() == "Length curve Polyline"
                && !polyline->IsClosed()
                && polyline->GetPointCount() == 4,
            "Curve To Polyline did not preserve the Old Dom Qty rule.");
    for (size_t index = 0; index < polyline->GetPointCount(); ++index) {
        const double expected = 100.0 * static_cast<double>(index) / 3.0;
        require(std::abs(polyline->GetPoints()[index].x - expected) < 1.0e-5
                    && std::abs(polyline->GetPoints()[index].y) < 1.0e-8
                    && std::abs(polyline->GetPoints()[index].z) < 1.0e-8,
                "Curve To Polyline knots are not equidistant by curve length.");
    }
}

void TestMeshWireColor() {
    const Color chosen{0.08f, 0.24f, 0.91f};
    const auto require_chosen = [chosen](MeshDisplayMode mode, bool selected) {
        const Color resolved = CMesh3D::ResolveWireColor(
            chosen, mode, selected);
        require(std::abs(resolved.r - chosen.r) < 1.0e-7f
                    && std::abs(resolved.g - chosen.g) < 1.0e-7f
                    && std::abs(resolved.b - chosen.b) < 1.0e-7f,
                "Mesh wire rendering ignored the selected object color.");
    };
    require_chosen(MeshDisplayMode::SurfaceGray, false);
    require_chosen(MeshDisplayMode::SurfaceGray, true);
    require_chosen(MeshDisplayMode::SurfaceColored, false);
    require_chosen(MeshDisplayMode::SurfaceColored, true);
    require_chosen(MeshDisplayMode::SurfaceMaterialWithMesh, false);
    require_chosen(MeshDisplayMode::SurfaceMaterialWithMesh, true);
}

void TestLowPolyDensityCalibration() {
    constexpr double longestEdge = 218.098;
    constexpr double bossEdge = 55.1373;
    const std::array<std::pair<float, int>, 5> cases{{
        {0.200f, 3},
        // Do not let the boss edge stay at Qty Min after the background grid
        // has already crossed to its denser 0.25 level.
        {0.250f, 5},
        {0.296f, 5},
        {0.520f, 7},
        {0.693f, 11},
    }};

    for (const auto& [density, expectedPointCount] : cases) {
        TopoDS_Shape shape = BRepPrimAPI_MakeBox(
            longestEdge, bossEdge, 10.0).Shape();
        CSolid solid(shape);
        solid.MeshQuadro = true;
        require(solid.InitSurfaces() && solid.InitEdges()
                    && solid.ReBuldMesh(1.0f / density),
                "Could not build the Low Poly density calibration box.");

        int matchingEdges = 0;
        for (int surfaceIndex = 0;
             surfaceIndex < solid.GetNumSurfaces(); ++surfaceIndex) {
            const CSurfaceFace* surface = solid.GetSurfaceFace(surfaceIndex);
            if (!surface)
                continue;
            for (int edgeIndex = 0;
                 edgeIndex < surface->GetPreparedPolylineCount();
                 ++edgeIndex) {
                std::vector<CPoint3d> points;
                if (!surface->GetPreparedPolylinePoints(edgeIndex, points)
                    || points.size() < 2) {
                    continue;
                }
                double length = 0.0;
                for (size_t pointIndex = 1;
                     pointIndex < points.size(); ++pointIndex) {
                    length += points[pointIndex - 1].DistTo(
                        &points[pointIndex]);
                }
                if (std::abs(length - bossEdge) > 1.0e-3)
                    continue;
                ++matchingEdges;
                require(static_cast<int>(points.size()) == expectedPointCount,
                        "Low Poly density does not match the calibrated 3DCoat progression.");
            }
        }
        require(matchingEdges > 0,
                "The Low Poly density calibration edge was not found.");
    }
}

void TestSurfacePatchWelding() {
    TopoDS_Shape shape = BRepPrimAPI_MakeBox(60.0, 36.0, 8.0).Shape();
    shape = BRepAlgoAPI_Cut(shape, BRepPrimAPI_MakeBox(
        gp_Pnt(10.0, 10.0, -1.0), 10.0, 10.0, 10.0).Shape()).Shape();
    shape = BRepAlgoAPI_Cut(shape, BRepPrimAPI_MakeBox(
        gp_Pnt(40.0, 10.0, -1.0), 10.0, 10.0, 10.0).Shape()).Shape();

    CSolid solid(shape);
    solid.MeshQuadro = true;
    require(solid.InitSurfaces() && solid.InitEdges()
                && solid.ReBuldMesh(2.0f),
            "Could not prepare the surface-patch welding regression solid.");

    bool tested = false;
    for (int index = 0; index < solid.GetNumSurfaces(); ++index) {
        CSurfaceFace* surface = solid.GetSurfaceFace(index);
        if (!surface || surface->m_TypeMesh == REGULAR_MESH
            || surface->GetPreparedPolylineCount() < 8) {
            continue;
        }
        require(surface->BuildFilledMeshWhithHoles(2.0f),
                "Could not build the two-hole UV patch mesh.");
        require(surface->pMesh3D
                    && ActiveFaceEdgeComponentCount(*surface->pMesh3D) == 1,
                "Patches of one surface remained separate mesh islands.");
        bool manifold = false;
        require(ClosedMeshBoundaryLoopCount(*surface->pMesh3D, manifold) == 3
                    && manifold,
                "A welded two-hole surface retained an internal open patch seam.");
        tested = true;
    }
    require(tested, "The patch-welding regression found no two-hole surface.");
}

void TestSmallHoleSlxCollar() {
    const std::array<gp_Pnt, 3> centers{{
        gp_Pnt(50.0, 50.0, -1.0),
        gp_Pnt(43.0, 47.0, -1.0),
        gp_Pnt(51.5, 43.5, -1.0)}};
	for (const float density : {0.15f, 0.20f, 0.25f}) {
	for (const gp_Pnt& center : centers) {
        TopoDS_Shape shape = BRepPrimAPI_MakeBox(100.0, 100.0, 10.0).Shape();
        const TopoDS_Shape cutter = BRepPrimAPI_MakeCylinder(
            gp_Ax2(center, gp_Dir(0.0, 0.0, 1.0)), 3.0, 12.0).Shape();
        shape = BRepAlgoAPI_Cut(shape, cutter).Shape();

        CSolid solid(shape);
        solid.MeshQuadro = true;
        solid.MeshQuadroHoleSLX = true;
        require(solid.InitSurfaces() && solid.InitEdges()
                    && solid.ReBuldMesh(1.0f / density),
                "Could not build the small-hole SLX regression solid.");

        bool tested = false;
        for (int index = 0; index < solid.GetNumSurfaces(); ++index) {
            CSurfaceFace* surface = solid.GetSurfaceFace(index);
            if (!surface || surface->m_TypeMesh == REGULAR_MESH
                || BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                    != GeomAbs_Plane) {
                continue;
            }
            require(surface->BuildFilledMeshWhithHoles(1.0f / density, true),
                    "SLX rejected a small circular hole.");
            require(ActiveFaceEdgeComponentCount(*surface->pMesh3D) == 1,
                    "The small-hole collar is disconnected from the background mesh.");
			double minimum_collar_reach =
				std::numeric_limits<double>::max();
            for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
				if (face.deleted)
					continue;
				require(face.corners.size() == 3 || face.corners.size() == 4,
						"The small-hole collar contains a non-local polygon.");
				bool touches_hole = false;
				double face_reach = 0.0;
				double longest_edge = 0.0;
				for (const MeshCorner& corner : face.corners) {
					const Vec3 vertex = surface->pMesh3D->GetVertices()[corner.v];
					const double radius = std::hypot(
						static_cast<double>(vertex.x) - center.X(),
						static_cast<double>(vertex.y) - center.Y());
					touches_hole = touches_hole
						|| std::fabs(radius - 3.0) <= 0.05;
					face_reach = std::max(face_reach, radius);
				}
				for (size_t corner = 0; corner < face.corners.size(); ++corner) {
					const Vec3 first = surface->pMesh3D->GetVertices()[
						face.corners[corner].v];
					const Vec3 second = surface->pMesh3D->GetVertices()[
						face.corners[(corner + 1) % face.corners.size()].v];
					const Vec3 edge = second - first;
					longest_edge = std::max(longest_edge,
						std::sqrt(static_cast<double>(dot(edge, edge))));
				}
				if (face.corners.size() == 3) {
					require(longest_edge <= 20.0,
							"The small-hole collar retained a long radial triangle fan.");
				}
				if (touches_hole)
					minimum_collar_reach = std::min(
						minimum_collar_reach, face_reach);
            }
			require(std::isfinite(minimum_collar_reach)
					&& minimum_collar_reach >= 6.0,
					"The small-hole SLX collar is narrower than one useful mesh transition.");
            tested = true;
        }
        require(tested, "The small-hole regression found no trimmed planar face.");
    }
	}
}
}

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    if (argc == 2 && std::string(argv[1]) == "--test-low-poly-quadro") {
        TestLowPolyQuadroBranch();
        std::cout << "Low Poly Quadro tests passed.\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-surface-patch-welding") {
        TestSurfacePatchWelding();
        std::cout << "Surface patch welding tests passed.\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-small-hole-slx-collar") {
        TestSmallHoleSlxCollar();
        std::cout << "Small-hole SLX collar tests passed.\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-boss-pocket") {
        TestBossPocketRegression();
		TestFaceBasedPrimitiveBooleanOverlap();
        std::cout << "Boss/Pocket tests passed.\n";
        return 0;
    }
    if (argc == 2
        && std::string(argv[1]) == "--test-cylinder-seam-trim") {
        TestCylinderSeamTrimDirection();
        std::cout << "Cylinder seam trim tests passed.\n";
        return 0;
    }
    if (argc == 2
        && std::string(argv[1]) == "--test-sphere-pole-quadro") {
        TestSpherePoleQuadroProjection();
        std::cout << "Sphere-pole Quadro tests passed.\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-drafting") {
        TestDraftingPersistence();
        std::cout << "Drafting persistence tests passed.\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-mesh-var7") {
        TestMeshVar7Regression();
        std::cout << "Mesh Var-7 tests passed.\n";
        return 0;
    }
    if (argc == 2
        && std::string(argv[1]) == "--test-trim-diagnostics") {
        TestTrimClassificationDiagnostics();
        std::cout << "Trim classification diagnostics tests passed.\n";
        return 0;
    }
    if (argc == 2
        && std::string(argv[1]) == "--test-create-sketch-surface") {
        TestCreateSurfaceFromSketch();
        std::cout << "Create Surface from sketch tests passed.\n";
        return 0;
    }
    if (argc == 2
        && std::string(argv[1]) == "--test-curve-to-polyline-by-length") {
        TestCurveToPolylineByLength();
        std::cout << "Curve To Polyline By length tests passed.\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-mesh-wire-color") {
        TestMeshWireColor();
        std::cout << "Mesh wire color tests passed.\n";
        return 0;
    }
    if (argc == 2
        && std::string(argv[1]) == "--test-low-poly-density-calibration") {
        TestLowPolyDensityCalibration();
        std::cout << "Low Poly density calibration tests passed.\n";
        return 0;
    }
    if ((argc == 4 || argc == 5)
        && std::string(argv[1]) == "--diagnose-project-quadro") {
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room;
        ProjectViewState view;
        QString error;
        require(serializer.Load(QString::fromLocal8Bit(argv[2]), document,
                                room, view, error),
                error.toLocal8Bit().constData());
        const float density = std::stof(argv[3]);
        require(density > 0.0f, "Density must be positive.");
        const bool useSlx = argc == 5 && std::string(argv[4]) == "slx";
        for (const auto& object : document.GetObjects()) {
            auto* solid = dynamic_cast<CSolid*>(object.get());
            if (!solid)
                continue;
            solid->MeshQuadro = true;
            solid->MeshQuadroHoleSLX = useSlx;
            const bool rebuilt = solid->ReBuldMesh(1.0f / density);
            std::cout << "solid=\"" << solid->GetName() << "\" rebuilt="
                      << rebuilt << " surfaces=" << solid->GetNumSurfaces()
                      << '\n';
            for (int index = 0; index < solid->GetNumSurfaces(); ++index) {
                const CSurfaceFace* surface = solid->GetSurfaceFace(index);
                if (!surface)
                    continue;
                GeomAbs_SurfaceType geometry_type = GeomAbs_OtherSurface;
                int topological_edges = 0;
                int degenerated_edges = 0;
                try {
                    geometry_type = BRepAdaptor_Surface(
                        TopoDS::Face(surface->m_Face)).GetType();
                    for (TopExp_Explorer edge(surface->m_Face, TopAbs_EDGE);
                         edge.More(); edge.Next()) {
                        ++topological_edges;
                        if (BRep_Tool::Degenerated(
                                TopoDS::Edge(edge.Current()))) {
                            ++degenerated_edges;
                        }
                    }
                } catch (const Standard_Failure&) {
                }
                size_t triangles = 0;
                size_t quads = 0;
                size_t vertices = 0;
                double maximum_mesh_edge = 0.0;
                double minimum_mesh_edge = std::numeric_limits<double>::max();
				Vec3 minimum_edge_first{};
				Vec3 minimum_edge_second{};
                size_t non_manifold_index_edges = 0;
                size_t overlapping_geometric_edges = 0;
                size_t mesh_components = 0;
                size_t occt_outside_faces = 0;
                Vec3 mesh_min{
                    std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max()};
                Vec3 mesh_max{
                    std::numeric_limits<float>::lowest(),
                    std::numeric_limits<float>::lowest(),
                    std::numeric_limits<float>::lowest()};
                if (surface->pMesh3D) {
					SurfaceUVMapping surface_mapping(surface);
					std::map<std::pair<size_t, size_t>, size_t> index_edge_use;
					using PositionKey = std::array<long long, 3>;
					std::map<std::pair<PositionKey, PositionKey>, size_t>
						geometric_edge_use;
					const auto position_key = [](Vec3 point) {
						constexpr double scale = 100000.0;
						return PositionKey{
							std::llround(point.x * scale),
							std::llround(point.y * scale),
							std::llround(point.z * scale)};
					};
                    vertices = surface->pMesh3D->GetVertices().size();
                    for (const Vec3& vertex : surface->pMesh3D->GetVertices()) {
                        mesh_min.x = std::min(mesh_min.x, vertex.x);
                        mesh_min.y = std::min(mesh_min.y, vertex.y);
                        mesh_min.z = std::min(mesh_min.z, vertex.z);
                        mesh_max.x = std::max(mesh_max.x, vertex.x);
                        mesh_max.y = std::max(mesh_max.y, vertex.y);
                        mesh_max.z = std::max(mesh_max.z, vertex.z);
                    }
                    for (const CMesh3D::Face& face :
                         surface->pMesh3D->GetFaces()) {
                        if (face.deleted)
                            continue;
						Vec3 face_center{};
						for (const MeshCorner& corner : face.corners) {
							if (corner.v < surface->pMesh3D->GetVertices().size())
								face_center = face_center
									+ surface->pMesh3D->GetVertices()[corner.v];
						}
						if (!face.corners.empty()) {
							face_center = face_center * (1.0f
								/ static_cast<float>(face.corners.size()));
							SurfaceUVPoint uv;
							if (surface_mapping.Project(face_center, uv)) {
								BRepClass_FaceClassifier classifier(
									TopoDS::Face(surface->m_Face),
									gp_Pnt2d(uv.u, uv.v), 1.0e-7,
									Standard_False);
								occt_outside_faces +=
									classifier.State() == TopAbs_OUT;
							}
						}
                        triangles += face.corners.size() == 3;
                        quads += face.corners.size() == 4;
                        for (size_t corner = 0; corner < face.corners.size(); ++corner) {
                            const size_t first = face.corners[corner].v;
                            const size_t second = face.corners[
                                (corner + 1) % face.corners.size()].v;
                            if (first >= surface->pMesh3D->GetVertices().size()
                                || second >= surface->pMesh3D->GetVertices().size()) {
                                continue;
                            }
                            const Vec3 edge = surface->pMesh3D->GetVertices()[second]
                                - surface->pMesh3D->GetVertices()[first];
							const double edge_length = std::sqrt(
								static_cast<double>(dot(edge, edge)));
                            maximum_mesh_edge = std::max(maximum_mesh_edge, edge_length);
							if (edge_length < minimum_mesh_edge) {
								minimum_mesh_edge = edge_length;
								minimum_edge_first = surface->pMesh3D->GetVertices()[first];
								minimum_edge_second = surface->pMesh3D->GetVertices()[second];
							}
							++index_edge_use[std::minmax(first, second)];
							PositionKey first_key = position_key(
								surface->pMesh3D->GetVertices()[first]);
							PositionKey second_key = position_key(
								surface->pMesh3D->GetVertices()[second]);
							if (second_key < first_key)
								std::swap(first_key, second_key);
							++geometric_edge_use[{first_key, second_key}];
                        }
                    }
					for (const auto& edge : index_edge_use)
						non_manifold_index_edges += edge.second > 2;
					for (const auto& edge : geometric_edge_use)
						overlapping_geometric_edges += edge.second > 2;
					mesh_components = ActiveFaceEdgeComponentCount(*surface->pMesh3D);
                }
                int adaptive_net_result = -1;
                if (vertices == 0) {
                    CNet probe;
                    adaptive_net_result = probe.Build(
                        const_cast<CSurfaceFace*>(surface),
                        0.5 / static_cast<double>(density));
                }
                std::cout << "  surface=" << index
                          << " type=" << surface->m_TypeMesh
                          << " geom=" << static_cast<int>(geometry_type)
                          << " edges=" << topological_edges
                          << " degenerated=" << degenerated_edges
                          << " prepared="
                          << surface->GetPreparedPolylineCount()
						  << " preparedEdges=[";
				for (int edge_index = 0;
					edge_index < surface->GetPreparedPolylineCount(); ++edge_index) {
					std::vector<CPoint3d> edge_points;
					double edge_length = 0.0;
					if (surface->GetPreparedPolylinePoints(edge_index, edge_points)) {
						for (size_t point_index = 1;
							point_index < edge_points.size(); ++point_index) {
							edge_length += edge_points[point_index - 1].DistTo(
								&edge_points[point_index]);
						}
					}
					if (edge_index > 0)
						std::cout << ',';
					std::cout << edge_points.size() << '@' << edge_length;
				}
				std::cout << ']'
                          << " qty=" << surface->m_QtyU
                          << 'x' << surface->m_QtyV
                          << " initialized=" << surface->IsInitMesh
                          << " trimmed=" << surface->IsTrimmed
                          << " vertices=" << vertices
                          << " adaptiveResult=" << adaptive_net_result
                          << " triangles=" << triangles
                          << " quads=" << quads
                          << " minEdge=" << (std::isfinite(minimum_mesh_edge)
						? minimum_mesh_edge : 0.0)
                          << " maxEdge=" << maximum_mesh_edge
					  << " minEdgeAt=[" << minimum_edge_first.x << ','
					  << minimum_edge_first.y << ',' << minimum_edge_first.z
					  << "]-[" << minimum_edge_second.x << ','
					  << minimum_edge_second.y << ',' << minimum_edge_second.z << ']'
					  << " components=" << mesh_components
					  << " nonManifold=" << non_manifold_index_edges
					  << " overlappingEdges=" << overlapping_geometric_edges;
				std::cout << " occtOutside=" << occt_outside_faces;
                if (vertices > 0) {
                    std::cout << " meshBounds=["
                              << mesh_min.x << ',' << mesh_min.y << ',' << mesh_min.z
                              << "]-[" << mesh_max.x << ',' << mesh_max.y << ','
                              << mesh_max.z << ']';
                }
                if (!surface->GetLastIslandFillError().empty())
                    std::cout << " error=\""
                              << surface->GetLastIslandFillError() << '"';
                if (!surface->GetLastQuadrangulationDiagnostic().empty())
                    std::cout << " diagnostic=\""
                              << surface->GetLastQuadrangulationDiagnostic()
                              << '"';
				std::vector<std::unique_ptr<CPolyline>> island_boundaries;
				if (surface->CreateLastQuadrangulationBoundaryPolylines(
					island_boundaries)) {
					std::cout << " islandBoundaries=[";
					for (size_t boundary_index = 0;
						boundary_index < island_boundaries.size(); ++boundary_index) {
						if (boundary_index > 0)
							std::cout << ',';
						std::cout << island_boundaries[boundary_index]->GetPointCount();
					}
					std::cout << ']';
				}
                std::cout << '\n';
            }
			std::vector<Vec3> combined_vertices;
			std::vector<CMesh3D::Face> combined_faces;
			for (int surface_index = 0;
				surface_index < solid->GetNumSurfaces(); ++surface_index) {
				const CSurfaceFace* surface = solid->GetSurfaceFace(surface_index);
				if (!surface || !surface->pMesh3D)
					continue;
				const size_t offset = combined_vertices.size();
				combined_vertices.insert(combined_vertices.end(),
					surface->pMesh3D->GetVertices().begin(),
					surface->pMesh3D->GetVertices().end());
				for (CMesh3D::Face face : surface->pMesh3D->GetFaces()) {
					if (face.deleted || face.corners.size() < 3)
						continue;
					for (MeshCorner& corner : face.corners) {
						corner.v += offset;
						corner.uv = corner.v;
						corner.n = corner.v;
					}
					face.sourceFaceId = surface_index;
					combined_faces.push_back(std::move(face));
				}
			}
			CMesh3D combined("Diagnostic Low Poly");
			if (combined.SetGeometry(std::move(combined_vertices),
					std::move(combined_faces))) {
				size_t welded_vertices = 0;
				float weld_tolerance = 0.0f;
				std::unique_ptr<CMesh3D> welded = CMesh3D::CreateWelded(
					{&combined}, &welded_vertices, &weld_tolerance);
				if (welded) {
					bool manifold = true;
					const size_t boundary_loops = ClosedMeshBoundaryLoopCount(
						*welded, manifold);
					size_t boundary_edges = 0;
					std::map<std::pair<size_t, size_t>, size_t> edge_use;
					std::map<std::pair<size_t, size_t>, int> edge_source;
					std::map<int, size_t> boundary_edges_by_surface;
					std::vector<double> mesh_edge_lengths;
					for (const CMesh3D::Face& face : welded->GetFaces()) {
						if (face.deleted || face.corners.size() < 3)
							continue;
						for (size_t corner = 0; corner < face.corners.size(); ++corner) {
							const auto edge = std::minmax(face.corners[corner].v,
								face.corners[(corner + 1) % face.corners.size()].v);
							++edge_use[edge];
							edge_source[edge] = face.sourceFaceId;
							const Vec3 delta = welded->GetVertices()[edge.second]
								- welded->GetVertices()[edge.first];
							const double length = std::sqrt(
								static_cast<double>(dot(delta, delta)));
							if (length > 0.0)
								mesh_edge_lengths.push_back(length);
						}
					}
					for (const auto& edge : edge_use) {
						if (edge.second == 1) {
							++boundary_edges;
							++boundary_edges_by_surface[edge_source[edge.first]];
						}
					}
					std::cout << "  welded vertices="
						<< welded->GetVertices().size()
						<< " merged=" << welded_vertices
						<< " tolerance=" << weld_tolerance
						<< " boundaryEdges=" << boundary_edges
						<< " boundaryLoops=" << boundary_loops
						<< " manifold=" << manifold << " bySurface=[";
					for (const auto& item : boundary_edges_by_surface)
						std::cout << item.first << ':' << item.second << ',';
					std::cout << ']';
					std::sort(mesh_edge_lengths.begin(), mesh_edge_lengths.end());
					if (!mesh_edge_lengths.empty()) {
						std::cout << " edge[p10/p25/median]="
							<< mesh_edge_lengths[mesh_edge_lengths.size() / 10] << '/'
							<< mesh_edge_lengths[mesh_edge_lengths.size() / 4] << '/'
							<< mesh_edge_lengths[mesh_edge_lengths.size() / 2];
					}
					std::map<int, std::set<size_t>> boundary_vertices_by_surface;
					for (const auto& edge : edge_use) {
						if (edge.second != 1)
							continue;
						const int source = edge_source[edge.first];
						boundary_vertices_by_surface[source].insert(edge.first.first);
						boundary_vertices_by_surface[source].insert(edge.first.second);
					}
					std::vector<double> nearest_other_surface;
					for (const auto& source : boundary_vertices_by_surface) {
						for (size_t vertex : source.second) {
							double nearest = std::numeric_limits<double>::max();
							for (const auto& other : boundary_vertices_by_surface) {
								if (other.first == source.first)
									continue;
								for (size_t candidate : other.second) {
									const Vec3 delta = welded->GetVertices()[candidate]
										- welded->GetVertices()[vertex];
									nearest = std::min(nearest, std::sqrt(
										static_cast<double>(dot(delta, delta))));
								}
							}
							if (std::isfinite(nearest))
								nearest_other_surface.push_back(nearest);
						}
					}
					std::sort(nearest_other_surface.begin(),
						nearest_other_surface.end());
					if (!nearest_other_surface.empty()) {
						std::cout << " nearestOther[min/median/max]="
							<< nearest_other_surface.front() << '/'
							<< nearest_other_surface[
								nearest_other_surface.size() / 2] << '/'
							<< nearest_other_surface.back();
					}
					std::cout << '\n';
				}
			}
        }
        return EXIT_SUCCESS;
    }
    if (argc == 3
        && std::string(argv[1]) == "--quadrangulate-obj") {
        DiagnoseQuadrangulatorObj(argv[2]);
        return EXIT_SUCCESS;
    }
    if (argc == 2
        && std::string(argv[1]) == "--dense-notch-quadrangulation") {
        TestDenseNotchBoundaryQuadrangulation();
        return EXIT_SUCCESS;
    }
    if (argc == 2
        && std::string(argv[1]) == "--fusion-cylinder-front-guard") {
        TestFusionCylinderFrontGuard();
        return EXIT_SUCCESS;
    }
    if (argc == 2
        && std::string(argv[1]) == "--island-boundary-removal") {
        TestIslandBoundaryRemoval();
        return EXIT_SUCCESS;
    }
    if (argc == 2
        && std::string(argv[1]) == "--low-poly-quadro-regression") {
        TestLowPolyQuadroBranch();
        return EXIT_SUCCESS;
    }
    if (argc == 2
        && std::string(argv[1]) == "--island-shpagin-regression") {
        TestLowPolyQuadroBranch(true);
        return EXIT_SUCCESS;
    }
    if (argc == 2 && std::string(argv[1]) == "--sphere-move-regression") {
        TopoDS_Shape sphere_shape = BRepPrimAPI_MakeSphere(50.0).Shape();
        CSolid sphere(sphere_shape);
        require(sphere.ReBuldMesh(), "Initial sphere mesh was not built.");
        sphere.SetParametricOperation(
            0, "SolidSphereTool", "Ball", {{"diameter", 100.0}});

        const auto active_face_count = [](const CSolid& solid) {
            std::size_t count = 0;
            for (int surface_index = 0;
                 surface_index < solid.GetNumSurfaces(); ++surface_index) {
                const CSurfaceFace* surface =
                    solid.GetSurfaceFace(surface_index);
                if (!surface || !surface->pMesh3D)
                    continue;
                for (const CMesh3D::Face& face :
                     surface->pMesh3D->GetFaces()) {
                    if (!face.deleted)
                        ++count;
                }
            }
            return count;
        };

        const std::size_t faces_before = active_face_count(sphere);
        const CSurfaceFace* initial_surface = sphere.GetSurfaceFace(0);
        const int initial_mesh_type = initial_surface
            ? initial_surface->m_TypeMesh : -1;
        const bool initial_trimmed = initial_surface
            ? initial_surface->IsTrimmed : false;
        sphere.Translate({125.0f, -70.0f, 35.0f});
        const std::size_t faces_after = active_face_count(sphere);
        const CSurfaceFace* moved_surface = sphere.GetSurfaceFace(0);
        require(faces_before > 0 && faces_after == faces_before,
                "Translated sphere lost part of its regular mesh.");
        require(initial_mesh_type == REGULAR_MESH && !initial_trimmed
                    && moved_surface
                    && moved_surface->m_TypeMesh == REGULAR_MESH
                    && !moved_surface->IsTrimmed,
                "Complete translated sphere was classified as trimmed.");
        require(sphere.GetNumOperations() == 2
                    && sphere.GetOperation(1)
                    && sphere.GetOperation(1)->ToolId == "SolidTransform"
                    && sphere.GetOperation(1)->Name == "Move",
                "Translated sphere did not retain its Move operation.");
        return EXIT_SUCCESS;
    }
    if (argc == 3 && std::string(argv[1]) == "--benchmark-step") {
        StepIO step_io;
        std::vector<std::unique_ptr<CSolid>> solids;
        std::string error;
        const auto begin = std::chrono::steady_clock::now();
        require(step_io.Import(argv[2], solids, error), error.c_str());
        const auto end = std::chrono::steady_clock::now();
        std::size_t surfaces = 0;
        std::size_t triangles = 0;
        for (const auto& solid : solids) {
            surfaces += static_cast<std::size_t>(solid->GetNumSurfaces());
            for (int index = 0; index < solid->GetNumSurfaces(); ++index) {
                const CSurfaceFace* surface = solid->GetSurfaceFace(index);
                if (surface && surface->pMesh3D) {
                    triangles += surface->pMesh3D->GetFaces().size();
                }
            }
        }
        const double seconds = std::chrono::duration<double>(end - begin).count();
        std::cout << "STEP benchmark: " << seconds << " s, "
                  << solids.size() << " objects, " << surfaces << " surfaces, "
                  << triangles << " mesh faces\n";
        return EXIT_SUCCESS;
    }
    if (argc == 3 && std::string(argv[1]) == "--benchmark-project") {
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room;
        ProjectViewState view;
        QString error;
        const auto load_begin = std::chrono::steady_clock::now();
        require(serializer.Load(QString::fromLocal8Bit(argv[2]), document,
                                room, view, error),
                error.toLocal8Bit().constData());
        const auto load_end = std::chrono::steady_clock::now();
        std::size_t solids = 0;
        std::size_t surfaces = 0;
        std::size_t triangles = 0;
        const auto mesh_begin = std::chrono::steady_clock::now();
        for (const auto& object : document.GetObjects()) {
            const auto* solid = dynamic_cast<const CSolid*>(object.get());
            if (!solid) continue;
            ++solids;
            require(solid->EnsureRenderMesh(), "Could not restore project render mesh.");
            surfaces += static_cast<std::size_t>(solid->GetNumSurfaces());
            for (int index = 0; index < solid->GetNumSurfaces(); ++index) {
                const CSurfaceFace* surface = solid->GetSurfaceFace(index);
                if (surface && surface->pMesh3D) {
                    triangles += surface->pMesh3D->GetFaces().size();
                }
            }
        }
        const auto mesh_end = std::chrono::steady_clock::now();
        const auto snapshot_begin = std::chrono::steady_clock::now();
        const auto snapshot = document.CreateSnapshot();
        require(static_cast<bool>(snapshot),
                "Could not create project undo snapshot.");
        const auto snapshot_end = std::chrono::steady_clock::now();
        std::cout << "Project benchmark: load="
                  << std::chrono::duration<double>(load_end - load_begin).count()
                  << " s, render-mesh="
                  << std::chrono::duration<double>(mesh_end - mesh_begin).count()
                  << " s, undo-snapshot="
                  << std::chrono::duration<double>(snapshot_end - snapshot_begin).count()
                  << " s, " << document.GetObjects().size() << " objects, "
                  << solids << " solids, " << surfaces << " surfaces, "
                  << triangles << " triangles\n";
        return EXIT_SUCCESS;
    }

    // A section authored at the start of a sweep guide is already placed by
    // the user. Swept must retain its roll around the guide tangent instead
    // of replacing it with the automatically computed guide frame.
    CSmartLine authored_sweep_section("Authored sweep section");
    require(authored_sweep_section.SetCoordinateSystem(
                {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}, {1.0, 0.0, 0.0})
            && authored_sweep_section.Add(new CLinkLine(
                {-2.0, -3.0, 0.0}, {2.0, -3.0, 0.0}))
            && authored_sweep_section.Add(new CLinkLine(
                {2.0, -3.0, 0.0}, {2.0, 3.0, 0.0}))
            && authored_sweep_section.Add(new CLinkLine(
                {2.0, 3.0, 0.0}, {-2.0, 3.0, 0.0}))
            && authored_sweep_section.Add(new CLinkLine(
                {-2.0, 3.0, 0.0}, {-2.0, -3.0, 0.0}))
            && authored_sweep_section.SetClosed(true),
            "Could not create the authored sweep section fixture.");
    CSmartLine authored_sweep_guide("Authored sweep guide");
    require(authored_sweep_guide.SetCoordinateSystem(
                {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0})
            && authored_sweep_guide.Add(new CLinkLine(
                {0.0, 0.0, 0.0}, {100.0, 0.0, 0.0})),
            "Could not create the authored sweep guide fixture.");
    CSmartLine placed_authored_section("Placed authored section");
    require(BuildPlacedSweptSectionSketch(
                authored_sweep_section, authored_sweep_guide,
                placed_authored_section),
            "Swept did not accept a section authored at the guide start.");
    const SketchCoordinateSystem& authored_system =
        authored_sweep_section.GetCoordinateSystem();
    const SketchCoordinateSystem& placed_system =
        placed_authored_section.GetCoordinateSystem();
    require(std::abs(authored_system.x_axis.x - placed_system.x_axis.x) < 1.0e-9
                && std::abs(authored_system.x_axis.y - placed_system.x_axis.y) < 1.0e-9
                && std::abs(authored_system.x_axis.z - placed_system.x_axis.z) < 1.0e-9,
            "Swept rotated an already placed section at the guide start.");
    const TopoDS_Shape authored_sweep = BuildSweptSolidShape(
        authored_sweep_section, authored_sweep_guide, 1);
    require(!authored_sweep.IsNull(),
            "Swept could not build from an already placed section.");
    Bnd_Box authored_sweep_bounds;
    BRepBndLib::Add(authored_sweep, authored_sweep_bounds);
    double authored_x_min, authored_y_min, authored_z_min;
    double authored_x_max, authored_y_max, authored_z_max;
    authored_sweep_bounds.Get(
        authored_x_min, authored_y_min, authored_z_min,
        authored_x_max, authored_y_max, authored_z_max);
    require(authored_y_max - authored_y_min
                > authored_z_max - authored_z_min + 1.0,
            "Swept changed the authored section roll in the resulting solid.");

    CSmartLine remote_sweep_section = authored_sweep_section.MakeCopy();
    remote_sweep_section.Translate({0.0f, 40.0f, 25.0f});
    CSmartLine automatically_placed_section("Automatically placed section");
    require(BuildPlacedSweptSectionSketch(
                remote_sweep_section, authored_sweep_guide,
                automatically_placed_section),
            "Swept did not automatically place a remote section.");
    const SketchCoordinateSystem& automatic_system =
        automatically_placed_section.GetCoordinateSystem();
    require(std::abs(automatic_system.origin.x) < 1.0e-9
                && std::abs(automatic_system.origin.y) < 1.0e-9
                && std::abs(automatic_system.origin.z) < 1.0e-9
                && std::abs(std::abs(automatic_system.normal.x) - 1.0) < 1.0e-9,
            "Swept did not move a remote section to the guide start normal.");

    // Furniture code can sweep a local sketch directly along the legacy
    // CSplineCurve returned by CConic without exposing OCCT conversion details.
    auto wrapped_profile = std::make_unique<CSmartLine>(
        "Wrapped sweep profile");
    require(wrapped_profile->Add(new CLinkLine(
                {0.0, 0.0, 0.0}, {6.0, 0.0, 0.0}))
            && wrapped_profile->Add(new CLinkLine(
                {6.0, 0.0, 0.0}, {6.0, 4.0, 0.0}))
            && wrapped_profile->Add(new CLinkLine(
                {6.0, 4.0, 0.0}, {0.0, 4.0, 0.0}))
            && wrapped_profile->Add(new CLinkLine(
                {0.0, 4.0, 0.0}, {0.0, 0.0, 0.0}))
            && wrapped_profile->SetClosed(true)
            && wrapped_profile->SetCoordinateSystem(
                CPoint3d(0.0, 0.0, 0.0),
                CPoint3d(1.0, 0.0, 0.0),
                CPoint3d(0.0, 0.0, 1.0)),
            "Could not create the wrapped sweep profile fixture.");
    std::string wrapped_extrude_error;
    const CVector wrapped_extrude_direction(0.0, 0.0, 2.0);
    const TopoDS_Shape wrapped_extrude =
        CFacadeFurniture::BuildExtrude(
            wrapped_profile.get(), wrapped_extrude_direction,
            25.0, &wrapped_extrude_error);
    require(!wrapped_extrude.IsNull(),
            ("BuildExtrude failed: "
             + wrapped_extrude_error).c_str());
    Bnd_Box wrapped_extrude_bounds;
    BRepBndLib::Add(wrapped_extrude, wrapped_extrude_bounds);
    double extrude_x_min, extrude_y_min, extrude_z_min;
    double extrude_x_max, extrude_y_max, extrude_z_max;
    wrapped_extrude_bounds.Get(
        extrude_x_min, extrude_y_min, extrude_z_min,
        extrude_x_max, extrude_y_max, extrude_z_max);
    require(std::abs(extrude_z_min) < 1.0e-3
                && std::abs(extrude_z_max - 25.0) < 1.0e-3,
            "BuildExtrude did not normalize its direction.");
    const CVector zero_extrude_direction(0.0, 0.0, 0.0);
    const TopoDS_Shape rejected_extrude =
        CFacadeFurniture::BuildExtrude(
            wrapped_profile.get(), zero_extrude_direction,
            25.0, &wrapped_extrude_error);
    require(rejected_extrude.IsNull()
                && !wrapped_extrude_error.empty(),
            "BuildExtrude did not reject a zero direction.");

    TopoDS_Shape transform_source =
        BRepPrimAPI_MakeBox(10.0, 20.0, 30.0).Shape();
    TopoDS_Shape moved_shape = CSolid::CopyShape(transform_source);
    require(!moved_shape.IsNull(), "CopyShape failed.");
    require(CSolid::MoveShape(
                moved_shape, CVector(2.0, 0.0, 0.0), 5.0),
            "MoveShape failed.");
    Bnd_Box moved_bounds;
    BRepBndLib::Add(moved_shape, moved_bounds);
    double moved_x_min, moved_y_min, moved_z_min;
    double moved_x_max, moved_y_max, moved_z_max;
    moved_bounds.Get(
        moved_x_min, moved_y_min, moved_z_min,
        moved_x_max, moved_y_max, moved_z_max);
    require(std::abs(moved_x_min - 5.0) < 1.0e-3
                && std::abs(moved_x_max - 15.0) < 1.0e-3,
            "MoveShape did not normalize its direction.");

    TopoDS_Shape rotated_shape = CSolid::CopyShape(transform_source);
    require(CSolid::RotateShape(
                rotated_shape, CPoint3d(0.0, 0.0, 0.0),
                CVector(0.0, 0.0, 1.0),
                3.14159265358979323846 * 0.5),
            "RotateShape failed.");
    Bnd_Box rotated_bounds;
    BRepBndLib::Add(rotated_shape, rotated_bounds);
    double rotated_x_min, rotated_y_min, rotated_z_min;
    double rotated_x_max, rotated_y_max, rotated_z_max;
    rotated_bounds.Get(
        rotated_x_min, rotated_y_min, rotated_z_min,
        rotated_x_max, rotated_y_max, rotated_z_max);
    require(std::abs(rotated_x_min + 20.0) < 1.0e-3
                && std::abs(rotated_x_max) < 1.0e-3
                && std::abs(rotated_y_min) < 1.0e-3
                && std::abs(rotated_y_max - 10.0) < 1.0e-3,
            "RotateShape did not rotate around the requested axis.");

    require(CSolid::MirrorShape(
                moved_shape, CPlane(1.0, 0.0, 0.0, 0.0)),
            "MirrorShape failed.");
    Bnd_Box mirrored_bounds;
    BRepBndLib::Add(moved_shape, mirrored_bounds);
    double mirrored_x_min, mirrored_y_min, mirrored_z_min;
    double mirrored_x_max, mirrored_y_max, mirrored_z_max;
    mirrored_bounds.Get(
        mirrored_x_min, mirrored_y_min, mirrored_z_min,
        mirrored_x_max, mirrored_y_max, mirrored_z_max);
    require(std::abs(mirrored_x_min + 15.0) < 1.0e-3
                && std::abs(mirrored_x_max + 5.0) < 1.0e-3,
            "MirrorShape did not use the CPlane equation.");

    TopoDS_Shape rejected_transform = CSolid::CopyShape(transform_source);
    require(!CSolid::RotateShape(
                rejected_transform, CPoint3d(0.0, 0.0, 0.0),
                CVector(0.0, 0.0, 0.0), 1.0),
            "RotateShape did not reject a zero axis.");
    CPoint3d wrapped_a(0.0, 0.0, 0.0);
    CPoint3d wrapped_b(0.0, 80.0, 0.0);
    CPoint3d wrapped_c(120.0, 80.0, 0.0);
    CConic wrapped_conic(
        &wrapped_a, &wrapped_b, &wrapped_c, 0.4142);
    std::unique_ptr<CSplineCurve> wrapped_guide(
        wrapped_conic.MakeSpline(20));
    std::string wrapped_sweep_error;
    std::unique_ptr<CSmartLine> exactly_placed_profile =
        CFacadeFurniture::PlaceSweptProfile(
            wrapped_profile.get(), wrapped_guide.get(),
            0.0, 0.0, 0.0, &wrapped_sweep_error);
    require(exactly_placed_profile != nullptr,
            ("PlaceSweptProfile failed: " + wrapped_sweep_error).c_str());
    CPoint7d wrapped_start{};
    require(wrapped_guide->GetPoint7d(0.0, &wrapped_start),
            "Could not evaluate the wrapped guide start fixture.");
    const SketchCoordinateSystem& legacy_placed_system =
        exactly_placed_profile->GetCoordinateSystem();
    const double tangent_alignment =
        legacy_placed_system.normal.x * wrapped_start.l
        + legacy_placed_system.normal.y * wrapped_start.m
        + legacy_placed_system.normal.z * wrapped_start.n;
    require(std::abs(legacy_placed_system.origin.x - wrapped_start.x) < 1.0e-9
                && std::abs(legacy_placed_system.origin.y - wrapped_start.y) < 1.0e-9
                && std::abs(legacy_placed_system.origin.z - wrapped_start.z) < 1.0e-9
                && tangent_alignment > 1.0 - 1.0e-9,
            "Placed profile does not use the exact legacy guide start frame.");
    for (const double angle : {43.0, 45.0, 180.0}) {
        const TopoDS_Shape wrapped_sweep =
            CFacadeFurniture::BuildSweptProfile(
                wrapped_profile.get(), wrapped_guide.get(),
                angle, 0.0, 0.0, &wrapped_sweep_error);
        const std::string wrapped_sweep_message =
            "BuildSweptProfile failed at " + std::to_string(angle)
            + " degrees: " + wrapped_sweep_error;
        require(!wrapped_sweep.IsNull(),
                wrapped_sweep_message.c_str());
    }

    // Reproduce the Radius-3 document workflow: a Milano section is first
    // placed at Guide2, both objects are added to the document, and the user
    // then invokes the general Solid Swept command with Angle = 0.
    CPoint3d radius3_a(-300.0, 250.0, 0.0);
    CPoint3d radius3_b(-300.0, -250.0, 0.0);
    CPoint3d radius3_c(300.0, -250.0, 0.0);
    CConic radius3_conic(
        &radius3_a, &radius3_b, &radius3_c, 0.4142);
    std::unique_ptr<CSplineCurve> radius3_legacy_guide(
        radius3_conic.MakeSpline(10));
    require(radius3_legacy_guide != nullptr,
            "Could not create the Radius-3 legacy guide fixture.");
    std::unique_ptr<CBSpline> radius3_document_guide =
        CFacadeFurniture::CreateNurbsGuide(
            radius3_legacy_guide.get(), &wrapped_sweep_error);
    require(radius3_document_guide != nullptr,
            ("Could not convert Radius-3 Guide2 to NURBS: "
             + wrapped_sweep_error).c_str());
    double radius3_nurbs_max_error = 0.0;
    for (int sample = 0; sample <= 100; ++sample) {
        const double normalized = static_cast<double>(sample) / 100.0;
        CPoint3d legacy_point;
        require(radius3_legacy_guide->GetPoint(
                    normalized * (radius3_legacy_guide->np() - 1),
                    &legacy_point),
                "Could not evaluate the legacy Radius-3 guide.");
        CPoint3d nurbs_point = radius3_document_guide->Evaluate(
            static_cast<float>(normalized));
        const double error = legacy_point.DistTo(&nurbs_point);
        if (error > radius3_nurbs_max_error) {
            radius3_nurbs_max_error = error;
        }
    }
    // CBSpline's public display evaluator takes float, so comparison at the
    // 600 mm scale is limited to approximately 1e-4. OCCT receives the same
    // NURBS poles and knots directly in double precision.
    require(radius3_nurbs_max_error < 1.0e-4,
            "The NURBS guide does not reproduce CSplineCurve exactly.");
    std::unique_ptr<CSmartLine> radius3_milano =
        CFacadeFurniture::CreateMilanoProfile(60.0);
    std::unique_ptr<CSmartLine> radius3_placed_milano =
        CFacadeFurniture::PlaceSweptProfile(
            radius3_milano.get(), radius3_legacy_guide.get(),
            -90.0, 0.0, 0.0, &wrapped_sweep_error);
    require(radius3_placed_milano != nullptr,
            ("Could not place Radius-3 Milano: "
             + wrapped_sweep_error).c_str());
    const TopoDS_Shape radius3_document_sweep = BuildSweptSolidShape(
        *radius3_placed_milano, *radius3_document_guide,
        1, 0.0, 0.0, 0.0);
    require(!radius3_document_sweep.IsNull(),
            "General Solid Swept rejected the placed Radius-3 Milano.");
    Bnd_Box radius3_sweep_bounds;
    BRepBndLib::Add(radius3_document_sweep, radius3_sweep_bounds);
    double radius3_x_min, radius3_y_min, radius3_z_min;
    double radius3_x_max, radius3_y_max, radius3_z_max;
    radius3_sweep_bounds.Get(
        radius3_x_min, radius3_y_min, radius3_z_min,
        radius3_x_max, radius3_y_max, radius3_z_max);
    GProp_GProps radius3_volume_properties;
    BRepGProp::VolumeProperties(
        radius3_document_sweep, radius3_volume_properties);
    int radius3_face_count = 0;
    for (TopExp_Explorer face(
             radius3_document_sweep, TopAbs_FACE);
         face.More(); face.Next()) ++radius3_face_count;
    require(radius3_x_min > -500.0 && radius3_x_max < 500.0
                && radius3_y_min > -450.0 && radius3_y_max < 450.0
                && radius3_z_min > -100.0 && radius3_z_max < 100.0,
            "General Solid Swept created remote Radius-3 faces.");
    TopoDS_Shape radius3_render_shape = radius3_document_sweep;
    CSolid radius3_rendered_sweep(radius3_render_shape);
    require(radius3_rendered_sweep.ReBuldMesh(),
            "Could not triangulate the Radius-3 Swept Solid.");
    double radius3_max_mesh_edge = 0.0;
    for (int surface_index = 0;
         surface_index < radius3_rendered_sweep.GetNumSurfaces();
         ++surface_index) {
        const CSurfaceFace* surface =
            radius3_rendered_sweep.GetSurfaceFace(surface_index);
        if (!surface || !surface->pMesh3D) continue;
        const std::vector<Vec3>& vertices =
            surface->pMesh3D->GetVertices();
        for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
            const size_t vertex_count = CMesh3D::FaceVertexCount(face);
            for (size_t edge = 0; edge < vertex_count; ++edge) {
                const size_t first = CMesh3D::GetFaceVertexIndex(face, edge);
                const size_t second = CMesh3D::GetFaceVertexIndex(
                    face, (edge + 1) % vertex_count);
                if (first >= vertices.size() || second >= vertices.size()) {
                    continue;
                }
                const Vec3 delta = vertices[first] - vertices[second];
                const double edge_length = std::sqrt(
                    static_cast<double>(delta.x) * delta.x
                    + static_cast<double>(delta.y) * delta.y
                    + static_cast<double>(delta.z) * delta.z);
                radius3_max_mesh_edge = std::max(
                    radius3_max_mesh_edge, edge_length);
            }
        }
    }
    require(radius3_max_mesh_edge < 150.0,
            "Radius-3 Swept Solid render mesh contains triangle fans.");
    const TopoDS_Shape rejected_wrapped_sweep =
        CFacadeFurniture::BuildSweptProfile(
            nullptr, wrapped_guide.get(), 0.0, 0.0, 0.0,
            &wrapped_sweep_error);
    require(rejected_wrapped_sweep.IsNull()
                && !wrapped_sweep_error.empty(),
            "BuildSweptProfile did not report an invalid input.");

    // Sheet-metal bend: a line directed along +Y bends the +X side. Reversing
    // the line or toggling direction reverses the signed rotation.
    const TopoDS_Shape flat_sheet =
        BRepPrimAPI_MakeBox(gp_Pnt(0.0, 0.0, 0.0), 100.0, 60.0, 4.0).Shape();
    SheetBendParameters bend_parameters;
    bend_parameters.line_start_x = 50.0;
    bend_parameters.line_start_y = 0.0;
    bend_parameters.line_start_z = 4.0;
    bend_parameters.line_end_x = 50.0;
    bend_parameters.line_end_y = 60.0;
    bend_parameters.line_end_z = 4.0;
    bend_parameters.inner_radius = 2.0;
    bend_parameters.angle_degrees = 90.0;
    bend_parameters.clockwise = true;
    TopoDS_Shape bent_sheet;
    std::string bend_error;
    require(BuildSheetBendShape(
                flat_sheet, bend_parameters, bent_sheet, bend_error),
            bend_error.c_str());
    require(!bent_sheet.IsNull(), "Sheet Bend returned a null shape.");
    bool has_cylindrical_bend_face = false;
    for (TopExp_Explorer face_explorer(bent_sheet, TopAbs_FACE);
         face_explorer.More(); face_explorer.Next()) {
        if (BRepAdaptor_Surface(
                TopoDS::Face(face_explorer.Current()), true).GetType()
            == GeomAbs_Cylinder) {
            has_cylindrical_bend_face = true;
            break;
        }
    }
    require(has_cylindrical_bend_face,
            "Sheet Bend must create an analytic cylindrical face.");
    Bnd_Box bent_bounds;
    BRepBndLib::Add(bent_sheet, bent_bounds);
    double bend_x_min, bend_y_min, bend_z_min;
    double bend_x_max, bend_y_max, bend_z_max;
    bent_bounds.Get(bend_x_min, bend_y_min, bend_z_min,
                    bend_x_max, bend_y_max, bend_z_max);
    require(bend_z_max - bend_z_min > 40.0,
            "A 90-degree sheet bend did not raise the moving side.");
    GProp_GProps flat_properties;
    GProp_GProps bent_properties;
    BRepGProp::VolumeProperties(flat_sheet, flat_properties);
    BRepGProp::VolumeProperties(bent_sheet, bent_properties);
    require(std::abs(flat_properties.Mass() - bent_properties.Mass())
                / flat_properties.Mass() < 0.02,
            "Sheet Bend did not preserve sheet volume.");

    SheetBendParameters opposite_bend_parameters = bend_parameters;
    opposite_bend_parameters.clockwise = false;
    TopoDS_Shape opposite_bent_sheet;
    require(BuildSheetBendShape(flat_sheet, opposite_bend_parameters,
                                opposite_bent_sheet, bend_error),
            bend_error.c_str());
    const TopoDS_Shape fixed_side_probe =
        BRepPrimAPI_MakeBox(gp_Pnt(0.0, 0.0, 0.0), 49.0, 60.0, 4.0).Shape();
    BRepAlgoAPI_Common clockwise_fixed_probe(
        bent_sheet, fixed_side_probe);
    BRepAlgoAPI_Common counterclockwise_fixed_probe(
        opposite_bent_sheet, fixed_side_probe);
    clockwise_fixed_probe.Build();
    counterclockwise_fixed_probe.Build();
    GProp_GProps clockwise_fixed_properties;
    GProp_GProps counterclockwise_fixed_properties;
    BRepGProp::VolumeProperties(
        clockwise_fixed_probe.Shape(), clockwise_fixed_properties);
    BRepGProp::VolumeProperties(
        counterclockwise_fixed_probe.Shape(), counterclockwise_fixed_properties);
    const double expected_fixed_volume = 49.0 * 60.0 * 4.0;
    require(clockwise_fixed_properties.Mass() > expected_fixed_volume * 0.99
                && counterclockwise_fixed_properties.Mass()
                    > expected_fixed_volume * 0.99,
            "Bend direction changed which side of the sheet moves.");

    // The creation line is only an input gesture. The saved operation owns
    // two points and can rebuild after that line has been deleted.
    TopoDS_Shape parametric_bent_shape = bent_sheet;
    auto parametric_bend_solid =
        std::make_unique<CSolid>(parametric_bent_shape);
    TopoDS_Shape frozen_flat_shape = flat_sheet;
    CSolid frozen_flat_solid(frozen_flat_shape);
    const size_t frozen_base_index =
        parametric_bend_solid->AddBooleanToolCopy(frozen_flat_solid);
    parametric_bend_solid->SetParametricOperation(
        0, "SolidSheetBend", "Sheet Bend",
        {{"point1.x", 50.0}, {"point1.y", 0.0}, {"point1.z", 4.0},
         {"point2.x", 50.0}, {"point2.y", 60.0}, {"point2.z", 4.0},
         {"radius", 2.0}, {"angle", 90.0}, {"direction", 0.0},
         {"base.tool.index", static_cast<double>(frozen_base_index)}});
    CAlfaDoc parametric_bend_document;
    parametric_bend_document.AddObject(std::move(parametric_bend_solid));
    const size_t parametric_bend_index =
        parametric_bend_document.GetObjects().size() - 1;
    ToolRegistry bend_tools;
    ActiveParametricObject bend_edit = bend_tools.ActiveObjectFromDocument(
        parametric_bend_index,
        *parametric_bend_document.GetObjects()[parametric_bend_index], 0,
        &parametric_bend_document);
    require(bend_edit.tool_id == "SolidSheetBend"
                && bend_edit.parameters.size() == 3,
            "Sheet Bend parameters are not available for editing.");
    for (ToolParameter& parameter : bend_edit.parameters) {
        if (parameter.id == "angle") parameter.value = 45.0;
    }
    bend_tools.Rebuild(bend_edit, parametric_bend_document);
    const auto* rebuilt_bend = dynamic_cast<const CSolid*>(
        parametric_bend_document.GetObjects()[parametric_bend_index].get());
    const ParametricFunction* rebuilt_bend_operation =
        rebuilt_bend ? rebuilt_bend->GetOperation(0) : nullptr;
    const auto bend_saved_value = [](const std::vector<ParametricParameterValue>& values,
                                     const char* id, double fallback) {
        for (const ParametricParameterValue& value : values) {
            if (value.id == id) return value.value;
        }
        return fallback;
    };
    require(rebuilt_bend && rebuilt_bend_operation
                && std::abs(bend_saved_value(
                    rebuilt_bend_operation->Parameters,
                    "point1.x", -1.0) - 50.0) < 1.0e-9
                && std::abs(bend_saved_value(
                    rebuilt_bend_operation->Parameters,
                    "point2.y", -1.0) - 60.0) < 1.0e-9
                && std::abs(bend_saved_value(
                    rebuilt_bend_operation->Parameters,
                    "angle", -1.0) - 45.0) < 1.0e-9,
            "Sheet Bend lost its independent points during rebuild.");

    // A local bend line only has to cross the flange at the bend. It must not
    // be rejected because another arm of the same sheet is wider globally.
    BRepAlgoAPI_Fuse cross_fuse(
        BRepPrimAPI_MakeBox(gp_Pnt(0.0, 40.0, 0.0), 120.0, 20.0, 4.0).Shape(),
        BRepPrimAPI_MakeBox(gp_Pnt(40.0, 0.0, 0.0), 40.0, 100.0, 4.0).Shape());
    cross_fuse.Build();
    require(cross_fuse.IsDone(), "Cross-shaped sheet was not constructed.");
    SheetBendParameters local_bend = bend_parameters;
    local_bend.line_start_x = 100.0;
    local_bend.line_start_y = 30.0;
    local_bend.line_end_x = 100.0;
    local_bend.line_end_y = 70.0;
    TopoDS_Shape locally_bent_sheet;
    require(BuildSheetBendShape(cross_fuse.Shape(), local_bend,
                                locally_bent_sheet, bend_error),
            bend_error.c_str());
    require(!locally_bent_sheet.IsNull(),
            "Local Sheet Bend returned a null shape.");
    GProp_GProps cross_flat_properties;
    GProp_GProps cross_bent_properties;
    BRepGProp::VolumeProperties(cross_fuse.Shape(), cross_flat_properties);
    BRepGProp::VolumeProperties(locally_bent_sheet, cross_bent_properties);
    require(std::abs(cross_flat_properties.Mass()
                     - cross_bent_properties.Mass())
                / cross_flat_properties.Mass() < 0.02,
            "Local Sheet Bend protruded beyond the sheet contour.");

    QTemporaryDir directory;
    require(directory.isValid(), "Temporary catalog directory was not created.");

    const QString cycles_script = BlenderCyclesRenderer::PythonScript();
    const qsizetype vertical_fit = cycles_script.indexOf(
        "camera_object_data.sensor_fit = 'VERTICAL'");
    const qsizetype orthographic_branch = cycles_script.indexOf(
        "if camera_data.get('orthographic', False):");
    require(vertical_fit >= 0 && orthographic_branch > vertical_fit,
            "Blender camera must use vertical fit for an exact Dom3D viewport match.");
    require(cycles_script.contains("Dom3D Interior Light-1")
                && cycles_script.contains("Dom3D Interior Light-2")
                && cycles_script.contains("Dom3D Interior Light-3")
                && !cycles_script.contains("Dom3D Window Daylight")
                && !cycles_script.contains("Dom3D Interior Camera Fill"),
            "Interior preset must create the adaptive three-light rig.");
    require(cycles_script.contains("def add_custom_lights")
                && cycles_script.contains("light_mode', 'AUTO') == 'CUSTOMIZE'")
                && cycles_script.contains("Shadow Fill")
                && cycles_script.contains("else 1000.0"),
            "Cycles custom light mode must preserve legacy light controls.");
    require(cycles_script.contains("legacy_reflection")
                && cycles_script.contains("legacy_shininess")
                && cycles_script.contains("def srgb_to_linear")
                && cycles_script.contains(
                    "color = srgb_to_linear(data.get('base_color'")
                && cycles_script.contains("metallic_value = pbr_metallic")
                && !cycles_script.contains("legacy_metallic")
                && cycles_script.contains("Transmission Weight")
                && cycles_script.contains("Is Glossy Ray")
                && cycles_script.contains("reflection_background")
                && cycles_script.contains("standalone_reflections")
                && cycles_script.contains("Dom3D Rim Softbox")
                && cycles_script.contains(
                    "light_object.visible_glossy = reflection_card")
                && cycles_script.contains("independent_surface_patch")
                && cycles_script.contains("use the exported CAD vertex normals"),
            "Cycles must linearize Dom3D colours, preserve legacy dielectric "
            "materials and keep HDRI/studio cards visible to reflection rays.");
    require(cycles_script.count("add_area('Dom3D Interior Light-") == 3
                && cycles_script.contains(
                    "flat_shaded = item['name'].casefold().startswith('room ')")
                && cycles_script.contains(
                    "center.x - size_x * 0.28")
                && !cycles_script.contains("DOM3D_INTERIOR_OPEN_WALL")
                && !cycles_script.contains("obj.hide_render = True"),
            "Interior render must keep room walls and use three soft sources.");

    // Minimal end-to-end external-render scene: camera basis, metric scale,
    // clear coat, Unicode paths and an actual background Blender invocation.
    RenderScene cycles_scene;
    Material cycles_material;
    cycles_material.name = "Red lacquer";
    cycles_material.diffuse = {0.65f, 0.01f, 0.01f};
    cycles_material.roughness = 0.3f;
    cycles_material.coat_weight = 1.0f;
    cycles_material.coat_roughness = 0.03f;
    cycles_material.specular = 0.8f;
    cycles_material.shininess = 120.0f;
    cycles_material.reflectivity = 0.65f;
    cycles_scene.materials.push_back(cycles_material);
    RenderMesh cycles_mesh;
    cycles_mesh.name = "Lacquer triangle";
    cycles_mesh.material_index = 0;
    cycles_mesh.independent_surface_patch = true;
    cycles_mesh.vertices = {
        {-500.0f, -400.0f, 0.0f},
        {500.0f, -400.0f, 0.0f},
        {0.0f, 500.0f, 0.0f}};
    RenderTriangle cycles_triangle;
    cycles_triangle.vertices = {0, 1, 2};
    cycles_triangle.uvs = {UV{0.0f, 0.0f}, UV{1.0f, 0.0f}, UV{0.5f, 1.0f}};
    cycles_triangle.normals = {
        Vec3{0.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f},
        Vec3{0.0f, 0.0f, 1.0f}};
    cycles_mesh.triangles.push_back(cycles_triangle);
    cycles_scene.meshes.push_back(cycles_mesh);
    cycles_scene.camera.position = {0.0f, 0.0f, 2000.0f};
    cycles_scene.camera.forward = {0.0f, 0.0f, -1.0f};
    cycles_scene.camera.right = {1.0f, 0.0f, 0.0f};
    cycles_scene.camera.up = {0.0f, 1.0f, 0.0f};
    cycles_scene.environment.enabled = false;

    // The restored Dom3D tracer must produce exactly the same pixels when its
    // rows are distributed across worker threads.  This catches accidental
    // shared recursion/statistics state in the legacy core.
    RenderScene native_scene = cycles_scene;
    native_scene.meshes.clear();
    native_scene.materials.clear();
    for (int index = 0; index < 6; ++index) {
        QImage texture(4, 4, QImage::Format_RGB32);
        texture.fill(QColor::fromHsv(index * 45, 220, 220));
        const QString texture_path = directory.filePath(
            QString("native-texture-%1.png").arg(index));
        require(texture.save(texture_path),
                "Native raytrace test texture could not be saved.");
        Material material;
        material.name = QString("Native texture %1").arg(index).toStdString();
        material.ambient = {1.0f, 1.0f, 1.0f};
        material.diffuse = {1.0f, 1.0f, 1.0f};
        material.color_texture_path = texture_path.toStdString();
        native_scene.materials.push_back(material);

        RenderMesh mesh = cycles_mesh;
        mesh.name = QString("Textured triangle %1").arg(index);
        mesh.material_index = index;
        const float left = -540.0f + index * 180.0f;
        mesh.vertices = {
            {left, -350.0f, 0.0f}, {left + 150.0f, -350.0f, 0.0f},
            {left + 75.0f, 350.0f, 0.0f}};
        native_scene.meshes.push_back(std::move(mesh));
    }
    // Reuse the first texture after all others: this exposed stale cached
    // pointers when the legacy contiguous texture array was reallocated.
    RenderMesh reused_texture_mesh = native_scene.meshes.front();
    reused_texture_mesh.name = "Reused first texture";
    reused_texture_mesh.vertices = {
        {-100.0f, 360.0f, 10.0f}, {100.0f, 360.0f, 10.0f},
        {0.0f, 520.0f, 10.0f}};
    native_scene.meshes.push_back(std::move(reused_texture_mesh));

    NativeRaytraceSettings native_settings;
    native_settings.width = 112;
    native_settings.height = 64;
    native_settings.reflection_depth = 2;
    native_settings.anti_alias_level = 1;
    native_settings.progressive_passes = 1;
    native_settings.light_count = 1;
    native_settings.thread_count = 1;
    QImage native_single;
    QString native_error;
    bool native_live_preview = false;
    int native_last_progress = -1;
    require(NativeRaytraceRenderer::Render(
                native_scene, native_settings, &native_single, &native_error,
                nullptr,
                [&](int percent, const QImage& preview, const QString&) {
                    require(percent >= native_last_progress && percent <= 100,
                            "Native render progress must be monotonic.");
                    native_last_progress = percent;
                    if (percent < 100 && !preview.isNull()) {
                        native_live_preview = true;
                    }
                })
                && native_single.size() == QSize(112, 64),
            native_error.toUtf8().constData());
    require(native_live_preview && native_last_progress == 100,
            "Native raytrace must publish a live preview and finish at 100%.");
    native_settings.thread_count = 4;
    QImage native_multi;
    require(NativeRaytraceRenderer::Render(
                native_scene, native_settings, &native_multi, &native_error),
            native_error.toUtf8().constData());
    require(native_single == native_multi,
            "Native raytrace output changed when multithreading was enabled.");
    const QRgb background = native_single.pixel(0, 0);
    bool native_geometry_visible = false;
    for (int y = 0; y < native_single.height() && !native_geometry_visible; ++y) {
        for (int x = 0; x < native_single.width(); ++x) {
            if (native_single.pixel(x, y) != background) {
                native_geometry_visible = true;
                break;
            }
        }
    }
    require(native_geometry_visible,
            "Native raytrace smoke scene contains only the background.");

    // PBR metallic must be converted to the legacy RT_REFLECT channel.  Aim a
    // 45-degree mirror at a red card which is outside the camera view; the
    // centre pixel can become red only through a secondary reflection ray.
    RenderScene mirror_scene;
    Material mirror_material;
    mirror_material.name = "PBR mirror";
    mirror_material.diffuse = {1.0f, 1.0f, 1.0f};
    mirror_material.ambient = {0.0f, 0.0f, 0.0f};
    mirror_material.specular = 0.0f;
    mirror_material.metallic = 1.0f;
    mirror_material.roughness = 0.04f;
    mirror_scene.materials.push_back(mirror_material);
    Material red_material;
    red_material.name = "Reflected red card";
    red_material.diffuse = {1.0f, 0.0f, 0.0f};
    red_material.ambient = {1.0f, 0.0f, 0.0f};
    mirror_scene.materials.push_back(red_material);

    RenderMesh mirror_mesh;
    mirror_mesh.name = "Angled PBR mirror";
    mirror_mesh.material_index = 0;
    mirror_mesh.vertices = {
        {-250.0f, -350.0f, 250.0f}, {250.0f, -350.0f, -250.0f},
        {250.0f, 350.0f, -250.0f}, {-250.0f, 350.0f, 250.0f}};
    const Vec3 mirror_normal{0.70710678f, 0.0f, 0.70710678f};
    RenderTriangle mirror_a;
    mirror_a.vertices = {0, 1, 2};
    mirror_a.normals = {mirror_normal, mirror_normal, mirror_normal};
    RenderTriangle mirror_b;
    mirror_b.vertices = {0, 2, 3};
    mirror_b.normals = {mirror_normal, mirror_normal, mirror_normal};
    mirror_mesh.triangles = {mirror_a, mirror_b};
    mirror_scene.meshes.push_back(mirror_mesh);

    RenderMesh red_card;
    red_card.name = "Red reflection target";
    red_card.material_index = 1;
    red_card.vertices = {
        {500.0f, -500.0f, -500.0f}, {500.0f, 500.0f, -500.0f},
        {500.0f, 500.0f, 500.0f}, {500.0f, -500.0f, 500.0f}};
    const Vec3 card_normal{-1.0f, 0.0f, 0.0f};
    RenderTriangle card_a;
    card_a.vertices = {0, 1, 2};
    card_a.normals = {card_normal, card_normal, card_normal};
    RenderTriangle card_b;
    card_b.vertices = {0, 2, 3};
    card_b.normals = {card_normal, card_normal, card_normal};
    red_card.triangles = {card_a, card_b};
    mirror_scene.meshes.push_back(red_card);
    mirror_scene.camera.position = {0.0f, 0.0f, 1000.0f};
    mirror_scene.camera.forward = {0.0f, 0.0f, -1.0f};
    mirror_scene.camera.right = {1.0f, 0.0f, 0.0f};
    mirror_scene.camera.up = {0.0f, 1.0f, 0.0f};
    mirror_scene.environment.background_color = {0.0f, 0.0f, 0.0f};
    NativeRaytraceSettings mirror_settings = native_settings;
    mirror_settings.width = 65;
    mirror_settings.height = 65;
    mirror_settings.light_strength = 0.0;
    mirror_settings.ambient_strength = 1.0;
    mirror_settings.exposure_ev = 0.0;
    QImage mirror_image;
    require(NativeRaytraceRenderer::Render(
                mirror_scene, mirror_settings, &mirror_image, &native_error),
            native_error.toUtf8().constData());
    const QColor reflected_pixel(mirror_image.pixel(32, 32));
    require(reflected_pixel.red() > 80
                && reflected_pixel.red() > reflected_pixel.green() * 3,
            "PBR metallic material did not produce a secondary reflection.");
    mirror_scene.materials[0].metallic = 0.0f;
    mirror_scene.materials[0].specular = 1.0f;
    mirror_scene.materials[0].roughness = 0.04f;
    QImage dielectric_mirror_image;
    require(NativeRaytraceRenderer::Render(
                mirror_scene, mirror_settings, &dielectric_mirror_image,
                &native_error),
            native_error.toUtf8().constData());
    const QColor dielectric_reflection(
        dielectric_mirror_image.pixel(32, 32));
    require(dielectric_reflection.red() > 40
                && dielectric_reflection.red()
                    > dielectric_reflection.green() * 3,
            "Smooth PBR dielectric did not produce a Fresnel reflection.");

    // A mirror in an otherwise empty scene used to become black because the
    // native tracer could only reflect rtBackColor. The enabled environment
    // must now provide visible studio illumination, as the OpenGL viewport
    // does for material previews.
    RenderScene environment_mirror_scene = mirror_scene;
    environment_mirror_scene.meshes.resize(1);
    environment_mirror_scene.materials[0].metallic = 1.0f;
    environment_mirror_scene.environment.enabled = true;
    environment_mirror_scene.environment.hdri_path.clear();
    environment_mirror_scene.environment.strength = 1.0f;
    QImage environment_mirror_image;
    require(NativeRaytraceRenderer::Render(
                environment_mirror_scene, mirror_settings,
                &environment_mirror_image, &native_error),
            native_error.toUtf8().constData());
    const QColor environment_reflection(
        environment_mirror_image.pixel(32, 32));
    require(std::max({environment_reflection.red(),
                      environment_reflection.green(),
                      environment_reflection.blue()}) > 20,
            "Native mirror did not reflect the studio environment.");
    bool native_geometry_crosses_middle = false;
    const int middle_y = native_single.height() / 2;
    for (int x = 0; x < native_single.width(); ++x) {
        if (native_single.pixel(x, middle_y) != background) {
            native_geometry_crosses_middle = true;
            break;
        }
    }
    require(native_geometry_crosses_middle,
            "Native raytrace used image width for its vertical coordinate.");

    RenderScene native_ortho_scene = native_scene;
    native_ortho_scene.camera.orthographic = true;
    native_ortho_scene.camera.orthographic_scale_mm = 1200.0f;
    QImage native_ortho;
    require(NativeRaytraceRenderer::Render(
                native_ortho_scene, native_settings,
                &native_ortho, &native_error)
                && native_ortho.size() == QSize(112, 64),
            native_error.toUtf8().constData());
    bool native_ortho_visible = false;
    const QRgb ortho_background = native_ortho.pixel(0, 0);
    for (int y = 0; y < native_ortho.height() && !native_ortho_visible; ++y) {
        for (int x = 0; x < native_ortho.width(); ++x) {
            if (native_ortho.pixel(x, y) != ortho_background) {
                native_ortho_visible = true;
                break;
            }
        }
    }
    require(native_ortho_visible,
            "Native orthographic raytrace contains only the background.");

    RenderSettings cycles_settings;
    cycles_settings.width = 64;
    cycles_settings.height = 64;
    cycles_settings.samples = 1;
    cycles_settings.noise_threshold = 1.0;
    cycles_settings.denoise = false;
    cycles_settings.device = RenderSettings::Device::CPU;
    cycles_settings.output_file = directory.filePath("Cycles результат.png");
    const QString cycles_json = directory.filePath("Cycles сцена.json");
    QString cycles_error;
    require(cycles_scene.SaveJson(cycles_json, cycles_settings, &cycles_error)
                && QFileInfo::exists(cycles_json),
            cycles_error.toUtf8().constData());
    const QString blender_path = BlenderCyclesRenderer::FindBlender();
    if (!blender_path.isEmpty()) {
        BlenderCyclesRenderer cycles_renderer(blender_path);
        require(cycles_renderer.IsAvailable(&cycles_error),
                cycles_error.toUtf8().constData());
        QEventLoop render_loop;
        QTimer render_timeout;
        render_timeout.setSingleShot(true);
        bool cycles_finished = false;
        QString cycles_failure;
        QObject::connect(&cycles_renderer,
                         &BlenderCyclesRenderer::RenderFinished,
                         &render_loop,
                         [&](const QString& output, double) {
            cycles_finished = QFileInfo(output).size() > 0;
            render_loop.quit();
        });
        QObject::connect(&cycles_renderer,
                         &BlenderCyclesRenderer::RenderFailed,
                         &render_loop,
                         [&](const QString& error, const QString&) {
            cycles_failure = error;
            render_loop.quit();
        });
        QObject::connect(&render_timeout, &QTimer::timeout, &render_loop, [&]() {
            cycles_renderer.Cancel();
            cycles_failure = "Blender Cycles smoke render timed out.";
            render_loop.quit();
        });
        require(cycles_renderer.StartRender(
                    cycles_scene, cycles_settings, &cycles_error),
                cycles_error.toUtf8().constData());
        render_timeout.start(120000);
        render_loop.exec();
        require(cycles_finished,
                cycles_failure.isEmpty()
                    ? "Blender Cycles did not return a PNG."
                    : cycles_failure.toUtf8().constData());
    }

    // DXF-authored catalog profiles can be stored as an open CPolyline whose
    // final point repeats the first one. Promotion must recognize that as a
    // closed sketch and preserve the authored corner radii.
    CPolyline dxf_cutter("DXF cutter profile");
    dxf_cutter.AddPoint(CPoint3d(-5.0, 30.0, 0.0));
    dxf_cutter.AddPoint(CPoint3d(-5.0, 0.0, 0.0));
    dxf_cutter.AddPoint(CPoint3d(5.0, 0.0, 0.0));
    dxf_cutter.AddPoint(CPoint3d(5.0, 30.0, 0.0));
    dxf_cutter.AddPoint(CPoint3d(-5.0, 30.0, 0.0));
    require(dxf_cutter.SetVertexRadius(1, 4.0)
                && dxf_cutter.SetVertexRadius(2, 4.0),
            "DXF cutter corner radii were not created.");
    CSmartLine promoted_dxf_cutter("Promoted DXF cutter profile");
    require(promoted_dxf_cutter.Create(dxf_cutter)
                && promoted_dxf_cutter.IsClosed()
                && promoted_dxf_cutter.GetNumLines() == 4
                && promoted_dxf_cutter.GetNumFillets() == 2,
            "Repeated-end DXF cutter was not promoted to a closed rounded sketch.");
    require(std::abs(promoted_dxf_cutter.GetFillet(0)->GetRadius() - 4.0)
                    < 1.0e-9
                && std::abs(promoted_dxf_cutter.GetFillet(1)->GetRadius() - 4.0)
                    < 1.0e-9,
            "DXF cutter radii changed during sketch promotion.");
    const SketchCoordinateSystem& promoted_axes =
        promoted_dxf_cutter.GetCoordinateSystem();
    require(std::abs(promoted_axes.origin.x) < 1.0e-9
                && std::abs(promoted_axes.origin.y) < 1.0e-9
                && std::abs(promoted_axes.x_axis.x - 1.0) < 1.0e-9
                && std::abs(promoted_axes.y_axis.y - 1.0) < 1.0e-9,
            "DXF cutter catalog XY axes were not preserved.");
    const MillingCutterPlacement promoted_placement =
        ResolveMillingCutterPlacement(promoted_dxf_cutter);
    require(!promoted_placement.rotated_legacy_profile
                && promoted_placement.tip_at_cutting_depth
                && std::abs(promoted_placement.angle_degrees) < 1.0e-9,
            "Positive-Y physical cutter was not recognized by its tip datum.");
    std::string cutter_validation_error;
    require(ValidateMillingCutterCatalogProfile(
                promoted_dxf_cutter, cutter_validation_error),
            cutter_validation_error.c_str());

    CSmartLine anisotropic_guide("Anisotropic milling guide");
    require(anisotropic_guide.Add(new CLinkLine(
                {10.0, 20.0, 0.0}, {30.0, 40.0, 0.0})),
            "Anisotropic milling guide was not created.");
    anisotropic_guide.ScaleLocal(2.0, 3.0);
    require(std::abs(anisotropic_guide.GetLine(0)->GetStart().x - 20.0)
                    < 1.0e-9
                && std::abs(anisotropic_guide.GetLine(0)->GetStart().y - 60.0)
                    < 1.0e-9
                && std::abs(anisotropic_guide.GetLine(0)->GetEnd().x - 60.0)
                    < 1.0e-9
                && std::abs(anisotropic_guide.GetLine(0)->GetEnd().y - 120.0)
                    < 1.0e-9,
            "Milling guide X/Y scaling was not independent.");

    CAlfaDoc cutter_catalog_with_empty_placeholder;
    cutter_catalog_with_empty_placeholder.GetObjects().clear();
    cutter_catalog_with_empty_placeholder.AddObject(
        std::make_unique<CPolyline>("Empty catalog placeholder"));
    cutter_catalog_with_empty_placeholder.AddObject(
        std::make_unique<CSmartLine>(promoted_dxf_cutter.MakeCopy()));
    const CSmartLine* resolved_catalog_cutter =
        ResolveMillingCutterCatalogProfile(
            cutter_catalog_with_empty_placeholder,
            cutter_validation_error);
    require(resolved_catalog_cutter
                && resolved_catalog_cutter->GetNumLines() == 4,
            cutter_validation_error.c_str());

    auto ambiguous_polyline = std::make_unique<CPolyline>(
        "Non-empty legacy cutter profile");
    ambiguous_polyline->AddPoint(CPoint3d(-5.0, 0.0, 0.0));
    ambiguous_polyline->AddPoint(CPoint3d(5.0, 0.0, 0.0));
    cutter_catalog_with_empty_placeholder.AddObject(
        std::move(ambiguous_polyline));
    require(!ResolveMillingCutterCatalogProfile(
                cutter_catalog_with_empty_placeholder,
                cutter_validation_error),
            "A second non-empty catalog profile was not rejected.");

    const auto require_quad_solid = [](TopoDS_Shape shape,
                                       const char* build_message) {
        CSolid solid(shape);
        require(solid.InitSurfaces() && solid.InitEdges()
                    && solid.ReBuldMesh(), build_message);
        require(solid.GetNumSurfaces() == 6,
                "Default Solid box has an unexpected face count.");
        for (int surface_index = 0;
             surface_index < solid.GetNumSurfaces(); ++surface_index) {
            const CSurfaceFace* surface = solid.GetSurfaceFace(surface_index);
            require(surface && surface->pMesh3D
                        && !surface->pMesh3D->GetFaces().empty(),
                    "Default Solid box face has no render mesh.");
            for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
                require(face.deleted || face.corners.size() == 4,
                        "Untrimmed Solid box face was triangulated instead of using CNet quads.");
            }
        }
    };

    require_quad_solid(
        BRepPrimAPI_MakeBox(gp_Pnt(0.0, 0.0, 0.0), 600.0, 500.0, 800.0).Shape(),
        "BRepPrimAPI Solid box mesh was not built.");

    // SolidBoxTool creates its parametric box by extruding a polygonal face,
    // not with BRepPrimAPI_MakeBox. Keep this topology in the regression test:
    // its copied cap used to fall back to an OCCT triangle fan.
    BRepBuilderAPI_MakePolygon box_polygon;
    box_polygon.Add(gp_Pnt(0.0, 0.0, 0.0));
    box_polygon.Add(gp_Pnt(600.0, 0.0, 0.0));
    box_polygon.Add(gp_Pnt(600.0, 500.0, 0.0));
    box_polygon.Add(gp_Pnt(0.0, 500.0, 0.0));
    box_polygon.Close();
    BRepBuilderAPI_MakeFace box_face(box_polygon.Wire());
    BRepPrimAPI_MakePrism box_prism(box_face.Face(), gp_Vec(0.0, 0.0, 800.0));
    require_quad_solid(box_prism.Shape(),
                       "SolidBoxTool prism mesh was not built.");

    // The fast renderer deliberately sends trimmed faces to OCCT, but Low
    // Poly + Mesh Quadro must keep using the July quad/trimming algorithm for
    // every face.  A through-hole gives us both natural and trimmed faces in
    // one solid and protects the two strategies from being merged again.
    TestLowPolyQuadroBranch();

    CAlfaDoc lightweight_document;
    lightweight_document.GetObjects().clear();
    auto lightweight_curve = std::make_unique<CPolyline>("Undo curve");
    lightweight_curve->AddPoint(CPoint3d(0.0, 0.0, 0.0));
    lightweight_curve->AddPoint(CPoint3d(10.0, 0.0, 0.0));
    lightweight_document.AddObject(std::move(lightweight_curve));
    const unsigned long lightweight_id =
        lightweight_document.GetObjects().back()->m_id;
    CUndoRedo lightweight_undo(lightweight_document);
    lightweight_document.GetObjects().back()->m_LayerID = 7;
    require(lightweight_undo.RecordCommand(
                "Change layer",
                [lightweight_id](CAlfaDoc& document) {
                    CAlfaObject* object = document.FindObjectById(lightweight_id);
                    if (!object) return false;
                    object->m_LayerID = 1;
                    return true;
                },
                [lightweight_id](CAlfaDoc& document) {
                    CAlfaObject* object = document.FindObjectById(lightweight_id);
                    if (!object) return false;
                    object->m_LayerID = 7;
                    return true;
                })
            && lightweight_undo.Undo()
            && lightweight_document.FindObjectById(lightweight_id)->m_LayerID == 1
            && lightweight_undo.Redo()
            && lightweight_document.FindObjectById(lightweight_id)->m_LayerID == 7,
            "Lightweight layer Undo/Redo failed.");

    CAlfaDoc source;
    source.GetObjects().clear();
    auto first = std::make_unique<CPolyline>("Seat outline");
    first->AddPoint(CPoint3d(0.0, 0.0, 0.0));
    first->AddPoint(CPoint3d(10.0, 0.0, 0.0));
    source.AddObject(std::move(first));
    const unsigned long first_id = source.GetObjects().back()->m_id;
    auto second = std::make_unique<CPolyline>("Back outline");
    second->AddPoint(CPoint3d(0.0, 0.0, 0.0));
    second->AddPoint(CPoint3d(0.0, 12.0, 0.0));
    source.AddObject(std::move(second));
    const unsigned long second_id = source.GetObjects().back()->m_id;
    source.AddObject(std::make_unique<CGroup>(
        "Chair assembly", std::vector<unsigned long>{first_id, second_id}));

    Dom3DProjectSerializer serializer;
    ProjectViewState view;
    require(std::abs(view.camera.vertical_fov_degrees - 50.0f) < 0.001f,
            "Default camera field of view is not 50 degrees.");
    QString error;

    CAlfaDoc film_source;
    film_source.GetObjects().clear();
    TopoDS_Shape film_shape = BRepPrimAPI_MakeBox(120.0, 80.0, 20.0).Shape();
    auto film_solid = std::make_unique<CSolid>(film_shape);
    require(film_solid->ReBuldMesh() && film_solid->GetNumSurfaces() > 0,
            "Film persistence test body could not be meshed.");
    Material film = Material::DefaultGloss();
    film.name = "Oracal Film Translucent Glossy RAL 3020";
    film.diffuse = {0.8f, 0.0235f, 0.0196f};
    film.alpha = 0.58f;
    film.roughness = 0.18f;
    film.coat_weight = 0.72f;
    const Material& saved_film = film_source.UpsertMaterial(film);
    film_solid->SetSurfaceCoating(0, saved_film);
    film_solid->SetParametricOperation(
        0, "SurfaceFilmCoating", "Oracal Film — RAL 3020",
        {{"material.id", static_cast<double>(saved_film.id)},
         {"ral", 3020.0}, {"film.type", 1.0}}, {0});
    film_source.AddObject(std::move(film_solid));
    const QString film_path = directory.filePath("FilmCoating.dom3d");
    require(serializer.Save(film_path, film_source, "Test", view, {}, error),
            error.toUtf8().constData());
    CAlfaDoc film_reload;
    QString film_room;
    require(serializer.Load(film_path, film_reload, film_room, view, error),
            error.toUtf8().constData());
    const auto* restored_film_solid = film_reload.GetObjects().empty()
        ? nullptr
        : dynamic_cast<const CSolid*>(film_reload.GetObjects().front().get());
    const CSurfaceFace* restored_film_surface = restored_film_solid
        ? restored_film_solid->GetSurfaceFace(0) : nullptr;
    const ParametricFunction* restored_film_operation = restored_film_solid
        ? restored_film_solid->GetOperation(0) : nullptr;
    require(restored_film_surface
                && restored_film_surface->MaterialOverride.coating_enabled
                && restored_film_surface->MaterialOverride.coating_material_id
                    == saved_film.id
                && restored_film_operation
                && restored_film_operation->ToolId == "SurfaceFilmCoating"
                && restored_film_operation->Name == "Oracal Film — RAL 3020",
            "Oracal film layer or operation did not survive project save/load.");

    const QString item_path = directory.filePath("Chair.dom3d");
    view.camera.vertical_fov_degrees = 67.0f;
    require(serializer.Save(
                item_path, source, "Catalog", view, {}, error),
            error.toUtf8().constData());
    CAlfaDoc camera_reload;
    QString camera_room;
    ProjectViewState camera_view;
    require(serializer.Load(
                item_path, camera_reload, camera_room, camera_view, error)
                && camera_view.has_camera
                && std::abs(camera_view.camera.vertical_fov_degrees - 67.0f)
                    < 0.001f,
            "Camera field of view did not survive project save/load.");

    CAlfaDoc target;
    target.GetObjects().clear();
    auto existing = std::make_unique<CPolyline>("Existing");
    existing->AddPoint(CPoint3d(-5.0, -5.0, 0.0));
    existing->AddPoint(CPoint3d(-1.0, -1.0, 0.0));
    target.AddObject(std::move(existing));
    const unsigned long existing_id = target.GetObjects().front()->m_id;

    require(serializer.ImportPart(
                item_path, target, "Chair", {20.0f, 30.0f, 0.0f},
                {1.0f, 1.0f, 1.0f}, true, true, error),
            error.toUtf8().constData());
    require(serializer.ImportPart(
                item_path, target, "Chair", {50.0f, 0.0f, 0.0f},
                {1.0f, 1.0f, 1.0f}, false, true, error),
            error.toUtf8().constData());

    std::set<unsigned long> ids;
    int part_count = 0;
    const CPart* first_part = nullptr;
    for (const auto& object : target.GetObjects()) {
        require(object && ids.insert(object->m_id).second,
                "Imported object IDs are not unique.");
        if (const auto* part = dynamic_cast<const CPart*>(object.get())) {
            if (!first_part) {
                first_part = part;
            }
            ++part_count;
            for (unsigned long child_id : part->GetElementIds()) {
                require(target.FindObjectById(child_id) != nullptr,
                        "Part contains a stale child ID.");
            }
        }
    }
    require(ids.count(existing_id) == 1,
            "Import replaced the existing document object.");
    require(part_count == 2, "Two imports did not create two Parts.");
    require(first_part && first_part->IsFileLinked()
                && !first_part->GetSourcePath().empty(),
            "Linked Part did not retain its source file.");
    require(first_part->GetName() == "Chair",
            "First imported Part has an unexpected name.");
    size_t first_part_index = target.GetObjects().size();
    bool found_unique_name = false;
    for (size_t index = 0; index < target.GetObjects().size(); ++index) {
        const auto& object = target.GetObjects()[index];
        const auto* part = dynamic_cast<const CPart*>(object.get());
        found_unique_name = found_unique_name
            || (part && part->GetName() == "Chair 2");
        if (part == first_part) {
            first_part_index = index;
        }
    }
    require(found_unique_name, "Repeated Part name was not made unique.");

    Vec3 minimum{};
    Vec3 maximum{};
    require(first_part->GetBounds(minimum, maximum)
                && minimum.x >= 19.999f && minimum.y >= 29.999f,
            "Part insertion point was not applied to its contents.");

    const auto project_xy = [](Vec3 world, DomPoint& screen) {
        screen.x = static_cast<int>(world.x);
        screen.y = static_cast<int>(world.y);
        return true;
    };
    require(target.SelectPolylineAtScreen(
                {25, 30}, project_xy, 2.0f, SelectionAction::Replace),
            "Could not select a curve inside an imported assembly.");
    require(dynamic_cast<CGroup*>(target.GetSelectedObject()) != nullptr
                && dynamic_cast<CPart*>(target.GetSelectedObject()) == nullptr,
            "Nested imported assembly was incorrectly promoted to the outer Part.");

    const QString selection_path = directory.filePath("SelectedPart.dom3d");
    require(first_part_index < target.GetObjects().size()
                && serializer.SaveSelection(
                    selection_path, target, {first_part_index}, "Catalog",
                    view, {}, error),
            error.toUtf8().constData());
    CAlfaDoc selected_reload;
    QString selected_room;
    require(serializer.Load(
                selection_path, selected_reload, selected_room, view, error),
            error.toUtf8().constData());
    int selected_part_count = 0;
    for (const auto& object : selected_reload.GetObjects()) {
        selected_part_count += dynamic_cast<const CPart*>(object.get()) ? 1 : 0;
        require(object && object->GetName() != "Existing"
                    && object->GetName() != "Chair 2",
                "Catalog selection export included unrelated objects.");
    }
    require(selected_part_count == 1,
            "Selected Part export lost its Part container.");

    CAlfaDoc grouped_sketch_source;
    grouped_sketch_source.GetObjects().clear();
    auto first_guide = std::make_unique<CSmartLine>("First milling guide");
    require(first_guide->Add(new CLinkLine(
                {10.0, 10.0, 0.0}, {90.0, 10.0, 0.0})),
            "First grouped milling guide was not created.");
    grouped_sketch_source.AddObject(std::move(first_guide));
    const size_t first_guide_index =
        grouped_sketch_source.GetObjects().size() - 1;
    const unsigned long first_guide_id =
        grouped_sketch_source.GetObjects().back()->m_id;

    auto second_guide = std::make_unique<CSmartLine>("Second milling guide");
    require(second_guide->Add(new CLinkLine(
                {10.0, 20.0, 0.0}, {90.0, 20.0, 0.0})),
            "Second grouped milling guide was not created.");
    grouped_sketch_source.AddObject(std::move(second_guide));
    const unsigned long second_guide_id =
        grouped_sketch_source.GetObjects().back()->m_id;

    grouped_sketch_source.AddObject(std::make_unique<CGroup>(
        "Facade milling pattern",
        std::vector<unsigned long>{first_guide_id, second_guide_id}));
    auto unrelated_guide = std::make_unique<CSmartLine>("Unrelated guide");
    require(unrelated_guide->Add(new CLinkLine(
                {0.0, 0.0, 0.0}, {0.0, 50.0, 0.0})),
            "Unrelated milling guide was not created.");
    grouped_sketch_source.AddObject(std::move(unrelated_guide));

    const QString grouped_selection_path =
        directory.filePath("GroupedMillingPattern.dom3d");
    require(serializer.SaveSelection(
                grouped_selection_path, grouped_sketch_source,
                {first_guide_index}, "Catalog", view, {}, error),
            error.toUtf8().constData());
    CAlfaDoc grouped_selection_reload;
    require(serializer.Load(
                grouped_selection_path, grouped_selection_reload,
                selected_room, view, error),
            error.toUtf8().constData());
    int grouped_sketch_count = 0;
    int group_count = 0;
    for (const auto& object : grouped_selection_reload.GetObjects()) {
        grouped_sketch_count +=
            dynamic_cast<const CSmartLine*>(object.get()) ? 1 : 0;
        group_count += dynamic_cast<const CGroup*>(object.get()) ? 1 : 0;
        require(object && object->GetName() != "Unrelated guide",
                "Grouped catalog selection included an unrelated sketch.");
    }
    require(grouped_sketch_count == 2 && group_count == 1,
            "Selecting a sketch in a group did not export the complete group.");
    SetAlfaDoc(&target);

    Material lacquer_material;
    lacquer_material.name = "Lacquer regression";
    lacquer_material.coat_weight = 0.85f;
    lacquer_material.coat_roughness = 0.06f;
    const Material& saved_lacquer = target.UpsertMaterial(lacquer_material);
    const unsigned long lacquer_id = saved_lacquer.id;

    QMimeData lacquer_mime;
    lacquer_mime.setData(
        MaterialDrag::MimeType(), MaterialDrag::Encode(saved_lacquer));
    Material dragged_lacquer;
    require(MaterialDrag::Decode(&lacquer_mime, dragged_lacquer)
                && std::abs(dragged_lacquer.coat_weight - 0.85f) < 1.0e-6f
                && std::abs(dragged_lacquer.coat_roughness - 0.06f) < 1.0e-6f,
            "Clear-coat parameters did not survive material drag-and-drop.");

    const QString roundtrip_path = directory.filePath("Roundtrip.dom3d");
    require(serializer.Save(
                roundtrip_path, target, "Catalog", view, {}, error),
            error.toUtf8().constData());
    CAlfaDoc reloaded;
    QString room;
    require(serializer.Load(
                roundtrip_path, reloaded, room, view, error),
            error.toUtf8().constData());
    int reloaded_parts = 0;
    for (const auto& object : reloaded.GetObjects()) {
        reloaded_parts += dynamic_cast<const CPart*>(object.get()) ? 1 : 0;
    }
    require(reloaded_parts == 2,
            "Parts did not survive project save/load.");
    const Material* reloaded_lacquer = reloaded.FindMaterial(lacquer_id);
    require(reloaded_lacquer
                && std::abs(reloaded_lacquer->coat_weight - 0.85f) < 1.0e-6f
                && std::abs(reloaded_lacquer->coat_roughness - 0.06f) < 1.0e-6f,
            "Clear-coat parameters did not survive project save/load.");

    CSmartLine cutter_profile("Catalog cutter profile");
    require(cutter_profile.Add(new CLinkLine({-2.0, 0.0, 0.0}, {2.0, 0.0, 0.0}))
            && cutter_profile.Add(new CLinkLine({2.0, 0.0, 0.0}, {2.0, -4.0, 0.0}))
            && cutter_profile.Add(new CLinkLine({2.0, -4.0, 0.0}, {-2.0, -4.0, 0.0}))
            && cutter_profile.Add(new CLinkLine({-2.0, -4.0, 0.0}, {-2.0, 0.0, 0.0}))
            && cutter_profile.SetClosed(true),
            "Catalog cutter profile was not created.");
    const MillingCutterPlacement negative_y_cutter_placement =
        ResolveMillingCutterPlacement(cutter_profile);
    require(negative_y_cutter_placement.rotated_legacy_profile
                && negative_y_cutter_placement.tip_at_cutting_depth
                && std::abs(negative_y_cutter_placement.angle_degrees - 180.0)
                    < 1.0e-9,
            "Negative-Y physical cutter was not rotated around its tip datum.");
    require(!ValidateMillingCutterCatalogProfile(
                cutter_profile, cutter_validation_error)
                && cutter_validation_error.find("Y>=0") != std::string::npos,
            "Strict cutter catalog validation accepted a negative-Y profile.");

    CSmartLine off_axis_cutter("Off-axis catalog cutter");
    require(off_axis_cutter.Add(
                new CLinkLine({0.0, 0.0, 0.0}, {4.0, 0.0, 0.0}))
            && off_axis_cutter.Add(
                new CLinkLine({4.0, 0.0, 0.0}, {4.0, 6.0, 0.0}))
            && off_axis_cutter.Add(
                new CLinkLine({4.0, 6.0, 0.0}, {0.0, 6.0, 0.0}))
            && off_axis_cutter.Add(
                new CLinkLine({0.0, 6.0, 0.0}, {0.0, 0.0, 0.0}))
            && off_axis_cutter.SetClosed(true),
            "Off-axis cutter fixture was not created.");
    require(!ValidateMillingCutterCatalogProfile(
                off_axis_cutter, cutter_validation_error)
                && cutter_validation_error.find("X=0") != std::string::npos,
            "Strict cutter catalog validation accepted an off-axis profile.");

    CSmartLine closed_guide("Closed milling guide");
    require(closed_guide.SetCoordinateSystem(
                {0.0, -0.1, 0.0}, {1.0, 0.0, 0.0}, {0.0, -1.0, 0.0})
            && closed_guide.Add(new CLinkLine({20.0, 20.0, 0.0}, {80.0, 20.0, 0.0}))
            && closed_guide.Add(new CLinkLine({80.0, 20.0, 0.0}, {80.0, 80.0, 0.0}))
            && closed_guide.Add(new CLinkLine({80.0, 80.0, 0.0}, {20.0, 80.0, 0.0}))
            && closed_guide.Add(new CLinkLine({20.0, 80.0, 0.0}, {20.0, 20.0, 0.0}))
            && closed_guide.SetClosed(true),
            "Closed catalog milling guide was not created.");
    CSmartLine placed_physical_cutter("Placed physical cutter");
    constexpr double physical_cutting_depth = 12.0;
    require(BuildPlacedSweptSectionSketch(
                promoted_dxf_cutter, closed_guide, placed_physical_cutter,
                0.0,
                promoted_placement.delta_y - physical_cutting_depth,
                promoted_placement.angle_degrees),
            "Physical cutter was not placed at the requested depth.");
    double placed_min_y = std::numeric_limits<double>::max();
    double placed_max_y = std::numeric_limits<double>::lowest();
    for (const CPoint3d& point :
         placed_physical_cutter.GetProfilePointsWorld()) {
        placed_min_y = std::min(placed_min_y, point.y);
        placed_max_y = std::max(placed_max_y, point.y);
    }
    require(placed_min_y < -17.9
                && placed_max_y > 11.8
                && placed_max_y < 12.0,
            "Physical cutter shank did not remain outside while its tip advanced to depth.");
    const TopoDS_Shape closed_cutter = BuildSweptSolidShape(
        cutter_profile, closed_guide, 2);
    require(!closed_cutter.IsNull(),
            "Closed catalog milling guide did not create a cutter sweep.");
    const TopoDS_Shape panel = BRepPrimAPI_MakeBox(
        gp_Pnt(0.0, 0.0, 0.0), 100.0, 18.0, 100.0).Shape();
    BRepAlgoAPI_Cut milling_cut(panel, closed_cutter);
    milling_cut.Build();
    require(milling_cut.IsDone(),
            "Closed catalog milling cutter could not be subtracted.");
    GProp_GProps panel_properties;
    GProp_GProps cut_properties;
    BRepGProp::VolumeProperties(panel, panel_properties);
    BRepGProp::VolumeProperties(milling_cut.Shape(), cut_properties);
    require(cut_properties.Mass() < panel_properties.Mass() - 1.0,
            "Closed catalog milling cutter did not remove facade material.");

    const std::vector<const CSmartLine*> catalog_guides{&closed_guide};
    const TopoDS_Shape catalog_milled_facade =
        CFacadeFurniture::BuildPlanarMilledFromProfiles(
            cutter_profile, catalog_guides,
            0.0, 0.0, 0.0, 100.0, 18.0, 100.0,
            nullptr, false, nullptr, 3.0);
    require(!catalog_milled_facade.IsNull(),
            "Catalog Milled facade was not created by the common algorithm.");
    GProp_GProps catalog_facade_properties;
    BRepGProp::VolumeProperties(
        catalog_milled_facade, catalog_facade_properties);
    require(catalog_facade_properties.Mass()
                < panel_properties.Mass() - 1.0,
            "Catalog Milled facade did not remove material.");

    CSmartLine built_in_debug("Built-in cutter debug");
    const TopoDS_Shape built_in_milled =
        CFacadeFurniture::BuildPlanarShape(
            KitchenCabinetFacadeStyle::Milled,
            0.0, 0.0, 0.0, 500.0, 18.0, 700.0,
            true, KitchenCabinetShowcaseFill::Glass,
            &built_in_debug);
    require(!built_in_milled.IsNull()
                && built_in_debug.IsClosed()
                && built_in_debug.GetNumLines() == 10,
            "Built-in Milled did not expose its pre-Swept cutter sketch.");

    const TopoDS_Shape rounded_plain =
        CFacadeFurniture::BuildPlanarShape(
            KitchenCabinetFacadeStyle::Plain,
            0.0, 0.0, 0.0, 500.0, 18.0, 700.0);
    int rounded_plain_edges = 0;
    int rounded_plain_r2_faces = 0;
    int rounded_plain_r3_faces = 0;
    for (TopExp_Explorer edge(rounded_plain, TopAbs_EDGE);
         edge.More(); edge.Next()) {
        ++rounded_plain_edges;
    }
    for (TopExp_Explorer face(rounded_plain, TopAbs_FACE);
         face.More(); face.Next()) {
        BRepAdaptor_Surface surface(TopoDS::Face(face.Current()));
        if (surface.GetType() == GeomAbs_Cylinder) {
            const double radius = surface.Cylinder().Radius();
            if (std::abs(radius - 2.0) < 1.0e-6) {
                ++rounded_plain_r2_faces;
            } else if (std::abs(radius - 3.0) < 1.0e-6) {
                ++rounded_plain_r3_faces;
            }
        }
    }
    require(!rounded_plain.IsNull() && rounded_plain_edges > 20
                && rounded_plain_r2_faces == 4
                && rounded_plain_r3_faces == 4,
            "Plain facade has no R3 corners and R2 front contour.");

    for (int facade_style = 0; facade_style < 5; ++facade_style) {
        FurnitureDrawerDefinition drawer_facade;
        drawer_facade.facade_style =
            static_cast<FurnitureDrawerFacadeStyle>(facade_style);
        drawer_facade.make_handle = false;
        const auto drawer_parts =
            CFurnitureDrawer::BuildParts(drawer_facade);
        const auto facade = std::find_if(
            drawer_parts.begin(), drawer_parts.end(),
            [&drawer_facade](const auto& part) {
                return part
                    && part->GetName() == drawer_facade.facade_name;
            });
        const bool has_facade = facade != drawer_parts.end();
        require(has_facade,
                "One of the five Drawer Box facade styles was not built.");
        if (facade_style == static_cast<int>(
                FurnitureDrawerFacadeStyle::Screen)) {
            const auto* solid = dynamic_cast<const CSolid*>(facade->get());
            int solid_count = 0;
            if (solid) {
                for (TopExp_Explorer part(solid->m_Shape, TopAbs_SOLID);
                     part.More(); part.Next()) {
                    ++solid_count;
                }
            }
            require(solid_count >= 5,
                    "Drawer Screen facade has no closed centre panel.");
        }
    }

    KitchenCabinetDefinition radius_milano;
    radius_milano.body_type = KitchenCabinetBodyType::Radius3;
    radius_milano.facade_type = KitchenCabinetFacadeType::SingleDoor;
    radius_milano.facade_style = KitchenCabinetFacadeStyle::Milano;
    radius_milano.width = 631.543;
    radius_milano.depth = 626.115;
    radius_milano.height = 659.112;
    radius_milano.shelf_count = 2;
    const auto has_five_part_radius_facade = [](
        const auto& parts, const std::string& facade_name) {
        const std::array<const char*, 5> suffixes{{
            " Bottom Profile", " Top Profile", " Right Profile",
            " Left Profile", " Center Panel"}};
        for (const char* suffix : suffixes) {
            const std::string detail_name = facade_name + suffix;
            const auto detail = std::find_if(
                parts.begin(), parts.end(), [&detail_name](const auto& part) {
                    return part && part->GetName() == detail_name;
                });
            if (detail == parts.end()) return false;
            const auto* solid = dynamic_cast<const CSolid*>(detail->get());
            if (!solid || solid->m_Shape.IsNull()
                || !BRepCheck_Analyzer(solid->m_Shape).IsValid()) {
                return false;
            }
        }
        return true;
    };
    const auto radius_milano_parts =
        CKitchenCabinet::BuildParts(radius_milano);
    const auto has_valid_part = [](const auto& parts,
                                   const std::string& name) {
        const auto part = std::find_if(
            parts.begin(), parts.end(), [&name](const auto& candidate) {
                return candidate && candidate->GetName() == name;
            });
        const auto* solid = part == parts.end()
            ? nullptr : dynamic_cast<const CSolid*>(part->get());
        return solid && !solid->m_Shape.IsNull()
            && BRepCheck_Analyzer(solid->m_Shape).IsValid();
    };
    const std::array<const char*, 4> radius3_profile_names{{
        "Radius-3 Cabinet Facade Bottom Profile",
        "Radius-3 Cabinet Facade Top Profile",
        "Radius-3 Cabinet Facade Right Profile",
        "Radius-3 Cabinet Facade Left Profile"}};
    require(std::all_of(
                radius3_profile_names.begin(), radius3_profile_names.end(),
                [&radius_milano_parts, &has_valid_part](const char* name) {
                    return has_valid_part(radius_milano_parts, name);
                }),
            "One of the new Radius-3 Milano profiles is missing.");
    require(has_valid_part(radius_milano_parts, "Radius-3 Cabinet Shelf 1")
                && has_valid_part(
                    radius_milano_parts, "Radius-3 Cabinet Shelf 2"),
            "The new Radius-3 builder did not add all requested shelves.");

    KitchenCabinetDefinition compact_radius_milano = radius_milano;
    compact_radius_milano.width = 576.8;
    compact_radius_milano.depth = 492.4;
    compact_radius_milano.height = 706.5;
    const auto compact_radius_parts =
        CKitchenCabinet::BuildParts(compact_radius_milano);
    require(std::all_of(
                radius3_profile_names.begin(), radius3_profile_names.end(),
                [&compact_radius_parts, &has_valid_part](const char* name) {
                    return has_valid_part(compact_radius_parts, name);
                }),
            "A compact Radius-3 Milano profile disappeared.");

    const std::array<std::pair<KitchenCabinetBodyType, const char*>, 3>
        additional_radius_milano_types{{
            {KitchenCabinetBodyType::Radius, "Cabinet Radius Facade"},
            {KitchenCabinetBodyType::Radius2, "Radius-2 Cabinet Facade"},
            {KitchenCabinetBodyType::Radius4, "Radius-4 Cabinet Facade"}}};
    for (const auto& [body_type, facade_name]
         : additional_radius_milano_types) {
        KitchenCabinetDefinition curved_milano = radius_milano;
        curved_milano.body_type = body_type;
        curved_milano.radius2_bulge = 120.0;
        curved_milano.radius_side_straight = 160.0;
        const auto curved_parts = CKitchenCabinet::BuildParts(curved_milano);
        require(has_five_part_radius_facade(curved_parts, facade_name),
                "A radius body type did not build a five-detail Milano facade.");
        if (body_type == KitchenCabinetBodyType::Radius4) {
            const auto left_side = std::find_if(
                curved_parts.begin(), curved_parts.end(), [](const auto& part) {
                    return part
                        && part->GetName() == "Radius-4 Cabinet Left Side";
                });
            const std::string left_profile_name =
                std::string(facade_name) + " Left Profile";
            const auto left_profile = std::find_if(
                curved_parts.begin(), curved_parts.end(),
                [&left_profile_name](const auto& part) {
                    return part && part->GetName() == left_profile_name;
                });
            const auto* facade_solid = left_profile != curved_parts.end()
                ? dynamic_cast<const CSolid*>(left_profile->get()) : nullptr;
            const auto* side_solid = left_side != curved_parts.end()
                ? dynamic_cast<const CSolid*>(left_side->get()) : nullptr;
            BRepExtrema_DistShapeShape side_to_facade;
            if (facade_solid && side_solid) {
                side_to_facade.LoadS1(side_solid->m_Shape);
                side_to_facade.LoadS2(facade_solid->m_Shape);
                side_to_facade.Perform();
            }
            require(side_to_facade.IsDone()
                        && side_to_facade.Value() <= 1.0e-4,
                    "Radius-4 has a gap between its left side and facade.");
        }
    }

    CSmartLine positive_depth_cutter("Rotated catalog cutter profile");
    require(positive_depth_cutter.Add(
                new CLinkLine({-3.0, 0.0, 0.0}, {3.0, 0.0, 0.0}))
            && positive_depth_cutter.Add(
                new CLinkLine({3.0, 0.0, 0.0}, {3.0, 6.0, 0.0}))
            && positive_depth_cutter.Add(
                new CLinkLine({3.0, 6.0, 0.0}, {-3.0, 6.0, 0.0}))
            && positive_depth_cutter.Add(
                new CLinkLine({-3.0, 6.0, 0.0}, {-3.0, 0.0, 0.0}))
            && positive_depth_cutter.SetClosed(true),
            "Positive-depth catalog cutter profile was not created.");
    const MillingCutterPlacement physical_cutter_placement =
        ResolveMillingCutterPlacement(positive_depth_cutter);
    require(!physical_cutter_placement.rotated_legacy_profile
                && physical_cutter_placement.tip_at_cutting_depth
                && std::abs(physical_cutter_placement.angle_degrees) < 1.0e-9,
            "Physical cutter tip was not selected as the depth datum.");
    const TopoDS_Shape positive_depth_facade =
        CFacadeFurniture::BuildPlanarMilledFromProfiles(
            positive_depth_cutter, catalog_guides,
            0.0, 0.0, 0.0, 100.0, 18.0, 100.0,
            nullptr, false, nullptr, 3.0);
    GProp_GProps positive_depth_properties;
    BRepGProp::VolumeProperties(
        positive_depth_facade, positive_depth_properties);
    require(!positive_depth_facade.IsNull()
                && positive_depth_properties.Mass()
                    < panel_properties.Mass() - 1.0,
            "Positive-depth catalog cutter was oriented outside the facade.");

    // When the designer's catalog is present beside the Release build, also
    // exercise the actual Cutter_1 and Varian-1 files that exposed the axis
    // reversal. The regular test remains portable when those files are absent.
    QDir catalog_root(QCoreApplication::applicationDirPath());
    catalog_root.cdUp();
    const QString real_cutter_path = catalog_root.filePath(
        "Release/Catalog/Sketches/Milling cutters/Cutter_1.dom3d");
    const QString real_rounded_cutter_path = catalog_root.filePath(
        "Release/Catalog/Sketches/Milling cutters/Cutter_D10_R4.dom3d");
    const QString real_grouped_guide_path = catalog_root.filePath(
        "Release/Catalog/Sketches/Milling pattern/Group 1.dom3d");
    const QString real_guide_path = catalog_root.filePath(
        "Release/Catalog/Sketches/Milling pattern/Varian-1.dom3d");
    if (QFileInfo::exists(real_rounded_cutter_path)) {
        CAlfaDoc rounded_cutter_document;
        QString rounded_cutter_room;
        ProjectViewState rounded_cutter_view;
        require(serializer.Load(
                    real_rounded_cutter_path, rounded_cutter_document,
                    rounded_cutter_room, rounded_cutter_view, error),
                error.toUtf8().constData());
        const CSmartLine* rounded_cutter =
            ResolveMillingCutterCatalogProfile(
                rounded_cutter_document, cutter_validation_error);
        require(rounded_cutter,
                cutter_validation_error.c_str());

        if (QFileInfo::exists(real_grouped_guide_path)) {
            CAlfaDoc grouped_guide_document;
            QString grouped_guide_room;
            ProjectViewState grouped_guide_view;
            require(serializer.Load(
                        real_grouped_guide_path, grouped_guide_document,
                        grouped_guide_room, grouped_guide_view, error),
                    error.toUtf8().constData());
            std::vector<const CSmartLine*> grouped_guides;
            for (const auto& object : grouped_guide_document.GetObjects()) {
                if (const auto* guide =
                        dynamic_cast<const CSmartLine*>(object.get())) {
                    grouped_guides.push_back(guide);
                }
            }
            require(!grouped_guides.empty(),
                    "Group 1 contains no milling guide sketches.");
            const std::vector<const CSmartLine*> no_grouped_guides;
            const TopoDS_Shape grouped_baseline =
                CFacadeFurniture::BuildPlanarMilledFromProfiles(
                    *rounded_cutter, no_grouped_guides,
                    0.0, 0.0, 0.0, 560.0, 18.0, 760.0,
                    nullptr, true, nullptr, 12.0);
            const TopoDS_Shape grouped_milled_facade =
                CFacadeFurniture::BuildPlanarMilledFromProfiles(
                    *rounded_cutter, grouped_guides,
                    0.0, 0.0, 0.0, 560.0, 18.0, 760.0,
                    nullptr, true, nullptr, 12.0);
            GProp_GProps grouped_baseline_properties;
            GProp_GProps grouped_milled_properties;
            BRepGProp::VolumeProperties(
                grouped_baseline, grouped_baseline_properties);
            BRepGProp::VolumeProperties(
                grouped_milled_facade, grouped_milled_properties);
            require(!grouped_milled_facade.IsNull()
                        && grouped_milled_properties.Mass()
                            < grouped_baseline_properties.Mass() - 1.0,
                    "Cutter_D10_R4 did not mill the open sketches in Group 1.");
        }
    }
    if (QFileInfo::exists(real_cutter_path)
        && QFileInfo::exists(real_guide_path)) {
        auto load_first_sketch = [&](const QString& path,
                                     CAlfaDoc& catalog_document)
                -> const CSmartLine* {
            QString catalog_room;
            ProjectViewState catalog_view;
            QString catalog_error;
            if (!serializer.Load(path, catalog_document, catalog_room,
                                 catalog_view, catalog_error)) {
                return nullptr;
            }
            for (const auto& object : catalog_document.GetObjects()) {
                if (const auto* sketch =
                        dynamic_cast<const CSmartLine*>(object.get())) {
                    return sketch;
                }
            }
            return nullptr;
        };
        CAlfaDoc cutter_document;
        CAlfaDoc guide_document;
        const CSmartLine* real_cutter =
            load_first_sketch(real_cutter_path, cutter_document);
        const CSmartLine* real_guide =
            load_first_sketch(real_guide_path, guide_document);
        require(real_cutter && real_guide,
                "Real catalog milling sketches could not be loaded.");
        const MillingCutterPlacement real_cutter_placement =
            ResolveMillingCutterPlacement(*real_cutter);
        require(real_cutter_placement.rotated_legacy_profile
                    && real_cutter_placement.tip_at_cutting_depth
                    && std::abs(real_cutter_placement.angle_degrees - 180.0)
                        < 1.0e-9,
                "Cutter_1 was not oriented from its Y=0 tip datum.");
        CSmartLine placed_real_cutter("Placed Cutter_1");
        require(BuildPlacedSweptSectionSketch(
                    *real_cutter, closed_guide, placed_real_cutter,
                    0.0,
                    real_cutter_placement.delta_y - 12.0,
                    real_cutter_placement.angle_degrees),
                "Cutter_1 was not advanced to 12 mm depth.");
        double real_placed_min_y = std::numeric_limits<double>::max();
        double real_placed_max_y = std::numeric_limits<double>::lowest();
        for (const CPoint3d& point :
             placed_real_cutter.GetProfilePointsWorld()) {
            real_placed_min_y = std::min(real_placed_min_y, point.y);
            real_placed_max_y = std::max(real_placed_max_y, point.y);
        }
        require(real_placed_min_y < -5.3
                    && real_placed_max_y > 11.8
                    && real_placed_max_y < 12.0,
                "Cutter_1 did not cross the facade surface at its 12 mm section.");
        const std::vector<const CSmartLine*> real_guides{real_guide};
        const std::vector<const CSmartLine*> no_guides;
        const TopoDS_Shape real_baseline =
            CFacadeFurniture::BuildPlanarMilledFromProfiles(
                *real_cutter, no_guides,
                0.0, 0.0, 0.0, 500.0, 18.0, 700.0);
        TopoDS_Shape real_cutter_debug;
        const TopoDS_Shape real_facade =
            CFacadeFurniture::BuildPlanarMilledFromProfiles(
                *real_cutter, real_guides,
                0.0, 0.0, 0.0, 500.0, 18.0, 700.0,
                nullptr, true, &real_cutter_debug);
        const TopoDS_Shape real_shallow_facade =
            CFacadeFurniture::BuildPlanarMilledFromProfiles(
                *real_cutter, real_guides,
                0.0, 0.0, 0.0, 500.0, 18.0, 700.0,
                nullptr, true, nullptr, 3.0);
        const TopoDS_Shape real_overdepth_facade =
            CFacadeFurniture::BuildPlanarMilledFromProfiles(
                *real_cutter, real_guides,
                0.0, 0.0, 0.0, 500.0, 18.0, 700.0,
                nullptr, true, nullptr, 29.0);
        GProp_GProps real_facade_properties;
        GProp_GProps real_shallow_facade_properties;
        GProp_GProps real_baseline_properties;
        GProp_GProps real_cutter_properties;
        BRepGProp::VolumeProperties(real_facade, real_facade_properties);
        BRepGProp::VolumeProperties(
            real_shallow_facade, real_shallow_facade_properties);
        BRepGProp::VolumeProperties(real_baseline, real_baseline_properties);
        BRepGProp::VolumeProperties(real_cutter_debug, real_cutter_properties);
        const TopoDS_Shape real_limited_cutter =
            LimitMillingCutterToFacadeDepth(
                real_cutter_debug, real_baseline, 9.0);
        GProp_GProps real_limited_cutter_properties;
        BRepGProp::VolumeProperties(
            real_limited_cutter, real_limited_cutter_properties);
        require(!real_cutter_debug.IsNull()
                    && real_cutter_properties.Mass() > 1.0,
                "Cutter_1 did not create a physical milling envelope.");
        require(!real_limited_cutter.IsNull()
                    && real_limited_cutter_properties.Mass() > 1000.0,
                "Cutter_1 depth limiter produced no cutting volume.");
        require(!real_facade.IsNull()
                    && real_facade_properties.Mass()
                        < real_baseline_properties.Mass() - 1000.0,
                "Cutter_1 did not mill the facade with Varian-1.");
        require(!real_shallow_facade.IsNull()
                    && real_shallow_facade_properties.Mass()
                        < real_baseline_properties.Mass() - 1000.0
                    && real_shallow_facade_properties.Mass()
                        > real_facade_properties.Mass() + 1000.0,
                "Milling Depth did not change the removed facade volume.");
        const TopoDS_Shape front_test_slab = BRepPrimAPI_MakeBox(
            gp_Pnt(0.0, 0.0, 0.0), 500.0, 0.5, 700.0).Shape();
        BRepAlgoAPI_Common baseline_front_common(
            real_baseline, front_test_slab);
        baseline_front_common.Build();
        BRepAlgoAPI_Common milled_front_common(real_facade, front_test_slab);
        milled_front_common.Build();
        GProp_GProps baseline_front_properties;
        GProp_GProps milled_front_properties;
        BRepGProp::VolumeProperties(
            baseline_front_common.Shape(), baseline_front_properties);
        BRepGProp::VolumeProperties(
            milled_front_common.Shape(), milled_front_properties);
        require(baseline_front_common.IsDone()
                    && milled_front_common.IsDone()
                    && milled_front_properties.Mass()
                        < baseline_front_properties.Mass() - 1000.0,
                "Catalog groove is enclosed below the facade front face.");
        const TopoDS_Shape rear_test_slab = BRepPrimAPI_MakeBox(
            gp_Pnt(50.0, 17.0, 50.0), 400.0, 1.0, 600.0).Shape();
        BRepAlgoAPI_Common rear_common(real_facade, rear_test_slab);
        rear_common.Build();
        GProp_GProps rear_properties;
        BRepGProp::VolumeProperties(rear_common.Shape(), rear_properties);
        require(rear_common.IsDone() && rear_properties.Mass() > 239000.0,
                "Catalog cutter passed through to the rear facade face.");
        const TopoDS_Shape rear_skin_slab = BRepPrimAPI_MakeBox(
            gp_Pnt(50.0, 17.9, 50.0), 400.0, 0.1, 600.0).Shape();
        BRepAlgoAPI_Common overdepth_rear_common(
            real_overdepth_facade, rear_skin_slab);
        overdepth_rear_common.Build();
        GProp_GProps overdepth_rear_properties;
        BRepGProp::VolumeProperties(
            overdepth_rear_common.Shape(), overdepth_rear_properties);
        require(!real_overdepth_facade.IsNull()
                    && overdepth_rear_common.IsDone()
                    && overdepth_rear_properties.Mass() > 23900.0,
                "Over-depth milling broke through the rear facade skin.");
    }
    NikaKitchenDefinition nika;
    auto nika_parts = CNikaKitchenFurniture::BuildParts(nika);
    require(nika_parts.size() > 100,
            "Nika-260 kitchen did not create its component parts.");
    bool has_worktop = false;
    double worktop_volume = 0.0;
    int showcase_glass_count = 0;
    int drawer_facade_count = 0;
    for (const auto& part : nika_parts) {
        require(part != nullptr, "Nika-260 kitchen contains an invalid part.");
        has_worktop = has_worktop || part->GetName() == "Nika Worktop";
        if (part->GetName() == "Nika Worktop") {
            const auto* worktop = dynamic_cast<const CSolid*>(part.get());
            require(worktop != nullptr,
                    "Nika-260 kitchen worktop is not a solid.");
            GProp_GProps worktop_properties;
            BRepGProp::VolumeProperties(
                worktop->m_Shape, worktop_properties);
            worktop_volume = worktop_properties.Mass();
        }
        showcase_glass_count +=
            part->GetName().find("Showcase Glass") != std::string::npos ? 1 : 0;
        drawer_facade_count +=
            part->GetName().find("Drawer") != std::string::npos
                && part->GetName().find("Facade") != std::string::npos ? 1 : 0;
    }
    require(has_worktop, "Nika-260 kitchen worktop is missing.");
    require(worktop_volume > 0.0
                && worktop_volume < 2630.0 * 540.0 * 38.0 - 1000.0,
            "Nika-260 kitchen worktop front edge was not rounded.");
    require(showcase_glass_count == 2,
            "Nika-260 kitchen must contain two showcase glass panels.");
    require(drawer_facade_count >= 8,
            "Nika-260 kitchen must contain two four-drawer units.");

    for (int facade_style = 3; facade_style <= 4; ++facade_style) {
        NikaKitchenDefinition extended_nika;
        extended_nika.facade_style = facade_style;
        const auto extended_parts =
            CNikaKitchenFurniture::BuildParts(extended_nika);
        const bool has_extended_facade = std::any_of(
            extended_parts.begin(), extended_parts.end(),
            [](const auto& part) {
                return part
                    && part->GetName().find("Facade Panel")
                        != std::string::npos;
            });
        require(has_extended_facade,
                "One of the two added Nika facade styles was not built.");
    }

    const auto bounds_for_name = [](
        const std::vector<std::unique_ptr<CAlfaObject>>& parts,
        const std::string& name, Vec3& minimum, Vec3& maximum) {
        const auto part = std::find_if(
            parts.begin(), parts.end(), [&name](const auto& item) {
                return item && item->GetName() == name;
            });
        return part != parts.end() && (*part)->GetBounds(minimum, maximum);
    };
    Vec3 left_handle_min{};
    Vec3 left_handle_max{};
    Vec3 right_handle_min{};
    Vec3 right_handle_max{};
    require(bounds_for_name(
                nika_parts, "Nika Lower 4 Left Door Handle Bar",
                left_handle_min, left_handle_max)
            && bounds_for_name(
                nika_parts, "Nika Lower 4 Right Door Handle Bar",
                right_handle_min, right_handle_max),
            "Nika-260 paired-door handles are missing.");
    const double left_handle_x =
        (left_handle_min.x + left_handle_max.x) * 0.5;
    const double right_handle_x =
        (right_handle_min.x + right_handle_max.x) * 0.5;
    require(left_handle_x < right_handle_x
                && right_handle_x - left_handle_x < 100.0,
            "Nika-260 paired-door handles are not mirrored at the joint.");

    NikaKitchenDefinition open_door_nika;
    open_door_nika.door_open_angles[2] = 90.0;
    auto open_door_parts = CNikaKitchenFurniture::BuildParts(open_door_nika);
    Vec3 closed_door_min{};
    Vec3 closed_door_max{};
    Vec3 open_door_min{};
    Vec3 open_door_max{};
    require(bounds_for_name(
                nika_parts, "Nika Lower 4 Right Door Facade Panel",
                closed_door_min, closed_door_max)
            && bounds_for_name(
                open_door_parts, "Nika Lower 4 Right Door Facade Panel",
                open_door_min, open_door_max)
            && open_door_min.y < closed_door_min.y - 200.0,
            "Nika-260 right door did not open around its outside hinge.");

    NikaKitchenDefinition open_drawer_nika;
    open_drawer_nika.open_drawer = 1;
    open_drawer_nika.pullout_distance = 300.0;
    auto open_drawer_parts = CNikaKitchenFurniture::BuildParts(open_drawer_nika);
    Vec3 closed_drawer_min{};
    Vec3 closed_drawer_max{};
    Vec3 open_drawer_min{};
    Vec3 open_drawer_max{};
    require(bounds_for_name(
                nika_parts, "Nika Lower 2 Drawer 1 Bottom",
                closed_drawer_min, closed_drawer_max)
            && bounds_for_name(
                open_drawer_parts, "Nika Lower 2 Drawer 1 Bottom",
                open_drawer_min, open_drawer_max)
            && std::abs((closed_drawer_min.y - open_drawer_min.y) - 300.0)
                < 1.0,
            "Nika-260 drawer body did not move with its facade.");

    CornerKitchenDefinition corner_kitchen;
    auto corner_parts = CCornerKitchenFurniture::BuildParts(corner_kitchen);
    require(corner_parts.size() > nika_parts.size() * 1.8,
            "Corner kitchen did not create both furniture runs.");
    bool has_left_worktop = false;
    bool has_right_worktop = false;
    bool has_lower_corner_cabinet = false;
    bool has_upper_corner_cabinet = false;
    Vec3 corner_minimum{1.0e9f, 1.0e9f, 1.0e9f};
    Vec3 corner_maximum{-1.0e9f, -1.0e9f, -1.0e9f};
    for (const auto& part : corner_parts) {
        require(part != nullptr, "Corner kitchen contains an invalid part.");
        has_left_worktop = has_left_worktop
            || part->GetName() == "Corner Left Worktop";
        has_right_worktop = has_right_worktop
            || part->GetName() == "Corner Right Worktop";
        has_lower_corner_cabinet = has_lower_corner_cabinet
            || part->GetName().find("Corner Base Corner Cabinet") == 0;
        has_upper_corner_cabinet = has_upper_corner_cabinet
            || part->GetName().find("Corner Upper Corner Cabinet") == 0;
        Vec3 part_minimum{};
        Vec3 part_maximum{};
        if (part->GetBounds(part_minimum, part_maximum)) {
            corner_minimum.x = std::min(corner_minimum.x, part_minimum.x);
            corner_minimum.y = std::min(corner_minimum.y, part_minimum.y);
            corner_minimum.z = std::min(corner_minimum.z, part_minimum.z);
            corner_maximum.x = std::max(corner_maximum.x, part_maximum.x);
            corner_maximum.y = std::max(corner_maximum.y, part_maximum.y);
            corner_maximum.z = std::max(corner_maximum.z, part_maximum.z);
        }
    }
    require(has_left_worktop && has_right_worktop,
            "Corner kitchen worktops are missing.");
    require(has_lower_corner_cabinet && has_upper_corner_cabinet,
            "Corner kitchen does not contain real corner cabinets.");
    require(corner_maximum.x - corner_minimum.x > 2700.0
                && corner_maximum.y - corner_minimum.y > 1800.0,
            "Corner kitchen does not have an L-shaped footprint.");

    Vec3 left_worktop_min{};
    Vec3 left_worktop_max{};
    Vec3 right_worktop_min{};
    Vec3 right_worktop_max{};
    require(bounds_for_name(corner_parts, "Corner Left Worktop",
                            left_worktop_min, left_worktop_max)
            && bounds_for_name(corner_parts, "Corner Right Worktop",
                               right_worktop_min, right_worktop_max)
            && left_worktop_max.x - left_worktop_min.x
                > left_worktop_max.y - left_worktop_min.y
            && right_worktop_max.y - right_worktop_min.y
                > right_worktop_max.x - right_worktop_min.x,
            "Corner kitchen right run has the wrong orientation.");

    CornerKitchenDefinition open_corner_kitchen;
    open_corner_kitchen.lower_corner_door_angle = 90.0;
    auto open_corner_parts = CCornerKitchenFurniture::BuildParts(
        open_corner_kitchen);
    Vec3 closed_corner_handle_min{};
    Vec3 closed_corner_handle_max{};
    Vec3 open_corner_handle_min{};
    Vec3 open_corner_handle_max{};
    require(bounds_for_name(
                corner_parts,
                "Corner Base Corner Cabinet Right Facade Handle",
                closed_corner_handle_min, closed_corner_handle_max)
            && bounds_for_name(
                open_corner_parts,
                "Corner Base Corner Cabinet Right Facade Handle",
                open_corner_handle_min, open_corner_handle_max)
            && (std::abs(open_corner_handle_min.x - closed_corner_handle_min.x) > 10.0
                || std::abs(open_corner_handle_min.y - closed_corner_handle_min.y) > 10.0),
            "Corner kitchen corner door did not open.");

    ToolRegistry component_tools;
    const auto set_tool_parameter = [](std::vector<ToolParameter>& parameters,
                                       const std::string& id,
                                       double value) {
        for (ToolParameter& parameter : parameters) {
            if (parameter.id == id) {
                parameter.value = value;
                return;
            }
        }
    };

    const ToolDefinition* radius_assembly_tool =
        component_tools.Find("cabinet_advanced");
    require(radius_assembly_tool != nullptr,
            "Cabinet Advanced tool is not registered.");
    std::vector<ToolParameter> radius_assembly_parameters =
        radius_assembly_tool->defaults;
    set_tool_parameter(radius_assembly_parameters, "body_type", 5.0);
    set_tool_parameter(radius_assembly_parameters, "facade_type", 1.0);
    set_tool_parameter(radius_assembly_parameters, "facade_style", 4.0);
    CAlfaDoc radius_assembly_document;
    ActiveParametricObject radius_assembly_object =
        component_tools.CreateParametricObject(
            "cabinet_advanced", radius_assembly_document,
            radius_assembly_parameters);
    const auto radius_facade_profile_count = [&]() {
        if (radius_assembly_object.object_index
            >= radius_assembly_document.GetObjects().size()) {
            return 0;
        }
        const auto* cabinet = dynamic_cast<const CKitchenCabinet*>(
            radius_assembly_document.GetObjects()[
                radius_assembly_object.object_index].get());
        if (!cabinet) return 0;
        int profile_count = 0;
        for (unsigned long id : cabinet->GetElementIds()) {
            const CAlfaObject* child =
                radius_assembly_document.FindObjectById(id);
            if (child
                && child->GetName().find(
                    "Radius-3 Cabinet Facade ") == 0
                && child->GetName().find(" Profile") != std::string::npos) {
                ++profile_count;
            }
        }
        return profile_count;
    };
    require(radius_facade_profile_count() == 4,
            "Radius Milano did not create four individual profiles.");
    set_tool_parameter(radius_assembly_object.parameters, "width", 640.0);
    component_tools.Rebuild(
        radius_assembly_object, radius_assembly_document);
    require(radius_facade_profile_count() == 4,
            "Rebuilding a radius cabinet destroyed its four profiles.");

    const ToolDefinition* room_tool = component_tools.Find("room");
    require(room_tool != nullptr && room_tool->defaults.size() == 6,
            "Room tool is not registered with its architecture parameters.");
    CAlfaDoc room_document;
    ActiveParametricObject room_object =
        component_tools.CreateParametricObject(
            "room", room_document, room_tool->defaults);
    auto* room_assembly = room_object.object_index < room_document.GetObjects().size()
        ? dynamic_cast<CAssembled*>(
              room_document.GetObjects()[room_object.object_index].get())
        : nullptr;
    require(room_assembly && room_assembly->GetElementIds().size() == 5,
            "Room must contain a floor and four walls.");
    set_tool_parameter(room_object.parameters, "ceiling", 1.0);
    component_tools.Rebuild(room_object, room_document);
    room_assembly = dynamic_cast<CAssembled*>(
        room_document.GetObjects()[room_object.object_index].get());
    require(room_assembly && room_assembly->GetElementIds().size() == 6,
            "Room ceiling parameter did not rebuild the assembly.");
    const Material* room_wall_material = room_document.FindMaterial(
        "Room Wall Textile 24708", true);
    const Material* room_floor_material = room_document.FindMaterial(
        "Room Floor Unopark Merbau", true);
    const Material* room_ceiling_material = room_document.FindMaterial(
        "Room Ceiling White Paint", true);
    const Material* room_window_glass_material = room_document.FindMaterial(
        "Room Window Clear Glass", true);
    require(room_wall_material
                && room_wall_material->color_texture_path
                    == "texture/textiles/24708.jpg",
            "Room wall textile material was not created.");
    require(room_floor_material
                && room_floor_material->color_texture_path
                    == "texture/parquet/Unopark_Merbau.jpg",
            "Room floor Merbau material was not created.");
    require(room_ceiling_material
                && room_ceiling_material->diffuse.r > 0.95f
                && room_ceiling_material->ambient.r > 0.7f
                && room_ceiling_material->emission.r > 0.05f,
            "Room white ceiling material was not created.");
    require(room_window_glass_material
                && room_window_glass_material->alpha < 0.25f
                && room_window_glass_material->roughness < 0.05f,
            "Room window glass material was not created.");
    const CAlfaObject* room_floor = room_document.FindObjectById(
        room_assembly->GetElementIds().front());
    require(room_floor
                && room_floor->GetMaterialId() == room_floor_material->id,
            "Room floor did not receive the Merbau material.");
    const CAlfaObject* room_ceiling = room_document.FindObjectById(
        room_assembly->GetElementIds()[5]);
    require(room_ceiling
                && room_ceiling->GetMaterialId() == room_ceiling_material->id,
            "Room ceiling did not receive the white paint material.");
    const RenderScene room_render_scene =
        BuildRenderScene(room_document, Camera{}, false);
    const bool rendered_white_ceiling = std::any_of(
        room_render_scene.meshes.begin(), room_render_scene.meshes.end(),
        [&room_render_scene](const RenderMesh& mesh) {
            return mesh.name.startsWith("Room Ceiling Surface")
                && mesh.material_index >= 0
                && mesh.material_index
                    < static_cast<int>(room_render_scene.materials.size())
                && room_render_scene.materials[mesh.material_index].name
                    == "Room Ceiling White Paint";
        });
    require(rendered_white_ceiling,
            "Room renderer did not receive the white ceiling material.");
    for (size_t wall_index = 1; wall_index <= 4; ++wall_index) {
        const auto* material_wall = dynamic_cast<const CSolid*>(
            room_document.FindObjectById(
                room_assembly->GetElementIds()[wall_index]));
        int textile_faces = 0;
        if (material_wall) {
            for (int surface_index = 0;
                 surface_index < material_wall->GetNumSurfaces();
                 ++surface_index) {
                const CSurfaceFace* surface =
                    material_wall->GetSurfaceFace(surface_index);
                if (surface && surface->MaterialOverride.enabled
                    && surface->MaterialOverride.material_id
                        == room_wall_material->id) {
                    ++textile_faces;
                }
            }
        }
        require(textile_faces == 1,
                "Textile material must be assigned only to the inner wall face.");
    }
    require(component_tools.HasVisibleArchitectureWalls(room_document),
            "Visible room walls were not detected for opening placement.");
    room_assembly->SetVisible(false);
    require(!component_tools.HasVisibleArchitectureWalls(room_document),
            "Walls of a hidden room must not request opening placement.");
    room_assembly->SetVisible(true);

    const ToolDefinition* window_tool = component_tools.Find("window");
    require(window_tool != nullptr && window_tool->defaults.size() == 13,
            "Window tool is not registered with its architecture parameters.");
    CAlfaDoc window_document;
    require(!component_tools.HasVisibleArchitectureWalls(window_document),
            "An empty document must not request wall placement.");
    ActiveParametricObject window_object =
        component_tools.CreateParametricObject(
            "window", window_document, window_tool->defaults);
    auto* window_assembly = window_object.object_index
            < window_document.GetObjects().size()
        ? dynamic_cast<CAssembled*>(
              window_document.GetObjects()[window_object.object_index].get())
        : nullptr;
    require(window_assembly && window_assembly->GetElementIds().size() == 7,
            "Window must contain a frame, center mullion, and two glass panes.");
    const Material* window_glass_material = window_document.FindMaterial(
        "Room Window Clear Glass", true);
    int material_glass_panes = 0;
    for (const unsigned long element_id : window_assembly->GetElementIds()) {
        const CAlfaObject* window_part =
            window_document.FindObjectById(element_id);
        if (window_part
            && window_part->GetName().rfind("Window Glass ", 0) == 0
            && window_glass_material
            && window_part->GetMaterialId() == window_glass_material->id) {
            ++material_glass_panes;
        }
    }
    require(material_glass_panes == 2,
            "Clear glass material was not assigned to both window panes.");
    set_tool_parameter(window_object.parameters, "vertical_bars", 2.0);
    set_tool_parameter(window_object.parameters, "horizontal_bars", 1.0);
    component_tools.Rebuild(window_object, window_document);
    window_assembly = dynamic_cast<CAssembled*>(
        window_document.GetObjects()[window_object.object_index].get());
    require(window_assembly && window_assembly->GetElementIds().size() == 10,
            "Window muntin parameters did not rebuild the assembly.");

    const ToolDefinition* door_tool = component_tools.Find("door");
    require(door_tool != nullptr && door_tool->defaults.size() == 12,
            "Door tool is not registered with its architecture parameters.");
    CAlfaDoc door_document;
    ActiveParametricObject door_object =
        component_tools.CreateParametricObject(
            "door", door_document, door_tool->defaults);
    auto* door_assembly = door_object.object_index < door_document.GetObjects().size()
        ? dynamic_cast<CAssembled*>(
              door_document.GetObjects()[door_object.object_index].get())
        : nullptr;
    require(door_assembly && door_assembly->GetElementIds().size() == 9,
            "Glass door must contain a frame, glazed leaf, and handle.");
    set_tool_parameter(door_object.parameters, "type", 1.0);
    set_tool_parameter(door_object.parameters, "double_door", 1.0);
    component_tools.Rebuild(door_object, door_document);
    door_assembly = dynamic_cast<CAssembled*>(
        door_document.GetObjects()[door_object.object_index].get());
    require(door_assembly && door_assembly->GetElementIds().size() == 6,
            "Double door parameters did not rebuild the assembly.");

    Vec3 standalone_window_min{};
    Vec3 standalone_window_max{};
    const CAlfaObject* standalone_window_part =
        window_document.FindObjectById(window_assembly->GetElementIds().front());
    require(standalone_window_part
            && standalone_window_part->GetBounds(
                standalone_window_min, standalone_window_max)
            && standalone_window_max.y - standalone_window_min.y > 1200.0
            && standalone_window_max.z - standalone_window_min.z < 100.0,
            "A standalone window must lie on the XY plane.");

    const unsigned long room_id = room_assembly->m_id;
    const unsigned long front_wall_id = room_assembly->GetElementIds()[1];
    auto* front_wall = dynamic_cast<CSolid*>(
        room_document.FindObjectById(front_wall_id));
    GProp_GProps original_front_properties;
    require(front_wall != nullptr, "Room front wall is missing.");
    BRepGProp::VolumeProperties(
        front_wall->m_Shape, original_front_properties);

    ActiveParametricObject attached_window =
        component_tools.ActivateArchitectureOpening(
            "window", room_document, front_wall_id,
            CPoint3d(-1000.0, -2000.0, 1000.0));
    require(!attached_window.tool_id.empty(),
            "Window could not be attached to the selected wall.");
    const auto attached_host = std::find_if(
        attached_window.parameters.begin(), attached_window.parameters.end(),
        [](const ToolParameter& parameter) {
            return parameter.id == "host.wall.id";
        });
    require(attached_host != attached_window.parameters.end()
                && static_cast<unsigned long>(attached_host->value)
                    == front_wall_id,
            "Window did not keep its host-wall association.");
    const auto clicked_distance = std::find_if(
        attached_window.parameters.begin(), attached_window.parameters.end(),
        [](const ToolParameter& parameter) {
            return parameter.id == "distance_along_wall";
        });
    require(clicked_distance != attached_window.parameters.end()
                && std::abs(clicked_distance->value - 2000.0) < 1.0e-6,
            "Wall click did not set the distance along the wall axis.");

    front_wall = dynamic_cast<CSolid*>(
        room_document.FindObjectById(front_wall_id));
    GProp_GProps window_cut_properties;
    BRepGProp::VolumeProperties(front_wall->m_Shape, window_cut_properties);
    require(window_cut_properties.Mass()
                < original_front_properties.Mass() - 1000000.0,
            "Attached window did not cut an opening in the wall.");

    set_tool_parameter(attached_window.parameters, "width", 1400.0);
    component_tools.Rebuild(attached_window, room_document);
    front_wall = dynamic_cast<CSolid*>(
        room_document.FindObjectById(front_wall_id));
    GProp_GProps enlarged_window_properties;
    BRepGProp::VolumeProperties(
        front_wall->m_Shape, enlarged_window_properties);
    require(enlarged_window_properties.Mass()
                < window_cut_properties.Mass() - 500000.0,
            "Changing window parameters did not update the wall opening.");

    CAlfaObject* attached_window_object =
        room_document.GetObjects()[attached_window.object_index].get();
    const unsigned long attached_window_id = attached_window_object->m_id;
    require(room_document.SelectObjectById(attached_window_id)
                && room_document.DeleteSelectedObject(),
            "Attached window could not be deleted.");
    component_tools.RebuildArchitectureRooms(room_document);
    front_wall = dynamic_cast<CSolid*>(
        room_document.FindObjectById(front_wall_id));
    GProp_GProps restored_front_properties;
    BRepGProp::VolumeProperties(
        front_wall->m_Shape, restored_front_properties);
    require(std::abs(restored_front_properties.Mass()
                     - original_front_properties.Mass()) < 1.0,
            "Deleting an attached window did not restore the wall.");

    room_assembly = dynamic_cast<CAssembled*>(
        room_document.FindObjectById(room_id));
    const unsigned long right_wall_id = room_assembly->GetElementIds()[4];
    auto* right_wall = dynamic_cast<CSolid*>(
        room_document.FindObjectById(right_wall_id));
    GProp_GProps original_right_properties;
    BRepGProp::VolumeProperties(right_wall->m_Shape, original_right_properties);
    ActiveParametricObject attached_door =
        component_tools.ActivateArchitectureOpening(
            "door", room_document, right_wall_id,
            CPoint3d(2900.0, 500.0, 1000.0));
    right_wall = dynamic_cast<CSolid*>(
        room_document.FindObjectById(right_wall_id));
    GProp_GProps door_cut_properties;
    BRepGProp::VolumeProperties(right_wall->m_Shape, door_cut_properties);
    require(!attached_door.tool_id.empty()
                && door_cut_properties.Mass()
                    < original_right_properties.Mass() - 1000000.0,
            "Attached door did not cut an opening in a transverse wall.");

    CAlfaDoc showcase_frame_document;
    const ToolDefinition* showcase_frame_tool =
        component_tools.Find("cabinet_showcase");
    require(showcase_frame_tool != nullptr,
            "Cabinet Showcase tool is not registered.");
    const auto showcase_style_parameter = std::find_if(
        showcase_frame_tool->defaults.begin(), showcase_frame_tool->defaults.end(),
        [](const ToolParameter& parameter) {
            return parameter.id == "facade_style";
        });
    require(showcase_style_parameter != showcase_frame_tool->defaults.end()
                && showcase_style_parameter->options.size() == 5,
            "Cabinet Showcase does not expose all five facade styles.");
    CAlfaDoc showcase_milano_document;
    std::vector<ToolParameter> showcase_milano_parameters =
        showcase_frame_tool->defaults;
    for (ToolParameter& parameter : showcase_milano_parameters) {
        if (parameter.id == "facade_style") parameter.value = 4.0;
        if (parameter.id == "facade_showcase") parameter.value = 0.0;
    }
    const ActiveParametricObject showcase_milano =
        component_tools.CreateParametricObject(
            "cabinet_showcase", showcase_milano_document,
            showcase_milano_parameters);
    const auto* showcase_milano_cabinet = showcase_milano.object_index
            < showcase_milano_document.GetObjects().size()
        ? dynamic_cast<const CKitchenCabinet*>(
            showcase_milano_document.GetObjects()[
                showcase_milano.object_index].get())
        : nullptr;
    require(showcase_milano_cabinet
                && showcase_milano_cabinet->GetDefinition().facade_style
                    == KitchenCabinetFacadeStyle::Milano,
            "Cabinet Showcase clamps the fifth facade style.");
    std::vector<ToolParameter> showcase_frame_parameters =
        showcase_frame_tool->defaults;
    for (ToolParameter& parameter : showcase_frame_parameters) {
        if (parameter.id == "facade_style") parameter.value = 1.0;
        if (parameter.id == "facade_showcase") parameter.value = 1.0;
        if (parameter.id == "showcase_fill") parameter.value = 0.0;
    }
    ActiveParametricObject showcase_frame =
        component_tools.CreateParametricObject(
            "cabinet_showcase", showcase_frame_document,
            showcase_frame_parameters);
    auto* showcase_frame_assembly = showcase_frame.object_index
            < showcase_frame_document.GetObjects().size()
        ? dynamic_cast<CAssembled*>(showcase_frame_document.GetObjects()[
              showcase_frame.object_index].get())
        : nullptr;
    bool showcase_has_glass = false;
    bool showcase_has_profiled_frame = false;
    bool showcase_has_wood_panel = false;
    if (showcase_frame_assembly) {
        for (unsigned long id : showcase_frame_assembly->GetElementIds()) {
            const CAlfaObject* part = showcase_frame_document.FindObjectById(id);
            if (!part) continue;
            showcase_has_glass = showcase_has_glass
                || part->GetName().find("Showcase Glass") != std::string::npos;
            showcase_has_profiled_frame = showcase_has_profiled_frame
                || part->GetName().find("Showcase Frame") != std::string::npos;
            showcase_has_wood_panel = showcase_has_wood_panel
                || part->GetName().find("Center Panel") != std::string::npos;
        }
    }
    require(showcase_frame_assembly
                && showcase_has_glass
                && showcase_has_profiled_frame
                && !showcase_has_wood_panel,
            "Showcase Frame must contain glass and frame without a wood panel.");
    for (ToolParameter& parameter : showcase_frame.parameters) {
        if (parameter.id == "facade_style") parameter.value = 0.0;
        if (parameter.id == "facade_showcase") parameter.value = 0.0;
    }
    component_tools.Rebuild(showcase_frame, showcase_frame_document);
    showcase_frame_assembly = dynamic_cast<CAssembled*>(
        showcase_frame_document.GetObjects()[
            showcase_frame.object_index].get());
    bool plain_facade_found = false;
    bool plain_has_showcase_parts = false;
    if (showcase_frame_assembly) {
        for (unsigned long id : showcase_frame_assembly->GetElementIds()) {
            const CAlfaObject* part = showcase_frame_document.FindObjectById(id);
            if (!part) continue;
            plain_facade_found = plain_facade_found
                || (part->GetName().find("Facade") != std::string::npos
                    && part->GetName().find("Handle") == std::string::npos);
            plain_has_showcase_parts = plain_has_showcase_parts
                || part->GetName().find("Showcase") != std::string::npos
                || part->GetName().find("Center Panel") != std::string::npos;
        }
    }
    require(showcase_frame_assembly
                && plain_facade_found
                && !plain_has_showcase_parts,
            "Showcase Plain must be a solid facade without showcase parts.");
    for (ToolParameter& parameter : showcase_frame.parameters) {
        if (parameter.id == "facade_showcase") parameter.value = 1.0;
        if (parameter.id == "showcase_fill") parameter.value = 3.0;
    }
    component_tools.Rebuild(showcase_frame, showcase_frame_document);
    showcase_frame_assembly = dynamic_cast<CAssembled*>(
        showcase_frame_document.GetObjects()[
            showcase_frame.object_index].get());
    bool plain_showcase_has_glass = false;
    bool plain_showcase_has_stained_wire = false;
    if (showcase_frame_assembly) {
        for (unsigned long id : showcase_frame_assembly->GetElementIds()) {
            const CAlfaObject* part = showcase_frame_document.FindObjectById(id);
            if (!part) continue;
            plain_showcase_has_glass = plain_showcase_has_glass
                || part->GetName().find("Showcase Glass") != std::string::npos;
            plain_showcase_has_stained_wire = plain_showcase_has_stained_wire
                || part->GetName().find("Stained Glass Wire")
                    != std::string::npos;
        }
    }
    require(showcase_frame_assembly
                && plain_showcase_has_glass
                && plain_showcase_has_stained_wire,
            "Plain Facade Showcase did not apply the selected fill.");

    CAlfaDoc overhead_cabinet_document;
    const ToolDefinition* overhead_cabinet_tool =
        component_tools.Find("cabinet");
    require(overhead_cabinet_tool != nullptr,
            "Cabinet tool is not registered.");
    std::vector<ToolParameter> overhead_parameters =
        overhead_cabinet_tool->defaults;
    for (ToolParameter& parameter : overhead_parameters) {
        if (parameter.id == "overhead") parameter.value = 1.0;
        if (parameter.id == "mounting_height") parameter.value = 1300.0;
        if (parameter.id == "facade_type") parameter.value = 0.0;
    }
    ActiveParametricObject overhead_cabinet =
        component_tools.CreateParametricObject(
            "cabinet", overhead_cabinet_document, overhead_parameters);
    auto* overhead_assembly = overhead_cabinet.object_index
            < overhead_cabinet_document.GetObjects().size()
        ? dynamic_cast<CAssembled*>(overhead_cabinet_document.GetObjects()[
              overhead_cabinet.object_index].get())
        : nullptr;
    Vec3 overhead_before_min{};
    Vec3 overhead_before_max{};
    require(overhead_assembly
                && overhead_assembly->GetBounds(
                    overhead_before_min, overhead_before_max)
                && std::abs(overhead_before_min.z - 1300.0) < 1.0,
            "Overhead cabinet was not placed at Height Hanger.");
    overhead_assembly->Translate({75.0f, 20.0f, 0.0f});
    for (ToolParameter& parameter : overhead_cabinet.parameters) {
        if (parameter.id == "mounting_height") parameter.value = 1500.0;
    }
    component_tools.Rebuild(overhead_cabinet, overhead_cabinet_document);
    overhead_assembly = dynamic_cast<CAssembled*>(
        overhead_cabinet_document.GetObjects()[
            overhead_cabinet.object_index].get());
    Vec3 overhead_after_min{};
    Vec3 overhead_after_max{};
    require(overhead_assembly
                && overhead_assembly->GetBounds(
                    overhead_after_min, overhead_after_max)
                && std::abs(overhead_after_min.z - 1500.0) < 1.0
                && std::abs(
                    (overhead_after_min.x + overhead_after_max.x) * 0.5
                    - 75.0) < 1.0,
            "Height Hanger rebuild lost the cabinet placement.");

    CAlfaDoc material_cabinet_document;
    ActiveParametricObject material_cabinet =
        component_tools.CreateParametricObject(
            "cabinet", material_cabinet_document,
            overhead_cabinet_tool->defaults);
    auto* material_cabinet_assembly = material_cabinet.object_index
            < material_cabinet_document.GetObjects().size()
        ? dynamic_cast<CAssembled*>(material_cabinet_document.GetObjects()[
              material_cabinet.object_index].get())
        : nullptr;
    require(material_cabinet_assembly != nullptr,
            "Material fast-path cabinet was not created.");
    const std::size_t material_object_count =
        material_cabinet_document.GetObjects().size();
    std::vector<std::pair<unsigned long, const CAlfaObject*>> material_part_pointers;
    for (unsigned long id : material_cabinet_assembly->GetElementIds()) {
        material_part_pointers.push_back(
            {id, material_cabinet_document.FindObjectById(id)});
    }
    Material instant_facade = Material::DefaultGloss();
    instant_facade.name = "Instant Facade Test";
    instant_facade.diffuse = {0.12f, 0.34f, 0.56f};
    instant_facade.id = 0;
    const unsigned long instant_facade_id =
        material_cabinet_document.UpsertMaterial(instant_facade).id;
    for (ToolParameter& parameter : material_cabinet.parameters) {
        if (parameter.id == "facade_material_id") {
            parameter.value = static_cast<double>(instant_facade_id);
        }
    }
    require(component_tools.ApplyFurnitureMaterialParameter(
                material_cabinet, material_cabinet_document,
                "facade_material_id"),
            "Cabinet facade material fast path failed.");
    require(material_cabinet_document.GetObjects().size()
                == material_object_count,
            "Facade material fast path changed the scene object count.");
    bool instant_facade_found = false;
    for (const auto& [id, pointer] : material_part_pointers) {
        const CAlfaObject* part = material_cabinet_document.FindObjectById(id);
        require(part == pointer,
                "Facade material fast path rebuilt a cabinet part.");
        if (part && part->GetName().find("Facade") != std::string::npos) {
            instant_facade_found = true;
            require(part->GetMaterialId() == instant_facade_id,
                    "Facade material fast path did not update a facade part.");
        }
    }
    require(instant_facade_found,
            "Material fast-path cabinet has no facade parts.");
    const auto saved_facade_material = std::find_if(
        material_cabinet_assembly->GetParametricParameters().begin(),
        material_cabinet_assembly->GetParametricParameters().end(),
        [](const ParametricParameterValue& parameter) {
            return parameter.id == "facade_material_id";
        });
    require(saved_facade_material
                != material_cabinet_assembly->GetParametricParameters().end()
                && static_cast<unsigned long>(saved_facade_material->value)
                    == instant_facade_id,
            "Facade material fast path did not save its parameter value.");

    CAlfaDoc component_document;
    const ToolDefinition* drawer_tool = component_tools.Find("single_drawer");
    require(drawer_tool != nullptr,
            "Single Drawer tool is not registered.");
    ActiveParametricObject single_drawer =
        component_tools.CreateParametricObject(
            "single_drawer", component_document, drawer_tool->defaults);
    auto* drawer_assembly = single_drawer.object_index
            < component_document.GetObjects().size()
        ? dynamic_cast<CAssembled*>(component_document.GetObjects()[
              single_drawer.object_index].get())
        : nullptr;
    require(drawer_assembly && drawer_assembly->GetElementIds().size() >= 7,
            "Single Drawer did not create an independent assembly.");
    Vec3 drawer_before_min{};
    Vec3 drawer_before_max{};
    require(drawer_assembly->GetBounds(drawer_before_min, drawer_before_max),
            "Single Drawer has no bounds.");
    drawer_assembly->Translate({125.0f, 40.0f, 25.0f});
    const auto drawer_width = std::find_if(
        single_drawer.parameters.begin(), single_drawer.parameters.end(),
        [](const ToolParameter& parameter) { return parameter.id == "width"; });
    require(drawer_width != single_drawer.parameters.end(),
            "Single Drawer width parameter is missing.");
    drawer_width->value = 620.0;
    component_tools.Rebuild(single_drawer, component_document);
    drawer_assembly = dynamic_cast<CAssembled*>(component_document.GetObjects()[
        single_drawer.object_index].get());
    Vec3 drawer_after_min{};
    Vec3 drawer_after_max{};
    require(drawer_assembly
                && drawer_assembly->GetBounds(drawer_after_min, drawer_after_max)
                && std::abs(
                    (drawer_after_min.x + drawer_after_max.x) * 0.5 - 125.0)
                    < 1.0,
            "Single Drawer lost its placement after rebuilding.");

    CAlfaDoc facade_document;
    const ToolDefinition* facade_tool = component_tools.Find("single_facade");
    require(facade_tool != nullptr,
            "Single Facade tool is not registered.");
    ActiveParametricObject single_facade =
        component_tools.CreateParametricObject(
            "single_facade", facade_document, facade_tool->defaults);
    auto* facade_assembly = single_facade.object_index
            < facade_document.GetObjects().size()
        ? dynamic_cast<CAssembled*>(facade_document.GetObjects()[
              single_facade.object_index].get())
        : nullptr;
    require(facade_assembly && facade_assembly->GetElementIds().size() == 2,
            "Single Facade did not create an independent facade and handle.");
    const CAlfaObject* facade_handle = facade_document.FindObjectById(
        facade_assembly->GetElementIds()[1]);
    Vec3 facade_handle_min{};
    Vec3 facade_handle_max{};
    require(facade_handle
                && facade_handle->GetName() == "Single Facade Handle"
                && facade_handle->GetBounds(
                    facade_handle_min, facade_handle_max)
                && (facade_handle_min.x + facade_handle_max.x) * 0.5 > 100.0,
            "Single Facade handle is not positioned at the free edge.");
    Vec3 facade_before_min{};
    Vec3 facade_before_max{};
    require(facade_assembly->GetBounds(
                facade_before_min, facade_before_max),
            "Single Facade has no bounds.");
    facade_assembly->Translate({-160.0f, 70.0f, 35.0f});
    Vec3 facade_after_min{};
    Vec3 facade_after_max{};
    require(facade_assembly->GetBounds(facade_after_min, facade_after_max)
                && std::abs((facade_after_min.x - facade_before_min.x) + 160.0)
                    < 1.0
                && std::abs((facade_after_min.y - facade_before_min.y) - 70.0)
                    < 1.0,
            "Single Facade cannot be moved independently.");

    CAlfaDoc mixed_uv_document;
    auto mixed_uv_mesh = std::make_unique<CMesh3D>("Mixed UV curved patch");
    require(mixed_uv_mesh->SetGeometry(
                {{0.0f, 0.0f, 0.0f},
                 {100.0f, 0.0f, 0.0f},
                 {100.0f, 0.0f, 700.0f}},
                {CMesh3D::Face{0, 1, 2}},
                {{0.0f, 0.0f}, {1.5f, 0.0f}, {1.5f, 700.0f}}),
            "Mixed-unit UV regression mesh was not created.");
    mixed_uv_document.AddObject(std::move(mixed_uv_mesh));
    const RenderScene mixed_uv_scene = BuildRenderScene(
        mixed_uv_document, Camera{}, false);
    require(mixed_uv_scene.meshes.size() == 1
                && mixed_uv_scene.meshes.front().triangles.size() == 1,
            "Mixed-unit UV regression mesh was not exported.");
    const RenderTriangle& mixed_uv_triangle =
        mixed_uv_scene.meshes.front().triangles.front();
    require(std::abs(mixed_uv_triangle.uvs[1].u - 1.5f) < 0.0001f,
            "Angular U coordinate was incorrectly converted as millimetres.");
    require(std::abs(mixed_uv_triangle.uvs[2].v - 0.7f) < 0.0001f,
            "Physical V coordinate was not converted from millimetres.");

    return EXIT_SUCCESS;
}
