#include "CAlfaDoc.h"

#include "solid/Solid.h"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <Standard_Failure.hxx>
#include <TopTools_MapOfShape.hxx>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {
std::vector<TopoDS_Edge> unique_edges(const std::vector<TopoDS_Edge>& edges)
{
    std::vector<TopoDS_Edge> unique;
    TopTools_MapOfShape seen;
    for (const TopoDS_Edge& edge : edges) {
        if (!edge.IsNull() && seen.Add(edge)) {
            unique.push_back(edge);
        }
    }
    return unique;
}

bool rebuild_solid_from_shape(CAlfaDoc::ObjectList& objects, size_t solid_index, CSolid* source_solid, const TopoDS_Shape& result_shape)
{
    if (solid_index >= objects.size() || !source_solid || result_shape.IsNull()) {
        return false;
    }

    TopoDS_Shape shape_copy = result_shape;
    auto result = std::make_unique<CSolid>(shape_copy);
    result->SetName("Fillet");
    result->SetColor(source_solid->GetColor());
    result->InitSurfaces();
    result->InitEdges();
    result->BuldMesh(0.1f);

    objects[solid_index] = std::move(result);
    return true;
}
}

CAlfaDoc::CAlfaDoc() {
    EnsureActivePolyline();
}

void CAlfaDoc::Clear() {
    objects_.clear();
    active_object_index_ = 0;
    ClearSelection();
    EnsureActivePolyline();
}

void CAlfaDoc::ClearActivePolyline() {
    GetActivePolyline().Clear();
}

void CAlfaDoc::CreatePolyline() {
    const size_t next_number = objects_.size() + 1;
    auto polyline = std::make_unique<CPolyline>("Curve " + std::to_string(next_number));
    polyline->SetColor({0.98f, 0.77f, 0.30f});
    objects_.push_back(std::move(polyline));
    active_object_index_ = objects_.size() - 1;
    ClearSelection();
}

void CAlfaDoc::AddCurvePoint(CurvePoint point) {
    GetActivePolyline().AddPoint(point);
}

bool CAlfaDoc::CreateMeshFromSelectedPolyline(CVector3d dir, float dist) {
    CPolyline* polyline = GetSelectedPolyline();
    if (!polyline) {
        return false;
    }

    auto mesh = std::make_unique<CMesh3D>();
    if (!mesh->Create(polyline, dir, dist)) {
        return false;
    }

    objects_.push_back(std::move(mesh));
    selected_object_index_ = objects_.size() - 1;
    selected_object_indices_ = {selected_object_index_};
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::SelectObjectAt(CurvePoint point, float tolerance) {
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        if (objects_[index]->IsVisible() && objects_[index]->HitTest(point, tolerance)) {
            for (ObjectPtr& object : objects_) {
                if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
                    solid->ClearSelectedEdge();
                }
            }
            selected_object_index_ = index;
            active_object_index_ = index;
            selected_object_indices_ = {index};
            has_selected_object_ = true;
            ClearPointSelection();
            return true;
        }
    }

    ClearSelection();
    return false;
}

bool CAlfaDoc::AddObjectToSelectionAt(CurvePoint point, float tolerance) {
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        if (objects_[index]->IsVisible() && objects_[index]->HitTest(point, tolerance)) {
            selected_object_index_ = index;
            active_object_index_ = index;
            has_selected_object_ = true;
            ClearPointSelection();
            if (!IsObjectSelected(index)) {
                selected_object_indices_.push_back(index);
            }
            return true;
        }
    }

    return false;
}

bool CAlfaDoc::RemoveObjectFromSelectionAt(CurvePoint point, float tolerance) {
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        if (!objects_[index]->IsVisible() || !objects_[index]->HitTest(point, tolerance)) {
            continue;
        }

        auto existing = std::find(selected_object_indices_.begin(), selected_object_indices_.end(), index);
        if (existing == selected_object_indices_.end()) {
            return false;
        }

        selected_object_indices_.erase(existing);
        ClearPointSelection();
        if (selected_object_indices_.empty()) {
            ClearSelection();
        } else {
            selected_object_index_ = selected_object_indices_.back();
            active_object_index_ = selected_object_index_;
            has_selected_object_ = true;
        }
        return true;
    }

    return false;
}

bool CAlfaDoc::ToggleObjectSelectionAt(CurvePoint point, float tolerance) {
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        if (!objects_[index]->IsVisible() || !objects_[index]->HitTest(point, tolerance)) {
            continue;
        }

        auto existing = std::find(selected_object_indices_.begin(), selected_object_indices_.end(), index);
        if (existing != selected_object_indices_.end()) {
            selected_object_indices_.erase(existing);
            ClearPointSelection();
            if (selected_object_indices_.empty()) {
                ClearSelection();
            } else {
                selected_object_index_ = selected_object_indices_.back();
                active_object_index_ = selected_object_index_;
                has_selected_object_ = true;
            }
            return true;
        }

        selected_object_indices_.push_back(index);
        selected_object_index_ = index;
        active_object_index_ = index;
        has_selected_object_ = true;
        ClearPointSelection();
        return true;
    }

    return false;
}

bool CAlfaDoc::SelectSolidEdgeAtScreen(DomPoint point,
                                       const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                       float tolerance,
                                       SelectionAction action) {
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        CSolid* solid = dynamic_cast<CSolid*>(objects_[index].get());
        if (!solid || !solid->IsVisible()) {
            continue;
        }

        int surface_index = -1;
        int edge_index = -1;
        if (solid->HitTestEdgeScreen(point, world_to_screen, tolerance, surface_index, edge_index)) {
            if (action == SelectionAction::Replace || selected_object_index_ != index) {
                for (ObjectPtr& object : objects_) {
                    if (auto* other_solid = dynamic_cast<CSolid*>(object.get())) {
                        other_solid->ClearSelectedEdge();
                    }
                }
                if (action == SelectionAction::Remove) {
                    return false;
                }
                solid->SetSelectedEdge(surface_index, edge_index);
            } else if (action == SelectionAction::Add) {
                solid->AddSelectedEdge(surface_index, edge_index);
            } else {
                solid->RemoveSelectedEdge(surface_index, edge_index);
            }

            selected_object_index_ = index;
            active_object_index_ = index;
            selected_object_indices_ = {index};
            has_selected_object_ = true;
            ClearPointSelection();
            return true;
        }
    }

    return false;
}

bool CAlfaDoc::SelectPolylineAt(CurvePoint point, float tolerance) {
    for (size_t i = objects_.size(); i > 0; --i) {
        const size_t index = i - 1;
        CPolyline* polyline = dynamic_cast<CPolyline*>(objects_[index].get());
        if (polyline && polyline->IsVisible() && polyline->HitTest(point, tolerance)) {
            selected_object_index_ = index;
            active_object_index_ = index;
            selected_object_indices_ = {index};
            has_selected_object_ = true;
            ClearPointSelection();
            return true;
        }
    }

    ClearSelection();
    return false;
}

bool CAlfaDoc::SelectPointAt(CurvePoint point, float tolerance) {
    if (!HasSelection()) {
        return false;
    }

    CPolyline* polyline = GetSelectedPolyline();
    if (!polyline) {
        ClearPointSelection();
        return false;
    }

    size_t point_index = 0;
    if (polyline->HitTestPoint(point, tolerance, point_index)) {
        selected_point_index_ = point_index;
        has_selected_point_ = true;
        return true;
    }

    ClearPointSelection();
    return false;
}

void CAlfaDoc::ClearSelection() {
    for (ObjectPtr& object : objects_) {
        if (auto* solid = dynamic_cast<CSolid*>(object.get())) {
            solid->ClearSelectedEdge();
        }
    }
    selected_object_index_ = 0;
    selected_object_indices_.clear();
    has_selected_object_ = false;
    ClearPointSelection();
}

void CAlfaDoc::ClearPointSelection() {
    selected_point_index_ = 0;
    has_selected_point_ = false;
}

bool CAlfaDoc::HasSelection() const {
    return has_selected_object_ && !selected_object_indices_.empty() && selected_object_index_ < objects_.size();
}

bool CAlfaDoc::HasSelectedPoint() const {
    const CPolyline* polyline = GetSelectedPolyline();
    return has_selected_point_ && polyline && selected_point_index_ < polyline->GetPointCount();
}

bool CAlfaDoc::HasSelectedSolidEdge() const {
    const CSolid* solid = GetSelectedSolid();
    return solid && solid->HasSelectedEdge();
}

size_t CAlfaDoc::GetSelectedObjectIndex() const {
    return selected_object_index_;
}

size_t CAlfaDoc::GetSelectedPointIndex() const {
    return selected_point_index_;
}

bool CAlfaDoc::IsObjectSelected(size_t index) const {
    return std::find(selected_object_indices_.begin(), selected_object_indices_.end(), index) != selected_object_indices_.end();
}

size_t CAlfaDoc::GetSelectedObjectCount() const {
    return selected_object_indices_.size();
}

const std::vector<size_t>& CAlfaDoc::GetSelectedObjectIndices() const {
    return selected_object_indices_;
}

CAlfaObject* CAlfaDoc::GetSelectedObject() {
    if (!HasSelection()) {
        return nullptr;
    }

    return objects_[selected_object_index_].get();
}

const CAlfaObject* CAlfaDoc::GetSelectedObject() const {
    if (!HasSelection()) {
        return nullptr;
    }

    return objects_[selected_object_index_].get();
}

CMesh3D* CAlfaDoc::GetSelectedMesh() {
    return dynamic_cast<CMesh3D*>(GetSelectedObject());
}

const CMesh3D* CAlfaDoc::GetSelectedMesh() const {
    return dynamic_cast<const CMesh3D*>(GetSelectedObject());
}

CPolyline* CAlfaDoc::GetSelectedPolyline() {
    return dynamic_cast<CPolyline*>(GetSelectedObject());
}

const CPolyline* CAlfaDoc::GetSelectedPolyline() const {
    return dynamic_cast<const CPolyline*>(GetSelectedObject());
}

CSolid* CAlfaDoc::GetSelectedSolid() {
    return dynamic_cast<CSolid*>(GetSelectedObject());
}

const CSolid* CAlfaDoc::GetSelectedSolid() const {
    return dynamic_cast<const CSolid*>(GetSelectedObject());
}

void CAlfaDoc::AddObject(std::unique_ptr<CAlfaObject> object) {
    if (!object) {
        return;
    }

    objects_.push_back(std::move(object));
    selected_object_index_ = objects_.size() - 1;
    selected_object_indices_ = {selected_object_index_};
    has_selected_object_ = true;
    ClearPointSelection();
}

void CAlfaDoc::AddMesh(std::unique_ptr<CMesh3D> mesh) {
    AddObject(std::move(mesh));
}

bool CAlfaDoc::DeleteSelectedObject() {
    if (!HasSelection()) {
        return false;
    }

    std::vector<size_t> indices = selected_object_indices_;
    std::sort(indices.begin(), indices.end(), std::greater<size_t>());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    for (size_t index : indices) {
        if (index < objects_.size()) {
            objects_.erase(objects_.begin() + static_cast<ObjectList::difference_type>(index));
        }
    }

    ClearSelection();
    active_object_index_ = 0;
    EnsureActivePolyline();
    return true;
}

bool CAlfaDoc::DeleteSelectedPoint() {
    if (!HasSelectedPoint()) {
        return false;
    }

    CPolyline* polyline = GetSelectedPolyline();
    if (!polyline || !polyline->RemovePoint(selected_point_index_)) {
        return false;
    }

    ClearPointSelection();
    return true;
}

bool CAlfaDoc::MoveSelectedPoint(CurvePoint point) {
    if (!HasSelectedPoint()) {
        return false;
    }

    CPolyline* polyline = GetSelectedPolyline();
    return polyline && polyline->SetPoint(selected_point_index_, point);
}

bool CAlfaDoc::MoveSelectedObjects(Vec3 delta) {
    if (!HasSelection()) {
        return false;
    }

    bool moved = false;
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size()) {
            objects_[index]->Translate(delta);
            moved = true;
        }
    }

    return moved;
}

bool CAlfaDoc::RotateSelectedObjects(Vec3 center, Vec3 axis, float angle) {
    if (!HasSelection()) {
        return false;
    }

    bool rotated = false;
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size()) {
            objects_[index]->Rotate(center, axis, angle);
            rotated = true;
        }
    }

    return rotated;
}

bool CAlfaDoc::ScaleSelectedObjects(Vec3 center, Vec3 axis, float factor) {
    if (!HasSelection() || factor <= 0.0001f) {
        return false;
    }

    bool scaled = false;
    for (size_t index : selected_object_indices_) {
        if (index < objects_.size()) {
            objects_[index]->Scale(center, axis, factor);
            scaled = true;
        }
    }

    return scaled;
}

bool CAlfaDoc::ApplyBooleanToSolids(size_t body_index, size_t tool_index, BooleanOperation operation) {
    if (body_index >= objects_.size() || tool_index >= objects_.size() || body_index == tool_index) {
        return false;
    }

    auto* body = dynamic_cast<CSolid*>(objects_[body_index].get());
    auto* tool = dynamic_cast<CSolid*>(objects_[tool_index].get());
    if (!body || !tool || body->m_Shape.IsNull() || tool->m_Shape.IsNull()) {
        return false;
    }

    TopoDS_Shape result_shape;
    if (operation == BooleanOperation::Union) {
        BRepAlgoAPI_Fuse algo(body->m_Shape, tool->m_Shape);
        algo.Build();
        if (!algo.IsDone()) {
            return false;
        }
        result_shape = algo.Shape();
    } else if (operation == BooleanOperation::Cut) {
        BRepAlgoAPI_Cut algo(body->m_Shape, tool->m_Shape);
        algo.Build();
        if (!algo.IsDone()) {
            return false;
        }
        result_shape = algo.Shape();
    } else {
        BRepAlgoAPI_Common algo(body->m_Shape, tool->m_Shape);
        algo.Build();
        if (!algo.IsDone()) {
            return false;
        }
        result_shape = algo.Shape();
    }

    if (result_shape.IsNull()) {
        return false;
    }

    auto result = std::make_unique<CSolid>(result_shape);
    result->SetName(operation == BooleanOperation::Union ? "Boolean Union" : operation == BooleanOperation::Cut ? "Boolean Cut" : "Boolean Common");
    result->SetColor(body->GetColor());
    result->InitSurfaces();
    result->InitEdges();
    result->BuldMesh(0.1f);

    std::vector<size_t> erase_indices = {body_index, tool_index};
    std::sort(erase_indices.begin(), erase_indices.end(), std::greater<size_t>());
    erase_indices.erase(std::unique(erase_indices.begin(), erase_indices.end()), erase_indices.end());
    for (size_t index : erase_indices) {
        objects_.erase(objects_.begin() + static_cast<ObjectList::difference_type>(index));
    }

    objects_.push_back(std::move(result));
    selected_object_index_ = objects_.size() - 1;
    selected_object_indices_ = {selected_object_index_};
    active_object_index_ = selected_object_index_;
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::ApplyFilletToSelectedEdge(double radius) {
    if (radius <= 0.0 || !HasSelection()) {
        return false;
    }

    const size_t solid_index = selected_object_index_;
    if (solid_index >= objects_.size()) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[solid_index].get());
    if (!solid || solid->m_Shape.IsNull() || !solid->HasSelectedEdge()) {
        return false;
    }

    const std::vector<TopoDS_Edge> selected_edges = unique_edges(solid->GetSelectedTopoEdges());
    if (selected_edges.empty()) {
        return false;
    }

    TopoDS_Shape result_shape;
    try {
        BRepFilletAPI_MakeFillet fillet(solid->m_Shape);
        for (const TopoDS_Edge& edge : selected_edges) {
            fillet.Add(radius, edge);
        }
        fillet.Build();
        if (!fillet.IsDone()) {
            return false;
        }
        result_shape = fillet.Shape();
    } catch (const Standard_Failure&) {
        return false;
    }

    if (result_shape.IsNull()) {
        return false;
    }

    if (!rebuild_solid_from_shape(objects_, solid_index, solid, result_shape)) {
        return false;
    }
    selected_object_index_ = solid_index;
    selected_object_indices_ = {solid_index};
    active_object_index_ = solid_index;
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::ApplyFilletToAllSelectedSolidEdges(double radius) {
    if (radius <= 0.0 || !HasSelection()) {
        return false;
    }

    const size_t solid_index = selected_object_index_;
    if (solid_index >= objects_.size()) {
        return false;
    }

    auto* solid = dynamic_cast<CSolid*>(objects_[solid_index].get());
    if (!solid || solid->m_Shape.IsNull()) {
        return false;
    }

    const std::vector<TopoDS_Edge> edges = unique_edges(solid->GetAllTopoEdges());
    if (edges.empty()) {
        return false;
    }

    TopoDS_Shape result_shape;
    try {
        BRepFilletAPI_MakeFillet fillet(solid->m_Shape);
        for (const TopoDS_Edge& edge : edges) {
            fillet.Add(radius, edge);
        }
        fillet.Build();
        if (!fillet.IsDone()) {
            return false;
        }
        result_shape = fillet.Shape();
    } catch (const Standard_Failure&) {
        return false;
    }

    if (!rebuild_solid_from_shape(objects_, solid_index, solid, result_shape)) {
        return false;
    }

    selected_object_index_ = solid_index;
    selected_object_indices_ = {solid_index};
    active_object_index_ = solid_index;
    has_selected_object_ = true;
    ClearPointSelection();
    return true;
}

bool CAlfaDoc::GetSelectionBounds(Vec3& min_point, Vec3& max_point) const {
    if (!HasSelection()) {
        return false;
    }

    bool has_bounds = false;
    for (size_t index : selected_object_indices_) {
        if (index >= objects_.size() || !objects_[index]->IsVisible()) {
            continue;
        }

        Vec3 object_min{};
        Vec3 object_max{};
        if (!objects_[index]->GetBounds(object_min, object_max)) {
            continue;
        }

        if (!has_bounds) {
            min_point = object_min;
            max_point = object_max;
            has_bounds = true;
        } else {
            min_point.x = std::min(min_point.x, object_min.x);
            min_point.y = std::min(min_point.y, object_min.y);
            min_point.z = std::min(min_point.z, object_min.z);
            max_point.x = std::max(max_point.x, object_max.x);
            max_point.y = std::max(max_point.y, object_max.y);
            max_point.z = std::max(max_point.z, object_max.z);
        }
    }

    return has_bounds;
}

bool CAlfaDoc::GetSelectionCenter(Vec3& center) const {
    Vec3 min_point{};
    Vec3 max_point{};
    if (!GetSelectionBounds(min_point, max_point)) {
        return false;
    }

    center = (min_point + max_point) * 0.5f;
    return true;
}

CPolyline& CAlfaDoc::GetActivePolyline() {
    EnsureActivePolyline();
    return *static_cast<CPolyline*>(objects_[active_object_index_].get());
}

const CPolyline& CAlfaDoc::GetActivePolyline() const {
    return *static_cast<const CPolyline*>(objects_[active_object_index_].get());
}

CAlfaDoc::ObjectList& CAlfaDoc::GetObjects() {
    return objects_;
}

const CAlfaDoc::ObjectList& CAlfaDoc::GetObjects() const {
    return objects_;
}

size_t CAlfaDoc::GetTotalPointCount() const {
    size_t count = 0;
    for (const ObjectPtr& object : objects_) {
        const auto* polyline = dynamic_cast<const CPolyline*>(object.get());
        if (polyline) {
            count += polyline->GetPointCount();
        }
    }
    return count;
}

void CAlfaDoc::EnsureActivePolyline() {
    if (objects_.empty()) {
        auto polyline = std::make_unique<CPolyline>("Curve 1");
        polyline->SetColor({0.98f, 0.77f, 0.30f});
        objects_.push_back(std::move(polyline));
        active_object_index_ = 0;
    }

    if (active_object_index_ >= objects_.size() || dynamic_cast<CPolyline*>(objects_[active_object_index_].get()) == nullptr) {
        for (size_t i = 0; i < objects_.size(); ++i) {
            if (dynamic_cast<CPolyline*>(objects_[i].get()) != nullptr) {
                active_object_index_ = i;
                break;
            }
        }
    }

    if (dynamic_cast<CPolyline*>(objects_[active_object_index_].get()) == nullptr) {
        auto polyline = std::make_unique<CPolyline>("Curve " + std::to_string(objects_.size() + 1));
        polyline->SetColor({0.98f, 0.77f, 0.30f});
        objects_.push_back(std::move(polyline));
        active_object_index_ = objects_.size() - 1;
    }

    if (selected_object_index_ >= objects_.size()) {
        ClearSelection();
    } else if (has_selected_object_ && selected_object_indices_.empty()) {
        selected_object_indices_ = {selected_object_index_};
    }
}
