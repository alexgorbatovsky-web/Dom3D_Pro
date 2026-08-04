#include "CView3d.h"

#include "OpenGLCompat.h"
#include "SmartLine.h"

#include <cmath>
#include <cstddef>
//#include "GLU.h"

namespace {
void set_color(float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
}
}

void CView3d::Draw(const CAlfaDoc& document, bool xy_plane_grid, bool show_grid) const {
    if (show_grid) {
        DrawGrid(xy_plane_grid);
    }
    DrawObjects(document);
}

void CView3d::DrawGrid(bool xy_plane_grid) const {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    const float first = xy_plane_grid ? 0.0f : -kDefaultGridHalfSize;
    const float last = xy_plane_grid ? kDefaultSceneSize : kDefaultGridHalfSize;
    const int line_count = static_cast<int>(
        std::round((last - first) / kDefaultGridStep));
    for (int line_index = 0; line_index <= line_count; ++line_index) {
        const float coordinate = first + static_cast<float>(line_index) * kDefaultGridStep;
        const bool major = line_index % 4 == 0;
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

void CView3d::DrawObjects(const CAlfaDoc& document) const {
    const auto& objects = document.GetObjects();
    const auto is_curve_overlay = [](const CAlfaObject& object) {
        return dynamic_cast<const CPolyline*>(&object)
            || dynamic_cast<const CBSpline*>(&object)
            || dynamic_cast<const CSmartLine*>(&object);
    };
    const auto draw_pass = [&](bool overlay) {
        for (size_t index = 0; index < objects.size(); ++index) {
            if (!objects[index]
                || !document.IsObjectVisible(*objects[index])
                || is_curve_overlay(*objects[index]) != overlay) {
                continue;
            }
            const bool selected = document.IsObjectSelectionHighlighted(index);
            const bool has_selected_point = document.HasSelection()
                && document.GetSelectedObjectIndex() == index
                && document.HasSelectedPoint();
            objects[index]->Render3d(
                selected, has_selected_point, document.GetSelectedPointIndex());
        }
    };

    draw_pass(false);
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
