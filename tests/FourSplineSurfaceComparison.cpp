#include "FourSplineSurfaceBuilder.h"

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

#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <Bnd_Box.hxx>
#include <IGESControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <Geom_Surface.hxx>
#include <Geom_BSplineSurface.hxx>
#include <TopAbs_State.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt2d.hxx>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
TopoDS_Shape read_iges(const char* path)
{
    IGESControl_Reader reader;
    if (reader.ReadFile(path) != IFSelect_RetDone || reader.TransferRoots() <= 0) {
        return {};
    }
    return reader.OneShape();
}

std::vector<SweepCurveSamples> read_curves(const TopoDS_Shape& shape)
{
    std::vector<SweepCurveSamples> curves;
    for (TopExp_Explorer edges(shape, TopAbs_EDGE); edges.More(); edges.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(edges.Current());
        BRepAdaptor_Curve curve(edge);
        const double first = curve.FirstParameter();
        const double last = curve.LastParameter();
        if (!std::isfinite(first) || !std::isfinite(last) || last <= first) {
            continue;
        }
        SweepCurveSamples samples;
        constexpr int count = 65;
        samples.points.reserve(count);
        for (int i = 0; i < count; ++i) {
            const double t = first + (last - first) * static_cast<double>(i) / (count - 1);
            const gp_Pnt point = curve.Value(t);
            samples.points.push_back({point.X(), point.Y(), point.Z()});
        }
        curves.push_back(std::move(samples));
    }
    return curves;
}

std::vector<TopoDS_Face> read_faces(const TopoDS_Shape& shape)
{
    std::vector<TopoDS_Face> faces;
    for (TopExp_Explorer it(shape, TopAbs_FACE); it.More(); it.Next()) {
        faces.push_back(TopoDS::Face(it.Current()));
    }
    return faces;
}

double shape_diagonal(const TopoDS_Shape& shape)
{
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    if (box.IsVoid()) {
        return 0.0;
    }
    double xmin, ymin, zmin, xmax, ymax, zmax;
    box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    return std::hypot(std::hypot(xmax - xmin, ymax - ymin), zmax - zmin);
}

struct Deviation {
    std::size_t count = 0;
    double maximum = 0.0;
    double sum = 0.0;
    double squared_sum = 0.0;
};

Deviation sample_to_face_exact(const TopoDS_Face& source,
                               const TopoDS_Face& target,
                               int divisions)
{
    Deviation result;
    double u0, u1, v0, v1;
    BRepTools::UVBounds(source, u0, u1, v0, v1);
    const Handle(Geom_Surface) surface = BRep_Tool::Surface(source);
    const double tolerance = std::max(1.0e-8, shape_diagonal(source) * 1.0e-9);
    for (int j = 0; j <= divisions; ++j) {
        const double v = v0 + (v1 - v0) * static_cast<double>(j) / divisions;
        for (int i = 0; i <= divisions; ++i) {
            const double u = u0 + (u1 - u0) * static_cast<double>(i) / divisions;
            BRepClass_FaceClassifier classifier(source, gp_Pnt2d(u, v), tolerance);
            if (classifier.State() != TopAbs_IN && classifier.State() != TopAbs_ON) {
                continue;
            }
            const gp_Pnt point = surface->Value(u, v);
            BRepExtrema_DistShapeShape distance(
                BRepBuilderAPI_MakeVertex(point).Vertex(), target);
            distance.Perform();
            if (!distance.IsDone() || distance.NbSolution() == 0) {
                continue;
            }
            const double value = distance.Value();
            result.maximum = std::max(result.maximum, value);
            result.sum += value;
            result.squared_sum += value * value;
            ++result.count;
        }
    }
    return result;
}

Deviation combine(const Deviation& first, const Deviation& second)
{
    return {first.count + second.count,
            std::max(first.maximum, second.maximum),
            first.sum + second.sum,
            first.squared_sum + second.squared_sum};
}

Deviation compare_parameters(const TopoDS_Face& first,
                             const TopoDS_Face& second,
                             int divisions,
                             bool swap,
                             bool reverse_u,
                             bool reverse_v)
{
    Deviation result;
    double first_u0, first_u1, first_v0, first_v1;
    double second_u0, second_u1, second_v0, second_v1;
    BRepTools::UVBounds(first, first_u0, first_u1, first_v0, first_v1);
    BRepTools::UVBounds(second, second_u0, second_u1, second_v0, second_v1);
    const Handle(Geom_Surface) first_surface = BRep_Tool::Surface(first);
    const Handle(Geom_Surface) second_surface = BRep_Tool::Surface(second);
    if (first_surface.IsNull() || second_surface.IsNull()) {
        return result;
    }
    const double tolerance = std::max(1.0e-8, shape_diagonal(first) * 1.0e-9);
    for (int j = 0; j <= divisions; ++j) {
        const double b = static_cast<double>(j) / divisions;
        for (int i = 0; i <= divisions; ++i) {
            const double a = static_cast<double>(i) / divisions;
            const double first_u = first_u0 + (first_u1 - first_u0) * a;
            const double first_v = first_v0 + (first_v1 - first_v0) * b;
            BRepClass_FaceClassifier first_classifier(
                first, gp_Pnt2d(first_u, first_v), tolerance);
            if (first_classifier.State() != TopAbs_IN
                && first_classifier.State() != TopAbs_ON) {
                continue;
            }
            double mapped_u = swap ? b : a;
            double mapped_v = swap ? a : b;
            if (reverse_u) {
                mapped_u = 1.0 - mapped_u;
            }
            if (reverse_v) {
                mapped_v = 1.0 - mapped_v;
            }
            const double second_u = second_u0 + (second_u1 - second_u0) * mapped_u;
            const double second_v = second_v0 + (second_v1 - second_v0) * mapped_v;
            BRepClass_FaceClassifier second_classifier(
                second, gp_Pnt2d(second_u, second_v), tolerance);
            if (second_classifier.State() != TopAbs_IN
                && second_classifier.State() != TopAbs_ON) {
                continue;
            }
            const gp_Pnt first_point = first_surface->Value(first_u, first_v);
            const gp_Pnt second_point = second_surface->Value(second_u, second_v);
            const double value = first_point.Distance(second_point);
            result.maximum = std::max(result.maximum, value);
            result.sum += value;
            result.squared_sum += value * value;
            ++result.count;
        }
    }
    return result;
}
}

int main(int argc, char** argv)
{
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: FourSplineSurfaceComparison guides.igs reference.igs [--quick]\n";
        return EXIT_FAILURE;
    }
    const TopoDS_Shape guide_shape = read_iges(argv[1]);
    const TopoDS_Shape reference_shape = read_iges(argv[2]);
    const auto curves = read_curves(guide_shape);
    const auto reference_faces = read_faces(reference_shape);
    std::cerr << "guide curves: " << curves.size()
              << ", reference faces: " << reference_faces.size() << std::endl;
    if (curves.size() != 4 || reference_faces.empty()) {
        return EXIT_FAILURE;
    }

    std::cerr << "building comparison surface..." << std::endl;
    const TopoDS_Shape built_shape = BuildFourSplineSurfaceShape(
        curves[0], curves[1], curves[2], curves[3]);
    std::cerr << "surface build finished, null=" << built_shape.IsNull() << std::endl;
    const auto built_faces = read_faces(built_shape);
    std::cerr << "generated faces: " << built_faces.size() << std::endl;
    if (built_faces.size() != 1) {
        std::cerr << "Generated surface has " << built_faces.size() << " faces." << std::endl;
        return EXIT_FAILURE;
    }
    const Handle(Geom_BSplineSurface) built_surface =
        Handle(Geom_BSplineSurface)::DownCast(BRep_Tool::Surface(built_faces.front()));
    if (!built_surface.IsNull()) {
        std::cerr << "surface structure: U poles=" << built_surface->NbUPoles()
                  << ", V poles=" << built_surface->NbVPoles()
                  << ", U knots=" << built_surface->NbUKnots()
                  << ", V knots=" << built_surface->NbVKnots()
                  << std::endl;
    }

    std::cout << std::fixed << std::setprecision(6);
    double best_maximum = std::numeric_limits<double>::infinity();
    std::size_t best_face = 0;
    for (std::size_t i = 0; i < reference_faces.size(); ++i) {
        Deviation symmetric;
        double best_rms = std::numeric_limits<double>::infinity();
        for (int swap = 0; swap < 2; ++swap) {
            for (int reverse_u = 0; reverse_u < 2; ++reverse_u) {
                for (int reverse_v = 0; reverse_v < 2; ++reverse_v) {
                    const Deviation candidate = compare_parameters(
                        built_faces.front(), reference_faces[i], 32,
                        swap != 0, reverse_u != 0, reverse_v != 0);
                    const double candidate_rms = candidate.count
                        ? std::sqrt(candidate.squared_sum / candidate.count)
                        : std::numeric_limits<double>::infinity();
                    if (candidate_rms < best_rms) {
                        best_rms = candidate_rms;
                        symmetric = candidate;
                    }
                }
            }
        }
        const double mean = symmetric.count ? symmetric.sum / symmetric.count : 0.0;
        const double rms = symmetric.count
            ? std::sqrt(symmetric.squared_sum / symmetric.count) : 0.0;
        std::cout << "face " << (i + 1)
                  << ": samples=" << symmetric.count
                  << " mean=" << mean
                  << " rms=" << rms
                  << " max=" << symmetric.maximum << std::endl;
        if (symmetric.maximum < best_maximum) {
            best_maximum = symmetric.maximum;
            best_face = i + 1;
        }
    }
    std::cout << "best reference face: " << best_face
              << ", model diagonal: " << shape_diagonal(built_shape)
              << ", symmetric max: " << best_maximum << '\n';
    if (best_face > 0 && argc == 3) {
        std::cout << "computing closest-point deviation for face "
                  << best_face << "..." << std::endl;
        const Deviation closest = combine(
            sample_to_face_exact(built_faces.front(), reference_faces[best_face - 1], 10),
            sample_to_face_exact(reference_faces[best_face - 1], built_faces.front(), 10));
        const double mean = closest.count ? closest.sum / closest.count : 0.0;
        const double rms = closest.count
            ? std::sqrt(closest.squared_sum / closest.count) : 0.0;
        std::cout << "closest-point: samples=" << closest.count
                  << " mean=" << mean
                  << " rms=" << rms
                  << " max=" << closest.maximum << std::endl;
    }
    return EXIT_SUCCESS;
}
