#pragma once

#include "CAlfaObject.h"
#include "Constraint.h"
#include "Fillet.h"
#include "LinkLine.h"

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <memory>
#include <vector>

class CPolyline;
class CBezierSpline;

struct SketchCoordinateSystem {
    CPoint3d origin{0.0, 0.0, 0.0};
    CPoint3d x_axis{1.0, 0.0, 0.0};
    CPoint3d y_axis{0.0, 0.0, 1.0};
    CPoint3d normal{0.0, -1.0, 0.0};
};

struct SketchFaceAttachment {
    unsigned long body_id = 0;
    int face_index = -1;
};

class CSmartLine final : public CAlfaObject {
public:
    CSmartLine();
    explicit CSmartLine(std::string name);
    CSmartLine(CSmartLine&&) noexcept = default;
    CSmartLine& operator=(CSmartLine&&) noexcept = default;
    ~CSmartLine() override;

    // Deep value copy intended for temporary/debug sketches. The returned
    // object has no document id and can safely be passed to AddObject after
    // moving it into a unique_ptr.
    CSmartLine MakeCopy() const;

    bool Create(const CPolyline& polyline);
    bool CreateFromWorldPoints(const std::vector<CPoint3d>& points,
                               bool closed,
                               CPoint3d origin,
                               CPoint3d x_axis,
                               CPoint3d y_axis);
    bool AddLine(std::unique_ptr<CLinkLine> line, bool connect_to_previous = true);
    bool AddBezierWorld(CPoint3d start,
                        CPoint3d control1,
                        CPoint3d control2,
                        CPoint3d end,
                        bool connect_to_previous = false);
    bool Add(CLinkLine* line, BOOL assign_id = TRUE);
    bool RemoveLine(std::size_t index);
    bool SplitLine(std::size_t index, CPoint3d local_point);
    bool SplitBezierLine(std::size_t index, double parameter);
    bool ConvertLineToBezier(std::size_t index);
    bool ConvertLineToArc(std::size_t index, CPoint3d world_point);

    std::size_t GetNumLines() const;
    CLinkLine* GetLine(std::size_t index);
    const CLinkLine* GetLine(std::size_t index) const;
    CLinkLine* Get_Link_by_index(int index, bool report_error = true);
    CLinkLine* GetLastLine();

    bool MovePoint(std::size_t line_index, int endpoint, CPoint3d local_point);
    std::size_t GetNodeCount() const;
    CPoint3d GetNodeWorld(std::size_t node_index) const;
    bool MoveNodeWorld(std::size_t node_index, CPoint3d world_point);
    std::size_t GetBezierControlPointCount() const;
    CPoint3d GetBezierControlPointWorld(std::size_t control_index) const;
    bool MoveBezierControlPointWorld(std::size_t control_index, CPoint3d world_point);
    std::size_t GetArcGripCount() const;
    CPoint3d GetArcGripWorld(std::size_t grip_index) const;
    bool MoveArcGripWorld(std::size_t grip_index, CPoint3d world_point);
    void ConnectAdjacentLines(std::size_t changed_line_index);
    bool IsClosed() const;
    bool SetClosed(bool closed);

    bool AddConstraint(std::unique_ptr<CConstraint> constraint);
    bool ConstrainHorizontal(std::size_t line_index);
    bool ConstrainVertical(std::size_t line_index);
    bool ConstrainBezierTangentAtStart(std::size_t line_index);
    bool ConstrainBezierTangentAtEnd(std::size_t line_index);
    bool ApplyConstraints();
    std::size_t GetNumConstraints() const;
    const CConstraint* GetConstraint(std::size_t index) const;

    bool AddFillet(std::size_t first_line_index, double radius);
    bool RemoveFillet(std::size_t first_line_index);
    std::size_t GetNumFillets() const;
    CFillet* GetFillet(std::size_t index);
    const CFillet* GetFillet(std::size_t index) const;
    CPoint3d GetFilletGripWorld(std::size_t fillet_index) const;
    bool SetFilletRadiusFromWorld(std::size_t fillet_index, CPoint3d world_point);

    const SketchCoordinateSystem& GetCoordinateSystem() const;
    bool SetCoordinateSystem(CPoint3d origin, CPoint3d x_axis, CPoint3d normal);
    void SetFaceAttachment(unsigned long body_id, int face_index);
    void ClearFaceAttachment();
    bool HasFaceAttachment() const;
    const SketchFaceAttachment& GetFaceAttachment() const;
    CPoint3d LocalToWorld(const CPoint3d& point) const;
    CPoint3d WorldToLocal(const CPoint3d& point) const;
    std::vector<CPoint3d> GetProfilePointsWorld() const;

    void Render3d(bool selected) const override;
    void Render2d(float center_x, float center_y, float scale) const override;
    bool HitTest(CurvePoint point, float tolerance) const override;
    bool HitTestScreen(
        DomPoint point,
        const std::function<bool(Vec3, DomPoint&)>& world_to_screen,
        float tolerance) const;
    bool Save(std::ostream& stream) const override;
    std::unique_ptr<CAlfaObject> Clone() const override;
    void Translate(Vec3 delta) override;
    void Rotate(Vec3 center, Vec3 axis, float angle) override;
    void Scale(Vec3 center, Vec3 axis, float factor) override;
    void ScaleLocal(double x_factor, double y_factor);
    bool GetBounds(Vec3& min_point, Vec3& max_point) const override;

private:
    struct DisplayGeometry {
        std::vector<CPoint3d> line_starts;
        std::vector<CPoint3d> line_ends;
        std::vector<std::vector<CPoint3d>> arcs;
        std::vector<std::vector<CPoint3d>> curves;
    };

    DisplayGeometry BuildDisplayGeometry() const;
    bool ReplaceLineWithSplitParts(
        std::size_t index,
        std::unique_ptr<CLinkLine> first,
        std::unique_ptr<CLinkLine> second);
    void RenumberLines();
    void RemoveInvalidConstraintsAndFillets();

    SketchCoordinateSystem coordinate_system_;
    SketchFaceAttachment face_attachment_;
    std::vector<std::unique_ptr<CLinkLine>> lines_;
    std::vector<std::unique_ptr<CConstraint>> constraints_;
    std::vector<CFillet> fillets_;
    bool closed_ = false;
};
