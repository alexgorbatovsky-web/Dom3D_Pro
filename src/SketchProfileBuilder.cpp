#include "SketchProfileBuilder.h"

#include "BezierSpline.h"
#include "SmartLine.h"
#include "SketchArcLine.h"

#ifdef Coord
#undef Coord
#endif
#ifdef String
#undef String
#endif
#ifdef LPCTSTR
#undef LPCTSTR
#endif
#ifdef Pixel
#undef Pixel
#endif

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <GC_MakeArcOfCircle.hxx>
#include <Geom_BezierCurve.hxx>
#include <Standard_Failure.hxx>
#include <ShapeFix_Wire.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <vector>

namespace {
constexpr double kProfileTolerance = 1.0e-8;
constexpr double kWireConnectionTolerance = 1.0e-6;

gp_Pnt to_occ_point(const CSmartLine& sketch, const CPoint3d& local) {
    const CPoint3d world = sketch.LocalToWorld(local);
    return gp_Pnt(world.x, world.y, world.z);
}

double point_distance(const CPoint3d& first, const CPoint3d& second) {
    const double dx = first.x - second.x;
    const double dy = first.y - second.y;
    const double dz = first.z - second.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool add_trimmed_arc_edge(BRepBuilderAPI_MakeWire& wire_builder,
                          const CSmartLine& sketch,
                          const CSketchArcLine& arc,
                          const CPoint3d& start,
                          const CPoint3d& end,
                          double start_parameter,
                          double end_parameter) {
    if (end_parameter - start_parameter <= kProfileTolerance) {
        return false;
    }

    // Fillets trim the neighbouring sketch curves.  The display geometry
    // already honours those parameters; the OCC wire must use the same
    // shortened arc instead of the original full arc, otherwise the fillet
    // edges overlap it and the profile is disconnected/self-intersecting.
    const CPoint3d middle =
        arc.GetPoint((start_parameter + end_parameter) * 0.5);
    GC_MakeArcOfCircle arc_builder(
        to_occ_point(sketch, start),
        to_occ_point(sketch, middle),
        to_occ_point(sketch, end));
    if (!arc_builder.IsDone()) {
        return false;
    }
    BRepBuilderAPI_MakeEdge edge(arc_builder.Value());
    if (!edge.IsDone()) {
        return false;
    }
    wire_builder.Add(edge.Edge());
    return true;
}

bool add_fillet_edge(BRepBuilderAPI_MakeWire& wire_builder,
                     const CSmartLine& sketch,
                     const FilletGeometry& geometry) {
    if (!geometry.valid
        || std::abs(geometry.signed_angle) <= kProfileTolerance) {
        return false;
    }

    // Do not use CFillet::Sample() for the edge endpoints.  Its last point is
    // obtained by rotating the first radius around the numerically solved
    // centre.  For an arc-to-fillet join the two solved radii may differ by a
    // few tenths of a micron, so that reconstructed point is not exactly the
    // tangent_on_second used by the following edge.  A wire can then be
    // accepted by MakeWire but still produce an invalid, uncapped prism.
    const double start_x = geometry.tangent_on_first.x - geometry.center.x;
    const double start_y = geometry.tangent_on_first.y - geometry.center.y;
    const double half_angle = geometry.signed_angle * 0.5;
    const double cosine = std::cos(half_angle);
    const double sine = std::sin(half_angle);
    const CPoint3d middle(
        geometry.center.x + start_x * cosine - start_y * sine,
        geometry.center.y + start_x * sine + start_y * cosine,
        geometry.center.z);

    GC_MakeArcOfCircle arc(
        to_occ_point(sketch, geometry.tangent_on_first),
        to_occ_point(sketch, middle),
        to_occ_point(sketch, geometry.tangent_on_second));
    if (!arc.IsDone()) {
        return false;
    }
    BRepBuilderAPI_MakeEdge edge(arc.Value());
    if (!edge.IsDone()) {
        return false;
    }
    wire_builder.Add(edge.Edge());
    return true;
}

bool fix_profile_wire(const TopoDS_Wire& source,
                      bool closed,
                      TopoDS_Wire& result) {
    if (source.IsNull()) {
        return false;
    }

    // Every curve is built independently by OpenCascade.  In particular,
    // the trimmed sketch arc and its neighbouring fillet receive distinct
    // vertices even though both endpoints originate from the same computed
    // tangent point.  After an arc grip is edited their coordinates can
    // differ by the solver tolerance, leaving a topologically open wire.
    // Sew only such numerical gaps before making the face; the deliberately
    // small tolerance cannot hide a genuinely disconnected sketch.
    ShapeFix_Wire fixer;
    fixer.Load(source);
    fixer.ClosedWireMode() = closed;
    fixer.FixReorder();
    fixer.FixConnected(kWireConnectionTolerance);
    if (closed) {
        fixer.FixClosed(kWireConnectionTolerance);
    }
    result = fixer.WireAPIMake();
    return !result.IsNull();
}
}

static bool BuildSketchProfileFaceImpl(const CSmartLine& sketch,
                                       bool allow_axis_closure,
                                       Vec3 axis_origin,
                                       Vec3 axis_direction,
                                       TopoDS_Face& face,
                                       Vec3& normal) {
    const bool close_to_axis = allow_axis_closure && !sketch.IsClosed();
    if ((!sketch.IsClosed() && !close_to_axis)
        || sketch.GetNumLines() < (close_to_axis ? 1u : 2u)) {
        return false;
    }
    const float axis_length_squared = dot(axis_direction, axis_direction);
    if (close_to_axis && axis_length_squared <= 1.0e-12f) {
        return false;
    }
    const auto project_to_axis =
        [axis_origin, axis_direction, axis_length_squared](const CPoint3d& point) {
            const Vec3 value{
                static_cast<float>(point.x),
                static_cast<float>(point.y),
                static_cast<float>(point.z)};
            const float parameter =
                dot(value - axis_origin, axis_direction)
                / axis_length_squared;
            const Vec3 projected =
                axis_origin + axis_direction * parameter;
            return CPoint3d(projected.x, projected.y, projected.z);
        };

    std::vector<CPoint3d> line_starts;
    std::vector<CPoint3d> line_ends;
    std::vector<double> line_start_parameters(sketch.GetNumLines(), 0.0);
    std::vector<double> line_end_parameters(sketch.GetNumLines(), 1.0);
    line_starts.reserve(sketch.GetNumLines());
    line_ends.reserve(sketch.GetNumLines());
    for (std::size_t index = 0; index < sketch.GetNumLines(); ++index) {
        const CLinkLine* line = sketch.GetLine(index);
        if (!line) {
            return false;
        }
        line_starts.push_back(line->GetStart());
        line_ends.push_back(line->GetEnd());
    }

    std::vector<const CFillet*> fillet_after_line(sketch.GetNumLines(), nullptr);
    for (std::size_t index = 0; index < sketch.GetNumFillets(); ++index) {
        const CFillet* fillet = sketch.GetFillet(index);
        if (!fillet
            || fillet->GetFirstLineIndex() >= sketch.GetNumLines()
            || fillet->GetSecondLineIndex() >= sketch.GetNumLines()) {
            return false;
        }
        const CLinkLine* first = sketch.GetLine(fillet->GetFirstLineIndex());
        const CLinkLine* second = sketch.GetLine(fillet->GetSecondLineIndex());
        const FilletGeometry geometry = fillet->Calculate(*first, *second);
        if (!geometry.valid) {
            return false;
        }
        line_ends[fillet->GetFirstLineIndex()] = geometry.tangent_on_first;
        line_starts[fillet->GetSecondLineIndex()] = geometry.tangent_on_second;
        line_end_parameters[fillet->GetFirstLineIndex()] = geometry.first_parameter;
        line_start_parameters[fillet->GetSecondLineIndex()] = geometry.second_parameter;
        fillet_after_line[fillet->GetFirstLineIndex()] = fillet;
    }

    std::vector<CPoint3d> orientation_points = sketch.GetProfilePointsWorld();
    if (close_to_axis && orientation_points.size() >= 2) {
        orientation_points.push_back(
            project_to_axis(orientation_points.back()));
        orientation_points.push_back(
            project_to_axis(orientation_points.front()));
    }
    if (orientation_points.size() < 3) {
        return false;
    }
    Vec3 newell{};
    for (std::size_t index = 0; index < orientation_points.size(); ++index) {
        const CPoint3d& current = orientation_points[index];
        const CPoint3d& next = orientation_points[(index + 1) % orientation_points.size()];
        newell.x += static_cast<float>((current.y - next.y) * (current.z + next.z));
        newell.y += static_cast<float>((current.z - next.z) * (current.x + next.x));
        newell.z += static_cast<float>((current.x - next.x) * (current.y + next.y));
    }
    const float normal_length = std::sqrt(dot(newell, newell));
    if (normal_length <= 0.0001f) {
        return false;
    }
    normal = newell * (1.0f / normal_length);

    try {
        BRepBuilderAPI_MakeWire wire_builder;
        for (std::size_t line_index = 0; line_index < sketch.GetNumLines(); ++line_index) {
            if (point_distance(line_starts[line_index], line_ends[line_index]) <= kProfileTolerance) {
                return false;
            }

            const CLinkLine* profile_line = sketch.GetLine(line_index);
            const auto* sketch_arc = dynamic_cast<const CSketchArcLine*>(profile_line);
            const auto* bezier = dynamic_cast<const CBezierSpline*>(profile_line);
            if (sketch_arc) {
                if (!add_trimmed_arc_edge(
                        wire_builder,
                        sketch,
                        *sketch_arc,
                        line_starts[line_index],
                        line_ends[line_index],
                        line_start_parameters[line_index],
                        line_end_parameters[line_index])) {
                    return false;
                }
            } else if (bezier) {
                TColgp_Array1OfPnt poles(1, 4);
                poles.SetValue(1, to_occ_point(sketch, bezier->GetStart()));
                poles.SetValue(2, to_occ_point(sketch, bezier->GetControl1()));
                poles.SetValue(3, to_occ_point(sketch, bezier->GetControl2()));
                poles.SetValue(4, to_occ_point(sketch, bezier->GetEnd()));
                const Handle(Geom_BezierCurve) curve = new Geom_BezierCurve(poles);
                BRepBuilderAPI_MakeEdge bezier_edge(
                    curve,
                    line_start_parameters[line_index],
                    line_end_parameters[line_index]);
                if (!bezier_edge.IsDone()) {
                    return false;
                }
                wire_builder.Add(bezier_edge.Edge());
            } else {
                BRepBuilderAPI_MakeEdge line_edge(
                    to_occ_point(sketch, line_starts[line_index]),
                    to_occ_point(sketch, line_ends[line_index]));
                if (!line_edge.IsDone()) {
                    return false;
                }
                wire_builder.Add(line_edge.Edge());
            }

            const CFillet* fillet = fillet_after_line[line_index];
            if (!fillet) {
                continue;
            }
            const CLinkLine* first = sketch.GetLine(fillet->GetFirstLineIndex());
            const CLinkLine* second = sketch.GetLine(fillet->GetSecondLineIndex());
            const FilletGeometry geometry = fillet->Calculate(*first, *second);
            if (!add_fillet_edge(wire_builder, sketch, geometry)) {
                return false;
            }
        }

        if (close_to_axis) {
            const CPoint3d first =
                sketch.LocalToWorld(line_starts.front());
            const CPoint3d last =
                sketch.LocalToWorld(line_ends.back());
            const CPoint3d first_on_axis = project_to_axis(first);
            const CPoint3d last_on_axis = project_to_axis(last);
            const auto add_closing_edge =
                [&wire_builder](const CPoint3d& start,
                                const CPoint3d& end) {
                    if (point_distance(start, end) <= kProfileTolerance) {
                        return true;
                    }
                    BRepBuilderAPI_MakeEdge edge(
                        gp_Pnt(start.x, start.y, start.z),
                        gp_Pnt(end.x, end.y, end.z));
                    if (!edge.IsDone()) {
                        return false;
                    }
                    wire_builder.Add(edge.Edge());
                    return true;
                };
            if (!add_closing_edge(last, last_on_axis)
                || !add_closing_edge(last_on_axis, first_on_axis)
                || !add_closing_edge(first_on_axis, first)) {
                return false;
            }
        }

        if (!wire_builder.IsDone()) {
            return false;
        }
        TopoDS_Wire wire;
        if (!fix_profile_wire(wire_builder.Wire(), true, wire)) {
            return false;
        }

        BRepBuilderAPI_MakeFace face_builder(wire, true);
        if (!face_builder.IsDone()) {
            return false;
        }
        face = face_builder.Face();
        return !face.IsNull();
    } catch (const Standard_Failure&) {
        return false;
    }
}

bool BuildSketchProfileFace(const CSmartLine& sketch,
                            TopoDS_Face& face,
                            Vec3& normal) {
    return BuildSketchProfileFaceImpl(
        sketch, false, {}, {}, face, normal);
}

bool BuildSketchRevolveProfileFace(const CSmartLine& sketch,
                                   Vec3 axis_origin,
                                   Vec3 axis_direction,
                                   TopoDS_Face& face,
                                   Vec3& normal) {
    return BuildSketchProfileFaceImpl(
        sketch,
        true,
        axis_origin,
        axis_direction,
        face,
        normal);
}

bool BuildSketchPathWire(const CSmartLine& sketch,
                         TopoDS_Wire& wire) {
    wire.Nullify();
    if (sketch.GetNumLines() == 0) {
        return false;
    }

    std::vector<CPoint3d> line_starts;
    std::vector<CPoint3d> line_ends;
    std::vector<double> line_start_parameters(sketch.GetNumLines(), 0.0);
    std::vector<double> line_end_parameters(sketch.GetNumLines(), 1.0);
    line_starts.reserve(sketch.GetNumLines());
    line_ends.reserve(sketch.GetNumLines());
    for (std::size_t index = 0; index < sketch.GetNumLines(); ++index) {
        const CLinkLine* line = sketch.GetLine(index);
        if (!line) {
            return false;
        }
        line_starts.push_back(line->GetStart());
        line_ends.push_back(line->GetEnd());
    }

    std::vector<const CFillet*> fillet_after_line(
        sketch.GetNumLines(), nullptr);
    for (std::size_t index = 0; index < sketch.GetNumFillets(); ++index) {
        const CFillet* fillet = sketch.GetFillet(index);
        if (!fillet
            || fillet->GetFirstLineIndex() >= sketch.GetNumLines()
            || fillet->GetSecondLineIndex() >= sketch.GetNumLines()) {
            return false;
        }
        const CLinkLine* first =
            sketch.GetLine(fillet->GetFirstLineIndex());
        const CLinkLine* second =
            sketch.GetLine(fillet->GetSecondLineIndex());
        const FilletGeometry geometry = fillet->Calculate(*first, *second);
        if (!geometry.valid) {
            return false;
        }
        line_ends[fillet->GetFirstLineIndex()] = geometry.tangent_on_first;
        line_starts[fillet->GetSecondLineIndex()] = geometry.tangent_on_second;
        line_end_parameters[fillet->GetFirstLineIndex()] =
            geometry.first_parameter;
        line_start_parameters[fillet->GetSecondLineIndex()] =
            geometry.second_parameter;
        fillet_after_line[fillet->GetFirstLineIndex()] = fillet;
    }

    try {
        BRepBuilderAPI_MakeWire wire_builder;
        for (std::size_t line_index = 0;
             line_index < sketch.GetNumLines();
             ++line_index) {
            if (point_distance(
                    line_starts[line_index], line_ends[line_index])
                <= kProfileTolerance) {
                return false;
            }

            const CLinkLine* profile_line = sketch.GetLine(line_index);
            const auto* sketch_arc =
                dynamic_cast<const CSketchArcLine*>(profile_line);
            const auto* bezier =
                dynamic_cast<const CBezierSpline*>(profile_line);
            if (sketch_arc) {
                if (!add_trimmed_arc_edge(
                        wire_builder,
                        sketch,
                        *sketch_arc,
                        line_starts[line_index],
                        line_ends[line_index],
                        line_start_parameters[line_index],
                        line_end_parameters[line_index])) {
                    return false;
                }
            } else if (bezier) {
                TColgp_Array1OfPnt poles(1, 4);
                poles.SetValue(1, to_occ_point(sketch, bezier->GetStart()));
                poles.SetValue(2, to_occ_point(sketch, bezier->GetControl1()));
                poles.SetValue(3, to_occ_point(sketch, bezier->GetControl2()));
                poles.SetValue(4, to_occ_point(sketch, bezier->GetEnd()));
                const Handle(Geom_BezierCurve) curve =
                    new Geom_BezierCurve(poles);
                BRepBuilderAPI_MakeEdge edge(
                    curve,
                    line_start_parameters[line_index],
                    line_end_parameters[line_index]);
                if (!edge.IsDone()) {
                    return false;
                }
                wire_builder.Add(edge.Edge());
            } else {
                BRepBuilderAPI_MakeEdge edge(
                    to_occ_point(sketch, line_starts[line_index]),
                    to_occ_point(sketch, line_ends[line_index]));
                if (!edge.IsDone()) {
                    return false;
                }
                wire_builder.Add(edge.Edge());
            }

            const CFillet* fillet = fillet_after_line[line_index];
            if (!fillet) {
                continue;
            }
            const CLinkLine* first =
                sketch.GetLine(fillet->GetFirstLineIndex());
            const CLinkLine* second =
                sketch.GetLine(fillet->GetSecondLineIndex());
            const FilletGeometry geometry = fillet->Calculate(*first, *second);
            if (!add_fillet_edge(wire_builder, sketch, geometry)) {
                return false;
            }
        }
        if (!wire_builder.IsDone()) {
            return false;
        }
        return fix_profile_wire(
            wire_builder.Wire(), sketch.IsClosed(), wire);
    } catch (const Standard_Failure&) {
        wire.Nullify();
        return false;
    }
}

bool BuildOpenSketchProfileWire(const CSmartLine& sketch,
                                TopoDS_Wire& wire) {
    return !sketch.IsClosed() && BuildSketchPathWire(sketch, wire);
}
