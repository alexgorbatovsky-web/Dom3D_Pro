#include "Net.h"

#include "CMesh3D.h"
#include "solid/SurfaceFace.h"

#include <BRepTools.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRep_Tool.hxx>
#include <Geom_BSplineSurface.hxx>
#include <GeomLProp_SLProps.hxx>
#include <Geom_Surface.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_Orientation.hxx>
#include <TopLoc_Location.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <utility>

void Step(const char* text);

namespace {
bool is_finite_point(const gp_Pnt& point)
{
    return std::isfinite(point.X())
        && std::isfinite(point.Y())
        && std::isfinite(point.Z());
}

bool is_finite_vec(const gp_Vec& vec)
{
    return std::isfinite(vec.X())
        && std::isfinite(vec.Y())
        && std::isfinite(vec.Z());
}

double parameter_step(double min_value, double max_value)
{
    if (std::isfinite(min_value) && std::isfinite(max_value) && max_value > min_value) {
        return std::max((max_value - min_value) * 1.0e-5, 1.0e-7);
    }
    return 1.0e-5;
}

double clamp_parameter(double value, double min_value, double max_value)
{
    if (std::isfinite(min_value) && value < min_value)
        return min_value;
    if (std::isfinite(max_value) && value > max_value)
        return max_value;
    return value;
}

bool point_on_surface(const Handle(Geom_Surface)& surface, double u, double v, gp_Pnt& point)
{
    try {
        surface->D0(u, v, point);
        return is_finite_point(point);
    } catch (const Standard_Failure&) {
        return false;
    }
}

bool normal_from_near_points(const Handle(Geom_Surface)& surface,
                             double u,
                             double v,
                             gp_Vec& normal)
{
    Standard_Real u_min = 0.0;
    Standard_Real u_max = 0.0;
    Standard_Real v_min = 0.0;
    Standard_Real v_max = 0.0;
    surface->Bounds(u_min, u_max, v_min, v_max);

    const double du = parameter_step(u_min, u_max);
    const double dv = parameter_step(v_min, v_max);

    gp_Pnt center;
    if (!point_on_surface(surface, u, v, center))
        return false;

    const std::pair<double, double> offsets[] = {
        { du, 0.0 }, { 0.0, dv }, { -du, 0.0 }, { 0.0, -dv },
        { du, dv }, { -du, dv }, { -du, -dv }, { du, -dv },
        { 2.0 * du, dv }, { -2.0 * du, dv }, { du, 2.0 * dv }, { du, -2.0 * dv }
    };

    gp_Vec best_normal;
    double best_square = 0.0;
    for (const auto& first_offset : offsets) {
        gp_Pnt first_point;
        const double first_u = clamp_parameter(u + first_offset.first, u_min, u_max);
        const double first_v = clamp_parameter(v + first_offset.second, v_min, v_max);
        if (!point_on_surface(surface, first_u, first_v, first_point))
            continue;
        const gp_Vec first_vec(center, first_point);
        if (first_vec.SquareMagnitude() <= 1.0e-20)
            continue;

        for (const auto& second_offset : offsets) {
            gp_Pnt second_point;
            const double second_u = clamp_parameter(u + second_offset.first, u_min, u_max);
            const double second_v = clamp_parameter(v + second_offset.second, v_min, v_max);
            if (!point_on_surface(surface, second_u, second_v, second_point))
                continue;
            const gp_Vec second_vec(center, second_point);
            if (second_vec.SquareMagnitude() <= 1.0e-20)
                continue;

            gp_Vec candidate = first_vec.Crossed(second_vec);
            const double square = candidate.SquareMagnitude();
            if (square > best_square) {
                best_square = square;
                best_normal = candidate;
            }
        }
    }

    if (best_square <= 1.0e-20 || !is_finite_vec(best_normal))
        return false;

    best_normal.Normalize();
    normal = best_normal;
    return true;
}

double point_segment_distance(const CPoint8d& point,
                              const CPoint8d& first,
                              const CPoint8d& second)
{
    const double dx = second.x - first.x;
    const double dy = second.y - first.y;
    const double dz = second.z - first.z;
    const double length_sq = dx * dx + dy * dy + dz * dz;
    double alpha = 0.0;
    if (length_sq > 1.0e-24) {
        alpha = ((point.x - first.x) * dx
               + (point.y - first.y) * dy
               + (point.z - first.z) * dz) / length_sq;
        alpha = std::clamp(alpha, 0.0, 1.0);
    }
    const double px = first.x + dx * alpha;
    const double py = first.y + dy * alpha;
    const double pz = first.z + dz * alpha;
    const double ex = point.x - px;
    const double ey = point.y - py;
    const double ez = point.z - pz;
    return std::sqrt(ex * ex + ey * ey + ez * ez);
}

double diagonal_line_distance(const CPoint8d& first,
                              const CPoint8d& second,
                              const CPoint8d& third,
                              const CPoint8d& fourth)
{
    const double ax = second.x - first.x;
    const double ay = second.y - first.y;
    const double az = second.z - first.z;
    const double bx = fourth.x - third.x;
    const double by = fourth.y - third.y;
    const double bz = fourth.z - third.z;
    const double cx = ay * bz - az * by;
    const double cy = az * bx - ax * bz;
    const double cz = ax * by - ay * bx;
    const double cross_length = std::sqrt(cx * cx + cy * cy + cz * cz);
    if (cross_length <= 1.0e-18) {
        return point_segment_distance(first, third, fourth);
    }
    return std::fabs((third.x - first.x) * cx
                   + (third.y - first.y) * cy
                   + (third.z - first.z) * cz) / cross_length;
}

void add_parameter(std::vector<double>& parameters, double value,
                   double minimum, double maximum)
{
    const double epsilon = std::max((maximum - minimum) * 1.0e-10, 1.0e-12);
    if (!std::isfinite(value)
        || value <= minimum + epsilon
        || value >= maximum - epsilon) {
        return;
    }
    parameters.push_back(value);
}

void sort_unique_parameters(std::vector<double>& parameters,
                            double minimum, double maximum)
{
    std::sort(parameters.begin(), parameters.end());
    const double epsilon = std::max((maximum - minimum) * 1.0e-10, 1.0e-12);
    parameters.erase(std::unique(parameters.begin(), parameters.end(),
        [epsilon](double first, double second) {
            return std::fabs(first - second) <= epsilon;
        }), parameters.end());
}

std::vector<double> orthogonal_samples(const std::vector<double>& parameters)
{
    std::vector<double> samples = parameters;
    samples.reserve(parameters.size() * 2);
    for (size_t i = 0; i + 1 < parameters.size(); ++i) {
        samples.push_back((parameters[i] + parameters[i + 1]) * 0.5);
    }
    std::sort(samples.begin(), samples.end());
    return samples;
}
}

size_t CNet::Index(int s, int t) const
{
    return static_cast<size_t>(t * qty_s_ + s);
}

CPoint8d* CNet::P(int s, int t)
{
    if (s < 0 || t < 0 || s >= qty_s_ || t >= qty_t_)
        return nullptr;
    return &points_[Index(s, t)];
}

const CPoint8d* CNet::P(int s, int t) const
{
    if (s < 0 || t < 0 || s >= qty_s_ || t >= qty_t_)
        return nullptr;
    return &points_[Index(s, t)];
}

bool CSurfaceFace::GetPoint(double U, double V, CPoint8d* pnt)
{
    if (!pnt || m_Face.IsNull())
        return false;

    try {
        const TopoDS_Face face = TopoDS::Face(m_Face);
        TopLoc_Location location;
        Handle(Geom_Surface) surface = BRep_Tool::Surface(face, location);
        if (surface.IsNull())
            return false;

        gp_Pnt point;
        gp_Vec d1u;
        gp_Vec d1v;
        surface->D1(U, V, point, d1u, d1v);

        const gp_Trsf& transform = location.Transformation();
        point.Transform(transform);
        d1u.Transform(transform);
        d1v.Transform(transform);

        gp_Vec normal = d1u.Crossed(d1v);
        if (normal.SquareMagnitude() > 1.0e-20 && is_finite_vec(normal)) {
            normal.Normalize();
        } else {
            normal = gp_Vec();
            GeomLProp_SLProps props(surface, U, V, 1, 1.0e-7);
            if (props.IsNormalDefined()) {
                normal = gp_Vec(props.Normal());
            }
            if (normal.SquareMagnitude() <= 1.0e-20 || !is_finite_vec(normal)) {
                if (!normal_from_near_points(surface, U, V, normal)) {
                    return false;
                }
            }
            normal.Transform(transform);
            if (normal.SquareMagnitude() <= 1.0e-20 || !is_finite_vec(normal))
                return false;
            normal.Normalize();
        }
       if (face.Orientation() == TopAbs_REVERSED)
            normal.Reverse();

        pnt->x = point.X();
        pnt->y = point.Y();
        pnt->z = point.Z();
        pnt->l = normal.X();
        pnt->m = normal.Y();
        pnt->n = normal.Z();
        pnt->s = U;
        pnt->t = V;
        return true;
    } catch (const Standard_Failure&) {
        return false;
    }
}

int CNet::Build(CSurfaceFace* mm, double delta)
{
    points_.clear();
    qty_s_ = 0;
    qty_t_ = 0;
    if (!mm || mm->m_Face.IsNull())
        return 1;

    delta = std::max(delta, 1.0e-5);
    try {
        const TopoDS_Face face = TopoDS::Face(mm->m_Face);
        Standard_Real u_min = 0.0;
        Standard_Real u_max = 0.0;
        Standard_Real v_min = 0.0;
        Standard_Real v_max = 0.0;
        BRepTools::UVBounds(face, u_min, u_max, v_min, v_max);
        if (!std::isfinite(u_min) || !std::isfinite(u_max)
            || !std::isfinite(v_min) || !std::isfinite(v_max)
            || u_max <= u_min || v_max <= v_min) {
            return 2;
        }

        std::vector<double> u_parameters{u_min, u_max};
        std::vector<double> v_parameters{v_min, v_max};
        BRepAdaptor_Surface adaptor(face);
        if (adaptor.GetType() == GeomAbs_BSplineSurface) {
            const Handle(Geom_BSplineSurface) bspline = adaptor.BSpline();
            if (!bspline.IsNull()) {
                for (int i = 1; i <= bspline->NbUKnots(); ++i)
                    add_parameter(u_parameters, bspline->UKnot(i), u_min, u_max);
                for (int i = 1; i <= bspline->NbVKnots(); ++i)
                    add_parameter(v_parameters, bspline->VKnot(i), v_min, v_max);
            }
        }
        sort_unique_parameters(u_parameters, u_min, u_max);
        sort_unique_parameters(v_parameters, v_min, v_max);

        constexpr size_t max_parameters = 513;
        constexpr size_t max_points = 180000;
        constexpr int max_passes = 24;
        const double samples[] = {0.25, 0.5, 0.75};

        for (int pass = 0; pass < max_passes; ++pass) {
            std::vector<bool> split_u(u_parameters.size() - 1, false);
            std::vector<bool> split_v(v_parameters.size() - 1, false);
            const std::vector<double> u_samples = orthogonal_samples(u_parameters);
            const std::vector<double> v_samples = orthogonal_samples(v_parameters);

            for (size_t i = 0; i + 1 < u_parameters.size(); ++i) {
                const double first_u = u_parameters[i];
                const double last_u = u_parameters[i + 1];
                for (double v : v_samples) {
                    CPoint8d first;
                    CPoint8d last;
                    if (!mm->GetPoint(first_u, v, &first)
                        || !mm->GetPoint(last_u, v, &last)) {
                        return 3;
                    }
                    for (double alpha : samples) {
                        CPoint8d point;
                        const double u = first_u + (last_u - first_u) * alpha;
                        if (!mm->GetPoint(u, v, &point))
                            return 3;
                        if (point_segment_distance(point, first, last) > delta) {
                            split_u[i] = true;
                            break;
                        }
                    }
                    if (split_u[i])
                        break;
                }
            }

            for (size_t j = 0; j + 1 < v_parameters.size(); ++j) {
                const double first_v = v_parameters[j];
                const double last_v = v_parameters[j + 1];
                for (double u : u_samples) {
                    CPoint8d first;
                    CPoint8d last;
                    if (!mm->GetPoint(u, first_v, &first)
                        || !mm->GetPoint(u, last_v, &last)) {
                        return 3;
                    }
                    for (double alpha : samples) {
                        CPoint8d point;
                        const double v = first_v + (last_v - first_v) * alpha;
                        if (!mm->GetPoint(u, v, &point))
                            return 3;
                        if (point_segment_distance(point, first, last) > delta) {
                            split_v[j] = true;
                            break;
                        }
                    }
                    if (split_v[j])
                        break;
                }
            }

            for (size_t j = 0; j + 1 < v_parameters.size(); ++j) {
                for (size_t i = 0; i + 1 < u_parameters.size(); ++i) {
                    CPoint8d p00;
                    CPoint8d p10;
                    CPoint8d p01;
                    CPoint8d p11;
                    if (!mm->GetPoint(u_parameters[i], v_parameters[j], &p00)
                        || !mm->GetPoint(u_parameters[i + 1], v_parameters[j], &p10)
                        || !mm->GetPoint(u_parameters[i], v_parameters[j + 1], &p01)
                        || !mm->GetPoint(u_parameters[i + 1], v_parameters[j + 1], &p11)) {
                        return 3;
                    }
                    if (diagonal_line_distance(p00, p11, p01, p10) > delta) {
                        split_u[i] = true;
                        split_v[j] = true;
                    }
                }
            }

            const size_t add_u = static_cast<size_t>(
                std::count(split_u.begin(), split_u.end(), true));
            const size_t add_v = static_cast<size_t>(
                std::count(split_v.begin(), split_v.end(), true));
            if (add_u == 0 && add_v == 0)
                break;
            if (u_parameters.size() + add_u > max_parameters
                || v_parameters.size() + add_v > max_parameters
                || (u_parameters.size() + add_u) * (v_parameters.size() + add_v) > max_points) {
                break;
            }

            std::vector<double> refined_u;
            refined_u.reserve(u_parameters.size() + add_u);
            for (size_t i = 0; i + 1 < u_parameters.size(); ++i) {
                refined_u.push_back(u_parameters[i]);
                if (split_u[i])
                    refined_u.push_back((u_parameters[i] + u_parameters[i + 1]) * 0.5);
            }
            refined_u.push_back(u_parameters.back());

            std::vector<double> refined_v;
            refined_v.reserve(v_parameters.size() + add_v);
            for (size_t j = 0; j + 1 < v_parameters.size(); ++j) {
                refined_v.push_back(v_parameters[j]);
                if (split_v[j])
                    refined_v.push_back((v_parameters[j] + v_parameters[j + 1]) * 0.5);
            }
            refined_v.push_back(v_parameters.back());
            u_parameters = std::move(refined_u);
            v_parameters = std::move(refined_v);
        }

        qty_s_ = static_cast<int>(u_parameters.size());
        qty_t_ = static_cast<int>(v_parameters.size());
        points_.resize(static_cast<size_t>(qty_s_ * qty_t_));
        for (int t = 0; t < qty_t_; ++t) {
            for (int s = 0; s < qty_s_; ++s) {
                if (!mm->GetPoint(u_parameters[static_cast<size_t>(s)],
                                  v_parameters[static_cast<size_t>(t)], P(s, t))) {
                    points_.clear();
                    qty_s_ = 0;
                    qty_t_ = 0;
                    return 4;
                }
            }
        }

        mm->Umin = u_min;
        mm->Umax = u_max;
        mm->Vmin = v_min;
        mm->Vmax = v_max;
        mm->m_QtyU = qty_s_;
        mm->m_QtyV = qty_t_;
        return 0;
    } catch (const Standard_Failure&) {
        points_.clear();
        qty_s_ = 0;
        qty_t_ = 0;
        return 5;
    }
}

int CNet::BuildNetByTwoQty(CSurfaceFace* mm, int QtyS, int QtyT)
{
    points_.clear();
    qty_s_ = 0;
    qty_t_ = 0;

    if (!mm || mm->m_Face.IsNull())
        return 1;

    QtyS = std::max(QtyS, 2);
    QtyT = std::max(QtyT, 2);

    try {
        const TopoDS_Face face = TopoDS::Face(mm->m_Face);
        TopLoc_Location location;
        Handle(Geom_Surface) surface = BRep_Tool::Surface(face, location);
        if (surface.IsNull())
            return 2;

        Standard_Real u_min = 0.0;
        Standard_Real u_max = 0.0;
        Standard_Real v_min = 0.0;
        Standard_Real v_max = 0.0;
        BRepTools::UVBounds(face, u_min, u_max, v_min, v_max);
       if (!std::isfinite(u_min) || !std::isfinite(u_max)
            || !std::isfinite(v_min) || !std::isfinite(v_max)
            || u_max <= u_min || v_max <= v_min) {
            return 3;
        } 

        points_.resize(static_cast<size_t>(QtyS * QtyT));
        qty_s_ = QtyS;
        qty_t_ = QtyT;

 /*       for (int t = 0; t < QtyT; ++t) {
            const double v_alpha = QtyT == 1 ? 0.0 : static_cast<double>(t) / static_cast<double>(QtyT - 1);
            const double v = v_min + (v_max - v_min) * v_alpha;
            for (int s = 0; s < QtyS; ++s) {
                const double u_alpha = QtyS == 1 ? 0.0 : static_cast<double>(s) / static_cast<double>(QtyS - 1);
                const double u = u_min + (u_max - u_min) * u_alpha;
                if (!mm->GetPoint(u, v, P(s, t))) {
                    points_.clear();
                    qty_s_ = 0;
                    qty_t_ = 0;
                    return 4;
                }
            }
        }
 */
        int np = (int)QtyS;
        int nl = (int)QtyT;
        double ds = (u_max -u_min) / (np - 1);
        double dt = (v_max - v_min) / (nl - 1);
        for (int j = 0; j < nl; j++)
            for (int i = 0; i < np; i++) {
                double U = u_min + ds * i;
                double V = v_min + dt * j;
                if (!mm->GetPoint(U, V, P(i, j))) {
                    points_.clear();
                    qty_s_ = 0;
                    qty_t_ = 0;
                    char buf[120];
                    sprintf(buf, " UVBounds QtyS=%d, QtyT=%d  u_min=%4.1f u_max=%4.1f v_min=%4.1f v_max=%4.1f   ", QtyS, QtyT, u_min, u_max, v_min, v_max);
                    Step(buf);
                    sprintf(buf, "Error GetPoint  U=%4.1f V=%4.1f   ", U, V);
                    Step(buf);
                    return 4;
                }

            }

        mm->Umin = u_min;
        mm->Vmin = v_min;
        mm->Umax = u_max;
        mm->Vmax = v_max;
    } catch (const Standard_Failure&) {
        points_.clear();
        qty_s_ = 0;
        qty_t_ = 0;
        return 5;
    }

    mm->m_QtyU = qty_s_;
    mm->m_QtyV = qty_t_;
    return 0;
}

bool CNet::BuildMesh3D(CMesh3D* m)
{
    if (!m || points_.empty() || qty_s_ < 2 || qty_t_ < 2)
        return false;

    std::vector<Vec3> vertices;
    std::vector<UV> uvs;
    std::vector<Vec3> normals;
    vertices.reserve(points_.size());
    uvs.reserve(points_.size());
    normals.reserve(points_.size());
    for (const CPoint8d& point : points_) {
        vertices.push_back({
            static_cast<float>(point.x),
            static_cast<float>(point.y),
            static_cast<float>(point.z)
        });
        uvs.push_back({
            static_cast<float>(point.s),
            static_cast<float>(point.t)
        });
        normals.push_back(normalize({
            static_cast<float>(point.l),
            static_cast<float>(point.m),
            static_cast<float>(point.n)
        }));
    }

    std::vector<CMesh3D::Face> faces;
    faces.reserve(static_cast<size_t>((qty_s_ - 1) * (qty_t_ - 1)));
    for (int t = 0; t + 1 < qty_t_; ++t) {
        for (int s = 0; s + 1 < qty_s_; ++s) {
            faces.push_back({
                Index(s, t),
                Index(s + 1, t),
                Index(s + 1, t + 1),
                Index(s, t + 1)
            });
        }
    }

    return m->SetGeometry(std::move(vertices),
                          std::move(faces),
                          std::move(uvs),
                          std::move(normals));
}

void CNet::ReversPoints()
{
    // GetPoint() already returns a normal adjusted for TopoDS_Face orientation.
    // Reverse only the grid winding here; flipping normals again would make
    // reversed OCCT faces light as if their normals pointed into the solid.
    CPoint8d ptm;
    for (int k = 0; k < qty_t_; k++) {
        for (int i = 0, j = qty_s_ - 1; i < j; i++, j--) {
            ptm = *P(i, k);
            *P(i, k) = *P(j, k);
            *P(j, k) = ptm;
        }
    }
}
