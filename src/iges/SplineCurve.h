#pragma once

#include <vector>
#include <string>

#include "../Point3d.h"
#include "../Plane.h"
#include "../SystemCoord.h"
#include "../Vector.h"

class CPolyline;

class CSplineCurve
{
public:
    CSplineCurve();
    explicit CSplineCurve(int qty);
    CSplineCurve(CPoint3d& p1, CPoint3d& p2);
    CSplineCurve(float rad, float delta);
    ~CSplineCurve();

    CSplineCurve(const CSplineCurve&) = delete;
    CSplineCurve& operator=(const CSplineCurve&) = delete;

    bool Copy(CSplineCurve* src);
    void Alloc();
    void Clear();

    CPoint7d* P(int num_p);
    const CPoint7d* P(int num_p) const;
    CPoint7d* PLast();
    const CPoint7d* PLast() const;
    CPoint3d* Pnt(int num_p);
    const CPoint3d* Pnt(int num_p) const;
    CPoint3d* PntLast();
    const CPoint3d* PntLast() const;

    int np() const { return m_n; }
    void SetQtyPnts(int n) { m_n = n; }

    bool Build();
    bool Update();
    int Realloc(int num_p);
    void Revers();

    bool GetPoint(double s, CPoint3d* p, short extra = 3);
    bool GetPoint7d(double s, CPoint7d* p, short extra = 3);
    bool GetPointLength(double length, CPoint7d* p, short extra = 3);
    void GetPointOrtho(CPoint3d* p, CPoint3d* pc, short extra = 3);
    double GetOrthPoint(CPoint3d* p);
    double GetDistMin(CPoint3d* p);
    double GetLength() const;

    void Draw(float r = 0.95f,
              float g = 0.72f,
              float b = 0.18f,
              float line_width = 3.0f,
              int steps_per_segment = 16,
              bool draw_points = false,
              bool overlay = true);
    void Render();

    int AddPoint(CPoint3d* p, bool rebuild = true);
    int AddPoint(const CPoint3d& p, bool rebuild = true);
    int AddPoint7d(CPoint7d& p);
    int InsertPoint(int n, CPoint7d* p);
    int DeletePoint(int n);

    void mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
    void mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz);
    void mod_coord_am(CSystemCoord* sc);
    void mod_coord_ma(CSystemCoord* sc);
    void Rotate(CPoint3d* p0, CPoint3d* p1, double alfa);
    void Move(CPoint3d* p1, CPoint3d* p2);
    void Move(CVector* vect, double dist);
    void Mirror();
    void Zoom(CPoint3d* p0, double sx, double sy, double sz);

    bool IsClosed() const;
    void AddKnot(CPoint3d* pnt);
    CPoint3d GetKnot(int ind) const;
    int GetQtyKnots() const { return static_cast<int>(m_Knots.size()); }
    void UpdateKnots();

    void print();
    void printS();
    void printKnots();
    void printToFile(std::string Name);
    bool MakePolylineByQtyKnots(CPolyline* pline, int Qty);
    bool Create(CPolyline* pline);

public:
    bool ShowPoints;
    bool Show1stPoints;
    unsigned long m_color;
    float m_Thickness;
    bool IsSlise;
    int QtyNodes;
    bool IsSnaped;
    int BegLink;
    int EndLink;
    bool DrawTangents;
    bool TangentsFrozen;
    bool Reversed;
    bool WasClosed;
    bool WasClosedTmp;
    int QtyModKnots;
    bool DrawNormals;
    bool Linked;
    bool InitLink;
    int CurveGUID;
    bool IsSharp;
    int QtyPointsPln;

protected:
    int ControlDelta(int num, double dopusk, double ds, int* pr);
    double Iteration(CPoint3d* p, double ds, double s_min);
    double Localized_point(double s, double ds, CPlane* pl, CPoint7d* p);

private:
    CPoint7d* m_p7;
    int m_n;
    std::vector<CPoint3d> m_Knots;
    std::vector<double> m_Params;
    bool m_InitOutLine;
};
