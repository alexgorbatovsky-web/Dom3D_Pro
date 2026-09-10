#include "CAlfaDoc.h"
#include "CMesh3D.h"
#include "CPolyline.h"
#include "BezierSpline.h"
#include "DrawingText.h"
#include "ExchangeIO.h"
#include "ObjIO.h"
#include "GlbIO.h"
#include "CFurnitureAssemblies.h"
#include "SketchArcLine.h"
#include "SmartLine.h"

#include <QCoreApplication>
#include <QApplication>
#include <QDir>
#include <QStringList>
#include <QTemporaryDir>

#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {
void require(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

void TestThreeMfIO(const QString& directory);
void TestFbxIO(const QString& directory);
void TestGlbIO(const QString& directory);
void TestGlbExport(const QString& directory);

int main(int argc, char** argv)
{
    std::cout << std::unitbuf;
    QApplication application(argc, argv);
    // Diagnostic entry point for checking real-world packages with the same
    // importer as the application, without adding private models to the suite.
    const auto arguments = application.arguments();
    if (arguments.size()==3&&arguments[1]=="--export-kitchen-glb") {
        CAlfaDoc kitchen;
        for(auto& part:CNikaKitchenFurniture::BuildParts(NikaKitchenDefinition{})) {
            Material material=part->GetMaterial();material.id=0;
            material.diffuse=part->GetColor();material.name=part->GetName()+" material";
            part->SetMaterial(kitchen.UpsertMaterial(material));
            kitchen.AddObject(std::move(part));
        }
        std::string error;
        require(GlbIO().Export(arguments[2].toStdString(),kitchen,error),error);
        std::cout<<"Kitchen GLB exported: "<<kitchen.GetObjects().size()<<" source objects\n";
        return EXIT_SUCCESS;
    }
    if (arguments.size()==4&&arguments[1]=="--roundtrip-glb") {
        CAlfaDoc document;std::string error;std::vector<std::unique_ptr<CMesh3D>> meshes;
        require(GlbIO().Import(arguments[2].toStdString(),meshes,error),error);
        for(auto& mesh:meshes){mesh->SetMaterial(document.UpsertMaterial(mesh->GetMaterial()));document.AddMesh(std::move(mesh));}
        require(GlbIO().Export(arguments[3].toStdString(),document,error),error);
        return EXIT_SUCCESS;
    }
    if (arguments.size() == 3 && arguments[1] == "--import-glb") {
        std::string error;
        std::vector<std::unique_ptr<CMesh3D>> meshes;
        require(GlbIO().Import(arguments[2].toStdString(), meshes, error), error);
        std::cout << "GLB import succeeded: " << meshes.size() << " mesh(es)\n";
        Vec3 total_low, total_high;
        bool first=true;
        for (const auto& mesh:meshes) {
            Vec3 low,high;require(mesh->GetBounds(low,high),"Imported GLB mesh has no bounds");
            if(first){total_low=low;total_high=high;first=false;}
            else {
                total_low.x=std::min(total_low.x,low.x);total_low.y=std::min(total_low.y,low.y);total_low.z=std::min(total_low.z,low.z);
                total_high.x=std::max(total_high.x,high.x);total_high.y=std::max(total_high.y,high.y);total_high.z=std::max(total_high.z,high.z);
            }
        }
        const auto size=total_high-total_low;
        std::cout<<"Dimensions (mm): "<<size.x<<", "<<size.y<<", "<<size.z<<'\n';
        return EXIT_SUCCESS;
    }
    if (arguments.size() == 3 && arguments[1] == "--write-fbx-test") {
        QDir().mkpath(arguments[2]);
        TestFbxIO(arguments[2]);
        return EXIT_SUCCESS;
    }
    if (arguments.size() == 3 && arguments[1] == "--import-3mf") {
        ThreeMfIO io;
        std::string error;
        std::vector<std::unique_ptr<CMesh3D>> meshes;
        require(io.Import(arguments[2].toStdString(), meshes, error), error);
        std::cout << "3MF import succeeded: " << meshes.size() << " mesh(es)\n";
        for (const auto& mesh : meshes) {
            Vec3 low, high;
            require(mesh->GetBounds(low, high), "Imported mesh has no bounds");
            std::cout << mesh->GetVertices().size() << " vertices, "
                      << mesh->GetFaces().size() << " faces; bounds (mm): "
                      << low.x << ',' << low.y << ',' << low.z << " -> "
                      << high.x << ',' << high.y << ',' << high.z << '\n';
        }
        return EXIT_SUCCESS;
    }
    QTemporaryDir directory;
    require(directory.isValid(), "Could not create the temporary exchange directory.");
    for (bool legacy : {false, true}) {
        const std::string stem = directory.path().toStdString() + (legacy ? "/legacy" : "/standard");
        {
            std::ofstream mtl(stem + ".mtl");
            if (legacy) mtl << "# Dom3D MTL File : 'legacy'\n";
            mtl << "newmtl Bronze\nKd 0 0 0\nKs .69 .48 .29\nd 0\n"
                   "newmtl Glass\nKd 0 0 0\nKs 0 0 0\nd .99\n";
            std::ofstream obj(stem + ".obj");
            obj << "mtllib " << (legacy ? "legacy.mtl" : "standard.mtl")
                << "\nv 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl Bronze\nf 1 2 3\nusemtl Glass\nf 1 3 2\n";
        }
        std::vector<std::unique_ptr<CMesh3D>> imported;
        std::string error;
        require(ObjIO().Import(stem + ".obj", imported, error), error);
        require(imported.size() == 2, "MTL regression: missing material parts");
        for (const auto& mesh : imported) {
            const auto& m = mesh->GetMaterial();
            if (m.name == "Bronze") {
                require(std::abs(m.alpha - (legacy ? 1.f : 0.f)) < .001f, "MTL opacity convention changed");
                require(std::abs(m.diffuse.r - (legacy ? .69f : 0.f)) < .001f, "Legacy Ks conversion leaked or failed");
            } else {
                require(std::abs(m.alpha - (legacy ? .01f : .99f)) < .001f, "MTL glass opacity incorrect");
                require(m.diffuse.r == 0.f, "Black glass must stay black");
            }
        }
    }

    CAlfaDoc document;
    auto curve = std::make_unique<CPolyline>("Round-trip curve");
    curve->AddPoint({0.0, 0.0, 0.0});
    curve->AddPoint({25.0, 10.0, 3.0});
    curve->AddPoint({50.0, 0.0, 0.0});
    curve->SetColor({0.2f, 0.7f, 0.4f});
    document.AddObject(std::move(curve));

    auto analytic_sketch = std::make_unique<CSmartLine>("Analytic DXF sketch");
    require(analytic_sketch->SetCoordinateSystem(
                {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0})
                && analytic_sketch->AddLine(
                    std::make_unique<CLinkLine>(
                        CPoint3d(0.0, 30.0, 0.0),
                        CPoint3d(20.0, 30.0, 0.0)), false)
                && analytic_sketch->AddLine(
                    std::make_unique<CSketchArcLine>(
                        CPoint3d(20.0, 30.0, 0.0),
                        CPoint3d(30.0, 20.0, 0.0),
                        CPoint3d(20.0, 10.0, 0.0)), true),
            "Could not create the analytic DXF source sketch.");
    analytic_sketch->SetLineWidth(0.7);
    analytic_sketch->SetLineStyle("CENTER");
    document.AddObject(std::move(analytic_sketch));

    auto drawing_text = std::make_unique<CDrawingText>(
        "Text-20", CPoint3d(5.0, 8.0, 0.0), 20.0, 12.0);
    drawing_text->SetLineWidth(0.35);
    drawing_text->SetLineStyle("HIDDEN");
    drawing_text->SetFontFamily("Courier New");
    document.AddObject(std::move(drawing_text));

    auto multiline_text = std::make_unique<CDrawingText>(
        "Hello World\nHow are you?", CPoint3d(5.0, 50.0, 0.0), 10.0, 0.0);
    document.AddObject(std::move(multiline_text));

    auto filleted_sketch = std::make_unique<CSmartLine>("Filleted DXF sketch");
    require(filleted_sketch->SetCoordinateSystem(
                {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0})
                && filleted_sketch->AddLine(
                    std::make_unique<CLinkLine>(
                        CPoint3d(0.0, 0.0, 0.0),
                        CPoint3d(20.0, 0.0, 0.0)), false)
                && filleted_sketch->AddLine(
                    std::make_unique<CSketchArcLine>(
                        CPoint3d(20.0, 0.0, 0.0),
                        CPoint3d(30.0, 10.0, 0.0),
                        CPoint3d(20.0, 20.0, 0.0)), true)
                && filleted_sketch->AddLine(
                    std::make_unique<CLinkLine>(
                        CPoint3d(20.0, 20.0, 0.0),
                        CPoint3d(0.0, 20.0, 0.0)), true)
                && filleted_sketch->AddLine(
                    std::make_unique<CLinkLine>(
                        CPoint3d(0.0, 20.0, 0.0),
                        CPoint3d(0.0, 0.0, 0.0)), true)
                && filleted_sketch->SetClosed(true)
                && filleted_sketch->AddFillet(2, 2.0)
                && filleted_sketch->AddFillet(3, 2.0),
            "Could not create the filleted DXF source sketch.");
    document.AddObject(std::move(filleted_sketch));

    auto mesh = std::make_unique<CMesh3D>("Round-trip mesh");
    require(mesh->SetGeometry(
                {{0.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}, {0.0f, 20.0f, 5.0f}},
                {CMesh3D::Face{0, 1, 2}}),
            "Could not create the source triangle mesh.");
    document.AddMesh(std::move(mesh));

    std::string error;
    const std::string base = directory.path().toStdString() + "/exchange";

    ObjIO obj;
    const std::string millimeter_obj = base + "_millimeters.obj";
    require(obj.Export(
                millimeter_obj, document, error,
                ObjLengthUnit::Millimeters),
            "Millimeter OBJ export failed: " + error);
    {
        std::ifstream stream(millimeter_obj);
        const std::string exported{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        require(exported.find("# Dom3D Pro units: millimeters")
                    != std::string::npos
                && exported.find("\nv 20 0 0\n") != std::string::npos,
                "Millimeter OBJ export changed source coordinates.");
    }

    const std::string meter_obj = base + "_meters.obj";
    require(obj.Export(meter_obj, document, error, ObjLengthUnit::Meters),
            "Meter OBJ export failed: " + error);
    {
        std::ifstream stream(meter_obj);
        const std::string exported{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        require(exported.find("# Dom3D Pro units: meters")
                    != std::string::npos
                && exported.find("\nv 0.02 0 0\n") != std::string::npos,
                "Meter OBJ export did not convert millimeters to meters.");
    }
    std::vector<std::unique_ptr<CMesh3D>> obj_meshes;
    require(obj.Import(meter_obj, obj_meshes, error),
            "Meter OBJ re-import failed: " + error);
    require(obj_meshes.size() == 1
                && obj_meshes[0]->GetVertices().size() == 3
                && std::abs(obj_meshes[0]->GetVertices()[1].x - 20.0f)
                    < 1.0e-5f,
            "OBJ unit metadata did not restore millimeter coordinates.");

    const std::string grouped_solids_obj = base + "_grouped_solids.obj";
    {
        std::ofstream stream(grouped_solids_obj);
        stream << "v 0 0 0\n"
               << "v 1 0 0\n"
               << "v 0 1 0\n"
               << "v 1 1 0\n"
               << "v 2 0 0\n"
               << "v 2 1 0\n"
               << "o Solid_A\n"
               << "g Solid_A_Surface_1\n"
               << "usemtl Shared\n"
               << "f 1 2 3\n"
               << "g Solid_A_Surface_2\n"
               << "usemtl Shared\n"
               << "f 2 4 3\n"
               << "o Solid_B\n"
               << "g Solid_B_Surface_1\n"
               << "usemtl Shared\n"
               << "f 2 5 4\n"
               << "g Solid_B_Surface_2\n"
               << "usemtl Shared\n"
               << "f 5 6 4\n";
    }
    std::vector<std::unique_ptr<CMesh3D>> grouped_solid_meshes;
    require(obj.Import(grouped_solids_obj, grouped_solid_meshes, error),
            "Grouped Solid OBJ import failed: " + error);
    require(grouped_solid_meshes.size() == 2
                && grouped_solid_meshes[0]->GetName() == "Solid_A"
                && grouped_solid_meshes[0]->GetFaces().size() == 2
                && grouped_solid_meshes[1]->GetName() == "Solid_B"
                && grouped_solid_meshes[1]->GetFaces().size() == 2,
            "OBJ import did not preserve one Mesh object per source Solid.");

    CAlfaDoc weld_document;
    auto weld_mesh = std::make_unique<CMesh3D>("Weld export mesh");
    require(weld_mesh->SetGeometry(
                {{0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f},
                 {0.0f, 10.0f, 0.0f}, {10.1f, 0.0f, 0.0f},
                 {10.0f, 10.0f, 0.0f}, {0.1f, 10.0f, 0.0f}},
                {CMesh3D::Face{0, 1, 2}, CMesh3D::Face{3, 4, 5}}),
            "Could not create the OBJ welding regression mesh.");
    weld_document.AddMesh(std::move(weld_mesh));
    const std::string welded_obj = base + "_welded.obj";
    require(obj.Export(
                welded_obj, weld_document, error,
                ObjLengthUnit::Millimeters),
            "Welded OBJ export failed: " + error);
    {
        std::ifstream stream(welded_obj);
        std::string line;
        size_t vertex_lines = 0;
        std::vector<std::string> face_lines;
        while (std::getline(stream, line)) {
            if (line.rfind("v ", 0) == 0) ++vertex_lines;
            if (line.rfind("f ", 0) == 0) face_lines.push_back(line);
        }
        require(vertex_lines == 4,
                "OBJ export did not weld vertices within LenMin / 3.0.");
        const auto position_indices = [](const std::string& face_line) {
            std::istringstream row(face_line);
            std::string marker;
            std::string corner;
            std::vector<size_t> indices;
            row >> marker;
            while (row >> corner) {
                indices.push_back(static_cast<size_t>(
                    std::stoull(corner.substr(0, corner.find('/')))));
            }
            return indices;
        };
        require(face_lines.size() == 2
                    && position_indices(face_lines[0])
                        == std::vector<size_t>({1, 2, 3})
                    && position_indices(face_lines[1])
                        == std::vector<size_t>({2, 4, 3}),
                "OBJ export did not remap faces to welded position indices.");
    }

    CMesh3D weld_part_a("Weld part A");
    CMesh3D weld_part_b("Weld part B");
    require(weld_part_a.SetGeometry(
                {{0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f},
                 {0.0f, 10.0f, 0.0f}},
                {CMesh3D::Face{0, 1, 2}})
                && weld_part_b.SetGeometry(
                    {{10.1f, 0.0f, 0.0f}, {10.0f, 10.0f, 0.0f},
                     {0.1f, 10.0f, 0.0f}},
                    {CMesh3D::Face{0, 1, 2}}),
            "Could not create Welding Vertex tool regression meshes.");
    size_t welded_vertex_count = 0;
    float welding_tolerance = 0.0f;
    std::unique_ptr<CMesh3D> merged_weld_mesh = CMesh3D::CreateWelded(
        {&weld_part_a, &weld_part_b},
        &welded_vertex_count, &welding_tolerance);
    require(merged_weld_mesh
                && merged_weld_mesh->GetVertices().size() == 4
                && merged_weld_mesh->GetFaces().size() == 2
                && welded_vertex_count == 2
                && std::abs(welding_tolerance - 1.98f) < 0.001f,
            "Welding Vertex did not merge Mesh3D objects with LenMin / 5.0.");

    // A curved strip without stored normals is displayed with averaged vertex
    // normals. Welding must not replace that shading with polygon normals.
    CMesh3D smooth_strip("Smooth strip");
    require(smooth_strip.SetGeometry(
                {{0, 0, 0}, {10, 0, 0}, {20, 0, 10},
                 {0, 10, 0}, {10, 10, 0}, {20, 10, 10}},
                {CMesh3D::Face{0, 1, 4, 3}, CMesh3D::Face{1, 2, 5, 4}}),
            "Could not create the smooth welding regression.");
    CMesh3D explicit_strip("Explicit normals");
    auto explicit_faces = smooth_strip.GetFaces();
    for (size_t i = 0; i < explicit_faces.size(); ++i)
        for (MeshCorner& corner : explicit_faces[i].corners)
            corner.n = i;
    require(explicit_strip.SetGeometry(smooth_strip.GetVertices(), explicit_faces,
                {}, {{0, 0, 1}, {-1, 0, 0}}),
            "Could not create explicit normal seams for welding.");
    auto smooth_weld = CMesh3D::CreateWelded({&smooth_strip, &explicit_strip});
    require(smooth_weld && smooth_weld->GetFaces().size() == 4,
            "Could not weld the smooth and explicit-normal strips.");
    const auto welded_normal = [&](size_t face, size_t corner) {
        return smooth_weld->GetNormals()[smooth_weld->GetFaces()[face].corners[corner].n];
    };
    const Vec3 expected_smooth = normalize(Vec3{0, 0, 1} + normalize(Vec3{-1, 0, 1}));
    require(dot(welded_normal(0, 1), expected_smooth) > 0.99999f
                && dot(welded_normal(1, 0), expected_smooth) > 0.99999f
                && dot(welded_normal(0, 0), Vec3{0, 0, 1}) > 0.99999f,
            "Welding flattened the implicit smooth vertex normals.");
    require(dot(welded_normal(2, 1), Vec3{0, 0, 1}) > 0.99999f
                && dot(welded_normal(3, 0), Vec3{-1, 0, 0}) > 0.99999f,
            "Welding changed explicit corner normals at a hard seam.");
    auto repeated_weld = CMesh3D::CreateWelded({smooth_weld.get()});
    require(repeated_weld && repeated_weld->GetNormals().size() == smooth_weld->GetNormals().size(),
            "Repeated welding lost the stored shading normals.");
    for (size_t i = 0; i < repeated_weld->GetNormals().size(); ++i)
        require(dot(repeated_weld->GetNormals()[i], smooth_weld->GetNormals()[i]) > 0.99999f,
                "Repeated welding changed surface shading.");

    // A collar can put vertices from two neighbouring rows inside the
    // welding tolerance. The weld must use the closest row, regardless
    // of which candidate appears first in the X lookup.
    CMesh3D weld_candidates("Weld candidates");
    CMesh3D weld_target("Weld target");
    require(weld_candidates.SetGeometry(
                {{-1.9f, 0.0f, 0.0f}, {-1.9f, 10.0f, 0.0f},
                 {-11.9f, 0.0f, 0.0f}, {0.2f, 0.0f, 0.0f},
                 {0.2f, -10.0f, 0.0f}, {10.2f, 0.0f, 0.0f}},
                {CMesh3D::Face{0, 1, 2}, CMesh3D::Face{3, 4, 5}})
                && weld_target.SetGeometry(
                    {{0.0f, 0.0f, 0.0f}, {0.0f, 20.0f, 0.0f},
                     {10.0f, 20.0f, 0.0f}},
                    {CMesh3D::Face{0, 1, 2}}),
            "Could not create the nearest-candidate welding regression.");
    std::unique_ptr<CMesh3D> nearest_weld = CMesh3D::CreateWelded(
        {&weld_candidates, &weld_target});
    require(nearest_weld && nearest_weld->GetFaces().size() == 3,
            "Nearest-candidate welding produced invalid geometry.");
    const size_t target_vertex = nearest_weld->GetFaces()[2].corners[0].v;
    require(target_vertex < nearest_weld->GetVertices().size()
                && std::abs(nearest_weld->GetVertices()[target_vertex].x
                            - 0.2f) < 0.001f,
            "Welding Vertex pulled a seam vertex to a farther mesh row.");

    // Neighbouring rows inside LenMin / 3 but outside LenMin / 5 must
    // remain separate, including when generated faces retain surface IDs.
    CMesh3D collar_rows("Collar rows");
    require(collar_rows.SetGeometry(
                {{0, 0, 0}, {10, 0, 0}, {0, 10, 0},
                 {0, 0, 2.5f}, {10, 0, 2.5f}, {0, 10, 2.5f}},
                {CMesh3D::Face{0, 1, 2}, CMesh3D::Face{3, 4, 5}}),
            "Could not create collar row welding regression.");
    for (bool with_surface_ids : {true, false}) {
        collar_rows.GetFaces()[0].sourceFaceId = with_surface_ids ? 0 : -1;
        collar_rows.GetFaces()[1].sourceFaceId = with_surface_ids ? 1 : -1;
        auto preserved_rows = CMesh3D::CreateWelded(
            {&collar_rows}, &welded_vertex_count, &welding_tolerance);
        require(preserved_rows && welded_vertex_count == 0
                    && preserved_rows->GetVertices().size() == 6
                    && preserved_rows->GetFaces().size() == 2
                    && std::abs(welding_tolerance - 2.0f) < 0.001f,
                "Welding collapsed collar rows or depended on surface IDs.");
    }

    // A tiny real edge must control the tolerance for the whole selection.
    CMesh3D tiny_edge("Tiny edge");
    require(tiny_edge.SetGeometry(
                {{100, 0, 0}, {100.25f, 0, 0}, {100, 10, 0}},
                {CMesh3D::Face{0, 1, 2}}),
            "Could not create minimum-edge welding regression.");
    auto conservative_weld = CMesh3D::CreateWelded(
        {&weld_part_a, &weld_part_b, &tiny_edge},
        &welded_vertex_count, &welding_tolerance);
    require(conservative_weld && welded_vertex_count == 0
                && std::abs(welding_tolerance - 0.05f) < 0.001f,
            "Welding expanded the tolerance beyond the minimum edge / 5.");

    // Shared edges stay closed even across source surfaces. An open sheet
    // close to a closed solid must not weld to the solid, in either order.
    CMesh3D closed_solid("Closed tetrahedron");
    CMesh3D open_sheet("Nearby open sheet");
    require(closed_solid.SetGeometry(
                {{0, 0, 0}, {10, 0, 0}, {0, 10, 0}, {0, 0, 10}},
                {CMesh3D::Face{0, 2, 1}, CMesh3D::Face{0, 1, 3},
                 CMesh3D::Face{0, 3, 2}, CMesh3D::Face{1, 2, 3}})
                && open_sheet.SetGeometry(
                    {{0.1f, 0, 0}, {30, 0, 0}, {0.1f, 30, 0}},
                    {CMesh3D::Face{0, 1, 2}}),
            "Could not create open-edge-only welding regression.");
    for (size_t index = 0; index < closed_solid.GetFaces().size(); ++index)
        closed_solid.GetFaces()[index].sourceFaceId = static_cast<int>(index);
    for (bool reverse_order : {false, true}) {
        auto boundary_only = CMesh3D::CreateWelded(
            reverse_order
                ? std::vector<const CMesh3D*>{&open_sheet, &closed_solid}
                : std::vector<const CMesh3D*>{&closed_solid, &open_sheet},
            &welded_vertex_count);
        require(boundary_only && welded_vertex_count == 0
                    && boundary_only->GetVertices().size() == 7
                    && boundary_only->GetFaces().size() == 5,
                "Welding merged an open-edge vertex with a closed solid.");
    }

    DxfIO dxf;
    require(dxf.Export(base + ".dxf", document, error), "DXF export failed: " + error);
    {
        std::ifstream stream(base + ".dxf");
        const std::string exported{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        require(exported.find("\nLWPOLYLINE\n") != std::string::npos
                    && exported.find("\n42\n") != std::string::npos,
                "DXF export did not preserve the sketch as one bulged polyline.");
        require(exported.find("\nTEXT\n") != std::string::npos
                    && exported.find("\nText-20\n") != std::string::npos,
                "DXF export lost drawing text.");
        require(exported.find("\nHello World\n") != std::string::npos
                    && exported.find("\nHow are you?\n") != std::string::npos
                    && exported.find("Hello World\nHow are you?") == std::string::npos,
                "DXF export wrote a multiline value into a single TEXT group.");
    }
    std::vector<std::unique_ptr<CAlfaObject>> dxf_objects;
    require(dxf.Import(base + ".dxf", dxf_objects, error), "DXF import failed: " + error);
    require(dxf_objects.size() >= 2, "DXF round-trip lost curve or mesh geometry.");
    bool found_bulged_sketch = false;
    bool found_text = false;
    bool found_first_multiline_row = false;
    bool found_second_multiline_row = false;
    for (const auto& object : dxf_objects) {
        if (const auto* text = dynamic_cast<const CDrawingText*>(object.get())) {
            found_text |= text->GetText() == "Text-20"
                && std::abs(text->GetHeight() - 20.0) < 1.0e-6
                && text->GetLineStyle() == "HIDDEN"
                && std::abs(text->GetLineWidth() - 0.35) < 1.0e-6
                && text->GetFontFamily() == "Courier New";
            found_first_multiline_row |= text->GetText() == "Hello World";
            found_second_multiline_row |= text->GetText() == "How are you?";
        }
        const auto* sketch = dynamic_cast<const CSmartLine*>(object.get());
        if (!sketch || sketch->GetNumLines() != 2) continue;
        if (sketch->GetLine(0)->GetType() == LinkLineType::Segment
            && sketch->GetLine(1)->GetType() == LinkLineType::Arc) {
            found_bulged_sketch = true;
            break;
        }
    }
    require(found_bulged_sketch,
            "Bulged DXF polyline did not round-trip as one analytic sketch.");
    require(found_text, "DXF text, line style, or line weight did not round-trip.");
    require(found_first_multiline_row && found_second_multiline_row,
            "Multiline DXF text rows did not round-trip.");
    {
        const std::string malformed_path = base + "_legacy_multiline.dxf";
        std::ofstream stream(malformed_path);
        stream << "0\nSECTION\n2\nENTITIES\n0\nTEXT\n8\n0\n"
               << "10\n10\n20\n20\n30\n0\n40\n8\n1\n"
               << "Hello World\nHow are you?\n0\nENDSEC\n0\nEOF\n";
        stream.close();
        std::vector<std::unique_ptr<CAlfaObject>> recovered;
        require(dxf.Import(malformed_path, recovered, error),
                "Could not recover the legacy multiline TEXT export: " + error);
        bool recovered_text = false;
        for (const auto& object : recovered) {
            const auto* text = dynamic_cast<const CDrawingText*>(object.get());
            recovered_text |= text && text->GetText() == "Hello World\nHow are you?";
        }
        require(recovered_text, "Legacy multiline TEXT content was not recovered.");
    }
    bool found_filleted_sketch = false;
    for (const auto& object : dxf_objects) {
        const auto* sketch = dynamic_cast<const CSmartLine*>(object.get());
        if (!sketch || sketch->GetNumLines() != 4
            || sketch->GetNumFillets() != 2 || !sketch->IsClosed()) continue;
        int arcs = 0;
        for (std::size_t index = 0; index < sketch->GetNumLines(); ++index) {
            if (sketch->GetLine(index)->GetType() == LinkLineType::Arc) ++arcs;
        }
        if (arcs == 1) {
            found_filleted_sketch = true;
            break;
        }
    }
    require(found_filleted_sketch,
            "Two fillets and the connected arc did not round-trip in one polyline.");

    const std::string analytic_dxf = base + "_analytic.dxf";
    {
        std::ofstream stream(analytic_dxf);
        stream
            << "0\nSECTION\n2\nENTITIES\n"
            << "0\nCIRCLE\n8\nCurves\n10\n25\n20\n30\n30\n4\n40\n12\n"
            << "0\nARC\n8\nCurves\n10\n-10\n20\n5\n30\n2\n40\n8\n"
            << "50\n30\n51\n210\n0\nENDSEC\n0\nEOF\n";
    }
    std::vector<std::unique_ptr<CAlfaObject>> analytic_objects;
    require(dxf.Import(analytic_dxf, analytic_objects, error),
            "Analytic DXF import failed: " + error);
    require(analytic_objects.size() == 2,
            "Analytic DXF import did not create both curves.");
    const auto* circle = dynamic_cast<const CSmartLine*>(analytic_objects[0].get());
    const auto* arc = dynamic_cast<const CSmartLine*>(analytic_objects[1].get());
    require(circle && circle->IsClosed() && circle->GetNumLines() == 2,
            "DXF CIRCLE was not preserved as two analytic half-arcs.");
    require(arc && !arc->IsClosed() && arc->GetNumLines() == 1,
            "DXF ARC was not preserved as one analytic arc.");
    require(circle->GetLine(0)->GetType() == LinkLineType::Arc
                && circle->GetLine(1)->GetType() == LinkLineType::Arc
                && arc->GetLine(0)->GetType() == LinkLineType::Arc,
            "Imported DXF curves lost their analytic arc type.");

    const std::string connected_dxf = base + "_connected.dxf";
    {
        std::ofstream stream(connected_dxf);
        stream
            << "0\nSECTION\n2\nENTITIES\n"
            << "0\nLINE\n8\nOutline\n10\n0\n20\n0\n30\n0\n"
               "11\n20\n21\n0\n31\n0\n"
            << "0\nLINE\n8\nOutline\n10\n20\n20\n20\n30\n0\n"
               "11\n0\n21\n20\n31\n0\n"
            << "0\nARC\n8\nOutline\n10\n20\n20\n10\n30\n0\n40\n10\n"
               "50\n270\n51\n90\n"
            << "0\nENDSEC\n0\nEOF\n";
    }
    std::vector<std::unique_ptr<CAlfaObject>> connected_objects;
    require(dxf.Import(connected_dxf, connected_objects, error),
            "Connected analytic DXF import failed: " + error);
    require(connected_objects.size() == 1,
            "Touching DXF LINE and ARC entities were not joined into one sketch.");
    const auto* connected = dynamic_cast<const CSmartLine*>(
        connected_objects.front().get());
    require(connected && connected->GetNumLines() == 3,
            "Joined DXF sketch lost analytic segments.");
    int segment_count = 0;
    int arc_count = 0;
    for (std::size_t index = 0; index < connected->GetNumLines(); ++index) {
        if (connected->GetLine(index)->GetType() == LinkLineType::Arc) {
            ++arc_count;
        } else {
            ++segment_count;
        }
    }
    require(segment_count == 2 && arc_count == 1,
            "Joined DXF sketch changed LINE or ARC segment types.");

    const std::string sampled_arc_dxf = base + "_sampled_arc.dxf";
    {
        constexpr double pi = 3.14159265358979323846;
        std::ofstream stream(sampled_arc_dxf);
        stream << "0\nSECTION\n2\nENTITIES\n0\nPOLYLINE\n8\nCurves\n"
               << "66\n1\n70\n8\n";
        for (int index = 0; index < 32; ++index) {
            const double angle = (20.0 + 190.0 * index / 31.0) * pi / 180.0;
            stream << "0\nVERTEX\n8\nCurves\n"
                   << "10\n" << 15.0 + 40.0 * std::cos(angle) << '\n'
                   << "20\n" << -8.0 + 40.0 * std::sin(angle) << '\n'
                   << "30\n3\n70\n32\n";
        }
        stream << "0\nSEQEND\n0\nENDSEC\n0\nEOF\n";
    }
    std::vector<std::unique_ptr<CAlfaObject>> sampled_arc_objects;
    require(dxf.Import(sampled_arc_dxf, sampled_arc_objects, error),
            "Sampled legacy arc DXF import failed: " + error);
    require(sampled_arc_objects.size() == 1,
            "Sampled legacy arc DXF produced an unexpected object count.");
    const auto* recovered_arc = dynamic_cast<const CSmartLine*>(
        sampled_arc_objects.front().get());
    require(recovered_arc && recovered_arc->GetNumLines() == 1
                && recovered_arc->GetLine(0)->GetType() == LinkLineType::Arc,
            "Sampled legacy POLYLINE was not recovered as an analytic arc.");

    auto eps_bezier = std::make_unique<CSmartLine>("EPS Bezier");
    require(eps_bezier->SetCoordinateSystem(
                {80.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 1.0})
                && eps_bezier->AddLine(
                    std::make_unique<CBezierSpline>(
                        CPoint3d(0.0, 0.0, 0.0),
                        CPoint3d(10.0, 25.0, 0.0),
                        CPoint3d(30.0, 0.0, 0.0),
                        CPoint3d(40.0, 0.0, 0.0)), false)
                && eps_bezier->AddLine(
                    std::make_unique<CLinkLine>(
                        CPoint3d(40.0, 0.0, 0.0),
                        CPoint3d(40.0, 30.0, 0.0)), true)
                && eps_bezier->AddFillet(0, 2.0),
            "Could not create the EPS Bezier source sketch.");
    document.AddObject(std::move(eps_bezier));

    EpsIO eps;
    require(eps.Export(base + ".eps", document, error), "EPS export failed: " + error);
    {
        std::ifstream stream(base + ".eps");
        const std::string exported{
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
        require(exported.find(" curveto\n") != std::string::npos,
                "EPS export flattened a cubic Bezier into line segments.");
    }
    std::vector<std::unique_ptr<CAlfaObject>> eps_objects;
    require(eps.Import(base + ".eps", eps_objects, error), "EPS import failed: " + error);
    require(!eps_objects.empty(), "EPS round-trip produced no vector paths.");
    bool found_eps_bezier = false;
    for (const auto& object : eps_objects) {
        const auto* sketch = dynamic_cast<const CSmartLine*>(object.get());
        if (!sketch) continue;
        for (std::size_t index = 0; index < sketch->GetNumLines(); ++index) {
            found_eps_bezier |=
                sketch->GetLine(index)->GetType() == LinkLineType::Bezier;
        }
    }
    require(found_eps_bezier,
            "EPS import flattened a cubic Bezier into line segments.");

    HpglIO hpgl;
    require(hpgl.Export(base + ".hpgl", document, error), "HPGL export failed: " + error);
    std::vector<std::unique_ptr<CAlfaObject>> hpgl_objects;
    require(hpgl.Import(base + ".hpgl", hpgl_objects, error), "HPGL import failed: " + error);
    require(!hpgl_objects.empty(), "HPGL round-trip produced no pen paths.");

    StlIO stl;
    require(stl.Export(base + ".stl", document, error), "STL export failed: " + error);
    std::vector<std::unique_ptr<CMesh3D>> stl_meshes;
    require(stl.Import(base + ".stl", stl_meshes, error), "STL import failed: " + error);
    require(stl_meshes.size() == 1 && stl_meshes.front()->GetFaces().size() == 1,
            "STL round-trip produced an unexpected triangle count.");

    TestThreeMfIO(directory.path());
    TestFbxIO(directory.path());
    TestGlbIO(directory.path());
    TestGlbExport(directory.path());
    std::cout << "DXF, EPS, HPGL, 3MF, and STL exchange round-trips passed.\n";
    return EXIT_SUCCESS;
}
