#include "solid/SheetBendShapeBuilder.h"
#include "solid/Solid.h"
#include "CAlfaDoc.h"
#include "CPolyline.h"
#include "Dom3DProjectSerializer.h"
#include "ui/ToolRegistry.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepGProp.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <QCoreApplication>
#include <QDir>
#include <iostream>
#include <stdexcept>
#include <cmath>

namespace {
void check(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
double volume(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}
bool inside(const TopoDS_Shape& shape, gp_Pnt p) {
    BRepClass3d_SolidClassifier classifier(shape, p, 1.e-6);
    return classifier.State() == TopAbs_IN || classifier.State() == TopAbs_ON;
}
void valid(const TopoDS_Shape& source, const TopoDS_Shape& result) {
    check(BRepCheck_Analyzer(result).IsValid(), "Invalid bent CAD");
    int solids = 0;
    for (TopExp_Explorer it(result, TopAbs_SOLID); it.More(); it.Next()) ++solids;
    check(solids == 1, "Bend left disconnected solids");
    const double before = volume(source), after = volume(result);
    std::cout << "volume=" << before << " -> " << after << std::endl;
    check(std::abs(before - after) < before * 1.e-5, "Bend lost or added material");
}
}

int TestSheetBend(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    std::string error;
    TopoDS_Shape flat = BRepPrimAPI_MakeBox(100, 60, 3).Shape();
    SheetBendParameters p;
    p.line_start_x = p.line_end_x = 50;
    p.line_end_y = 60;
    p.line_start_z = p.line_end_z = 3;
    for (bool side : {false, true}) {
        for (bool clockwise : {false, true}) {
            p.reverse_side = side;
            p.clockwise = clockwise;
            TopoDS_Shape result;
            check(BuildSheetBendShape(flat, p, result, error), error);
            valid(flat, result);
            check(inside(result, gp_Pnt(side ? 80 : 20, 30, 1.5)), "Fixed side moved");
            check(!inside(result, gp_Pnt(side ? 20 : 80, 30, 1.5)), "Moving side stayed flat");
        }
    }
    check(argc >= 3, "Missing Detail-1 fixture");
    const bool fourth = std::string(argv[1]) == "--test-sheet-bend-fourth";
    const bool third = std::string(argv[1]) == "--test-sheet-bend-third";
    {
        auto interior = p;
        interior.line_start_z = interior.line_end_z = 1.37241548;
        TopoDS_Shape result;
        check(BuildSheetBendShape(flat, interior, result, error), error);
        valid(flat, result);
        interior.line_start_z = interior.line_end_z = 10;
        check(BuildSheetBendShape(flat, interior, result, error), "Cannot project a parallel construction line onto the sheet");
        valid(flat, result);
        interior.line_end_z = 20;
        check(!BuildSheetBendShape(flat, interior, result, error), "Accepted a line oblique to the sheet");
    }
    {
        SheetBendParameters oversized = p;
        oversized.inner_radius = 1000;
        TopoDS_Shape rejected;
        check(!BuildSheetBendShape(flat, oversized, rejected, error),
              "An oversized bend created material beyond the flange");
    }
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room, load_error;
    ProjectViewState view;
    check(serializer.Load(QString::fromLocal8Bit(argv[2]), document, room, view, load_error),
          load_error.toStdString());
    CSolid* body = nullptr;
    size_t body_index = 0;
    const CPolyline* bend_line = nullptr;
    for (size_t i = 0; i < document.GetObjects().size(); ++i) {
        auto* object = document.GetObjects()[i].get();
        if (auto* solid = dynamic_cast<CSolid*>(object); solid && solid->IsVisible()) {
            body = solid;
            body_index = i;
        }
        if (auto* line = dynamic_cast<CPolyline*>(object);
            line && line->GetPointCount() == 2 && object->m_id == (fourth ? 94UL : (third ? 93UL : 91UL)))
            bend_line = line;
    }
    check(body && bend_line, "Missing sheet or left flange bend line");
    const auto a = bend_line->GetPoints().front(), b = bend_line->GetPoints().back();
    p.line_start_x = a.x; p.line_start_y = a.y; p.line_start_z = a.z;
    p.line_end_x = b.x; p.line_end_y = b.y; p.line_end_z = b.z;
    const TopoDS_Shape source = body->m_Shape;
    if (fourth) {
        SheetBendFrame frame;
        check(ResolveSheetBendFrame(source, p, frame, error), error);
        check(std::abs(frame.thickness - 3.0) < 1.e-5
                  && std::abs(frame.origin.Z() + 1.5) < 1.e-5
                  && frame.normal.Z() > 0.99,
              "Fourth bend picked a distant flange instead of the sheet base");
    }
    TopoDS_Shape fixed_clip = fourth
        ? BRepPrimAPI_MakeBox(gp_Pnt(-500, 200, -500), 520, 600, 1000).Shape()
        : third
        ? BRepPrimAPI_MakeBox(gp_Pnt(-500, 200, -500), 1000, 240, 1000).Shape()
        : BRepPrimAPI_MakeBox(gp_Pnt(-200, 200, -500), 700, 500, 1000).Shape();
    const TopoDS_Shape source_fixed = BRepAlgoAPI_Common(source, fixed_clip).Shape();
    bool found_left = false;
    for (bool side : {false, true}) {
        p.reverse_side = side;
        p.clockwise = true;
        TopoDS_Shape result;
        const bool built = BuildSheetBendShape(source, p, result, error);
        std::cout << "Detail side=" << side << " built=" << built << " " << error << std::endl;
        if (!built) continue;
        const TopoDS_Shape fixed = BRepAlgoAPI_Common(result, fixed_clip).Shape();
        const double removed = volume(BRepAlgoAPI_Cut(source_fixed, fixed).Shape());
        const double added = volume(BRepAlgoAPI_Cut(fixed, source_fixed).Shape());
        std::cout << "fixed removed=" << removed << " added=" << added << std::endl;
        if (std::abs(removed) > 1.e-3 || std::abs(added) > 1.e-3) continue;
        valid(source, result);
        found_left = true;
        body->m_Shape = result;
        body->SetParametricOperation(body->GetOperationTree().size(), "SolidSheetBend", "Sheet Bend",
            {{"point1.x", a.x}, {"point1.y", a.y}, {"point1.z", a.z},
             {"point2.x", b.x}, {"point2.y", b.y}, {"point2.z", b.z},
             {"radius", p.inner_radius}, {"angle", p.angle_degrees}, {"direction", 0},
             {"side", side ? 1.0 : 0.0}});
        const auto* operation = body->GetOperation(body->GetNumOperations() - 1);
        ToolRegistry registry;
        auto active = registry.ActiveObjectFromDocument(body_index, *body, body->GetNumOperations() - 1, &document);
        bool side_saved = false;
        for (const auto& value : active.parameters)
            if (value.id == "side") side_saved = (value.value > 0.5) == side;
        check(operation && side_saved, "Moving side was not restored for editing");
        check(registry.ReplayOperations(body_index, document), "Cannot replay both sheet bends");
        body = dynamic_cast<CSolid*>(document.GetObjects()[body_index].get());
        check(body != nullptr, "Replayed sheet disappeared");
        check(std::abs(volume(BRepAlgoAPI_Cut(result, body->m_Shape).Shape())) < 1.e-3
                  && std::abs(volume(BRepAlgoAPI_Cut(body->m_Shape, result).Shape())) < 1.e-3,
              "History replay changed the chosen bend side or geometry");
        if (argc >= 4) {
            QDir().mkpath(QString::fromLocal8Bit(argv[3]));
            check(body->ReBuldMesh(), "Cannot mesh bent Detail-1");
            const QString output = QString::fromLocal8Bit(argv[3]) + (fourth ? "/Detail-1-fourth-bent.dom3d" : (third ? "/Detail-1-third-bent.dom3d" : "/Detail-1-bent.dom3d"));
            check(serializer.Save(output, document, room, view, {}, load_error), load_error.toStdString());
        }
        break;
    }
    check(found_left, "Neither side bent the small left flange while preserving the main body");
    std::cout << "Sheet bend passed\n";
    return 0;
}
