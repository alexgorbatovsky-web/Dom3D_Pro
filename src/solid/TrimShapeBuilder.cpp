#include "TrimShapeBuilder.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Splitter.hxx>
#include <BRep_Builder.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp_Face.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeHalfSpace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <GProp_GProps.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <ShapeFix_Shape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopTools_ListOfShape.hxx>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
double BodyExtent(const TopoDS_Shape& body) {
    Bnd_Box bounds;
    BRepBndLib::Add(body, bounds);
    if (bounds.IsVoid()) {
        return 1000.0;
    }
    double x_min = 0.0;
    double y_min = 0.0;
    double z_min = 0.0;
    double x_max = 0.0;
    double y_max = 0.0;
    double z_max = 0.0;
    bounds.Get(x_min, y_min, z_min, x_max, y_max, z_max);
    const double diagonal = std::sqrt(
        (x_max - x_min) * (x_max - x_min)
        + (y_max - y_min) * (y_max - y_min)
        + (z_max - z_min) * (z_max - z_min));
    return std::max(100.0, diagonal * 4.0);
}

bool HasSolid(const TopoDS_Shape& shape) {
    if (shape.IsNull()) {
        return false;
    }
    if (shape.ShapeType() == TopAbs_SOLID) {
        return true;
    }
    TopExp_Explorer explorer(shape, TopAbs_SOLID);
    return explorer.More();
}

bool FinishBoolean(BRepAlgoAPI_BooleanOperation& operation,
                   TopoDS_Shape& result) {
    operation.Build();
    if (!operation.IsDone()
        || operation.Shape().IsNull()
        || !HasSolid(operation.Shape())
        || !BRepCheck_Analyzer(operation.Shape()).IsValid()) {
        return false;
    }
    result = operation.Shape();
    return true;
}

bool Normalize(Vec3& vector) {
    const float length = std::sqrt(
        vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
    if (length <= 1.0e-7f) {
        return false;
    }
    vector.x /= length;
    vector.y /= length;
    vector.z /= length;
    return true;
}

bool ExpandedPlanarFace(const TopoDS_Shape& body,
                        const TopoDS_Face& source,
                        TopoDS_Face& expanded,
                        gp_Pnt& plane_point,
                        gp_Vec& plane_normal) {
    try {
        BRepAdaptor_Surface adaptor(source, true);
        if (adaptor.GetType() != GeomAbs_Plane) {
            return false;
        }

        const gp_Pln plane = adaptor.Plane();
        plane_point = plane.Location();
        plane_normal = gp_Vec(plane.Axis().Direction());
        if (source.Orientation() == TopAbs_REVERSED) {
            plane_normal.Reverse();
        }

        Bnd_Box bounds;
        BRepBndLib::Add(body, bounds);
        if (bounds.IsVoid()) {
            return false;
        }
        double x_min = 0.0;
        double y_min = 0.0;
        double z_min = 0.0;
        double x_max = 0.0;
        double y_max = 0.0;
        double z_max = 0.0;
        bounds.Get(x_min, y_min, z_min, x_max, y_max, z_max);
        const gp_Pnt body_center(
            (x_min + x_max) * 0.5,
            (y_min + y_max) * 0.5,
            (z_min + z_max) * 0.5);
        const gp_Vec to_center(plane_point, body_center);
        const double center_u =
            to_center.Dot(gp_Vec(plane.Position().XDirection()));
        const double center_v =
            to_center.Dot(gp_Vec(plane.Position().YDirection()));
        const double extent = BodyExtent(body);

        BRepBuilderAPI_MakeFace maker(
            plane,
            center_u - extent,
            center_u + extent,
            center_v - extent,
            center_v + extent);
        if (!maker.IsDone()) {
            return false;
        }
        expanded = maker.Face();
        if (source.Orientation() == TopAbs_REVERSED) {
            expanded.Reverse();
        }
        return !expanded.IsNull();
    } catch (...) {
        return false;
    }
}

bool SplitByFaceAndKeepSide(const TopoDS_Shape& body,
                            const TopoDS_Face& cutting_face,
                            const gp_Pnt& plane_point,
                            const gp_Vec& keep_normal,
                            TopoDS_Shape& result) {
    BRepAlgoAPI_Splitter splitter;
    TopTools_ListOfShape arguments;
    arguments.Append(body);
    TopTools_ListOfShape tools;
    tools.Append(cutting_face);
    splitter.SetArguments(arguments);
    splitter.SetTools(tools);
    splitter.SetFuzzyValue(1.0e-6);
    splitter.SetNonDestructive(true);
    splitter.Build();
    if (!splitter.IsDone() || splitter.Shape().IsNull()) {
        return false;
    }

    std::vector<TopoDS_Shape> kept;
    for (TopExp_Explorer explorer(splitter.Shape(), TopAbs_SOLID);
         explorer.More(); explorer.Next()) {
        TopoDS_Shape candidate = explorer.Current();
        GProp_GProps mass;
        BRepGProp::VolumeProperties(candidate, mass);
        const gp_Pnt center = mass.CentreOfMass();
        const gp_Vec offset(plane_point, center);
        if (offset.Dot(keep_normal) < -1.0e-7) {
            continue;
        }
        if (!BRepCheck_Analyzer(candidate).IsValid()) {
            ShapeFix_Shape fixer(candidate);
            fixer.Perform();
            candidate = fixer.Shape();
        }
        if (HasSolid(candidate) && BRepCheck_Analyzer(candidate).IsValid()) {
            kept.push_back(candidate);
        }
    }
    if (kept.empty()) {
        return false;
    }
    if (kept.size() == 1) {
        result = kept.front();
        return true;
    }

    TopoDS_Compound compound;
    BRep_Builder builder;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape& shape : kept) {
        builder.Add(compound, shape);
    }
    result = compound;
    return BRepCheck_Analyzer(result).IsValid();
}
}

bool TrimSolidByFace(const TopoDS_Shape& body,
                     const TopoDS_Face& cutting_face,
                     bool keep_positive_side,
                     TopoDS_Shape& result) {
    result.Nullify();
    if (body.IsNull() || cutting_face.IsNull()) {
        return false;
    }

    try {
        gp_Pnt point;
        gp_Vec normal;
        TopoDS_Face effective_face = cutting_face;
        if (!ExpandedPlanarFace(
                body, cutting_face, effective_face, point, normal)) {
            BRepGProp_Face properties(cutting_face);
            double u_min = 0.0;
            double u_max = 0.0;
            double v_min = 0.0;
            double v_max = 0.0;
            properties.Bounds(u_min, u_max, v_min, v_max);
            properties.Normal(
                (u_min + u_max) * 0.5,
                (v_min + v_max) * 0.5,
                point,
                normal);
            if (cutting_face.Orientation() == TopAbs_REVERSED) {
                normal.Reverse();
            }
        }
        if (normal.SquareMagnitude() <= 1.0e-18) {
            return false;
        }
        normal.Normalize();
        if (!keep_positive_side) {
            normal.Reverse();
        }

        const double reference_offset =
            std::max(1.0e-4, BodyExtent(body) * 1.0e-4);
        const gp_Pnt reference =
            point.Translated(normal * reference_offset);
        BRepPrimAPI_MakeHalfSpace half_space(effective_face, reference);
        half_space.Build();
        if (!half_space.IsDone() || half_space.Solid().IsNull()) {
            return false;
        }

        BRepAlgoAPI_Common common(body, half_space.Solid());
        common.SetFuzzyValue(1.0e-6);
        common.SetNonDestructive(true);
        if (FinishBoolean(common, result)) {
            return true;
        }

        // Swept mouldings often contain seam and miter topology that is
        // valid for rendering but fragile in a half-space boolean.  Splitting
        // by the actual plane face and selecting pieces by mass centre is a
        // more robust equivalent for those shapes.
        return SplitByFaceAndKeepSide(
            body, effective_face, point, normal, result);
    } catch (...) {
        return false;
    }
}

bool TrimSolidByClosedProfile(const TopoDS_Shape& body,
                              const TopoDS_Face& profile,
                              Vec3 extrusion_normal,
                              bool keep_inside,
                              TopoDS_Shape& result) {
    result.Nullify();
    if (body.IsNull() || profile.IsNull() || !Normalize(extrusion_normal)) {
        return false;
    }

    try {
        const double extent = BodyExtent(body);
        gp_Trsf shift;
        shift.SetTranslation(gp_Vec(
            -extrusion_normal.x * extent,
            -extrusion_normal.y * extent,
            -extrusion_normal.z * extent));
        BRepBuilderAPI_Transform moved(profile, shift, true);
        BRepPrimAPI_MakePrism prism(
            moved.Shape(),
            gp_Vec(
                extrusion_normal.x * extent * 2.0,
                extrusion_normal.y * extent * 2.0,
                extrusion_normal.z * extent * 2.0),
            true);
        prism.Build();
        if (!prism.IsDone() || prism.Shape().IsNull()) {
            return false;
        }

        if (keep_inside) {
            BRepAlgoAPI_Common common(body, prism.Shape());
            return FinishBoolean(common, result);
        }
        BRepAlgoAPI_Cut cut(body, prism.Shape());
        return FinishBoolean(cut, result);
    } catch (...) {
        return false;
    }
}

bool TrimSolidByOpenProfile(const TopoDS_Shape& body,
                            const TopoDS_Wire& profile,
                            Vec3 extrusion_normal,
                            Vec3 side_normal,
                            Vec3 side_origin,
                            bool keep_positive_side,
                            TopoDS_Shape& result) {
    result.Nullify();
    if (body.IsNull()
        || profile.IsNull()
        || !Normalize(extrusion_normal)
        || !Normalize(side_normal)) {
        return false;
    }

    try {
        const double extent = BodyExtent(body);
        gp_Trsf shift;
        shift.SetTranslation(gp_Vec(
            -extrusion_normal.x * extent,
            -extrusion_normal.y * extent,
            -extrusion_normal.z * extent));
        BRepBuilderAPI_Transform moved(profile, shift, true);
        BRepPrimAPI_MakePrism curtain(
            moved.Shape(),
            gp_Vec(
                extrusion_normal.x * extent * 2.0,
                extrusion_normal.y * extent * 2.0,
                extrusion_normal.z * extent * 2.0),
            true);
        curtain.Build();
        if (!curtain.IsDone() || curtain.Shape().IsNull()) {
            return false;
        }

        BRepAlgoAPI_Splitter splitter;
        TopTools_ListOfShape arguments;
        arguments.Append(body);
        TopTools_ListOfShape tools;
        tools.Append(curtain.Shape());
        splitter.SetArguments(arguments);
        splitter.SetTools(tools);
        splitter.Build();
        if (!splitter.IsDone() || splitter.Shape().IsNull()) {
            return false;
        }

        std::vector<TopoDS_Shape> kept;
        for (TopExp_Explorer explorer(splitter.Shape(), TopAbs_SOLID);
             explorer.More();
             explorer.Next()) {
            const TopoDS_Shape solid = explorer.Current();
            GProp_GProps mass;
            BRepGProp::VolumeProperties(solid, mass);
            const gp_Pnt center = mass.CentreOfMass();
            const double side =
                (center.X() - side_origin.x) * side_normal.x
                + (center.Y() - side_origin.y) * side_normal.y
                + (center.Z() - side_origin.z) * side_normal.z;
            if ((side >= 0.0) == keep_positive_side) {
                kept.push_back(solid);
            }
        }
        if (kept.empty()) {
            return false;
        }
        if (kept.size() == 1) {
            result = kept.front();
        } else {
            TopoDS_Compound compound;
            BRep_Builder builder;
            builder.MakeCompound(compound);
            for (const TopoDS_Shape& shape : kept) {
                builder.Add(compound, shape);
            }
            result = compound;
        }
        return HasSolid(result) && BRepCheck_Analyzer(result).IsValid();
    } catch (...) {
        return false;
    }
}
