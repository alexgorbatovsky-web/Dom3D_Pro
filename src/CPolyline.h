#pragma once

#include "CAlfaObject.h"
#include "Point3d.h"

#ifdef Coord
#undef Coord
#endif
#ifdef String
#undef String
#endif
#ifdef LPCTSTR
#undef LPCTSTR
#endif
#ifdef Pixel
#undef Pixel
#endif
#ifdef XtPointer
#undef XtPointer
#endif

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

class CSurfaceFace;

struct CPolylineFilletGeometry {
    CPoint3d tangent_previous;
    CPoint3d tangent_next;
    CPoint3d center;
    Vec3 normal{};
    double signed_angle = 0.0;
};

class CPolyline : public CAlfaObject {
public:
    CPolyline();
    explicit CPolyline(std::string name);

    const std::vector<CPoint3d>& GetPoints() const;
    std::vector<CPoint3d>& GetPoints();
    float TmpLen;
    CVector Dir1;
    bool IsReversed;

    bool IsEmpty() const;
    bool IsClosed() const;
    bool CanClose() const;
    void SetClosed(bool closed);
    void MakeClosed();

    bool Close();
    void Open();
    size_t GetPointCount() const;
    size_t np() const;

    void Clear();
    void AddPoint(CPoint3d point);
    void AddPoint(CurvePoint point);
    bool InsertPoint(size_t index, CPoint3d point);
    bool InsertPoint(size_t index, CurvePoint point);
    bool SetPoint(size_t index, CPoint3d point);
    bool SetPoint(size_t index, CurvePoint point);
    bool PutOnSurface(CSurfaceFace* surface);
    bool RestoreTo3DFromUVSurface(CSurfaceFace* surface);

    bool ExportToObj(const std::string& name) const;
    bool printToFile(const std::string& name) const;
    static bool LoadTextPolylines(std::istream& stream,
                                  std::vector<std::unique_ptr<CPolyline>>& polylines,
                                  std::string& error);
    bool RemovePoint(size_t index);
    void SetLockedPlane(Vec3 plane_point, Vec3 plane_normal);
    void ClearLockedPlane();
    bool GetLockedPlane(Vec3& plane_point, Vec3& plane_normal) const;
    bool ApplyFillet(size_t point_index, double radius);
    bool SetVertexRadius(size_t point_index, double radius);
    double GetVertexRadius(size_t point_index) const;
    const std::vector<double>& GetVertexRadii() const;
    bool GetFilletGeometry(size_t point_index, CPolylineFilletGeometry& geometry) const;
    std::vector<CPoint3d> GetRoundedPathPoints(double maximum_angle_step = 0.17453292519943295) const;
    bool HitTestPoint(CurvePoint point, float tolerance, size_t& point_index) const;
    void Render3d(bool selected) const override;
    void Render3d(bool selected, bool has_selected_point, size_t selected_point_index) const override;
    void Render2d(float center_x, float center_y, float scale) const override;
    bool HitTest(CurvePoint point, float tolerance) const override;
    std::unique_ptr<CAlfaObject> Clone() const override;
    void Translate(Vec3 delta) override;
    void Rotate(Vec3 center, Vec3 axis, float angle) override;
    void Scale(Vec3 center, Vec3 axis, float factor) override;
    bool GetBounds(Vec3& min_point, Vec3& max_point) const override;
    void Edit(NativeWindowHandle parent_window) override;
    bool Save(std::ostream& stream) const override;
    bool Load(std::istream& stream);
    bool CreatePolygone(float Length, int qty);
    CPoint3d* P(int);
    CPoint3d* PLast();
    void Revers();
    double GetLength();
    bool IsConcavePolygonOnXY();
    bool JoinG(CPolyline* line2);
    bool JoinLine(CPolyline* line2, double delta);
    static bool JoinMultuLines(std::vector<CPolyline*>* lines, std::vector<CPolyline*>* LinesJoin, double delta);
    void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
    void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
    void mod_coord_am(CSystemCoord* sc);
    void mod_coord_ma(CSystemCoord* sc);
    bool IsAllPointsZeroZ();
    bool GetPlaneSC(CSystemCoord* msc);

private:
    static CPoint3d ToPoint3d(CurvePoint point);
    static CurvePoint ToCurvePoint(const CPoint3d& point);
    float DistanceToSegment(CurvePoint point, const CPoint3d& start, const CPoint3d& end) const;
    void DrawPointBox(const CPoint3d& point, bool selected, bool point_selected) const;

    std::vector<CPoint3d> points_;
    std::vector<double> vertex_radii_;
    bool closed_ = false;
    bool has_locked_plane_ = false;
    Vec3 locked_plane_point_{};
    Vec3 locked_plane_normal_{0.0f, 1.0f, 0.0f};
};
