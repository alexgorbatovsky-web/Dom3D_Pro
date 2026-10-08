#include "solid/OffsetFaceShapeBuilder.h"
#include "SurfaceOffsetBuilder.h"
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakePolygon.hxx>
#include <BRep_Tool.hxx>
#include <gp_Pln.hxx>
#include <limits>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <Geom_BezierSurface.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include "solid/TrimShapeBuilder.h"

#include <BRepAdaptor_Surface.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

double volume(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}

TopoDS_Face face_of_type(const TopoDS_Shape& shape,
                         GeomAbs_SurfaceType type,
                         bool last = false) {
    TopoDS_Face found;
    for (TopExp_Explorer explorer(shape, TopAbs_FACE);
         explorer.More(); explorer.Next()) {
        const TopoDS_Face face = TopoDS::Face(explorer.Current());
        if (BRepAdaptor_Surface(face, true).GetType() == type) {
            found = face;
            if (!last) {
                break;
            }
        }
    }
    return found;
}
}

int main() {
    const TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape();
    const TopoDS_Face box_face = face_of_type(box, GeomAbs_Plane, true);
    require(!box_face.IsNull(), "Box face was not found.");
    const double box_volume = volume(box);

    TopoDS_Shape box_out;
    require(BuildOffsetFaceShape(box, box_face, 2.0, box_out),
            "Positive box face offset failed.");
    require(volume(box_out) > box_volume, "Positive offset did not grow box.");

    TopoDS_Shape box_in;
    require(BuildOffsetFaceShape(box, box_face, -2.0, box_in),
            "Negative box face offset failed.");
    require(volume(box_in) < box_volume, "Negative offset did not shrink box.");

    const TopoDS_Shape cylinder =
        BRepPrimAPI_MakeCylinder(5.0, 10.0).Shape();
    const TopoDS_Face cylinder_face =
        face_of_type(cylinder, GeomAbs_Cylinder);
    require(!cylinder_face.IsNull(), "Cylinder face was not found.");
    const double cylinder_volume = volume(cylinder);
    TopoDS_Shape cylinder_out;
    require(BuildOffsetFaceShape(
                cylinder, cylinder_face, 1.0, cylinder_out),
            "Cylindrical face offset failed.");
    require(volume(cylinder_out) > cylinder_volume,
            "Cylindrical offset did not grow cylinder.");

    const TopoDS_Shape trim_box =
        BRepPrimAPI_MakeBox(gp_Pnt(-10.0, -10.0, -10.0),
                            20.0, 20.0, 20.0).Shape();
    const TopoDS_Shape trim_cylinder =
        BRepPrimAPI_MakeCylinder(
            gp_Ax2(gp_Pnt(0.0, 0.0, -20.0), gp_Dir(0.0, 0.0, 1.0)),
            12.0,
            40.0).Shape();
    const TopoDS_Face trim_cutter =
        face_of_type(trim_cylinder, GeomAbs_Cylinder);
    TopoDS_Shape trimmed;
    require(TrimSolidByFace(trim_box, trim_cutter, false, trimmed),
            "Curved trim for offset test failed.");
    const TopoDS_Face trimmed_curved_face =
        face_of_type(trimmed, GeomAbs_Cylinder);
    require(!trimmed_curved_face.IsNull(),
            "Trimmed cylindrical face was not found.");
    TopoDS_Shape trimmed_offset;
    require(BuildOffsetFaceShape(
                trimmed, trimmed_curved_face, 2.0, trimmed_offset),
            "Offset after curved trim failed.");
    require(std::fabs(volume(trimmed_offset) - volume(trimmed)) > 1.0,
            "Offset after curved trim did not change the body.");
    std::string error;
    const auto plane = BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(0,0,0),gp_Dir(0,0,1)),0,20,0,10).Face();
    for (double distance : {-3., 2.}) {
        for (bool reverse : {false, true}) {
            auto source = plane; if (reverse) source.Reverse();
            const auto offset = BuildSurfaceOffset(source, distance, error);
            require(!offset.IsNull(), error.c_str());
            GProp_GProps props; BRepGProp::SurfaceProperties(offset, props);
            require(std::abs(props.CentreOfMass().Z() - distance*(reverse?-1:1)) < 1.e-6, "Offset ignored distance or face orientation");
            require(std::abs(props.Mass()-200) < 1.e-6, "Planar offset changed area");
            require(!TopExp_Explorer(offset,TopAbs_SOLID).More(), "Offset created a solid");
        }
    }
    const auto triangle = BRepBuilderAPI_MakeFace(BRepBuilderAPI_MakePolygon(gp_Pnt(0,0,0),gp_Pnt(10,0,0),gp_Pnt(0,10,0),true).Wire()).Face();
    const auto triangleOffset = BuildSurfaceOffset(triangle, 2, error);
    require(!triangleOffset.IsNull(), error.c_str());
    GProp_GProps triangleProps; BRepGProp::SurfaceProperties(triangleOffset,triangleProps);
    require(std::abs(triangleProps.Mass()-50)<1.e-6, "Offset lost trimmed boundary");
    auto inner = BRepBuilderAPI_MakePolygon(gp_Pnt(5,2,0),gp_Pnt(10,2,0),gp_Pnt(10,6,0),gp_Pnt(5,6,0),true).Wire();
    inner.Reverse();
    BRepBuilderAPI_MakeFace holed(plane); holed.Add(inner);
    const auto holeOffset = BuildSurfaceOffset(holed.Face(),2,error);
    require(!holeOffset.IsNull(), error.c_str());
    GProp_GProps holeProps; BRepGProp::SurfaceProperties(holeOffset,holeProps);
    require(std::abs(holeProps.Mass()-180)<1.e-6,"Offset filled an inner hole");
    TColgp_Array2OfPnt poles(1,3,1,3);
    for(int i=1;i<=3;++i)for(int j=1;j<=3;++j)poles(i,j)=gp_Pnt((i-1)*20,(j-1)*20,i==2&&j==2?8:0);
    Handle(Geom_BezierSurface) curved = new Geom_BezierSurface(poles);
    const auto curvedFace = BRepBuilderAPI_MakeFace(curved,1.e-7).Face();
    for(double d : {-2.,2.}) {
        const auto curvedOffset=BuildSurfaceOffset(curvedFace,d,error);
        require(!curvedOffset.IsNull(),error.c_str());
        for(double u : {.2,.5,.8})for(double v : {.2,.5,.8}) {
            gp_Pnt p;gp_Vec du,dv;curved->D1(u,v,p,du,dv);
            p.Translate(du.Crossed(dv).Normalized()*d);
            BRepExtrema_DistShapeShape gap(BRepBuilderAPI_MakeVertex(p).Shape(),curvedOffset);
            require(gap.IsDone()&&gap.Value()<1.e-3,"Curved offset deviates from normal distance");
        }
    }
    auto cylinderOffset = BuildSurfaceOffset(cylinder_face, 1, error);
    require(!cylinderOffset.IsNull(), error.c_str());
    auto offsetCylinderFace = face_of_type(cylinderOffset,GeomAbs_Cylinder);
    require(!offsetCylinderFace.IsNull(), "Cylinder offset lost analytic surface");
    require(std::abs(BRepAdaptor_Surface(offsetCylinderFace).Cylinder().Radius()-6)<1.e-6,"Wrong offset cylinder radius");
    require(BuildSurfaceOffset(plane,0,error).IsNull()&&!error.empty(),"Zero offset accepted");
    require(BuildSurfaceOffset(plane,std::numeric_limits<double>::quiet_NaN(),error).IsNull(),"NaN offset accepted");
    require(BuildSurfaceOffset({},2,error).IsNull(),"Empty source accepted");
    require(BuildSurfaceOffset(box,2,error).IsNull(),"Solid body accepted");
    require(std::abs(BRep_Tool::Surface(plane)->Value(5,5).Z())<1.e-8,"Source surface was changed");
    return EXIT_SUCCESS;
}
