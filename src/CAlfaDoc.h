#pragma once

#include "CAlfaObject.h"
#include "CMesh3D.h"
#include "CPolyline.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

class CSolid;

class CAlfaDoc {
public:
    using ObjectPtr = std::unique_ptr<CAlfaObject>;
    using ObjectList = std::vector<ObjectPtr>;

    CAlfaDoc();

    void Clear();
    void ClearActivePolyline();
    void CreatePolyline();
    void AddCurvePoint(CurvePoint point);
    bool CreateMeshFromSelectedPolyline(CVector3d dir, float dist);
    bool SelectObjectAt(CurvePoint point, float tolerance);
    bool AddObjectToSelectionAt(CurvePoint point, float tolerance);
    bool RemoveObjectFromSelectionAt(CurvePoint point, float tolerance);
    bool ToggleObjectSelectionAt(CurvePoint point, float tolerance);
    bool SelectSolidEdgeAtScreen(DomPoint point,
                                 const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                 float tolerance,
                                 SelectionAction action = SelectionAction::Replace);
    bool SelectPolylineAt(CurvePoint point, float tolerance);
    bool SelectPointAt(CurvePoint point, float tolerance);
    void ClearSelection();
    void ClearPointSelection();
    bool HasSelection() const;
    bool HasSelectedPoint() const;
    bool HasSelectedSolidEdge() const;
    size_t GetSelectedObjectIndex() const;
    size_t GetSelectedPointIndex() const;
    bool IsObjectSelected(size_t index) const;
    size_t GetSelectedObjectCount() const;
    const std::vector<size_t>& GetSelectedObjectIndices() const;
    CAlfaObject* GetSelectedObject();
    const CAlfaObject* GetSelectedObject() const;
    CMesh3D* GetSelectedMesh();
    const CMesh3D* GetSelectedMesh() const;
    CPolyline* GetSelectedPolyline();
    const CPolyline* GetSelectedPolyline() const;
    CSolid* GetSelectedSolid();
    const CSolid* GetSelectedSolid() const;
    void AddObject(std::unique_ptr<CAlfaObject> object);
    void AddMesh(std::unique_ptr<CMesh3D> mesh);
    bool DeleteSelectedObject();
    bool DeleteSelectedPoint();
    bool MoveSelectedPoint(CurvePoint point);
    bool MoveSelectedObjects(Vec3 delta);
    bool RotateSelectedObjects(Vec3 center, Vec3 axis, float angle);
    bool ScaleSelectedObjects(Vec3 center, Vec3 axis, float factor);
    bool ApplyBooleanToSolids(size_t body_index, size_t tool_index, BooleanOperation operation);
    bool ApplyFilletToSelectedEdge(double radius);
    bool ApplyFilletToAllSelectedSolidEdges(double radius);
    bool GetSelectionBounds(Vec3& min_point, Vec3& max_point) const;
    bool GetSelectionCenter(Vec3& center) const;

    CPolyline& GetActivePolyline();
    const CPolyline& GetActivePolyline() const;

    ObjectList& GetObjects();
    const ObjectList& GetObjects() const;

    size_t GetTotalPointCount() const;

private:
    void EnsureActivePolyline();

    ObjectList objects_;
    size_t active_object_index_ = 0;
    size_t selected_object_index_ = 0;
    size_t selected_point_index_ = 0;
    std::vector<size_t> selected_object_indices_;
    bool has_selected_object_ = false;
    bool has_selected_point_ = false;
};
