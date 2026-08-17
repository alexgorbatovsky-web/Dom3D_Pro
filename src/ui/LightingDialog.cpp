#include "LightingDialog.h"

#include "../CMesh3D.h"
#include "LanguageManager.h"

#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace {
constexpr int kSliderScale = 100;
}

LightingDialog::LightingDialog(QWidget* parent)
    : QDialog(parent) {
    auto& language = LanguageManager::Instance();
    language.BindText(this, "LightingTitle", "Lighting");
    setModal(false);
    setMinimumWidth(540);
    setWindowFlag(Qt::Tool, true);

    auto* description = new QLabel(
        "Camera-relative studio lighting. Changes are applied to the scene immediately.",
        this);
    language.BindText(description, "LightingDescription",
                      "Camera-relative studio lighting. Changes are applied to the scene immediately.");
    description->setWordWrap(true);

    auto* group = new QGroupBox("Viewport lighting", this);
    language.BindText(group, "ViewportLighting", "Viewport lighting");
    form_ = new QFormLayout(group);

    light_x_ = AddControl(
        "LightX", "Light X", "LightX_HINT",
        "Horizontal light direction. Controls whether the model is lit from the left or right.",
        -1.0, 1.0, 0.05);
    light_y_ = AddControl(
        "LightY", "Light Y", "LightY_HINT",
        "Vertical light direction. Moves the light above or below the model.",
        -1.0, 1.0, 0.05);
    light_z_ = AddControl(
        "LightZ", "Light Z", "LightZ_HINT",
        "Light direction through scene depth. Controls whether the light comes from the front or rear.",
        -1.0, 1.0, 0.05);
    ambient_ = AddControl(
        "Ambient", "Ambient", "Ambient_HINT",
        "Background light over the whole model. Raise it when shadow details disappear; too much makes forms look flat.",
        0.0, 1.5, 0.02);
    wrap_light_ = AddControl(
        "WrapLight", "Wrap light", "WrapLight_HINT",
        "Softens the light-to-shadow boundary as if light wraps around the surface. Useful for furniture and rounded forms.",
        0.0, 1.0, 0.02);
    diffuse_ = AddControl(
        "Diffuse", "Diffuse", "Diffuse_HINT",
        "Strength of the main diffuse light. Controls surface brightness without highlights.",
        0.0, 1.5, 0.02);
    specular_ = AddControl(
        "Specular", "Specular", "Specular_HINT",
        "Strength of reflected highlights. Higher values make a material look more glossy or metallic.",
        0.0, 1.5, 0.02);
    shininess_scale_ = AddControl(
        "ShininessScale", "Shininess scale", "ShininessScale_HINT",
        "Highlight sharpness. Higher values produce a small sharp highlight; lower values make it broad and soft.",
        0.1, 3.0, 0.05);
    rim_ = AddControl(
        "Rim", "Rim", "Rim_HINT",
        "Illuminates silhouette edges. Helps separate the object from the background and emphasizes rounded forms.",
        0.0, 1.0, 0.02);
    gamma_ = AddControl(
        "Gamma", "Gamma", "Gamma_HINT",
        "Midtone brightness. Lower values brighten the image; higher values darken it.",
        0.2, 2.5, 0.02);

    environment_enabled_ = new QCheckBox(
        "Use HDRI environment", group);
    form_->addRow("Environment", environment_enabled_);
    auto* environment_row = new QWidget(group);
    auto* environment_layout = new QHBoxLayout(environment_row);
    environment_layout->setContentsMargins(0, 0, 0, 0);
    environment_combo_ = new QComboBox(environment_row);
    auto* browse_environment = new QPushButton("...", environment_row);
    browse_environment->setFixedWidth(36);
    environment_layout->addWidget(environment_combo_, 1);
    environment_layout->addWidget(browse_environment);
    form_->addRow("HDRI", environment_row);
    environment_strength_ = AddControl(
        "EnvironmentStrength", "HDRI Strength", "EnvironmentStrength_HINT",
        "Brightness of reflections and ambient light from the HDRI environment.",
        0.0, 5.0, 0.05);
    environment_rotation_ = AddControl(
        "EnvironmentRotation", "HDRI Rotation", "EnvironmentRotation_HINT",
        "Rotates the panorama and moves reflected windows and light sources around the furniture.",
        -360.0, 360.0, 1.0);
    connect(browse_environment, &QPushButton::clicked,
            this, &LightingDialog::BrowseEnvironment);

    auto* reset = new QPushButton("Reset lighting", this);
    auto* close = new QPushButton("Close", this);
    language.BindText(reset, "ResetLighting", "Reset lighting");
    language.BindText(close, "Close", "Close");
    connect(reset, &QPushButton::clicked, this, &LightingDialog::ResetDefaults);
    connect(close, &QPushButton::clicked, this, &QDialog::close);

    auto* buttons = new QHBoxLayout();
    buttons->addWidget(reset);
    buttons->addStretch();
    buttons->addWidget(close);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    layout->addWidget(description);
    layout->addWidget(group);
    layout->addLayout(buttons);

    LoadSettings();
    ConnectLiveUpdates();
}

QDoubleSpinBox* LightingDialog::AddControl(const QString& text_id,
                                           const QString& label,
                                           const QString& tooltip_id,
                                           const QString& tooltip,
                                           double minimum,
                                           double maximum,
                                           double step) {
    auto* row = new QWidget(this);
    auto* row_layout = new QHBoxLayout(row);
    row_layout->setContentsMargins(0, 0, 0, 0);
    row_layout->setSpacing(8);

    auto* slider = new QSlider(Qt::Horizontal, row);
    slider->setRange(qRound(minimum * kSliderScale),
                     qRound(maximum * kSliderScale));
    slider->setSingleStep(std::max(1, qRound(step * kSliderScale)));
    slider->setPageStep(slider->singleStep() * 5);
    slider->setMinimumWidth(250);
    auto& language = LanguageManager::Instance();
    language.BindToolTip(slider, tooltip_id, tooltip);

    auto* spin = new QDoubleSpinBox(row);
    spin->setDecimals(2);
    spin->setRange(minimum, maximum);
    spin->setSingleStep(step);
    spin->setKeyboardTracking(false);
    spin->setFixedWidth(92);
    language.BindToolTip(spin, tooltip_id, tooltip);

    connect(slider, &QSlider::valueChanged, spin, [spin](int value) {
        spin->setValue(static_cast<double>(value) / kSliderScale);
    });
    connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            slider, [slider](double value) {
        const int slider_value = qRound(value * kSliderScale);
        if (slider->value() != slider_value) {
            slider->setValue(slider_value);
        }
    });

    row_layout->addWidget(slider, 1);
    row_layout->addWidget(spin);
    auto* name = new QLabel(label, this);
    language.BindText(name, text_id, label);
    language.BindToolTip(name, tooltip_id, tooltip);
    form_->addRow(name, row);
    return spin;
}

void LightingDialog::LoadSettings() {
    QSettings settings("Dom3D", "Dom3D_Pro");
    const ViewportLightingSettings defaults;
    light_x_->setValue(settings.value("view/lighting/lightX", defaults.light_x).toDouble());
    light_y_->setValue(settings.value("view/lighting/lightY", defaults.light_y).toDouble());
    light_z_->setValue(settings.value("view/lighting/lightZ", defaults.light_z).toDouble());
    ambient_->setValue(settings.value("view/lighting/ambient", defaults.ambient).toDouble());
    wrap_light_->setValue(settings.value("view/lighting/wrapLight", defaults.wrap_light).toDouble());
    diffuse_->setValue(settings.value("view/lighting/diffuse", defaults.diffuse).toDouble());
    specular_->setValue(settings.value("view/lighting/specular", defaults.specular).toDouble());
    shininess_scale_->setValue(settings.value(
        "view/lighting/shininessScale", defaults.shininess_scale).toDouble());
    rim_->setValue(settings.value("view/lighting/rim", defaults.rim).toDouble());
    gamma_->setValue(settings.value("view/lighting/gamma", defaults.gamma).toDouble());
    environment_enabled_->setChecked(settings.value(
        "view/lighting/environmentEnabled",
        defaults.environment_enabled).toBool());
    PopulateEnvironments();
    const QString environment_path = settings.value(
        "view/lighting/environmentPath",
        QString::fromStdString(CMesh3D::DefaultEnvironmentPath())).toString();
    int environment_index = environment_combo_->findData(environment_path);
    if (environment_index < 0 && !environment_path.isEmpty()) {
        environment_combo_->addItem(
            QFileInfo(environment_path).completeBaseName(), environment_path);
        environment_index = environment_combo_->count() - 1;
    }
    environment_combo_->setCurrentIndex(std::max(0, environment_index));
    environment_strength_->setValue(settings.value(
        "view/lighting/environmentStrength",
        defaults.environment_strength).toDouble());
    environment_rotation_->setValue(settings.value(
        "view/lighting/environmentRotation",
        defaults.environment_rotation_degrees).toDouble());
}

void LightingDialog::PopulateEnvironments() {
    environment_combo_->clear();
    environment_combo_->addItem("Procedural studio", QString{});
    const QString root = QDir(
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .filePath("Dom3D Pro/HDRI");
    QDirIterator iterator(
        root, QStringList{"*_HDR.exr"}, QDir::Files,
        QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        const QFileInfo info(path);
        environment_combo_->addItem(info.dir().dirName(), path);
    }
}

void LightingDialog::BrowseEnvironment() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Choose HDRI Environment",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "HDR environments (*.exr *.hdr);;Panoramas (*.jpg *.jpeg *.png);;All files (*.*)");
    if (path.isEmpty()) return;
    int index = environment_combo_->findData(path);
    if (index < 0) {
        environment_combo_->addItem(QFileInfo(path).completeBaseName(), path);
        index = environment_combo_->count() - 1;
    }
    environment_combo_->setCurrentIndex(index);
}

void LightingDialog::ConnectLiveUpdates() {
    const QDoubleSpinBox* controls[] = {
        light_x_, light_y_, light_z_, ambient_, wrap_light_, diffuse_,
        specular_, shininess_scale_, rim_, gamma_};
    for (const QDoubleSpinBox* control : controls) {
        connect(control, qOverload<double>(&QDoubleSpinBox::valueChanged),
                this, [this](double) { SaveAndApply(); });
    }
    connect(environment_enabled_, &QCheckBox::toggled,
            this, [this](bool) { SaveAndApply(); });
    connect(environment_combo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { SaveAndApply(); });
    connect(environment_strength_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double) { SaveAndApply(); });
    connect(environment_rotation_, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double) { SaveAndApply(); });
}

void LightingDialog::SaveAndApply() {
    QSettings settings("Dom3D", "Dom3D_Pro");
    settings.setValue("view/lighting/lightX", light_x_->value());
    settings.setValue("view/lighting/lightY", light_y_->value());
    settings.setValue("view/lighting/lightZ", light_z_->value());
    settings.setValue("view/lighting/ambient", ambient_->value());
    settings.setValue("view/lighting/wrapLight", wrap_light_->value());
    settings.setValue("view/lighting/diffuse", diffuse_->value());
    settings.setValue("view/lighting/specular", specular_->value());
    settings.setValue("view/lighting/shininessScale", shininess_scale_->value());
    settings.setValue("view/lighting/rim", rim_->value());
    settings.setValue("view/lighting/gamma", gamma_->value());
    settings.setValue("view/lighting/environmentEnabled",
                      environment_enabled_->isChecked());
    settings.setValue("view/lighting/environmentPath",
                      environment_combo_->currentData().toString());
    settings.setValue("view/lighting/environmentStrength",
                      environment_strength_->value());
    settings.setValue("view/lighting/environmentRotation",
                      environment_rotation_->value());
    CMesh3D::ReloadLightingSettings();
    emit LightingChanged();
}

void LightingDialog::ResetDefaults() {
    const ViewportLightingSettings defaults;
    light_x_->setValue(defaults.light_x);
    light_y_->setValue(defaults.light_y);
    light_z_->setValue(defaults.light_z);
    ambient_->setValue(defaults.ambient);
    wrap_light_->setValue(defaults.wrap_light);
    diffuse_->setValue(defaults.diffuse);
    specular_->setValue(defaults.specular);
    shininess_scale_->setValue(defaults.shininess_scale);
    rim_->setValue(defaults.rim);
    gamma_->setValue(defaults.gamma);
    environment_enabled_->setChecked(defaults.environment_enabled);
    const QString default_environment =
        QString::fromStdString(CMesh3D::DefaultEnvironmentPath());
    int index = environment_combo_->findData(default_environment);
    environment_combo_->setCurrentIndex(std::max(0, index));
    environment_strength_->setValue(defaults.environment_strength);
    environment_rotation_->setValue(defaults.environment_rotation_degrees);
    SaveAndApply();
}
