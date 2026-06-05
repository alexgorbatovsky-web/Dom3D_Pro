#include "CView3d.h"

#include "OpenGLCompat.h"

#include <cstddef>
//#include "GLU.h"

namespace {
void set_color(float r, float g, float b, float a = 1.0f) {
    glColor4f(r, g, b, a);
}
}

void CView3d::Draw(const CAlfaDoc& document) const {
    DrawGrid();
    DrawObjects(document);
}

void CView3d::DrawGrid() const {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int i = -12; i <= 12; ++i) {
        const bool major = i % 4 == 0;
        const float r = major ? 0.34f : 0.22f;
        const float g = major ? 0.40f : 0.27f;
        const float b = major ? 0.48f : 0.34f;
        const float a = major ? 0.58f : 0.34f;
        set_color(r, g, b, a);
        glVertex3f(static_cast<float>(i), 0.0f, -12.0f);
        glVertex3f(static_cast<float>(i), 0.0f, 12.0f);
        glVertex3f(-12.0f, 0.0f, static_cast<float>(i));
        glVertex3f(12.0f, 0.0f, static_cast<float>(i));
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
    for (size_t index = 0; index < objects.size(); ++index) {
        if (!objects[index] || !objects[index]->IsVisible()) {
            continue;
        }
        const bool selected = document.IsObjectSelected(index);
        const bool has_selected_point = document.HasSelection() && document.GetSelectedObjectIndex() == index && document.HasSelectedPoint();
        objects[index]->Render3d(selected, has_selected_point, document.GetSelectedPointIndex());
    }
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
