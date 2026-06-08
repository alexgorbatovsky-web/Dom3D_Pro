#pragma once

#include "CAlfaObject.h"
#include "CBSpline.h"
#include "CMesh3D.h"
#include "CPolyline.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

class CSolid;

class CAlfaDoc {
public:
    using ObjectPtr = std::unique_ptr<CAlfaObject>;
    using ObjectList = std::vector<ObjectPtr>;

    CAlfaDoc();
    ~CAlfaDoc();

    void Clear();
    void ClearActivePolyline();
    void CreatePolyline();
    void CreateBSpline();
    void AddCurvePoint(CurvePoint point);
    void AddCurvePoint(CPoint3d point);
    void AddBSplinePoint(CPoint3d point);
    void CreateSketchRectangle(const std::vector<CPoint3d>& points, const std::string& sketch_name);
    bool CloseSelectedOrActivePolyline();
    bool CloseSelectedOrActiveBSpline();
    bool CreateMeshFromSelectedPolyline(CVector3d dir, float dist);
    bool BeginLiveExtrudeSelectedPolyline(double distance, bool reverse, double taper_angle_degrees);
    bool HasLivePolylineExtrude() const;
    bool UpdateLiveExtrudeSelectedPolyline(double distance, bool reverse, double taper_angle_degrees);
    bool FinishLiveExtrudeSelectedPolyline();
    void CancelLiveExtrudeSelectedPolyline();
    bool BeginLiveRevolveSelectedPolyline(double angle_degrees, int axis_index);
    bool HasLivePolylineRevolve() const;
    bool UpdateLiveRevolveSelectedPolyline(double angle_degrees, int axis_index);
    bool FinishLiveRevolveSelectedPolyline();
    void CancelLiveRevolveSelectedPolyline();
    bool SelectObjectAt(CurvePoint point, float tolerance, bool include_mesh = true);
    bool AddObjectToSelectionAt(CurvePoint point, float tolerance, bool include_mesh = true);
    bool RemoveObjectFromSelectionAt(CurvePoint point, float tolerance, bool include_mesh = true);
    bool ToggleObjectSelectionAt(CurvePoint point, float tolerance, bool include_mesh = true);
    bool SelectSolidEdgeAtScreen(DomPoint point,
                                 const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                 float tolerance,
                                 SelectionAction action = SelectionAction::Replace);
    bool SelectSolidMeshAtScreen(DomPoint point,
                                 const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                                 SelectionAction action = SelectionAction::Replace);
    bool SelectMeshAtScreen(DomPoint point,
                            const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                            SelectionAction action = SelectionAction::Replace);
    bool SelectSolidPlanarFaceAtScreen(DomPoint point,
                                       const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                                       SelectionAction action = SelectionAction::Replace);
    bool SelectSolidFaceAtScreen(DomPoint point,
                                 const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                                 bool planar_only = false,
                                 SelectionAction action = SelectionAction::Replace);
    bool HasSelectedSolidFace() const;
    bool GetSelectedSolidFaceCenterAndNormal(Vec3& center, Vec3& normal) const;
    bool PreviewExtrudeSelectedSolidFace(Vec3 delta);
    bool BeginLiveExtrudeSelectedSolidFace(double taper_angle_degrees = 0.0);
    bool IsLiveExtrudeSelectedSolidFaceActive() const;
    bool UpdateLiveExtrudeSelectedSolidFace(float distance);
    void FinishLiveExtrudeSelectedSolidFace();
    void CancelLiveExtrudeSelectedSolidFace();
    bool ApplyExtrudeSelectedSolidFace(float distance);
    bool BeginDraftFaceFromSelectedFace();
    bool HasDraftFace() const;
    bool HasDraftFaceAxis() const;
    bool GetDraftFaceAxis(Vec3& center, Vec3& axis) const;
    bool SelectDraftFaceAxisEdgeAtScreen(DomPoint point,
                                         const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                         float tolerance);
    bool BeginLiveDraftFace();
    bool IsLiveDraftFaceActive() const;
    bool UpdateLiveDraftFace(double angle_degrees);
    void FinishLiveDraftFace();
    void CancelLiveDraftFace();
    bool BeginLiveThickSolidFromSelectedSolid(double thickness);
    bool BeginLiveThickSolidFromSelectedFaces(double thickness);
    bool HasLiveThickSolid() const;
    bool SelectLiveThickSolidFaceAtScreen(DomPoint point,
                                          const std::function<bool(Vec3, DomPoint&, float&)>& project_world);
    bool UpdateLiveThickSolid(double thickness);
    bool FinishLiveThickSolid();
    void CancelLiveThickSolid();
    size_t GetLiveThickSolidFaceCount() const;
    bool SelectPolylineAt(CurvePoint point, float tolerance);
    bool SelectPolylineAtScreen(DomPoint point,
                                const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                float tolerance,
                                SelectionAction action = SelectionAction::Replace);
    bool SelectPointAt(CurvePoint point, float tolerance);
    bool SelectPolylinePointAtScreen(DomPoint point,
                                     const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                     float tolerance);
    bool SelectCurvePointAtScreen(DomPoint point,
                                  const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                  float tolerance,
                                  SelectionAction action = SelectionAction::Replace);
    bool PickSelectedCurvePointAtScreen(DomPoint point,
                                        const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                        float tolerance,
                                        CPoint3d& selected_point) const;
    bool SelectCurvePointsInScreenRect(DomRect rect,
                                       const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                       SelectionAction action = SelectionAction::Replace);
    bool FindPolylinePointAtScreen(DomPoint point,
                                   const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                   float tolerance,
                                   size_t& object_index,
                                   size_t& point_index) const;
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
    CBSpline* GetSelectedBSpline();
    const CBSpline* GetSelectedBSpline() const;
    CSolid* GetSelectedSolid();
    const CSolid* GetSelectedSolid() const;
    void AddObject(std::unique_ptr<CAlfaObject> object);
    void AddMesh(std::unique_ptr<CMesh3D> mesh);
    bool DuplicateSelectedObject();
    bool CreateLoftSurfaceFromSelectedBSplines();
    bool ReverseSelectedSurfaceNormals();
    bool DeleteSelectedObject();
    bool DeleteSelectedPoint();
    bool MoveSelectedPoint(CurvePoint point);
    bool MoveSelectedPoint(CPoint3d point);
    bool MoveSelectedCurvePoints(Vec3 delta);
    bool ApplyFilletToSelectedPolylinePoint(double radius);
    bool ApplyFilletToPolylinePointAtScreen(DomPoint point,
                                            const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                            float tolerance,
                                            double radius);
    bool GetSelectedPointPosition(CPoint3d& point) const;
    std::vector<CPoint3d> GetSelectedCurvePointPositions() const;
    bool MoveSelectedObjects(Vec3 delta);
    bool RotateSelectedObjects(Vec3 center, Vec3 axis, float angle);
    bool ScaleSelectedObjects(Vec3 center, Vec3 axis, float factor);
    bool UniformScaleSelectedObjects(Vec3 center, float factor);
    bool PreviewMoveSelectedObjects(Vec3 delta);
    bool PreviewRotateSelectedObjects(Vec3 center, Vec3 axis, float angle);
    bool PreviewScaleSelectedObjects(Vec3 center, Vec3 axis, float factor);
    bool PreviewUniformScaleSelectedObjects(Vec3 center, float factor);
    bool CommitMoveSelectedSolids(Vec3 delta);
    bool CommitRotateSelectedSolids(Vec3 center, Vec3 axis, float angle);
    bool CommitScaleSelectedSolids(Vec3 center, Vec3 axis, float factor);
    bool CommitUniformScaleSelectedSolids(Vec3 center, float factor);
    bool ApplyBooleanToSolids(size_t body_index, size_t tool_index, BooleanOperation operation);
    bool ApplyFilletToSelectedEdge(double radius);
    bool ApplyFilletToAllSelectedSolidEdges(double radius);
    bool BeginLiveFilletSelectedEdges(bool all_edges);
    bool HasLiveFillet() const;
    bool UpdateLiveFillet(double radius);
    void FinishLiveFillet();
    void CancelLiveFillet();
    bool BeginLiveChamferSelectedEdges();
    bool HasLiveChamfer() const;
    bool UpdateLiveChamfer(double distance);
    void FinishLiveChamfer();
    void CancelLiveChamfer();
    bool GetSelectionBounds(Vec3& min_point, Vec3& max_point) const;
    bool GetSelectionCenter(Vec3& center) const;

    CPolyline& GetActivePolyline();
    const CPolyline& GetActivePolyline() const;
    CBSpline& GetActiveBSpline();
    const CBSpline& GetActiveBSpline() const;

    ObjectList& GetObjects();
    const ObjectList& GetObjects() const;
    std::vector<Material>& GetMaterials();
    const std::vector<Material>& GetMaterials() const;
    void ResetDefaultMaterials();
    Material* FindMaterial(unsigned long id);
    const Material* FindMaterial(unsigned long id) const;
    Material& UpsertMaterial(Material material);
    bool DeleteMaterial(unsigned long id);

    size_t GetTotalPointCount() const;

private:
    struct LiveExtrudeData;
    struct LivePolylineExtrudeData;
    struct LivePolylineRevolveData;
    struct LiveFilletData;
    struct LiveChamferData;
    struct DraftFaceData;
    struct LiveThickSolidData;

    void EnsureActivePolyline();
    void EnsureActiveBSpline();
    void AssignDefaultMaterial(CAlfaObject& object);

    ObjectList objects_;
    std::vector<Material> materials_;
    size_t active_object_index_ = 0;
    size_t selected_object_index_ = 0;
    size_t selected_face_object_index_ = 0;
    size_t selected_point_index_ = 0;
    std::vector<size_t> selected_object_indices_;
    std::vector<std::pair<size_t, size_t>> selected_curve_points_;
    std::vector<int> selected_solid_face_indices_;
    std::unique_ptr<LiveExtrudeData> live_extrude_;
    std::unique_ptr<LivePolylineExtrudeData> live_polyline_extrude_;
    std::unique_ptr<LivePolylineRevolveData> live_polyline_revolve_;
    std::unique_ptr<LiveFilletData> live_fillet_;
    std::unique_ptr<LiveChamferData> live_chamfer_;
    std::unique_ptr<DraftFaceData> draft_face_;
    std::unique_ptr<LiveThickSolidData> live_thick_solid_;
    bool has_selected_solid_face_ = false;
    bool has_selected_object_ = false;
    bool has_selected_point_ = false;
};
