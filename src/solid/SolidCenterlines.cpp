#include "Solid.h"
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Lin.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_QuasiUniformDeflection.hxx>
#include <TopoDS.hxx>
#include <gp_Ax3.hxx>
#include <gp_GTrsf.hxx>
#include <QDomElement>
#include <QXmlStreamWriter>
#include <algorithm>
#include <cmath>

bool CSolid::SetCenterline(SolidCenterline line) {
    if (line.id.empty() || line.points.size() < 2) return false;
    for (const auto& p : line.points)
        if (!std::isfinite(p.X()) || !std::isfinite(p.Y()) || !std::isfinite(p.Z())) return false;
    if (std::none_of(line.points.begin() + 1, line.points.end(),
                    [&](const gp_Pnt& p) { return p.SquareDistance(line.points.front()) > 1e-18; })) return false;
    const auto existing = std::find_if(m_Centerlines.begin(), m_Centerlines.end(),
        [&](const SolidCenterline& p) { return p.id == line.id; });
    if (existing == m_Centerlines.end()) m_Centerlines.push_back(std::move(line));
    else *existing = std::move(line);
    return true;
}

bool CSolid::SetAxis(std::string id, SolidCenterlineKind kind, Vec3 origin, Vec3 direction,
                    double minimum, double maximum) {
    if (m_Shape.IsNull() || dot(direction, direction) < 1e-18f) return false;
    try {
        const gp_Pnt o(origin.x, origin.y, origin.z);
        const gp_Dir d(direction.x, direction.y, direction.z);
        gp_Trsf local;
        local.SetTransformation(gp_Ax3(o, d));
        Bnd_Box box;
        BRepBndLib::AddOptimal(BRepBuilderAPI_Transform(m_Shape, local, false).Shape(), box, false, false);
        if (box.IsVoid() || box.IsOpen()) return false;
        double x0, y0, z0, x1, y1, z1;
        box.Get(x0, y0, z0, x1, y1, z1);
        z0 = std::max(z0, minimum); z1 = std::min(z1, maximum);
        if (z1 - z0 <= 1e-9) return false;
        return SetCenterline({std::move(id), kind,
            {o.Translated(gp_Vec(d) * z0), o.Translated(gp_Vec(d) * z1)}, false});
    } catch (const Standard_Failure&) { return false; }
}

bool CSolid::SetCenterlinePath(std::string id, const TopoDS_Shape& path) {
    if (path.IsNull() || path.ShapeType() != TopAbs_WIRE) return false;
    SolidCenterline line{std::move(id), SolidCenterlineKind::Path, {}, path.Closed()};
    try {
        for (BRepTools_WireExplorer e(TopoDS::Wire(path)); e.More(); e.Next()) {
            BRepAdaptor_Curve curve(e.Current());
            GCPnts_QuasiUniformDeflection sample(curve, 0.01);
            if (!sample.IsDone() || sample.NbPoints() < 2) return false;
            const bool reversed = e.Current().Orientation() == TopAbs_REVERSED;
            for (int i = 1; i <= sample.NbPoints(); ++i) {
                const gp_Pnt p = sample.Value(reversed ? sample.NbPoints() + 1 - i : i);
                if (line.points.empty() || line.points.back().SquareDistance(p) > 1e-18)
                    line.points.push_back(p);
            }
        }
        return SetCenterline(std::move(line));
    } catch (const Standard_Failure&) { return false; }
}

void CSolid::AppendCenterlinesFrom(const CSolid& source, const std::string& prefix, bool holes) {
    for (auto line : source.GetCenterlines()) {
        line.id = prefix + line.id;
        if (holes && line.kind == SolidCenterlineKind::RotationAxis) line.kind = SolidCenterlineKind::HoleAxis;
        if (holes && line.kind == SolidCenterlineKind::HoleAxis && line.points.size() == 2) {
            const auto& p = line.points.front(); const gp_Vec d(p,line.points.back());
            if (SetAxis(line.id,line.kind,{float(p.X()),float(p.Y()),float(p.Z())},
                        {float(d.X()),float(d.Y()),float(d.Z())},0,d.Magnitude())) continue;
        }
        SetCenterline(std::move(line));
    }
}
void CSolid::TransformCenterlines(const gp_Trsf& transform) {
    m_PreviewCenterlines.clear();
    for (auto& line : m_Centerlines) for (auto& p : line.points) p.Transform(transform);
}
void CSolid::TransformCenterlines(const gp_GTrsf& transform) {
    m_PreviewCenterlines.clear();
    for (auto& line : m_Centerlines) for (auto& p : line.points) {
        auto xyz = p.XYZ(); transform.Transforms(xyz); p.SetXYZ(xyz);
    }
}
void CSolid::PreviewCenterlines(const gp_GTrsf& transform) {
    if (m_PreviewCenterlines.empty()) m_PreviewCenterlines = m_Centerlines;
    for (auto& line : m_PreviewCenterlines) for (auto& p : line.points) {
        auto xyz = p.XYZ(); transform.Transforms(xyz); p.SetXYZ(xyz);
    }
}
void CSolid::WriteCenterlines(QXmlStreamWriter& xml) const {
    xml.writeStartElement("centerlines");
    xml.writeAttribute("version", "1");
    for (const auto& line : m_Centerlines) {
        xml.writeStartElement("line"); xml.writeAttribute("id", QString::fromStdString(line.id));
        xml.writeAttribute("kind", line.kind == SolidCenterlineKind::RotationAxis ? "rotation"
            : line.kind == SolidCenterlineKind::HoleAxis ? "hole" : "path");
        xml.writeAttribute("closed", line.closed ? "true" : "false");
        for (const auto& p : line.points) {
            xml.writeEmptyElement("point");
            xml.writeAttribute("x", QString::number(p.X(), 'g', 17));
            xml.writeAttribute("y", QString::number(p.Y(), 'g', 17));
            xml.writeAttribute("z", QString::number(p.Z(), 'g', 17));
        }
        xml.writeEndElement();
    }
    xml.writeEndElement();
}
bool CSolid::ReadCenterlines(const QDomElement& object, QString& error) {
    const auto root = object.firstChildElement("centerlines");
    if (root.isNull()) return true; // Older projects have no stored axes.
    std::vector<SolidCenterline> parsed;
    for (auto e = root.firstChildElement("line"); !e.isNull(); e = e.nextSiblingElement("line")) {
        SolidCenterline line; line.id = e.attribute("id").toStdString();
        const auto kind = e.attribute("kind");
        if (kind != "rotation" && kind != "hole" && kind != "path") { error = "Invalid solid centerline kind."; return false; }
        line.kind = kind == "rotation" ? SolidCenterlineKind::RotationAxis
            : kind == "hole" ? SolidCenterlineKind::HoleAxis : SolidCenterlineKind::Path;
        line.closed = e.attribute("closed") == "true";
        for (auto p = e.firstChildElement("point"); !p.isNull(); p = p.nextSiblingElement("point")) {
            bool a, b, c;
            const double x = p.attribute("x").toDouble(&a), y = p.attribute("y").toDouble(&b), z = p.attribute("z").toDouble(&c);
            if (!a || !b || !c || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) { error = "Invalid solid centerline point."; return false; }
            line.points.emplace_back(x, y, z);
        }
        if (line.id.empty() || line.points.size() < 2) { error = "Incomplete solid centerline."; return false; }
        parsed.push_back(std::move(line));
    }
    m_Centerlines = std::move(parsed);
    // Hole history may retain a picked face position from before an extrusion
    // edit. Use the actual coaxial cylindrical wall to recover its axial span.
    if (!m_Shape.IsNull()) for (auto& line : m_Centerlines) {
        if (line.kind != SolidCenterlineKind::HoleAxis || line.points.size() != 2
            || line.points[0].Distance(line.points[1]) < 1e-9) continue;
        try {
            const gp_Pnt origin = line.points[0];
            const gp_Dir direction(gp_Vec(origin, line.points[1]));
            const gp_Lin axis(origin, direction);
            gp_Trsf local; local.SetTransformation(gp_Ax3(origin, direction));
            double low = std::numeric_limits<double>::infinity(), high = -low;
            for (TopExp_Explorer faces(m_Shape, TopAbs_FACE); faces.More(); faces.Next()) {
                BRepAdaptor_Surface surface(TopoDS::Face(faces.Current()));
                if (surface.GetType() != GeomAbs_Cylinder) continue;
                const auto cylinder = surface.Cylinder();
                if (!direction.IsParallel(cylinder.Axis().Direction(), 1e-7)
                    || axis.Distance(cylinder.Location()) > 1e-4) continue;
                Bnd_Box bounds;
                BRepBndLib::AddOptimal(BRepBuilderAPI_Transform(faces.Current(),local,false).Shape(),bounds,false,false);
                if (bounds.IsVoid() || bounds.IsOpen()) continue;
                double x0,y0,z0,x1,y1,z1; bounds.Get(x0,y0,z0,x1,y1,z1);
                low = std::min(low,z0); high = std::max(high,z1);
            }
            if (high > low) line.points = {origin.Translated(gp_Vec(direction)*low), origin.Translated(gp_Vec(direction)*high)};
        } catch (const Standard_Failure&) { /* Retain valid stored metadata. */ }
    }
    return true;
}
