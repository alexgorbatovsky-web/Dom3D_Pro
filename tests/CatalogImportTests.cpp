#include <QSettings>
#include <future>
#include "solid/LowPolyCompletion.h"
#include "IgesIO.h"
#include "ThreeDSIO.h"
#include "ObjIO.h"
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
#include "CBSpline.h"
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
#include "ui/OpenGLViewport.h"
#include <QApplication>
#include <QSurfaceFormat>
#include "solid/Solid.h"
#include "solid/CircularSplineBoundary.h"
#include <GeomConvert.hxx>
#include <Geom_Circle.hxx>
#include <Geom_Ellipse.hxx>
#include <Geom_BSplineCurve.hxx>
#include "solid/SheetBendShapeBuilder.h"
#include "solid/SurfaceSet.h"
#include "StepIO.h"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepGProp.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
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
#include <BRepTools.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>
#include <Standard_Failure.hxx>
#include <gp_Elips.hxx>

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

#include "QuadroPipelineDiagnostics.h"
#include "QuadroBoundaryReport.h"

void TestPrismHollowFillet(const char* path);
void TestBezierPocketFillet(const char* path, const char* output);
void TestHolePlacement();
void TestPanelContour();
void TestPillowCadQuadro(const char* path, const char* outputPrefix);
void TestBridgeShellQuadro(const char* path, const char* outputPrefix);
void TestFrameCadQuadro(const char* path);
void TestFilletMeshNormals(const char* path);
void TestCylinderQuadroNormals(const char* path);
void TestTwoRailSurfaceDocument(const char* path);
void TestPeriodicBSpline(const char* path);
void TestTwoSketchCornerNormals(const char* path);
void TestHairdryerCadBoundary(const char* path);
void DiagnoseHairdryerChartFill(const char* path,const char* output);

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
    // and never closed. Rejecting collapsed residual rings now lets the
    // advancing front complete both strips without the old triangle fallback.
    const auto validate_strip = [](const CMesh3D& mesh,
                                   const std::vector<Vec3>& boundary) {
        bool manifold = false;
        require(ClosedMeshBoundaryLoopCount(mesh, manifold) == 1 && manifold
                    && ActiveFaceEdgeComponentCount(mesh) == 1,
                "Cylinder front retained an internal open seam.");
        double expected_area = 0.0;
        for (size_t i = 0; i < boundary.size(); ++i) {
            const Vec3 a = boundary[i];
            const Vec3 b = boundary[(i + 1) % boundary.size()];
            expected_area += static_cast<double>(a.x) * b.y
                - static_cast<double>(b.x) * a.y;
        }
        double actual_area = 0.0;
        std::map<std::pair<size_t, size_t>, size_t> edges;
        for (const auto& face : mesh.GetFaces()) {
            if (face.deleted) continue;
            double area = 0.0;
            for (size_t i = 0; i < face.corners.size(); ++i) {
                const size_t first = face.corners[i].v;
                const size_t second = face.corners[(i + 1) % face.corners.size()].v;
                require(first < mesh.GetVertices().size() && second < mesh.GetVertices().size(),
                        "Cylinder front has an invalid mesh index.");
                const Vec3 a = mesh.GetVertices()[first];
                const Vec3 b = mesh.GetVertices()[second];
                area += static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y;
                ++edges[std::minmax(first, second)];
            }
            require(area > 1.0e-8, "Cylinder front has an inverted or collapsed quad.");
            actual_area += area;
        }
        require(std::fabs(actual_area - std::fabs(expected_area)) < 1.0e-4,
                "Cylinder front did not cover the complete input contour.");
        size_t boundary_edges = 0;
        for (const auto& edge : edges) {
            if (edge.second != 1) continue;
            ++boundary_edges;
            const Vec3 a = mesh.GetVertices()[edge.first.first];
            const Vec3 b = mesh.GetVertices()[edge.first.second];
            const auto same = [](Vec3 a, Vec3 b) {
                const Vec3 delta = a - b;
                return dot(delta, delta) < 1.0e-10f;
            };
            bool found = false;
            for (size_t i = 0; i < boundary.size(); ++i) {
                const Vec3 c = boundary[i], d = boundary[(i + 1) % boundary.size()];
                found = found || (same(a, c) && same(b, d)) || (same(a, d) && same(b, c));
            }
            require(found, "Cylinder front changed a source boundary edge.");
        }
        require(boundary_edges == boundary.size(), "Cylinder front lost boundary samples.");
    };
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
    require(rejection.empty() && quads == 7 && other_faces == 0,
            "Fusion cylinder front did not complete seven bounded quads.");
    validate_strip(result, contour);

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
    require(rejection.empty() && quads == 8 && result_triangles == 0,
            "Double-fillet front did not complete eight bounded quads.");
    validate_strip(result, double_fillet_contour);
    std::vector<std::unique_ptr<CPolyline>> saved_boundaries;
    require(!fill_surface.CreateLastQuadrangulationBoundaryPolylines(saved_boundaries)
                && saved_boundaries.empty(),
            "Successful cylinder fronts left rejected-input diagnostics.");

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

void TestWireCircularCaps(const char* path) {
    for (double radius : {0.01, 10.0, 1000.0}) {
        const gp_Ax2 frame(gp_Pnt(23, -17, 8), gp_Dir(1, 2, 3));
        const Handle(Geom_BSplineCurve) circle = GeomConvert::CurveToBSplineCurve(
            new Geom_Circle(gp_Circ(frame, radius)));
        require(IsCircularSplineBoundary(BRepBuilderAPI_MakeEdge(circle).Edge()),
                "A circular rational spline was not recognized.");
        require(!IsCircularSplineBoundary(BRepBuilderAPI_MakeEdge(circle,
                    circle->FirstParameter(),
                    (circle->FirstParameter() + circle->LastParameter()) * 0.5).Edge()),
                "An open circular arc was accepted as a full cap.");
        const Handle(Geom_BSplineCurve) ellipse = GeomConvert::CurveToBSplineCurve(
            new Geom_Ellipse(gp_Elips(frame, radius, radius * 0.8)));
        require(!IsCircularSplineBoundary(BRepBuilderAPI_MakeEdge(ellipse).Edge()),
                "A non-circular spline was accepted as a circle.");
        const gp_Pnt pole = circle->Pole(2);
        circle->SetPole(2, pole.Translated(gp_Vec(frame.Direction()) * radius * 0.1));
        require(!IsCircularSplineBoundary(BRepBuilderAPI_MakeEdge(circle).Edge()),
                "A non-planar spline was accepted as a circle.");
    }
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(QString::fromLocal8Bit(path), document, room, view, error),
            error.toLocal8Bit().constData());
    size_t tested = 0;
    for (const auto& object : document.GetObjects()) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid || solid->GetName() != "Wire") continue;
        solid->MeshQuadro = true;
        solid->MeshQuadroHoleSLX = false;
        for (float density : {0.30f, 0.50f, 0.80f}) {
            require(solid->ReBuldMesh(1.0f / density), "Wire remeshing failed.");
            std::vector<Vec3> side_vertices;
            for (int index = 0; index < solid->GetNumSurfaces(); ++index) {
                const auto* surface = solid->GetSurfaceFace(index);
                if (BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType() != GeomAbs_Plane) {
                    const auto& vertices = surface->pMesh3D->GetVertices();
                    side_vertices.insert(side_vertices.end(), vertices.begin(), vertices.end());
                }
            }
            size_t caps = 0;
            for (int index = 0; index < solid->GetNumSurfaces(); ++index) {
                const auto* surface = solid->GetSurfaceFace(index);
                if (BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType() != GeomAbs_Plane)
                    continue;
                ++caps;
                const auto& vertices = surface->pMesh3D->GetVertices();
                std::map<std::pair<size_t, size_t>, int> edges;
                size_t cells = 0;
                for (const auto& face : surface->pMesh3D->GetFaces()) {
                    if (face.deleted) continue;
                    ++cells;
                    require(face.corners.size() == 4, "Wire cap contains a non-quad cell.");
                    for (size_t i = 0; i < face.corners.size(); ++i)
                        ++edges[std::minmax(face.corners[i].v,
                            face.corners[(i + 1) % face.corners.size()].v)];
                }
                std::set<size_t> boundary;
                for (const auto& edge : edges) {
                    require(edge.second <= 2, "Wire cap has a non-manifold edge.");
                    if (edge.second == 1) {
                        boundary.insert(edge.first.first);
                        boundary.insert(edge.first.second);
                    }
                }
                require(boundary.size() >= 6 && cells > boundary.size()
                            && vertices.size() > boundary.size() * 2,
                        "Wire cap fell back to strips instead of concentric rings.");
                for (size_t id : boundary) {
                    double nearest = std::numeric_limits<double>::max();
                    for (Vec3 side : side_vertices) {
                        const Vec3 delta = vertices[id] - side;
                        nearest = std::min(nearest, static_cast<double>(dot(delta, delta)));
                    }
                    require(nearest < 1.0e-10, "Wire cap lost a side-wall boundary node.");
                }
                std::cout << "Wire density=" << density << " cap=" << index
                          << " boundary=" << boundary.size() << " quads=" << cells << '\n';
            }
            require(caps == 2, "Wire fixture must contain two planar caps.");
        }
        ++tested;
    }
    require(tested == 1, "Wire fixture was not found.");
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

    TopoDS_Shape full_sphere_shape = BRepPrimAPI_MakeSphere(10.0).Shape();
    CSolid full_sphere(full_sphere_shape);
    full_sphere.MeshQuadro = true;
    require(full_sphere.InitSurfaces() && full_sphere.InitEdges()
                && full_sphere.ReBuldMesh(1.0f / 0.45f),
            "Could not build the full-sphere cube Quadro mesh.");
    require(full_sphere.GetNumSurfaces() == 1,
            "The full-sphere regression shape has unexpected BRep faces.");
    const CSurfaceFace* full_surface = full_sphere.GetSurfaceFace(0);
    require(full_surface && full_surface->pMesh3D,
            "The full sphere has no cube Quadro mesh.");
    const auto& cube_vertices = full_surface->pMesh3D->GetVertices();
    const auto& cube_faces = full_surface->pMesh3D->GetFaces();
    require(cube_vertices.size() == 386 && cube_faces.size() == 384,
            "Density 0.45 did not produce six 8x8 sphere patches.");
    std::map<std::pair<size_t, size_t>, int> cube_edge_use;
    std::vector<std::set<size_t>> cube_neighbors(cube_vertices.size());
    size_t cube_quads = 0;
    double maximum_radius_error = 0.0;
    for (const CMesh3D::Face& face : cube_faces) {
        if (face.deleted) continue;
        cube_quads += face.corners.size() == 4;
        for (size_t corner = 0; corner < face.corners.size(); ++corner) {
            const size_t first = face.corners[corner].v;
            const size_t second = face.corners[
                (corner + 1) % face.corners.size()].v;
            require(first < cube_vertices.size()
                        && second < cube_vertices.size(),
                    "Cube sphere contains an invalid vertex index.");
            ++cube_edge_use[std::minmax(first, second)];
            cube_neighbors[first].insert(second);
            cube_neighbors[second].insert(first);
        }
    }
    for (const Vec3& vertex : cube_vertices) {
        maximum_radius_error = std::max(maximum_radius_error,
            std::abs(std::sqrt(static_cast<double>(dot(vertex, vertex)))
                     - 10.0));
    }
    const size_t extraordinary_vertices = static_cast<size_t>(std::count_if(
        cube_neighbors.begin(), cube_neighbors.end(),
        [](const std::set<size_t>& neighbors) {
            return neighbors.size() == 3;
        }));
    require(cube_quads == cube_faces.size()
                && std::all_of(cube_edge_use.begin(), cube_edge_use.end(),
                    [](const auto& edge) { return edge.second == 2; })
                && extraordinary_vertices == 8
                && std::all_of(cube_neighbors.begin(), cube_neighbors.end(),
                    [](const std::set<size_t>& neighbors) {
                        return neighbors.size() == 3 || neighbors.size() == 4;
                    })
                && maximum_radius_error < 1.0e-4,
            "Cube sphere lost its closed Catmull-Clark quad topology.");

    constexpr float trim_height = -2.3f;
    TopoDS_Shape union_box = BRepPrimAPI_MakeBox(
        gp_Pnt(-15.0, -15.0, -12.3), 30.0, 30.0, 10.0).Shape();
    TopoDS_Shape union_ball = BRepPrimAPI_MakeSphere(10.0).Shape();
    TopoDS_Shape union_shape = BRepAlgoAPI_Fuse(
        union_box, union_ball).Shape();
    CSolid trimmed_sphere(union_shape);
    trimmed_sphere.MeshQuadro = true;
    require(trimmed_sphere.InitSurfaces() && trimmed_sphere.InitEdges()
                && trimmed_sphere.ReBuldMesh(2.0f),
            "Could not cut a full cube sphere by the Boolean Union face.");
    bool found_trimmed_sphere = false;
    for (int surface_index = 0;
         surface_index < trimmed_sphere.GetNumSurfaces(); ++surface_index) {
        const CSurfaceFace* surface =
            trimmed_sphere.GetSurfaceFace(surface_index);
        if (!surface || !surface->pMesh3D
            || BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                != GeomAbs_Sphere) continue;
        found_trimmed_sphere = true;
        const auto& trimmed_vertices = surface->pMesh3D->GetVertices();
        const auto& trimmed_faces = surface->pMesh3D->GetFaces();
        const auto& trimmed_uvs = surface->pMesh3D->GetUVs();
        std::map<std::pair<size_t, size_t>, int> edge_use;
        std::set<size_t> used_vertices;
        for (const CMesh3D::Face& face : trimmed_faces) {
            if (face.deleted) continue;
            require(face.corners.size() >= 3,
                    "Trimmed cube sphere produced a collapsed polygon.");
            double previous_u = 0.0;
            bool have_previous_u = false;
            for (size_t corner = 0; corner < face.corners.size(); ++corner) {
                const MeshCorner& current = face.corners[corner];
                const MeshCorner& next = face.corners[
                    (corner + 1) % face.corners.size()];
                require(current.v < trimmed_vertices.size()
                            && current.uv < trimmed_uvs.size(),
                        "Trimmed cube sphere has an invalid corner index.");
                used_vertices.insert(current.v);
                ++edge_use[std::minmax(current.v, next.v)];
                if (have_previous_u) {
                    require(std::abs(trimmed_uvs[current.uv].u - previous_u)
                                <= M_PI + 1.0e-4,
                            "Sphere boundary UV jumped across the periodic seam.");
                }
                previous_u = trimmed_uvs[current.uv].u;
                have_previous_u = true;
            }
        }
        size_t open_edges = 0;
        for (const auto& edge : edge_use) {
            require(edge.second <= 2,
                    "Trimmed cube sphere produced a non-manifold edge.");
            if (edge.second != 1) continue;
            ++open_edges;
            const Vec3& first = trimmed_vertices[edge.first.first];
            const Vec3& second = trimmed_vertices[edge.first.second];
            if (!(std::abs(first.z - trim_height) < 1.0e-3f
                    && std::abs(second.z - trim_height) < 1.0e-3f)) {
                std::cerr << "Unexpected open cube-sphere edge: ("
                          << first.x << "," << first.y << "," << first.z
                          << ")-(" << second.x << "," << second.y << ","
                          << second.z << ") uses=" << edge.second << '\n';
            }
            require(std::abs(first.z - trim_height) < 1.0e-3f
                        && std::abs(second.z - trim_height) < 1.0e-3f,
                    "Cube-sphere cut left an open edge away from the BRep trim.");
        }
        for (size_t vertex_index : used_vertices) {
            const Vec3& vertex = trimmed_vertices[vertex_index];
            const double vertex_radius = std::sqrt(static_cast<double>(
                dot(vertex, vertex)));
            if (!(vertex.z >= trim_height - 1.0e-3f
                    && std::abs(vertex_radius - 10.0) < 1.0e-3)) {
                std::cerr << "Wrong retained sphere vertex: (" << vertex.x
                          << "," << vertex.y << "," << vertex.z
                          << ") radius=" << vertex_radius << '\n';
            }
            require(vertex.z >= trim_height - 1.0e-3f
                        && std::abs(vertex_radius - 10.0) < 1.0e-3,
                    "Cube-sphere classification retained the wrong side.");
        }
        require(open_edges > 0,
                "Trimmed cube sphere has no Boolean boundary.");
    }
    require(found_trimmed_sphere,
            "Boolean Union regression has no spherical face.");

    // CapRetopo::DoCap2 regression: an exact circular planar face must use
    // concentric quad rings and the three-boundary-nodes-plus-centre closure,
    // rather than the generic strip-like advancing-front result.
    TopoDS_Shape circular_cap_shape =
        BRepPrimAPI_MakeCylinder(9.0, 4.0).Shape();
    CSolid circular_cap_solid(circular_cap_shape);
    circular_cap_solid.MeshQuadro = true;
    require(circular_cap_solid.InitSurfaces()
                && circular_cap_solid.InitEdges()
                && circular_cap_solid.ReBuldMesh(1.0f / 0.45f),
            "Could not build the circular CapRetopo regression solid.");
    size_t circular_cap_count = 0;
    for (int surface_index = 0;
         surface_index < circular_cap_solid.GetNumSurfaces(); ++surface_index) {
        const CSurfaceFace* surface =
            circular_cap_solid.GetSurfaceFace(surface_index);
        if (!surface || !surface->pMesh3D
            || BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                != GeomAbs_Plane) {
            continue;
        }
        size_t wire_count = 0;
        size_t circle_count = 0;
        for (TopExp_Explorer wire(surface->m_Face, TopAbs_WIRE);
             wire.More(); wire.Next()) ++wire_count;
        for (TopExp_Explorer edge(surface->m_Face, TopAbs_EDGE);
             edge.More(); edge.Next()) {
            if (BRepAdaptor_Curve(TopoDS::Edge(edge.Current())).GetType()
                == GeomAbs_Circle) ++circle_count;
        }
        if (wire_count != 1 || circle_count != 1)
            continue;

        const CMesh3D& cap_mesh = *surface->pMesh3D;
        const auto& cap_vertices = cap_mesh.GetVertices();
        size_t active_faces = 0;
        std::map<std::pair<size_t, size_t>, int> cap_edge_use;
        std::vector<std::set<size_t>> cap_neighbors(cap_vertices.size());
        for (const CMesh3D::Face& face : cap_mesh.GetFaces()) {
            if (face.deleted) continue;
            ++active_faces;
            require(face.corners.size() == 4,
                    "Circular CapRetopo produced a non-quad face.");
            for (size_t corner = 0; corner < face.corners.size(); ++corner) {
                const size_t first = face.corners[corner].v;
                const size_t second = face.corners[
                    (corner + 1) % face.corners.size()].v;
                require(first < cap_vertices.size() && second < cap_vertices.size(),
                        "Circular CapRetopo contains an invalid vertex index.");
                ++cap_edge_use[std::minmax(first, second)];
                cap_neighbors[first].insert(second);
                cap_neighbors[second].insert(first);
            }
        }
        bool manifold = false;
        require(active_faces > 0
                    && ClosedMeshBoundaryLoopCount(cap_mesh, manifold) == 1
                    && manifold,
                "Circular CapRetopo is disconnected or non-manifold.");
        const size_t boundary_edge_count = static_cast<size_t>(std::count_if(
            cap_edge_use.begin(), cap_edge_use.end(),
            [](const auto& edge) { return edge.second == 1; }));
        const auto centre = std::max_element(cap_neighbors.begin(),
            cap_neighbors.end(), [](const auto& first, const auto& second) {
                return first.size() < second.size();
            });
        require(boundary_edge_count >= 6
                    && (boundary_edge_count & 1U) == 0
                    && centre != cap_neighbors.end()
                    && centre->size() == boundary_edge_count / 2,
                "Circular CapRetopo lost its paired central quad closure.");
        ++circular_cap_count;
    }
    require(circular_cap_count == 2,
            "The cylinder did not expose both circular CapRetopo faces.");

    // An oblique cylinder section is an ellipse on its planar Cap. It must use
    // the same concentric-ring topology instead of falling back to long strips.
    const gp_Elips ellipse(gp_Ax2(gp_Pnt(0.0, 0.0, 0.0),
        gp_Dir(0.0, 0.0, 1.0)), 9.0, 6.0);
    const TopoDS_Edge ellipse_edge = BRepBuilderAPI_MakeEdge(ellipse).Edge();
    const TopoDS_Wire ellipse_wire = BRepBuilderAPI_MakeWire(ellipse_edge).Wire();
    TopoDS_Shape ellipse_face = BRepBuilderAPI_MakeFace(ellipse_wire).Face();
    CSolid elliptical_cap_solid(ellipse_face);
    elliptical_cap_solid.MeshQuadro = true;
    require(elliptical_cap_solid.InitSurfaces()
                && elliptical_cap_solid.InitEdges()
                && elliptical_cap_solid.ReBuldMesh(1.0f / 0.65f),
            "Could not build the elliptical CapRetopo regression face.");
    require(elliptical_cap_solid.GetNumSurfaces() == 1,
            "Elliptical CapRetopo regression has unexpected surfaces.");
    const CSurfaceFace* elliptical_cap =
        elliptical_cap_solid.GetSurfaceFace(0);
    require(elliptical_cap && elliptical_cap->pMesh3D,
            "Elliptical CapRetopo produced no mesh.");
    size_t elliptical_quads = 0;
    for (const CMesh3D::Face& face : elliptical_cap->pMesh3D->GetFaces()) {
        if (face.deleted) continue;
        require(face.corners.size() == 4,
                "Elliptical CapRetopo produced a non-quad face.");
        ++elliptical_quads;
    }
    bool elliptical_manifold = false;
    require(elliptical_quads > 20
                && ClosedMeshBoundaryLoopCount(
                    *elliptical_cap->pMesh3D, elliptical_manifold) == 1
                && elliptical_manifold,
            "Elliptical CapRetopo is disconnected or non-manifold.");

    // The planar Cap must not resample a Boolean circle independently. Its
    // outer ring is the exact open boundary created by clipping the cube-sphere.
    TopoDS_Shape sphere_for_cap = BRepPrimAPI_MakeSphere(10.0).Shape();
    TopoDS_Shape cap_cutter = BRepPrimAPI_MakeBox(
        gp_Pnt(2.0, -20.0, -20.0), 20.0, 40.0, 40.0).Shape();
    TopoDS_Shape capped_shape = BRepAlgoAPI_Cut(
        sphere_for_cap, cap_cutter).Shape();
    CSolid capped_sphere(capped_shape);
    capped_sphere.MeshQuadro = true;
    require(capped_sphere.InitSurfaces() && capped_sphere.InitEdges()
                && capped_sphere.ReBuldMesh(1.0f / 0.45f),
            "Could not build the sphere-to-Cap boundary regression solid.");
    const CMesh3D* spherical_mesh = nullptr;
    const CMesh3D* planar_cap_mesh = nullptr;
    for (int surface_index = 0;
         surface_index < capped_sphere.GetNumSurfaces(); ++surface_index) {
        const CSurfaceFace* surface = capped_sphere.GetSurfaceFace(surface_index);
        if (!surface || !surface->pMesh3D)
            continue;
        const GeomAbs_SurfaceType type = BRepAdaptor_Surface(
            TopoDS::Face(surface->m_Face)).GetType();
        if (type == GeomAbs_Sphere)
            spherical_mesh = surface->pMesh3D;
        if (type == GeomAbs_Plane)
            planar_cap_mesh = surface->pMesh3D;
    }
    require(spherical_mesh && planar_cap_mesh,
            "The boundary regression did not expose both sphere and Cap meshes.");
    const auto open_boundary_vertices = [](const CMesh3D& mesh) {
        std::map<std::pair<size_t, size_t>, int> edge_use;
        for (const CMesh3D::Face& face : mesh.GetFaces()) {
            if (face.deleted || face.corners.size() < 3) continue;
            for (size_t corner = 0; corner < face.corners.size(); ++corner) {
                const size_t first = face.corners[corner].v;
                const size_t second = face.corners[
                    (corner + 1) % face.corners.size()].v;
                ++edge_use[std::minmax(first, second)];
            }
        }
        std::set<size_t> boundary_indices;
        for (const auto& edge : edge_use) {
            if (edge.second == 1) {
                boundary_indices.insert(edge.first.first);
                boundary_indices.insert(edge.first.second);
            }
        }
        std::vector<Vec3> result;
        for (size_t index : boundary_indices)
            result.push_back(mesh.GetVertices()[index]);
        return result;
    };
    const std::vector<Vec3> sphere_boundary =
        open_boundary_vertices(*spherical_mesh);
    const std::vector<Vec3> cap_boundary =
        open_boundary_vertices(*planar_cap_mesh);
    require(sphere_boundary.size() >= 6
                && sphere_boundary.size() == cap_boundary.size(),
            "Sphere and Cap use different boundary node counts.");
    for (Vec3 sphere_point : sphere_boundary) {
        double nearest = std::numeric_limits<double>::max();
        for (Vec3 cap_point : cap_boundary) {
            const Vec3 delta = sphere_point - cap_point;
            nearest = std::min(nearest, static_cast<double>(dot(delta, delta)));
        }
        require(nearest <= 1.0e-8,
                "The Cap did not reuse an exact sphere boundary vertex.");
    }
}

void TestCylinderMultipleWindows(const QString& path, bool box_windows = false) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error),
            error.toLocal8Bit().constData());
    CSolid* solid = nullptr;
    for (const auto& object : document.GetObjects()) {
        if (auto* candidate = dynamic_cast<CSolid*>(object.get())) {
            require(!solid, "Cylinder fixture must contain one solid body.");
            solid = candidate;
        }
    }
    require(solid && solid->GetNumSurfaces() == (box_windows ? 9 : 5),
            "Two-window cylinder fixture topology changed.");
    solid->MeshQuadro = true;
    for (bool slx : {false, true}) {
        solid->MeshQuadroHoleSLX = slx;
        const std::vector<float> densities = box_windows
            ? std::vector<float>{0.35f, 0.40f, 0.45f, 0.50f, 0.55f, 0.60f,
                                 0.65f, 0.70f, 0.75f, 0.80f, 0.90f, 1.0f, 0.40f}
            : std::vector<float>{0.35f, 0.50f, 0.65f, 0.80f, 1.0f, 0.50f};
        for (float density : densities) {
            require(solid->ReBuldMesh(1.0f / density), "Cylinder windows rebuild failed.");
            std::vector<const CMesh3D*> meshes;
            for (int index = 0; index < solid->GetNumSurfaces(); ++index) {
                const auto* surface = solid->GetSurfaceFace(index);
                require(surface && surface->pMesh3D, "Cylinder lost a surface mesh.");
                const auto& mesh = *surface->pMesh3D;
                SurfaceUVMapping mapping(surface);
                std::vector<double> edge_lengths;
                require(ActiveFaceEdgeComponentCount(mesh) == 1,
                        "Cylinder surface is empty or disconnected.");
                for (const auto& cell : mesh.GetFaces()) {
                    if (cell.deleted) continue;
                    require(cell.corners.size() == 3 || cell.corners.size() == 4,
                            "Cylinder trimming left an unsupported polygon.");
                    Vec3 area{};
                    Vec3 center{};
                    for (size_t i = 0; i < cell.corners.size(); ++i) {
                        const size_t a = cell.corners[i].v;
                        const size_t b = cell.corners[(i + 1) % cell.corners.size()].v;
                        require(a < mesh.GetVertices().size() && b < mesh.GetVertices().size(),
                                "Cylinder mesh contains an invalid index.");
                        const Vec3 first = mesh.GetVertices()[a];
                        const Vec3 second = mesh.GetVertices()[b];
                        require(std::isfinite(first.x) && std::isfinite(first.y)
                                    && std::isfinite(first.z),
                                "Cylinder mesh contains a nonfinite vertex.");
                        area = area + cross(first, second);
                        center = center + first;
                        const Vec3 edge = second - first;
                        edge_lengths.push_back(std::sqrt(double(dot(edge, edge))));
                    }
                    require(dot(area, area) > 1.0e-10f, "Cylinder mesh contains a collapsed cell.");
                    if (box_windows) {
                        center = center * (1.0f / cell.corners.size());
                        SurfaceUVPoint uv;
                        require(mapping.Project(center, uv), "Cannot project cell onto CAD face.");
                        BRepClass_FaceClassifier classifier(TopoDS::Face(surface->m_Face),
                            gp_Pnt2d(uv.u, uv.v), 1.0e-7, Standard_False);
                        require(classifier.State() != TopAbs_OUT,
                                "Cylinder cell crosses onto the discarded side of a window.");
                    }
                }
                if (index == 0) {
                    bool manifold = false;
                    require(surface->GetPreparedPolylineCount() == (box_windows ? 20 : 12),
                            "Outer cylinder must retain all CAD boundary edges.");
                    require(ClosedMeshBoundaryLoopCount(mesh, manifold) == 4 && manifold,
                            "Outer cylinder must have two end rings and both window boundaries.");
                    if (box_windows) {
                        std::sort(edge_lengths.begin(), edge_lengths.end());
                        require(edge_lengths.back() < 3.0 * edge_lengths[edge_lengths.size() / 2],
                                "Cylinder trimming stretched a cell across several mesh rows.");
                    }
                }
                meshes.push_back(&mesh);
            }
            auto welded = CMesh3D::CreateWelded(meshes);
            bool manifold = false;
            require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                    "Cylinder windows retained an open or nonmanifold seam.");
            std::cout << "Cylinder two windows density=" << density << " slx=" << slx
                      << " weldedVertices=" << welded->GetVertices().size() << '\n';
        }
    }
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

    const auto require_clean_cylinders = [](TopoDS_Shape shape,
                                             const char* fixture_name) {
        CSolid fixture(shape);
        fixture.MeshQuadro = true;
        require(fixture.InitSurfaces() && fixture.InitEdges()
                    && fixture.ReBuldMesh(1.0f / 0.50f),
                fixture_name);
        size_t cylinder_count = 0;
        for (int surface_index = 0;
             surface_index < fixture.GetNumSurfaces(); ++surface_index) {
            const CSurfaceFace* surface = fixture.GetSurfaceFace(surface_index);
            if (!surface || !surface->pMesh3D
                || BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                    != GeomAbs_Cylinder) {
                continue;
            }
            ++cylinder_count;
            size_t active_faces = 0;
            double maximum_edge = 0.0;
            std::vector<double> lengths;
            const auto& vertices = surface->pMesh3D->GetVertices();
            for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces()) {
                if (face.deleted) continue;
                require(face.corners.size() >= 3,
                        "Cylinder end-cut produced a degenerate face.");
                ++active_faces;
                for (size_t corner = 0; corner < face.corners.size(); ++corner) {
                    const size_t first = face.corners[corner].v;
                    const size_t second = face.corners[
                        (corner + 1) % face.corners.size()].v;
                    require(first < vertices.size() && second < vertices.size(),
                            "Cylinder end-cut produced an invalid mesh index.");
                    const Vec3 edge = vertices[second] - vertices[first];
                    const double length = std::sqrt(
                        static_cast<double>(dot(edge, edge)));
                    if (length > 1.0e-7) {
                        lengths.push_back(length);
                        maximum_edge = std::max(maximum_edge, length);
                    }
                }
            }
            require(active_faces > 0,
                    "Cylinder end-cut produced no active mesh faces.");
            Bnd_Box face_bounds;
            BRepBndLib::Add(surface->m_Face, face_bounds);
            double x_min = 0.0;
            double y_min = 0.0;
            double z_min = 0.0;
            double x_max = 0.0;
            double y_max = 0.0;
            double z_max = 0.0;
            face_bounds.Get(x_min, y_min, z_min, x_max, y_max, z_max);
            const double face_diagonal = std::hypot(
                std::hypot(x_max - x_min, y_max - y_min), z_max - z_min);
            require(!lengths.empty() && maximum_edge < face_diagonal * 0.75,
                    "Cylinder end-cut retained a periodic long-edge fan.");
        }
        require(cylinder_count > 0,
                "Cylinder end-cut fixture contained no cylindrical mesh.");
    };

    const TopoDS_Shape end_notch = BRepAlgoAPI_Cut(
        cylinder,
        BRepPrimAPI_MakeBox(gp_Pnt(cut_x, -6.0, -1.0),
                            gp_Pnt(6.0, 6.0, 4.25)).Shape()).Shape();
    require_clean_cylinders(end_notch,
        "Could not build the cylinder end-notch regression solid.");

    const TopoDS_Shape radial_cutter = BRepPrimAPI_MakeCylinder(
        gp_Ax2(gp_Pnt(0.0, 0.0, height * 0.5), gp_Dir(1.0, 0.0, 0.0)),
        2.25, radius + 2.0).Shape();
    require_clean_cylinders(BRepAlgoAPI_Cut(cylinder, radial_cutter).Shape(),
        "Could not build the cylinder-minus-cylinder regression solid.");
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
		const char* extrusion_parameter, double extrusion, double overlap = 0.001) {
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
		parameters.push_back({"boolean.overlap", "Boolean Overlap", overlap, 0.0, 1000000.0, 0.01});
		CAlfaDoc document;
		ActiveParametricObject object = registry.CreateParametricObject(
			tool_id, document, parameters);
		CSolid* solid = object.object_index < document.GetObjects().size()
			? dynamic_cast<CSolid*>(
				document.GetObjects()[object.object_index].get())
			: nullptr;
		require(solid != nullptr,
			"Face-overlap primitive was not created.");
		Vec3 minimum{};
		Vec3 maximum{};
		require(solid->GetBounds(minimum, maximum),
			"Face-overlap primitive has no bounds.");
		constexpr double delta = 0.001;
		constexpr double tolerance = 2.0e-4;
		const double expected_minimum = extrusion > 0.0
			? -delta : extrusion;
		const double expected_maximum = extrusion > 0.0
			? extrusion : delta;
		require(std::fabs(minimum.z - expected_minimum) <= tolerance
				&& std::fabs(maximum.z - expected_maximum) <= tolerance,
			"Face-based primitive must extend the start and preserve the requested far end.");
        if (extrusion > 0) {
            const auto host = BRepPrimAPI_MakeBox(gp_Pnt(-1000,-1000,-1000),2000,2000,1000).Shape();
            BRepAlgoAPI_Common common(host, solid->m_Shape);
            GProp_GProps volume;
            require(common.IsDone(), "Cannot check primitive penetration");
            BRepGProp::VolumeProperties(common.Shape(), volume);
            require(volume.Mass() > 1e-6, "Union tool only touches the host; positive penetration required");
            BRepAlgoAPI_Fuse fused(host, solid->m_Shape);
            unsigned count=0;
            require(fused.IsDone(), "Penetrating primitive union failed");
            for(TopExp_Explorer e(fused.Shape(),TopAbs_SOLID);e.More();e.Next())++count;
            require(count==1, "Union must produce one solid");
        }
        auto reopened=registry.ActiveObjectFromDocument(object.object_index,*solid,0,&document);
        const auto margin=std::find_if(reopened.parameters.begin(),reopened.parameters.end(),[](const auto& p){return p.id=="boolean.overlap";});
        require(margin!=reopened.parameters.end() && margin->value==overlap,"History lost face overlap");
        registry.Rebuild(reopened,document);
        solid=dynamic_cast<CSolid*>(document.GetObjects()[object.object_index].get());
        require(solid && solid->GetBounds(minimum,maximum)
            && std::fabs(minimum.z-expected_minimum)<=tolerance
            && std::fabs(maximum.z-expected_maximum)<=tolerance,"Rebuild lost penetration or moved far end");
	};

	verify("SolidBox", "depth", 5.0);
	verify("SolidBox", "depth", -5.0);
	verify("SolidCylinder", "height", 5.0);
	verify("SolidCylinder", "height", -5.0);
	verify("SolidBox", "depth", 5.0, 0.588);
	verify("SolidCylinder", "height", 5.0, 0.588);
	verify("SolidBox", "depth", -5.0, 0.588);
	verify("SolidCylinder", "height", -5.0, 0.588);

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
			parameter.value = 10.0 - 1.90734863e-6;
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

	BRepClass3d_SolidClassifier roof(result->m_Shape, gp_Pnt(2.5, 2.5, 10.0 - 0.5e-6), 1e-8);
	require(roof.State() == TopAbs_OUT, "Face cutter left a thin roof at the rounded placement plane");

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

void TestMeshVar11Regression() {
    for (bool adjacent : {false, true}) {
        for (bool reverse_face : {false, true}) {
            for (bool reverse_line : {false, true}) {
                for (int shift : {0, 2}) {
                    const std::vector<Vec3> vertices{{0,0,0},{4,0,0},{4,4,0},{0,4,0}};
                    CMesh3D::Face source{0,1,2,3};
                    if (reverse_face) std::reverse(source.corners.begin(), source.corners.end());
                    std::vector<CPoint3d> points = adjacent
                        ? std::vector<CPoint3d>{{0,0,0},{1,1,0},{3,1,0},{4,0,0},{6,-3,0},{-2,-3,0}}
                        : std::vector<CPoint3d>{{0,0,0},{1,1.5,0},{2.5,2,0},{4,4,0},{6,6,0},{6,-2,0},{-2,-2,0}};
                    if (reverse_line) std::reverse(points.begin(), points.end());
                    std::rotate(points.begin(), points.begin() + shift, points.end());
                    CPolyline line;
                    std::vector<int> interior;
                    for (size_t i = 0; i < points.size(); ++i) {
                        line.AddPoint(points[i]);
                        if (points[i].x > 0 && points[i].x < 4 && points[i].y > 0 && points[i].y < 4)
                            interior.push_back(static_cast<int>(i));
                    }
                    line.SetClosed(true);
                    if (!interior.empty() && interior.front() == 0)
                        interior.push_back(static_cast<int>(points.size())); // Classifier's repeated seam node.
                    std::reverse(interior.begin(), interior.end());
                    int first = -1, last = -1;
                    for (int i = 0; i < 4; ++i) {
                        if (source.corners[i].v == 0) first = i;
                        if (source.corners[i].v == (adjacent ? 1 : 2)) last = i;
                    }
                    CMesh3D mesh;
                    require(mesh.SetGeometry(vertices, {source}), "Cannot build Var-11 fixture.");
                    require(mesh.SplitFaceByVar11(0, first, last, interior, &line),
                            "Var-11 rejected an adjacent/opposite or cyclic interior chain.");
                    require(mesh.GetFaces().size() == 2 && mesh.GetVertices().size() == 6,
                            "Var-11 lost or duplicated an interior node.");
                    double total_area = 0;
                    std::map<std::pair<size_t,size_t>, int> edges;
                    for (const auto& face : mesh.GetFaces()) {
                        double area = 0;
                        for (size_t j = 0; j < face.corners.size(); ++j) {
                            const size_t a = face.corners[j].v, b = face.corners[(j+1)%face.corners.size()].v;
                            const Vec3 p = mesh.GetVertices()[a], q = mesh.GetVertices()[b];
                            area += p.x*q.y - p.y*q.x;
                            ++edges[std::minmax(a,b)];
                        }
                        require(reverse_face ? area < 0 : area > 0, "Var-11 reversed a polygon.");
                        total_area += std::fabs(area) * 0.5;
                    }
                    require(std::fabs(total_area - 16.0) < 1.e-6, "Var-11 changed the source area.");
                    size_t boundary_edges = 0, internal_edges = 0;
                    for (const auto& [edge, uses] : edges) {
                        require(uses == 1 || uses == 2, "Var-11 created a non-manifold edge.");
                        if (uses == 1) {
                            ++boundary_edges;
                            require(edge.first < 4 && edge.second < 4, "Var-11 tore the interior chain.");
                        } else ++internal_edges;
                    }
                    require(boundary_edges == 4 && internal_edges == 3,
                            "Var-11 changed the perimeter or lost a contour segment.");
                }
            }
        }
    }
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

void TestLoftSplineOrdering() {
    CAlfaDoc document;
    const auto add_profile = [&](const char* name, CPoint3d first,
                                 CPoint3d second) {
        auto spline = std::make_unique<CBSpline>(name);
        spline->SetDegree(1);
        spline->AddPoint(first);
        spline->AddPoint(second);
        document.AddObject(std::move(spline));
        return document.GetObjects().back()->m_id;
    };
    const unsigned long left_id = add_profile("Loft left",
        {-114.0, 373.0, 0.0}, {-127.0, -232.0, 0.0});
    const unsigned long right_id = add_profile("Loft right",
        {506.0, 357.0, 0.0}, {532.0, -204.0, 0.0});
    const unsigned long middle_id = add_profile("Loft middle",
        {180.0, 386.0, 115.0}, {197.0, -200.0, 115.0});

    require(document.SelectObjectById(left_id, SelectionAction::Replace)
                && document.SelectObjectById(right_id, SelectionAction::Add)
                && document.SelectObjectById(middle_id, SelectionAction::Add),
            "Could not prepare the deliberately unsorted Loft selection.");
    require(document.CreateLoftSurfaceFromSelectedBSplines(),
            "Geometrically sorted Loft construction failed.");
    const CAlfaObject* loft = document.GetObjects().back().get();
    require(loft && loft->GetParametricToolId() == "SurfaceLoft",
            "Loft ordering regression produced no parametric surface.");
    unsigned long stored_middle_id = 0;
    for (const ParametricParameterValue& parameter :
         loft->GetParametricParameters()) {
        if (parameter.id == "curve2.id") {
            stored_middle_id = static_cast<unsigned long>(
                std::llround(parameter.value));
        }
    }
    require(stored_middle_id == middle_id,
            "Loft preserved selection order instead of geometric profile order.");

    const auto* loft_surface = dynamic_cast<const CSurfaceSet*>(loft);
    const CSurfaceFace* loft_face = loft_surface
        ? loft_surface->GetSurfaceFace(0) : nullptr;
    require(loft_face && loft_face->pMesh3D
                && loft_face->pMesh3D->GetFaces().size() >= 100,
            "Loft started with the coarse bounding-box display mesh.");
}

void TestTangentCapFromClosedSpline() {
    TopoDS_Face cylindrical_face;
    const TopoDS_Shape cylinder =
        BRepPrimAPI_MakeCylinder(20.0, 60.0).Shape();
    for (TopExp_Explorer faces(cylinder, TopAbs_FACE);
         faces.More(); faces.Next()) {
        const TopoDS_Face candidate = TopoDS::Face(faces.Current());
        if (BRepAdaptor_Surface(candidate).GetType() == GeomAbs_Cylinder) {
            cylindrical_face = candidate;
            break;
        }
    }
    require(!cylindrical_face.IsNull(),
            "Tangent Cap fixture has no cylindrical support face.");

    CAlfaDoc document;
    TopoDS_Shape support_shape = cylindrical_face;
    auto support = std::make_unique<CSurfaceSet>(support_shape);
    support->SetName("Open fuselage surface");
    require(support->ReBuldMesh(2.0f),
            "Tangent Cap support surface could not be initialized.");
    document.AddObject(std::move(support));
    const unsigned long surface_id = document.GetSelectedObject()->m_id;

    auto boundary = std::make_unique<CBSpline>("Nose boundary");
    constexpr int point_count = 24;
    for (int point = 0; point < point_count; ++point) {
        const double angle = 2.0 * 3.14159265358979323846
            * static_cast<double>(point) / point_count;
        boundary->AddPoint({20.0 * std::cos(angle),
                            20.0 * std::sin(angle), 0.0});
    }
    boundary->SetClosed(true);
    document.AddObject(std::move(boundary));
    const unsigned long curve_id = document.GetSelectedObject()->m_id;
    require(document.SelectObjectById(surface_id, SelectionAction::Replace)
                && document.SelectObjectById(curve_id, SelectionAction::Add),
            "Tangent Cap fixture selection failed.");
    std::string error;
    const bool cap_created = document.CreateTangentCapFromSelection(&error);
    require(cap_created,
            error.empty() ? "Tangent Cap construction failed." : error.c_str());
    const auto* cap = dynamic_cast<const CSurfaceSet*>(
        document.GetObjects().back().get());
    require(cap && cap->GetName() == "Tangent Cap"
                && cap->GetParametricToolId() == "SurfaceTangentCap"
                && cap->GetNumSurfaces() == 1
                && BRepCheck_Analyzer(cap->m_Shape).IsValid(),
            "Tangent Cap did not create a valid parametric G1 patch.");

    const auto minimum_z = [](const TopoDS_Shape& shape) {
        Bnd_Box bounds;
        BRepBndLib::Add(shape, bounds);
        Standard_Real xmin = 0.0, ymin = 0.0, zmin = 0.0;
        Standard_Real xmax = 0.0, ymax = 0.0, zmax = 0.0;
        bounds.Get(xmin, ymin, zmin, xmax, ymax, zmax);
        return zmin;
    };
    const double short_nose_minimum = minimum_z(cap->m_Shape);
    const size_t cap_index = document.GetObjects().size() - 1;
    require(document.RebuildTangentCap(
                cap_index, curve_id, surface_id, 1.10, &error),
            error.empty() ? "Tangent Cap factor rebuild failed."
                          : error.c_str());
    cap = dynamic_cast<const CSurfaceSet*>(document.GetObjects().back().get());
    require(cap && minimum_z(cap->m_Shape) < short_nose_minimum - 5.0,
            "Nose Length Factor did not lengthen the tangent cap.");
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

void TestExtrudeSlQuadro(const QString& path) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error),
            error.toLocal8Bit().constData());
    size_t solids = 0;
    for (const auto& object : document.GetObjects()) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid) continue;
        ++solids;
        solid->MeshQuadro = true;
        solid->MeshQuadroHoleSLX = false;
        // Exercise the previously capped 18-face body above Density 0.50,
        // then return to low density to catch stale boundary caches.
        const std::vector<float> densities = solid->GetNumSurfaces() == 18
            ? std::vector<float>{0.20f, 0.25f, 0.30f, 0.35f, 0.50f, 0.65f, 0.80f, 1.0f, 0.25f}
            : std::vector<float>{0.20f, 0.25f, 0.30f, 0.35f, 0.50f, 0.25f};
        size_t previous_boundary_points = 0;
        size_t previous_vertices = 0;
        for (float density : densities) {
            const auto begin = std::chrono::steady_clock::now();
            require(solid->ReBuldMesh(1.0f / density),
                    "Extrude_SL Quadro rebuild failed.");
            const double seconds = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - begin).count();
            std::vector<const CMesh3D*> meshes;
            size_t boundary_points = 0;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                const CSurfaceFace* surface = solid->GetSurfaceFace(i);
                require(surface && surface->pMesh3D,
                        "Extrude_SL lost a surface mesh.");
                require(surface->GetLastQuadrangulationDiagnostic().empty(),
                        "Extrude_SL fell back from the advancing-front mesh.");
                const CMesh3D& mesh = *surface->pMesh3D;
                for (int edge = 0; edge < surface->GetPreparedPolylineCount(); ++edge)
                    boundary_points += surface->GetPreparedPolylinePointCount(edge);
                require(ActiveFaceEdgeComponentCount(mesh) == 1,
                        "Extrude_SL surface has disconnected mesh fragments.");
                for (const CMesh3D::Face& face : mesh.GetFaces()) {
                    if (face.deleted) continue;
                    require(face.corners.size() == 4,
                            "Extrude_SL retained a triangle.");
                    for (const MeshCorner& corner : face.corners) {
                        require(corner.v < mesh.GetVertices().size(),
                                "Extrude_SL has an invalid vertex index.");
                        const Vec3 p = mesh.GetVertices()[corner.v];
                        require(std::isfinite(p.x) && std::isfinite(p.y)
                                    && std::isfinite(p.z),
                                "Extrude_SL has a non-finite vertex.");
                    }
                }
                meshes.push_back(&mesh);
            }
            auto welded = CMesh3D::CreateWelded(meshes);
            require(static_cast<bool>(welded), "Extrude_SL could not be welded.");
            bool manifold = false;
            require(ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                    "Extrude_SL retained open or non-manifold mesh seams.");
            const size_t vertex_count = welded->GetVertices().size();
            if (density > 0.50f) {
                require(boundary_points > previous_boundary_points,
                        "Extrude_SL boundary density is still capped.");
                require(vertex_count > previous_vertices,
                        "Extrude_SL mesh does not get denser above 0.50.");
            }
            previous_boundary_points = boundary_points;
            previous_vertices = vertex_count;
            std::cout << "Extrude_SL surfaces=" << solid->GetNumSurfaces()
                      << " density=" << density << " rebuildSeconds=" << seconds
                      << " boundaryPoints=" << boundary_points
                      << " weldedVertices=" << vertex_count << '\n';
        }
    }
    require(solids == 2, "Extrude_SL regression must inspect both source solids.");
}

void TestBoxMinBoxFilledQuadro(const QString& path) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error),
            error.toLocal8Bit().constData());
    size_t tested = 0;
    for (const auto& object : document.GetObjects()) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid) continue;
        ++tested;
        require(solid->GetNumSurfaces() == 14, "Box fixture topology changed.");
        solid->MeshQuadro = true;
        solid->MeshQuadroHoleSLX = false;
        for (float density : {0.20f, 0.25f, 0.35f, 0.50f, 0.65f, 0.80f, 1.0f, 0.50f}) {
            const auto begin = std::chrono::steady_clock::now();
            require(solid->ReBuldMesh(1.0f / density), "Box Quadro rebuild failed.");
            std::vector<const CMesh3D*> meshes;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                CSurfaceFace* surface = solid->GetSurfaceFace(i);
                require(surface && surface->pMesh3D, "Box lost a surface mesh.");
                require(surface->GetLastQuadrangulationDiagnostic().empty()
                            && surface->GetLastIslandFillError().empty(),
                        "Box used an emergency mesh fallback.");
                const CMesh3D& mesh = *surface->pMesh3D;
                require(ActiveFaceEdgeComponentCount(mesh) == 1,
                        "Box surface contains disconnected patches.");
                for (const auto& face : mesh.GetFaces()) {
                    if (face.deleted) continue;
                    require(face.corners.size() == 4, "Box retained a non-quad cell.");
                    std::set<size_t> indices;
                    Vec3 area{};
                    for (size_t j = 0; j < 4; ++j) {
                        const size_t a = face.corners[j].v;
                        const size_t b = face.corners[(j + 1) % 4].v;
                        require(a < mesh.GetVertices().size() && b < mesh.GetVertices().size(),
                                "Box has an invalid vertex index.");
                        indices.insert(a);
                        const Vec3 p = mesh.GetVertices()[a];
                        require(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z),
                                "Box has a non-finite vertex.");
                        const Vec3 origin = mesh.GetVertices()[face.corners[0].v];
                        area = area + cross(p - origin, mesh.GetVertices()[b] - origin);
                    }
                    require(indices.size() == 4 && dot(area, area) > 1.0e-14f,
                            "Box has a collapsed quad.");
                }
                if (i == 2 || i == 4) {
                    bool manifold = false;
                    require(ClosedMeshBoundaryLoopCount(mesh, manifold) == 2 && manifold,
                            "Box planar face lost its hole or has an open patch seam.");
                    std::vector<std::unique_ptr<CPolyline>> patches;
                    require(surface->CreateLastIslandBoundaryPolylines(patches) && patches.size() == 4,
                            "Box hole was skipped while creating island contours.");
                }
                if (i == 12) {
                    require(surface->m_TypeMesh != REGULAR_MESH
                                && surface->GetPreparedPolylineCount() == 6,
                            "Split cylinder edges must not donate complete net rows.");
                    std::vector<CPoint3d> fragment;
                    require(surface->GetPreparedPolylinePoints(1, fragment) && fragment.size() >= 2,
                            "Box lost the short CAD arc.");
                    double length = 0;
                    for (size_t j = 1; j < fragment.size(); ++j)
                        length += fragment[j].DistTo(&fragment[j - 1]);
                    require(length < 0.50 && length > 0.45,
                            "Short CAD arc was replaced by the whole quarter-circle.");
                }
                meshes.push_back(&mesh);
            }
            auto welded = CMesh3D::CreateWelded(meshes);
            bool manifold = false;
            require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                    "Box retained open or non-manifold boundary seams.");
            std::cout << "Box_Min_Box_Filled density=" << density
                      << " vertices=" << welded->GetVertices().size()
                      << " seconds=" << std::chrono::duration<double>(
                          std::chrono::steady_clock::now() - begin).count() << '\n';
        }
    }
    require(tested == 1, "Box regression must inspect one source solid.");
}

void TestBoxDoubleCutSlx(const QString& path) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error),
            error.toLocal8Bit().constData());
    size_t tested = 0;
    for (const auto& object : document.GetObjects()) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid) continue;
        ++tested;
        require(solid->GetNumSurfaces() == 14, "Double-cut Box topology changed.");
        solid->MeshQuadro = true;
        const std::vector<std::pair<float, bool>> cases{
            {0.20f, true}, {0.24f, true}, {0.25f, true}, {0.26f, true},
            {0.30f, true}, {0.50f, true}, {0.80f, true}, {1.00f, true},
            {0.25f, false}, {0.25f, true}};
        for (const auto& [density, slx] : cases) {
            solid->MeshQuadroHoleSLX = slx;
            require(solid->ReBuldMesh(1.0f / density), "Double-cut Box rebuild failed.");
            std::vector<const CMesh3D*> meshes;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                const auto* surface = solid->GetSurfaceFace(i);
                require(surface && surface->pMesh3D, "Double-cut Box lost a surface.");
                const auto& mesh = *surface->pMesh3D;
                require(ActiveFaceEdgeComponentCount(mesh) == 1,
                        "Double-cut Box has disconnected surface patches.");
                double twice_area = 0;
                for (const auto& face : mesh.GetFaces()) {
                    if (face.deleted) continue;
                    require(face.corners.size() == 3 || face.corners.size() == 4,
                            "Double-cut Box retained an unsupported polygon.");
                    Vec3 area{};
                    for (size_t j = 0; j < face.corners.size(); ++j) {
                        const size_t a = face.corners[j].v;
                        const size_t b = face.corners[(j + 1) % face.corners.size()].v;
                        require(a < mesh.GetVertices().size() && b < mesh.GetVertices().size(),
                                "Double-cut Box has an invalid vertex index.");
                        const Vec3 p = mesh.GetVertices()[a], q = mesh.GetVertices()[b];
                        require(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z),
                                "Double-cut Box has a non-finite vertex.");
                        const Vec3 origin = mesh.GetVertices()[face.corners[0].v];
                        area = area + cross(p - origin, q - origin);
                    }
                    require(dot(area, area) > 1.0e-14f, "Double-cut Box has a collapsed cell.");
                    if (i == 3) twice_area += std::fabs(area.z);
                }
                if (i == 3) {
                    bool manifold = false;
                    require(ClosedMeshBoundaryLoopCount(mesh, manifold) == 2 && manifold,
                            "Double-cut Box top face lost its pocket boundary.");
                    require(std::fabs(twice_area * 0.5 - 19690.6922698) < 0.1,
                            "Double-cut Box top face has missing or excess area.");
                }
                meshes.push_back(&mesh);
            }
            auto welded = CMesh3D::CreateWelded(meshes);
            bool manifold = false;
            require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                    "Double-cut Box has an open or non-manifold welded seam.");
            std::cout << "Box double cut density=" << density << " slx=" << slx
                      << " vertices=" << welded->GetVertices().size() << '\n';
        }
    }
    require(tested == 1, "Double-cut Box fixture must contain one solid.");
}

void TestMeshPointSplitReallocation() {
    for (const auto& [first, second] : std::vector<std::pair<int, int>>{
            {0,2}, {1,3}, {0,1}, {1,2}, {2,3}, {3,0}}) {
        CMesh3D mesh;
        require(mesh.SetGeometry({{0,0,0}, {1,0,0}, {1,1,0}, {0,1,0}},
            {CMesh3D::Face{0,1,2,3}}), "Point-split setup failed.");
        mesh.GetFaces().shrink_to_fit();
        require(mesh.SplitFaceByPoint(0, first, second, cVec2(0.35, 0.55)),
                "Point split failed after forced reallocation.");
        double area = 0;
        for (const auto& face : mesh.GetFaces()) {
            require(face.corners.size() >= 3 && face.corners.size() <= 4,
                    "Point split retained an invalid face.");
            double twice_area = 0;
            for (size_t j = 0; j < face.corners.size(); ++j) {
                const Vec3 p = mesh.GetVertices().at(face.corners[j].v);
                const Vec3 q = mesh.GetVertices().at(face.corners[(j + 1) % face.corners.size()].v);
                twice_area += p.x * q.y - p.y * q.x;
            }
            require(twice_area > 0, "Point split inverted or collapsed a face.");
            area += twice_area * 0.5;
        }
        bool manifold = false;
        require(std::fabs(area - 1.0) < 1.e-6 && ActiveFaceEdgeComponentCount(mesh) == 1
            && ClosedMeshBoundaryLoopCount(mesh, manifold) == 1 && manifold,
            "Point split changed its source area or boundary after reallocation.");
    }
}

void TestMeshVar5Reallocation() {
    CMesh3D mesh;
    require(mesh.SetGeometry({{0,0,0}, {1,0,0}, {2,0,0}, {0,1,0}, {1,1,0}, {2,1,0}},
        {CMesh3D::Face{0,1,4,3}, CMesh3D::Face{1,2,5,4}}), "Var-5 setup failed.");
    mesh.GetFaces().shrink_to_fit(); // The first appended face must relocate the array.
    mesh.GetFaces()[1].pm = CPoint3d(1.5, 0.5, 0.0);
    mesh.GetFaces()[1].edgeIndex = 3;
    cVec2 center(0.5, 0.5);
    require(mesh.SplitFaceByVar5(0, 0, 1, center), "Var-5 pair split failed.");
    require(mesh.GetFaces().size() == 8 && mesh.GetVertices().size() == 8,
            "Var-5 pair split changed its expected topology.");
    double area = 0.0;
    for (const auto& face : mesh.GetFaces()) {
        require(face.corners.size() == 3, "Var-5 retained a non-triangle.");
        const Vec3 a = mesh.GetVertices().at(face.corners[0].v);
        const Vec3 b = mesh.GetVertices().at(face.corners[1].v);
        const Vec3 c = mesh.GetVertices().at(face.corners[2].v);
        const double twice_area = cross(b - a, c - a).z;
        require(twice_area > 0, "Var-5 inverted or collapsed a triangle.");
        area += twice_area * 0.5;
    }
    bool manifold = false;
    require(std::fabs(area - 2.0) < 1.e-8
        && ActiveFaceEdgeComponentCount(mesh) == 1
        && ClosedMeshBoundaryLoopCount(mesh, manifold) == 1 && manifold,
        "Var-5 changed the area or boundary of its source pair.");
}

void TestBooleanThreeSlx(const QString& path) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error), error.toLocal8Bit().constData());
    size_t tested = 0;
    for (const auto& object : document.GetObjects()) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid) continue;
        ++tested;
        require(solid->GetNumSurfaces() == 24, "Boolean-3 topology changed.");
        solid->MeshQuadro = true;
        for (const auto& [density, slx] : std::vector<std::pair<float, bool>>{
                {0.20f, true}, {0.25f, true}, {0.30f, true}, {0.34f, true},
                {0.35f, true}, {0.36f, true}, {0.40f, true}, {0.50f, true},
                {0.70f, true}, {0.85f, true}, {1.00f, true},
                {0.35f, false}, {0.35f, true}}) {
            const auto begin = std::chrono::steady_clock::now();
            solid->MeshQuadroHoleSLX = slx;
            require(solid->ReBuldMesh(1.0f / density), "Boolean-3 rebuild failed.");
            std::vector<const CMesh3D*> meshes;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                const auto* surface = solid->GetSurfaceFace(i);
                require(surface && surface->pMesh3D, "Boolean-3 lost a surface.");
                const auto& mesh = *surface->pMesh3D;
                require(ActiveFaceEdgeComponentCount(mesh) == 1,
                        "Boolean-3 has disconnected surface patches.");
                double area = 0;
                for (const auto& face : mesh.GetFaces()) {
                    if (face.deleted) continue;
                    require(face.corners.size() == 3 || face.corners.size() == 4,
                            "Boolean-3 retained an unsupported polygon.");
                    Vec3 normal{};
                    const Vec3 origin = mesh.GetVertices().at(face.corners[0].v);
                    for (size_t j = 0; j < face.corners.size(); ++j) {
                        const Vec3 p = mesh.GetVertices().at(face.corners[j].v);
                        const Vec3 q = mesh.GetVertices().at(face.corners[(j + 1) % face.corners.size()].v);
                        require(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z),
                                "Boolean-3 has a non-finite vertex.");
                        normal = normal + cross(p - origin, q - origin);
                    }
                    require(dot(normal, normal) > 1.e-14f, "Boolean-3 has a collapsed cell.");
                    area += std::fabs(normal.y) * 0.5;
                }
                if (i == 1 || i == 3) {
                    bool manifold = false;
                    require(ClosedMeshBoundaryLoopCount(mesh, manifold) == 4 && manifold,
                            "Boolean-3 plane lost its outer boundary or a hexagonal hole.");
                    const double expected = 19.0 * 15.5 - 1.5 * std::sqrt(3.0) * (8.0 + 2.3 * 2.3);
                    require(std::fabs(area - expected) < 1.e-3,
                            "Boolean-3 plane has missing or excess area around its holes.");
                    for (int edge = 0; edge < surface->GetPreparedPolylineCount(); ++edge) {
                        std::vector<CPoint3d> points;
                        require(surface->GetPreparedPolylinePoints(edge, points),
                                "Boolean-3 lost a prepared edge.");
                        for (const auto& point : points) {
                            require(std::any_of(mesh.GetVertices().begin(), mesh.GetVertices().end(),
                                [&](const Vec3& vertex) {
                                    const double dx = vertex.x - point.x, dy = vertex.y - point.y;
                                    const double dz = vertex.z - point.z;
                                    return dx * dx + dy * dy + dz * dz < 1.e-8;
                                }), "Boolean-3 omitted an original CAD boundary node.");
                        }
                    }
                }
                meshes.push_back(&mesh);
            }
            auto welded = CMesh3D::CreateWelded(meshes);
            bool manifold = false;
            require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                    "Boolean-3 has an open or non-manifold welded seam.");
            std::cout << "Boolean-3 density=" << density << " slx=" << slx
                      << " vertices=" << welded->GetVertices().size() << " seconds="
                      << std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count()
                      << std::endl;
        }
    }
    require(tested == 1, "Boolean-3 fixture must contain one solid.");
}

void TestBooleanFourFillQuadro(const QString& path) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error),
            error.toLocal8Bit().constData());
    size_t tested = 0;
    for (const auto& object : document.GetObjects()) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid) continue;
        ++tested;
        require(solid->GetNumSurfaces() == 148, "Boolean-4 topology changed.");
        solid->MeshQuadro = true;
        // Reported densities/modes, plus neighbouring successful cases and a
        // repeat after a dense build. Other known defects are listed in the Wiki.
        for (const auto& [density, slx] : std::vector<std::pair<float, bool>>{
                {0.30f, true}, {0.35f, true}, {0.70f, true},
                {0.30f, false}, {0.35f, false}, {0.70f, false}, {0.30f, true}}) {
            const auto begin = std::chrono::steady_clock::now();
            solid->MeshQuadroHoleSLX = slx;
            require(solid->ReBuldMesh(1.0f / density), "Boolean-4 rebuild failed.");
            std::vector<const CMesh3D*> meshes;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                const auto* surface = solid->GetSurfaceFace(i);
                require(surface && surface->pMesh3D, "Boolean-4 lost a surface.");
                const auto& mesh = *surface->pMesh3D;
                require(ActiveFaceEdgeComponentCount(mesh) == 1,
                        "Boolean-4 has disconnected surface patches.");
                if (i == 12 || i == 120) {
                    bool manifold = false;
                    require(ClosedMeshBoundaryLoopCount(mesh, manifold) == 5 && manifold,
                            "Boolean-4 plane lost its outer boundary or one of its four holes.");
                    SurfaceUVMapping mapping(const_cast<CSurfaceFace*>(surface));
                    double area = 0.0;
                    for (const auto& face : mesh.GetFaces()) {
                        if (face.deleted) continue;
                        require(face.corners.size() == 3 || face.corners.size() == 4,
                                "Boolean-4 has an unsupported polygon.");
                        Vec3 center{}, normal{};
                        const Vec3 origin = mesh.GetVertices().at(face.corners[0].v);
                        for (size_t j = 0; j < face.corners.size(); ++j) {
                            const Vec3 p = mesh.GetVertices().at(face.corners[j].v);
                            const Vec3 q = mesh.GetVertices().at(
                                face.corners[(j + 1) % face.corners.size()].v);
                            require(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z)
                                && std::fabs(p.y - (i == 12 ? 0.0 : 2.0)) < 1.e-5,
                                "Boolean-4 plane was displaced onto an adjacent fillet edge.");
                            center = center + p;
                            normal = normal + cross(p - origin, q - origin);
                        }
                        area += std::fabs(normal.y) * 0.5;
                        center = center * (1.0f / face.corners.size());
                        SurfaceUVPoint uv;
                        require(mapping.Project(center, uv), "Boolean-4 UV projection failed.");
                        BRepClass_FaceClassifier classifier(TopoDS::Face(surface->m_Face),
                            gp_Pnt2d(uv.u, uv.v), 1.e-6, Standard_False);
                        require(classifier.State() != TopAbs_OUT,
                                "Boolean-4 filled a through hole or escaped its CAD face.");
                    }
                    require(area > 523.0 && area < 525.0,
                            "Boolean-4 has missing or excess planar area.");
                }
                meshes.push_back(&mesh);
            }
            auto welded = CMesh3D::CreateWelded(meshes);
            bool manifold = false;
            require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                    "Boolean-4 has an open or non-manifold welded seam.");
            std::cout << "Boolean-4 density=" << density << " slx=" << slx
                      << " vertices=" << welded->GetVertices().size() << " seconds="
                      << std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count()
                      << std::endl;
        }
    }
    require(tested == 1, "Boolean-4 fixture must contain one solid.");
    for (const auto& object : document.GetObjects()) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid) continue;
        auto* surface = solid->GetSurfaceFace(12);
        std::vector<CPoint3d> original;
        require(surface->GetPreparedPolylinePoints(17, original),
                "Boolean-4 lost the outer edge used by the broken-contour regression.");
        auto displaced = original;
        for (auto& point : displaced) { point.x += 0.2; point.y += 0.2; }
        require(surface->SetPreparedPolylinePoints(17, displaced),
                "Could not set up the broken-contour regression.");
        for (bool slx : {false, true}) {
            require(!surface->BuildFilledMeshWhithHoles(1.0f / 0.30f, slx)
                        && surface->pMesh3D->GetVertices().empty()
                        && surface->m_LastIslandFillError.find("Incomplete planar boundary") != std::string::npos,
                    "A missing outer contour must not fill the largest through hole.");
        }
        surface->SetPreparedPolylinePoints(17, original);
    }
}

void TestPrismFilletedSlx(const QString& path) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error),
            error.toLocal8Bit().constData());
    size_t tested = 0;
    for (const auto& object : document.GetObjects()) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid) continue;
        ++tested;
        require(solid->GetNumSurfaces() == 62, "Filleted prism topology changed.");
        solid->MeshQuadro = true;
        const std::vector<std::pair<float, bool>> cases{
            {0.20f, true}, {0.24f, true}, {0.25f, true}, {0.26f, true},
            {0.35f, true}, {0.50f, true}, {0.80f, true}, {1.00f, true},
            {0.25f, false}, {0.25f, true}};
        for (const auto& [density, slx] : cases) {
            solid->MeshQuadroHoleSLX = slx;
            require(solid->ReBuldMesh(1.0f / density), "Filleted prism rebuild failed.");
            std::vector<const CMesh3D*> meshes;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                const auto* surface = solid->GetSurfaceFace(i);
                require(surface && surface->pMesh3D, "Filleted prism lost a surface.");
                const auto& mesh = *surface->pMesh3D;
                require(ActiveFaceEdgeComponentCount(mesh) == 1,
                        "Filleted prism has disconnected surface patches.");
                double twice_area = 0;
                for (const auto& face : mesh.GetFaces()) {
                    if (face.deleted) continue;
                    require(face.corners.size() == 3 || face.corners.size() == 4,
                            "Filleted prism retained an unsupported polygon.");
                    Vec3 area{};
                    for (size_t j = 0; j < face.corners.size(); ++j) {
                        const size_t a = face.corners[j].v;
                        const size_t b = face.corners[(j + 1) % face.corners.size()].v;
                        require(a < mesh.GetVertices().size() && b < mesh.GetVertices().size(),
                                "Filleted prism has an invalid vertex index.");
                        const Vec3 p = mesh.GetVertices()[a], q = mesh.GetVertices()[b];
                        require(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z),
                                "Filleted prism has a non-finite vertex.");
                        const Vec3 origin = mesh.GetVertices()[face.corners[0].v];
                        area = area + cross(p - origin, q - origin);
                    }
                    if (i != 8 && i != 49) // The two OCCT spherical poles have a collapsed row.
                        require(dot(area, area) > 1.0e-14f, "Filleted prism has a collapsed cell.");
                    if (i == 9 || i == 10) twice_area += std::fabs(area.z);
                }
                if (i == 9 || i == 10) {
                    bool manifold = false;
                    require(ClosedMeshBoundaryLoopCount(mesh, manifold) == 2 && manifold,
                            "Filleted prism top face lost its pocket boundary.");
                    require(twice_area > 0, "Filleted prism top face is empty.");
                    for (int edge = 0; edge < surface->GetPreparedPolylineCount(); ++edge) {
                        std::vector<CPoint3d> points;
                        require(surface->GetPreparedPolylinePoints(edge, points),
                                "Filleted prism lost its prepared boundary.");
                        for (const auto& point : points) {
                            bool found = false;
                            for (const Vec3& vertex : mesh.GetVertices()) {
                                const double dx = vertex.x - point.x, dy = vertex.y - point.y;
                                const double dz = vertex.z - point.z;
                                if (dx * dx + dy * dy + dz * dz < 1.e-8) { found = true; break; }
                            }
                            require(found, "Filleted prism omitted a prepared CAD boundary node.");
                        }
                    }
                }
                meshes.push_back(&mesh);
            }
            auto welded = CMesh3D::CreateWelded(meshes);
            bool manifold = false;
            require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                    "Filleted prism has an open or non-manifold welded seam.");
            std::cout << "Filleted prism density=" << density << " slx=" << slx
                      << " vertices=" << welded->GetVertices().size() << '\n';
        }
    }
    require(tested == 1, "Filleted prism fixture must contain one solid.");
}

void TestChamferAndFilletsQuadro(const QString& path) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error),
            error.toLocal8Bit().constData());
    size_t tested = 0;
    for (const auto& object : document.GetObjects()) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid) continue;
        ++tested;
        require(solid->GetNumSurfaces() == 42, "Chamfer fixture topology changed.");
        solid->MeshQuadro = true;
        solid->MeshQuadroHoleSLX = false;
        for (float density : {0.20f, 0.35f, 0.50f, 0.65f, 0.80f, 1.0f, 0.50f}) {
            require(solid->ReBuldMesh(1.0f / density), "Chamfer Quadro rebuild failed.");
            std::vector<const CMesh3D*> meshes;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                const CSurfaceFace* surface = solid->GetSurfaceFace(i);
                require(surface && surface->pMesh3D, "Chamfer lost a surface mesh.");
                const CMesh3D& mesh = *surface->pMesh3D;
                require(ActiveFaceEdgeComponentCount(mesh) == 1,
                        "Chamfer has disconnected surface fragments.");
                const bool pole_corner = i == 26 || i == 30 || i == 40 || i == 41;
                size_t active_faces = 0;
                for (const auto& face : mesh.GetFaces()) {
                    if (face.deleted) continue;
                    ++active_faces;
                    require(face.corners.size() == 4, "Chamfer retained a non-quad cell.");
                    if (!pole_corner) continue;
                    Vec3 area{};
                    for (size_t j = 0; j < 4; ++j) {
                        const Vec3 a = mesh.GetVertices().at(face.corners[j].v);
                        const Vec3 b = mesh.GetVertices().at(face.corners[(j + 1) % 4].v);
                        const Vec3 origin = mesh.GetVertices().at(face.corners[0].v);
                        require(std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z)
                                    && dot(b - a, b - a) > 1.0e-12f,
                                "Chamfer pole quad has invalid or collapsed edges.");
                        area = area + cross(a - origin, b - origin);
                    }
                    require(dot(area, area) > 1.0e-12f, "Chamfer pole quad has zero area.");
                }
                if (pole_corner) {
                    require(surface->GetLastQuadrangulationDiagnostic().empty()
                                && surface->GetLastIslandFillError().empty(),
                            "Chamfer pole corner used an emergency fallback.");
                    if (density == 0.5f)
                        require(active_faces == 4, "UV classifier removed a valid pole quad.");
                }
                meshes.push_back(&mesh);
            }
            auto welded = CMesh3D::CreateWelded(meshes);
            bool manifold = false;
            require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                    "Chamfer retained a hole at a spherical pole.");
            std::cout << "ChamferAndFillets density=" << density
                      << " weldedVertices=" << welded->GetVertices().size() << '\n';
        }
    }
    require(tested == 1, "Chamfer regression must inspect one source solid.");
}

void TestPrismTwoFilletsQuadro(const QString& path) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error),
            error.toLocal8Bit().constData());
    size_t tested = 0;
    for (const auto& object : document.GetObjects()) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid || solid->GetName() != "Prism") continue;
        ++tested;
        require(solid->GetNumSurfaces() == 10, "Prism fixture topology changed.");
        solid->MeshQuadro = true;
        solid->MeshQuadroHoleSLX = false;
        CSurfaceFace* corner = solid->GetSurfaceFace(5);
        const TopoDS_Face face = TopoDS::Face(corner->m_Face);
        BRepAdaptor_Surface adaptor(face);
        require(adaptor.GetType() == GeomAbs_BSplineSurface,
                "Prism singular corner is no longer a B-Spline surface.");
        double u0, u1, v0, v1;
        BRepTools::UVBounds(face, u0, u1, v0, v1);
        for (double a : {0.0, 0.5, 1.0}) {
            for (double b : {0.0, 0.5, 1.0}) {
                const double u = u0 + (u1 - u0) * a;
                const double v = v0 + (v1 - v0) * b;
                CPoint8d point;
                require(corner->GetPoint(u, v, &point),
                        "Prism singular boundary has no point/normal.");
                const gp_Pnt exact = adaptor.Value(u, v);
                require(exact.Distance(gp_Pnt(point.x, point.y, point.z)) < 1.0e-7,
                        "Normal recovery moved the original surface point.");
                const double length2 = point.l * point.l + point.m * point.m + point.n * point.n;
                require(std::isfinite(length2) && std::abs(length2 - 1.0) < 1.0e-8,
                        "Prism recovered normal is not finite and unit length.");
            }
        }
        for (float density : {0.20f, 0.40f, 0.50f, 0.65f, 0.80f, 1.0f, 0.40f}) {
            require(solid->ReBuldMesh(1.0f / density), "Prism Quadro rebuild failed.");
            std::vector<const CMesh3D*> meshes;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                const auto* surface = solid->GetSurfaceFace(i);
                require(surface && surface->pMesh3D, "Prism lost a surface mesh.");
                require(surface->GetLastQuadrangulationDiagnostic().empty(),
                        "Prism used a quadrangulation fallback.");
                const auto& mesh = *surface->pMesh3D;
                require(ActiveFaceEdgeComponentCount(mesh) == 1,
                        "Prism surface is empty or disconnected.");
                for (const auto& cell : mesh.GetFaces())
                    if (!cell.deleted) {
                        if (cell.corners.size() != 4)
                            std::cerr << "Prism non-quad surface=" << i
                                << " type=" << BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                                << " edges=" << surface->GetPreparedPolylineCount() << '\n';
                        require(cell.corners.size() == 4, "Prism source net contains a non-quad.");
                    }
                meshes.push_back(&mesh);
            }
            // ReBuldMesh replaces the CSurfaceFace instances.
            corner = solid->GetSurfaceFace(5);
            require(corner->IsInitMesh && corner->m_QtyU >= 3 && corner->m_QtyV >= 3,
                    "Prism corner net was not initialized.");
            auto welded = CMesh3D::CreateWelded(meshes);
            bool manifold = false;
            require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                    "Prism has an open seam around its singular corner.");
            const QByteArray dump_prefix = qgetenv("DOM3D_WELD_DUMP");
            if (density == 0.40f && !dump_prefix.isEmpty())
                require(welded->ExportToObj(dump_prefix.toStdString() + "-prism.obj"),
                        "Could not export the Prism diagnostic mesh.");
            std::cout << "Prism_And_2_Fill_Var-2 density=" << density
                      << " weldedVertices=" << welded->GetVertices().size() << '\n';
        }
    }
    require(tested == 1, "Expected one named Prism body in the five-solid fixture.");
}

void TestBezierCollarWelding(const QString& path) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room;
    ProjectViewState view;
    QString error;
    require(serializer.Load(path, document, room, view, error),
            error.toLocal8Bit().constData());
    size_t tested_solids = 0;
    for (const auto& object : document.GetObjects()) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid)
            continue;
        solid->MeshQuadro = true;
        solid->MeshQuadroHoleSLX = true;
        require(solid->ReBuldMesh(2.0f), "Bezier-4 SLX Density 0.5 rebuild failed.");
        std::vector<const CMesh3D*> meshes;
        size_t source_faces = 0;
        for (int index = 0; index < solid->GetNumSurfaces(); ++index) {
            const CSurfaceFace* surface = solid->GetSurfaceFace(index);
            require(surface && surface->pMesh3D, "Bezier-4 lost a surface mesh.");
            meshes.push_back(surface->pMesh3D);
            for (const CMesh3D::Face& face : surface->pMesh3D->GetFaces())
                source_faces += !face.deleted;
        }
        float tolerance = 0.0f;
        auto welded = CMesh3D::CreateWelded(meshes, nullptr, &tolerance);
        // Full-density sampling, with the shared Var-7 region cut atomically:
        // one former quad is represented by two triangles (3055 -> 3056).
        // A/B OBJ comparison preserves every vertex, perimeter edge and area.
        require(welded && welded->GetFaces().size() == source_faces
                    && source_faces == 3056,
                "Bezier-4 welding changed or lost mesh cells.");
        require(std::abs(tolerance - 0.0224838f) < 1.0e-6f,
                "Bezier-4 welding no longer uses the minimum edge / 5.");
        bool manifold = false;
        require(ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                "Bezier-4 collar retained open seams after welding.");
        for (const CMesh3D::Face& face : welded->GetFaces()) {
            std::set<size_t> corners;
            for (const MeshCorner& corner : face.corners)
                corners.insert(corner.v);
            require(corners.size() == face.corners.size(),
                    "Bezier-4 welding collapsed a mesh cell.");
        }
        ++tested_solids;
    }
    require(tested_solids == 1, "Bezier-4 regression did not test its solid.");
}

void TestFourHoleCollars(const QString& path) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error), error.toLocal8Bit().constData());
    CSolid* solid = nullptr;
    for (const auto& object : document.GetObjects())
        if (auto* body = dynamic_cast<CSolid*>(object.get())) {
            require(!solid, "Four-hole fixture must contain one solid.");
            solid = body;
        }
    require(solid && solid->GetNumSurfaces() == 14, "Four-hole fixture topology changed.");
    solid->MeshQuadro = true;
    for (bool divide_face : {false, true}) {
        solid->MeshQuadroHoleSLX = !divide_face;
        solid->MeshQuadroHoleDivideFace = divide_face;
        for (float density : {0.20f, 0.25f, 0.30f, 0.35f, 0.40f, 0.50f, 0.25f, 0.20f}) {
            require(solid->ReBuldMesh(1.0f / density), "Four-hole collar rebuild failed.");
            const CSurfaceFace* surface = solid->GetSurfaceFace(4);
            require(surface && surface->pMesh3D, "Four-hole plate is missing.");
            const auto& mesh = *surface->pMesh3D;
            bool manifold = false;
            require(ClosedMeshBoundaryLoopCount(mesh, manifold) == 5 && manifold
                        && ActiveFaceEdgeComponentCount(mesh) == 1,
                    "Four collars must keep four distinct holes in one connected plate.");
            double mesh_area = 0.0;
            for (const auto& cell : mesh.GetFaces()) {
                if (cell.deleted) continue;
                require(cell.corners.size() == 3 || cell.corners.size() == 4,
                        "Four-hole plate contains an unsupported polygon.");
                const Vec3 a = mesh.GetVertices()[cell.corners[0].v];
                for (size_t j = 1; j + 1 < cell.corners.size(); ++j) {
                    const Vec3 b = mesh.GetVertices()[cell.corners[j].v];
                    const Vec3 c = mesh.GetVertices()[cell.corners[j + 1].v];
                    const double area = (double(b.x) - a.x) * (c.y - a.y)
                        - (double(b.y) - a.y) * (c.x - a.x);
                    require(area > 1.0e-7, "Four-hole collar has a folded display triangle.");
                    mesh_area += area * 0.5;
                }
            }
            size_t circles = 0;
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(surface->m_Face, properties);
            double expected_area = properties.Mass();
            for (int edge = 0; edge < surface->GetPreparedPolylineCount(); ++edge) {
                TopoDS_Edge topology;
                require(surface->GetPreparedTopoEdge(edge, topology), "Missing hole boundary.");
                BRepAdaptor_Curve curve(topology);
                if (curve.GetType() != GeomAbs_Circle) continue;
                ++circles;
                const auto circle = curve.Circle();
                const auto center = circle.Location();
                const double radius = circle.Radius();
                const size_t sectors = surface->GetPreparedPolylinePointCount(edge) - 1;
                constexpr double pi = 3.14159265358979323846;
                // The sampled polygon removes slightly less area than a CAD circle.
                expected_area += radius * radius * (pi - 0.5 * sectors * std::sin(2 * pi / sectors));
                require(sectors == size_t(std::lround(40 * density)),
                        "Hole sampling is pinned instead of following Density.");
                size_t radial_quads = 0;
                for (const auto& cell : mesh.GetFaces()) {
                    if (cell.deleted || cell.corners.size() != 4) continue;
                    std::array<double, 4> radii{};
                    for (size_t j = 0; j < 4; ++j) {
                        const auto p = mesh.GetVertices()[cell.corners[j].v];
                        radii[j] = std::hypot(p.x - center.X(), p.y - center.Y());
                    }
                    std::sort(radii.begin(), radii.end());
                    if (std::fabs(radii[0] - radii[1]) < 1.0e-3
                        && std::fabs(radii[2] - radii[3]) < 1.0e-3
                        && radii[2] - radii[1] > 0.1
                        && radii[0] >= radius - 1.0e-3) ++radial_quads;
                }
                // Exact cuts insert extra nodes in the outer collar boundary;
                // that transition can contain triangles. Require a full inner row.
                require(radial_quads >= sectors * (!divide_face && density <= 0.35f ? 2 : 1),
                        "An individual hole lost its concentric quad collar.");
            }
            require(circles == 4, "Four-hole plate must retain four circular boundaries.");
            require(expected_area > 0.0 && std::fabs(mesh_area - expected_area) < expected_area * 1.0e-5,
                    "Four-hole collar overlaps or leaves uncovered area.");
            std::vector<const CMesh3D*> meshes;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i)
                meshes.push_back(solid->GetSurfaceFace(i)->pMesh3D);
            auto welded = CMesh3D::CreateWelded(meshes);
            require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                    "Four-hole body retained an open or nonmanifold seam.");
            std::cout << "Four hole collars DivideFace=" << divide_face << " density=" << density << " vertices="
                      << welded->GetVertices().size() << std::endl;
        }
    }
}

void TestSixHoleSharedZone(const QString& path) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error),
            error.toLocal8Bit().constData());
    CSolid* solid = nullptr;
    for (const auto& object : document.GetObjects()) {
        if (auto* body = dynamic_cast<CSolid*>(object.get())) {
            require(!solid, "Six-hole fixture must contain one solid.");
            solid = body;
        }
    }
    require(solid && solid->GetNumSurfaces() == 36,
            "Six-hole fixture topology changed.");
    solid->MeshQuadro = true;
    for (int mode : {0, 1, 2}) {
        solid->MeshQuadroHoleSLX = mode == 1;
        solid->MeshQuadroHoleDivideFace = mode == 2;
        for (float density : {0.25f, 0.35f, 0.40f, 0.50f}) {
            require(solid->ReBuldMesh(1.0f / density),
                    "Six-hole shared-zone rebuild failed.");
            const CSurfaceFace* surface = solid->GetSurfaceFace(4);
            require(surface && surface->pMesh3D,
                    "Six-hole top plate is missing.");
            const CMesh3D& mesh = *surface->pMesh3D;
            bool manifold = false;
            require(ClosedMeshBoundaryLoopCount(mesh, manifold) == 7
                        && manifold && ActiveFaceEdgeComponentCount(mesh) == 1,
                    "Shared zone must retain six separate holes in one plate.");
            double maximum_edge = 0.0;
            for (const CMesh3D::Face& face : mesh.GetFaces()) {
                if (face.deleted) continue;
                require(face.corners.size() == 3 || face.corners.size() == 4,
                        "Shared zone contains an unsupported polygon.");
                for (size_t j = 0; j < face.corners.size(); ++j) {
                    const Vec3 delta = mesh.GetVertices()[face.corners[j].v]
                        - mesh.GetVertices()[face.corners[(j + 1) % face.corners.size()].v];
                    maximum_edge = std::max(maximum_edge,
                        std::sqrt(static_cast<double>(dot(delta, delta))));
                }
            }
            require(maximum_edge < 20.0,
                    "Shared zone allowed a coarse spike across the plate.");
            std::vector<const CMesh3D*> meshes;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i)
                meshes.push_back(solid->GetSurfaceFace(i)->pMesh3D);
            auto welded = CMesh3D::CreateWelded(meshes);
            require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0
                        && manifold,
                    "Six-hole body retained an open or nonmanifold seam.");
        }
    }
}

void TestHoleLowDensityCollar(const QString& path) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error),
            error.toLocal8Bit().constData());
    size_t tested = 0;
    for (const auto& object : document.GetObjects()) {
        auto* solid = dynamic_cast<CSolid*>(object.get());
        if (!solid) continue;
        solid->MeshQuadro = true;
        solid->MeshQuadroHoleSLX = true;
        for (float density : {0.10f, 0.15f, 0.20f, 0.15f, 0.35f, 0.40f, 0.45f, 0.50f}) {
            // Exercise the dialog sequence too: islands, then SLX at the
            // same density. Cached geometry must not change the result.
            solid->MeshQuadroHoleSLX = false;
            require(solid->ReBuldMesh(1.0f / density), "Hole island preview failed.");
            solid->MeshQuadroHoleSLX = true;
            require(solid->ReBuldMesh(1.0f / density),
                    "Hole low-density SLX rebuild failed.");
            std::vector<const CMesh3D*> meshes;
            size_t holed_planes = 0;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                const CSurfaceFace* surface = solid->GetSurfaceFace(i);
                require(surface && surface->pMesh3D, "Hole lost a surface mesh.");
                meshes.push_back(surface->pMesh3D);
                if (BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                    != GeomAbs_Plane) continue;
                size_t wires = 0;
                for (TopExp_Explorer wire(surface->m_Face, TopAbs_WIRE);
                     wire.More(); wire.Next()) ++wires;
                if (wires != 2) continue;
                ++holed_planes;
                gp_Pnt center;
                double radius = 0.0;
                size_t angular_sectors = 0;
                for (int edge = 0; edge < surface->GetPreparedPolylineCount(); ++edge) {
                    TopoDS_Edge topology;
                    require(surface->GetPreparedTopoEdge(edge, topology),
                            "Hole lost its prepared edge topology.");
                    BRepAdaptor_Curve curve(topology);
                    if (curve.GetType() == GeomAbs_Line)
                        require(surface->GetPreparedPolylinePointCount(edge) >= 9,
                                "A holed plate must have at least eight boundary intervals.");
                    if (curve.GetType() == GeomAbs_Circle) {
                        center = curve.Circle().Location();
                        radius = curve.Circle().Radius();
                        angular_sectors = surface->GetPreparedPolylinePointCount(edge) - 1;
                    }
                }
                require(radius > 0.0, "Hole circular boundary was not found.");
                bool manifold = false;
                require(ClosedMeshBoundaryLoopCount(*surface->pMesh3D, manifold) == 2
                            && manifold && ActiveFaceEdgeComponentCount(*surface->pMesh3D) == 1,
                        "Hole collar must be connected with exactly two boundary loops.");
                size_t radial_quads = 0;
                for (const auto& face : surface->pMesh3D->GetFaces()) {
                    if (face.deleted) continue;
                    require(face.corners.size() == 3 || face.corners.size() == 4,
                            "Hole collar contains an unprocessed polygon.");
                    const auto& vertices = surface->pMesh3D->GetVertices();
                    const Vec3 a = vertices[face.corners[0].v];
                    for (size_t j = 1; j + 1 < face.corners.size(); ++j) {
                        const Vec3 b = vertices[face.corners[j].v];
                        const Vec3 c = vertices[face.corners[j + 1].v];
                        const double area = (static_cast<double>(b.x) - a.x) * (c.y - a.y)
                            - (static_cast<double>(b.y) - a.y) * (c.x - a.x);
                        require(area > 1.0e-7,
                                "Hole collar contains a reversed or folded display triangle.");
                    }
                    if (face.corners.size() != 4) continue;
                    std::array<double, 4> radii{};
                    for (size_t j = 0; j < 4; ++j) {
                        const Vec3 v = surface->pMesh3D->GetVertices()[face.corners[j].v];
                        radii[j] = std::hypot(v.x - center.X(), v.y - center.Y());
                    }
                    std::sort(radii.begin(), radii.end());
                    if (std::fabs(radii[0] - radii[1]) < 1.0e-3
                        && std::fabs(radii[2] - radii[3]) < 1.0e-3
                        && radii[2] - radii[1] > 1.0
                        && radii[0] >= radius - 1.0e-3) ++radial_quads;
                }
                require(radial_quads >= 2 * angular_sectors,
                        "Hole collar lost its two concentric rows of radial quads.");
            }
            require(holed_planes == 1, "Hole regression did not find the top plate.");
            auto welded = CMesh3D::CreateWelded(meshes);
            bool manifold = false;
            require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                    "Hole low-density mesh retained open seams after welding.");
            ++tested;
        }
    }
    require(tested == 8, "Hole regression did not check all density/mode transitions.");
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

void TestParametricCurveOffset() {
    const auto count_self_intersections = [](
            const std::vector<CPoint3d>& points, bool closed) {
        size_t count = 0;
        const size_t segment_count = closed
            ? points.size() : points.size() > 1 ? points.size() - 1 : 0;
        for (size_t first = 0; first < segment_count; ++first) {
            const size_t first_next = (first + 1) % points.size();
            const double first_dx = points[first_next].x - points[first].x;
            const double first_dy = points[first_next].y - points[first].y;
            for (size_t second = first + 1; second < segment_count; ++second) {
                const size_t second_next = (second + 1) % points.size();
                if (first_next == second || second_next == first) continue;
                const double second_dx = points[second_next].x - points[second].x;
                const double second_dy = points[second_next].y - points[second].y;
                const double denominator = first_dx * second_dy
                    - first_dy * second_dx;
                if (std::abs(denominator) <= 1.0e-10) continue;
                const double between_x = points[second].x - points[first].x;
                const double between_y = points[second].y - points[first].y;
                const double first_parameter =
                    (between_x * second_dy - between_y * second_dx)
                    / denominator;
                const double second_parameter =
                    (between_x * first_dy - between_y * first_dx)
                    / denominator;
                if (first_parameter > 1.0e-8 && first_parameter < 1.0 - 1.0e-8
                    && second_parameter > 1.0e-8
                    && second_parameter < 1.0 - 1.0e-8) {
                    ++count;
                }
            }
        }
        return count;
    };

    CAlfaDoc document;
    document.GetObjects().clear();
    auto source = std::make_unique<CPolyline>("Tilted planar rectangle");
    source->AddPoint({0.0, 0.0, 0.0});
    source->AddPoint({100.0, 0.0, 0.0});
    source->AddPoint({100.0, 70.0, 70.0});
    source->AddPoint({0.0, 70.0, 70.0});
    source->SetClosed(true);
    document.AddObject(std::move(source));
    CAlfaObject* source_object = document.GetSelectedObject();
    require(source_object != nullptr, "Curve Offset source was not created.");
    const unsigned long source_id = source_object->m_id;

    ToolRegistry tools;
    const std::vector<ToolParameter> parameters = {
        {"distance", "Distance", 10.0, -1000000.0, 1000000.0, 0.1},
        {"profile.id", "Source Curve ID", static_cast<double>(source_id),
            0.0, 4294967295.0, 1.0}
    };
    const ActiveParametricObject active = tools.CreateParametricObject(
        "CurveOffset", document, parameters);
    require(!active.tool_id.empty() && document.GetObjects().size() == 2,
            "Parametric Curve Offset was not created.");
    auto* offset = dynamic_cast<CPolyline*>(
        document.GetObjects()[active.object_index].get());
    require(offset && offset->IsClosed() && offset->GetPointCount() == 4,
            "Closed Curve Offset did not preserve its contour topology.");
    for (const CPoint3d& point : offset->GetPoints()) {
        require(std::abs(point.y - point.z) < 1.0e-6,
                "Curve Offset left the source plane.");
    }
    Vec3 before_min{};
    Vec3 before_max{};
    require(offset->GetBounds(before_min, before_max)
                && std::abs(before_min.x + 10.0) < 1.0e-5
                && std::abs(before_max.x - 110.0) < 1.0e-5,
            "Positive closed Curve Offset was not built outside.");

    auto* editable_source = dynamic_cast<CPolyline*>(
        document.FindObjectById(source_id));
    require(editable_source
                && editable_source->SetPoint(1, {150.0, 0.0, 0.0})
                && editable_source->SetPoint(2, {150.0, 70.0, 70.0}),
            "Curve Offset source could not be edited.");
    require(tools.ReplayProfileDependents(source_id, document),
            "Curve Offset did not replay after editing its source.");
    offset = dynamic_cast<CPolyline*>(
        document.GetObjects()[active.object_index].get());
    Vec3 after_min{};
    Vec3 after_max{};
    require(offset && offset->GetBounds(after_min, after_max)
                && std::abs(after_max.x - 160.0) < 1.0e-5,
            "Curve Offset did not rebuild from the edited source.");

    ActiveParametricObject edited_offset = active;
    for (ToolParameter& parameter : edited_offset.parameters) {
        if (parameter.id == "distance") parameter.value = -10.0;
    }
    tools.Rebuild(edited_offset, document);
    offset = dynamic_cast<CPolyline*>(
        document.GetObjects()[active.object_index].get());
    require(offset && offset->GetBounds(after_min, after_max)
                && std::abs(after_min.x - 10.0) < 1.0e-5
                && std::abs(after_max.x - 140.0) < 1.0e-5,
            "Negative closed Curve Offset was not built inside.");

    auto spline = std::make_unique<CBSpline>("Tilted open spline");
    spline->AddPoint({0.0, 0.0, 0.0});
    spline->AddPoint({35.0, 20.0, 20.0});
    spline->AddPoint({70.0, -10.0, -10.0});
    spline->AddPoint({110.0, 25.0, 25.0});
    document.AddObject(std::move(spline));
    const unsigned long spline_id = document.GetSelectedObject()->m_id;
    const ActiveParametricObject spline_offset = tools.CreateParametricObject(
        "CurveOffset", document,
        {{"distance", "Distance", 5.0, -1000000.0, 1000000.0, 0.1},
         {"profile.id", "Source Curve ID", static_cast<double>(spline_id),
             0.0, 4294967295.0, 1.0}});
    const auto* spline_result = spline_offset.object_index
            < document.GetObjects().size()
        ? dynamic_cast<const CBSpline*>(
              document.GetObjects()[spline_offset.object_index].get())
        : nullptr;
    require(spline_result && !spline_result->IsClosed()
                && spline_result->GetPointCount() >= 4
                && spline_result->GetPointCount() < 32,
            "Open B-Spline Offset was not fitted to a compact smooth curve.");
    for (const CPoint3d& point : spline_result->GetPoints()) {
        require(std::abs(point.y - point.z) < 1.0e-5,
                "Open B-Spline Offset left the source plane.");
    }

    auto closed_spline = std::make_unique<CBSpline>("Closed smooth spline");
    for (const CPoint3d& point : std::vector<CPoint3d>{
             {60.0, 0.0, 0.0}, {42.0, 35.0, 0.0},
             {0.0, 50.0, 0.0}, {-42.0, 35.0, 0.0},
             {-60.0, 0.0, 0.0}, {-42.0, -35.0, 0.0},
             {0.0, -50.0, 0.0}, {42.0, -35.0, 0.0}}) {
        closed_spline->AddPoint(point);
    }
    closed_spline->SetClosed(true);
    document.AddObject(std::move(closed_spline));
    const unsigned long closed_spline_id = document.GetSelectedObject()->m_id;
    const ActiveParametricObject closed_spline_offset =
        tools.CreateParametricObject(
            "CurveOffset", document,
            {{"distance", "Distance", 7.5, -1000000.0, 1000000.0, 0.1},
             {"delete_loops", "Delete Loops", 1.0, 0.0, 1.0, 1.0,
                 ToolParameterType::Checkbox},
             {"profile.id", "Source Curve ID",
                 static_cast<double>(closed_spline_id),
                 0.0, 4294967295.0, 1.0}});
    const auto* closed_spline_result = dynamic_cast<const CBSpline*>(
        document.GetObjects()[closed_spline_offset.object_index].get());
    const CPoint3d closed_start = closed_spline_result
        ? closed_spline_result->Evaluate(0.0f) : CPoint3d{};
    const CPoint3d closed_end = closed_spline_result
        ? closed_spline_result->Evaluate(1.0f) : CPoint3d{};
    require(closed_spline_result && closed_spline_result->IsClosed()
                && closed_spline_result->GetPointCount() <= 16
                && (closed_start - closed_end).Length() < 0.05f,
            "Closed smooth Curve Offset has too many poles or an open seam.");

    auto concave = std::make_unique<CPolyline>("Concave loop test");
    for (const CPoint3d& point : std::vector<CPoint3d>{
             {0.0, 0.0, 0.0}, {100.0, 0.0, 0.0},
             {100.0, 40.0, 0.0}, {40.0, 40.0, 0.0},
             {80.0, 50.0, 0.0}, {40.0, 60.0, 0.0},
             {100.0, 60.0, 0.0}, {100.0, 100.0, 0.0},
             {0.0, 100.0, 0.0}}) {
        concave->AddPoint(point);
    }
    concave->SetClosed(true);
    document.AddObject(std::move(concave));
    const unsigned long concave_id = document.GetSelectedObject()->m_id;
    const ActiveParametricObject raw_loop_offset = tools.CreateParametricObject(
        "CurveOffset", document,
        {{"distance", "Distance", -5.0, -1000000.0, 1000000.0, 0.1},
         {"delete_loops", "Delete Loops", 0.0, 0.0, 1.0, 1.0,
             ToolParameterType::Checkbox},
         {"profile.id", "Source Curve ID", static_cast<double>(concave_id),
             0.0, 4294967295.0, 1.0}});
    const auto* raw_loop = dynamic_cast<const CPolyline*>(
        document.GetObjects()[raw_loop_offset.object_index].get());
    require(raw_loop
                && count_self_intersections(
                       raw_loop->GetPoints(), raw_loop->IsClosed()) > 0,
            "Curve Offset loop regression contour did not produce a loop.");
    const ActiveParametricObject clean_loop_offset = tools.CreateParametricObject(
        "CurveOffset", document,
        {{"distance", "Distance", -5.0, -1000000.0, 1000000.0, 0.1},
         {"delete_loops", "Delete Loops", 1.0, 0.0, 1.0, 1.0,
             ToolParameterType::Checkbox},
         {"profile.id", "Source Curve ID", static_cast<double>(concave_id),
             0.0, 4294967295.0, 1.0}});
    const auto* clean_loop = dynamic_cast<const CPolyline*>(
        document.GetObjects()[clean_loop_offset.object_index].get());
    require(clean_loop && clean_loop->GetPointCount() >= 3
                && count_self_intersections(
                       clean_loop->GetPoints(), clean_loop->IsClosed()) == 0,
            "Curve Offset did not remove a self-intersection loop.");
}

void TestParametricCurveMirrorCopy() {
    CAlfaDoc document;
    document.GetObjects().clear();

    auto source = std::make_unique<CBSpline>("Frame guide");
    source->SetCurveType(SplineCurveType::Nurbs);
    source->AddPoint({2.0, 3.0, 4.0});
    source->AddPoint({5.0, 7.0, 8.0});
    source->AddPoint({9.0, 11.0, 12.0});
    source->SetDegree(2);
    source->SetWeights({1.0, 2.0, 1.5});
    require(source->SetKnots({0.0, 0.0, 0.0, 1.0, 1.0, 1.0}),
            "Mirror Copy source NURBS knots are invalid.");
    document.AddObject(std::move(source));
    auto* editable_source = dynamic_cast<CBSpline*>(document.GetSelectedObject());
    require(editable_source != nullptr,
            "Mirror Copy source was not created.");
    const unsigned long source_id = editable_source->m_id;

    ToolRegistry tools;
    const ActiveParametricObject first = tools.CreateParametricObject(
        "CurveMirrorCopy", document,
        {{"plane", "Mirror Plane", 0.0, 0.0, 2.0, 1.0,
             ToolParameterType::Combo, {"YZ", "XZ", "XY"}},
         {"offset", "Plane Offset", 10.0, -1000000.0, 1000000.0, 0.1},
         {"profile.id", "Source Curve ID", static_cast<double>(source_id),
             0.0, 4294967295.0, 1.0}});
    auto* mirrored = first.object_index < document.GetObjects().size()
        ? dynamic_cast<CBSpline*>(document.GetObjects()[first.object_index].get())
        : nullptr;
    require(mirrored && mirrored->GetCurveType() == SplineCurveType::Nurbs
                && mirrored->GetDegree() == 2
                && mirrored->GetWeights() == editable_source->GetWeights()
                && mirrored->GetKnots() == editable_source->GetKnots(),
            "Mirror Copy did not preserve the exact NURBS definition.");
    require(std::abs(mirrored->GetPoints()[0].x - 18.0) < 1.0e-6
                && std::abs(mirrored->GetPoints()[0].y - 3.0) < 1.0e-6
                && std::abs(mirrored->GetPoints()[0].z - 4.0) < 1.0e-6,
            "Mirror Copy did not reflect across the offset YZ plane.");
    const unsigned long first_id = mirrored->m_id;

    const ActiveParametricObject second = tools.CreateParametricObject(
        "CurveMirrorCopy", document,
        {{"plane", "Mirror Plane", 1.0, 0.0, 2.0, 1.0,
             ToolParameterType::Combo, {"YZ", "XZ", "XY"}},
         {"offset", "Plane Offset", 0.0, -1000000.0, 1000000.0, 0.1},
         {"profile.id", "Source Curve ID", static_cast<double>(first_id),
             0.0, 4294967295.0, 1.0}});
    require(second.object_index < document.GetObjects().size(),
            "Chained Mirror Copy was not created.");

    require(editable_source->SetPoint(0, {4.0, 6.0, 9.0}),
            "Mirror Copy source could not be edited.");
    require(tools.ReplayProfileDependents(source_id, document),
            "Mirror Copy dependency chain did not replay.");
    mirrored = dynamic_cast<CBSpline*>(document.FindObjectById(first_id));
    const auto* chained = dynamic_cast<const CBSpline*>(
        document.GetObjects()[second.object_index].get());
    require(mirrored && chained
                && std::abs(mirrored->GetPoints()[0].x - 16.0) < 1.0e-6
                && std::abs(mirrored->GetPoints()[0].y - 6.0) < 1.0e-6
                && std::abs(chained->GetPoints()[0].x - 16.0) < 1.0e-6
                && std::abs(chained->GetPoints()[0].y + 6.0) < 1.0e-6
                && std::abs(chained->GetPoints()[0].z - 9.0) < 1.0e-6,
            "Mirror Copy did not rebuild its chained dependent geometry.");
}

void TestParametricCurveLink() {
    CAlfaDoc document;
    document.GetObjects().clear();

    auto first = std::make_unique<CPolyline>("First rail");
    first->AddPoint({0.0, 0.0, 0.0});
    first->AddPoint({10.0, 0.0, 0.0});
    document.AddObject(std::move(first));
    const unsigned long first_id = document.GetSelectedObject()->m_id;

    auto second = std::make_unique<CBSpline>("Second rail");
    second->AddPoint({30.0, 10.0, 0.0});
    second->AddPoint({40.0, 10.0, 0.0});
    second->AddPoint({50.0, 15.0, 0.0});
    document.AddObject(std::move(second));
    const unsigned long second_id = document.GetSelectedObject()->m_id;

    ToolRegistry tools;
    ActiveParametricObject link = tools.CreateParametricObject(
        "CurveLinkedBridge", document,
        {{"mode", "Connection", 1.0, 0.0, 1.0, 1.0,
             ToolParameterType::Combo, {"Straight", "Smooth"}},
         {"handle_percent", "Handle Length", 25.0, 0.0, 200.0, 1.0},
         {"curve1.id", "Curve 1 ID", static_cast<double>(first_id),
             0.0, 4294967295.0, 1.0},
         {"curve1.end", "Curve 1 End", 1.0, 0.0, 1.0, 1.0,
             ToolParameterType::Combo, {"Start", "End"}},
         {"curve2.id", "Curve 2 ID", static_cast<double>(second_id),
             0.0, 4294967295.0, 1.0},
         {"curve2.end", "Curve 2 End", 0.0, 0.0, 1.0, 1.0,
             ToolParameterType::Combo, {"Start", "End"}}});
    auto* connector = link.object_index < document.GetObjects().size()
        ? dynamic_cast<CBSpline*>(document.GetObjects()[link.object_index].get())
        : nullptr;
    require(connector && connector->GetCurveType() == SplineCurveType::Bezier
                && connector->GetPointCount() == 4
                && (connector->GetPoints().front() - CPoint3d(10.0, 0.0, 0.0)).Length()
                    < 1.0e-6f
                && (connector->GetPoints().back() - CPoint3d(30.0, 10.0, 0.0)).Length()
                    < 1.0e-6f,
            "Linked Curve did not connect the requested endpoints.");
    require(connector->GetPoints()[1].x > connector->GetPoints()[0].x
                && connector->GetPoints()[2].x < connector->GetPoints()[3].x,
            "Smooth Linked Curve did not follow the endpoint tangents.");

    auto* editable_first = dynamic_cast<CPolyline*>(
        document.FindObjectById(first_id));
    auto* editable_second = dynamic_cast<CBSpline*>(
        document.FindObjectById(second_id));
    require(editable_first && editable_second
                && editable_first->SetPoint(1, {14.0, 3.0, 2.0})
                && editable_second->SetPoint(0, {34.0, 13.0, 5.0}),
            "Linked Curve sources could not be edited.");
    require(tools.ReplayProfileDependents(first_id, document)
                && tools.ReplayProfileDependents(second_id, document),
            "Linked Curve did not replay from both source curves.");
    connector = dynamic_cast<CBSpline*>(
        document.GetObjects()[link.object_index].get());
    require(connector
                && (connector->GetPoints().front() - CPoint3d(14.0, 3.0, 2.0)).Length()
                    < 1.0e-6f
                && (connector->GetPoints().back() - CPoint3d(34.0, 13.0, 5.0)).Length()
                    < 1.0e-6f,
            "Linked Curve endpoints did not follow their sources.");

    for (ToolParameter& parameter : link.parameters) {
        if (parameter.id == "mode") parameter.value = 0.0;
    }
    tools.Rebuild(link, document);
    connector = dynamic_cast<CBSpline*>(
        document.GetObjects()[link.object_index].get());
    const CPoint3d chord_third = connector
        ? connector->GetPoints().front()
            + (connector->GetPoints().back() - connector->GetPoints().front())
                * (1.0f / 3.0f)
        : CPoint3d{};
    require(connector
                && (connector->GetPoints()[1] - chord_third).Length() < 1.0e-6f,
            "Straight Linked Curve mode is not linear.");

    const ActiveParametricObject opposite = tools.CreateParametricObject(
        "CurveLinkedBridge", document,
        {{"mode", "Connection", 0.0, 0.0, 1.0, 1.0,
             ToolParameterType::Combo, {"Straight", "Smooth"}},
         {"handle_percent", "Handle Length", 33.0, 0.0, 200.0, 1.0},
         {"curve1.id", "Curve 1 ID", static_cast<double>(first_id),
             0.0, 4294967295.0, 1.0},
         {"curve1.end", "Curve 1 End", 0.0, 0.0, 1.0, 1.0,
             ToolParameterType::Combo, {"Start", "End"}},
         {"curve2.id", "Curve 2 ID", static_cast<double>(second_id),
             0.0, 4294967295.0, 1.0},
         {"curve2.end", "Curve 2 End", 1.0, 0.0, 1.0, 1.0,
             ToolParameterType::Combo, {"Start", "End"}}});
    const auto* opposite_connector = dynamic_cast<const CBSpline*>(
        document.GetObjects()[opposite.object_index].get());
    require(opposite_connector
                && (opposite_connector->GetPoints().front()
                    - editable_first->GetPoints().front()).Length() < 1.0e-6f
                && (opposite_connector->GetPoints().back()
                    - editable_second->GetPoints().back()).Length() < 1.0e-6f,
            "Linked Curve did not honor explicitly selected opposite ends.");
    for (auto& parameter : link.parameters)
        if (parameter.id == "mode") parameter.value = 2.0;
    tools.Rebuild(link, document);
    connector = dynamic_cast<CBSpline*>(document.GetObjects()[link.object_index].get());
    require(connector && connector->SetPointDirect(1, {17, 20, 8})
            && connector->SetPointDirect(2, {29, 25, -3}), "Cannot edit spline bridge poles");
    const unsigned long bridge_id = connector->m_id;
    const auto edited = connector->GetPoints();
    require(editable_first->SetPoint(1, {16,4,3}), "Cannot move source endpoint");
    tools.ReplayProfileDependents(first_id, document);
    connector = dynamic_cast<CBSpline*>(document.FindObjectById(bridge_id));
    require(connector && (connector->GetPoints()[1] - edited[1]).Length() < 1.e-6f
            && (connector->GetPoints()[2] - edited[2]).Length() < 1.e-6f
            && (connector->GetPoints().front() - CPoint3d{16,4,3}).Length() < 1.e-6f,
            "Spline bridge lost manual edits on source replay");
    QTemporaryDir directory;
    Dom3DProjectSerializer serializer;
    ProjectViewState view;
    QString error, room = "Lines";
    const QString path = directory.filePath("spline-bridge.dom3d");
    require(serializer.Save(path, document, room, view, {}, error), "Cannot save spline bridge");
    CAlfaDoc loaded;
    require(serializer.Load(path, loaded, room, view, error), "Cannot load spline bridge");
    auto* loaded_source = dynamic_cast<CBSpline*>(loaded.FindObjectById(second_id));
    require(loaded_source && loaded_source->SetPoint(0, {37,16,9}), "Cannot edit loaded source");
    tools.ReplayProfileDependents(second_id, loaded);
    const auto* loaded_bridge = dynamic_cast<const CBSpline*>(loaded.FindObjectById(bridge_id));
    require(loaded_bridge && (loaded_bridge->GetPoints()[1] - edited[1]).Length() < 1.e-6f
            && (loaded_bridge->GetPoints()[2] - edited[2]).Length() < 1.e-6f
            && (loaded_bridge->GetPoints().back() - CPoint3d{37,16,9}).Length() < 1.e-6f,
            "Reloaded spline bridge lost its shape or endpoint association");
}

// Convex fixtures only: their bounds center is inside the reference body.
void RequireHybridOutwardNormals(const CSolid& solid) {
    Bnd_Box bounds;
    BRepBndLib::Add(solid.m_Shape, bounds);
    double x0, y0, z0, x1, y1, z1;
    bounds.Get(x0, y0, z0, x1, y1, z1);
    const gp_Pnt center((x0+x1)/2, (y0+y1)/2, (z0+z1)/2);
    int inward_cad = 0, inward_mesh = 0, inverted_shading = 0;
    for (TopExp_Explorer faces(solid.m_Shape, TopAbs_FACE); faces.More(); faces.Next()) {
        const auto face = TopoDS::Face(faces.Current());
        BRepAdaptor_Surface surface(face);
        double u0, u1, v0, v1;
        BRepTools::UVBounds(face, u0, u1, v0, v1);
        gp_Pnt point;
        gp_Vec du, dv;
        surface.D1((u0+u1)/2, (v0+v1)/2, point, du, dv);
        gp_Vec normal = du.Crossed(dv);
        if (face.Orientation() == TopAbs_REVERSED) normal.Reverse();
        if (normal.Dot(gp_Vec(center, point)) < -1.e-6) {
            ++inward_cad;
            std::cout << "Inward face center=" << point.X() << "," << point.Y() << "," << point.Z() << "\n";
        }
    }
    for (int i = 0; i < solid.GetNumSurfaces(); ++i) {
        const auto* surface = solid.GetSurfaceFace(i);
        require(surface && surface->pMesh3D, "Hybrid surface has no display mesh");
        const auto& mesh = *surface->pMesh3D;
        const auto& vertices = mesh.GetVertices();
        for (const auto& face : mesh.GetFaces()) {
            if (face.deleted || face.corners.size() < 3) continue;
            const auto& a = vertices[face.corners[0].v];
            const auto& b = vertices[face.corners[1].v];
            const auto& c = vertices[face.corners[2].v];
            gp_Vec normal = gp_Vec(b.x-a.x,b.y-a.y,b.z-a.z).Crossed(
                gp_Vec(c.x-a.x,c.y-a.y,c.z-a.z));
            const gp_Pnt point((a.x+b.x+c.x)/3.,(a.y+b.y+c.y)/3.,(a.z+b.z+c.z)/3.);
            if (normal.Dot(gp_Vec(center, point)) < -1.e-6) ++inward_mesh;
            gp_Vec shading(0,0,0);
            for (const auto& corner : face.corners) {
                if (corner.n < mesh.GetNormals().size()) {
                    const auto& n = mesh.GetNormals()[corner.n];
                    shading += gp_Vec(n.x,n.y,n.z);
                }
            }
            if (normal.Dot(shading) < -1.e-6) ++inverted_shading;
        }
    }
    std::cout << "Hybrid inward CAD=" << inward_cad << " mesh=" << inward_mesh << "\n";
    require(inward_cad == 0, "Smart Hybrid has inward CAD faces");
    require(inward_mesh == 0, "Smart Hybrid has inward mesh polygons");
    require(inverted_shading == 0, "Smart Hybrid shading normals oppose mesh winding");
}

void TestParametricSmartHybrid(bool open = false, bool surface_only = false, bool reverse = false) {
    CAlfaDoc document;
    document.GetObjects().clear();
    const std::array<CPoint3d, 8> vertices{
        CPoint3d{0.0, 0.0, 0.0}, CPoint3d{100.0, 0.0, 0.0},
        CPoint3d{100.0, 80.0, 0.0}, CPoint3d{0.0, 80.0, 0.0},
        CPoint3d{0.0, 0.0, 60.0}, CPoint3d{100.0, 0.0, 60.0},
        CPoint3d{100.0, 80.0, 60.0}, CPoint3d{0.0, 80.0, 60.0}};
    const std::array<std::array<int, 2>, 12> edge_vertices{{
        {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
        {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
        {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}}};
    std::vector<unsigned long> curve_ids;
    for (size_t index = 0; index < edge_vertices.size(); ++index) {
        if (open && index == 0) continue;
        auto curve = std::make_unique<CPolyline>(
            "Frame edge " + std::to_string(index + 1));
        curve->AddPoint(vertices[static_cast<size_t>(edge_vertices[index][reverse ? 1 : 0])]);
        curve->AddPoint(vertices[static_cast<size_t>(edge_vertices[index][reverse ? 0 : 1])]);
        document.AddObject(std::move(curve));
        curve_ids.push_back(document.GetSelectedObject()->m_id);
    }

    ToolRegistry tools;
    const ToolDefinition* definition = tools.Find("SurfaceSmartHybrid");
    require(definition != nullptr, "Smart Hybrid tool is not registered.");
    std::vector<ToolParameter> parameters = definition->defaults;
    const auto set_value = [&parameters](const std::string& id, double value) {
        const auto found = std::find_if(
            parameters.begin(), parameters.end(), [&id](const ToolParameter& parameter) {
                return parameter.id == id;
            });
        require(found != parameters.end(), "Smart Hybrid parameter is missing.");
        found->value = value;
    };
    set_value("make_solid", surface_only ? 0.0 : 1.0);
    if (reverse) std::reverse(curve_ids.begin(), curve_ids.end());
    set_value("curve.count", static_cast<double>(curve_ids.size()));
    for (size_t index = 0; index < curve_ids.size(); ++index) {
        set_value("curve" + std::to_string(index + 1) + ".id",
                  static_cast<double>(curve_ids[index]));
    }
    const ActiveParametricObject hybrid = tools.CreateParametricObject(
        "SurfaceSmartHybrid", document, parameters);
    const auto* solid = hybrid.object_index < document.GetObjects().size()
        ? dynamic_cast<const CSolid*>(document.GetObjects()[hybrid.object_index].get())
        : nullptr;
    require(solid
                && ((open || surface_only) == (dynamic_cast<const CSurfaceSet*>(solid) != nullptr))
                && !solid->m_Shape.IsNull()
                && (open || surface_only || solid->m_Shape.ShapeType() == TopAbs_SOLID)
                && BRepCheck_Analyzer(solid->m_Shape).IsValid(),
            "Smart Hybrid did not convert a closed curve frame into a valid Solid.");
    RequireHybridOutwardNormals(*solid);
    require(tools.ReplayProfileDependents(curve_ids.front(), document),
            "Smart Hybrid did not participate in the parametric dependency graph.");
    solid = dynamic_cast<const CSolid*>(
        document.GetObjects()[hybrid.object_index].get());
    require(solid && !solid->m_Shape.IsNull(),
            "Smart Hybrid lost its Solid after a parametric replay.");
    RequireHybridOutwardNormals(*solid);
}

void TestLargeSmartHybrid(const char* source_path = nullptr, size_t expected_faces = 0) {
    CAlfaDoc document;
    document.GetObjects().clear();
    Dom3DProjectSerializer serializer;
    ProjectViewState view;
    QString error, room = "Surfaces";
    if (source_path) {
        require(serializer.Load(QString::fromLocal8Bit(source_path), document,
                                room, view, error), "Cannot load Smart Hybrid fixture");
    } else {
        const auto add_edge = [&](CPoint3d first, CPoint3d second) {
            auto curve = std::make_unique<CPolyline>("Grid edge");
            curve->AddPoint(first);
            curve->AddPoint(second);
            document.AddObject(std::move(curve));
        };
        // 40 edges, 13 adjacent patches; the final boundary is beyond slot 32.
        for (int x = 0; x < 13; ++x) {
            for (int y = 0; y < 2; ++y)
                add_edge({x * 10.0, y * 10.0, 0}, {(x + 1) * 10.0, y * 10.0, 0});
        }
        for (int x = 0; x <= 13; ++x)
            add_edge({x * 10.0, 0, 0}, {x * 10.0, 10, 0});
    }
    std::vector<unsigned long> ids;
    for (const auto& object : document.GetObjects()) {
        const auto* polyline = dynamic_cast<const CPolyline*>(object.get());
        const auto* spline = dynamic_cast<const CBSpline*>(object.get());
        if ((polyline && !polyline->IsClosed() && polyline->GetPoints().size() >= 2)
            || (spline && !spline->IsClosed() && spline->GetPoints().size() >= 2)) {
            document.EnsureObjectId(*object);
            ids.push_back(object->m_id);
        }
    }
    require(ids.size() > 32, "Large Smart Hybrid fixture needs more than 32 curves");
    ToolRegistry tools;
    auto parameters = tools.Find("SurfaceSmartHybrid")->defaults;
    for (auto& parameter : parameters)
        if (parameter.id == "curve.count") parameter.value = static_cast<double>(ids.size());
    for (size_t i = 0; i < ids.size(); ++i) {
        const std::string id = "curve" + std::to_string(i + 1) + ".id";
        const auto found = std::find_if(parameters.begin(), parameters.end(),
            [&](const auto& parameter) { return parameter.id == id; });
        if (found != parameters.end()) found->value = static_cast<double>(ids[i]);
        else parameters.push_back({id, id, static_cast<double>(ids[i]), 0, 4294967295.0, 1});
    }
    const size_t before = document.GetObjects().size();
    const auto hybrid = tools.CreateParametricObject("SurfaceSmartHybrid", document, parameters);
    require(document.GetObjects().size() == before + 1, "Large Smart Hybrid failed to build");
    const unsigned long hybrid_id = document.GetObjects()[hybrid.object_index]->m_id;
    const auto face_count = [&](CAlfaDoc& doc) {
        const auto* solid = dynamic_cast<const CSolid*>(doc.FindObjectById(hybrid_id));
        require(solid && !solid->m_Shape.IsNull(), "Large Smart Hybrid lost its shape");
        size_t count = 0;
        for (TopExp_Explorer faces(solid->m_Shape, TopAbs_FACE); faces.More(); faces.Next()) ++count;
        return count;
    };
    if (source_path) RequireHybridOutwardNormals(*dynamic_cast<const CSolid*>(document.FindObjectById(hybrid_id)));
    if (source_path) {
        auto* body = dynamic_cast<CSolid*>(document.FindObjectById(hybrid_id));
        body->MeshQuadro = true;
        require(body->ReBuldMesh(4.0f), "Cannot build Hybrid quad mesh");
        RequireHybridOutwardNormals(*body);
    }
    const size_t faces = face_count(document);
    require(expected_faces == 0 || faces == expected_faces, "Smart Hybrid has missing or internal patches");
    require(faces > 0 && (source_path || faces == 13), "Large Smart Hybrid omitted patches");
    QTemporaryDir directory;
    const QString path = directory.filePath("large-hybrid.dom3d");
    require(serializer.Save(path, document, room, view, {}, error), "Cannot save large Smart Hybrid");
    CAlfaDoc loaded;
    require(serializer.Load(path, loaded, room, view, error), "Cannot reload large Smart Hybrid");
    const auto active = tools.ActiveObjectFromDocument(hybrid.object_index,
        *loaded.FindObjectById(hybrid_id), 0, &loaded);
    const std::string last_reference = "curve" + std::to_string(ids.size()) + ".id";
    require(std::any_of(active.parameters.begin(), active.parameters.end(),
        [&](const auto& parameter) {
            return parameter.id == last_reference
                && parameter.value == static_cast<double>(ids.back());
        }), "Saved replay truncated Smart Hybrid curve references");
    require(tools.ReplayProfileDependents(ids.back(), loaded), "Curve beyond 32 lost dependency");
    if (source_path) RequireHybridOutwardNormals(*dynamic_cast<const CSolid*>(loaded.FindObjectById(hybrid_id)));
    require(face_count(loaded) == faces, "Large Smart Hybrid lost patches on saved replay");
    std::cout << "Smart Hybrid curves=" << ids.size() << " patches=" << faces << " saved replay OK\n";
}

void TestLiveFilletValidation() {
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(20.0, 20.0, 20.0).Shape();
    TopExp_Explorer edge_explorer(box, TopAbs_EDGE);
    require(edge_explorer.More(), "Fillet validation fixture has no edge.");

    CAlfaDoc::LiveFilletBuildRequest request;
    request.source_shape = box;
    request.base_shape = box;
    request.edges.push_back(TopoDS::Edge(edge_explorer.Current()));

    TopoDS_Shape result;
    std::vector<int> generated_faces;
    require(CAlfaDoc::BuildLiveFilletShape(
                request, {2.0}, result, generated_faces)
                && !result.IsNull() && BRepCheck_Analyzer(result).IsValid(),
            "A safe constant fillet radius was rejected.");

    result.Nullify();
    generated_faces.clear();
    require(!CAlfaDoc::BuildLiveFilletShape(
                request, {30.0}, result, generated_faces),
            "An oversized fillet radius produced an accepted preview.");

    CAlfaDoc live_document;
    TopoDS_Shape live_box = box;
    auto live_solid = std::make_unique<CSolid>(live_box);
    require(live_solid->ReBuldMesh(),
            "The live fillet fixture could not initialize its solid.");
    live_document.AddObject(std::move(live_solid));
    auto* selected_solid = live_document.GetSelectedSolid();
    require(selected_solid != nullptr && selected_solid->GetNumSurfaces() > 0,
            "The live fillet fixture has no selectable surface.");
    selected_solid->SetSelectedEdge(0, 0);
    require(live_document.BeginLiveFilletSelectedEdges(false)
                && live_document.UpdateLiveFillet(2.0)
                && live_document.IsLiveFilletPreviewValid(),
            "A valid constant-radius live preview was rejected.");
    live_document.CancelLiveFillet();
}

void TestFrame2Fillet(const QString& path, const QString& output) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error), "Cannot load Frame-2");
    size_t index = 0;
    while (index < document.GetObjects().size()
           && document.GetObjects()[index]->GetParametricToolId() != "SurfaceSmartHybrid") ++index;
    require(index < document.GetObjects().size(), "Missing Frame-2 Hybrid");
    auto* original = dynamic_cast<CSolid*>(document.GetObjects()[index].get());
    require(original && original->m_Shape.ShapeType() == TopAbs_SHELL, "Missing legacy shell");
    const TopoDS_Shape original_shape = original->m_Shape;
    TopExp_Explorer legacy_edge(original_shape, TopAbs_EDGE);
    legacy_edge.Next();
    CAlfaDoc::LiveFilletBuildRequest request;
    request.base_shape = request.source_shape = original_shape;
    request.edges = {TopoDS::Edge(legacy_edge.Current())};
    TopoDS_Shape result;
    std::vector<int> faces;
    std::cout << "Checking legacy kernel failure is contained" << std::endl;
    const bool legacy_built = std::async(std::launch::async, [&] {
        return CAlfaDoc::BuildLiveFilletShape(request, {1.0}, result, faces);
    }).get();
    require(!legacy_built || (!result.IsNull() && BRepCheck_Analyzer(result).IsValid()),
            "Legacy Frame-2 returned an invalid successful preview");
    require(original->m_Shape.IsSame(original_shape) && BRepCheck_Analyzer(original_shape).IsValid(),
            "Failed preview modified the source shell");
    ToolRegistry tools;
    ActiveParametricObject active;
    active.tool_id = "SurfaceSmartHybrid";
    active.object_index = index;
    active.parameters = tools.Find(active.tool_id)->defaults;
    for (auto& parameter : active.parameters)
        for (const auto& saved : original->GetParametricParameters())
            if (parameter.id == saved.id) parameter.value = saved.value;
    tools.Rebuild(active, document);
    auto* rebuilt = dynamic_cast<CSolid*>(document.GetObjects()[index].get());
    require(rebuilt && rebuilt->m_Shape.ShapeType() == TopAbs_SOLID
                && BRepCheck_Analyzer(rebuilt->m_Shape).IsValid(),
            "Frame-2 shared curves did not sew into a solid");
    std::vector<TopoDS_Edge> edges;
    for (const auto& edge : rebuilt->GetAllTopoEdges())
        if (std::none_of(edges.begin(), edges.end(), [&](const auto& other) { return edge.IsSame(other); }))
            edges.push_back(edge);
    require(edges.size() == 12, "Frame-2 contains unsewn boundary edges");
    request.base_shape = request.source_shape = rebuilt->m_Shape;
    for (size_t i = 0; i < edges.size(); ++i) {
        std::cout << "Checking rebuilt edge " << i << std::endl;
        request.edges = {edges[i]};
        require(CAlfaDoc::BuildLiveFilletShape(request, {1.0}, result, faces)
                    && BRepCheck_Analyzer(result).IsValid() && !faces.empty(),
                "Rebuilt Frame-2 failed a 1 mm fillet");
    }
    if (!output.isEmpty())
        require(serializer.Save(output, document, room, view, {}, error), "Cannot save repaired Frame-2");
}

void TestFrame2RenderSeams(const QString& path, const QString& output) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, error;
    ProjectViewState view;
    require(serializer.Load(path, document, room, view, error), "Cannot load repaired Frame-2");
    for (size_t index = 0; index < document.GetObjects().size(); ++index) {
        auto* solid = dynamic_cast<CSolid*>(document.GetObjects()[index].get());
        if (!solid) continue;
        TopExp_Explorer edge(solid->m_Shape, TopAbs_EDGE);
        edge.Next();
        CAlfaDoc::LiveFilletBuildRequest request;
        request.base_shape = request.source_shape = solid->m_Shape;
        request.edges = {TopoDS::Edge(edge.Current())};
        TopoDS_Shape result;
        std::vector<int> faces;
        require(CAlfaDoc::BuildLiveFilletShape(request, {1.0}, result, faces), "Cannot build render-seam fillet");
        auto rendered = std::make_unique<CSolid>(result);
        rendered->m_id = solid->m_id;
        rendered->m_LayerID = solid->m_LayerID;
        rendered->SetMaterial(solid->GetMaterial());
        rendered->SetMaterialId(solid->GetMaterialId());
        rendered->SetColor(solid->GetColor());
        rendered->SetName("Frame-2 Fillet 1 mm");
        require(rendered->ReBuldMesh(), "Cannot build fillet render mesh");
        std::vector<const CMesh3D*> meshes;
        for (int f = 0; f < rendered->GetNumSurfaces(); ++f) {
            auto* surface = rendered->GetSurfaceFace(f);
            meshes.push_back(surface->pMesh3D);
            std::cout << "Render face=" << f << " meshType=" << surface->m_TypeMesh
                      << " cells=" << surface->pMesh3D->GetFaces().size() << std::endl;
        }
        auto welded = CMesh3D::CreateWelded(meshes);
        bool manifold = false;
        const size_t loops = ClosedMeshBoundaryLoopCount(*welded, manifold);
        std::cout << "Render seam loops=" << loops << " manifold=" << manifold << std::endl;
        std::map<std::array<float, 3>, size_t> positions;
        std::map<std::pair<size_t, size_t>, size_t> edge_uses;
        for (const auto* mesh : meshes) for (const auto& cell : mesh->GetFaces()) {
            if (cell.deleted || cell.corners.size() < 3) continue;
            for (size_t c = 0; c < cell.corners.size(); ++c) {
                const auto id = [&](size_t corner) {
                    const auto p = mesh->GetVertices()[cell.corners[corner].v];
                    return positions.emplace(std::array<float, 3>{p.x, p.y, p.z}, positions.size()).first->second;
                };
                const size_t a = id(c), b = id((c + 1) % cell.corners.size());
                if (a != b) ++edge_uses[std::minmax(a, b)];
            }
        }
        size_t unmatched = 0;
        for (const auto& edge_use : edge_uses) if (edge_use.second != 2) ++unmatched;
        std::cout << "Exact unmatched render edges=" << unmatched << std::endl;
        document.GetObjects()[index] = std::move(rendered);
        if (!output.isEmpty()) require(serializer.Save(output, document, room, view, {}, error), "Cannot save render-seam fixture");
        require(loops == 0 && manifold, "Fillet render mesh has open seams");
        require(unmatched == 0, "Fillet seam only closes with tolerance-based welding");
        return;
    }
    require(false, "Missing repaired Frame-2 solid");
}

void DiagnoseProjectFillets(const QString& path, double radius) {
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room;
    ProjectViewState view;
    QString error;
    require(serializer.Load(path, document, room, view, error),
            error.toLocal8Bit().constData());
    if (std::getenv("DOM3D_FILLET_RECREATE")) {
        ToolRegistry tools;
        for (const auto& object : document.GetObjects()) {
            if (object->GetParametricToolId() != "SurfaceSmartHybrid") continue;
            auto parameters = tools.Find("SurfaceSmartHybrid")->defaults;
            for (auto& parameter : parameters)
                for (const auto& saved : object->GetParametricParameters())
                    if (parameter.id == saved.id) parameter.value = saved.value;
            const auto created = tools.CreateParametricObject("SurfaceSmartHybrid", document, parameters);
            auto replacement = std::move(document.GetObjects()[created.object_index]);
            document.GetObjects().clear();
            document.GetObjects().push_back(std::move(replacement));
            break;
        }
    }

    for (size_t object_index = 0;
         object_index < document.GetObjects().size(); ++object_index) {
        const auto* solid = dynamic_cast<const CSolid*>(
            document.GetObjects()[object_index].get());
        if (!solid || solid->m_Shape.IsNull()) continue;
        int edge_index = 0;
        std::cout << "Fillet input '" << solid->GetName() << "' valid="
                  << BRepCheck_Analyzer(solid->m_Shape).IsValid()
                  << " type=" << solid->m_Shape.ShapeType() << std::endl;
        int built = 0;
        int valid = 0;
        int guarded = 0;
        double maximum_tolerance = 0.0;
        for (TopExp_Explorer explorer(solid->m_Shape, TopAbs_EDGE);
             explorer.More(); explorer.Next(), ++edge_index) {
            const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
            if (const char* selected = std::getenv("DOM3D_FILLET_EDGE"))
                if (edge_index != std::atoi(selected)) continue;
            std::cout << "Fillet edge=" << edge_index << " radius=" << radius << std::endl;
            maximum_tolerance = std::max(
                maximum_tolerance, BRep_Tool::Tolerance(edge));
            try {
                BRepFilletAPI_MakeFillet fillet(solid->m_Shape);
                fillet.Add(radius, edge);
                fillet.Build();
                if (fillet.IsDone() && !fillet.Shape().IsNull()) {
                    ++built;
                    if (BRepCheck_Analyzer(fillet.Shape()).IsValid()) ++valid;
                }
                CAlfaDoc::LiveFilletBuildRequest request;
                request.source_shape = solid->m_Shape;
                request.base_shape = solid->m_Shape;
                request.edges = {edge};
                TopoDS_Shape guarded_result;
                std::vector<int> guarded_faces;
                if (CAlfaDoc::BuildLiveFilletShape(
                        request, {radius}, guarded_result, guarded_faces)) {
                    ++guarded;
                }
            } catch (const Standard_Failure&) {
            }
        }
        std::cout << "Object " << object_index << " '" << solid->GetName()
                  << "': shapeValid="
                  << BRepCheck_Analyzer(solid->m_Shape).IsValid()
                  << ", edges=" << edge_index << ", filletBuilt=" << built
                  << ", filletValid=" << valid
                  << ", guardedFillet=" << guarded
                  << ", maxEdgeTolerance=" << maximum_tolerance << "\n";
    }
}

void TestObjSharpEdges();
void TestLowPolySharpEdges();
void TestMeshSubdivision();
void RenderMeshSubdivisionPreview(const char* directory);
void TestToolsMesh3D(const char* boundary_path, const char* diagnostic_directory);
void TestDivideFace(const char* output_directory);
void TestDemosMesh(const char* path);
int TestRepeatCommand(int argc, char** argv);
int TestKitchenLayout(int argc, char** argv);
int TestKitchenEditor(int argc, char** argv);
int TestScenePersistence(int argc, char** argv);
int TestSolidCenterlines(int argc, char** argv);
int TestDraftingOutlines(int argc, char** argv);
int TestDraftingDimensions(int argc, char** argv);
int TestSheetBend(int argc, char** argv);
int TestProjectOpenDialog(int argc, char** argv);
int TestBooleanTool(int argc, char** argv);
int TestSolidPrimitiveTool(int argc, char** argv);
int TestTile(int argc, char** argv);
int TestSurfaceDisplayNet();
int TestCurveEndpointLinks(int argc, char** argv);
int TestTextToCurves(int argc, char** argv);
int TestMultiSketch(int argc, char** argv);
int TestProceduralMaterial(int argc, char** argv);
int TestFacePrimitiveCut(int argc, char** argv);

void TestVisibleStepExportAndSelectAll() {
    QTemporaryDir directory;
    require(directory.isValid(), "Temporary STEP export directory was not created.");
    const auto make_box = [](gp_Pnt origin, double x, double y, double z) {
        TopoDS_Shape shape = BRepPrimAPI_MakeBox(origin, x, y, z).Shape();
        return std::make_unique<CSolid>(shape);
    };

    CAlfaDoc export_document;
    const int default_layer = export_document.GetWorkLayerID();
    auto visible = make_box(gp_Pnt(0.0, 0.0, 0.0), 10.0, 10.0, 10.0);
    export_document.AddObject(std::move(visible), false);

    auto hidden = make_box(gp_Pnt(30.0, 0.0, 0.0), 20.0, 10.0, 10.0);
    hidden->SetVisible(false);
    export_document.AddObject(std::move(hidden), false);

    CLayer* hidden_layer = export_document.AddLayer("Hidden STEP layer");
    require(hidden_layer != nullptr, "Hidden STEP layer was not created.");
    hidden_layer->Visible = false;
    require(export_document.SetWorkLayer(hidden_layer->ID()),
            "Hidden STEP layer could not be activated.");
    auto layer_hidden = make_box(
        gp_Pnt(60.0, 0.0, 0.0), 30.0, 10.0, 10.0);
    export_document.AddObject(std::move(layer_hidden), false);
    require(export_document.SetWorkLayer(default_layer),
            "Default layer could not be restored.");

    const QString step_path = directory.filePath("visible-only.step");
    StepIO step_io;
    std::string error;
    require(step_io.Export(step_path.toStdString(), export_document, error),
            error.c_str());
    std::vector<std::unique_ptr<CSolid>> imported;
    require(step_io.Import(step_path.toStdString(), imported, error), error.c_str());
    double exported_volume = 0.0;
    for (const auto& solid : imported) {
        GProp_GProps properties;
        BRepGProp::VolumeProperties(solid->m_Shape, properties);
        exported_volume += properties.Mass();
    }
    require(std::abs(exported_volume - 1000.0) < 1.0e-4,
            "STEP export included an object hidden directly or by its layer.");

    CAlfaDoc selection_document;
    auto selected_visible = make_box(
        gp_Pnt(0.0, 0.0, 0.0), 10.0, 10.0, 10.0);
    selection_document.AddObject(std::move(selected_visible), false);
    const unsigned long visible_id = selection_document.GetObjects().back()->m_id;
    auto protected_hidden = make_box(
        gp_Pnt(20.0, 0.0, 0.0), 10.0, 10.0, 10.0);
    protected_hidden->SetVisible(false);
    selection_document.AddObject(std::move(protected_hidden), false);
    const unsigned long hidden_id = selection_document.GetObjects().back()->m_id;
    auto mixed_group = std::make_unique<CGroup>(
        "Visible and hidden", std::vector<unsigned long>{visible_id, hidden_id});
    selection_document.AddObject(std::move(mixed_group), false);
    const unsigned long group_id = selection_document.GetObjects().back()->m_id;

    require(selection_document.SelectAllVisibleObjects() > 0,
            "Select All Visible selected nothing.");
    require(selection_document.IsObjectSelected(
                selection_document.FindObjectIndexById(visible_id))
            && !selection_document.IsObjectSelected(
                selection_document.FindObjectIndexById(hidden_id))
            && !selection_document.IsObjectSelected(
                selection_document.FindObjectIndexById(group_id)),
            "Select All Visible selected a hidden object through its group.");
    require(selection_document.DeleteSelectedObject(),
            "Visible selection could not be deleted.");
    require(selection_document.FindObjectById(visible_id) == nullptr
                && selection_document.FindObjectById(hidden_id) != nullptr,
            "Deleting all visible objects also deleted a hidden group member.");
    const auto* surviving_group = dynamic_cast<const CGroup*>(
        selection_document.FindObjectById(group_id));
    require(surviving_group && surviving_group->GetElementIds()
                == std::vector<unsigned long>{hidden_id},
            "The surviving group retained references to deleted visible members.");
}

void TestCushionStageOneMesh() {
    CAlfaDoc document;
    ToolRegistry registry;
    const ToolDefinition* definition = registry.Find("MeshCushion");
    require(definition != nullptr, "Cushion mesh tool is not registered.");
    ActiveParametricObject active = registry.CreateParametricObject(
        "MeshCushion", document, definition->defaults);
    require(!active.tool_id.empty()
                && active.object_index < document.GetObjects().size(),
            "Cushion mesh tool did not create an object.");
    const auto* mesh = dynamic_cast<const CMesh3D*>(
        document.GetObjects()[active.object_index].get());
    require(mesh && !mesh->GetVertices().empty()
                && mesh->GetFaces().size() == 96,
            "Cushion stage-one mesh has an unexpected topology.");
    require(std::all_of(mesh->GetFaces().begin(), mesh->GetFaces().end(),
                [](const CMesh3D::Face& face) {
                    return !face.deleted && face.corners.size() == 4;
                }),
            "Cushion stage-one mesh is not all quads.");
    std::map<std::pair<size_t, size_t>, int> edge_owners;
    for (const CMesh3D::Face& face : mesh->GetFaces()) {
        for (size_t corner = 0; corner < face.corners.size(); ++corner) {
            const size_t first = face.corners[corner].v;
            const size_t second = face.corners[
                (corner + 1) % face.corners.size()].v;
            ++edge_owners[std::minmax(first, second)];
        }
    }
    require(std::all_of(edge_owners.begin(), edge_owners.end(),
                [](const auto& edge) { return edge.second == 2; }),
            "Cushion stage-one mesh is not closed and manifold.");
    Vec3 minimum{};
    Vec3 maximum{};
    require(mesh->GetBounds(minimum, maximum)
                && std::abs((maximum.x - minimum.x) - 73.0f) < 1.0e-3f
                && std::abs((maximum.y - minimum.y) - 56.78f) < 1.0e-3f
                && maximum.z > 20.61f && std::abs(minimum.z) < 1.0e-6f,
            "Cushion parameters did not produce the requested bounds and crown.");

    const float original_crown = maximum.z;
    for (ToolParameter& parameter : active.parameters) {
        if (parameter.id == "sphere_radius")
            parameter.value = 120.0;
    }
    registry.Rebuild(active, document);
    mesh = dynamic_cast<const CMesh3D*>(
        document.GetObjects()[active.object_index].get());
    require(mesh && mesh->GetBounds(minimum, maximum)
                && maximum.z < original_crown,
            "Cushion sphere radius did not rebuild the crown.");

    for (ToolParameter& parameter : active.parameters) {
        if (parameter.id == "hybrid") parameter.value = 1.0;
    }
    registry.Rebuild(active, document);
    const auto* hybrid = dynamic_cast<const CSolid*>(
        document.GetObjects()[active.object_index].get());
    require(hybrid && !dynamic_cast<const CMesh3D*>(hybrid)
                && hybrid->GetNumSurfaces() == 96,
            "Cushion Hybrid did not create one Catmull-Clark surface per control face.");
    require(!hybrid->m_Shape.IsNull()
                && BRepCheck_Analyzer(hybrid->m_Shape).IsValid(),
            "Cushion Hybrid produced an invalid sewn shape.");
    require(hybrid->m_Shape.ShapeType() == TopAbs_SOLID,
            "Cushion Hybrid patches were not sewn into a solid.");
}

int TestNativeColors();
int TestImageRelief();
int TestBallCylinder(const char* path);
int TestSphereUnion(const char* path);
int TestBoolAndFill(const char* path);
int TestBoxDraftsMesh(const char* path);
int TestDraftFace(const char* path);

int TestPlasticityImport(const char* path);

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--test-plasticity-import") return TestPlasticityImport(argv[2]);
    if (argc == 3 && std::string(argv[1]) == "--test-box-drafts-mesh") return TestBoxDraftsMesh(argv[2]);
    if (argc == 3 && std::string(argv[1]) == "--test-draft-face") return TestDraftFace(argv[2]);
    if (argc == 3 && std::string(argv[1]) == "--test-bool-and-fill") return TestBoolAndFill(argv[2]);
    if (argc == 3 && std::string(argv[1]) == "--test-ball-cylinder") return TestBallCylinder(argv[2]);
    if (argc == 3 && std::string(argv[1]) == "--test-sphere-union") return TestSphereUnion(argv[2]);
    if (argc == 2 && std::string(argv[1]) == "--test-image-relief") return TestImageRelief();
    if (argc >= 2 && std::string(argv[1]) == "--test-solid-centerlines") return TestSolidCenterlines(argc,argv);
    if(argc==4 && std::string(argv[1])=="--import-obj-project") {
        QApplication application(argc,argv); ObjIO io; std::string error;
        std::vector<std::unique_ptr<CMesh3D>> meshes;
        require(io.Import(argv[2],meshes,error),error.c_str());CAlfaDoc document;
        std::map<std::string, Material> imported_materials;
        for(auto& mesh:meshes) {
            auto material=mesh->GetMaterial();
            auto found=imported_materials.find(material.name);
            if(found==imported_materials.end()) {
                material.id=0;
                auto saved=document.UpsertMaterial(material);
                found=imported_materials.emplace(material.name,saved).first;
            }
            mesh->SetMaterial(found->second);document.AddMesh(std::move(mesh));
        }
        Dom3DProjectSerializer serializer;QString save_error;ProjectViewState view;
        require(serializer.Save(QString::fromLocal8Bit(argv[3]),document,"Mesh",view,{},save_error),save_error.toStdString().c_str());
        return EXIT_SUCCESS;
    }
    if(argc==5 && std::string(argv[1])=="--check-3ds-normals") {
        QApplication application(argc,argv); ThreeDSIO io; std::string error;
        std::vector<std::unique_ptr<CMesh3D>> meshes;
        require(io.Import(argv[2],meshes,error),error.c_str());
        size_t smooth=0,flat=0;
        CAlfaDoc document;
        for(auto& mesh:meshes) {
            for(const auto& face:mesh->GetFaces()) {
                const auto& a=mesh->GetVertices()[face.corners[0].v];
                const auto& b=mesh->GetVertices()[face.corners[1].v];
                const auto& c=mesh->GetVertices()[face.corners[2].v];
                const Vec3 n=normalize(cross(b-a,c-a));
                for(const auto& corner:face.corners) {
                    if(dot(n,mesh->GetNormals()[corner.n])<0.99999f)++smooth;else ++flat;
                }
            }
            document.AddMesh(std::move(mesh));
        }
        std::cout<<"Smoothed corners: "<<smooth<<", flat corners: "<<flat<<std::endl;
        const std::string expected=argv[3];
        if(expected=="flat")require(smooth==0,"Sharp crease was smoothed");
        if(expected=="smooth")require(smooth>0,"Gentle crease remains faceted");
        Dom3DProjectSerializer serializer;QString save_error;ProjectViewState view;
        require(serializer.Save(QString::fromLocal8Bit(argv[4]),document,"Mesh",view,{},save_error),save_error.toStdString().c_str());
        return EXIT_SUCCESS;
    }
    if(argc==4 && std::string(argv[1])=="--render-iges") {
        QApplication application(argc,argv);IgesIO io;std::string error;
        std::vector<std::unique_ptr<CAlfaObject>> objects;
        require(io.Import(argv[2],objects,error),error.c_str());CAlfaDoc document;
        for(auto& object:objects)document.AddObject(std::move(object),false);
        document.ClearSelection();OpenGLViewport viewport;viewport.resize(1000,800);viewport.move(-20000,-20000);
        viewport.SetDocument(&document);viewport.SetOrthographicProjection(true);
        viewport.SetFloorGridVisible(false);viewport.SetCoordinateAxesVisible(false);viewport.FitToDocument();
        CSolid::SetDisplayMode(SolidDisplayMode::SurfacesAndEdges);CMesh3D::SetDisplayMode(MeshDisplayMode::SurfaceMaterial);
        viewport.show();application.processEvents();
        require(viewport.CaptureSceneImage({1000,800}).save(QString::fromLocal8Bit(argv[3])),"IGES render failed");
        viewport.SetDocument(nullptr);return EXIT_SUCCESS;
    }
    if(argc>=2 && std::string(argv[1])=="--test-procedural-material")return TestProceduralMaterial(argc,argv);
    if(argc>=2 && std::string(argv[1])=="--test-tile")return TestTile(argc,argv);
    if(argc==2 && std::string(argv[1])=="--test-high-dpi-viewport") { extern int TestHighDpiViewport(int,char**); return TestHighDpiViewport(argc,argv); }
    if(argc==2 && std::string(argv[1])=="--test-surface-display-net")return TestSurfaceDisplayNet();
    if(argc>=2 && std::string(argv[1])=="--test-curve-endpoint-links")return TestCurveEndpointLinks(argc,argv);
    if(argc>=2 && std::string(argv[1])=="--test-text-to-curves")return TestTextToCurves(argc,argv);
    if(argc>=2 && std::string(argv[1])=="--test-multi-sketch")return TestMultiSketch(argc,argv);
    if (argc >= 2 && std::string(argv[1]) == "--test-project-open-dialog")
        return TestProjectOpenDialog(argc, argv);
    if (argc >= 2 && std::string(argv[1]) == "--test-face-primitive-cut")
        return TestFacePrimitiveCut(argc, argv);
    if (argc >= 2 && std::string(argv[1]) == "--test-solid-primitive-tool")
        return TestSolidPrimitiveTool(argc, argv);
    if (argc >= 2 && std::string(argv[1]) == "--test-boolean-tool")
        return TestBooleanTool(argc, argv);
    // Manual visual regression through the real viewport, including batched
    // shading and mesh edges. Keep it opt-in because it needs a native GL driver.
    if ((argc >= 4 && argc <= 6) && (std::string(argv[1]) == "--render-project-quadro" || std::string(argv[1]) == "--render-project-native")) {
        QSurfaceFormat format;
        format.setRenderableType(QSurfaceFormat::OpenGL);
        format.setVersion(2, 1);
        format.setProfile(QSurfaceFormat::CompatibilityProfile);
        format.setDepthBufferSize(24);
        format.setSamples(8);
        QSurfaceFormat::setDefaultFormat(format);
        QApplication application(argc, argv);
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState view;
        require(serializer.Load(QString::fromLocal8Bit(argv[2]), document, room, view, error),
                error.toLocal8Bit().constData());
        for (const auto& object : document.GetObjects())
            if (std::string(argv[1]) != "--render-project-native"
                && !dynamic_cast<CSolid*>(object.get())) object->SetVisible(false);
        OpenGLViewport viewport;
        viewport.resize(900, 1000);
        viewport.move(-20000, -20000);
        viewport.SetDocument(&document);
        viewport.SetCamera(view.camera);
        viewport.SetOrthographicProjection(true);
        viewport.SetFloorGridVisible(false);
        viewport.SetCoordinateAxesVisible(false);
        viewport.FitToDocument();
        viewport.show();
        application.processEvents();
        CSolid::SetDisplayMode(std::string(argv[1]) == "--render-project-native" ? SolidDisplayMode::SurfacesAndEdges : SolidDisplayMode::SurfacesAndRaisedMesh);
        CMesh3D::SetDisplayMode(std::string(argv[1]) == "--render-project-native" ? MeshDisplayMode::SurfaceMaterial : MeshDisplayMode::SurfaceColored);
        const auto densities = argc >= 5 ? std::vector<float>{std::stof(argv[4])}
            : std::vector<float>{0.35f, 0.40f, 0.45f, 0.50f, 0.80f};
        for (float density : densities) {
            for (const auto& object : document.GetObjects()) {
                if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
                    if(std::string(argv[1]) == "--render-project-native")continue;
                    solid->MeshQuadro = argc < 6 || std::string(argv[5]) != "--hybrid";
                    solid->MeshQuadroTrimByPline = argc >= 6 && std::string(argv[5]) == "--trim-by-pline";
                    solid->MeshQuadroHoleSLX = argc < 6 || std::string(argv[5]) != "--no-slx";
                    solid->MeshQuadroHoleDivideFace = argc >= 6 && std::string(argv[5]) == "--divide-face";
                    if (solid->MeshQuadroHoleDivideFace) solid->MeshQuadroHoleSLX = false;
                    require(solid->ReBuldMesh(1.0f / density), "Rebuild failed");
                }
            }
            const Camera original = viewport.GetCamera();
            for (int angle = 0; angle < 4; ++angle) {
                Camera camera = original;
                camera.orientation = quaternion_from_axis_angle({0, 0, 1}, angle * kPi / 2)
                    * original.orientation;
                viewport.SetCamera(camera);
                const auto frame = viewport.CaptureSceneImage(QSize(900, 1000));
                require(!frame.isNull() && frame.save(QString::fromLocal8Bit(argv[3])
                        + QString::number(density, 'f', 2) + "-" + QString::number(angle)
                        + ".png"), "Could not save viewport frame");
            }
            viewport.SetCamera(original);
        }
        return 0;
    }
    if (argc >= 2 && std::string(argv[1]) == "--test-repeat-command") {
        return TestRepeatCommand(argc, argv);
    }
    if (argc >= 3 && (std::string(argv[1]) == "--test-sheet-bend-detail"
                      || std::string(argv[1]) == "--test-sheet-bend-third"
                      || std::string(argv[1]) == "--test-sheet-bend-fourth")) {
        return TestSheetBend(argc, argv);
    }
    if (argc >= 2 && std::string(argv[1]) == "--test-drafting-dimensions") return TestDraftingDimensions(argc, argv);
    if (argc >= 3 && std::string(argv[1]) == "--test-drafting-outlines") {
        return TestDraftingOutlines(argc, argv);
    }
    if (argc >= 2 && std::string(argv[1]) == "--test-scene-persistence") {
        // UI regressions own their QApplication, just like RepeatCommand.
        return TestScenePersistence(argc, argv);
    }
    if (argc >= 2 && std::string(argv[1]) == "--test-kitchen-editor") return TestKitchenEditor(argc, argv);
    if (argc >= 2 && std::string(argv[1]) == "--test-kitchen-layout") return TestKitchenLayout(argc, argv);
    if (argc == 3 && std::string(argv[1]) == "--render-mesh-subdivision") {
        QSurfaceFormat format; format.setVersion(2, 1);
        format.setProfile(QSurfaceFormat::CompatibilityProfile); format.setDepthBufferSize(24); format.setSamples(4);
        QSurfaceFormat::setDefaultFormat(format);
        QApplication application(argc, argv);
        RenderMeshSubdivisionPreview(argv[2]); return 0;
    }
    QCoreApplication application(argc, argv);
    if (argc == 4 && std::string(argv[1]) == "--test-rebuild-polyhedron") {
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState view;
        require(serializer.Load(QString::fromLocal8Bit(argv[2]), document, room, view, error),
                error.toLocal8Bit().constData());
        ToolRegistry registry;
        size_t tested = 0;
        for (size_t i = 0; i < document.GetObjects().size(); ++i) {
            auto* solid = dynamic_cast<CSolid*>(document.GetObjects()[i].get());
            if (!solid || !solid->GetOperation(0)
                || solid->GetOperation(0)->ToolId != "SolidPolyhedronTool") continue;
            ++tested;
            const int operations = solid->GetNumOperations();
            require(operations == 2 && solid->GetOperation(1)->ToolId == "fillet_edge",
                    "Expected the Polyhedron fixture with its ledge fillet.");
            GProp_GProps before_volume;
            BRepGProp::VolumeProperties(solid->m_Shape, before_volume);
            std::cout << "Polyhedron before faces=" << solid->GetNumSurfaces() << '\n';
            // Fixture migration: the twelve edges of the horizontal ledge
            // belonged to F6 in the old triangulated base, and F4 in the
            // rebuilt base. Preserve the selected ledge, not the stale index.
            for (auto& p : solid->GetOperation(1)->Parameters)
                if (p.id.rfind("edge.", 0) == 0 && p.id.find(".surface") != std::string::npos)
                    p.value = 4;
            require(registry.ReplayOperations(i, document), "Polyhedron operation replay failed.");
            solid = dynamic_cast<CSolid*>(document.GetObjects()[i].get());
            require(solid && solid->GetNumOperations() == operations,
                    "Polyhedron replay lost operations.");
            require(BRepCheck_Analyzer(solid->m_Shape).IsValid(), "Rebuilt polyhedron is invalid.");
            require(solid->GetNumSurfaces() == 44, "Polyhedron retained split side faces.");
            GProp_GProps after_volume;
            BRepGProp::VolumeProperties(solid->m_Shape, after_volume);
            require(std::fabs(after_volume.Mass() - before_volume.Mass())
                        < std::fabs(before_volume.Mass()) * 1.0e-5,
                    "Polyhedron repair changed the volume or moved the fillet.");
            std::cout << "Polyhedron after faces=" << solid->GetNumSurfaces() << '\n';
            for (int f = 0; f < solid->GetNumSurfaces(); ++f) {
                const auto* surface = solid->GetSurfaceFace(f);
                int edges = 0;
                for (TopExp_Explorer e(surface->m_Face, TopAbs_EDGE); e.More(); e.Next()) ++edges;
                if (edges == 3) {
                    GProp_GProps props;
                    BRepGProp::SurfaceProperties(surface->m_Face, props);
                    const auto p = props.CentreOfMass();
                    // The profile ends on the axis below the bottom rim;
                    // those six genuine apex facets are not split side panels.
                    require(p.Z() < -27.0, "Polyhedron has a triangular side panel.");
                    std::cout << "triangle=" << f << " center=" << p.X() << ',' << p.Y() << ',' << p.Z() << '\n';
                }
            }
        }
        require(tested > 0, "Missing polyhedron fixture.");
        require(serializer.Save(QString::fromLocal8Bit(argv[3]), document, room, view, {}, error),
                error.toLocal8Bit().constData());
        return 0;
    }
    if (argc >= 2 && std::string(argv[1]) == "--test-mixed-low-poly") {
        TopoDS_Shape box_shape = BRepPrimAPI_MakeBox(10, 12, 14).Shape();
        CSolid box(box_shape);
        box.MeshQuadro = true;
        require(box.ReBuldMesh(2.0f), "Cannot prepare mixed mesh test");
        auto intact = lowpoly::CompleteWithTriangles(box, 2.0f);
        require(intact.complete() && intact.retained == 6 && intact.triangulated.empty(),
                "Successful Quadro should not request triangle fallback");
        const auto original_vertices = box.GetSurfaceFace(1)->pMesh3D->GetVertices();
        const auto original_faces = box.GetSurfaceFace(1)->pMesh3D->GetFaces();
        // Simulate a failed quadrangulator that left provisional geometry.
        box.GetSurfaceFace(0)->IsInitMesh = false;
        auto mixed = lowpoly::CompleteWithTriangles(box, 2.0f);
        require(mixed.complete() && mixed.retained == 5
                && mixed.triangulated == std::vector<int>{0}, "Incorrect fallback scope");
        for (const auto& face : box.GetSurfaceFace(0)->pMesh3D->GetFaces())
            require(face.deleted || face.corners.size() == 3, "Fallback is not triangular");
        const auto& kept = *box.GetSurfaceFace(1)->pMesh3D;
        require(kept.GetVertices().size() == original_vertices.size()
                && kept.GetFaces().size() == original_faces.size(), "Successful mesh changed");
        for (size_t i = 0; i < original_vertices.size(); ++i) {
            const auto a = original_vertices[i], b = kept.GetVertices()[i];
            require(a.x == b.x && a.y == b.y && a.z == b.z, "Successful nodes changed");
        }
        box.GetSurfaceFace(0)->IsInitMesh = false;
        box.GetSurfaceFace(0)->m_Face.Nullify();
        auto missing = lowpoly::CompleteWithTriangles(box, 2.0f);
        require(!missing.complete() && missing.missing == std::vector<int>{0},
                "Unmeshed face must block export");
        if (argc == 3) {
            CAlfaDoc document;
            Dom3DProjectSerializer serializer;
            QString room,error; ProjectViewState view;
            require(serializer.Load(QString::fromLocal8Bit(argv[2]),document,room,view,error),
                    "Cannot load mixed mesh fixture");
            int tested = 0;
            for (const auto& object : document.GetObjects()) {
                auto* solid = dynamic_cast<CSolid*>(object.get());
                if (!solid || solid->GetName() != "Imported STEP 4") continue;
                solid->MeshQuadro = true;
                solid->MeshQuadroHoleSLX = true;
                solid->ReBuldMesh(5.0f);
                const auto result = lowpoly::CompleteWithTriangles(*solid, 5.0f);
                std::cout << "Mixed Low Poly: " << result.retained << " retained, "
                    << result.triangulated.size() << " triangles, " << result.missing.size()
                    << " missing" << std::endl;
                require(result.complete(), "Hairdryer mixed mesh is incomplete");
                ++tested;
            }
            require(tested > 0, "Hairdryer body not found");
        }
        return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "--test-imported-endcaps-window") {
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState view;
        require(serializer.Load(QString::fromLocal8Bit(argv[2]), document, room, view, error),
                error.toLocal8Bit().constData());
        const bool frame = std::string(argv[3]) == "frame";
        CSolid* solid = nullptr;
        for (const auto& object : document.GetObjects())
            if (auto* candidate = dynamic_cast<CSolid*>(object.get())) solid = candidate;
        require(solid && solid->GetNumSurfaces() == (frame ? 10 : 8), "Unexpected regression fixture");
        solid->MeshQuadro = true;
        for (bool slx : {true, false}) for (float density : {.25f, .50f, 1.f, .50f}) {
            solid->MeshQuadroHoleSLX = slx;
            require(solid->ReBuldMesh(1.f / density), "Imported endcaps/window rebuild failed");
            std::vector<const CMesh3D*> meshes;
            std::vector<double> cylinder_steps;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                const auto* surface = solid->GetSurfaceFace(i);
                require(surface && surface->pMesh3D, "Missing regression surface");
                const auto& mesh = *surface->pMesh3D;
                bool manifold = false;
                ClosedMeshBoundaryLoopCount(mesh, manifold);
                require(manifold && ActiveFaceEdgeComponentCount(mesh) == 1,
                        "Surface is disconnected or nonmanifold");
                std::vector<double> lengths;
                for (const auto& cell : mesh.GetFaces()) {
                    if (cell.deleted) continue;
                    require(cell.corners.size() == 4 || (!frame && cell.corners.size() == 3),
                            "End cap lost its quad fill or window has an unsupported cell");
                    for (size_t k = 0; k < cell.corners.size(); ++k) {
                        const auto a = cell.corners[k].v, b = cell.corners[(k+1)%cell.corners.size()].v;
                        require(a < mesh.GetVertices().size() && b < mesh.GetVertices().size(), "Invalid mesh index");
                        const auto d = mesh.GetVertices()[a] - mesh.GetVertices()[b];
                        const double length = std::sqrt(double(dot(d,d)));
                        require(std::isfinite(length) && length > 1.e-6, "Collapsed or nonfinite edge");
                        lengths.push_back(length);
                    }
                }
                require(!lengths.empty(), "Empty surface");
                if (!frame && BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType() == GeomAbs_Cylinder) {
                    std::sort(lengths.begin(), lengths.end());
                    const double step = lengths[lengths.size()/2];
                    require(lengths.back() < step*5, "Cylinder cell jumps across the periodic seam");
                    cylinder_steps.push_back(step);
                }
                meshes.push_back(&mesh);
            }
            if (!frame) {
                require(cylinder_steps.size() == 2, "Missing cylinders");
                require(std::max(cylinder_steps[0],cylinder_steps[1]) < 1.8*std::min(cylinder_steps[0],cylinder_steps[1]),
                        "Cylinder physical mesh steps diverged");
            }
            auto welded = CMesh3D::CreateWelded(meshes);
            bool manifold = false;
            require(welded && ClosedMeshBoundaryLoopCount(*welded,manifold) == 0 && manifold,
                    "Imported endcaps/window retained an open seam");
            std::cout << "endcaps/window density=" << density << " slx=" << slx << std::endl;
        }
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-frame-slx-boundary") {
        std::vector<Vec3> vertices;
        std::vector<CMesh3D::Face> faces;
        for (int y=0; y<=8; ++y) for (int x=0; x<=8; ++x)
            vertices.push_back({float(x*10), float(y*10), 0});
        for (size_t y=0; y<8; ++y) for (size_t x=0; x<8; ++x) {
            CMesh3D::Face f;
            const size_t i=y*9+x;
            f.corners={{i,0,0},{i+1,0,0},{i+10,0,0},{i+9,0,0}};
            faces.push_back(f);
        }
        CMesh3D mesh;
        require(mesh.SetGeometry(vertices, faces), "Cannot make frame background");
        CPolyline line;
        for (int x=5; x<65; x+=10) line.AddPoint(CPoint3d(x,15,0));
        for (int y=15; y<45; y+=10) line.AddPoint(CPoint3d(65,y,0));
        for (int x=65; x>5; x-=10) line.AddPoint(CPoint3d(x,45,0));
        for (int y=45; y>15; y-=10) line.AddPoint(CPoint3d(5,y,0));
        line.SetClosed(true);
        require(mesh.TrimByPline(&line,CPoint3d(75,75,0),true), "Frame SLX cut failed");
        size_t moved=0;
        for (size_t i=0; i<vertices.size(); ++i) {
            const auto p=vertices[i], q=mesh.GetVertices()[i];
            const bool changed=dot(q-p,q-p)>1e-8;
            if (p.x==0 || p.x==80 || p.y==0 || p.y==80)
                require(!changed,"Frame snapping moved the existing CAD boundary");
            else if (changed) ++moved;
        }
        require(moved>0,"Frame cutter bypassed vertex snapping");
        bool manifold=false;
        require(ClosedMeshBoundaryLoopCount(mesh,manifold)==2 && manifold
                    && ActiveFaceEdgeComponentCount(mesh)==1,
                "Frame cut broke the background topology");
        double area=0;
        for (const auto& f:mesh.GetFaces()) {
            if(f.deleted || f.corners.size()<3) continue;
            const auto& v=mesh.GetVertices();
            double twice=0;
            for(size_t i=0;i<f.corners.size();++i) {
                const auto a=v[f.corners[i].v], b=v[f.corners[(i+1)%f.corners.size()].v];
                twice+=double(a.x)*b.y-double(a.y)*b.x;
            }
            area+=std::fabs(twice)*0.5;
        }
        require(std::fabs(area-4600)<0.01,"Frame cut changed the retained area");
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-square-filleted-quadro") {
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState view;
        require(serializer.Load(QString::fromLocal8Bit(argv[2]), document, room, view, error),
                error.toLocal8Bit().constData());
        CSolid* solid = nullptr;
        for (const auto& object : document.GetObjects())
            if (object->GetName() == "Beam") solid = dynamic_cast<CSolid*>(object.get());
        require(solid, "Missing Square_Filleted beam");
        solid->MeshQuadro = true;
        for (bool slx : {false, true}) for (float density : {0.20f, 0.24f, 0.25f, 0.26f, 0.29f, 0.30f, 0.31f, 0.50f, 0.25f}) {
            std::cout << "Square_Filleted density=" << density << " slx=" << slx << std::endl;
            solid->MeshQuadroHoleSLX = slx;
            require(solid->ReBuldMesh(1.0f / density), "Filleted beam rebuild failed");
            std::vector<const CMesh3D*> meshes;
            size_t rims = 0;
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                const auto* surface = solid->GetSurfaceFace(i);
                require(surface && surface->pMesh3D, "Missing filleted beam mesh");
                meshes.push_back(surface->pMesh3D);
                size_t wires = 0;
                for (TopExp_Explorer wire(surface->m_Face, TopAbs_WIRE); wire.More(); wire.Next()) ++wires;
                if (wires != 2) continue;
                ++rims;
                bool manifold = false;
                require(ClosedMeshBoundaryLoopCount(*surface->pMesh3D, manifold) == 2 && manifold
                            && ActiveFaceEdgeComponentCount(*surface->pMesh3D) == 1,
                        "Filleted rim has missing patches or a filled opening");
                // The reported 0.25/0.30 cases must retain four inner and eight outer intervals.
                if (density == 0.25f || density == 0.30f) {
                    size_t inner = 0, outer = 0;
                    for (int edge = 0; edge < surface->GetPreparedPolylineCount(); ++edge) {
                        TopoDS_Edge shape;
                        require(surface->GetPreparedTopoEdge(edge, shape), "Missing prepared rim edge");
                        GProp_GProps properties;
                        BRepGProp::LinearProperties(shape, properties);
                        const double length = properties.Mass();
                        if (length > 14 && length < 16) {
                            ++inner;
                            require(surface->GetPreparedPolylinePointCount(edge) == 5, "Inner wall density jumped");
                        }
                        if (length > 23 && length < 24) {
                            ++outer;
                            require(surface->GetPreparedPolylinePointCount(edge) == 9, "Outer wall density changed");
                        }
                    }
                    require(inner == 4 && outer == 4, "Unexpected filleted rim geometry");
                }
            }
            require(rims == 2, "Missing top or bottom filleted rim");
            auto welded = CMesh3D::CreateWelded(meshes);
            bool manifold = false;
            require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                    "Filleted beam has open CAD seams");
        }
        return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "--test-grouped-hole-zones") {
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState view;
        require(serializer.Load(QString::fromLocal8Bit(argv[2]), document, room, view, error),
                error.toLocal8Bit().constData());
        const size_t expected_groups = std::stoul(argv[3]);
        size_t checked = 0;
        for (const auto& object : document.GetObjects()) {
            auto* solid = dynamic_cast<CSolid*>(object.get());
            if (!solid) continue;
            solid->MeshQuadro = true;
            for (bool slx : {false, true}) {
                solid->MeshQuadroHoleSLX = slx;
                for (float density : {0.15f, 0.20f, 0.35f, 0.50f, 0.70f, 1.0f, 0.35f}) {
                    std::cout << "Grouped frames density=" << density << " slx=" << slx << std::endl;
                    require(solid->ReBuldMesh(1.0f / density), "Grouped frames failed to rebuild");
                    std::vector<const CMesh3D*> meshes;
                    for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                        auto* surface = solid->GetSurfaceFace(i);
                        require(surface && surface->pMesh3D, "Missing grouped-frame surface");
                        meshes.push_back(surface->pMesh3D);
                        size_t wires = 0;
                        for (TopExp_Explorer wire(surface->m_Face, TopAbs_WIRE); wire.More(); wire.Next()) ++wires;
                        if (wires < 4) continue;
                        ++checked;
                        require(surface->UsedSlxHoleCut() == slx, "Grouped frames bypassed the requested mode");
                        std::vector<std::unique_ptr<CPolyline>> frames;
                        require(surface->CreateLastIslandBoundaryPolylines(frames)
                                    && frames.size() == expected_groups,
                                "Missing independent row frames");
                        const auto& mesh = *surface->pMesh3D;
                        bool manifold = false;
                        require(ClosedMeshBoundaryLoopCount(mesh, manifold) == wires && manifold
                                    && ActiveFaceEdgeComponentCount(mesh) == 1,
                                "Grouped frames changed the hole topology");
                        size_t quads = 0, cells = 0;
                        double area = 0;
                        for (const auto& face : mesh.GetFaces()) {
                            if (face.deleted || face.corners.size() < 3) continue;
                            ++cells;
                            require(face.corners.size() <= 4, "Unsupported grouped-frame polygon");
                            const auto& v = mesh.GetVertices();
                            const auto a = v[face.corners[0].v];
                            Vec3 av{};
                            for (size_t k = 1; k + 1 < face.corners.size(); ++k)
                                av = av + cross(v[face.corners[k].v] - a, v[face.corners[k + 1].v] - a);
                            area += 0.5 * std::sqrt(dot(av, av));
                            if (face.corners.size() == 4) {
                                ++quads;
                                const auto b = v[face.corners[1].v], c = v[face.corners[2].v], d = v[face.corners[3].v];
                                require(dot(cross(b-a,c-a), cross(c-a,d-a)) > 0,
                                        "Grouped frame contains a folded quad");
                            }
                        }
                        GProp_GProps properties;
                        BRepGProp::SurfaceProperties(surface->m_Face, properties);
                        require(std::fabs(area / properties.Mass() - 1) < 0.001,
                                "Grouped frames lost area or overlap");
                        require(quads * 2 > cells, "Grouped frames lost the quad background");
                    }
                    auto welded = CMesh3D::CreateWelded(meshes);
                    bool manifold = false;
                    require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                            "Grouped frames broke a CAD seam");
                }
            }
        }
        require(checked > 0, "No grouped-hole face tested");
        return 0;
    }
    if (argc == 3 && (std::string(argv[1]) == "--test-rounded-opening-collar"
                     || std::string(argv[1]) == "--test-multirow-hole-zone")) {
        const bool multirow = std::string(argv[1]) == "--test-multirow-hole-zone";
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room,error;
        ProjectViewState view;
        require(serializer.Load(QString::fromLocal8Bit(argv[2]),document,room,view,error), "Cannot load rounded opening");
        CSolid* solid = nullptr;
        for (const auto& object : document.GetObjects())
            if (object->GetName() == "Boolean Cut") solid = dynamic_cast<CSolid*>(object.get());
        require(solid, "Missing rounded opening body");
        solid->MeshQuadro = true;
        const std::vector<float> densities = multirow
            ? std::vector<float>{0.15f,0.2f,0.35f,0.5f,0.15f}
            : std::vector<float>{0.2f,0.35f,0.5f,0.7f,1.f,0.35f};
        for (bool slx : {false,true}) for (float density : densities) {
            solid->MeshQuadroHoleSLX = slx;
            require(solid->ReBuldMesh(1.f/density), "Rounded opening rebuild failed");
            std::vector<const CMesh3D*> meshes;
            for (int i=0;i<solid->GetNumSurfaces();++i) meshes.push_back(solid->GetSurfaceFace(i)->pMesh3D);
            auto welded = CMesh3D::CreateWelded(meshes);
            bool manifold = false;
            require(welded && ClosedMeshBoundaryLoopCount(*welded,manifold) == 0 && manifold,
                    "Rounded collar has open seams");
            const auto* surface = solid->GetSurfaceFace(multirow ? 4 : 0);
            const auto& mesh = *surface->pMesh3D;
            require(surface->UsedSlxHoleCut() == slx, "Rounded opening silently bypassed SLX");
            double mesh_area = 0;
            for (const auto& face : mesh.GetFaces()) {
                if (face.deleted || face.corners.size()<3) continue;
                const auto a = mesh.GetVertices()[face.corners[0].v];
                Vec3 area_vector{};
                for (size_t k=1;k+1<face.corners.size();++k) {
                    const auto n = cross(mesh.GetVertices()[face.corners[k].v]-a,
                                         mesh.GetVertices()[face.corners[k+1].v]-a);
                    area_vector = area_vector + n;
                }
                mesh_area += 0.5*std::sqrt(dot(area_vector,area_vector));
            }
            GProp_GProps area_properties;
            BRepGProp::SurfaceProperties(surface->m_Face,area_properties);
            std::cout << "Area error=" << mesh_area/area_properties.Mass()-1 << std::endl;
            require(std::fabs(mesh_area/area_properties.Mass()-1)<0.001,
                    "SLX cut lost area or created overlapping cells");
            if (multirow) {
                size_t quads=0, cells=0;
                for(const auto& face:mesh.GetFaces()) {
                    if(face.deleted || face.corners.size()<3) continue;
                    ++cells;
                    if(face.corners.size()==4) {
                        ++quads;
                        const auto& p=mesh.GetVertices();
                        const auto a=p[face.corners[0].v],b=p[face.corners[1].v],
                            c=p[face.corners[2].v],d=p[face.corners[3].v];
                        require(dot(cross(b-a,c-a),cross(c-a,d-a))>0, "Folded multirow quad");
                    }
                }
                require(quads*2>cells, "Multirow panel is mostly triangles");
                std::cout << "Multirow density=" << density << " slx=" << slx << " passed\n";
                continue;
            }
            std::vector<std::pair<Vec3,Vec3>> boundary;
            const auto outer = BRepTools::OuterWire(TopoDS::Face(surface->m_Face));
            for (int e=0;e<surface->GetPreparedPolylineCount();++e) {
                TopoDS_Edge edge;
                surface->GetPreparedTopoEdge(e,edge);
                bool external=false;
                for (TopExp_Explorer x(outer,TopAbs_EDGE);x.More();x.Next()) external = external || x.Current().IsSame(edge);
                if (external) continue;
                std::vector<CPoint3d> points;
                surface->GetPreparedPolylinePoints(e,points);
                for (size_t p=1;p<points.size();++p) boundary.push_back({
                    {float(points[p-1].x),float(points[p-1].y),float(points[p-1].z)},
                    {float(points[p].x),float(points[p].y),float(points[p].z)}});
            }
            for (const auto& edge : boundary) {
                bool found=false;
                for (const auto& face : mesh.GetFaces()) {
                    if (face.deleted || face.corners.size()!=4) continue;
                    for (size_t k=0;k<4;++k) {
                        auto a=mesh.GetVertices()[face.corners[k].v], b=mesh.GetVertices()[face.corners[(k+1)%4].v];
                        auto coincident=[](Vec3 x,Vec3 y){return dot(x-y,x-y)<1.e-6f;};
                        if (!((coincident(a,edge.first)&&coincident(b,edge.second))||(coincident(a,edge.second)&&coincident(b,edge.first)))) continue;
                        auto c=mesh.GetVertices()[face.corners[(k+2)%4].v], d=mesh.GetVertices()[face.corners[(k+3)%4].v];
                        require(dot(cross(b-a,c-a),cross(c-a,d-a))>1.e-10f,"Folded collar quad");
                        found=true;
                    }
                }
                require(found,"An opening edge has no complete collar quad");
            }
            std::cout << "Rounded opening density=" << density << " slx=" << slx << " passed\n";
        }
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-ball-box-quadro") {
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState view;
        require(serializer.Load(QString::fromLocal8Bit(argv[2]), document, room, view, error), "Cannot load Ball And Box");
        size_t tested = 0;
        for (const auto& object : document.GetObjects()) {
            auto* solid = dynamic_cast<CSolid*>(object.get());
            if (!solid || solid->GetName() != "Boolean Union") continue;
            ++tested;
            solid->MeshQuadro = true;
            solid->MeshQuadroHoleSLX = false;
            for (float density : {0.25f,0.35f,0.5f,0.7f,1.f,0.35f}) {
                require(solid->ReBuldMesh(1.f/density), "Ball And Box rebuild failed");
                std::vector<const CMesh3D*> meshes;
                for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                    const auto* surface = solid->GetSurfaceFace(i);
                    require(surface && surface->pMesh3D, "Missing Ball And Box surface");
                    meshes.push_back(surface->pMesh3D);
                    if (BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType() != GeomAbs_BSplineSurface) continue;
                    size_t quads = 0;
                    for (const auto& face : surface->pMesh3D->GetFaces()) {
                        if (face.deleted) continue;
                        ++quads;
                        require(face.corners.size() == 4, "Non-quad fillet cell");
                        const auto& p = surface->pMesh3D->GetVertices();
                        auto a=p[face.corners[0].v], b=p[face.corners[1].v], c=p[face.corners[2].v], d=p[face.corners[3].v];
                        require(dot(cross(b-a,c-a),cross(c-a,d-a)) > 1.e-10f, "Folded fillet cell");
                    }
                    if (density <= 0.35f) require(quads <= 120, "Over-refined fillet");
                }
                auto welded = CMesh3D::CreateWelded(meshes);
                bool manifold = false;
                require(welded && ClosedMeshBoundaryLoopCount(*welded,manifold) == 0 && manifold,
                        "Ball And Box has mismatched seams");
                std::cout << "Ball And Box density=" << density << " passed\n";
            }
        }
        require(tested == 1, "Missing Ball And Box body");
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-torus-plane-cut-quadro") {
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState view;
        require(serializer.Load(QString::fromLocal8Bit(argv[2]), document, room, view, error),
                error.toLocal8Bit().constData());
        size_t tested = 0;
        for (const auto& object : document.GetObjects()) {
            auto* solid = dynamic_cast<CSolid*>(object.get());
            if (!solid || solid->GetName() != "Torus") continue;
            ++tested;
            solid->MeshQuadro = true;
            solid->MeshQuadroHoleSLX = false;
            for (float density : {0.25f, 0.50f, 0.70f, 1.0f, 0.50f}) {
                require(solid->ReBuldMesh(1.0f/density), "Cut torus rebuild failed.");
                std::vector<const CMesh3D*> meshes;
                size_t toroidal = 0;
                for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                    const auto* surface = solid->GetSurfaceFace(i);
                    require(surface && surface->pMesh3D, "Missing cut torus mesh.");
                    const auto& mesh = *surface->pMesh3D;
                    meshes.push_back(&mesh);
                    require(ActiveFaceEdgeComponentCount(mesh) == 1, "Disconnected cut torus face.");
                    const bool torus = BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType() == GeomAbs_Torus;
                    toroidal += torus;
                    double area = 0;
                    for (const auto& face : mesh.GetFaces()) {
                        if (face.deleted) continue;
                        require(face.corners.size() == 4, "Cut torus contains non-quads.");
                        const auto a = mesh.GetVertices().at(face.corners[0].v);
                        const auto b = mesh.GetVertices().at(face.corners[1].v);
                        const auto c = mesh.GetVertices().at(face.corners[2].v);
                        const auto d = mesh.GetVertices().at(face.corners[3].v);
                        const auto first = cross(b-a,c-a), second = cross(c-a,d-a);
                        const double cellArea = 0.5*(std::sqrt(dot(first,first))+std::sqrt(dot(second,second)));
                        require(std::isfinite(cellArea) && cellArea > 1.e-8, "Degenerate cut torus cell.");
                        area += cellArea;
                        if (torus) {
                            require(dot(first,second) > 0, "Folded torus quad.");
                            require(std::max({dot(b-a,b-a),dot(c-b,c-b),dot(d-c,d-c),dot(a-d,a-d)}) < 100,
                                    "Torus edge crosses the body.");
                        }
                    }
                    GProp_GProps props;
                    BRepGProp::SurfaceProperties(surface->m_Face, props);
                    if (torus) require(std::abs(area/props.Mass()-1) < 0.06, "Cut torus surface area was lost.");
                }
                require(toroidal == 2, "Expected two toroidal charts.");
                auto welded = CMesh3D::CreateWelded(meshes);
                bool manifold = false;
                require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                        "Cut torus has open or non-manifold seams.");
                std::cout << "Cut torus density=" << density << " passed.\n";
            }
        }
        require(tested == 1, "Expected one cut torus.");
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-shell-rim-quadro") {
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState view;
        require(serializer.Load(QString::fromLocal8Bit(argv[2]), document, room, view, error),
                error.toLocal8Bit().constData());
        size_t tested = 0;
        for (const auto& object : document.GetObjects()) {
            auto* solid = dynamic_cast<CSolid*>(object.get());
            if (!solid || solid->GetName() != "Shell") continue;
            ++tested;
            solid->MeshQuadro = true;
            solid->MeshQuadroHoleSLX = false;
            for (float density : {0.25f, 0.50f, 0.70f, 1.0f, 0.50f}) {
                require(solid->ReBuldMesh(1.0f / density), "Shell rebuild failed.");
                std::vector<const CMesh3D*> meshes;
                size_t rims = 0;
                for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                    auto* surface = solid->GetSurfaceFace(i);
                    require(surface && surface->pMesh3D, "Missing shell mesh.");
                    const auto& mesh = *surface->pMesh3D;
                    meshes.push_back(&mesh);
                    require(ActiveFaceEdgeComponentCount(mesh) == 1, "Disconnected shell face.");
                    if (BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                        != GeomAbs_BSplineSurface) continue;
                    ++rims;
                    // Both rims have radius above 125. A reversed boundary
                    // produces chords through the opening despite a watertight mesh.
                    for (const auto& face : mesh.GetFaces()) {
                        if (face.deleted) continue;
                        require(face.corners.size() == 4, "Shell rim lost its quads.");
                        Vec3 center{};
                        for (size_t k = 0; k < 4; ++k) {
                            const auto a = mesh.GetVertices().at(face.corners[k].v);
                            const auto b = mesh.GetVertices().at(face.corners[(k+1)%4].v);
                            require(dot(b-a,b-a) < 6400.0f, "Shell rim crosses its opening.");
                            center = center + a * 0.25f;
                        }
                        require(center.x*center.x + center.y*center.y > 120.0f*120.0f,
                                "Shell rim fills the central opening.");
                    }
                }
                require(rims == 2, "Expected two shell rims.");
                auto welded = CMesh3D::CreateWelded(meshes);
                bool manifold = false;
                require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                        "Shell has open or non-manifold seams.");
                std::cout << "Shell rim density=" << density << " passed.\n";
            }
        }
        require(tested == 1, "Expected one Shell.");
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-revolve-all-filleted") {
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState view;
        require(serializer.Load(QString::fromLocal8Bit(argv[2]), document, room, view, error),
                error.toLocal8Bit().constData());
        size_t tested = 0;
        for (const auto& object : document.GetObjects()) {
            auto* solid = dynamic_cast<CSolid*>(object.get());
            if (!solid) continue;
            ++tested;
            solid->MeshQuadro = true;
            solid->MeshQuadroHoleSLX = false;
            for (float density : {0.25f, 0.35f, 0.50f, 0.80f, 0.35f}) {
                require(solid->ReBuldMesh(1.0f / density), "Revolve fillet rebuild failed.");
                std::vector<const CMesh3D*> meshes;
                size_t revolved = 0;
                for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                    auto* surface = solid->GetSurfaceFace(i);
                    require(surface && surface->pMesh3D, "Missing revolve surface mesh.");
                    const auto& mesh = *surface->pMesh3D;
                    require(ActiveFaceEdgeComponentCount(mesh) == 1,
                            "Revolve surface mesh is empty or disconnected.");
                    meshes.push_back(&mesh);
                    if (BRepAdaptor_Surface(TopoDS::Face(surface->m_Face)).GetType()
                        != GeomAbs_SurfaceOfRevolution) continue;
                    ++revolved;
                    SurfaceUVMapping mapping(surface);
                    for (int edge = 0; edge < surface->GetPreparedPolylineCount(); ++edge) {
                        std::vector<CPoint3d> points;
                        require(surface->GetPreparedPolylinePoints(edge, points), "Missing fillet boundary.");
                        for (const auto& p : points) {
                            SurfaceUVPoint uv;
                            require(mapping.Project({float(p.x), float(p.y), float(p.z)}, uv),
                                    "Fillet boundary projection failed.");
                            CPoint8d restored;
                            require(surface->GetPoint(uv.u, uv.v, &restored), "Fillet evaluation failed.");
                            require(std::hypot(std::hypot(restored.x - p.x, restored.y - p.y),
                                               restored.z - p.z) < 2.0e-4,
                                    "Fillet projection jumped to another branch.");
                        }
                    }
                    for (const auto& face : mesh.GetFaces()) {
                        if (face.deleted) continue;
                        require(face.corners.size() == 4, "Revolved fillet lost its quad strip.");
                        for (size_t k = 0; k < face.corners.size(); ++k) {
                            const Vec3 a = mesh.GetVertices().at(face.corners[k].v);
                            const Vec3 b = mesh.GetVertices().at(face.corners[(k + 1) % 4].v);
                            require(dot(b - a, b - a) < 400.0f, "Fillet edge crosses the body.");
                        }
                    }
                }
                require(revolved > 0, "Fixture lacks a revolved fillet.");
                auto welded = CMesh3D::CreateWelded(meshes);
                bool manifold = false;
                require(welded && ClosedMeshBoundaryLoopCount(*welded, manifold) == 0 && manifold,
                        "Revolve fillets retained open or non-manifold seams.");
                std::cout << "Revolve all filleted density=" << density << " passed.\n";
            }
        }
        require(tested == 1, "Expected one revolve solid.");
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-native-colors") return TestNativeColors();
    if (argc == 4 && std::string(argv[1]) == "--render-native-project") {
        CAlfaDoc doc;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState view;
        require(serializer.Load(QString::fromLocal8Bit(argv[2]),
                                doc, room, view, error), qPrintable(error));
        for (const auto& object : doc.GetObjects()) {
            if (auto* solid = dynamic_cast<CSolid*>(object.get()))
                require(solid->ReBuldMesh(), "Cannot build render geometry");
        }
        const auto scene = BuildRenderScene(doc, view.camera,
                                             view.orthographic_projection);
        NativeRaytraceSettings settings;
        settings.width = 640;
        settings.height = 480;
        settings.progressive_passes = 1;
        settings.thread_count = 4;
        QSettings saved("Dom3D", "Dom3D_Pro");
        saved.beginGroup("render/native");
        settings.light_strength = saved.value(
            "lightStrength", settings.light_strength).toDouble();
        settings.exposure_ev = saved.value("exposure", settings.exposure_ev).toDouble();
        settings.ambient_strength = saved.value(
            "ambient", settings.ambient_strength).toDouble();
        settings.light_count = saved.value("interiorLights", settings.light_count).toInt();
        settings.shadow_density = saved.value("shadowDensity", 100.0).toDouble() / 100.0;
        settings.light_radius_fraction = saved.value("lightSize", 6.0).toDouble() / 100.0;
        std::cout << "meshes=" << scene.meshes.size()
                  << " triangles=" << scene.TriangleCount()
                  << " light=" << settings.light_strength
                  << " exposure=" << settings.exposure_ev
                  << " ambient=" << settings.ambient_strength << std::endl;
        for (const auto& material : scene.materials)
            std::cout << material.name << " diffuse=" << material.diffuse.r
                      << "," << material.diffuse.g << "," << material.diffuse.b << std::endl;
        QImage image;
        require(NativeRaytraceRenderer::Render(scene, settings, &image, &error),
                qPrintable(error));
        require(image.save(QString::fromLocal8Bit(argv[3])),
                "Cannot save native diagnostic render");
        return 0;
    }

    if (argc == 2 && std::string(argv[1]) == "--test-visible-step-and-selection") {
        TestVisibleStepExportAndSelectAll();
        std::cout << "Visible STEP export and selection tests passed.\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-cushion-stage-one") {
        TestCushionStageOneMesh();
        std::cout << "Cushion stage-one mesh tests passed.\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-four-hole-collars") {
        TestFourHoleCollars(QString::fromLocal8Bit(argv[2]));
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-six-hole-shared-zone") {
        TestSixHoleSharedZone(QString::fromLocal8Bit(argv[2]));
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-wire-circular-caps") {
        TestWireCircularCaps(argv[2]);
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-cylinder-box-windows") {
        TestCylinderMultipleWindows(QString::fromLocal8Bit(argv[2]), true);
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-cylinder-multiple-windows") {
        TestCylinderMultipleWindows(QString::fromLocal8Bit(argv[2]));
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-mesh-var5-reallocation") {
        TestMeshVar5Reallocation();
        std::cout << "Mesh Var-5 reallocation tests passed.\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-hole-placement") {
        TestHolePlacement();
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-mesh-point-split-reallocation") {
        TestMeshPointSplitReallocation();
        std::cout << "Mesh point-split reallocation tests passed.\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-boolean-three-slx") {
        TestBooleanThreeSlx(QString::fromLocal8Bit(argv[2]));
        std::cout << "Boolean-3 SLX tests passed.\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-boolean-four-fill") {
        TestBooleanFourFillQuadro(QString::fromLocal8Bit(argv[2]));
        std::cout << "Boolean-4 Quadro tests passed.\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-mesh-var11") {
        TestMeshVar11Regression();
        std::cout << "Mesh Var-11 tests passed.\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-prism-filleted-slx") {
        TestPrismFilletedSlx(QString::fromLocal8Bit(argv[2]));
        std::cout << "Filleted prism SLX tests passed.\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-box-double-cut-slx") {
        TestBoxDoubleCutSlx(QString::fromLocal8Bit(argv[2]));
        std::cout << "Double-cut Box SLX tests passed.\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-prism-two-fillets") {
        TestPrismTwoFilletsQuadro(QString::fromLocal8Bit(argv[2]));
        std::cout << "Prism two-fillet Quadro tests passed.\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-chamfer-and-fillets") {
        TestChamferAndFilletsQuadro(QString::fromLocal8Bit(argv[2]));
        std::cout << "ChamferAndFillets Quadro tests passed.\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-box-min-box-filled") {
        TestBoxMinBoxFilledQuadro(QString::fromLocal8Bit(argv[2]));
        std::cout << "Box_Min_Box_Filled Quadro tests passed.\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-demos-mesh") {
        TestDemosMesh(argv[2]); return 0;
    }
    if ((argc == 2 || argc == 3) && std::string(argv[1]) == "--test-divide-face") {
        TestDivideFace(argc == 3 ? argv[2] : nullptr);
        return 0;
    }
    if (argc == 6 && std::string(argv[1]) == "--export-slx-audit") {
        extern void ExportSlxAudit(const char*,float,int,const char*);
        ExportSlxAudit(argv[2],std::stof(argv[3]),std::stoi(argv[4]),argv[5]); return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-slx-hole-scope") {
        extern void TestSlxHoleScope(const char*);
        TestSlxHoleScope(argv[2]);
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-extrude-corner-quads") {
        extern void TestExtrudeCornerQuads(const char*);
        TestExtrudeCornerQuads(argv[2]);
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-obj-sharp-edges") {
        TestObjSharpEdges(); return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-low-poly-sharp-edges") {
        TestLowPolySharpEdges(); return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-mesh-subdivision") {
        TestMeshSubdivision(); return 0;
    }
    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--test-tools-mesh3d") {
        TestToolsMesh3D(argv[2], argc == 4 ? argv[3] : nullptr);
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-extrude-sl-quadro") {
        TestExtrudeSlQuadro(QString::fromLocal8Bit(argv[2]));
        std::cout << "Extrude_SL Quadro tests passed.\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-bezier-collar-welding") {
        TestBezierCollarWelding(QString::fromLocal8Bit(argv[2]));
        std::cout << "Bezier collar welding tests passed.\n";
        return 0;
    }
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
    if (argc == 3 && std::string(argv[1]) == "--test-hole-low-density-collar") {
        TestHoleLowDensityCollar(QString::fromLocal8Bit(argv[2]));
        std::cout << "Hole low-density collar tests passed.\n";
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
        TestLoftSplineOrdering();
        TestTangentCapFromClosedSpline();
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
    if (argc == 2 && std::string(argv[1]) == "--test-curve-offset") {
        TestParametricCurveOffset();
        std::cout << "Parametric Curve Offset tests passed.\n";
        return 0;
    }
    if (argc == 2
        && std::string(argv[1]) == "--test-curve-mirror-copy") {
        TestParametricCurveMirrorCopy();
        std::cout << "Parametric Curve Mirror Copy tests passed.\n";
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-curve-link") {
        TestParametricCurveLink();
        std::cout << "Parametric Curve Link tests passed.\n";
        return 0;
    }
    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--test-smart-hybrid-file") {
        TestLargeSmartHybrid(argv[2], argc == 4 ? std::stoul(argv[3]) : 0);
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-panel-contour") {
        TestPanelContour();
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-smart-hybrid") {
        for (bool reverse : {false, true}) {
            TestParametricSmartHybrid(false, false, reverse);
            TestParametricSmartHybrid(false, true, reverse);
            TestParametricSmartHybrid(true, true, reverse);
        }
        TestLargeSmartHybrid();
        std::cout << "Parametric Smart Hybrid tests passed.\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-prism-hollow-fillet") {
        TestPrismHollowFillet(argv[2]);
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-live-fillet-validation") {
        TestLiveFilletValidation();
        std::cout << "Live fillet validation tests passed.\n";
        return 0;
    }
    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--test-frame2-render-seams") {
        TestFrame2RenderSeams(QString::fromLocal8Bit(argv[2]), argc == 4 ? QString::fromLocal8Bit(argv[3]) : QString{});
        return 0;
    }
    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--test-frame2-fillet") {
        TestFrame2Fillet(QString::fromLocal8Bit(argv[2]), argc == 4 ? QString::fromLocal8Bit(argv[3]) : QString{});
        return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "--diagnose-project-fillets") {
        DiagnoseProjectFillets(QString::fromLocal8Bit(argv[2]), std::stod(argv[3]));
        return 0;
    }
    if (argc == 2
        && std::string(argv[1]) == "--test-low-poly-density-calibration") {
        TestLowPolyDensityCalibration();
        std::cout << "Low Poly density calibration tests passed.\n";
        return 0;
    }
    if (argc == 7 && std::string(argv[1]) == "--export-quadro-boundaries-uv") {
        const QString source = QString::fromLocal8Bit(argv[2]);
        const size_t requested_solid = std::stoul(argv[4]);
        const int requested_surface = std::stoi(argv[5]);
        const QString destination = QString::fromLocal8Bit(argv[6]);
        require(requested_solid > 0 && requested_surface > 0,
                "Solid and surface indices are one-based.");
        require(QFileInfo(source).absoluteFilePath() != QFileInfo(destination).absoluteFilePath()
                    && !QFileInfo::exists(destination),
                "Boundary export requires a new destination project.");
        const float density = std::stof(argv[3]);
        require(std::isfinite(density) && density > 0.0f, "Density must be positive.");
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState view;
        require(serializer.Load(source, document, room, view, error),
                error.toLocal8Bit().constData());
        QImage thumbnail;
        serializer.LoadThumbnail(source, thumbnail, error);
        std::vector<std::unique_ptr<CPolyline>> boundaries;
        size_t solid_index = 0;
        for (const auto& object : document.GetObjects()) {
            auto* solid = dynamic_cast<CSolid*>(object.get());
            if (!solid) continue;
            ++solid_index;
            if (solid_index != requested_solid) continue;
            solid->MeshQuadro = true;
            solid->MeshQuadroHoleSLX = false;
            solid->ptchDensity = density;
            require(solid->ReBuldMesh(1.0f / density), "Boundary export rebuild failed.");
            for (int i = 0; i < solid->GetNumSurfaces(); ++i) {
                if (i + 1 != requested_surface) continue;
                const CSurfaceFace* surface = solid->GetSurfaceFace(i);
                std::vector<std::unique_ptr<CPolyline>> lines;
                require(surface && !surface->m_Face.IsNull(), "Missing export surface.");
                // Use the cached island passed to MakeFilledContour, not an
                // OCCT re-tessellation and not only rejected-input diagnostics.
                // Boundary Line must remain in native UV: no radius scaling,
                // restoration to 3D, translation, or resampling.
                for (const auto& patch : surface->m_LastIslandBoundariesUV) {
                    auto line = std::make_unique<CPolyline>();
                    for (const CPoint3d& uv : patch) {
                        const float u = static_cast<float>(uv.x);
                        const float v = static_cast<float>(uv.y);
                        line->AddPoint({u, v, 0.0});
                    }
                    line->SetClosed(true);
                    lines.push_back(std::move(line));
                }
                for (size_t j = 0; j < lines.size(); ++j) {
                    lines[j]->SetName("Boundary Line UV - Solid " + std::to_string(solid_index)
                        + " - Surface " + std::to_string(i + 1)
                        + " - Loop " + std::to_string(j + 1)
                        + " - Density " + QString::number(density, 'f', 2).toStdString());
                    lines[j]->SetColor({0.0f, 1.0f, 1.0f});
                    lines[j]->SetLineWidth(2.0);
                    std::cout << lines[j]->GetName() << " nodes="
                              << lines[j]->GetPointCount() << '\n';
                    boundaries.push_back(std::move(lines[j]));
                }
            }
        }
        require(!boundaries.empty(), "No cached quadrangulation input boundaries were captured.");
        const size_t original_count = document.GetObjects().size();
        int boundary_layer = document.GetWorkLayerID();
        for (const CLayer* layer : document.m_Layers) {
            if (layer && layer->Name == "Default") {
                boundary_layer = layer->ID();
                break;
            }
        }
        std::vector<std::vector<CPoint3d>> expected_points;
        for (auto& boundary : boundaries) {
            expected_points.push_back(boundary->GetPoints());
            CPolyline* added = boundary.get();
            document.AddObject(std::move(boundary), false);
            added->m_LayerID = boundary_layer;
        }
        require(serializer.Save(destination, document, room, view, thumbnail, error),
                error.toLocal8Bit().constData());
        CAlfaDoc round_trip;
        require(serializer.Load(destination, round_trip, room, view, error),
                error.toLocal8Bit().constData());
        require(round_trip.GetObjects().size() == original_count + expected_points.size(),
                "Boundary export changed the document object count.");
        for (size_t i = 0; i < expected_points.size(); ++i) {
            const auto* boundary = dynamic_cast<const CPolyline*>(
                round_trip.GetObjects()[original_count + i].get());
            require(boundary && boundary->IsClosed()
                        && boundary->GetPointCount() == expected_points[i].size(),
                    "Boundary export lost the closed polyline or its nodes.");
            for (size_t j = 0; j < expected_points[i].size(); ++j) {
                const CPoint3d actual = boundary->GetPoints()[j];
                const CPoint3d expected = expected_points[i][j];
                // The mesh pipeline consumes Vec3 floats; nine significant
                // digits in the project must round-trip that input exactly.
                require(static_cast<float>(actual.x) == static_cast<float>(expected.x)
                            && static_cast<float>(actual.y) == static_cast<float>(expected.y)
                            && static_cast<float>(actual.z) == static_cast<float>(expected.z),
                        "Boundary export changed the quadrangulator input coordinates.");
            }
        }
        std::cout << "Saved and verified " << expected_points.size()
                  << " exact boundary polyline(s).\n";
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-hairdryer-cad-boundary") {
        try {TestHairdryerCadBoundary(argv[2]);return 0;}
        catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    if (argc == 4 && std::string(argv[1]) == "--diagnose-hairdryer-chart-fill") {
        try {DiagnoseHairdryerChartFill(argv[2],argv[3]);return 0;}
        catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    }
    if (argc == 3 && std::string(argv[1]) == "--test-fillet-mesh-normals") {
        TestFilletMeshNormals(argv[2]);
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-two-rail-surface-document") {
        TestTwoRailSurfaceDocument(argv[2]);
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-periodic-bspline") {
        TestPeriodicBSpline(argv[2]);
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-cylinder-quadro-normals") {
        TestCylinderQuadroNormals(argv[2]);
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-two-sketch-corner-normals") {
        TestTwoSketchCornerNormals(argv[2]);
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--test-frame-cad-quadro") {
        TestFrameCadQuadro(argv[2]);
        return 0;
    }
    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--test-bridge-shell-quadro") {
        TestBridgeShellQuadro(argv[2], argc == 4 ? argv[3] : nullptr);
        return 0;
    }
    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--test-pillow-cad-quadro") {
        TestPillowCadQuadro(argv[2], argc == 4 ? argv[3] : nullptr);
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--test-quadro-body-cache") {
        auto shape = BRepPrimAPI_MakeBox(10,20,30).Shape();
        CSolid solid(shape);
        const auto first = solid.GetQuadroTopologySnapshot();
        require(first->topologyValid(), "Body snapshot invalid");
        solid.MeshQuadro = true;
        require(solid.ReBuldMesh(2.f), "Initial box mesh failed");
        require(solid.GetQuadroTopologySnapshot() == first, "Remeshing recreated CAD snapshot");
        require(solid.ReBuldMesh(1.f), "Changed-density box mesh failed");
        require(solid.GetQuadroTopologySnapshot() == first, "Density invalidated CAD snapshot");
        quadro::SamplingOptions options;
        auto prepared = solid.PrepareQuadroBoundary(options);
        require(prepared == solid.PrepareQuadroBoundary(options), "Identical sampling not cached");
        options.maxSegmentLength /= 2;
        require(prepared != solid.PrepareQuadroBoundary(options), "Sampling change not detected");
        require(solid.GetQuadroBoundaryCaptureCount() == 1, "Density recaptured topology");
        solid.m_Shape = BRepPrimAPI_MakeCylinder(5,15).Shape();
        const auto replaced = solid.GetQuadroTopologySnapshot();
        require(replaced != first && replaced->bodyRevision() > first->bodyRevision(), "Shape replacement not detected");
        solid.InvalidateQuadroBoundary();
        require(solid.GetQuadroTopologySnapshot() != replaced, "Explicit CAD invalidation ignored");
        solid.Clear();
        require(first->topologyValid() && first->faces().size() == 6, "Retained immutable snapshot damaged");
        std::cout << "Body boundary cache: remeshing, density, replacement and invalidation passed.\n";
        return 0;
    }
    if ((argc == 5 || argc == 6) && std::string(argv[1]) == "--snapshot-project-boundary") {
        CAlfaDoc document;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState view;
        require(serializer.Load(QString::fromLocal8Bit(argv[2]), document, room, view, error), error.toLocal8Bit().constData());
        for (const auto& object : document.GetObjects()) {
            auto* solid = dynamic_cast<CSolid*>(object.get());
            if (!solid || solid->GetName() != argv[3]) continue;
            const auto topology = solid->GetQuadroTopologySnapshot();
            quadro::SamplingOptions sampling;
            if(argc==6){require(std::string(argv[5])=="--cad-tolerance","Unknown boundary preparation option");sampling.allowCadToleranceReconciliation=true;sampling.maximumChartRefinementPasses=8;}
            const auto boundary = solid->PrepareQuadroBoundary(sampling);
            require(topology == solid->GetQuadroTopologySnapshot() && solid->GetQuadroBoundaryCaptureCount() == 1,
                "Preparation recaptured body topology");
            WriteQuadroBoundaryReport(boundary, QString::fromLocal8Bit(argv[4]));
            std::cout << "topologyValid=" << boundary->topologyValid() << " discretizationReady=" << boundary->discretizationReady()
                << " faces=" << boundary->faces().size() << " edges=" << boundary->edges().size()
                << " vertices=" << boundary->vertices().size() << " occurrences=" << boundary->occurrences().size()
                << " issues=" << boundary->issues().size() << '\n';
            return boundary->discretizationReady() ? 0 : 2;
        }
        throw std::runtime_error("Requested solid was not found");
    }
    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--test-periodic-bands") {
        extern void TestPeriodicBandMesh(const char*, const char*);
        TestPeriodicBandMesh(argv[2], argc == 4 ? argv[3] : nullptr);
        return 0;
    }
    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--test-nist-ctc") {
        extern void TestNistCtcMesh(const char*,const char*);
        TestNistCtcMesh(argv[2],argc==4?argv[3]:nullptr);
        return 0;
    }
    if ((argc == 5 || argc == 6) && std::string(argv[1]) == "--test-turned-bands") {
        extern void TestTurnedBandMesh(const char*,float,bool,const char*);
        TestTurnedBandMesh(argv[2],std::stof(argv[3]),std::string(argv[4])=="slx",argc==6?argv[5]:nullptr);
        return 0;
    }
    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--test-bezier-pocket-fillet") {
        TestBezierPocketFillet(argv[2], argc == 4 ? argv[3] : nullptr);
        return 0;
    }
    if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--test-rib-quadro") {
        extern void TestRibQuadro(const char*, const char*);
        TestRibQuadro(argv[2], argc == 4 ? argv[3] : nullptr);
        return 0;
    }
    if (argc >= 4 && argc <= 6
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
        bool useSlx = false;
        std::string requested_solid;
        for (int argument = 4; argument < argc; ++argument) {
            const std::string value = argv[argument];
            if (value == "slx")
                useSlx = true;
            else
                requested_solid = value;
        }
        bool found_requested_solid = requested_solid.empty();
        for (const auto& object : document.GetObjects()) {
            auto* solid = dynamic_cast<CSolid*>(object.get());
            if (!solid)
                continue;
            if (!requested_solid.empty()
                && solid->GetName() != requested_solid) {
                continue;
            }
            found_requested_solid = true;
            std::cout << "rebuilding solid=\"" << solid->GetName()
                      << "\" surfaces=" << solid->GetNumSurfaces()
                      << " density=" << density
                      << " slx=" << useSlx << std::endl;
            solid->MeshQuadro = true;
            solid->MeshQuadroHoleSLX = useSlx;
            ExportQuadroPipelineDiagnostics(*solid, "before", density);
            const bool rebuilt = solid->ReBuldMesh(1.0f / density);
            ExportQuadroPipelineDiagnostics(*solid, "after", density);
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
					const QByteArray dump_prefix = qgetenv("DOM3D_WELD_DUMP");
					if (!dump_prefix.isEmpty()) {
						require(combined.ExportToObj(dump_prefix.toStdString() + "-before.obj"),
							"Could not export the source welding diagnostic mesh.");
						require(welded->ExportToObj(dump_prefix.toStdString() + "-after.obj"),
							"Could not export the welded diagnostic mesh.");
					}
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
        require(found_requested_solid, "Requested solid was not found.");
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
    if (argc == 3 && std::string(argv[1]) == "--benchmark-iges") {
        IgesIO io;std::vector<std::unique_ptr<CAlfaObject>> objects;std::string error;
        const auto begin=std::chrono::steady_clock::now();
        require(io.Import(argv[2],objects,error),error.c_str());
        size_t surfaces=0,faces=0;
        for(const auto& object:objects)if(const auto* solid=dynamic_cast<const CSolid*>(object.get())) {
            surfaces+=solid->GetNumSurfaces();
            require(!solid->MeshQuadro,"IGES import enabled expensive normalized Quadro meshing");
            for(int i=0;i<solid->GetNumSurfaces();++i) {
                const auto* surface=solid->GetSurfaceFace(i);
                require(surface && surface->pMesh3D && !surface->pMesh3D->GetFaces().empty(),"Imported surface has no display mesh");
                faces+=surface->pMesh3D->GetFaces().size();
                for(const Vec3& point:surface->pMesh3D->GetVertices())
                    require(std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z),"Non-finite IGES mesh vertex");
            }
        }
        std::cout<<"IGES benchmark: "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()
            <<" s, "<<objects.size()<<" objects, "<<surfaces<<" surfaces, "<<faces<<" mesh faces\n";
        return EXIT_SUCCESS;
    }
    if (argc == 4 && std::string(argv[1]) == "--test-cylinder-step") {
        extern void TestCylinderStep(const char*, const char*);
        TestCylinderStep(argv[2],argv[3]);
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
                && bend_edit.parameters.size() == 4,
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
