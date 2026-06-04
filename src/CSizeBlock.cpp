#include "SizeBlock.h"


// CSizeBlock

void CSizeBlock::Update()
{
    for (int i = 0; i < 4; i++) {
        m_p[i].y = Y_min;
        m_p[i + 4].y = Y_max;
    }
    m_p[0].x = m_p[2].x = m_p[4].x = m_p[6].x = X_min;
    m_p[1].x = m_p[3].x = m_p[7].x = m_p[5].x = X_max;

    m_p[0].z = m_p[1].z = m_p[4].z = m_p[5].z = Z_min;
    m_p[2].z = m_p[3].z = m_p[7].z = m_p[6].z = Z_max;

    CPlane plX(-1, 0, 0, 0);
    CPlane plY(0, -1, 0, 0);
    CPlane plZ(0, 0, -1, 0);

    m_plane[0] = m_plane[1] = plX;
    m_plane[0].d = X_min;
    m_plane[1].d = X_max;

    m_plane[2] = m_plane[3] = plY;
    m_plane[2].d = Y_min;
    m_plane[3].d = Y_max;

    m_plane[4] = m_plane[5] = plZ;
    m_plane[4].d = Z_min;
    m_plane[5].d = Z_max;

    m_pc.x = (X_min + X_max) / 2.0;
    m_pc.y = (Y_min + Y_max) / 2.0;
    m_pc.z = (Z_min + Z_max) / 2.0;

    m_Rad = m_pc.DistTo(&m_p[0]);
    for (int i = 1; i < 8; i++) {
        double dist = m_pc.DistTo(&m_p[i]);
        if (dist > m_Rad)
            m_Rad = dist;
    }
}

int CSizeBlock::CrossLine(CPoint3d* p1, CPoint3d* p2, CPoint3d* pc, CPlane* pl)
{
    double dopusk = DELTA;
    for (int i = 0; i < 6; i++) {
        double delta1 = p1->x * m_plane[i].a + p1->y * m_plane[i].b + p1->z * m_plane[i].c + m_plane[i].d;
        double delta2 = p2->x * m_plane[i].a + p2->y * m_plane[i].b + p2->z * m_plane[i].c + m_plane[i].d;
        if ((delta1<dopusk && delta2>-dopusk) || (delta1 > -dopusk && delta2 < dopusk)) {
            if (m_plane[i].cross_Line(p1, p2, pc) == OK_AG) {
                *pl = m_plane[i];
                return 1;
            }
        }
    }
    return 0;
}


bool CSizeBlock::IsPointIn(CSizeBlock* Block2, CPoint3d* p_In)
{//ѕровер€ем, находитс€ ли углова€ точка m_p[i] в блоке Block2
    for (int i = 0; i < 8; i++)
        if (m_p[i].IsPointIn(Block2)) {
            *p_In = m_p[i];
            return true;
        }
    return false;
}

void CSizeBlock::Move(CVector* dir, double dist)
{
    if (dist < DDELTA)
        return;
    for (int i = 0; i < 8; i++)
        m_p[i].Move(dir, dist);
    for (int i = 0; i < 6; i++)
        m_plane[i].Move(dir, dist);
    m_pc.Move(dir, dist);
    X_max = m_p[7].x;
    X_min = m_p[0].x;

    Y_max = m_p[7].y;
    Y_min = m_p[0].y;

    Z_max = m_p[7].z;
    Z_min = m_p[0].z;
    Update();
}

void CSizeBlock::Move(CPoint3d* p1, CPoint3d* p2)
{
    CVector	dir(p1, p2);
    double dist = p1->DistTo(p2);
    Move(&dir, dist);
}

void CSizeBlock::print()
{
    FILE* strm = NULL;
    for (int i = 0; i < 6; i++)
        m_plane[i].print(strm);
}

void CSizeBlock::AddPoint(CPoint3d* p)
{
    if (X_min > p->x)
        X_min = p->x;
    if (X_max < p->x)
        X_max = p->x;

    if (Y_min > p->y)
        Y_min = p->y;
    if (Y_max < p->y)
        Y_max = p->y;

    if (Z_min > p->z)
        Z_min = p->z;
    if (Z_max < p->z)
        Z_max = p->z;

    m_pc.x = (X_max - X_min) / 2.0;
    m_pc.y = (Y_max - Y_min) / 2.0;
    m_pc.z = (Z_max - Z_min) / 2.0;
}

CPoint3d CSizeBlock::GetMin()
{
    CPoint3d p(X_min, Y_min, Z_min);

    return p;
}

CPoint3d CSizeBlock::GetMax()
{
    CPoint3d p(X_max, Y_max, Z_max);

    return p;
}