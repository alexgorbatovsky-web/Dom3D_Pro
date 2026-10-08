#include "CPolyline.h"
#include "BezierSpline.h"
#include "ExtrudeShapeBuilder.h"
#include "LinkLineHor.h"
#include "LinkLineVert.h"
#include "SketchArcLine.h"
#include "SketchProfileBuilder.h"
#include "SmartLine.h"

#include <BRepCheck_Analyzer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>
#include <type_traits>
#include <limits>

// SmartLine::Create references these accessors, although this focused test
// builds the sketch from its native segments and does not need CPolyline.cpp.
const std::vector<CPoint3d>& CPolyline::GetPoints() const {
    return points_;
}

bool CPolyline::IsClosed() const {
    return closed_;
}

double CPoint3d::DistTo(CPoint3d* other) {
    if (!other) {
        return 0.0;
    }
    return std::hypot(
        std::hypot(x - other->x, y - other->y),
        z - other->z);
}

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main() try {
    static_assert(sizeof(SketchPoint)==2*sizeof(double), "Sketch points must have only u,v");
    static_assert(std::is_same_v<decltype(std::declval<CLinkLine&>().P(0)),const CPoint3d*>, "No writable point pointers");
    {
        CSmartLine planar;
        require(planar.SetCoordinateSystem({1e6,-2e6,3e6},{1,2,3},{2,-1,0}),"Tilted sketch frame failed");
        require(planar.AddLine(std::make_unique<CLinkLine>(CPoint3d(0,0,55),CPoint3d(10,0,-9))),"UV segment failed");
        require(planar.AddLine(std::make_unique<CBezierSpline>(CPoint3d(10,0,8),CPoint3d(11,2,4),CPoint3d(15,4,-7),CPoint3d(20,5,11))),"UV Bezier failed");
        require(planar.GetEndpointId(0,1)==planar.GetEndpointId(1,0),"Joined endpoints are separate nodes");
        const auto line_id=planar.GetLine(1)->GetID();
        const auto node_id=planar.GetEndpointId(0,1);
        for(size_t i=0;i<planar.GetNumLines();++i) for(const auto& p:planar.GetLine(i)->Sample(16))
            require(p.z==0,"Local curve left the sketch plane");
        int events=0; planar.SetChangeCallback([&](unsigned,SketchRevisions){++events;});
        const auto before=planar.GetRevisions();
        planar.BeginEdit();
        require(planar.MovePoint(0,1,{12,1,900}),"First transaction edit failed");
        require(planar.MovePoint(0,1,{13,2,-900}),"Second transaction edit failed");
        require(events==0,"Preview notified dependencies before commit");
        require(planar.CommitEdit(),"Commit failed");
        require(events==1 && planar.GetRevisions().geometry==before.geometry+1,"One edit did not produce one revision/event");
        require(planar.GetLine(1)->GetStart().x==13 && planar.GetLine(1)->GetStart().z==0,"Shared node was not updated");
        const auto* moved_bezier=dynamic_cast<const CBezierSpline*>(planar.GetLine(1));
        require(moved_bezier && moved_bezier->GetControl1().x==14 && moved_bezier->GetControl1().y==4,"Shared anchor left its Bezier handle behind");
        const auto committed=planar.GetLine(0)->GetEnd();
        planar.BeginEdit(); planar.MovePoint(0,1,committed); require(planar.CommitEdit(),"No-op commit failed");
        require(events==1,"No-op edit notified dependents");
        planar.BeginEdit(); planar.MovePoint(0,1,{30,20,0}); planar.CancelEdit();
        require(events==1 && planar.GetLine(0)->GetEnd().x==committed.x,"Cancel leaked preview changes");
        require(planar.GetEndpointId(0,1)==node_id && planar.GetLine(1)->GetID()==line_id,"Cancel changed identities");
        planar.BeginEdit(); planar.MovePoint(0,1,{20,20,0});
        require(!planar.MovePoint(99,0,{1,1,0}),"Invalid edit accepted");
        require(!planar.CommitEdit() && planar.GetLine(0)->GetEnd().x==committed.x,"Failed transaction was not rolled back");
        bool protected_write=false;
        try { planar.GetLine(0)->SetEnd({50,50,0}); } catch(const std::logic_error&) { protected_write=true; }
        require(protected_write && planar.GetLine(0)->GetEnd().x==committed.x,"Untracked write escaped the owner");
        const auto revisions=planar.GetRevisions();
        planar.Translate({1,2,3});
        require(planar.GetRevisions().geometry==revisions.geometry && planar.GetRevisions().placement==revisions.placement+1,"Placement invalidated local geometry");
        for(int i=0;i<100;++i) planar.Rotate({},{1,2,3},0.01f);
        const auto& frame=planar.GetCoordinateSystem();
        for(const auto& p:planar.GetProfilePointsWorld()) {
            const double d=(p.x-frame.origin.x)*frame.normal.x+(p.y-frame.origin.y)*frame.normal.y+(p.z-frame.origin.z)*frame.normal.z;
            require(std::abs(d)<1e-7,"World profile lost planarity after repeated transforms");
        }
        const auto joint=planar.GetLine(0)->GetEnd(); planar.ScaleLocal(2,2);
        require(std::abs(planar.GetLine(0)->GetEnd().x-joint.x*2)<1e-9,"Shared node was scaled twice");
        auto copy=planar.MakeCopy(); copy.MovePoint(0,1,{3,4,0});
        require(planar.GetLine(0)->GetEnd().x!=copy.GetLine(0)->GetEnd().x,"Copy shares mutable sketch nodes");
        require(planar.SplitLine(0,{2,0,0}) && planar.GetLine(2)->GetID()==line_id,"Split renumbered unaffected line IDs");
        CSmartLine imported;
        require(!imported.CreateFromWorldPoints({{0,0,0},{10,0,0},{10,10,0.1}},false,{},{1,0,0},{0,1,0})
                && !imported.GetLastGeometryError().empty(),"Nonplanar import was silently flattened");
        require(imported.CreateFromWorldPoints({{0,0,0},{10,0,0},{10,10,1e-8}},false,{},{1,0,0},{0,1,0}),"Numerical plane residual was not accepted");
        CSmartLine topology;
        require(topology.CreateFromWorldPoints({{0,0,0},{10,0,0},{10,10,0},{0,10,0}},true,{},{1,0,0},{0,1,0}),"Topology sketch failed");
        const auto survivor=topology.GetLine(2)->GetID();
        require(topology.ConstrainHorizontal(2),"Topology constraint failed");
        require(topology.SetClosed(false) && topology.GetEndpointId(0,0)!=topology.GetEndpointId(3,1),"Opening a contour kept its endpoints welded");
        require(topology.RemoveLine(0) && topology.GetLine(1)->GetID()==survivor && topology.GetNumConstraints()==1
                && topology.GetConstraint(0)->GetLineIndex()==1,"Deletion lost an unaffected constraint/identity");
        CSmartLine separate;
        require(separate.AddLine(std::make_unique<CLinkLine>(CPoint3d(0,0,0),CPoint3d(10,0,0)),false)
                && separate.AddLine(std::make_unique<CLinkLine>(CPoint3d(30,0,0),CPoint3d(40,0,0)),false)
                && separate.GetNodeCount()==4,"Disconnected links lost a node");
        require(separate.MoveNodeWorld(2,separate.LocalToWorld({31,2,0}))
                && separate.GetLine(1)->GetStart().x==31 && separate.GetLine(0)->GetEnd().x==10,"Moving a disconnected node altered the other link");
    }
    CSmartLine sketch("Filleted arc profile");
    require(
        sketch.SetCoordinateSystem(
            CPoint3d(12.0, -4.0, 7.0),
            CPoint3d(1.0, 0.2, 0.0),
            CPoint3d(0.1, -0.5, 1.0)),
        "Tilted sketch coordinate system was rejected.");

    require(sketch.Add(new CLinkLine(
                CPoint3d(0.0, 0.0, 0.0),
                CPoint3d(0.0, 10.0, 0.0))),
            "First profile line was rejected.");
    require(sketch.Add(new CLinkLine(
                CPoint3d(0.0, 10.0, 0.0),
                CPoint3d(10.0, 10.0, 0.0))),
            "Second profile line was rejected.");
    require(sketch.Add(new CSketchArcLine(
                CPoint3d(10.0, 10.0, 0.0),
                CPoint3d(16.0, 5.0, 0.0),
                CPoint3d(10.0, 0.0, 0.0))),
            "Profile arc was rejected.");
    require(sketch.Add(new CLinkLine(
                CPoint3d(10.0, 0.0, 0.0),
                CPoint3d(0.0, 0.0, 0.0))),
            "Closing profile line was rejected.");
    require(sketch.SetClosed(true), "Profile could not be closed.");
    require(sketch.AddFillet(1, 0.25),
            "Fillet before the arc was rejected.");
    require(sketch.AddFillet(2, 0.25),
            "Fillet after the arc was rejected.");

    TopoDS_Face profile;
    Vec3 normal{};
    require(BuildSketchProfileFace(sketch, profile, normal),
            "Profile with an arc trimmed at both ends was not built.");
    require(!profile.IsNull() && BRepCheck_Analyzer(profile).IsValid(),
            "Trimmed-arc profile face is invalid.");

    const TopoDS_Shape extrusion =
        BuildExtrudeShape(profile, normal, 4.0, 0.0);
    require(!extrusion.IsNull() && BRepCheck_Analyzer(extrusion).IsValid(),
            "Trimmed-arc profile could not be extruded.");

    // Moving the arc grip repeatedly used to expose tiny gaps between the
    // independently built arc and fillet vertices.  Exercise several valid
    // edited shapes and ensure every rebuilt contour remains extrudable.
    auto* edited_arc = dynamic_cast<CSketchArcLine*>(sketch.GetLine(2));
    require(edited_arc != nullptr, "Editable profile arc was lost.");
    const std::vector<CPoint3d> edited_arc_points = {
        CPoint3d(12.0, 5.0, 0.0),
        CPoint3d(14.0, 3.5, 0.0),
        CPoint3d(18.0, 6.5, 0.0),
        CPoint3d(22.0, 5.0, 0.0)};
    for (const CPoint3d& arc_point : edited_arc_points) {
        sketch.BeginEdit();
        edited_arc->SetPointOnArc(arc_point);
        require(sketch.CommitEdit(), "Arc transaction failed");
        TopoDS_Face edited_profile;
        Vec3 edited_normal{};
        require(BuildSketchProfileFace(
                    sketch, edited_profile, edited_normal)
                    && BRepCheck_Analyzer(edited_profile).IsValid(),
                "Edited arc-to-fillet contour is invalid.");
        const TopoDS_Shape edited_extrusion = BuildExtrudeShape(
            edited_profile, edited_normal, 4.0, 0.0);
        require(!edited_extrusion.IsNull()
                    && BRepCheck_Analyzer(edited_extrusion).IsValid(),
                "Edited arc-to-fillet contour could not be extruded.");
    }

    // Exact geometry from Bad_Extrude-2.dom3d.  This position of the arc
    // exposed the failure while moving the arc grip slightly made it vanish.
    CSmartLine reported_profile("Reported arc/fillet profile");
    require(reported_profile.Add(new CLinkLine(
                CPoint3d(35.0, 20.0, 0.0), CPoint3d(35.0, 50.0, 0.0)))
            && reported_profile.Add(new CLinkLine(
                CPoint3d(35.0, 50.0, 0.0), CPoint3d(90.0, 50.0, 0.0)))
            && reported_profile.Add(new CLinkLine(
                CPoint3d(90.0, 50.0, 0.0), CPoint3d(120.0, 65.0, 0.0)))
            && reported_profile.Add(new CLinkLine(
                CPoint3d(120.0, 65.0, 0.0), CPoint3d(160.0, 65.0, 0.0)))
            && reported_profile.Add(new CSketchArcLine(
                CPoint3d(160.0, 65.0, 0.0),
                CPoint3d(177.44332885742188, 40.79736328125, 0.0),
                CPoint3d(160.0, 20.0, 0.0)))
            && reported_profile.Add(new CLinkLine(
                CPoint3d(160.0, 20.0, 0.0), CPoint3d(35.0, 20.0, 0.0)))
            && reported_profile.SetClosed(true),
            "Reported profile could not be created.");
    require(reported_profile.AddFillet(1, 7.0)
            && reported_profile.AddFillet(2, 7.0)
            && reported_profile.AddFillet(3, 7.0)
            && reported_profile.AddFillet(4, 7.0)
            && reported_profile.AddFillet(0, 10.000000000000002)
            && reported_profile.AddFillet(5, 7.905694150420949),
            "Reported profile fillets could not be created.");
    TopoDS_Face reported_face;
    Vec3 reported_normal{};
    require(BuildSketchProfileFace(
                reported_profile, reported_face, reported_normal),
            "Reported arc-to-fillet profile face was not built.");
    require(BRepCheck_Analyzer(reported_face, true).IsValid(),
            "Reported arc-to-fillet profile face is invalid.");
    const TopoDS_Shape reported_extrusion = BuildExtrudeShape(
        reported_face, reported_normal, 20.0, 0.0);
    require(!reported_extrusion.IsNull()
                && BRepCheck_Analyzer(reported_extrusion, true).IsValid(),
            "Reported arc-to-fillet extrusion is invalid.");
    int reported_solid_count = 0;
    int reported_face_count = 0;
    for (TopExp_Explorer explorer(reported_extrusion, TopAbs_SOLID);
         explorer.More(); explorer.Next()) {
        ++reported_solid_count;
    }
    for (TopExp_Explorer explorer(reported_extrusion, TopAbs_FACE);
         explorer.More(); explorer.Next()) {
        ++reported_face_count;
    }
    require(reported_solid_count == 1 && reported_face_count == 14,
            "Reported extrusion is not one capped 12-sided solid.");

    CSmartLine split_profile("Split profile");
    require(split_profile.Add(new CLinkLine(
                CPoint3d(0.0, 0.0, 0.0), CPoint3d(20.0, 0.0, 0.0)))
            && split_profile.Add(new CLinkLine(
                CPoint3d(20.0, 0.0, 0.0), CPoint3d(20.0, 10.0, 0.0)))
            && split_profile.Add(new CLinkLine(
                CPoint3d(20.0, 10.0, 0.0), CPoint3d(0.0, 10.0, 0.0)))
            && split_profile.Add(new CLinkLine(
                CPoint3d(0.0, 10.0, 0.0), CPoint3d(0.0, 0.0, 0.0)))
            && split_profile.SetClosed(true),
            "Split test profile could not be created.");
    require(split_profile.ConstrainHorizontal(0),
            "Split test constraint could not be created.");
    require(split_profile.SplitLine(0, CPoint3d(7.0, 0.0, 0.0)),
            "Straight sketch segment was not split.");
    require(split_profile.GetNumLines() == 5 && split_profile.IsClosed(),
            "Splitting changed the closed profile topology.");
    require(split_profile.GetLine(0)->GetType() == LinkLineType::Segment
                && split_profile.GetLine(1)->GetType() == LinkLineType::Segment,
            "Splitting a constrained line did not create ordinary LinkLine parts.");
    const CPoint3d split_left = split_profile.GetLine(0)->GetEnd();
    const CPoint3d split_right = split_profile.GetLine(1)->GetStart();
    require(std::hypot(
                std::hypot(split_left.x - split_right.x,
                           split_left.y - split_right.y),
                split_left.z - split_right.z) < 1.0e-8,
            "Inserted sketch node is disconnected.");
    TopoDS_Face split_face;
    Vec3 split_normal{};
    require(BuildSketchProfileFace(split_profile, split_face, split_normal)
                && BRepCheck_Analyzer(split_face).IsValid(),
            "Profile became invalid after splitting a segment.");

    CSmartLine split_axis_lines("Split axis lines");
    require(split_axis_lines.Add(new CLinkLineHor(
                CPoint3d(0.0, 0.0, 0.0), CPoint3d(20.0, 0.0, 0.0)))
            && split_axis_lines.Add(new CLinkLineVert(
                CPoint3d(20.0, 0.0, 0.0), CPoint3d(20.0, 20.0, 0.0))),
            "Axis-line split test could not be created.");
    require(split_axis_lines.SplitLine(0, CPoint3d(8.0, 0.0, 0.0))
                && split_axis_lines.GetLine(0)->GetType() == LinkLineType::Segment
                && split_axis_lines.GetLine(1)->GetType() == LinkLineType::Segment,
            "LinkLineHor split did not create two ordinary LinkLine parts.");
    require(split_axis_lines.SplitLine(2, CPoint3d(20.0, 9.0, 0.0))
                && split_axis_lines.GetLine(2)->GetType() == LinkLineType::Segment
                && split_axis_lines.GetLine(3)->GetType() == LinkLineType::Segment,
            "LinkLineVert split did not create two ordinary LinkLine parts.");

    CSmartLine bezier_profile("Split Bezier profile");
    require(bezier_profile.Add(new CBezierSpline(
                CPoint3d(0.0, 0.0, 0.0),
                CPoint3d(8.0, 14.0, 0.0),
                CPoint3d(22.0, -6.0, 0.0),
                CPoint3d(30.0, 8.0, 0.0)))
            && bezier_profile.Add(new CLinkLine(
                CPoint3d(30.0, 8.0, 0.0), CPoint3d(30.0, -8.0, 0.0)))
            && bezier_profile.Add(new CLinkLine(
                CPoint3d(30.0, -8.0, 0.0), CPoint3d(0.0, -8.0, 0.0)))
            && bezier_profile.Add(new CLinkLine(
                CPoint3d(0.0, -8.0, 0.0), CPoint3d(0.0, 0.0, 0.0)))
            && bezier_profile.SetClosed(true),
            "Bezier split test profile could not be created.");
    std::vector<CPoint3d> original_bezier_points;
    for (int sample = 0; sample <= 20; ++sample) {
        original_bezier_points.push_back(
            bezier_profile.GetLine(0)->GetPoint(sample / 20.0));
    }
    constexpr double split_parameter = 0.37;
    require(bezier_profile.SplitBezierLine(0, split_parameter),
            "Bezier segment was not split.");
    require(bezier_profile.GetNumLines() == 5 && bezier_profile.IsClosed(),
            "Bezier split changed the profile topology.");
    for (int sample = 0; sample <= 20; ++sample) {
        const double parameter = sample / 20.0;
        const CPoint3d reconstructed = parameter <= split_parameter
            ? bezier_profile.GetLine(0)->GetPoint(
                parameter / split_parameter)
            : bezier_profile.GetLine(1)->GetPoint(
                (parameter - split_parameter) / (1.0 - split_parameter));
        const CPoint3d& original =
            original_bezier_points[static_cast<std::size_t>(sample)];
        require(std::hypot(
                    std::hypot(reconstructed.x - original.x,
                               reconstructed.y - original.y),
                    reconstructed.z - original.z) < 1.0e-8,
                "Bezier shape changed after exact split.");
    }

    CSmartLine tangent_profile("Bezier tangency constraints");
    require(tangent_profile.Add(new CLinkLine(
                CPoint3d(0.0, 0.0, 0.0), CPoint3d(10.0, 0.0, 0.0)))
            && tangent_profile.Add(new CBezierSpline(
                CPoint3d(10.0, 0.0, 0.0), CPoint3d(12.0, 4.0, 0.0),
                CPoint3d(6.0, 8.0, 0.0), CPoint3d(10.0, 10.0, 0.0)))
            && tangent_profile.Add(new CLinkLine(
                CPoint3d(10.0, 10.0, 0.0), CPoint3d(10.0, 20.0, 0.0))),
            "Tangency test sketch could not be created.");
    require(tangent_profile.ConstrainBezierTangentAtStart(1)
                && tangent_profile.ConstrainBezierTangentAtEnd(1),
            "Bezier tangency constraints were rejected.");
    const auto* tangent_bezier = dynamic_cast<const CBezierSpline*>(
        tangent_profile.GetLine(1));
    require(tangent_bezier
                && std::abs(tangent_bezier->GetControl1().y) < 1.0e-9
                && std::abs(tangent_bezier->GetControl2().x - 10.0) < 1.0e-9
                && tangent_bezier->GetControl2().y < 10.0,
            "Bezier handles are not tangent to the adjacent segments.");

    CSmartLine smooth_pair("Bidirectional Bezier tangency");
    require(smooth_pair.Add(new CBezierSpline(
                CPoint3d(0.0, 0.0, 0.0), CPoint3d(2.0, 0.0, 0.0),
                CPoint3d(7.0, 0.0, 0.0), CPoint3d(10.0, 0.0, 0.0)))
            && smooth_pair.Add(new CBezierSpline(
                CPoint3d(10.0, 0.0, 0.0), CPoint3d(13.0, 0.0, 0.0),
                CPoint3d(18.0, 6.0, 0.0), CPoint3d(20.0, 6.0, 0.0)))
            && smooth_pair.ConstrainBezierTangentAtStart(1),
            "Smooth Bezier pair could not be created.");
    require(smooth_pair.MoveBezierControlPointWorld(
                2, smooth_pair.LocalToWorld(CPoint3d(12.0, 5.0, 0.0))),
            "Constrained Bezier handle could not drive the opposite handle.");
    const auto* smooth_left = dynamic_cast<const CBezierSpline*>(
        smooth_pair.GetLine(0));
    const auto* smooth_right = dynamic_cast<const CBezierSpline*>(
        smooth_pair.GetLine(1));
    require(smooth_left && smooth_right,
            "Smooth pair no longer contains two Bezier curves.");
    const CPoint3d left_tangent(
        smooth_left->GetEnd().x - smooth_left->GetControl2().x,
        smooth_left->GetEnd().y - smooth_left->GetControl2().y, 0.0);
    const CPoint3d right_tangent(
        smooth_right->GetControl1().x - smooth_right->GetStart().x,
        smooth_right->GetControl1().y - smooth_right->GetStart().y, 0.0);
    require(std::abs(left_tangent.x * right_tangent.y
                            - left_tangent.y * right_tangent.x) < 1.0e-9
                && left_tangent.x * right_tangent.x
                     + left_tangent.y * right_tangent.y > 0.0,
            "Bezier smoothness did not remain bidirectional.");
    const auto smooth_at = [](const CSmartLine& sketch, size_t incoming, size_t outgoing) {
        const auto a = sketch.GetLine(incoming)->GetTangent(1.0);
        const auto b = sketch.GetLine(outgoing)->GetTangent(0.0);
        const double lengths = std::hypot(a.x, a.y) * std::hypot(b.x, b.y);
        return lengths > 1.e-12 && std::abs(a.x*b.y-a.y*b.x) / lengths < 1.e-10
            && a.x*b.x+a.y*b.y > 0;
    };
    CSmartLine joints("Node smoothness");
    require(joints.Add(new CBezierSpline({0,0,0}, {2,1,0}, {7,-2,0}, {10,0,0}))
         && joints.Add(new CBezierSpline({10,0,0}, {12,-3,0}, {18,4,0}, {20,5,0})),
            "Cannot create kinked Bezier pair");
    require(!smooth_at(joints,0,1) && joints.SetNodeSmooth(1,true) && smooth_at(joints,0,1),
            "Node tool did not align the two tangents");
    require(joints.GetNumConstraints()==1 && joints.SetNodeSmooth(1,true)
            && joints.GetNumConstraints()==1, "Smoothing duplicated a joint constraint");
    require(joints.MoveBezierControlPointWorld(1,joints.LocalToWorld({8,-5,0})) && smooth_at(joints,0,1),
            "Incoming handle lost joint smoothness");
    require(joints.MoveBezierControlPointWorld(2,joints.LocalToWorld({14,2,0})) && smooth_at(joints,0,1),
            "Outgoing handle lost joint smoothness");
    require(joints.MoveNodeWorld(1,joints.LocalToWorld({11,1,0})) && smooth_at(joints,0,1),
            "Moving the joint lost smoothness");
    const auto before_sharp = joints.GetBezierControlPointWorld(1);
    require(joints.SetNodeSmooth(1,false) && joints.GetNumConstraints()==0,
            "Sharp joint did not remove its constraint");
    const auto after_sharp = joints.GetBezierControlPointWorld(1);
    require(before_sharp.x==after_sharp.x && before_sharp.y==after_sharp.y
            && before_sharp.z==after_sharp.z, "Removing smoothness changed the curve");
    require(joints.MoveBezierControlPointWorld(2,joints.LocalToWorld({15,-4,0})) && !smooth_at(joints,0,1),
            "Sharp joint still couples its two handles");
    require(!joints.SetNodeSmooth(0,true) && !joints.SetNodeSmooth(2,true),
            "A free endpoint was accepted as a joint");
    require(joints.ConstrainBezierTangentAtStart(1) && joints.ConstrainBezierTangentAtEnd(0)
            && joints.SetNodeSmooth(1,false) && joints.GetNumConstraints()==0,
            "Sharp joint did not remove both legacy tangency representations");
    require(tangent_profile.SetNodeSmooth(1,false) && tangent_profile.SetNodeSmooth(1,true),
            "Line-Bezier joint was rejected");
    require(tangent_profile.MoveBezierControlPointWorld(0,tangent_profile.LocalToWorld({13,7,0}))
            && smooth_at(tangent_profile,0,1), "Line-Bezier smoothness was not preserved");

    CSmartLine loop("Closing joint");
    require(loop.Add(new CBezierSpline({0,0,0},{4,-1,0},{8,-2,0},{10,0,0}))
         && loop.Add(new CBezierSpline({10,0,0},{12,4,0},{4,8,0},{0,0,0}))
         && loop.SetClosed(true), "Cannot make closed joint fixture");
    // Closed sketches enumerate end nodes first: the closing joint is node 1.
    require(loop.SetNodeSmooth(1,true) && smooth_at(loop,1,0), "Closing node was not smoothed");
    require(loop.MoveBezierControlPointWorld(0,loop.LocalToWorld({4,-3,0})) && smooth_at(loop,1,0),
            "Closing-node handle did not propagate");
    require(loop.SetNodeSmooth(1,false) && loop.GetNumConstraints()==0, "Closing smoothness was not removed");
    return EXIT_SUCCESS;
} catch (const std::exception& error) {
    std::cerr << error.what() << std::endl; return EXIT_FAILURE;
}
