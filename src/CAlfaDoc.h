#pragma once

#include "CAlfaObject.h"
#include "CBSpline.h"
#include "CMesh3D.h"
#include "CPolyline.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>

class CSolid;
class CAlfaDoc;
class CSmartLine;

CAlfaDoc* GetAlfaDoc();
void SetAlfaDoc(CAlfaDoc* document);

class CLayer {
public:
    CLayer(int id, std::string name);

    int ID() const;

    std::string Name;
    bool Visible = true;
    bool Selectable = true;

private:
    int m_ID = 0;
};

class CAlfaDoc {
public:
    using ObjectPtr = std::unique_ptr<CAlfaObject>;
    using ObjectList = std::vector<ObjectPtr>;

    struct Snapshot;
    struct LiveFilletBuildRequest {
        // Identity of the live source shape. The background builder must never
        // read or modify this shape; it is used only when applying the result.
        TopoDS_Shape source_shape;
        // Private deep copy owned by the build request.
        TopoDS_Shape base_shape;
        std::vector<TopoDS_Edge> edges;
        size_t object_index = 0;
    };

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
    bool CreateSketchPolyline(const std::vector<CPoint3d>& points,
                              bool closed,
                              const std::string& sketch_name,
                              CPoint3d origin,
                              CPoint3d x_axis,
                              CPoint3d y_axis);
    bool CreateSketchBezier(const std::vector<CPoint3d>& control_points,
                            const std::string& sketch_name,
                            CPoint3d origin,
                            CPoint3d x_axis,
                            CPoint3d y_axis);
    bool CreateSweptSolid(unsigned long section_id,
                          unsigned long guide_id,
                          int transition_mode = 1,
                          double delta_x = 0.0,
                          double delta_y = 0.0,
                          double angle_degrees = 0.0);
    bool CreateFrameSolid(unsigned long profile_id,
                          double width,
                          double height);
    bool CreateWireSolid(unsigned long path_id, double radius);
    bool CreatePolyhedronSolid(unsigned long profile_id,
                               int axis_index,
                               int turns);
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
    bool FindSolidEdgeAtScreen(
        DomPoint point,
        const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
        float tolerance,
        size_t& object_index,
        int& surface_index,
        int& edge_index,
        float* screen_distance = nullptr) const;
    bool FindRotationAxisLineAtScreen(
        DomPoint point,
        const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
        float tolerance,
        Vec3& start,
        Vec3& end,
        float* screen_distance = nullptr) const;
    bool SelectSolidMeshAtScreen(DomPoint point,
                                 const std::function<bool(Vec3, DomPoint&, float&)>& project_world,
                                 SelectionAction action = SelectionAction::Replace);
    CSolid* FindSolidAtScreen(
        DomPoint point,
        const std::function<bool(Vec3, DomPoint&, float&)>& project_world);
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
    bool SelectEdgesOfSelectedFaces();
    CSolid* GetSelectedFaceSolid();
    const CSolid* GetSelectedFaceSolid() const;
    bool GetSelectedSolidFaceCenterAndNormal(Vec3& center, Vec3& normal) const;
    bool GetSelectedSolidFaceSketchPlane(Vec3& origin,
                                         Vec3& x_axis,
                                         Vec3& y_axis,
                                         Vec3& normal,
                                         unsigned long& body_id,
                                         int& face_index) const;
    bool GetSolidFaceSketchPlane(unsigned long body_id,
                                 int face_index,
                                 Vec3& origin,
                                 Vec3& x_axis,
                                 Vec3& y_axis,
                                 Vec3& normal) const;
    bool UpdateAttachedSketches();
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
    bool SelectAllPointsOfSelectedCurve();
    bool SelectObjectsInScreenRect(DomRect rect,
                                   const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                   SelectionAction action = SelectionAction::Replace);
    bool SelectSolidFacesInScreenRect(DomRect rect,
                                      const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                      SelectionAction action = SelectionAction::Replace);
    bool SelectSolidEdgesInScreenRect(DomRect rect,
                                      const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                      SelectionAction action = SelectionAction::Replace);
    bool FindPolylinePointAtScreen(DomPoint point,
                                   const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                   float tolerance,
                                   size_t& object_index,
                                   size_t& point_index) const;
    void ClearSelection();
    bool SelectObjectById(unsigned long object_id,
                          SelectionAction action = SelectionAction::Replace);
    void ClearPointSelection();
    bool HasSelection() const;
    void SetGroupInteractionEnabled(bool enabled) {
        group_interaction_enabled_ = enabled;
    }
    bool IsGroupInteractionEnabled() const {
        return group_interaction_enabled_;
    }
    bool ExpandSelectedGroups();
    bool HasSelectedPoint() const;
    bool HasSelectedSolidEdge() const;
    size_t SelectAllVisibleObjects();
    size_t GetSelectedObjectIndex() const;
    size_t GetSelectedPointIndex() const;
    bool IsObjectSelected(size_t index) const;
    bool IsObjectSelectionHighlighted(size_t index) const;
    void SetObjectSelectionHighlightHidden(size_t index, bool hidden);
    size_t GetSelectedObjectCount() const;
    const std::vector<size_t>& GetSelectedObjectIndices() const;
    CAlfaObject* GetSelectedObject();
    const CAlfaObject* GetSelectedObject() const;
    CMesh3D* GetSelectedMesh();
    const CMesh3D* GetSelectedMesh() const;
    CPolyline* GetSelectedPolyline();
    const CPolyline* GetSelectedPolyline() const;
    CSmartLine* GetSelectedSketch();
    const CSmartLine* GetSelectedSketch() const;
    CBSpline* GetSelectedBSpline();
    const CBSpline* GetSelectedBSpline() const;
    CSolid* GetSelectedSolid();
    const CSolid* GetSelectedSolid() const;
    CAlfaObject* FindObjectById(unsigned long id);
    const CAlfaObject* FindObjectById(unsigned long id) const;
    size_t FindObjectIndexById(unsigned long id) const;
    void EnsureObjectId(CAlfaObject& object);
    void EnsureObjectIds();
    void AddObject(std::unique_ptr<CAlfaObject> object);
    void AddMesh(std::unique_ptr<CMesh3D> mesh);
    bool CreateGroupFromSelection();
    bool CreateAssemblyFromSelection();
    bool UngroupSelection();
    bool DuplicateSelectedObject();
    bool CreateAssociativeCloneFromSelection();
    bool RebuildAssociativeClones(unsigned long source_id = 0);
    bool CreateSolidFromTwoSelectedSketches();
    bool RebuildTwoSketchSolid(size_t object_index);
    bool MirrorSelectedObjects(Vec3 plane_point, Vec3 plane_normal);
    bool CreateLoftSurfaceFromSelectedBSplines();
    bool JoinSelectedSurfaces();
    size_t CreatePlaneIntersectionCurves(
        std::string* error_message = nullptr);
    size_t CreatePlaneIntersectionCurves(unsigned long target_id,
                                         Vec3 plane_origin,
                                         Vec3 plane_normal,
                                         std::string* error_message = nullptr);
    bool GetObjectPlane(unsigned long object_id,
                        Vec3& origin,
                        Vec3& normal,
                        std::string* error_message = nullptr) const;
    size_t CreateSurfaceIntersectionCurves();
    size_t ProjectSelectedCurveToSurface(Vec3 direction);
    size_t ExtractSelectedSurfaceEdges();
    bool CreateRuledSurfaceFromSpline(unsigned long curve_id,
                                      double length,
                                      int direction_axis,
                                      bool reverse_normal);
    bool RebuildRuledSurface(size_t object_index,
                             unsigned long curve_id,
                             double length,
                             int direction_axis,
                             bool reverse_normal);
    bool CreateShellFromSurface(unsigned long surface_id,
                                int face_index,
                                double distance,
                                std::string* error_message = nullptr);
    bool RebuildShellFromSurface(size_t object_index,
                                 unsigned long surface_id,
                                 int face_index,
                                 double distance,
                                 std::string* error_message = nullptr);
    bool CreateFourSplineSurfaceFromSelection();
    bool RebuildFourSplineSurface(size_t object_index,
                                  unsigned long first_id,
                                  unsigned long second_id,
                                  unsigned long third_id,
                                  unsigned long fourth_id);
    bool CreateTwoRailSweepSurfaceFromSelection();
    bool RebuildTwoRailSweepSurface(size_t object_index,
                                    unsigned long profile_id,
                                    unsigned long first_rail_id,
                                    unsigned long second_rail_id);
    bool CreateTwoRailSweepSolidFromSelection();
    bool RebuildTwoRailSweepSolid(size_t object_index,
                                  unsigned long profile_id,
                                  unsigned long first_rail_id,
                                  unsigned long second_rail_id);
    bool ReverseSelectedSurfaceNormals();
    bool SewSelectedSurfacesToSolid(double tolerance,
                                    std::string* error_message = nullptr,
                                    double* used_tolerance = nullptr);
    int RebuildVisibleObjectMeshes(float mesh_deflection);
    bool DeleteSelectedObject();
    bool DeleteSelectedPoint();
    bool MoveSelectedPoint(CurvePoint point);
    bool MoveSelectedPoint(CPoint3d point);
    bool MoveSelectedCurvePoints(Vec3 delta, bool constrain_to_xy = false);
    bool RotateSelectedCurvePoints(Vec3 center, Vec3 axis, float angle);
    bool ScaleSelectedCurvePoints(Vec3 center, Vec3 axis, float factor);
    bool ApplyFilletToSelectedPolylinePoint(double radius);
    bool ApplyFilletToPolylinePointAtScreen(DomPoint point,
                                            const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
                                            float tolerance,
                                            double radius);
    bool GetSelectedPointPosition(CPoint3d& point) const;
    std::vector<CPoint3d> GetSelectedCurvePointPositions() const;
    const std::vector<std::pair<size_t, size_t>>& GetSelectedCurvePoints() const;
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
    std::vector<unsigned long> GetSelectedTransformRootIds() const;
    bool ApplyBooleanToSolids(size_t body_index, size_t tool_index, BooleanOperation operation);
    bool ApplyFilletToSelectedEdge(double radius);
    bool ApplyFilletToAllSelectedSolidEdges(double radius);
    bool BeginLiveFilletSelectedEdges(bool all_edges);
    bool HasLiveFillet() const;
    std::vector<std::pair<int, int>> GetLiveFilletEdgeRefs() const;
    std::vector<int> GetLiveFilletCreatedSurfaceIndices() const;
    bool GetLiveFilletEndPoints(CPoint3d& start, CPoint3d& end) const;
    std::vector<CPoint3d> GetLiveFilletPoints(size_t count) const;
    bool UpdateLiveFillet(double radius);
    bool UpdateLiveFillet(double start_radius, double end_radius);
    bool UpdateLiveFillet(const std::vector<double>& radius_law);
    bool CreateLiveFilletBuildRequest(LiveFilletBuildRequest& request);
    static bool BuildLiveFilletShape(
        const LiveFilletBuildRequest& request,
        const std::vector<double>& radius_law,
        TopoDS_Shape& result_shape,
        std::vector<int>& created_surface_indices);
    bool ApplyLiveFilletShape(
        const LiveFilletBuildRequest& request,
        const TopoDS_Shape& result_shape,
        std::vector<int> created_surface_indices);
    void FinishLiveFillet();
    void CancelLiveFillet();
    bool BeginLiveChamferSelectedEdges();
    bool HasLiveChamfer() const;
    std::vector<std::pair<int, int>> GetLiveChamferEdgeRefs() const;
    std::vector<int> GetLiveChamferCreatedSurfaceIndices() const;
    bool UpdateLiveChamfer(double distance);
    void FinishLiveChamfer();
    void CancelLiveChamfer();
    bool GetSelectionBounds(Vec3& min_point, Vec3& max_point) const;
    bool GetSelectionCenter(Vec3& center) const;
    bool GetTransformGizmoCenter(Vec3& center) const;
    void SetTransformGizmoOrigin(Vec3 origin);
    void ClearTransformGizmoOrigin();
    bool PickTransformGizmoOriginAtScreen(
        DomPoint point,
        const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
        float tolerance,
        Vec3& origin) const;

    CPolyline& GetActivePolyline();
    const CPolyline& GetActivePolyline() const;
    CBSpline& GetActiveBSpline();
    const CBSpline& GetActiveBSpline() const;

    ObjectList& GetObjects();
    const ObjectList& GetObjects() const;
    std::vector<CLayer*> m_Layers;
    int Work_layer = 0;
    void EnsureDefaultLayer();
    CLayer* AddLayer(const std::string& name);
    CLayer* GetLayerByID(int layer_id);
    const CLayer* GetLayerByID(int layer_id) const;
    int GetWorkLayerID() const;
    bool SetWorkLayer(int layer_id);
    bool IsLayerVisible(int layer_id) const;
    bool IsLayerSelectable(int layer_id) const;
    bool IsObjectVisible(const CAlfaObject& object) const;
    bool IsObjectSelectable(const CAlfaObject& object) const;
    size_t ResolveGroupSelectionIndex(size_t object_index) const;
    void AssignObjectToWorkLayer(CAlfaObject& object) const;
    std::vector<Material>& GetMaterials();
    const std::vector<Material>& GetMaterials() const;
    void ResetDefaultMaterials();
    Material* FindMaterial(unsigned long id);
    const Material* FindMaterial(unsigned long id) const;
    Material* FindMaterial(const std::string& name, bool case_insensitive = true);
    const Material* FindMaterial(const std::string& name, bool case_insensitive = true) const;
    Material& UpsertMaterial(Material material);
    bool DeleteMaterial(unsigned long id);

    const std::string& GetDraftingData() const { return drafting_data_; }
    void SetDraftingData(std::string data) { drafting_data_ = std::move(data); }

    size_t GetTotalPointCount() const;
    std::shared_ptr<const Snapshot> CreateSnapshot() const;
    bool RestoreSnapshot(const Snapshot& snapshot);

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
    std::vector<size_t> GetSelectedTransformRootIndices() const;

    ObjectList objects_;
    std::string drafting_data_;
    std::vector<Material> materials_;
    unsigned long next_object_id_ = 1;
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
    bool group_interaction_enabled_ = true;
    bool has_transform_gizmo_origin_ = false;
    Vec3 transform_gizmo_origin_{};
    size_t hidden_selection_highlight_index_ = static_cast<size_t>(-1);
};
