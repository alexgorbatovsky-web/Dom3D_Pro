#include "../OpenGLCompat.h"

#include "SplineCurve.h"
#include "../CPolyline.h"

#include "defines.h"
#include "../ageom.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

extern int INKM(double RES[39], int ISK[7], double T[3], double* A);
extern int STAFM2(int IPR, int N, int LR[], double D, double* A);

CSplineCurve::CSplineCurve()
{
    Alloc();
}

CSplineCurve::CSplineCurve(int num_p)
{
    Alloc();
    if (num_p) {
        m_p7 = new CPoint7d[num_p];
        if (!m_p7) {
            m_n = 0;
            Message_err(IDS_BAD_ALLOC_MEMORY);
            return;
        }
    }
    m_n = num_p;
}

CSplineCurve::CSplineCurve(CPoint3d& p1, CPoint3d& p2)
{
    Alloc();
    m_p7 = new CPoint7d[2];
    if (!m_p7) {
        Message_err(IDS_BAD_ALLOC_MEMORY);
        return;
    }
    m_n = 2;
    *Pnt(0) = p1;
    *Pnt(1) = p2;
    Build();
}

CSplineCurve::CSplineCurve(float rad, float delta)
{
    Alloc();
    const double two_pi = 2.0 * PI;
    const int qty = std::max(4, static_cast<int>(two_pi / delta) + 1);
    Realloc(qty + 1);
    for (int i = 0; i <= qty; ++i) {
        const double a = two_pi * static_cast<double>(i) / static_cast<double>(qty);
        P(i)->x = rad * std::cos(a);
        P(i)->y = rad * std::sin(a);
        P(i)->z = 0.0;
    }
    WasClosed = true;
    Build();
}

CSplineCurve::~CSplineCurve()
{
    Clear();
}

void CSplineCurve::Alloc()
{
    m_p7 = nullptr;
    m_n = 0;
    ShowPoints = false;
    Show1stPoints = true;
    m_color = 0xFFF544A0;
    m_Thickness = 1.0f;
    IsSlise = false;
    IsSnaped = false;
    QtyNodes = 0;
    BegLink = -1;
    EndLink = -1;
    DrawTangents = false;
    TangentsFrozen = false;
    Reversed = false;
    WasClosed = false;
    WasClosedTmp = false;
    QtyModKnots = 0;
    DrawNormals = false;
    Linked = false;
    InitLink = false;
    CurveGUID = 0;
    IsSharp = false;
    QtyPointsPln = 0;
    m_InitOutLine = false;
}

void CSplineCurve::Clear()
{
    delete[] m_p7;
    m_p7 = nullptr;
    m_n = 0;
    m_Knots.clear();
    m_Params.clear();
    m_InitOutLine = false;
}

int CSplineCurve::Realloc(int num_p)
{
    if (num_p == 0) {
        delete[] m_p7;
        m_p7 = nullptr;
        m_n = 0;
        return OK;
    }
    if (m_n >= num_p) {
        m_n = num_p;
        return OK;
    }

    CPoint7d* ptr = new CPoint7d[num_p];
    if (!ptr) {
        Message_err(IDS_BAD_ALLOC_MEMORY);
        return BAD;
    }
    if (m_p7) {
        std::memcpy(ptr, m_p7, sizeof(CPoint7d) * std::min(m_n, num_p));
        delete[] m_p7;
    }
    m_p7 = ptr;
    m_n = num_p;
    return OK;
}

bool CSplineCurve::Copy(CSplineCurve* src)
{
    if (!src)
        return false;
    if (Realloc(src->m_n))
        return false;
    for (int i = 0; i < m_n; ++i)
        m_p7[i] = src->m_p7[i];
    m_Knots = src->m_Knots;
    m_Params = src->m_Params;
    ShowPoints = src->ShowPoints;
    Show1stPoints = src->Show1stPoints;
    m_color = src->m_color;
    m_Thickness = src->m_Thickness;
    WasClosed = src->WasClosed;
    QtyModKnots = src->QtyModKnots;
    m_InitOutLine = false;
    return true;
}

CPoint7d* CSplineCurve::P(int num_p)
{
    if (num_p < 0 || num_p >= m_n) {
        Message_err("Bad index Points");
        return m_p7;
    }
    return &m_p7[num_p];
}

const CPoint7d* CSplineCurve::P(int num_p) const
{
    if (num_p < 0 || num_p >= m_n)
        return m_p7;
    return &m_p7[num_p];
}

CPoint7d* CSplineCurve::PLast()
{
    return m_n > 0 ? &m_p7[m_n - 1] : nullptr;
}

const CPoint7d* CSplineCurve::PLast() const
{
    return m_n > 0 ? &m_p7[m_n - 1] : nullptr;
}

CPoint3d* CSplineCurve::Pnt(int num_p)
{
    return reinterpret_cast<CPoint3d*>(P(num_p));
}

const CPoint3d* CSplineCurve::Pnt(int num_p) const
{
    return reinterpret_cast<const CPoint3d*>(P(num_p));
}

CPoint3d* CSplineCurve::PntLast()
{
    return reinterpret_cast<CPoint3d*>(PLast());
}

const CPoint3d* CSplineCurve::PntLast() const
{
    return reinterpret_cast<const CPoint3d*>(PLast());
}

bool CSplineCurve::Update()
{
    if (!m_p7) {
        Message_err("m_p7==NULL");
        return false;
    }
    if (m_n < 2)
        return true;

    int lr[4] = {1, 4, 7, 7};
    double d = -1;
    int ipr = 0;

    for (int i = 0; i < m_n; ++i)
        P(i)->s = 0;
    m_InitOutLine = false;

    if (STAFM2(ipr, m_n, lr, d, reinterpret_cast<double*>(m_p7)) == BAD)
        return false;

    if (!IsClosed())
        return true;

    CVector v1(P(0)->l, P(0)->m, P(0)->n);
    CVector v2(PLast()->l, PLast()->m, PLast()->n);
    if (!v1.IsCollinear(&v2, PI / 18.0))
        return true;

    CVector mid(v1.l + v2.l, v1.m + v2.m, v1.n + v2.n);
    mid.Normalize();
    P(0)->l = PLast()->l = mid.l;
    P(0)->m = PLast()->m = mid.m;
    P(0)->n = PLast()->n = mid.n;

    for (int i = 0; i < m_n; ++i)
        P(i)->s = 0;
    return STAFM2(ipr, m_n, lr, d, reinterpret_cast<double*>(m_p7)) != BAD;
}

bool CSplineCurve::Build()
{
    for (int i = 0; i < m_n; ++i)
        P(i)->l = P(i)->m = P(i)->n = P(i)->s = 0;

    if (WasClosed) {
        Update();
        return Update();
    }
    return Update();
}

bool CSplineCurve::GetPoint(double s, CPoint3d* p, short extra)
{
    if (!p || !m_p7 || m_n < 1)
        return false;

    double res[39];
    int isk[7] = {1, 12, 7, 2, 1, 30, 0};
    double t[3];

    isk[0] = extra;
    isk[3] = m_n;
    t[0] = s + 1.0;
    t[1] = 1.0;
    t[2] = static_cast<double>(m_n);

    if (INKM(res, isk, t, reinterpret_cast<double*>(m_p7)))
        return false;
    p->x = res[0];
    p->y = res[1];
    p->z = res[2];
    return true;
}

bool CSplineCurve::GetPoint7d(double s, CPoint7d* p, short extra)
{
    if (!p || !m_p7 || m_n < 1)
        return false;

    double res[39];
    int isk[7] = {1, 12, 7, 2, 1, 30, 0};
    double t[3];

    isk[0] = extra;
    isk[3] = m_n;
    t[0] = s + 1.0;
    t[1] = 1.0;
    t[2] = static_cast<double>(m_n);

    if (INKM(res, isk, t, reinterpret_cast<double*>(m_p7)))
        return false;
    p->x = res[0];
    p->y = res[1];
    p->z = res[2];
    p->l = res[3];
    p->m = res[4];
    p->n = res[5];
    p->s = s;

    CVector vect(p->l, p->m, p->n);
    vect.Normalize();
    p->l = vect.l;
    p->m = vect.m;
    p->n = vect.n;
    return true;
}

bool CSplineCurve::GetPointLength(double length, CPoint7d* p, short extra)
{
    if (!p || m_n < 1)
        return false;
    if (m_p7[0].s > 0) {
        message_error_("BAD spline!\n p[0].s>0");
        return false;
    }

    int num = 0;
    double s = 0;
    if (length > P(m_n - 1)->s) {
        num = m_n - 1;
    } else {
        while (num < m_n - 1 && length > P(num)->s)
            ++num;
    }

    double ds = length - P(num)->s;
    if (length == 0) {
        s = 0;
    } else if (length > 0 && num > 0) {
        s = ds / (P(num)->s - P(num - 1)->s);
        s += num;
    } else if (m_n > 1) {
        s = ds / (P(1)->s - P(0)->s);
    }
    return GetPoint7d(s, p, extra);
}

void CSplineCurve::GetPointOrtho(CPoint3d* p, CPoint3d* pc, short extra)
{
    if (!p || !pc)
        return;
    *pc = *p;
    double s = GetOrthPoint(p);
    if (!extra) {
        if (s < 0)
            s = 0;
        if (s > m_n - 1)
            s = m_n - 1;
    }
    GetPoint(s, pc);
}

double CSplineCurve::GetOrthPoint(CPoint3d* p)
{
    if (!p || m_n < 1)
        return 0.0;

    double s_min = 0;
    int ns = 10;
    double ds = 1.0 / static_cast<double>(ns);
    CPoint3d pn;
    double dist_min = 1e15;

    for (int j = 0; j < m_n; ++j) {
        for (int i = 0; i < ns; ++i) {
            GetPoint(j + i * ds, &pn);
            const double dist = dist_POINT(&pn.x, &p->x);
            if (dist_min > dist) {
                dist_min = dist;
                s_min = j + i * ds;
            }
        }
    }

    ds = 0.15;
    for (int i = 0; i < 6; ++i) {
        s_min = Iteration(p, ds, s_min);
        ds *= 0.15;
    }
    return s_min;
}

double CSplineCurve::Iteration(CPoint3d* p, double ds, double s_min)
{
    double sb = s_min - ds / 2.0;
    int ns = 10;
    double dds = ds / static_cast<double>(ns);
    CPoint3d pn;
    double dist_min = 1e15;

    for (int i = 0; i < ns; ++i) {
        GetPoint(sb + i * dds, &pn);
        const double dist = dist_POINT(&pn.x, &p->x);
        if (dist_min > dist) {
            dist_min = dist;
            s_min = sb + i * dds;
        }
    }
    return s_min;
}

double CSplineCurve::GetDistMin(CPoint3d* p)
{
    if (!p || m_n < 1)
        return 1e15;
    const double s = GetOrthPoint(p);
    CPoint3d pc;
    GetPoint(s, &pc);
    return pc.DistTo(p);
}

double CSplineCurve::GetLength() const
{
    return m_n > 0 ? m_p7[m_n - 1].s : 0.0;
}

void CSplineCurve::Draw(float r,
                        float g,
                        float b,
                        float line_width,
                        int steps_per_segment,
                        bool draw_points,
                        bool overlay)
{
    if (!m_p7 || m_n < 1)
        return;

    const bool depth_enabled = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
    if (overlay)
        glDisable(GL_DEPTH_TEST);

    glLineWidth(line_width);
    glColor3f(r, g, b);

    if (m_n == 1) {
        glPointSize(line_width + 3.0f);
        glBegin(GL_POINTS);
        glVertex3f(static_cast<float>(P(0)->x), static_cast<float>(P(0)->y), static_cast<float>(P(0)->z));
        glEnd();
    } else {
        const int steps = std::max(1, steps_per_segment);
        const double s_max = static_cast<double>(m_n - 1);
        CPoint3d p;

        glBegin(GL_LINE_STRIP);
        for (int seg = 0; seg < m_n - 1; ++seg) {
            for (int step = 0; step < steps; ++step) {
                const double s = static_cast<double>(seg) + static_cast<double>(step) / static_cast<double>(steps);
                if (GetPoint(s, &p)) {
                    glVertex3f(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z));
                }
            }
        }
        if (GetPoint(s_max, &p)) {
            glVertex3f(static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z));
        }
        glEnd();
    }

    if (draw_points) {
        glPointSize(line_width + 2.0f);
        glColor3f(std::min(1.0f, r + 0.18f), std::min(1.0f, g + 0.18f), std::min(1.0f, b + 0.18f));
        glBegin(GL_POINTS);
        for (int i = 0; i < m_n; ++i) {
            glVertex3f(static_cast<float>(P(i)->x), static_cast<float>(P(i)->y), static_cast<float>(P(i)->z));
        }
        glEnd();
    }

    if (overlay && depth_enabled)
        glEnable(GL_DEPTH_TEST);
}

void CSplineCurve::Render()
{
    Draw();
}

int CSplineCurve::AddPoint(CPoint3d* p, bool rebuild)
{
    if (!p)
        return BAD;
    return AddPoint(*p, rebuild);
}

int CSplineCurve::AddPoint(const CPoint3d& p, bool rebuild)
{
    if (Realloc(m_n + 1))
        return BAD;
    PLast()->x = p.x;
    PLast()->y = p.y;
    PLast()->z = p.z;
    if (m_n < 2 || !rebuild)
        return OK;
    return Build() ? OK : BAD;
}

int CSplineCurve::AddPoint7d(CPoint7d& p)
{
    if (Realloc(m_n + 1))
        return BAD;
    *PLast() = p;
    return Build() ? OK : BAD;
}

int CSplineCurve::InsertPoint(int n, CPoint7d* p)
{
    if (!p || n < 0 || n > m_n)
        return BAD;
    const int old_n = m_n;
    if (Realloc(m_n + 1))
        return BAD;
    for (int i = old_n; i > n; --i)
        m_p7[i] = m_p7[i - 1];
    m_p7[n] = *p;
    return Build() ? OK : BAD;
}

int CSplineCurve::DeletePoint(int n)
{
    if (n < 0 || n >= m_n)
        return BAD;
    for (int i = n; i < m_n - 1; ++i)
        m_p7[i] = m_p7[i + 1];
    --m_n;
    return Build() ? OK : BAD;
}

void CSplineCurve::mod_coord_ma(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
    for (int i = 0; i < m_n; ++i)
        P(i)->mod_coord_ma(p0, cx, cy, cz);
    for (CPoint3d& knot : m_Knots)
        knot.mod_coord_ma(p0, cx, cy, cz);
}

void CSplineCurve::mod_coord_am(CPoint3d* p0, CVector* cx, CVector* cy, CVector* cz)
{
    for (int i = 0; i < m_n; ++i)
        P(i)->mod_coord_am(p0, cx, cy, cz);
    for (CPoint3d& knot : m_Knots)
        knot.mod_coord_am(p0, cx, cy, cz);
}

void CSplineCurve::mod_coord_am(CSystemCoord* sc)
{
    mod_coord_am(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CSplineCurve::mod_coord_ma(CSystemCoord* sc)
{
    mod_coord_ma(&sc->p0, &sc->cx, &sc->cy, &sc->cz);
}

void CSplineCurve::Rotate(CPoint3d* p0, CPoint3d* p1, double alfa)
{
    for (int i = 0; i < m_n; ++i)
        P(i)->Rotate(p0, p1, alfa);
    for (CPoint3d& knot : m_Knots)
        knot.Rotate(p0, p1, alfa);
}

void CSplineCurve::Move(CPoint3d* p1, CPoint3d* p2)
{
    if (!p1 || !p2)
        return;
    CVector v(p1, p2);
    Move(&v, p1->DistTo(p2));
}

void CSplineCurve::Move(CVector* vect, double dist)
{
    if (!vect)
        return;
    for (int i = 0; i < m_n; ++i)
        P(i)->Move(vect, dist);
    for (CPoint3d& knot : m_Knots)
        knot.Move(vect, dist);
}

void CSplineCurve::Mirror()
{
    for (int i = 0; i < m_n; ++i)
        P(i)->Mirror();
    for (CPoint3d& knot : m_Knots)
        knot.Mirror();
    Update();
}

void CSplineCurve::Zoom(CPoint3d* p0, double sx, double sy, double sz)
{
    if (!p0)
        return;
    for (int i = 0; i < m_n; ++i)
        P(i)->Zoom(p0, sx, sy, sz);
    for (CPoint3d& knot : m_Knots)
        knot.Zoom(p0, sx, sy, sz);
    Update();
}

void CSplineCurve::Revers()
{
    if (m_n < 2)
        return;
    for (int i = 0; i < m_n / 2; ++i)
        std::swap(m_p7[i], m_p7[m_n - 1 - i]);
    for (int i = 0; i < m_n; ++i)
        P(i)->Revers();
    Reversed = !Reversed;
    Update();
}

bool CSplineCurve::IsClosed() const
{
    if (m_n < 2)
        return false;
    CPoint3d p0(const_cast<CPoint7d*>(&m_p7[0]));
    CPoint3d p1(const_cast<CPoint7d*>(&m_p7[m_n - 1]));
    return p0.DistTo(&p1) < DDELTA;
}

void CSplineCurve::AddKnot(CPoint3d* pnt)
{
    if (pnt)
        m_Knots.push_back(*pnt);
}

CPoint3d CSplineCurve::GetKnot(int ind) const
{
    if (ind < 0 || ind >= static_cast<int>(m_Knots.size()))
        return CPoint3d();
    return m_Knots[ind];
}

void CSplineCurve::UpdateKnots()
{
    m_Knots.clear();
    for (int i = 0; i < m_n; ++i)
        m_Knots.push_back(*Pnt(i));
}

double CSplineCurve::Localized_point(double s, double ds, CPlane* pl, CPoint7d* p)
{
    if (!pl || !p)
        return s;
    CPoint7d p1;
    CPoint7d pm;
    for (int i = 0; i < 12; ++i) {
        GetPoint7d(s, &pm);
        GetPoint7d(s + ds, &p1);
        const double d0 = pl->a * pm.x + pl->b * pm.y + pl->c * pm.z + pl->d;
        const double d1 = pl->a * p1.x + pl->b * p1.y + pl->c * p1.z + pl->d;
        if ((d0 <= 0 && d1 >= 0) || (d0 >= 0 && d1 <= 0))
            ds *= 0.5;
        else
            s += ds;
    }
    GetPoint7d(s, p);
    return s;
}

int CSplineCurve::ControlDelta(int num, double dopusk, double ds, int* pr)
{
    if (!pr)
        return BAD;
    *pr = OK;
    if (num < 0 || num >= m_n - 1)
        return BAD;
    CPoint3d p0 = *Pnt(num);
    CPoint3d p1 = *Pnt(num + 1);
    CPoint3d pm;
    GetPoint(num + ds, &pm);
    if (pm.GetDistLine(&p0, &p1) > dopusk)
        *pr = BAD;
    return OK;
}

void CSplineCurve::print()
{
    FILE* strm = std::fopen("c:\\temp\\stdout.txt", "a+");
    if (!strm)
        return;
    std::fprintf(strm, "SPLINE\n%d\n", m_n);
    for (int i = 0; i < m_n; ++i)
        std::fprintf(strm, "%10.4lf  %10.4lf %10.4lf\n", P(i)->x, P(i)->y, P(i)->z);
    std::fclose(strm);
}

void CSplineCurve::printS()
{
    FILE* strm = std::fopen("c:\\temp\\stdout.txt", "a+");
    if (!strm)
        return;
    std::fprintf(strm, "SPLINE\n%d\n", m_n);
    for (int i = 0; i < m_n; ++i)
        std::fprintf(strm, "%10.4lf  %10.4lf %10.4lf  %10.4lf\n", P(i)->x, P(i)->y, P(i)->z, P(i)->s);
    std::fclose(strm);
}

void CSplineCurve::printKnots()
{
    FILE* strm = std::fopen("c:\\temp\\stdout.txt", "a+");
    if (!strm)
        return;
    std::fprintf(strm, "POLYLINE\n%zu\n", m_Knots.size());
    for (const CPoint3d& knot : m_Knots)
        std::fprintf(strm, "%10.4lf  %10.4lf %10.4lf\n", knot.x, knot.y, knot.z);
    std::fclose(strm);
}
void CSplineCurve::printToFile(std::string Name)
{
    std::string fileName = std::string("c:\\temp\\") + Name + ".txt";
    FILE* strm = std::fopen(fileName.c_str(), "a+");
    if (nullptr == strm)
        return;
    fprintf(strm, "SPLINE\n");
    fprintf(strm, " %d\n", m_n);

    for (int i = 0; i < m_n; i++)
        fprintf(strm, "%10.4lf  %10.4lf %10.4lf\n", P(i)->x, P(i)->y, P(i)->z);
    fclose(strm);
}

bool CSplineCurve::MakePolylineByQtyKnots(CPolyline* pline, int Qty)
{
    if (Qty < 2) {
        Message_err("Qty< 2, MakePolylineByQtyKnots");
        return false;
    }
    double Length = GetLength();
    double dLen = Length / (double)(Qty - 1);
    for (int i = 0; i < Qty; i++) {
        double len = dLen * i;
        CPoint7d p7;
        GetPointLength(len, &p7);
        CPoint3d p3d(p7.x, p7.y, p7.z);
        pline->AddPoint(&p3d);
    }

    return true;
}

bool CSplineCurve::Create(CPolyline* pline)
{
    if (Realloc(pline->np()))
        return false;
  
    for (int i = 0; i < pline->np(); i++) {
        Pnt(i)->x = pline->P(i)->x;
        Pnt(i)->y = pline->P(i)->y;
        Pnt(i)->z = pline->P(i)->z;
    }
    Build();
    return true;
}
