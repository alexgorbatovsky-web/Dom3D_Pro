#include "RenderLightEditorDialog.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

struct RenderLightEditorDialog::Controls {
    QCheckBox* enabled = nullptr;
    QComboBox* type = nullptr;
    QDoubleSpinBox* px = nullptr;
    QDoubleSpinBox* py = nullptr;
    QDoubleSpinBox* pz = nullptr;
    QDoubleSpinBox* dx = nullptr;
    QDoubleSpinBox* dy = nullptr;
    QDoubleSpinBox* dz = nullptr;
    QPushButton* ambient = nullptr;
    QPushButton* diffuse = nullptr;
    QPushButton* specular = nullptr;
    QDoubleSpinBox* hotspot = nullptr;
    QDoubleSpinBox* falloff = nullptr;
    QDoubleSpinBox* exponent = nullptr;
    QCheckBox* attenuation = nullptr;
    QDoubleSpinBox* range = nullptr;
    QDoubleSpinBox* constant = nullptr;
    QDoubleSpinBox* linear = nullptr;
    QDoubleSpinBox* quadratic = nullptr;
    QDoubleSpinBox* size = nullptr;
    QDoubleSpinBox* shadow = nullptr;
    QCheckBox* casts_shadows = nullptr;
    Color ambient_color{};
    Color diffuse_color{};
    Color specular_color{};
};

namespace {
QDoubleSpinBox* spin(QWidget* parent, double minimum, double maximum,
                     int decimals = 3, double step = 1.0) {
    auto* result = new QDoubleSpinBox(parent);
    result->setRange(minimum, maximum);
    result->setDecimals(decimals);
    result->setSingleStep(step);
    return result;
}

QWidget* vector_row(QWidget* parent, QDoubleSpinBox*& x,
                    QDoubleSpinBox*& y, QDoubleSpinBox*& z,
                    double minimum, double maximum, int decimals,
                    double step) {
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    x = spin(row, minimum, maximum, decimals, step);
    y = spin(row, minimum, maximum, decimals, step);
    z = spin(row, minimum, maximum, decimals, step);
    x->setPrefix("X "); y->setPrefix("Y "); z->setPrefix("Z ");
    layout->addWidget(x); layout->addWidget(y); layout->addWidget(z);
    return row;
}

void show_color(QPushButton* button, Color color) {
    button->setText(QString("R %1  G %2  B %3")
        .arg(color.r, 0, 'f', 2).arg(color.g, 0, 'f', 2)
        .arg(color.b, 0, 'f', 2));
    button->setStyleSheet(QString(
        "QPushButton { background: rgb(%1,%2,%3); color: %4; }")
        .arg(qRound(std::clamp(color.r, 0.0f, 1.0f) * 255.0f))
        .arg(qRound(std::clamp(color.g, 0.0f, 1.0f) * 255.0f))
        .arg(qRound(std::clamp(color.b, 0.0f, 1.0f) * 255.0f))
        .arg(color.r + color.g + color.b > 1.5f ? "black" : "white"));
}

void choose_color(QWidget* parent, QPushButton* button, Color& color) {
    const QColor chosen = QColorDialog::getColor(
        QColor::fromRgbF(color.r, color.g, color.b), parent, "Light Color");
    if (!chosen.isValid()) return;
    color = {static_cast<float>(chosen.redF()),
             static_cast<float>(chosen.greenF()),
             static_cast<float>(chosen.blueF())};
    show_color(button, color);
}

QString key(int index, const char* field) {
    return QString("render/customLights/%1/%2").arg(index).arg(field);
}
}

RenderLightEditorDialog::RenderLightEditorDialog(
    std::vector<RenderLight> lights, QWidget* parent) : QDialog(parent) {
    setWindowTitle("Light Sources — Customize");
    resize(650, 700);
    if (lights.size() < 3) lights.resize(3);
    auto* root = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    root->addWidget(tabs, 1);
    for (int index = 0; index < 3; ++index) {
        const RenderLight& light = lights[static_cast<size_t>(index)];
        auto* controls = new Controls;
        controls_.push_back(controls);
        auto* page = new QWidget(tabs);
        auto* form = new QFormLayout(page);
        controls->enabled = new QCheckBox(
            "Light enabled (turn source on/off)", page);
        QFont enabled_font = controls->enabled->font();
        enabled_font.setBold(true);
        controls->enabled->setFont(enabled_font);
        controls->enabled->setChecked(light.enabled);
        form->addRow(controls->enabled);
        connect(controls->enabled, &QCheckBox::toggled, this,
            [tabs, index](bool enabled) {
                tabs->setTabText(index, QString("Light-%1%2")
                    .arg(index + 1).arg(enabled ? "" : " (OFF)"));
            });
        controls->type = new QComboBox(page);
        controls->type->addItems({"Omni", "Spot", "Directional"});
        controls->type->setCurrentIndex(light.type == RenderLight::Type::Spot
            ? 1 : light.type == RenderLight::Type::Directional ? 2 : 0);
        form->addRow("Type", controls->type);
        form->addRow("Position (mm)", vector_row(page, controls->px,
            controls->py, controls->pz, -1000000.0, 1000000.0, 1, 100.0));
        controls->px->setValue(light.position.x);
        controls->py->setValue(light.position.y);
        controls->pz->setValue(light.position.z);
        form->addRow("Direction", vector_row(page, controls->dx,
            controls->dy, controls->dz, -1.0, 1.0, 4, 0.05));
        controls->dx->setValue(light.direction.x);
        controls->dy->setValue(light.direction.y);
        controls->dz->setValue(light.direction.z);
        controls->ambient = new QPushButton(page);
        controls->diffuse = new QPushButton(page);
        controls->specular = new QPushButton(page);
        controls->ambient_color = light.ambient;
        controls->diffuse_color = light.diffuse;
        controls->specular_color = light.specular;
        show_color(controls->ambient, controls->ambient_color);
        show_color(controls->diffuse, controls->diffuse_color);
        show_color(controls->specular, controls->specular_color);
        connect(controls->ambient, &QPushButton::clicked, this,
            [this, controls]() { choose_color(this, controls->ambient, controls->ambient_color); });
        connect(controls->diffuse, &QPushButton::clicked, this,
            [this, controls]() { choose_color(this, controls->diffuse, controls->diffuse_color); });
        connect(controls->specular, &QPushButton::clicked, this,
            [this, controls]() { choose_color(this, controls->specular, controls->specular_color); });
        form->addRow("Ambient", controls->ambient);
        form->addRow("Diffuse", controls->diffuse);
        form->addRow("Specular", controls->specular);
        controls->hotspot = spin(page, 0.0, 180.0, 1, 1.0);
        controls->falloff = spin(page, 0.0, 180.0, 1, 1.0);
        controls->exponent = spin(page, 0.0, 128.0, 1, 1.0);
        controls->hotspot->setValue(light.hotspot_radians * 180.0 / 3.141592653589793);
        controls->falloff->setValue(light.falloff_radians * 180.0 / 3.141592653589793);
        controls->exponent->setValue(light.spot_exponent);
        form->addRow("Hotspot angle", controls->hotspot);
        form->addRow("Falloff angle", controls->falloff);
        form->addRow("Spot exponent", controls->exponent);
        controls->attenuation = new QCheckBox("Enabled", page);
        controls->attenuation->setChecked(light.attenuation_enabled);
        form->addRow("Attenuation", controls->attenuation);
        controls->range = spin(page, 0.0, 10000000.0, 1, 1000.0);
        controls->constant = spin(page, 0.0, 1000.0, 6, 0.1);
        controls->linear = spin(page, 0.0, 1000.0, 6, 0.001);
        controls->quadratic = spin(page, 0.0, 1000.0, 6, 0.0001);
        controls->range->setValue(light.range);
        controls->constant->setValue(light.constant_attenuation);
        controls->linear->setValue(light.linear_attenuation);
        controls->quadratic->setValue(light.quadratic_attenuation);
        form->addRow("Range", controls->range);
        form->addRow("Constant", controls->constant);
        form->addRow("Linear", controls->linear);
        form->addRow("Quadratic", controls->quadratic);
        controls->size = spin(page, 0.0, 100000.0, 1, 10.0);
        controls->size->setValue(light.size);
        form->addRow("Size (mm)", controls->size);
        controls->shadow = spin(page, 0.0, 100.0, 1, 5.0);
        controls->shadow->setSuffix(" %");
        controls->shadow->setValue(light.shadow_density * 100.0f);
        controls->casts_shadows = new QCheckBox(
            "Cast shadows only (light stays enabled)", page);
        controls->casts_shadows->setChecked(light.casts_shadows);
        form->addRow("Shadow density", controls->shadow);
        form->addRow("Cast shadows", controls->casts_shadows);
        tabs->addTab(page, QString("Light-%1").arg(index + 1));
        if (!light.enabled) {
            tabs->setTabText(index, QString("Light-%1 (OFF)").arg(index + 1));
        }
    }
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

RenderLightEditorDialog::~RenderLightEditorDialog() {
    for (Controls* controls : controls_) delete controls;
}

std::vector<RenderLight> RenderLightEditorDialog::Lights() const {
    std::vector<RenderLight> result;
    for (const Controls* controls : controls_) {
        RenderLight light;
        light.enabled = controls->enabled->isChecked();
        light.type = controls->type->currentIndex() == 1
            ? RenderLight::Type::Spot
            : controls->type->currentIndex() == 2
                ? RenderLight::Type::Directional : RenderLight::Type::Omni;
        light.position = {static_cast<float>(controls->px->value()),
                          static_cast<float>(controls->py->value()),
                          static_cast<float>(controls->pz->value())};
        light.direction = {static_cast<float>(controls->dx->value()),
                           static_cast<float>(controls->dy->value()),
                           static_cast<float>(controls->dz->value())};
        light.ambient = controls->ambient_color;
        light.diffuse = controls->diffuse_color;
        light.specular = controls->specular_color;
        light.hotspot_radians = static_cast<float>(controls->hotspot->value() * 3.141592653589793 / 180.0);
        light.falloff_radians = static_cast<float>(controls->falloff->value() * 3.141592653589793 / 180.0);
        light.spot_exponent = static_cast<float>(controls->exponent->value());
        light.attenuation_enabled = controls->attenuation->isChecked();
        light.range = static_cast<float>(controls->range->value());
        light.constant_attenuation = static_cast<float>(controls->constant->value());
        light.linear_attenuation = static_cast<float>(controls->linear->value());
        light.quadratic_attenuation = static_cast<float>(controls->quadratic->value());
        light.size = static_cast<float>(controls->size->value());
        light.shadow_density = static_cast<float>(controls->shadow->value() / 100.0);
        light.casts_shadows = controls->casts_shadows->isChecked();
        result.push_back(light);
    }
    return result;
}

std::vector<RenderLight> RenderLightEditorDialog::Defaults(const RenderScene& scene) {
    Vec3 minimum{0.0f, 0.0f, 0.0f};
    Vec3 maximum{6000.0f, 4000.0f, 2900.0f};
    bool first = true;
    for (const RenderMesh& mesh : scene.meshes) for (Vec3 point : mesh.vertices) {
        if (first) { minimum = maximum = point; first = false; }
        minimum = {std::min(minimum.x, point.x), std::min(minimum.y, point.y), std::min(minimum.z, point.z)};
        maximum = {std::max(maximum.x, point.x), std::max(maximum.y, point.y), std::max(maximum.z, point.z)};
    }
    const Vec3 center{(minimum.x + maximum.x) * 0.5f,
                      (minimum.y + maximum.y) * 0.5f,
                      (minimum.z + maximum.z) * 0.5f};
    const Vec3 extent{maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z};
    const std::array<Vec3, 3> positions{{
        {center.x + extent.x * 0.18f, center.y + extent.y * 0.06f, minimum.z + extent.z * 0.91f},
        {center.x - extent.x * 0.28f, center.y - extent.y * 0.24f, minimum.z + extent.z * 0.76f},
        {center.x - extent.x * 0.34f, center.y + extent.y * 0.28f, minimum.z + extent.z * 0.60f}}};
    std::vector<RenderLight> result(3);
    for (size_t index = 0; index < result.size(); ++index) {
        RenderLight& light = result[index];
        light.position = positions[index];
        light.direction = {center.x - light.position.x,
                           center.y - light.position.y,
                           minimum.z + extent.z * 0.30f - light.position.z};
        const float length = std::sqrt(light.direction.x * light.direction.x
            + light.direction.y * light.direction.y + light.direction.z * light.direction.z);
        if (length > 0.0001f) light.direction = {
            light.direction.x / length, light.direction.y / length, light.direction.z / length};
        light.range = 100000.0f;
        light.constant_attenuation = 1.0f;
        light.size = std::max({extent.x, extent.y, extent.z}) * 0.06f;
        light.shadow_density = index == 0 ? 1.0f : index == 1 ? 0.82f : 0.68f;
    }
    result[0].diffuse = {0.279f, 0.240f, 0.201f}; result[0].ambient = {0.042f, 0.042f, 0.042f};
    result[1].diffuse = {0.184f, 0.194f, 0.205f}; result[1].ambient = {0.036f, 0.036f, 0.036f};
    result[2].diffuse = {0.095f, 0.087f, 0.076f}; result[2].ambient = {0.022f, 0.022f, 0.022f};
    result[2].specular = {0.18f, 0.18f, 0.18f};
    return result;
}

std::vector<RenderLight> RenderLightEditorDialog::Load(
    QSettings& settings, const std::vector<RenderLight>& defaults) {
    constexpr int current_version = 2;
    if (settings.value("render/customLightsVersion", 0).toInt()
        < current_version) {
        return defaults;
    }
    std::vector<RenderLight> result = defaults;
    for (int index = 0; index < static_cast<int>(result.size()); ++index) {
        RenderLight& light = result[static_cast<size_t>(index)];
        light.enabled = settings.value(key(index, "enabled"), light.enabled).toBool();
        light.type = static_cast<RenderLight::Type>(std::clamp(settings.value(key(index, "type"), static_cast<int>(light.type)).toInt(), 0, 2));
        light.position = {settings.value(key(index, "px"), light.position.x).toFloat(), settings.value(key(index, "py"), light.position.y).toFloat(), settings.value(key(index, "pz"), light.position.z).toFloat()};
        light.direction = {settings.value(key(index, "dx"), light.direction.x).toFloat(), settings.value(key(index, "dy"), light.direction.y).toFloat(), settings.value(key(index, "dz"), light.direction.z).toFloat()};
        auto color = [&](const char* field, Color value) { return Color{
            settings.value(key(index, (QString(field) + "R").toUtf8().constData()), value.r).toFloat(),
            settings.value(key(index, (QString(field) + "G").toUtf8().constData()), value.g).toFloat(),
            settings.value(key(index, (QString(field) + "B").toUtf8().constData()), value.b).toFloat()}; };
        light.ambient = color("ambient", light.ambient); light.diffuse = color("diffuse", light.diffuse); light.specular = color("specular", light.specular);
        light.hotspot_radians = settings.value(key(index, "hotspot"), light.hotspot_radians).toFloat();
        light.falloff_radians = settings.value(key(index, "falloff"), light.falloff_radians).toFloat();
        light.spot_exponent = settings.value(key(index, "exponent"), light.spot_exponent).toFloat();
        light.attenuation_enabled = settings.value(key(index, "attenuation"), light.attenuation_enabled).toBool();
        light.range = settings.value(key(index, "range"), light.range).toFloat();
        light.constant_attenuation = settings.value(key(index, "constant"), light.constant_attenuation).toFloat();
        light.linear_attenuation = settings.value(key(index, "linear"), light.linear_attenuation).toFloat();
        light.quadratic_attenuation = settings.value(key(index, "quadratic"), light.quadratic_attenuation).toFloat();
        light.size = settings.value(key(index, "size"), light.size).toFloat();
        light.shadow_density = settings.value(key(index, "shadow"), light.shadow_density).toFloat();
        light.casts_shadows = settings.value(key(index, "castsShadows"), light.casts_shadows).toBool();
    }
    return result;
}

void RenderLightEditorDialog::Save(QSettings& settings,
                                   const std::vector<RenderLight>& lights) {
    settings.setValue("render/customLightsVersion", 2);
    for (int index = 0; index < static_cast<int>(lights.size()); ++index) {
        const RenderLight& light = lights[static_cast<size_t>(index)];
        settings.setValue(key(index, "enabled"), light.enabled); settings.setValue(key(index, "type"), static_cast<int>(light.type));
        settings.setValue(key(index, "px"), light.position.x); settings.setValue(key(index, "py"), light.position.y); settings.setValue(key(index, "pz"), light.position.z);
        settings.setValue(key(index, "dx"), light.direction.x); settings.setValue(key(index, "dy"), light.direction.y); settings.setValue(key(index, "dz"), light.direction.z);
        auto color = [&](const char* field, Color value) { settings.setValue(key(index, (QString(field) + "R").toUtf8().constData()), value.r); settings.setValue(key(index, (QString(field) + "G").toUtf8().constData()), value.g); settings.setValue(key(index, (QString(field) + "B").toUtf8().constData()), value.b); };
        color("ambient", light.ambient); color("diffuse", light.diffuse); color("specular", light.specular);
        settings.setValue(key(index, "hotspot"), light.hotspot_radians); settings.setValue(key(index, "falloff"), light.falloff_radians); settings.setValue(key(index, "exponent"), light.spot_exponent);
        settings.setValue(key(index, "attenuation"), light.attenuation_enabled); settings.setValue(key(index, "range"), light.range); settings.setValue(key(index, "constant"), light.constant_attenuation); settings.setValue(key(index, "linear"), light.linear_attenuation); settings.setValue(key(index, "quadratic"), light.quadratic_attenuation);
        settings.setValue(key(index, "size"), light.size); settings.setValue(key(index, "shadow"), light.shadow_density); settings.setValue(key(index, "castsShadows"), light.casts_shadows);
    }
}
