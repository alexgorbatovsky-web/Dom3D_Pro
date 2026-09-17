#include "PanelContourShapeBuilder.h"
#include <BRepAlgoAPI_Splitter.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepOffsetAPI_MakeOffset.hxx>
#include <BRepOffsetAPI_MakeOffsetShape.hxx>
#include <BRepOffsetAPI_MakeFilling.hxx>
#include <BRepOffsetAPI_MakeThickSolid.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepProj_Projection.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <BRep_Builder.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <Standard_Failure.hxx>
#include <ShapeFix_Face.hxx>
#include <gp_Pln.hxx>
#include <gp_Trsf.hxx>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
double area(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(shape, properties);
    return properties.Mass();
}
TopoDS_Shape prism(const TopoDS_Shape& shape, const gp_Dir& direction, double reach) {
    gp_Trsf shift;
    shift.SetTranslation(gp_Vec(direction) * -reach);
    const TopoDS_Shape start = BRepBuilderAPI_Transform(shape, shift, true).Shape();
    return BRepPrimAPI_MakePrism(start, gp_Vec(direction) * (2 * reach)).Shape();
}
}

bool BuildThickenedPanel(const TopoDS_Shape& panel, double thickness,
    TopoDS_Shape& result, std::string& error) {
    result.Nullify(); error.clear();
    if (panel.IsNull() || !std::isfinite(thickness) || std::abs(thickness)<1.e-7) {
        error="Panel thickness must be nonzero and finite."; return false;
    }
    try {
        TopExp_Explorer faces(panel,TopAbs_FACE);
        if (!faces.More()) {error="The panel has no face.";return false;}
        const TopoDS_Face face=TopoDS::Face(faces.Current()); faces.Next();
        if (faces.More()) {error="Thickening requires a single panel face.";return false;}
        BRepOffsetAPI_MakeThickSolid builder;
        builder.MakeThickSolidBySimple(face,-thickness);
        if (!builder.IsDone() || builder.Shape().IsNull()
            || !BRepCheck_Analyzer(builder.Shape()).IsValid()) {
            error="Cannot build a valid panel at this thickness.";return false;
        }
        int count=0;
        for(TopExp_Explorer solids(builder.Shape(),TopAbs_SOLID);solids.More();solids.Next()) ++count;
        GProp_GProps volume; BRepGProp::VolumeProperties(builder.Shape(),volume);
        if(count!=1 || std::abs(volume.Mass())<1.e-9) {
            error="The thickened panel is not a closed body.";return false;
        }
        result=builder.Shape();return true;
    } catch(const Standard_Failure& failure) {
        error=failure.GetMessageString() ? failure.GetMessageString() : "Panel thickening failed.";return false;
    }
}

bool BuildBulgedPanelFace(const TopoDS_Face& source, double height,
    TopoDS_Shape& result, std::string& error, double profile) {
    result.Nullify(); error.clear();
    if (source.IsNull() || !std::isfinite(height)) {
        error = "Select a panel and a finite bulge height."; return false;
    }
    if (!std::isfinite(profile) || (profile!=0 && (profile<0.25 || profile>0.75))) {
        error="Bulge profile must be between 0.25 and 0.75.";return false;
    }
    if (std::abs(height) < 1.e-7) { result = source; return true; }
    try {
        int wires = 0;
        for (TopExp_Explorer it(source, TopAbs_WIRE); it.More(); it.Next()) ++wires;
        if (wires != 1) {
            error = "Bulge requires one panel without inner holes."; return false;
        }
        const auto boundary = BRepTools::OuterWire(source);
        double u0,u1,v0,v1;
        BRepTools::UVBounds(source,u0,u1,v0,v1);
        BRepAdaptor_Surface surface(source);
        struct Sample { gp_Pnt point; gp_Vec normal; double distance; double center_distance; double u,v; };
        std::vector<Sample> samples;
        for (int i=1; i<6; ++i) for (int j=1; j<6; ++j) {
            const double u=u0+(u1-u0)*i/6, v=v0+(v1-v0)*j/6;
            BRepClass_FaceClassifier classifier(source,gp_Pnt2d(u,v),1.e-7);
            if (classifier.State()!=TopAbs_IN) continue;
            gp_Pnt point; gp_Vec du,dv;
            surface.D1(u,v,point,du,dv);
            gp_Vec normal=du.Crossed(dv);
            if (normal.SquareMagnitude()<1.e-18) continue;
            normal.Normalize();
            if (source.Orientation()==TopAbs_REVERSED) normal.Reverse();
            BRepExtrema_DistShapeShape distance(BRepBuilderAPI_MakeVertex(point).Vertex(),boundary);
            if (!distance.IsDone() || distance.Value()<1.e-6) continue;
            samples.push_back({point,normal,distance.Value(),double((i-3)*(i-3)+(j-3)*(j-3)),u,v});
        }
        if (samples.empty()) { error="The panel is too narrow to form a bulge."; return false; }
        BRepOffsetAPI_MakeFilling filling(4,24,3,Standard_True,
            1.e-5,1.e-4,0.01,0.1,10,20);
        // LoadInitSurface requires orthogonal local coordinates. Arbitrary
        // Smart Hybrid patches do not satisfy that requirement; using them
        // as an initial plate produces large interior oscillations. Let OCCT
        // construct its initial support from the boundary constraints instead.
        // A plane does have orthogonal coordinates. Its known support avoids
        // an unstable fitted initial plate on long, slightly skewed panels.
        if (surface.GetType()==GeomAbs_Plane) filling.LoadInitSurface(source);
        for (TopExp_Explorer it(boundary,TopAbs_EDGE); it.More(); it.Next())
            filling.Add(TopoDS::Edge(it.Current()),source,GeomAbs_G1,Standard_True);
        // On long skewed panels the maximal-clearance region is almost flat.
        // Tiny differences must not push the apex toward an end of the panel.
        const double clearance=std::max_element(samples.begin(),samples.end(),
            [](const auto& a,const auto& b){return a.distance<b.distance;})->distance;
        auto apex=samples.end();
        for(auto it=samples.begin();it!=samples.end();++it) {
            if(it->distance<clearance*0.95) continue;
            if(apex==samples.end() || it->center_distance<apex->center_distance) apex=it;
        }
        // Keep the legacy single-height construction when no profile was saved.
        filling.Add(apex->point.Translated(apex->normal*height));
        if(profile!=0) {
            // Constrain the crown as well as the shoulders: one outer ring
            // alone lets the plate rise above the apex and form a saddle.
            // Find the first trim boundary on each ray so all constraints
            // remain inside an irregular panel.
            for(int ray=0;ray<8;++ray) {
                const double angle=ray*3.141592653589793/4;
                const double ru=std::cos(angle)*(u1-u0),rv=std::sin(angle)*(v1-v0);
                auto inside=[&](double t) {
                    BRepClass_FaceClassifier c(source,gp_Pnt2d(apex->u+t*ru,apex->v+t*rv),1.e-7);
                    return c.State()==TopAbs_IN;
                };
                double lo=0,hi=0;
                for(int step=1;step<=64;++step) {
                    hi=step/32.0;
                    if(!inside(hi)) break;
                    lo=hi;
                }
                for(int step=0;step<30;++step) {
                    const double mid=(lo+hi)/2;
                    if(inside(mid)) lo=mid; else hi=mid;
                }
                if(lo<1.e-6) continue;
                for(double radius:{0.2,0.35,0.5}) {
                    const double t=lo*radius;
                    gp_Pnt point;gp_Vec du,dv;
                    surface.D1(apex->u+t*ru,apex->v+t*rv,point,du,dv);
                    gp_Vec normal=du.Crossed(dv);
                    if(normal.SquareMagnitude()<1.e-18) continue;
                    normal.Normalize();if(source.Orientation()==TopAbs_REVERSED) normal.Reverse();
                    const double shoulder=std::pow(std::cos(radius*3.141592653589793/2),std::exp2(2-4*profile));
                    filling.Add(point.Translated(normal*(height*shoulder)));
                }
            }
        }
        filling.Build();
        if (!filling.IsDone() || filling.Shape().IsNull() ||
            !std::isfinite(filling.G0Error()) || filling.G0Error()>0.005 ||
            !BRepCheck_Analyzer(filling.Shape()).IsValid()) {
            error="Cannot build this bulge while preserving the boundary. Reduce the height. Constraint error: "
                +std::to_string(filling.G0Error()); return false;
        }
        result=filling.Shape();
        // Match the oriented normal of the input, including reversed faces.
        TopExp_Explorer face_it(result,TopAbs_FACE);
        if (!face_it.More()) { result.Nullify(); error="Bulge produced no face."; return false; }
        const auto built=TopoDS::Face(face_it.Current());
        BRepAdaptor_Surface output(built);
        double a,b,c,d; BRepTools::UVBounds(built,a,b,c,d);
        // G0 error measures the supplied constraints only. It does not detect
        // an otherwise valid plate oscillating between those constraints.
        // Check the interior against the source before publishing the shape.
        const double maximum_deviation=std::abs(height)*(profile==0 ? 1.5 : 1.005)+0.005;
        const int divisions=profile==0 ? 12 : 24;
        for (int i=1;i<divisions;++i) for (int j=1;j<divisions;++j) {
            const double u=a+(b-a)*i/divisions, v=c+(d-c)*j/divisions;
            BRepClass_FaceClassifier classifier(built,gp_Pnt2d(u,v),1.e-7);
            if (classifier.State()!=TopAbs_IN) continue;
            const gp_Pnt point=output.Value(u,v);
            BRepExtrema_DistShapeShape deviation(BRepBuilderAPI_MakeVertex(point).Vertex(),source);
            if (!deviation.IsDone() || !std::isfinite(deviation.Value()) || deviation.Value()>maximum_deviation) {
                result.Nullify();
                error="Cannot form a controlled bulge on this panel without interior overshoot.";
                return false;
            }
        }
        gp_Pnt p; gp_Vec du,dv; output.D1((a+b)/2,(c+d)/2,p,du,dv);
        gp_Vec n=du.Crossed(dv);
        if (built.Orientation()==TopAbs_REVERSED) n.Reverse();
        if (n.Dot(apex->normal)<0) result.Reverse();
        return true;
    } catch (const Standard_Failure& failure) {
        error=std::string("Bulge failed: ")+(failure.GetMessageString()?failure.GetMessageString():"CAD error");
        return false;
    }
}

bool BuildPanelContourShape(const TopoDS_Shape& body, const TopoDS_Face& face,
    const TopoDS_Wire& contour, const gp_Dir& direction, bool larger_region,
    double gap, double depth, PanelContourResult& result, std::string& error, double thickness) {
    result = {};
    error.clear();
    const auto fail = [&](const char* message) { error = message; return false; };
    if (!std::isfinite(thickness) || thickness < 0)
        return fail("Thickness must be finite and non-negative.");
    if (body.IsNull() || face.IsNull() || contour.IsNull())
        return fail("Select a surface and a curve.");
    if (!std::isfinite(gap) || gap < 0 || !std::isfinite(depth))
        return fail("Gap must be finite and non-negative; depth must be finite.");
    try {
        Bnd_Box bounds;
        BRepBndLib::Add(body, bounds);
        BRepBndLib::Add(contour, bounds);
        double x0,y0,z0,x1,y1,z1;
        bounds.Get(x0,y0,z0,x1,y1,z1);
        const gp_Pnt center((x0+x1)/2, (y0+y1)/2, (z0+z1)/2);
        const double reach = std::max(1.0, gp_Pnt(x0,y0,z0).Distance(gp_Pnt(x1,y1,z1)) * 2);
        TopTools_ListOfShape arguments, tools;
        arguments.Append(face);
        tools.Append(prism(contour, direction, reach));
        BRepAlgoAPI_Splitter split;
        split.SetArguments(arguments);
        split.SetTools(tools);
        split.SetNonDestructive(true);
        split.SetFuzzyValue(1.e-6);
        split.Build();
        if (!split.IsDone()) return fail("Cannot project and split this surface. Try a more frontal view.");
        std::vector<TopoDS_Face> regions;
        for (TopExp_Explorer it(split.Shape(), TopAbs_FACE); it.More(); it.Next())
            regions.push_back(TopoDS::Face(it.Current()));
        // A snapped spline endpoint can miss the exact boundary by a few
        // 1e-5 model units after curve fitting/projection. Retry only an open
        // contour that left the face uncut, with a small bounded tolerance.
        // Successful exact cuts and closed contours keep their original result.
        if (regions.size()==1 && !BRep_Tool::IsClosed(contour)) {
            split.SetFuzzyValue(1.e-4);
            split.Build();
            if (split.IsDone()) {
                regions.clear();
                for (TopExp_Explorer it(split.Shape(),TopAbs_FACE);it.More();it.Next())
                    regions.push_back(TopoDS::Face(it.Current()));
            }
        }
        if (regions.size() != 2)
            return fail("The curve must form one closed contour or cross the surface from boundary to boundary, producing two regions.");
        std::sort(regions.begin(), regions.end(), [](const auto& a, const auto& b) { return area(a) < area(b); });
        const size_t selected = larger_region ? 1 : 0;
        TopoDS_Shape panel = regions[selected];

        if (gap > 1.e-7) {
            // Inset a planar projection, then intersect its prism with the
            // exact curved patch. The panel remains on the original surface.
            const TopoDS_Face plane = BRepBuilderAPI_MakeFace(
                gp_Pln(center, direction), -reach, reach, -reach, reach).Face();
            BRepProj_Projection projection(BRepTools::OuterWire(regions[selected]), plane, direction);
            if (!projection.More()) return fail("Cannot construct the panel's gap in this view.");
            const TopoDS_Wire outline = projection.Current();
            projection.Next();
            if (projection.More()) return fail("The panel overlaps itself in this view. Choose another view.");
            BRepBuilderAPI_MakeFace footprint_builder(BRep_Tool::Surface(plane), outline, true);
            const TopoDS_Wire outer = BRepTools::OuterWire(regions[selected]);
            for (TopExp_Explorer it(regions[selected], TopAbs_WIRE); it.More(); it.Next()) {
                if (it.Current().IsSame(outer)) continue;
                BRepProj_Projection hole(TopoDS::Wire(it.Current()), plane, direction);
                if (!hole.More()) return fail("Cannot project a panel hole in this view.");
                const TopoDS_Face hole_face = BRepBuilderAPI_MakeFace(BRep_Tool::Surface(plane), hole.Current(), true).Face();
                TopoDS_Wire hole_wire = BRepTools::OuterWire(hole_face);
                hole_wire.Reverse();
                footprint_builder.Add(hole_wire);
                hole.Next();
                if (hole.More()) return fail("A panel hole overlaps itself in this view.");
            }
            ShapeFix_Face footprint_fixer(footprint_builder.Face());
            footprint_fixer.Perform();
            const TopoDS_Face footprint = footprint_fixer.Face();
            if (!BRepCheck_Analyzer(footprint).IsValid()) return fail("The projected panel boundary is invalid.");
            double u0,u1,v0,v1;
            BRepTools::UVBounds(footprint,u0,u1,v0,v1);
            if (gap >= 0.5 * std::min(u1-u0,v1-v0))
                return fail("The gap is too large for this panel.");
            const double original_area = area(footprint);
            TopoDS_Face inset;
            for (double sign : {-1.0, 1.0}) {
                BRepOffsetAPI_MakeOffset offset(footprint, GeomAbs_Arc);
                offset.Perform(sign * gap);
                if (!offset.IsDone() || offset.Shape().IsNull()) continue;
                std::vector<TopoDS_Wire> wires;
                for (TopExp_Explorer it(offset.Shape(), TopAbs_WIRE); it.More(); it.Next())
                    wires.push_back(TopoDS::Wire(it.Current()));
                if (wires.empty()) continue;
                std::sort(wires.begin(), wires.end(), [](const auto& a, const auto& b) {
                    return area(BRepBuilderAPI_MakeFace(a, true).Face()) > area(BRepBuilderAPI_MakeFace(b, true).Face());
                });
                BRepBuilderAPI_MakeFace candidate_builder(BRep_Tool::Surface(footprint), wires.front(), true);
                for (size_t i = 1; i < wires.size(); ++i) {
                    const TopoDS_Face hole_face = BRepBuilderAPI_MakeFace(BRep_Tool::Surface(footprint), wires[i], true).Face();
                    TopoDS_Wire hole_wire = BRepTools::OuterWire(hole_face);
                    hole_wire.Reverse();
                    candidate_builder.Add(hole_wire);
                }
                ShapeFix_Face candidate_fixer(candidate_builder.Face());
                candidate_fixer.Perform();
                const TopoDS_Face candidate = candidate_fixer.Face();
                if (!BRepCheck_Analyzer(candidate).IsValid()) continue;
                const double candidate_area = area(candidate);
                if (candidate_area <= 1.e-9 || candidate_area >= original_area - 1.e-8) continue;
                BRepAlgoAPI_Cut outside(candidate, footprint);
                if (!outside.IsDone() || area(outside.Shape()) > original_area * 1.e-7) continue;
                inset = candidate;
                break;
            }
            if (inset.IsNull()) return fail("The gap is too large or the panel outline cannot be inset.");
            BRepAlgoAPI_Common clipped(panel, prism(inset, direction, reach));
            if (!clipped.IsDone() || !TopExp_Explorer(clipped.Shape(), TopAbs_FACE).More())
                return fail("The gap removes the entire panel.");
            panel = clipped.Shape();
        }
        if (std::abs(depth) > 1.e-7) {
            BRepOffsetAPI_MakeOffsetShape offset;
            offset.PerformBySimple(panel, -depth);
            if (!offset.IsDone() || offset.Shape().IsNull())
                return fail("Cannot recess the panel by this depth.");
            panel = offset.Shape();
        }
        if (panel.IsNull() || !BRepCheck_Analyzer(panel).IsValid())
            return fail("The resulting panel is invalid. Reduce gap or depth.");
        BRep_Builder builder;
        TopoDS_Compound remainder;
        builder.MakeCompound(remainder);
        bool found_face = false;
        for (TopExp_Explorer it(body, TopAbs_FACE); it.More(); it.Next()) {
            if (it.Current().IsSame(face)) {
                found_face = true;
                builder.Add(remainder, regions[1 - selected]);
            } else builder.Add(remainder, it.Current());
        }
        if (!found_face) return fail("The selected face no longer belongs to this body.");
        if (!BRepCheck_Analyzer(remainder).IsValid()) return fail("The remaining surface is invalid.");
        if (thickness > 0) {
            TopoDS_Shape thick;
            if (!BuildThickenedPanel(panel,thickness,thick,error)) return false;
            panel=thick;
        }
        result.panel = panel;
        result.remainder = remainder;
        return true;
    } catch (const Standard_Failure& failure) {
        error = std::string("Panel construction failed: ") + (failure.GetMessageString() ? failure.GetMessageString() : "CAD error");
        return false;
    }
}
