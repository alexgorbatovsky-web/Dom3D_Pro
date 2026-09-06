#include "solid/HolePlacement.h"
#include "solid/Solid.h"
#include "ui/ToolRegistry.h"
#include "Dom3DProjectSerializer.h"
#include "UndoRedo.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}
void set(std::vector<ToolParameter>& parameters, const std::string& id, double value) {
    for (auto& parameter : parameters) if (parameter.id == id) { parameter.value = value; return; }
    check(false, "Missing hole test parameter.");
}
double value(const std::vector<ToolParameter>& parameters, const std::string& id) {
    for (const auto& parameter : parameters) if (parameter.id == id) return parameter.value;
    check(false, "Missing hole test value.");
    return 0;
}
double volume(const CSolid& solid) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(solid.m_Shape, properties);
    return properties.Mass();
}
}

void TestHolePlacement() {
    HoleFacePlacement placement;
    std::string error;
    const TopoDS_Face rectangle = BRepBuilderAPI_MakeFace(
        gp_Pln(gp_Pnt(0,0,0), gp_Dir(0,0,1)), 0, 100, 0, 20).Face();
    const gp_Pnt click(20,4,0);
    check(BuildHoleFacePlacement(rectangle, click, placement, error), error.c_str());
    check(placement.center.Distance(click) < 1.e-7, "The hole was not placed at the click.");
    check(std::fabs(placement.distances[0] - 4) < 1.e-7
        && std::fabs(placement.distances[1] - 20) < 1.e-7,
        "Auto placement chose opposite parallel edges instead of an independent pair.");
    check(std::fabs(placement.clearance - 4) < 1.e-7, "Wrong click clearance.");
    check(BuildHoleFacePlacement(rectangle, gp_Pnt(80,16,0), placement, error)
        && placement.center.Distance(gp_Pnt(80,16,0)) < 1.e-7,
        "A second click reused the old placement.");
    check(std::fabs(placement.starts[0].Y() - 20) < 1.e-7
        && std::fabs(placement.ends[0].Y() - 20) < 1.e-7
        && std::fabs(placement.starts[1].X() - 100) < 1.e-7
        && std::fabs(placement.ends[1].X() - 100) < 1.e-7,
        "Reference edges were not selected relative to the new click.");
    const auto reversed = TopoDS::Face(rectangle.Reversed());
    check(BuildHoleFacePlacement(reversed, click, placement, error) && placement.normal.Z() < -0.99,
          "Reversed face normal was lost.");
    gp_Trsf transform;
    transform.SetRotation(gp_Ax1(gp_Pnt(0,0,0), gp_Dir(1,2,3)), 0.7);
    transform.SetTranslationPart(gp_Vec(23,-17,12));
    const auto inclined = TopoDS::Face(BRepBuilderAPI_Transform(rectangle, transform, true).Shape());
    check(BuildHoleFacePlacement(inclined, click.Transformed(transform), placement, error), error.c_str());
    check(placement.center.Distance(click.Transformed(transform)) < 1.e-7,
          "Placement used world axes instead of the inclined face.");
    check(std::fabs(placement.distances[0] - 4) < 1.e-6
        && std::fabs(placement.distances[1] - 20) < 1.e-6, "Inclined edge distances changed.");
    BRepBuilderAPI_MakePolygon triangle;
    triangle.Add(gp_Pnt(0,0,0)); triangle.Add(gp_Pnt(12,0,0)); triangle.Add(gp_Pnt(0,6,0)); triangle.Close();
    check(BuildHoleFacePlacement(BRepBuilderAPI_MakeFace(triangle.Wire()).Face(), gp_Pnt(2,1,0), placement, error), error.c_str());
    check(placement.center.Distance(gp_Pnt(2,1,0)) < 1.e-7,
          "Triangle used its centroid rather than the click.");

    const auto cylinder = BRepPrimAPI_MakeCylinder(5, 10).Shape();
    for (TopExp_Explorer faces(cylinder, TopAbs_FACE); faces.More(); faces.Next()) {
        check(!BuildHoleFacePlacement(TopoDS::Face(faces.Current()), gp_Pnt(1,1,0), placement, error) && !error.empty(),
              "Cylinder side or circular face must not invent straight reference edges.");
    }
    const auto cutter = BRepPrimAPI_MakeCylinder(
        gp_Ax2(gp_Pnt(50,10,-1), gp_Dir(0,0,1)), 3, 2).Shape();
    const auto perforated = BRepAlgoAPI_Cut(rectangle, cutter).Shape();
    for (TopExp_Explorer faces(perforated, TopAbs_FACE); faces.More(); faces.Next()) {
        check(!BuildHoleFacePlacement(TopoDS::Face(faces.Current()), gp_Pnt(50,10,0), placement, error),
              "Auto placement accepted a center in an existing hole.");
        check(BuildHoleFacePlacement(TopoDS::Face(faces.Current()), click, placement, error)
            && placement.center.Distance(click) < 1.e-7,
            "Valid click on a perforated face was replaced by its empty centroid.");
    }

    ToolRegistry tools;
    const auto* box_tool = tools.Find("SolidBox");
    const auto* hole_tool = tools.Find("SolidHole");
    check(box_tool && hole_tool, "Hole test tools are missing.");
    for (int type : {0, 1}) {
        CAlfaDoc document;
        auto box_parameters = box_tool->defaults;
        set(box_parameters, "width", 100); set(box_parameters, "height", 60); set(box_parameters, "depth", 20);
        const auto box = tools.CreateParametricObject("SolidBox", document, box_parameters);
        auto* body = dynamic_cast<CSolid*>(document.GetObjects().at(box.object_index).get());
        check(body != nullptr, "Could not create hole test box.");
        document.EnsureObjectId(*body);
        const auto body_id = body->m_id;
        Vec3 minimum{}, maximum{};
        check(body->GetBounds(minimum, maximum), "Missing box bounds.");
        int face_index = -1;
        for (int i = 0; i < body->GetNumSurfaces(); ++i) {
            Vec3 center{}, normal{};
            if (body->GetFaceCenterAndNormal(i, center, normal) && normal.z > 0.99f) face_index = i;
        }
        check(face_index >= 0, "Missing top face.");
        auto parameters = hole_tool->defaults;
        set(parameters, "diameter", 8); set(parameters, "hole_type", type); set(parameters, "depth", 5);
        const double before = volume(*body);
        const CPoint3d picked(minimum.x + 35, minimum.y + 24, maximum.z);
        auto rejected_parameters = parameters;
        check(!tools.PrepareHoleOnFace(document, body_id, -1, picked, rejected_parameters, error)
            && !error.empty() && volume(*body) == before,
            "Invalid face selection changed the body or was accepted.");
        check(tools.PrepareHoleOnFace(document, body_id, face_index, picked, parameters, error), error.c_str());
        check(body->GetNumOperations() == 1 && std::fabs(volume(*body) - before) < 1.e-7,
              "Read-only hole placement changed the body.");
        check(value(parameters, "hole.refs.valid") == 1, "Automatic references were not saved.");
        check(std::fabs(value(parameters, "hole.center.x") - picked.x) < 1.e-5
            && std::fabs(value(parameters, "hole.center.y") - picked.y) < 1.e-5,
            "One-click preparation replaced the picked coordinates with the face center.");
        check(std::fabs(value(parameters, "hole.center.z") - maximum.z) < 1.e-5,
              "Hole center is not on the clicked face.");
        auto oversized = hole_tool->defaults;
        set(oversized, "diameter", 10000);
        check(tools.PrepareHoleOnFace(document, body_id, face_index, picked, oversized, error)
            && value(oversized, "diameter") <= 24.001, "Initial diameter did not fit at the click.");
        auto fitting = hole_tool->defaults;
        set(fitting, "diameter", 40);
        check(tools.PrepareHoleOnFace(document, body_id, face_index, picked, fitting, error)
            && value(fitting, "diameter") == 40, "A fitting remembered diameter was unnecessarily reduced.");
        CUndoRedo history(document);
        check(history.BeginChange(), "Could not begin hole undo transaction.");
        const auto operation = tools.ApplyHole(document, body_id, parameters);
        check(!operation.tool_id.empty() && body->GetNumOperations() == 2,
              "One-click hole did not create exactly one history operation.");
        check(BRepCheck_Analyzer(body->m_Shape).IsValid(), "One-click hole produced an invalid solid.");
        const double expected_removed = std::acos(-1.0) * 16 * (type == 0 ? 20 : 5);
        check(std::fabs((before - volume(*body)) - expected_removed) < 0.01,
              "One-click hole diameter, outward normal or depth is incorrect.");
        check(history.CommitChange("Create hole") && history.Undo(), "Hole undo failed.");
        body = dynamic_cast<CSolid*>(document.FindObjectById(body_id));
        check(body && body->GetNumOperations() == 1 && std::fabs(volume(*body) - before) < 0.01,
              "Undo did not restore the original body.");
        check(history.Redo(), "Hole redo failed.");
        body = dynamic_cast<CSolid*>(document.FindObjectById(body_id));
        check(body && body->GetNumOperations() == 2
            && std::fabs((before - volume(*body)) - expected_removed) < 0.01,
            "Redo did not restore the centered hole.");

        auto moved = operation;
        set(moved.parameters, "hole.distance1", value(parameters, "hole.distance1") - 3);
        tools.Rebuild(moved, document);
        body = dynamic_cast<CSolid*>(document.FindObjectById(body_id));
        check(body && BRepCheck_Analyzer(body->m_Shape).IsValid(), "Distance edit failed.");
        const gp_Pnt old_center(value(parameters, "hole.center.x"), value(parameters, "hole.center.y"), maximum.z);
        bool moved_circle = false;
        for (TopExp_Explorer edges(body->m_Shape, TopAbs_EDGE); edges.More(); edges.Next()) {
            BRepAdaptor_Curve curve(TopoDS::Edge(edges.Current()));
            if (curve.GetType() == GeomAbs_Circle && std::fabs(curve.Circle().Radius() - 4) < 1.e-6) {
                const gp_Pnt center = curve.Circle().Location();
                if (std::fabs(center.Z() - maximum.z) < 1.e-5 && std::fabs(center.Distance(old_center) - 3) < 1.e-4)
                    moved_circle = true;
            }
        }
        check(moved_circle && body->GetNumOperations() == 2,
              "Editing an automatic edge distance did not move the same hole operation.");
        QTemporaryDir directory;
        check(directory.isValid(), "Could not create hole round-trip directory.");
        Dom3DProjectSerializer serializer;
        QString io_error, room;
        ProjectViewState view;
        const QString path = directory.filePath("hole.dom3d");
        check(serializer.Save(path, document, {}, view, {}, io_error), io_error.toLocal8Bit().constData());
        CAlfaDoc reloaded;
        check(serializer.Load(path, reloaded, room, view, io_error), io_error.toLocal8Bit().constData());
        check(tools.ReplayOperations(box.object_index, reloaded), "Saved hole history did not replay.");
        const auto* restored = dynamic_cast<const CSolid*>(reloaded.FindObjectById(body_id));
        check(restored && restored->GetNumOperations() == 2
            && std::fabs(volume(*restored) - volume(*body)) < 0.01,
            "Saving and reopening lost the automatic hole operation.");
    }
    std::cout << "One-click hole placement tests passed.\n";
}
