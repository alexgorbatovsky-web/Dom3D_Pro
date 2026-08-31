#include "Line2D.h"

#include <algorithm>
#include <cmath>
#include <set>

void Step(const char* text);

namespace {

cVec2 Lerp2(const cVec2& a, const cVec2& b, double t)
{
    return a + (b - a) * t;
}

double Dot2(const cVec2& a, const cVec2& b)
{
    return a.x * b.x + a.y * b.y;
}

double Cross2(const cVec2& a, const cVec2& b)
{
    return a.x * b.y - a.y * b.x;
}

bool SamePoint2(const cVec2& a, const cVec2& b, double eps)
{
    return Length2(a - b) <= eps * eps;
}

int FindFaceVertexIndex2(const Face2D& face, const cVec2& point, double eps)
{
    for (int i = 0; i < static_cast<int>(face.verts.size()); ++i) {
        if (SamePoint2(face.verts[static_cast<size_t>(i)], point, eps))
            return i;
    }
    return -1;
}

bool PointOnSegment2(const cVec2& point, const cVec2& a, const cVec2& b, double eps, double* out_t = nullptr)
{
    const cVec2 ab = b - a;
    const cVec2 ap = point - a;
    if (std::fabs(Cross2(ab, ap)) > eps)
        return false;

    const double ab2 = Length2(ab);
    if (ab2 <= eps * eps) {
        if (!SamePoint2(point, a, eps))
            return false;
        if (out_t)
            *out_t = 0.0;
        return true;
    }

    double t = Dot2(ap, ab) / ab2;
    if (t < -eps || t > 1.0 + eps)
        return false;
    t = std::clamp(t, 0.0, 1.0);
    if (out_t)
        *out_t = t;
    return true;
}

enum SegSegRelation {
    SSR_NONE = 0,
    SSR_POINT = 1,
    SSR_OVERLAP = 2
};

struct SegSegHit {
    SegSegRelation rel = SSR_NONE;
    double tA = 0.0;
    double tB = 0.0;
    cVec2 pt;
};

SegSegHit IntersectSegments2(const cVec2& a0, const cVec2& a1, const cVec2& b0, const cVec2& b1, double eps)
{
    SegSegHit out;
    const cVec2 r = a1 - a0;
    const cVec2 s = b1 - b0;
    const cVec2 qp = b0 - a0;
    const double rxs = Cross2(r, s);
    const double qpxr = Cross2(qp, r);

    if (std::fabs(rxs) <= eps && std::fabs(qpxr) <= eps) {
        const double rr = Length2(r);
        if (rr <= eps * eps) {
            if (SamePoint2(a0, b0, eps)) {
                out.rel = SSR_POINT;
                out.pt = a0;
            }
            return out;
        }

        double t0 = Dot2(b0 - a0, r) / rr;
        double t1 = Dot2(b1 - a0, r) / rr;
        if (t0 > t1)
            std::swap(t0, t1);

        const double lo = std::max(0.0, t0);
        const double hi = std::min(1.0, t1);
        if (hi < -eps || lo > 1.0 + eps || hi < lo - eps)
            return out;

        if (hi - lo <= eps) {
            out.rel = SSR_POINT;
            out.tA = std::clamp(lo, 0.0, 1.0);
            out.pt = Lerp2(a0, a1, out.tA);
            PointOnSegment2(out.pt, b0, b1, eps, &out.tB);
            return out;
        }

        out.rel = SSR_OVERLAP;
        out.tA = lo;
        out.tB = hi;
        out.pt = Lerp2(a0, a1, lo);
        return out;
    }

    if (std::fabs(rxs) <= eps)
        return out;

    double t = Cross2(qp, s) / rxs;
    double u = Cross2(qp, r) / rxs;
    if (t >= -eps && t <= 1.0 + eps && u >= -eps && u <= 1.0 + eps) {
        out.rel = SSR_POINT;
        out.tA = std::clamp(t, 0.0, 1.0);
        out.tB = std::clamp(u, 0.0, 1.0);
        out.pt = Lerp2(a0, a1, out.tA);
    }
    return out;
}

void DeduplicateHits2(std::vector<CellCutHit>& hits, double eps)
{
    std::vector<CellCutHit> out;
    for (const CellCutHit& hit : hits) {
        bool merged = false;
        for (CellCutHit& existing : out) {
            if (hit.segIndex == existing.segIndex
                && std::fabs(hit.t - existing.t) <= 1e-8
                && SamePoint2(hit.pt, existing.pt, eps)) {
                if (existing.vertexIndex == -1 && hit.vertexIndex != -1)
                    existing.vertexIndex = hit.vertexIndex;
                merged = true;
                break;
            }
        }
        if (!merged)
            out.push_back(hit);
    }

    std::sort(out.begin(), out.end(), [](const CellCutHit& a, const CellCutHit& b) {
        if (a.segIndex != b.segIndex)
            return a.segIndex < b.segIndex;
        return a.t < b.t;
    });
    hits.swap(out);
}

void AddUniquePoint2(std::vector<cVec2>& points, const cVec2& point, double eps)
{
    for (const cVec2& existing : points) {
        if (SamePoint2(existing, point, eps))
            return;
    }
    points.push_back(point);
}

} // namespace

void InitCellCutInfo(CellCutInfo& info)
{
    info = CellCutInfo{};
}

PointFacePos ClassifyPointInFace2(const Face2D& face, const cVec2& point, double eps)
{
    const int count = static_cast<int>(face.verts.size());
    if (count < 3)
        return PFP_OUTSIDE;

    // Keep vertex classification consistent with AnalyzeFaceCut().
    // A point can be within the vertex tolerance while the cross-product
    // tolerance used for a long edge would otherwise reject it as boundary.
    for (int i = 0; i < count; ++i) {
        if (SamePoint2(point, face.verts[static_cast<size_t>(i)], eps))
            return PFP_BOUNDARY;
    }

    for (int i = 0; i < count; ++i) {
        const cVec2& a = face.verts[static_cast<size_t>(i)];
        const cVec2& b = face.verts[static_cast<size_t>((i + 1) % count)];
        if (PointOnSegment2(point, a, b, eps))
            return PFP_BOUNDARY;
    }

    bool inside = false;
    for (int i = 0, j = count - 1; i < count; j = i++) {
        const cVec2& vi = face.verts[static_cast<size_t>(i)];
        const cVec2& vj = face.verts[static_cast<size_t>(j)];
        const bool crosses = ((vi.y > point.y) != (vj.y > point.y));
        if (crosses) {
            const double x_intersection = vi.x + (vj.x - vi.x) * (point.y - vi.y) / (vj.y - vi.y);
            if (point.x < x_intersection)
                inside = !inside;
        }
    }
    return inside ? PFP_INSIDE : PFP_OUTSIDE;
}



bool AnalyzeFaceCut(const Face2D& face, const std::vector<cVec2>& cut, CellCutInfo& info, double eps)
{
    InitCellCutInfo(info);
    const int face_count = static_cast<int>(face.verts.size());
    const int cut_count = static_cast<int>(cut.size());
    if (face_count < 3 || cut_count < 2)
        return false;

    for (int j = 0; j < cut_count; ++j) {
        for (int i = 0; i < face_count; ++i) {
            if (SamePoint2(cut[j], face.verts[i], eps)) {
                // PntTouchedFaceVertx stores indices of Cut points, not face vertices.
                info.PntTouchedFaceVertx.push_back(j);
                break;
            }
        }
    }

    for (int s = 0; s < cut_count - 1; ++s) {
        for (int e = 0; e < face_count; ++e) {
            const SegSegHit hit = IntersectSegments2(
                cut[static_cast<size_t>(s)],
                cut[static_cast<size_t>(s + 1)],
                face.verts[static_cast<size_t>(e)],
                face.verts[static_cast<size_t>((e + 1) % face_count)],
                eps);

            if (hit.rel == SSR_OVERLAP) {
                info.hasBorderOverlap = true;
                continue;
            }
            if (hit.rel != SSR_POINT)
                continue;

            CellCutHit cell_hit;
            cell_hit.edgeIndex = e;
            cell_hit.vertexIndex = FindFaceVertexIndex2(face, hit.pt, eps);
            cell_hit.segIndex = s;
            cell_hit.t = hit.tA;
            cell_hit.u = hit.tB;
            cell_hit.pt = hit.pt;
            info.hits.push_back(cell_hit);
        }
    }

    DeduplicateHits2(info.hits, eps);
    std::set<int> vertex_set;
    for (const CellCutHit& hit : info.hits) {
        if (hit.vertexIndex != -1)
            vertex_set.insert(hit.vertexIndex);
    }
    info.touchedFaceVertices.assign(vertex_set.begin(), vertex_set.end());
    return true;
}

static void FindPointOfFaceinCut(const Face2D& face, const std::vector<int>& vrts, const std::vector<cVec2>& cut, std::vector<int>& points, double eps)
{
    bool IClosed = EqualPoint2(cut.front(), cut.back(), eps);
    int n = (int)cut.size();
    if (IClosed)
        n--;
    for (int i = 0; i < n; ++i)
    {
        for (size_t j = 0; j < vrts.size(); ++j)
        {
            if (SamePoint2(cut[static_cast<size_t>(i)],
                           face.verts[static_cast<size_t>(vrts[j])], eps))
            {
                points.push_back(i);
                break;
            }
        }
    }
}

static int AnalizPointOfFaceinCut(const Face2D& face, const std::vector<cVec2>& cut, std::vector<int>& points, double eps, int& v1, int& v2)
{
    if (points.size() != 3)
        return 0;
    std::sort(points.begin(), points.end());
    int faceVertexCount = static_cast<int>(face.verts.size());
    if (faceVertexCount > 3
        && SamePoint2(face.verts.front(), face.verts.back(), eps)) {
        --faceVertexCount;
    }

    // Var-2A is a quadrilateral case: of the three touched vertices, the
    // segment joining opposite vertices is the segment that splits the face.
    if (faceVertexCount != 4)
        return 0;

    int faceIndices[3] = {-1, -1, -1};
    for (size_t pointIndex = 0; pointIndex < points.size(); ++pointIndex) {
        for (int faceIndex = 0; faceIndex < faceVertexCount; ++faceIndex) {
            if (SamePoint2(cut[static_cast<size_t>(points[pointIndex])],
                           face.verts[static_cast<size_t>(faceIndex)], eps)) {
                faceIndices[pointIndex] = faceIndex;
                break;
            }
        }
    }

    const auto detectOppositePair = [&](int first, int second) {
        if (faceIndices[first] < 0 || faceIndices[second] < 0)
            return false;
        if (std::abs(faceIndices[first] - faceIndices[second]) != 2)
            return false;
        v1 = faceIndices[first];
        v2 = faceIndices[second];
        return true;
    };

    if (detectOppositePair(0, 1) || detectOppositePair(1, 2))
        return 2;

    // For a closed contour, the last unique node and the first node are also
    // consecutive.  Omitting this pair made Var-2A depend on where the same
    // contour happened to start.
    const bool closed = cut.size() > 1
        && SamePoint2(cut.front(), cut.back(), eps);
    if (closed && detectOppositePair(2, 0))
        return 2;

    return 0;
}

static bool AreAdjacentFaceVertices(int v0, int v1, int vertexCount)
{
    if (vertexCount < 3)
        return false;

    if (v0 < 0 || v0 >= vertexCount)
        return false;

    if (v1 < 0 || v1 >= vertexCount)
        return false;

    if (v0 == v1)
        return false;

    const int next0 = (v0 + 1) % vertexCount;
    const int next1 = (v1 + 1) % vertexCount;

    return next0 == v1 || next1 == v0;
}

static bool DetectVariant8(
    const Face2D& face,
    const CellCutHit& edgeHit,
    int touchedVertex,
    CellCutInfo& info,
    double eps)
{
    int n = static_cast<int>(face.verts.size());

    if (n < 3)
        return false;

    // If the first vertex is repeated at the end:
    // 0, 1, 2, 3, 0
    if (n > 3 && EqualPoint2(face.verts.front(), face.verts.back(), eps))
        --n;

    if (n < 3)
        return false;

    const int e0 = edgeHit.edgeIndex;

    if (e0 < 0 || e0 >= n)
        return false;

    const int e1 = (e0 + 1) % n;

    if (touchedVertex < 0 || touchedVertex >= n)
        return false;

    // For Var-8, the hit must lie inside the edge,
    // and not coincide with the vertex.
    if (edgeHit.vertexIndex >= 0)
        return false;

    if (edgeHit.u <= eps || edgeHit.u >= 1.0 - eps)
        return false;

    const cVec2& hitPoint = edgeHit.pt;

    int vertexToMove = -1;

    if (AreAdjacentFaceVertices(e0, touchedVertex, n))
        vertexToMove = e0;

    if (AreAdjacentFaceVertices(e1, touchedVertex, n))
    {
        if (vertexToMove >= 0)
            return false;

        vertexToMove = e1;
    }

    if (vertexToMove < 0 || vertexToMove >= n)
        return false;

    info.VariantCut = 8;
    info.vertexToMove = vertexToMove;
    info.moveTarget = hitPoint;
    info.hasMoveTarget = true;

    return true;
}

static bool CutHitSegmentContainsFaceVertex(
    const Face2D& face,
    const std::vector<cVec2>& cut,
    const CellCutHit& hit,
    int faceVertex,
    double eps)
{
    if (faceVertex < 0
        || faceVertex >= static_cast<int>(face.verts.size())
        || hit.segIndex < 0
        || hit.segIndex + 1 >= static_cast<int>(cut.size())) {
        return false;
    }

    return PointOnSegment2(
        face.verts[static_cast<size_t>(faceVertex)],
        cut[static_cast<size_t>(hit.segIndex)],
        cut[static_cast<size_t>(hit.segIndex + 1)], eps);
}


void ClassifyFaceCut(const Face2D& face, const std::vector<cVec2>& cut, CellCutInfo& info, double eps)
{
    const bool closed = cut.size() > 1 && EqualPoint2(cut.front(), cut.back(), eps);
    info.cutCase = FACECUT_NONE;
    info.boundaryContactCount = 0;
    info.interiorNodeCount = 0;
    info.hasInteriorSegment = false;
    info.PntInFace.clear();

    if (face.verts.size() < 3 || cut.size() < 2)
        return;

    std::vector<cVec2> boundary_points;
    for (const CellCutHit& hit : info.hits)
        AddUniquePoint2(boundary_points, hit.pt, eps);

    
    for (size_t i = 0; i < cut.size(); ++i) {
        const PointFacePos pos = ClassifyPointInFace2(face, cut[i], eps);
        if (pos == PFP_BOUNDARY)
            AddUniquePoint2(boundary_points, cut[i], eps);
 
        if (pos == PFP_INSIDE) {
            info.PntInFace.push_back(static_cast<int>(i));
            ++info.interiorNodeCount;
            info.hasInteriorSegment = true;
        }
    }

    info.boundaryContactCount = static_cast<int>(boundary_points.size());
    info.VariantCut = 0;
    int NumCutEdge = 0;
    int edgeIndex1 = -1;
    int edgeIndex2 = -1;
    int NumCutVertex = 0;

    for (int i = 0; i < info.hits.size(); ++i) {
        if (fabs(info.hits[i].u) > 0.001 && fabs(info.hits[i].u - 1) > 0.001) {
            NumCutEdge++;
            NumCutVertex = info.hits[i].segIndex;
            if (fabs(info.hits[i].t - 1) < 0.001)
                NumCutVertex++;
            if (edgeIndex1 == -1)
                edgeIndex1 = info.hits[i].edgeIndex;
            else
                edgeIndex2 = info.hits[i].edgeIndex;
        }
    }
    info.NumCutEdge = NumCutEdge;
    info.edgeIndex1 = edgeIndex1;
    info.edgeIndex2 = edgeIndex2;
    info.NumCutVertex = NumCutVertex;

    if (info.touchedFaceVertices.size() == 2 && info.NumCutEdge == 1 && info.interiorNodeCount == 0) {
        for (const CellCutHit& hit : info.hits) {
            for (int touchedVertex : info.touchedFaceVertices) {
                // Var-8 moves the endpoint of the crossed face edge which is
                // adjacent to the vertex reached by that same cut segment.
                // Choosing either touched vertex merely by vector order can
                // move the opposite shared mesh vertex and create a fan of
                // distorted cells at a trim corner.
                if (!CutHitSegmentContainsFaceVertex(
                        face, cut, hit, touchedVertex, eps)) {
                    continue;
                }
                if (DetectVariant8(
                        face, hit, touchedVertex, info, eps)) {
                    return;
                }
            }
        }
    }

    if (info.touchedFaceVertices.size() == 1 && info.NumCutEdge == 1 && info.interiorNodeCount == 0) {
        info.VariantCut = 7;
        info.cutCase = FACECUT_SPLIT_2;
        return;
    }

    if (info.touchedFaceVertices.size() == 3 && info.interiorNodeCount == 0) {
        std::vector<int> points;
        FindPointOfFaceinCut(face, info.touchedFaceVertices, cut, points, eps);
        int v1, v2;
        int v = AnalizPointOfFaceinCut(face, cut, points, eps, v1, v2);
        if (v == 2) {
            info.VariantCut = 2;
            info.cutCase = FACECUT_SPLIT_2;
            info.touchedFaceVertices = {v1, v2};
            return;
        }
        else {
            info.VariantCut = 0;
            info.cutCase = FACECUT_NONE;
            return;
        }
    }
    if (info.touchedFaceVertices.size() == 2 && info.interiorNodeCount == 0) {
        std::sort(info.touchedFaceVertices.begin(), info.touchedFaceVertices.end());
        if (info.touchedFaceVertices[1] - info.touchedFaceVertices[0] == 1) {
            info.VariantCut = 0;
            info.cutCase = FACECUT_NONE;
            return;
        }
        if (info.touchedFaceVertices[1] - info.touchedFaceVertices[0] == 3) {
            info.VariantCut = 0;
            info.cutCase = FACECUT_NONE;
            return;
        }
    }

    if (info.touchedFaceVertices.size()) {

        if (info.interiorNodeCount == 0) {
            info.VariantCut = 2;
        }
        else if (info.interiorNodeCount == 1)
            info.VariantCut = 3;
        else if (closed && info.interiorNodeCount == 2)
            info.VariantCut = 3;
    }

    if (info.touchedFaceVertices.size() == 1 && info.interiorNodeCount == 1) {
        if (info.NumCutEdge == 1) {
            info.VariantCut = 5;
        }
    }
    if (info.touchedFaceVertices.size() == 2 && info.interiorNodeCount == 1) {
        if (info.NumCutEdge == 1) {
            info.VariantCut = 6;
        }
    }
    if (info.touchedFaceVertices.size() == 2 && info.PntInFace.size() > 1) {
        info.VariantCut = 11;
    }


    if (!info.hasInteriorSegment)
        return;
    if (info.boundaryContactCount == 2)
        info.cutCase = FACECUT_SPLIT_2;
    else
        info.cutCase = FACECUT_COMPLEX;
}
