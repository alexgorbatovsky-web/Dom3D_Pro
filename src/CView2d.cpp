#include "CView2d.h"

#include "OpenGLCompat.h"

#include <algorithm>

namespace {
void draw_rect(float x, float y, float width, float height, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void draw_line_rect(float x, float y, float width, float height, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}
}

void CView2d::DrawPreview(const CAlfaDoc& document, float x, float y, float width, float height) const {
    DrawFrame(x, y, width, height);

    const float scale = std::min(width / 10.0f, height / 8.0f);
    const float center_x = x + width * 0.5f;
    const float center_y = y + height * 0.5f;

    glLineWidth(1.0f);
    glColor3f(0.22f, 0.28f, 0.35f);
    glBegin(GL_LINES);
    for (int i = -5; i <= 5; ++i) {
        glVertex2f(center_x + i * scale, y + 10.0f);
        glVertex2f(center_x + i * scale, y + height - 10.0f);
        glVertex2f(x + 10.0f, center_y + i * scale);
        glVertex2f(x + width - 10.0f, center_y + i * scale);
    }
    glEnd();

    for (const auto& object : document.GetObjects()) {
        if (!object || !object->IsVisible()) {
            continue;
        }
        object->Render2d(center_x, center_y, scale);
    }
}

void CView2d::DrawFrame(float x, float y, float width, float height) const {
    draw_rect(x, y, width, height, 0.07f, 0.085f, 0.105f);
    draw_line_rect(x, y, width, height, 0.25f, 0.32f, 0.40f);
}
