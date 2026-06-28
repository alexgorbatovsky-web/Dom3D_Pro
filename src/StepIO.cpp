#include "StepIO.h"

#include "solid/SurfaceSet.h"
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
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Shape.hxx>

#include <memory>
#include <vector>
#include <iostream>
#include <iomanip>

namespace {
bool shape_has_type(const TopoDS_Shape& shape, TopAbs_ShapeEnum type)
{
    TopExp_Explorer explorer(shape, type);
    return explorer.More();
}

void collect_top_level_shapes(const TopoDS_Shape& shape, std::vector<TopoDS_Shape>& shapes)
{
    if (shape.IsNull()) {
        return;
    }

    const TopAbs_ShapeEnum shape_type = shape.ShapeType();
    if (shape_type != TopAbs_COMPOUND && shape_type != TopAbs_COMPSOLID) {
        shapes.push_back(shape);
        return;
    }

    const size_t before = shapes.size();
    for (TopoDS_Iterator iterator(shape); iterator.More(); iterator.Next()) {
        collect_top_level_shapes(iterator.Value(), shapes);
    }
    if (shapes.size() == before) {
        shapes.push_back(shape);
    }
}
/*
std::unique_ptr<CSolid> make_imported_solid(const TopoDS_Shape& source_shape, int index, int count)
{
    TopoDS_Shape shape = source_shape;
    std::unique_ptr<CSolid> loaded;
    const bool has_solid = shape_has_type(shape, TopAbs_SOLID);
    if (has_solid) {
        loaded = std::make_unique<CSolid>(shape);
        loaded->SetName(count > 1 ? "Imported STEP " + std::to_string(index) : "Imported STEP");
        loaded->SetGroupName(count > 1 ? "Solids from STEP" : "");
    } else {
        loaded = std::make_unique<CSurfaceSet>(shape);
        loaded->SetName(count > 1 ? "Imported STEP Surface Set " + std::to_string(index) : "Imported STEP Surface Set");
        loaded->SetGroupName(count > 1 ? "Surfaces from STEP" : "");
    }
    loaded->SetColor({0.58f, 0.68f, 0.76f});
    loaded->InitSurfaces();
    loaded->InitEdges();
    loaded->BuldMesh(1.0f);
    return loaded;
}
*/

std::unique_ptr<CSolid> make_imported_solid(
    const TopoDS_Shape& source_shape,
    int index,
    int count)
{
    TopoDS_Shape shape = source_shape;

    std::unique_ptr<CSolid> loaded;

    const bool has_solid = shape_has_type(shape, TopAbs_SOLID);

    if (has_solid)
    {
        loaded = std::make_unique<CSolid>(shape);
        loaded->SetName(count > 1 ? "Imported STEP " + std::to_string(index) : "Imported STEP");
        loaded->SetGroupName(count > 1 ? "Solids from STEP" : "");
    }
    else
    {
        loaded = std::make_unique<CSurfaceSet>(shape);
        loaded->SetName(count > 1 ? "Imported STEP Surface Set " + std::to_string(index) : "Imported STEP Surface Set");
        loaded->SetGroupName(count > 1 ? "Surfaces from STEP" : "");
    }

    loaded->SetColor({ 0.58f, 0.68f, 0.76f });

  
    loaded->ReBuldMesh();

    return loaded;
}
}

/*
bool StepIO::Import(const std::string& path, std::vector<std::unique_ptr<CSolid>>& solids, std::string& error) const {
    solids.clear();
    STEPControl_Reader reader;
    char buffer[100];
    void Step(char* text);
    SYSTEMTIME st;
    // ѕолучаем текущее системное врем€ (местное)
    GetLocalTime(&st);
    sprintf(buffer, "STEP file read Started at %02d:%02d:%02d.%03d", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    Step(buffer);

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
        for (Standard_Integer i = 1; i <= reader.NbShapes(); ++i) {
            collect_top_level_shapes(reader.Shape(i), imported_shapes);
        }
        if (imported_shapes.empty()) {
            collect_top_level_shapes(shape, imported_shapes);
        }
        GetLocalTime(&st);
        sprintf(buffer, "STEP file read completed at %02d:%02d:%02d.%03d", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        Step(buffer);

        const int imported_count = static_cast<int>(imported_shapes.size());
        for (int i = 0; i < imported_count; ++i) {
            solids.push_back(make_imported_solid(imported_shapes[static_cast<size_t>(i)], i + 1, imported_count));
        }

        GetLocalTime(&st);
        sprintf(buffer, "Imported solid completed at %02d:%02d:%02d.%03d", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
        Step(buffer);
        return true;
    } catch (const Standard_Failure& failure) {
        error = failure.GetMessageString();
        if (error.empty()) {
            error = "OpenCascade failed while importing STEP.";
        }
        return false;
    }
}
*/

bool StepIO::Import(
    const std::string& path,
    std::vector<std::unique_ptr<CSolid>>& solids,
    std::string& error) const
{
    solids.clear();

    STEPControl_Reader reader;

    char buffer[100];
    void Step(char* text);
    SYSTEMTIME st;

    auto LogTime = [&](const char* text)
        {
            GetLocalTime(&st);
            sprintf(buffer, "%s at %02d:%02d:%02d.%03d",
                text, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
            Step(buffer);
        };

    try
    {
        LogTime("STEP ReadFile started");

        const IFSelect_ReturnStatus status = reader.ReadFile(path.c_str());
        if (status != IFSelect_RetDone)
        {
            error = "Could not read STEP file.";
            return false;
        }

        LogTime("STEP ReadFile completed");

        LogTime("STEP TransferRoot started");

        const Standard_Integer roots = reader.NbRootsForTransfer();

        for (Standard_Integer n = 1; n <= roots; ++n)
        {
            reader.TransferRoot(n);
        }

        LogTime("STEP TransferRoot completed");

        const Standard_Integer nbs = reader.NbShapes();

        if (nbs <= 0)
        {
            error = "STEP file does not contain transferable shapes.";
            return false;
        }

        sprintf(buffer, "STEP NbShapes = %d", (int)nbs);
        Step(buffer);

        LogTime("Make solids started");

        solids.reserve((size_t)nbs);

        for (Standard_Integer i = 1; i <= nbs; ++i)
        {
            TopoDS_Shape sh = reader.Shape(i);

            if (sh.IsNull())
                continue;

            solids.push_back(make_imported_solid(sh, (int)i, (int)nbs));
        }

        LogTime("Make solids completed");

        return true;
    }
    catch (const Standard_Failure& failure)
    {
        error = failure.GetMessageString();

        if (error.empty())
            error = "OpenCascade failed while importing STEP.";

        return false;
    }
}
bool StepIO::Export(const std::string& path, const CAlfaDoc& document, std::string& error) const {
    TopoDS_Shape export_shape;

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);

    int solid_count = 0;
    for (const auto& object : document.GetObjects()) {
        const auto* solid = dynamic_cast<const CSolid*>(object.get());
        if (solid && solid->IsVisible() && !solid->m_Shape.IsNull()) {
            builder.Add(compound, solid->m_Shape);
            ++solid_count;
        }
    }

    if (solid_count == 0) {
        error = "There are no visible solid objects to export.";
        return false;
    }
    export_shape = compound;

    if (export_shape.IsNull()) {
        error = "Visible solids have no STEP shape.";
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
