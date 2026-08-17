#include "IgesIO.h"
#include "IgesShapeCollector.h"

#include "CadCurve3D.h"
#include "CAlfaDoc.h"
#include "CBSpline.h"
#include "solid/SurfaceSet.h"
#include "solid/Solid.h"

#include <BRep_Builder.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Tool.hxx>
#include <Geom_BSplineCurve.hxx>
#include <Geom_Curve.hxx>
#include <Geom_TrimmedCurve.hxx>
#include <GeomConvert.hxx>
#include <IGESControl_Reader.hxx>
#include <IGESControl_Writer.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Precision.hxx>
#include <ShapeFix_Shape.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_MapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Shape.hxx>
#include <TopLoc_Location.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr int kCurveSampleCount = 96;

Handle(Geom_BSplineCurve) make_bspline_curve(
    const std::vector<CPoint3d>& points,
    int degree,
    const std::vector<double>* weights = nullptr,
    const std::vector<double>* expanded_knots = nullptr)
{
    const int point_count = static_cast<int>(points.size());
    if (point_count < 2) {
        return {};
    }
    degree = std::clamp(degree, 1, point_count - 1);
    TColgp_Array1OfPnt poles(1, point_count);
    for (int index = 0; index < point_count; ++index) {
        const CPoint3d& point = points[static_cast<size_t>(index)];
        poles.SetValue(index + 1, gp_Pnt(point.x, point.y, point.z));
    }

    std::vector<double> knot_values;
    std::vector<int> knot_multiplicities;
    const size_t required_knot_count =
        static_cast<size_t>(point_count + degree + 1);
    if (expanded_knots && expanded_knots->size() == required_knot_count) {
        for (double knot : *expanded_knots) {
            if (knot_values.empty()
                || std::abs(knot - knot_values.back()) > Precision::PConfusion()) {
                knot_values.push_back(knot);
                knot_multiplicities.push_back(1);
            } else {
                ++knot_multiplicities.back();
            }
        }
    } else {
        const int interior_count = point_count - degree - 1;
        const int unique_knot_count = interior_count + 2;
        for (int knot = 0; knot < unique_knot_count; ++knot) {
            knot_values.push_back(
                static_cast<double>(knot) / static_cast<double>(unique_knot_count - 1));
            knot_multiplicities.push_back(
                knot == 0 || knot + 1 == unique_knot_count ? degree + 1 : 1);
        }
    }
    TColStd_Array1OfReal knots(1, static_cast<Standard_Integer>(knot_values.size()));
    TColStd_Array1OfInteger multiplicities(
        1, static_cast<Standard_Integer>(knot_multiplicities.size()));
    for (size_t knot = 0; knot < knot_values.size(); ++knot) {
        knots.SetValue(static_cast<Standard_Integer>(knot + 1), knot_values[knot]);
        multiplicities.SetValue(
            static_cast<Standard_Integer>(knot + 1), knot_multiplicities[knot]);
    }

    if (!weights) {
        return new Geom_BSplineCurve(
            poles, knots, multiplicities, degree, Standard_False);
    }

    TColStd_Array1OfReal occ_weights(1, point_count);
    for (int index = 0; index < point_count; ++index) {
        const double weight = static_cast<size_t>(index) < weights->size()
            ? (*weights)[static_cast<size_t>(index)] : 1.0;
        occ_weights.SetValue(index + 1, std::max(weight, 1.0e-6));
    }
    return new Geom_BSplineCurve(
        poles, occ_weights, knots, multiplicities, degree, Standard_False);
}

Handle(Geom_BSplineCurve) make_periodic_bspline_curve(
    const std::vector<CPoint3d>& source_points,
    int degree,
    const std::vector<double>& source_weights)
{
    size_t point_count = source_points.size();
    if (point_count > 3) {
        const CPoint3d& first = source_points.front();
        const CPoint3d& last = source_points.back();
        const double dx = first.x - last.x;
        const double dy = first.y - last.y;
        const double dz = first.z - last.z;
        if (dx * dx + dy * dy + dz * dz <= 1.0e-20) --point_count;
    }
    if (point_count < 3) return {};
    degree = std::clamp(degree, 1, static_cast<int>(point_count) - 1);

    TColgp_Array1OfPnt poles(1, static_cast<Standard_Integer>(point_count));
    TColStd_Array1OfReal weights(1, static_cast<Standard_Integer>(point_count));
    TColStd_Array1OfReal knots(
        1, static_cast<Standard_Integer>(point_count + 1));
    TColStd_Array1OfInteger multiplicities(
        1, static_cast<Standard_Integer>(point_count + 1));
    for (size_t index = 0; index < point_count; ++index) {
        const CPoint3d& point = source_points[index];
        const Standard_Integer occ_index = static_cast<Standard_Integer>(index + 1);
        poles.SetValue(occ_index, gp_Pnt(point.x, point.y, point.z));
        weights.SetValue(occ_index, index < source_weights.size()
            ? std::max(source_weights[index], 1.0e-6) : 1.0);
    }
    for (size_t index = 0; index <= point_count; ++index) {
        const Standard_Integer occ_index = static_cast<Standard_Integer>(index + 1);
        knots.SetValue(occ_index, static_cast<double>(index));
        multiplicities.SetValue(occ_index, 1);
    }
    return new Geom_BSplineCurve(
        poles, weights, knots, multiplicities, degree, Standard_True);
}

TopoDS_Shape make_export_curve_shape(const CBSpline& source)
{
    const std::vector<CPoint3d>& points = source.GetPoints();
    if (points.size() < 2) {
        return {};
    }

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    const auto add_curve = [&builder, &compound](
                               const Handle(Geom_BSplineCurve)& curve) {
        if (curve.IsNull()) return false;
        const TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(curve);
        if (edge.IsNull()) return false;
        builder.Add(compound, edge);
        return true;
    };

    if (source.GetCurveType() == SplineCurveType::Bezier) {
        if (source.IsBezierChain()) {
            const size_t segment_count = (points.size() - 1) / 3;
            for (size_t segment = 0; segment < segment_count; ++segment) {
                const size_t first = segment * 3;
                add_curve(make_bspline_curve(
                    {points[first], points[first + 1],
                     points[first + 2], points[first + 3]},
                    3));
            }
        } else {
            // Compatibility with Bezier objects saved by older versions.
            add_curve(make_bspline_curve(
                points, static_cast<int>(points.size()) - 1));
        }
        return compound;
    }

    if (source.GetCurveType() == SplineCurveType::Nurbs) {
        Handle(Geom_BSplineCurve) curve;
        if (source.IsClosed() && !source.GetKnots().empty()) {
            curve = make_bspline_curve(
                points, source.GetDegree(), &source.GetWeights(),
                &source.GetKnots());
            if (!curve.IsNull() && curve->IsClosed() && !curve->IsPeriodic()) {
                curve->SetPeriodic();
            }
        } else if (source.IsClosed()) {
            curve = make_periodic_bspline_curve(
                points, source.GetDegree(), source.GetWeights());
        } else {
            curve = make_bspline_curve(
                points, source.GetDegree(), &source.GetWeights(),
                &source.GetKnots());
        }
        add_curve(curve);
        return compound;
    }

    if (!source.IsClosed()) {
        add_curve(make_bspline_curve(points, source.GetDegree()));
        return compound;
    }

    // Dom3D's closed B-Spline is a uniform Catmull-Rom curve.  Convert every
    // span exactly to a cubic Bezier (and therefore to a cubic B-Spline).
    const size_t count = points.size();
    for (size_t segment = 0; segment < count; ++segment) {
        const CPoint3d& p0 = points[(segment + count - 1) % count];
        const CPoint3d& p1 = points[segment];
        const CPoint3d& p2 = points[(segment + 1) % count];
        const CPoint3d& p3 = points[(segment + 2) % count];
        const std::vector<CPoint3d> bezier{
            p1,
            CPoint3d(p1.x + (p2.x - p0.x) / 6.0,
                     p1.y + (p2.y - p0.y) / 6.0,
                     p1.z + (p2.z - p0.z) / 6.0),
            CPoint3d(p2.x - (p3.x - p1.x) / 6.0,
                     p2.y - (p3.y - p1.y) / 6.0,
                     p2.z - (p3.z - p1.z) / 6.0),
            p2};
        add_curve(make_bspline_curve(bezier, 3));
    }
    return compound;
}

void collect_top_level_shapes(const TopoDS_Shape& shape, std::vector<TopoDS_Shape>& shapes)
{
    if (shape.IsNull()) {
        return;
    }

    const TopAbs_ShapeEnum shape_type = shape.ShapeType();
    if (shape_type != TopAbs_COMPOUND && shape_type != TopAbs_COMPSOLID) {
        shapes.push_back(shape);
        return;
    }

    const size_t before = shapes.size();
    for (TopoDS_Iterator iterator(shape); iterator.More(); iterator.Next()) {
        collect_top_level_shapes(iterator.Value(), shapes);
    }
    if (shapes.size() == before) {
        shapes.push_back(shape);
    }
}

std::unique_ptr<CSurfaceSet> make_imported_surface(const TopoDS_Shape& source_shape, int index, int count)
{
    TopoDS_Shape shape = source_shape;
    auto loaded = std::make_unique<CSurfaceSet>(shape);
    loaded->SetName(count > 1 ? "Imported IGES Surface Set " + std::to_string(index) : "Imported IGES Surface Set");
    loaded->SetGroupName(count > 1 ? "Surfaces from IGES" : "");
    loaded->SetColor(kDefaultSolidObjectColor);
    loaded->InitSurfaces();
    loaded->InitEdges();

    // CSolid::BuldMesh converts this value to an absolute linear deflection
    // by dividing it by ten.  A fixed 0.1 therefore requests a 0.01-unit
    // mesh even for IGES surfaces hundreds or thousands of units wide.  On
    // large NURBS patches that creates an excessive triangulation and blocks
    // the UI for many seconds.  Use a scale-aware 0.1% model deflection.
    loaded->BuldMesh(ComputeIgesMeshDeflection(shape));
    return loaded;
}

TopoDS_Shape fix_shape(const TopoDS_Shape& source_shape)
{
    if (source_shape.IsNull()) {
        return source_shape;
    }

    Handle(ShapeFix_Shape) fixer = new ShapeFix_Shape(source_shape);
    fixer->Perform();
    TopoDS_Shape fixed_shape = fixer->Shape();
    return fixed_shape.IsNull() ? source_shape : fixed_shape;
}

TopTools_MapOfShape collect_face_edges(const TopoDS_Shape& shape)
{
    TopTools_MapOfShape face_edges;
    for (TopExp_Explorer face_explorer(shape, TopAbs_FACE); face_explorer.More(); face_explorer.Next()) {
        TopTools_IndexedMapOfShape edges;
        TopExp::MapShapes(face_explorer.Current(), TopAbs_EDGE, edges);
        for (Standard_Integer i = 1; i <= edges.Extent(); ++i) {
            face_edges.Add(edges.FindKey(i));
        }
    }
    return face_edges;
}

std::vector<Vec3> sample_edge(const TopoDS_Edge& edge)
{
    std::vector<Vec3> points;
    if (edge.IsNull()) {
        return points;
    }

    BRepAdaptor_Curve curve(edge);
    double first = curve.FirstParameter();
    double last = curve.LastParameter();
    if (Precision::IsNegativeInfinite(first) || Precision::IsPositiveInfinite(first)) {
        first = 0.0;
    }
    if (Precision::IsNegativeInfinite(last) || Precision::IsPositiveInfinite(last)) {
        last = 1.0;
    }
    if (last < first) {
        std::swap(first, last);
    }
    if (std::abs(last - first) <= Precision::PConfusion()) {
        return points;
    }

    points.reserve(kCurveSampleCount + 1);
    for (int i = 0; i <= kCurveSampleCount; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(kCurveSampleCount);
        const double parameter = first + (last - first) * t;
        const gp_Pnt p = curve.Value(parameter);
        points.push_back({static_cast<float>(p.X()), static_cast<float>(p.Y()), static_cast<float>(p.Z())});
    }

    return points;
}

std::unique_ptr<CAlfaObject> make_editable_nurbs_curve(
    const TopoDS_Edge& edge, int curve_index)
{
    Standard_Real first = 0.0;
    Standard_Real last = 0.0;
    TopLoc_Location location;
    const Handle(Geom_Curve) source = BRep_Tool::Curve(edge, location, first, last);
    if (source.IsNull()
        || Precision::IsNegativeInfinite(first)
        || Precision::IsPositiveInfinite(last)
        || last - first <= Precision::PConfusion()) {
        return {};
    }

    Handle(Geom_Curve) world_curve = Handle(Geom_Curve)::DownCast(source->Copy());
    if (world_curve.IsNull()) return {};
    if (!location.IsIdentity()) world_curve->Transform(location.Transformation());

    Handle(Geom_BSplineCurve) nurbs =
        Handle(Geom_BSplineCurve)::DownCast(world_curve);
    const bool source_periodic = !nurbs.IsNull() && nurbs->IsPeriodic();
    if (!source_periodic) {
        const Handle(Geom_TrimmedCurve) trimmed =
            new Geom_TrimmedCurve(world_curve, first, last);
        nurbs = GeomConvert::CurveToBSplineCurve(trimmed);
    }
    if (nurbs.IsNull() || nurbs->NbPoles() < 2) return {};
    const bool closed = nurbs->IsClosed();
    if (closed && !nurbs->IsPeriodic()) {
        // IGES type 126 preserves the closed flag, but OpenCascade commonly
        // reads it back as a closed non-periodic B-Spline.  Convert it to the
        // periodic representation used by Dom3D before extracting the poles.
        nurbs->SetPeriodic();
    }
    const bool preserve_closed = nurbs->IsPeriodic() || closed;
    if (preserve_closed && nurbs->IsPeriodic()) {
        // Store an exact clamped representation in CBSpline.  Its duplicated
        // endpoint and complete knot vector preserve the imported shape for
        // every degree, while the separate closed flag keeps the topology.
        nurbs->SetNotPeriodic();
    }

    auto result = std::make_unique<CBSpline>(
        "Imported IGES NURBS Curve " + std::to_string(curve_index));
    result->SetCurveType(SplineCurveType::Nurbs);
    result->SetDegree(nurbs->Degree());

    std::vector<double> weights;
    weights.reserve(static_cast<size_t>(nurbs->NbPoles()));
    for (Standard_Integer index = 1; index <= nurbs->NbPoles(); ++index) {
        const gp_Pnt pole = nurbs->Pole(index);
        result->AddPoint(CPoint3d(pole.X(), pole.Y(), pole.Z()));
        weights.push_back(nurbs->Weight(index));
    }
    result->SetCurveType(SplineCurveType::Nurbs);
    result->SetDegree(nurbs->Degree());
    result->SetWeights(std::move(weights));

    const TColStd_Array1OfReal& sequence = nurbs->KnotSequence();
    std::vector<double> knots;
    knots.reserve(static_cast<size_t>(sequence.Length()));
    for (Standard_Integer index = sequence.Lower();
         index <= sequence.Upper(); ++index) {
        knots.push_back(sequence.Value(index));
    }
    if (!result->SetKnots(std::move(knots))) return {};
    result->SetClosed(preserve_closed);

    result->SetGroupName("Curves from IGES");
    result->SetColor(kDefaultCurveColor);
    return result;
}

void collect_standalone_curves(const TopoDS_Shape& shape, int& curve_index, std::vector<std::unique_ptr<CAlfaObject>>& objects)
{
    const TopTools_MapOfShape face_edges = collect_face_edges(shape);
    for (TopExp_Explorer explorer(shape, TopAbs_EDGE); explorer.More(); explorer.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(explorer.Current());
        if (edge.IsNull() || face_edges.Contains(edge)) {
            continue;
        }

        try {
            std::unique_ptr<CAlfaObject> curve =
                make_editable_nurbs_curve(edge, curve_index);
            if (curve) {
                ++curve_index;
                objects.push_back(std::move(curve));
                continue;
            }
        } catch (const Standard_Failure&) {
            // Preserve unsupported analytic edge types as display curves.
        }

        std::vector<Vec3> points = sample_edge(edge);
        if (points.size() < 2) continue;
        auto curve = std::make_unique<CCadCurve3D>(
            "Imported IGES Curve " + std::to_string(curve_index++),
            std::move(points));
        curve->SetGroupName("Curves from IGES");
        curve->SetColor(kDefaultCurveColor);
        objects.push_back(std::move(curve));
    }
}
}

bool IgesIO::Import(const std::string& path, std::vector<std::unique_ptr<CAlfaObject>>& objects, std::string& error) const {
    objects.clear();

    IGESControl_Reader reader;
    const IFSelect_ReturnStatus status = reader.ReadFile(path.c_str());
    if (status != IFSelect_RetDone) {
        error = "Could not read IGES file.";
        return false;
    }

    try {
        std::vector<TopoDS_Shape> root_shapes;
        const Standard_Integer root_count = reader.NbRootsForTransfer();
        for (Standard_Integer root = 1; root <= root_count; ++root) {
            reader.ClearShapes();
            if (!reader.TransferOneRoot(root)) {
                continue;
            }
            for (Standard_Integer shape_index = 1; shape_index <= reader.NbShapes(); ++shape_index) {
                collect_top_level_shapes(fix_shape(reader.Shape(shape_index)), root_shapes);
            }
        }

        if (root_shapes.empty()) {
            reader.ClearShapes();
            const Standard_Integer transferred = reader.TransferRoots();
            if (transferred <= 0) {
                error = "IGES file does not contain transferable NURBS geometry.";
                return false;
            }

            TopoDS_Shape shape = reader.OneShape();
            if (shape.IsNull()) {
                error = "IGES file does not contain a valid shape.";
                return false;
            }

            collect_top_level_shapes(fix_shape(shape), root_shapes);
        }

        std::vector<TopoDS_Shape> surface_shapes;
        for (const TopoDS_Shape& root_shape : root_shapes) {
            CollectIgesSurfaceShapes(root_shape, surface_shapes);
        }
        const int surface_count = static_cast<int>(surface_shapes.size());
        for (int i = 0; i < surface_count; ++i) {
            objects.push_back(make_imported_surface(surface_shapes[static_cast<size_t>(i)], i + 1, surface_count));
        }

        int curve_index = 1;
        for (const TopoDS_Shape& root_shape : root_shapes) {
            collect_standalone_curves(root_shape, curve_index, objects);
        }

        if (objects.empty()) {
            error = "IGES file does not contain supported NURBS curves or surfaces.";
            return false;
        }
        return true;
    } catch (const Standard_Failure& failure) {
        error = failure.GetMessageString();
        if (error.empty()) {
            error = "OpenCascade failed while importing IGES.";
        }
        return false;
    }
}

bool IgesIO::Export(const std::string& path, const CAlfaDoc& document, std::string& error) const {
    int shape_count = 0;

    try {
        IGESControl_Writer writer;
        for (const auto& object : document.GetObjects()) {
            const auto* solid = dynamic_cast<const CSolid*>(object.get());
            if (solid && solid->IsVisible() && !solid->m_Shape.IsNull()) {
                if (!writer.AddShape(solid->m_Shape)) {
                    error = "Could not transfer visible shape to IGES writer.";
                    return false;
                }
                ++shape_count;
                continue;
            }

            const auto* spline = dynamic_cast<const CBSpline*>(object.get());
            if (spline && spline->IsVisible()) {
                const TopoDS_Shape curve_shape = make_export_curve_shape(*spline);
                if (curve_shape.IsNull()) {
                    continue;
                }
                if (!writer.AddShape(curve_shape)) {
                    error = "Could not transfer visible curve to IGES writer.";
                    return false;
                }
                ++shape_count;
            }
        }

        if (shape_count == 0) {
            error = "There are no visible solid, surface, B-Spline, Bezier, or NURBS objects to export.";
            return false;
        }

        writer.ComputeModel();
        if (!writer.Write(path.c_str())) {
            error = "Could not save IGES file.";
            return false;
        }
        return true;
    } catch (const Standard_Failure& failure) {
        error = failure.GetMessageString();
        if (error.empty()) {
            error = "OpenCascade failed while exporting IGES.";
        }
        return false;
    }
}
