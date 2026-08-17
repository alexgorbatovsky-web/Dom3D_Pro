#include "OffsetFaceShapeBuilder.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRep_Builder.hxx>
#include <BRepOffset_MakeOffset.hxx>
#include <BRepOffset_Mode.hxx>
#include <BRepTools_History.hxx>
#include <GeomAbs_JoinType.hxx>
#include <Standard_Failure.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS.hxx>
#include <TopTools_ListIteratorOfListOfShape.hxx>

#include <cmath>

bool BuildOffsetFaceShape(const TopoDS_Shape& body,
                          const TopoDS_Face& face,
                          double distance,
                          TopoDS_Shape& result)
{
    result.Nullify();
    if (body.IsNull() || face.IsNull()
        || !std::isfinite(distance) || std::fabs(distance) <= 1.0e-5) {
        return false;
    }

    TopoDS_Shape offset_source = body;
    int solid_count = 0;
    TopoDS_Solid owner_solid;
    for (TopExp_Explorer solid_explorer(body, TopAbs_SOLID);
         solid_explorer.More(); solid_explorer.Next()) {
        ++solid_count;
        const TopoDS_Solid candidate =
            TopoDS::Solid(solid_explorer.Current());
        if (owner_solid.IsNull()) {
            for (TopExp_Explorer face_explorer(candidate, TopAbs_FACE);
                 face_explorer.More(); face_explorer.Next()) {
                if (face_explorer.Current().IsSame(face)) {
                    owner_solid = candidate;
                    break;
                }
            }
        }
    }
    if (!owner_solid.IsNull()) {
        offset_source = owner_solid;
    }

    const auto build_offset = [distance](const TopoDS_Shape& source,
                                         const TopoDS_Face& source_face,
                                         TopoDS_Shape& built) {
        BRepOffset_MakeOffset offset;
        offset.Initialize(source,
                          0.0,
                          1.0e-5,
                          BRepOffset_Skin,
                          Standard_False,
                          Standard_False,
                          GeomAbs_Intersection,
                          Standard_False,
                          Standard_True);
        offset.SetOffsetOnFace(source_face, distance);
        offset.MakeOffsetShape();
        if (!offset.IsDone() || offset.Shape().IsNull()) {
            return false;
        }
        built = offset.Shape();
        return true;
    };

    try {
        if (!build_offset(offset_source, face, result)) {
            ShapeUpgrade_UnifySameDomain unify(
                offset_source, Standard_True, Standard_True, Standard_True);
            unify.Build();
            const TopoDS_Shape unified_body = unify.Shape();
            if (unified_body.IsNull()) {
                return false;
            }

            TopoDS_Face unified_face;
            const Handle(BRepTools_History)& history = unify.History();
            if (!history.IsNull()) {
                const TopTools_ListOfShape& modified = history->Modified(face);
                for (TopTools_ListIteratorOfListOfShape iterator(modified);
                     iterator.More(); iterator.Next()) {
                    if (iterator.Value().ShapeType() == TopAbs_FACE) {
                        unified_face = TopoDS::Face(iterator.Value());
                        break;
                    }
                }
            }
            if (unified_face.IsNull()) {
                for (TopExp_Explorer explorer(unified_body, TopAbs_FACE);
                     explorer.More(); explorer.Next()) {
                    const TopoDS_Face candidate =
                        TopoDS::Face(explorer.Current());
                    if (candidate.IsSame(face)) {
                        unified_face = candidate;
                        break;
                    }
                }
            }
            if (unified_face.IsNull()
                || !build_offset(unified_body, unified_face, result)) {
                return false;
            }
        }

        if (!owner_solid.IsNull() && solid_count > 1) {
            const TopoDS_Shape offset_owner = result;
            TopoDS_Compound compound;
            BRep_Builder builder;
            builder.MakeCompound(compound);
            for (TopExp_Explorer explorer(body, TopAbs_SOLID);
                 explorer.More(); explorer.Next()) {
                const TopoDS_Solid candidate =
                    TopoDS::Solid(explorer.Current());
                if (candidate.IsSame(owner_solid)) {
                    for (TopExp_Explorer result_solids(
                             offset_owner, TopAbs_SOLID);
                         result_solids.More(); result_solids.Next()) {
                        builder.Add(compound, result_solids.Current());
                    }
                } else {
                    builder.Add(compound, candidate);
                }
            }
            result = compound;
        }
    } catch (const Standard_Failure&) {
        result.Nullify();
        return false;
    }

    bool has_solid = false;
    for (TopExp_Explorer explorer(result, TopAbs_SOLID);
         explorer.More(); explorer.Next()) {
        has_solid = true;
        break;
    }
    return has_solid && BRepCheck_Analyzer(result).IsValid();
}
