#pragma once

#include "Common.h"
#include "Material.h"

#include <iosfwd>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>


struct ParametricParameterValue {
    std::string id;
    double value = 0.0;
};

class CAlfaObject {
public:
    virtual ~CAlfaObject() = default;

    // Shared high-contrast color for selected object geometry.  It follows
    // the viewport background so selection remains visible in light and dark
    // themes.
    static Color SelectedColor;
    static void UpdateSelectedColorFromBackground(Color background);

    unsigned long m_col = 0;
    bool m_selected = false;
	unsigned long m_id = 0;
    int m_LayerID = 0;

    virtual void Render3d(bool selected) const = 0;
    virtual void Render3d(bool selected, bool has_selected_point, size_t selected_point_index) const;
    virtual void Render2d(float center_x, float center_y, float scale) const = 0;
    virtual bool HitTest(CurvePoint point, float tolerance) const = 0;
    virtual bool Save(std::ostream& stream) const = 0;
    virtual std::unique_ptr<CAlfaObject> Clone() const;
    virtual void Translate(Vec3 delta) = 0;
    virtual void Rotate(Vec3 center, Vec3 axis, float angle) = 0;
    virtual void Scale(Vec3 center, Vec3 axis, float factor) = 0;
    virtual void Mirror(Vec3 plane_point, Vec3 plane_normal);
    virtual bool GetBounds(Vec3& min_point, Vec3& max_point) const = 0;
    virtual void Edit(NativeWindowHandle parent_window);

    const std::string& GetName() const;
    void SetName(std::string name);
    const std::string& GetGroupName() const;
    void SetGroupName(std::string group_name);
    bool IsVisible() const;
    virtual void SetVisible(bool visible);

    Color GetColor() const;
    virtual void SetColor(Color color);
    double GetLineWidth() const;
    void SetLineWidth(double width);
    const std::string& GetLineStyle() const;
    void SetLineStyle(std::string style);
    void ApplyLineAppearance(bool selected,
                             float selected_width = 5.0f,
                             float default_width = 2.0f) const;
    void ResetLineAppearance() const;
    Material GetMaterial() const;
    void SetMaterial(Material material);
    unsigned long GetMaterialId() const;
    void SetMaterialId(unsigned long id);
    bool IsParametric() const;
    const std::string& GetParametricToolId() const;
    const std::vector<ParametricParameterValue>& GetParametricParameters() const;
    void SetParametricDefinition(std::string tool_id, std::vector<ParametricParameterValue> parameters);
    void ClearParametricDefinition();

protected:
    CAlfaObject();
    explicit CAlfaObject(std::string name);

private:
    std::string name_;
    std::string group_name_;
    Material material_;
    unsigned long material_id_ = 0;
    bool visible_ = true;
    double line_width_ = 0.5;
    std::string line_style_ = "CONTINUOUS";
    std::string parametric_tool_id_;
    std::vector<ParametricParameterValue> parametric_parameters_;
};

#define RGB_BLACK	0x00000000
#define RGB_BLUE	0x00FF0000
#define RGB_GREEN	0x0000FF00
#define RGB_CYAN	0x00FFFF00
#define RGB_RED		0x000000FF
#define RGB_MAGENTA	0x00FF00FF
#define RGB_BROWN	0x0000418B
#define RGB_LIGHTGRAY	0x00808080
#define RGB_DARKGRAY	0x00404040
#define RGB_LIGHTBLUE	0x00FFFF80
#define RGB_LIGHTGREEN	0x0080FF80
#define RGB_LIGHTCYAN	0x00FFFF00
#define RGB_LIGHTRED		0x008080FF
#define RGB_LIGHTMAGENTA	0x00FF00FF
#define RGB_YELLOW	0x0000FFFF
#define RGB_WHITE	0x00FFFFFF



#define RGB_KR		0x00FF4392
#define RGB_BSPLINE_CURVE	0x00B066E5
#define RGB_CURVE2P	0x00E19D8D
#define RGB_CURVE2PP	0x0092665b
#define RGB_SMART_LINE	0x00FF8000
#define RGB_SETKA	0x00FFBF00
#define RGB_POLIS	0x00AF8FB0	
#define RGB_TRACE	0x001FC94B
#define RGB_DRAW_POLIGON	0x00A08A6A	
#define RGB_MATMOD	0x007F7F7F
#define RGB_CIRCLE	0x00B7B763
#define RGB_INTERCURV	0x003274f0
#define RGB_TOOL	0x007F2F2F
#define RGB_ZAGOTOVKA	0x00D3BA7B
#define RGB_FRONTZ	0x007F7F2F
#define RGB_REARZ	0x007F7F7F
#define RGB_NORMAL	0x000FAFFF
#define RGB_POINT	0x0068CCCA
#define RGB_POINTN	0x008C488E
#define RGB_SCAN_POINT	0x0000FFFF
#define RGB_SLED_TOOL	0x002DC994
#define RGB_2DTOLL	0x00ef8888
#define RGB_LINEV	0x00d76e8c
#define RGB_LINEU	0x00ac82d7
#define RGB_LINE	0x00FF3E40
#define RGB_SOLID	0x00AF8FB0	

#define RGB_PICT	0x00004393
#define RGB_DIMS	0x005e5e00
#define RGB_BASE	0x0000FF00
#define RGB_RUBBERBAND	0x00F08F0E
#define RGB_LIN_MAL	0x00FF7C3A
#define RGB_TEXT	0x00B4B4B4

enum TypeObject {
    TYPE_EMPTY,
    TYPE_POINT,
    TYPE_VECTOR,
    TYPE_LINE,
    TYPE_CIRCLE,
    TYPE_SPLINE,
    TYPE_CONIC,
    TYPE_SKETCH,
    TYPE_EDGE,
    TYPE_BEZIER_SPLINE,
    TYPE_HATCHING,
    TYPE_TRACE,
    TYPE_SURFACE,
    TYPE_CUNC,
    TYPE_PLANE,
    TYPE_SETKA,
    TYPE_FACE,
    TYPE_SOLID,
    TYPE_LIGHT,
    TYPE_TOLL,
    TYPE_MSK,
    TYPE_ASK,
    TYPE_GRID,
    TYPE_23,
    TYPE_VIEW_DRAFT,
    TYPE_GROUP,
    TYPE_TEXT,
    TYPE_DIMENSION,
    TYPE_BITMAP,
    TYPE_TABLE,
    TYPE_KNOT,
    TYPE_OLE,
    TYPE_ASSEMBLY,
    MSK_AXIS_X,
    MSK_AXIS_Y,
    MSK_AXIS_Z,
    TYPE_VERTEX,
};
