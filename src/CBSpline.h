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
#include <string>
#include <vector>

enum class SplineCurveType {
    BSpline,
    Bezier,
    Nurbs
};

class CBSpline : public CAlfaObject {
public:
    CBSpline();
    explicit CBSpline(std::string name);

    const std::vector<CPoint3d>& GetPoints() const;
    std::vector<CPoint3d>& GetPoints();
    bool IsEmpty() const;
    bool IsClosed() const;
    bool CanClose() const;
    void SetClosed(bool closed);
    bool Close();
    void Open();
    size_t GetPointCount() const;
    void Clear();
    void AddPoint(CPoint3d point);
    void SetBezierInterpolationPoints(const std::vector<CPoint3d>& points);
    void SetClosedBezierInterpolationPoints(const std::vector<CPoint3d>& points);
    bool IsBezierChain() const;
    bool InsertPoint(size_t index, CPoint3d point, double weight = 1.0);
    bool InsertShapePreservingPoint(double parameter);
    bool SetPoint(size_t index, CPoint3d point);
    bool SetPointDirect(size_t index, CPoint3d point);
    bool RemovePoint(size_t index);
    void Reverse();
    bool ExtendEndpoint(bool at_start, double distance);
    CPoint3d Evaluate(float t) const;
    SplineCurveType GetCurveType() const;
    void SetCurveType(SplineCurveType type);
    int GetDegree() const;
    void SetDegree(int degree);
    const std::vector<double>& GetWeights() const;
    void SetWeights(std::vector<double> weights);
    void SetWeight(size_t index, double weight);
    const std::vector<double>& GetKnots() const;
    bool SetKnots(std::vector<double> knots);

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

private:
    CPoint3d EvaluateOpen(const std::vector<CPoint3d>& points, float t) const;
    CPoint3d EvaluateClosed(float t) const;
    CPoint3d EvaluateBezier(float t) const;
    CPoint3d EvaluateNurbs(float t) const;
    float DistanceToSegment(CurvePoint point, const CPoint3d& start, const CPoint3d& end) const;
    void DrawPointBox(const CPoint3d& point, bool selected,
                      bool point_selected, bool bezier_control = false) const;

    std::vector<CPoint3d> points_;
    std::vector<double> weights_;
    // Expanded knot vector, including repeated knots. Empty means the
    // standard clamped uniform vector generated from point count and degree.
    std::vector<double> knots_;
    SplineCurveType curve_type_ = SplineCurveType::BSpline;
    int degree_ = 3;
    bool closed_ = false;
};
