#include "HolePlacement.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pln.hxx>
#include <gp_Vec.hxx>

bool BuildHoleFacePlacement(const TopoDS_Face& face,
                           const gp_Pnt& clicked_point,
                           HoleFacePlacement& placement, std::string& error) {
    placement = {};
    error.clear();
    try {
        if (face.IsNull() || BRepAdaptor_Surface(face).GetType() != GeomAbs_Plane) {
            error = "Select a planar body face.";
            return false;
        }
        GProp_GProps properties;
        BRepGProp::SurfaceProperties(face, properties);
        if (!(properties.Mass() > 1.e-12)) {
            error = "The face has no usable area.";
            return false;
        }
        HoleFacePlacement candidate;
        const gp_Pln plane = BRepAdaptor_Surface(face).Plane();
        candidate.normal = plane.Axis().Direction();
        // The click is intersected with the face plane by the viewport.
        // Remove only its normal component to eliminate float camera error;
        // never replace the in-plane position with a face centroid.
        const gp_Vec normal(candidate.normal);
        candidate.center = clicked_point.Translated(
            normal * -normal.Dot(gp_Vec(plane.Location(), clicked_point)));
        if (face.Orientation() == TopAbs_REVERSED) candidate.normal.Reverse();
        BRepClass_FaceClassifier classifier(face, candidate.center, 1.e-7);
        if (classifier.State() != TopAbs_IN) {
            error = "The clicked point is outside the face material or inside an existing hole.";
            return false;
        }
        struct Edge {
            gp_Pnt start, end;
            gp_Dir direction;
            double distance;
        };
        std::vector<Edge> edges;
        const TopoDS_Shape vertex = BRepBuilderAPI_MakeVertex(candidate.center).Shape();
        TopTools_IndexedMapOfShape edge_map;
        TopExp::MapShapes(face, TopAbs_EDGE, edge_map);
        candidate.clearance = std::numeric_limits<double>::max();
        for (int i = 1; i <= edge_map.Extent(); ++i) {
            const auto& edge = TopoDS::Edge(edge_map(i));
            BRepExtrema_DistShapeShape distance(vertex, edge);
            if (!distance.IsDone() || !std::isfinite(distance.Value())) {
                error = "Could not measure the face boundary.";
                return false;
            }
            candidate.clearance = std::min(candidate.clearance, distance.Value());
            BRepAdaptor_Curve curve(edge);
            if (curve.GetType() != GeomAbs_Line) continue;
            const gp_Pnt a = curve.Value(curve.FirstParameter());
            const gp_Pnt b = curve.Value(curve.LastParameter());
            if (a.Distance(b) <= 1.e-7) continue;
            edges.push_back({a, b, gp_Dir(gp_Vec(a, b)), distance.Value()});
        }
        std::stable_sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
            return a.distance < b.distance;
        });
        size_t second = 1;
        while (second < edges.size()
            && gp_Vec(edges[0].direction).Crossed(gp_Vec(edges[second].direction)).Magnitude() < 1.e-6)
            ++second;
        if (edges.size() < 2 || second >= edges.size()) {
            error = "The face needs two non-parallel straight edges for automatic dimensions.";
            return false;
        }
        for (size_t i = 0; i < 2; ++i) {
            const Edge& edge = edges[i == 0 ? 0 : second];
            candidate.starts[i] = edge.start;
            candidate.ends[i] = edge.end;
            const gp_Vec perpendicular = gp_Vec(candidate.normal).Crossed(gp_Vec(edge.direction));
            const double signed_distance = perpendicular.Dot(gp_Vec(edge.start, candidate.center));
            candidate.distances[i] = std::fabs(signed_distance);
            candidate.sides[i] = signed_distance < 0 ? -1.0 : 1.0;
        }
        if (!std::isfinite(candidate.clearance) || candidate.clearance <= 1.e-6) {
            error = "There is not enough room for a hole at the clicked point.";
            return false;
        }
        placement = candidate;
        return true;
    } catch (const Standard_Failure&) {
        error = "Could not calculate the hole placement on this face.";
        return false;
    }
}
