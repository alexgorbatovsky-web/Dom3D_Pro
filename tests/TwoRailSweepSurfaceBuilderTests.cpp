#include "TwoRailSweepSurfaceBuilder.h"

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
#ifdef XtPointer
#undef XtPointer
#endif

#include <TopAbs_ShapeEnum.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <IGESControl_Reader.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <array>
#include <limits>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

int face_count(const TopoDS_Shape& shape) {
    int count = 0;
    for (TopExp_Explorer faces(shape, TopAbs_FACE); faces.More(); faces.Next()) {
        ++count;
    }
    return count;
}

int solid_count(const TopoDS_Shape& shape) {
    int count = 0;
    for (TopExp_Explorer solids(shape, TopAbs_SOLID); solids.More(); solids.Next()) {
        ++count;
    }
    return count;
}

void test_open_profile_and_reversed_rail() {
    SweepCurveSamples profile{{
        {0.0, 0.0, 0.0},
        {0.0, 2.5, 2.0},
        {0.0, 5.0, 0.0}}, false};
    SweepCurveSamples first{{
        {-10.0, 0.0, 0.0},
        {0.0, 0.0, 0.0},
        {10.0, 0.0, 1.0}}, false};
    SweepCurveSamples second{{
        {10.0, 5.0, 1.5},
        {0.0, 5.0, 0.0},
        {-10.0, 5.0, 0.0}}, false};

    const TopoDS_Shape result = BuildTwoRailSweepSurfaceShape(profile, first, second);
    require(!result.IsNull(), "open profile sweep should build");
    require(face_count(result) > 0, "open profile sweep should contain a face");
}

void test_noisy_closed_profile_is_planarized() {
    SweepCurveSamples profile;
    profile.closed = true;
    constexpr int point_count = 32;
    for (int i = 0; i <= point_count; ++i) {
        const double angle = 6.28318530717958647692 * static_cast<double>(i)
            / static_cast<double>(point_count);
        profile.points.emplace_back(
            0.015 * std::sin(angle * 3.0),
            2.5 + 2.5 * std::cos(angle),
            2.0 * std::sin(angle));
    }
    SweepCurveSamples first{{
        {-10.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {10.0, 0.0, 1.0}}, false};
    SweepCurveSamples second{{
        {-10.0, 5.0, 0.0}, {0.0, 5.0, 0.0}, {10.0, 5.0, 1.5}}, false};

    const TopoDS_Shape result = BuildTwoRailSweepSurfaceShape(profile, first, second);
    require(!result.IsNull(), "numerically noisy planar profile should build safely");
    require(face_count(result) > 0, "closed profile sweep should contain faces");
}

void test_closed_profile_builds_anisotropic_solid() {
    SweepCurveSamples profile{{
        {0.0, -2.0, -0.5},
        {0.0, 2.0, -0.5},
        {0.0, 2.0, 0.5},
        {0.0, -2.0, 0.5}}, true};
    SweepCurveSamples first{{
        {-10.0, -2.0, 0.0}, {0.0, -3.0, 1.0}, {10.0, -1.5, 0.0}}, false};
    SweepCurveSamples second{{
        {-10.0, 2.0, 0.0}, {0.0, 3.0, 1.0}, {10.0, 1.5, 0.0}}, false};

    const TopoDS_Shape surface = BuildTwoRailSweepSurfaceShape(profile, first, second);
    require(!surface.IsNull(), "same closed sections should build a surface");
    BRepBuilderAPI_MakePolygon exact_profile;
    for (const CPoint3d& point : profile.points) {
        exact_profile.Add(gp_Pnt(point.x, point.y, point.z));
    }
    exact_profile.Close();
    require(exact_profile.IsDone(), "exact profile wire should build");
    const TopoDS_Shape result = BuildTwoRailSweepSolidShape(
        profile, first, second, exact_profile.Wire());
    require(!result.IsNull(), "closed profile two-rail solid should build");
    require(solid_count(result) == 1, "two-rail sweep should create one solid");
    require(face_count(result) >= 3, "two-rail sweep solid should contain side and cap faces");
    require(BRepCheck_Analyzer(result).IsValid(), "two-rail sweep solid should be valid");
}

std::array<double, 6> bounds(const TopoDS_Shape& shape) {
    require(!shape.IsNull(), "placement sweep should build");
    require(BRepCheck_Analyzer(shape).IsValid(), "placement sweep should be valid");
    Bnd_Box box;
    BRepBndLib::AddOptimal(shape, box, false, false);
    std::array<double, 6> result{};
    box.Get(result[0], result[1], result[2], result[3], result[4], result[5]);
    return result;
}

void test_section_placement() {
    const SweepCurveSamples profile{{
        {0, -2, -0.5}, {0, 2, -0.5}, {0, 2, 0.5}, {0, -2, 0.5}}, true};
    const SweepCurveSamples first{{{0, -2, 0}, {10, -2, 0}}, false};
    const SweepCurveSamples second{{{0, 2, 0}, {10, 2, 0}}, false};
    BRepBuilderAPI_MakePolygon polygon;
    for (const auto& p : profile.points) polygon.Add(gp_Pnt(p.x, p.y, p.z));
    polygon.Close();
    // Check both surface and solid paths, including the exact CAD profile used by the UI.
    for (int mode = 0; mode < 3; ++mode) {
        const auto build = [&](double dx, double dy, double angle) {
            if (mode == 0) return BuildTwoRailSweepSurfaceShape(profile, first, second, dx, dy, angle);
            if (mode == 1) return BuildTwoRailSweepSolidShape(profile, first, second, dx, dy, angle);
            return BuildTwoRailSweepSolidShape(profile, first, second, polygon.Wire(), dx, dy, angle);
        };
        const auto base = bounds(build(0, 0, 0));
        const auto shifted = bounds(build(1.25, -0.75, 0));
        const auto rotated = bounds(build(0, 0, -90));
        const auto combined = bounds(build(1.25, -0.75, -90));
        const auto restored = bounds(build(0, 0, 0));
        for (int axis = 0; axis < 3; ++axis) {
            require(std::abs((shifted[axis + 3] - shifted[axis]) - (base[axis + 3] - base[axis])) < 1e-4,
                    "offsets must not resize the section");
            require(std::abs((combined[axis] - rotated[axis]) - (shifted[axis] - base[axis])) < 1e-4,
                    "offsets must use the unrotated frame");
            require(std::abs(restored[axis] - base[axis]) < 1e-6,
                    "resetting placement must restore geometry");
        }
        require(std::abs(shifted[0] - base[0]) < 1e-4, "offsets must stay in section plane");
        require(std::abs(shifted[1] - base[1] - 1.25) < 1e-4, "Delta X must move along the rail span");
        require(std::abs(std::abs(shifted[2] - base[2]) - 0.75) < 1e-4, "Delta Y must move along section height");
        require(std::abs((rotated[4] - rotated[1]) - (base[5] - base[2])) < 1e-4,
                "90 degree rotation must exchange section width and height");
        require(std::abs((rotated[5] - rotated[2]) - (base[4] - base[1])) < 1e-4,
                "rotation must preserve the rail-scaled section size");
        require(build(0, 0, std::numeric_limits<double>::quiet_NaN()).IsNull(),
                "invalid placement must be rejected");
    }
}

void test_iges_curves(const char* path) {
    IGESControl_Reader reader;
    require(reader.ReadFile(path) == IFSelect_RetDone, "IGES regression file should open");

    std::vector<SweepCurveSamples> curves;
    for (Standard_Integer root = 1; root <= reader.NbRootsForTransfer(); ++root) {
        reader.ClearShapes();
        if (!reader.TransferOneRoot(root)) {
            continue;
        }
        for (Standard_Integer shape_index = 1; shape_index <= reader.NbShapes(); ++shape_index) {
            const TopoDS_Shape shape = reader.Shape(shape_index);
            if (shape.IsNull() || TopExp_Explorer(shape, TopAbs_FACE).More()) {
                continue;
            }
            TopExp_Explorer edges(shape, TopAbs_EDGE);
            if (!edges.More()) {
                continue;
            }
            BRepAdaptor_Curve curve(TopoDS::Edge(edges.Current()));
            SweepCurveSamples samples;
            constexpr int sample_count = 96;
            for (int i = 0; i <= sample_count; ++i) {
                const double parameter = curve.FirstParameter()
                    + (curve.LastParameter() - curve.FirstParameter())
                        * static_cast<double>(i) / static_cast<double>(sample_count);
                const gp_Pnt point = curve.Value(parameter);
                samples.points.emplace_back(point.X(), point.Y(), point.Z());
            }
            const CPoint3d& first = samples.points.front();
            const CPoint3d& last = samples.points.back();
            const double dx = first.x - last.x;
            const double dy = first.y - last.y;
            const double dz = first.z - last.z;
            samples.closed = dx * dx + dy * dy + dz * dz < 1.0e-6;
            curves.push_back(std::move(samples));
        }
    }
    require(curves.size() == 3, "Swept IGES should contain three standalone curves");

    std::size_t profile = 0;
    for (std::size_t i = 0; i < curves.size(); ++i) {
        if (curves[i].closed) {
            profile = i;
            break;
        }
    }
    std::vector<std::size_t> rails;
    for (std::size_t i = 0; i < curves.size(); ++i) {
        if (i != profile) {
            rails.push_back(i);
        }
    }
    const TopoDS_Shape result = BuildTwoRailSweepSurfaceShape(
        curves[profile], curves[rails[0]], curves[rails[1]]);
    require(!result.IsNull(), "real Swept IGES curves should build without a crash");
    require(face_count(result) > 0, "real Swept IGES sweep should contain faces");
}
}

int main(int argc, char** argv) {
    test_open_profile_and_reversed_rail();
    test_noisy_closed_profile_is_planarized();
    test_closed_profile_builds_anisotropic_solid();
    test_section_placement();
    if (argc > 1) {
        test_iges_curves(argv[1]);
    }
    std::cout << "TwoRailSweepSurfaceBuilderTests passed\n";
    return 0;
}
