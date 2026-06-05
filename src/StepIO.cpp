#include "StepIO.h"

#include "solid/Solid.h"

#include <BRep_Builder.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_StepModelType.hxx>
#include <STEPControl_Writer.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shape.hxx>

#include <memory>
#include <vector>

namespace {
std::unique_ptr<CSolid> make_imported_solid(const TopoDS_Shape& source_shape, int index, int count)
{
    TopoDS_Shape shape = source_shape;
    auto loaded = std::make_unique<CSolid>(shape);
    loaded->SetName(count > 1 ? "Imported STEP " + std::to_string(index) : "Imported STEP");
    loaded->SetGroupName(count > 1 ? "Solids from STEP" : "");
    loaded->SetColor({0.58f, 0.68f, 0.76f});
    loaded->InitSurfaces();
    loaded->InitEdges();
    loaded->BuldMesh(0.1f);
    return loaded;
}
}

bool StepIO::Import(const std::string& path, std::vector<std::unique_ptr<CSolid>>& solids, std::string& error) const {
    solids.clear();
    STEPControl_Reader reader;
    const IFSelect_ReturnStatus status = reader.ReadFile(path.c_str());
    if (status != IFSelect_RetDone) {
        error = "Could not read STEP file.";
        return false;
    }

    try {
        const Standard_Integer transferred = reader.TransferRoots();
        if (transferred <= 0) {
            error = "STEP file does not contain transferable shapes.";
            return false;
        }

        TopoDS_Shape shape = reader.OneShape();
        if (shape.IsNull()) {
            error = "STEP file does not contain a valid shape.";
            return false;
        }

        std::vector<TopoDS_Shape> imported_shapes;
        for (TopExp_Explorer explorer(shape, TopAbs_SOLID); explorer.More(); explorer.Next()) {
            imported_shapes.push_back(explorer.Current());
        }
        if (imported_shapes.empty()) {
            imported_shapes.push_back(shape);
        }

        const int imported_count = static_cast<int>(imported_shapes.size());
        for (int i = 0; i < imported_count; ++i) {
            solids.push_back(make_imported_solid(imported_shapes[static_cast<size_t>(i)], i + 1, imported_count));
        }
        return true;
    } catch (const Standard_Failure& failure) {
        error = failure.GetMessageString();
        if (error.empty()) {
            error = "OpenCascade failed while importing STEP.";
        }
        return false;
    }
}

bool StepIO::Export(const std::string& path, const CAlfaDoc& document, std::string& error) const {
    TopoDS_Shape export_shape;

    if (const CSolid* selected_solid = document.GetSelectedSolid()) {
        export_shape = selected_solid->m_Shape;
    } else {
        BRep_Builder builder;
        TopoDS_Compound compound;
        builder.MakeCompound(compound);

        int solid_count = 0;
        for (const auto& object : document.GetObjects()) {
            const auto* solid = dynamic_cast<const CSolid*>(object.get());
            if (solid && !solid->m_Shape.IsNull()) {
                builder.Add(compound, solid->m_Shape);
                ++solid_count;
            }
        }

        if (solid_count == 0) {
            error = "There are no solid objects to export.";
            return false;
        }
        export_shape = compound;
    }

    if (export_shape.IsNull()) {
        error = "Selected solid has no STEP shape.";
        return false;
    }

    try {
        STEPControl_Writer writer;
        const IFSelect_ReturnStatus transfer_status = writer.Transfer(export_shape, STEPControl_AsIs);
        if (transfer_status != IFSelect_RetDone) {
            error = "Could not transfer shape to STEP writer.";
            return false;
        }

        const IFSelect_ReturnStatus write_status = writer.Write(path.c_str());
        if (write_status != IFSelect_RetDone) {
            error = "Could not save STEP file.";
            return false;
        }
        return true;
    } catch (const Standard_Failure& failure) {
        error = failure.GetMessageString();
        if (error.empty()) {
            error = "OpenCascade failed while exporting STEP.";
        }
        return false;
    }
}
