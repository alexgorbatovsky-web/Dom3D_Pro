#include "CView3d.h"

#include "OpenGLCompat.h"
#include "SmartLine.h"
#include "CGroup.h"
#include "ReferenceImage.h"
#include "solid/Solid.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
//#include "GLU.h"

namespace {
void set_color(float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
}
}

void CView3d::Draw(const CAlfaDoc& document,
                   Vec3 camera_eye,
                   Vec3 camera_forward,
                   bool xy_plane_grid,
                   bool show_grid,
                   float grid_size,
                   float grid_step,
                   int grid_subdivisions) const {
    if (show_grid) {
        DrawGrid(xy_plane_grid, grid_size, grid_step, grid_subdivisions);
    }
    DrawObjects(document, camera_eye, camera_forward);
}

void CView3d::DrawGrid(bool xy_plane_grid,
                       float grid_size,
                       float grid_step,
                       int grid_subdivisions) const {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    const float safe_grid_size = std::max(1.0f, grid_size);
    const float grid_half_size = safe_grid_size * 0.5f;
    const float first = xy_plane_grid ? 0.0f : -grid_half_size;
    const float last = xy_plane_grid ? safe_grid_size : grid_half_size;
    const int safe_subdivisions = std::clamp(grid_subdivisions, 1, 100);
    const float minor_step = std::max(
        0.000001f, grid_step / static_cast<float>(safe_subdivisions));
    const int line_count = static_cast<int>(
        std::round((last - first) / minor_step));
    for (int line_index = 0; line_index <= line_count; ++line_index) {
        const float coordinate = first + static_cast<float>(line_index) * minor_step;
        const bool major = line_index % safe_subdivisions == 0;
        const float r = major ? 0.34f : 0.22f;
        const float g = major ? 0.40f : 0.27f;
        const float b = major ? 0.48f : 0.34f;
        const float a = major ? 0.58f : 0.34f;
        set_color(r, g, b, a);
        glVertex3f(coordinate, first, 0.0f);
        glVertex3f(coordinate, last, 0.0f);
        glVertex3f(first, coordinate, 0.0f);
        glVertex3f(last, coordinate, 0.0f);
    }
    glEnd();
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_BLEND);
}

void CView3d::DrawRoom() const {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBegin(GL_QUADS);
    set_color(0.72f, 0.75f, 0.78f);
    glVertex3f(-4.0f, 0.02f, -3.0f);
    glVertex3f(4.0f, 0.02f, -3.0f);
    glVertex3f(4.0f, 0.02f, 3.0f);
    glVertex3f(-4.0f, 0.02f, 3.0f);

    set_color(0.52f, 0.60f, 0.68f);
    glVertex3f(-4.0f, 0.0f, -3.0f);
    glVertex3f(-4.0f, 2.8f, -3.0f);
    glVertex3f(4.0f, 2.8f, -3.0f);
    glVertex3f(4.0f, 0.0f, -3.0f);

    set_color(0.45f, 0.54f, 0.62f);
    glVertex3f(-4.0f, 0.0f, 3.0f);
    glVertex3f(-4.0f, 2.8f, 3.0f);
    glVertex3f(-4.0f, 2.8f, -3.0f);
    glVertex3f(-4.0f, 0.0f, -3.0f);
    glEnd();

    DrawBox(-2.8f, 0.0f, -2.4f, 1.6f, 0.9f, 0.7f, 0.70f, 0.47f, 0.28f);
    DrawBox(1.4f, 0.0f, -2.35f, 1.7f, 1.2f, 0.65f, 0.30f, 0.43f, 0.55f);
    DrawBox(-0.6f, 0.0f, 0.25f, 1.7f, 0.8f, 1.1f, 0.38f, 0.56f, 0.43f);

    glLineWidth(2.0f);
    set_color(0.95f, 0.96f, 0.97f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-4.0f, 0.0f, -3.0f);
    glVertex3f(4.0f, 0.0f, -3.0f);
    glVertex3f(4.0f, 0.0f, 3.0f);
    glVertex3f(-4.0f, 0.0f, 3.0f);
    glEnd();
}

void CView3d::DrawObjects(const CAlfaDoc& document,
                          Vec3 camera_eye,
                          Vec3 camera_forward) const {
    const auto& objects = document.GetObjects();
    const auto is_curve_overlay = [](const CAlfaObject& object) {
        return dynamic_cast<const CPolyline*>(&object)
            || dynamic_cast<const CBSpline*>(&object)
            || dynamic_cast<const CSmartLine*>(&object);
    };
    bool has_zebra_selection = false;
    if (CMesh3D::IsZebraAnalysisEnabled()) {
        for (size_t index : document.GetSelectedObjectIndices()) {
            if (index >= objects.size() || !objects[index]) {
                continue;
            }
            const CAlfaObject* object = objects[index].get();
            if (dynamic_cast<const CSolid*>(object)
                || (dynamic_cast<const CMesh3D*>(object)
                    && !dynamic_cast<const CReferenceImage*>(object))
                || dynamic_cast<const CGroup*>(object)) {
                has_zebra_selection = true;
                break;
            }
        }
    }
    const auto is_transparent = [](const CAlfaObject& object) {
        if (object.GetMaterial().alpha < 0.999f) {
            return true;
        }
        return dynamic_cast<const CSolid*>(&object)
            && CSolid::IsSurfaceTransparencyEnabled();
    };
    const auto object_depth = [&](size_t index) {
        Vec3 min_point{};
        Vec3 max_point{};
        if (!objects[index]->GetBounds(min_point, max_point)) {
            return -std::numeric_limits<float>::max();
        }
        const Vec3 center = (min_point + max_point) * 0.5f;
        return dot(center - camera_eye, camera_forward);
    };
    const auto render_object = [&](size_t index) {
        const bool selected = document.IsObjectSelectionHighlighted(index);
        const bool has_selected_point = document.HasSelection()
            && document.GetSelectedObjectIndex() == index
            && document.HasSelectedPoint();
        CMesh3D::SetZebraAnalysisTarget(
            !has_zebra_selection || selected);
        objects[index]->Render3d(
            selected, has_selected_point, document.GetSelectedPointIndex());
    };
    const auto draw_pass = [&](bool overlay) {
        std::vector<size_t> transparent_indices;
        for (size_t index = 0; index < objects.size(); ++index) {
            if (!objects[index]
                || !document.IsObjectVisible(*objects[index])
                || is_curve_overlay(*objects[index]) != overlay) {
                continue;
            }
            if (is_transparent(*objects[index])) {
                transparent_indices.push_back(index);
            } else {
                render_object(index);
            }
        }

        // Alpha blending is order dependent.  Opaque geometry has already
        // populated the depth buffer; now composite transparent parts from
        // back to front while CMesh3D keeps depth testing enabled and depth
        // writes disabled for their faces.
        std::stable_sort(
            transparent_indices.begin(), transparent_indices.end(),
            [&](size_t lhs, size_t rhs) {
                return object_depth(lhs) > object_depth(rhs);
            });
        for (size_t index : transparent_indices) {
            render_object(index);
        }
        CMesh3D::SetZebraAnalysisTarget(false);
    };

    if (!CMesh3D::IsZebraAnalysisEnabled()
        && CSolid::GetDisplayMode() == SolidDisplayMode::HiddenLineHatch) {
        // Hidden-line rendering must be scene-wide.  Filling and drawing one
        // solid at a time cannot hide an edge of an earlier solid behind a
        // later one (for example a table leg behind its top).
        for (size_t index = 0; index < objects.size(); ++index) {
            if (!objects[index]
                || !document.IsObjectVisible(*objects[index])
                || is_curve_overlay(*objects[index])) {
                continue;
            }
            if (const auto* solid = dynamic_cast<const CSolid*>(objects[index].get())) {
                solid->RenderHiddenLineDepth();
            } else if (const auto* mesh =
                           dynamic_cast<const CMesh3D*>(objects[index].get());
                       mesh && !dynamic_cast<const CReferenceImage*>(mesh)) {
                mesh->RenderHiddenLineDepth(CSolid::GetHiddenLineBackgroundColor());
            }
        }
        for (const auto& object : objects) {
            if (object && document.IsObjectVisible(*object)) {
                if (const auto* solid = dynamic_cast<const CSolid*>(object.get())) {
                    solid->RenderHiddenLineEdges(true);
                } else if (const auto* mesh =
                               dynamic_cast<const CMesh3D*>(object.get());
                           mesh && !dynamic_cast<const CReferenceImage*>(mesh)) {
                    mesh->RenderHiddenLineEdges(
                        true, CSolid::GetHiddenLineBackgroundColor());
                }
            }
        }
        for (const auto& object : objects) {
            if (object && document.IsObjectVisible(*object)) {
                if (const auto* solid = dynamic_cast<const CSolid*>(object.get())) {
                    solid->RenderHiddenLineEdges(false);
                } else if (const auto* mesh =
                               dynamic_cast<const CMesh3D*>(object.get());
                           mesh && !dynamic_cast<const CReferenceImage*>(mesh)) {
                    mesh->RenderHiddenLineEdges(
                        false, CSolid::GetHiddenLineBackgroundColor());
                }
            }
        }

        // Draw non-solid scene helpers once, after the geometry passes.
        for (size_t index = 0; index < objects.size(); ++index) {
            if (!objects[index]
                || !document.IsObjectVisible(*objects[index])
                || is_curve_overlay(*objects[index])
                || dynamic_cast<const CSolid*>(objects[index].get())
                || (dynamic_cast<const CMesh3D*>(objects[index].get())
                    && !dynamic_cast<const CReferenceImage*>(objects[index].get()))
                || dynamic_cast<const CGroup*>(objects[index].get())) {
                continue;
            }
            const bool selected = document.IsObjectSelectionHighlighted(index);
            const bool has_selected_point = document.HasSelection()
                && document.GetSelectedObjectIndex() == index
                && document.HasSelectedPoint();
            objects[index]->Render3d(
                selected, has_selected_point, document.GetSelectedPointIndex());
        }
    } else {
        draw_pass(false);
    }
    draw_pass(true);
}

void CView3d::DrawBox(float x, float y, float z, float w, float h, float d, float r, float g, float b) const {
    const float x2 = x + w;
    const float y2 = y + h;
    const float z2 = z + d;

    glBegin(GL_QUADS);
    set_color(r, g, b);
    glVertex3f(x, y, z);
    glVertex3f(x2, y, z);
    glVertex3f(x2, y2, z);
    glVertex3f(x, y2, z);

    set_color(r * 0.85f, g * 0.85f, b * 0.85f);
    glVertex3f(x2, y, z);
    glVertex3f(x2, y, z2);
    glVertex3f(x2, y2, z2);
    glVertex3f(x2, y2, z);

    set_color(r * 0.75f, g * 0.75f, b * 0.75f);
    glVertex3f(x, y, z2);
    glVertex3f(x, y, z);
    glVertex3f(x, y2, z);
    glVertex3f(x, y2, z2);

    set_color(r * 0.92f, g * 0.92f, b * 0.92f);
    glVertex3f(x, y2, z);
    glVertex3f(x2, y2, z);
    glVertex3f(x2, y2, z2);
    glVertex3f(x, y2, z2);
    glEnd();
}

void CView3d::Project(CPoint3d* wp, CPoint3d* win)
{

//    gluProject(wp->x, wp->y, wp->z, m_mvmatrix, m_projmatrix, m_viewport, &win->x, &win->y, &win->z);
//    win->y = Size_Y - win->y;
}

void Set_Color(unsigned long col)
{
    BYTE r = BYTE(col & 0x000000FF);
    col = col >> 8;
    BYTE g = BYTE(col & 0x000000FF);
    col = col >> 8;
    BYTE b = BYTE(col & 0x000000FF);

    glColor3ub(r, g, b);
}
