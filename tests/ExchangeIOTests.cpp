#include "CAlfaDoc.h"
#include "CMesh3D.h"
#include "CPolyline.h"
#include "BezierSpline.h"
#include "DrawingText.h"
#include "ExchangeIO.h"
#include "ObjIO.h"
#include "SketchArcLine.h"
#include "SmartLine.h"

#include <QCoreApplication>
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

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir directory;
    require(directory.isValid(), "Could not create the temporary exchange directory.");

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
                && std::abs(welding_tolerance - 3.3f) < 0.001f,
            "Welding Vertex did not merge Mesh3D objects with LenMin / 3.0.");

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

    std::cout << "DXF, EPS, HPGL, and STL exchange round-trips passed.\n";
    return EXIT_SUCCESS;
}
