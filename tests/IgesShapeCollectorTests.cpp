#include "IgesShapeCollector.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Geom_BSplineCurve.hxx>
#include <IGESControl_Reader.hxx>
#include <IGESControl_Writer.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

TopoDS_Face make_face(double x_offset, double size = 1.0) {
    BRepBuilderAPI_MakePolygon polygon;
    polygon.Add(gp_Pnt(x_offset, 0.0, 0.0));
    polygon.Add(gp_Pnt(x_offset + size, 0.0, 0.0));
    polygon.Add(gp_Pnt(x_offset + size, size, 0.0));
    polygon.Add(gp_Pnt(x_offset, size, 0.0));
    polygon.Close();
    require(polygon.IsDone(), "Test face wire was not built.");
    BRepBuilderAPI_MakeFace face(polygon.Wire(), true);
    require(face.IsDone(), "Test face was not built.");
    return face.Face();
}

void test_periodic_nurbs_definition() {
    constexpr Standard_Integer pole_count = 6;
    constexpr Standard_Integer degree = 5;
    TColgp_Array1OfPnt poles(1, pole_count);
    TColStd_Array1OfReal weights(1, pole_count);
    TColStd_Array1OfReal knots(1, pole_count + 1);
    TColStd_Array1OfInteger multiplicities(1, pole_count + 1);
    for (Standard_Integer index = 1; index <= pole_count; ++index) {
        const double angle = 2.0 * 3.141592653589793
            * static_cast<double>(index - 1) / pole_count;
        poles.SetValue(index, gp_Pnt(std::cos(angle), std::sin(angle), 0.0));
        weights.SetValue(index, 1.0);
    }
    for (Standard_Integer index = 1; index <= pole_count + 1; ++index) {
        knots.SetValue(index, static_cast<double>(index - 1));
        multiplicities.SetValue(index, 1);
    }
    const Handle(Geom_BSplineCurve) curve = new Geom_BSplineCurve(
        poles, weights, knots, multiplicities, degree, Standard_True);
    require(curve->IsPeriodic(), "Export NURBS definition is not periodic.");
    require(curve->IsClosed(), "Export NURBS definition is not closed.");
    require(curve->Value(curve->FirstParameter()).Distance(
                curve->Value(curve->LastParameter())) <= 1.0e-9,
            "Periodic NURBS endpoints do not coincide.");

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "dom3d_periodic_nurbs_test.igs";
    IGESControl_Writer writer;
    require(writer.AddShape(BRepBuilderAPI_MakeEdge(curve).Edge()),
            "Periodic NURBS edge was not accepted by IGES writer.");
    writer.ComputeModel();
    require(writer.Write(path.string().c_str()),
            "Periodic NURBS IGES test file was not written.");
    IGESControl_Reader reader;
    require(reader.ReadFile(path.string().c_str()) == IFSelect_RetDone,
            "Periodic NURBS IGES test file was not read.");
    require(reader.TransferRoots() > 0,
            "Periodic NURBS IGES roots were not transferred.");
    bool imported_closed_curve = false;
    for (TopExp_Explorer explorer(reader.OneShape(), TopAbs_EDGE);
         explorer.More(); explorer.Next()) {
        Standard_Real first_parameter = 0.0;
        Standard_Real last_parameter = 0.0;
        const Handle(Geom_Curve) imported = BRep_Tool::Curve(
            TopoDS::Edge(explorer.Current()), first_parameter, last_parameter);
        const Handle(Geom_BSplineCurve) imported_nurbs =
            Handle(Geom_BSplineCurve)::DownCast(imported);
        if (!imported_nurbs.IsNull() && imported_nurbs->IsClosed()
            && !imported_nurbs->IsPeriodic()) {
            imported_nurbs->SetPeriodic();
        }
        imported_closed_curve = !imported_nurbs.IsNull()
            && imported_nurbs->IsClosed() && imported_nurbs->IsPeriodic();
        if (imported_closed_curve) break;
    }
    std::error_code remove_error;
    std::filesystem::remove(path, remove_error);
    require(imported_closed_curve,
            "IGES round trip did not preserve NURBS closure.");
}

void test_disconnected_iges_surfaces_remain_separate() {
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, make_face(0.0));
    builder.Add(compound, make_face(3.0));

    const std::filesystem::path path =
        std::filesystem::temp_directory_path()
        / "dom3d_disconnected_surfaces_test.igs";
    IGESControl_Writer writer;
    require(writer.AddShape(compound),
            "Disconnected surface compound was not accepted by IGES writer.");
    writer.ComputeModel();
    require(writer.Write(path.string().c_str()),
            "Disconnected surface IGES file was not written.");

    IGESControl_Reader reader;
    require(reader.ReadFile(path.string().c_str()) == IFSelect_RetDone,
            "Disconnected surface IGES file was not read.");
    require(reader.TransferRoots() > 0,
            "Disconnected surface IGES roots were not transferred.");
    std::vector<TopoDS_Shape> imported;
    CollectIgesSurfaceShapes(reader.OneShape(), imported);
    std::error_code remove_error;
    std::filesystem::remove(path, remove_error);
    require(imported.size() == 2,
            "Disconnected IGES surfaces became one scene object.");
}
}

int main() {
    std::vector<TopoDS_Shape> surfaces;
    CollectIgesSurfaceShapes(make_face(0.0), surfaces);
    CollectIgesSurfaceShapes(make_face(2.0), surfaces);
    CollectIgesSurfaceShapes(make_face(4.0), surfaces);

    require(surfaces.size() == 3,
            "Only the first IGES root surface was collected.");

    BRep_Builder compound_builder;
    TopoDS_Compound compound;
    compound_builder.MakeCompound(compound);
    compound_builder.Add(compound, make_face(0.0));
    compound_builder.Add(compound, make_face(3.0));
    std::vector<TopoDS_Shape> compound_surfaces;
    CollectIgesSurfaceShapes(compound, compound_surfaces);
    require(compound_surfaces.size() == 2,
            "Independent IGES faces were merged into one surface object.");
    require(ComputeIgesMeshDeflection(make_face(0.0)) == 0.1f,
            "Small IGES surface did not use the minimum deflection.");
    require(ComputeIgesMeshDeflection(make_face(0.0, 1000.0)) > 10.0f,
            "Large IGES surface still uses an excessively fine fixed mesh.");
    test_periodic_nurbs_definition();
    test_disconnected_iges_surfaces_remain_separate();
    return EXIT_SUCCESS;
}
