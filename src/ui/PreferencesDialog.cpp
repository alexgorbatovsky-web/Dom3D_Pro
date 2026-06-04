#include "PreferencesDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Options of program");
    setModal(true);
    setMinimumWidth(360);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(CreatePlaceholderPage("Project preferences will be added here."), "Project");
    tabs->addTab(CreateModelingPage(), "Modeling");
    tabs->addTab(CreatePlaceholderPage("Draft options will be added here."), "Draft");
    tabs->addTab(CreatePlaceholderPage("Picture and viewport options will be added here."), "Picture");
    tabs->setCurrentIndex(1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        ApplySettings();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &PreferencesDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() {
        ApplySettings();
    });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    layout->addWidget(tabs);
    layout->addWidget(buttons);

    LoadSettings();
}

QWidget* PreferencesDialog::CreateModelingPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* tolerance_group = new QGroupBox("Tolerance modeling", page);
    auto* tolerance_layout = new QFormLayout(tolerance_group);
    tolerance_modeling_ = new QDoubleSpinBox(tolerance_group);
    tolerance_modeling_->setDecimals(3);
    tolerance_modeling_->setRange(0.001, 10.0);
    tolerance_modeling_->setSingleStep(0.01);
    tolerance_layout->addRow("Value", tolerance_modeling_);
    layout->addWidget(tolerance_group);

    auto* offset_group = new QGroupBox("2D Offset", page);
    auto* offset_layout = new QVBoxLayout(offset_group);
    delete_loop_ = new QCheckBox("Delete loop", offset_group);
    offset_layout->addWidget(delete_loop_);

    auto* corner_group = new QGroupBox("Offset of Corner", offset_group);
    auto* corner_layout = new QHBoxLayout(corner_group);
    offset_corner_ = new QRadioButton("Corner", corner_group);
    offset_radius_ = new QRadioButton("Radius", corner_group);
    auto* offset_button_group = new QButtonGroup(corner_group);
    offset_button_group->addButton(offset_corner_);
    offset_button_group->addButton(offset_radius_);
    corner_layout->addWidget(offset_corner_);
    corner_layout->addStretch();
    corner_layout->addWidget(offset_radius_);
    offset_layout->addWidget(corner_group);
    layout->addWidget(offset_group);

    auto* alignment_layout = new QFormLayout();
    angle_alignment_ = new QSpinBox(page);
    angle_alignment_->setRange(0, 90);
    alignment_layout->addRow("Angle alignment", angle_alignment_);
    spiral_ = new QCheckBox("Spiral", page);
    spiral_value_ = new QSpinBox(page);
    spiral_value_->setRange(0, 100);
    auto* spiral_row = new QWidget(page);
    auto* spiral_layout = new QHBoxLayout(spiral_row);
    spiral_layout->setContentsMargins(0, 0, 0, 0);
    spiral_layout->addWidget(spiral_);
    spiral_layout->addWidget(spiral_value_);
    alignment_layout->addRow(spiral_row);
    layout->addLayout(alignment_layout);

    auto* parametrization_group = new QGroupBox("Parametrization", page);
    auto* parametrization_layout = new QHBoxLayout(parametrization_group);
    parametrization_none_ = new QRadioButton("No", parametrization_group);
    parametrization_ortho_ = new QRadioButton("Ortho", parametrization_group);
    parametrization_max_template_ = new QRadioButton("Max template", parametrization_group);
    auto* parametrization_button_group = new QButtonGroup(parametrization_group);
    parametrization_button_group->addButton(parametrization_none_);
    parametrization_button_group->addButton(parametrization_ortho_);
    parametrization_button_group->addButton(parametrization_max_template_);
    parametrization_layout->addWidget(parametrization_none_);
    parametrization_layout->addWidget(parametrization_ortho_);
    parametrization_layout->addWidget(parametrization_max_template_);
    layout->addWidget(parametrization_group);

    auto* flags_grid = new QGridLayout();
    change_group_ = new QCheckBox("Change the group", page);
    two_d_drag_auto_ = new QCheckBox("2D Dragin Auto", page);
    control_intersections_ = new QCheckBox("Control intersections", page);
    gizmo_3d_enable_ = new QCheckBox("Gizmo 3D Enable", page);
    flags_grid->addWidget(change_group_, 0, 0);
    flags_grid->addWidget(two_d_drag_auto_, 0, 1);
    flags_grid->addWidget(control_intersections_, 1, 0);
    flags_grid->addWidget(gizmo_3d_enable_, 1, 1);
    layout->addLayout(flags_grid);
    layout->addStretch();

    return page;
}

QWidget* PreferencesDialog::CreatePlaceholderPage(const QString& text) {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->addWidget(new QLabel(text, page));
    layout->addStretch();
    return page;
}

void PreferencesDialog::LoadSettings() {
    QSettings settings("Dom3D", "Dom3D_Pro");

    tolerance_modeling_->setValue(settings.value("preferences/modeling/tolerance", 0.02).toDouble());
    delete_loop_->setChecked(settings.value("preferences/modeling/deleteLoop", true).toBool());
    offset_corner_->setChecked(settings.value("preferences/modeling/offsetCornerMode", "corner").toString() == "corner");
    offset_radius_->setChecked(!offset_corner_->isChecked());
    angle_alignment_->setValue(settings.value("preferences/modeling/angleAlignment", 7).toInt());
    spiral_->setChecked(settings.value("preferences/modeling/spiral", true).toBool());
    spiral_value_->setValue(settings.value("preferences/modeling/spiralValue", 6).toInt());

    const QString parametrization = settings.value("preferences/modeling/parametrization", "maxTemplate").toString();
    parametrization_none_->setChecked(parametrization == "none");
    parametrization_ortho_->setChecked(parametrization == "ortho");
    parametrization_max_template_->setChecked(parametrization != "none" && parametrization != "ortho");

    change_group_->setChecked(settings.value("preferences/modeling/changeGroup", true).toBool());
    two_d_drag_auto_->setChecked(settings.value("preferences/modeling/2dDragAuto", false).toBool());
    control_intersections_->setChecked(settings.value("preferences/modeling/controlIntersections", false).toBool());
    gizmo_3d_enable_->setChecked(settings.value("preferences/modeling/gizmo3dEnable", true).toBool());
}

void PreferencesDialog::ApplySettings() {
    QSettings settings("Dom3D", "Dom3D_Pro");

    settings.setValue("preferences/modeling/tolerance", tolerance_modeling_->value());
    settings.setValue("preferences/modeling/deleteLoop", delete_loop_->isChecked());
    settings.setValue("preferences/modeling/offsetCornerMode", offset_corner_->isChecked() ? "corner" : "radius");
    settings.setValue("preferences/modeling/angleAlignment", angle_alignment_->value());
    settings.setValue("preferences/modeling/spiral", spiral_->isChecked());
    settings.setValue("preferences/modeling/spiralValue", spiral_value_->value());

    QString parametrization = "maxTemplate";
    if (parametrization_none_->isChecked()) {
        parametrization = "none";
    } else if (parametrization_ortho_->isChecked()) {
        parametrization = "ortho";
    }
    settings.setValue("preferences/modeling/parametrization", parametrization);
    settings.setValue("preferences/modeling/changeGroup", change_group_->isChecked());
    settings.setValue("preferences/modeling/2dDragAuto", two_d_drag_auto_->isChecked());
    settings.setValue("preferences/modeling/controlIntersections", control_intersections_->isChecked());
    settings.setValue("preferences/modeling/gizmo3dEnable", gizmo_3d_enable_->isChecked());
}
