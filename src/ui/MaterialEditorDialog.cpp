#include "MaterialEditorDialog.h"

#include "MaterialDrag.h"
#include "MaterialSphereBrowser.h"
#include "DragSpinBoxLabel.h"

#include <QColorDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDrag>
#include <QDir>
#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QRadialGradient>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <map>

namespace {
constexpr int kEntryIndexRole = Qt::UserRole + 1;
constexpr int kDocumentIndexRole = Qt::UserRole + 2;

struct RalColor {
    const char* code;
    const char* name;
    Color color;
};

constexpr RalColor kRalColors[] = {
    {"1003", "Signal Yellow", {0.9765f, 0.6627f, 0.0000f}},
    {"1013", "Oyster White", {0.9137f, 0.8980f, 0.8078f}},
    {"2004", "Pure Orange", {0.9059f, 0.3569f, 0.0706f}},
    {"3000", "Flame Red", {0.6549f, 0.1608f, 0.1255f}},
    {"3002", "Carmine Red", {0.6353f, 0.1373f, 0.1137f}},
    {"3020", "Traffic Red", {0.8000f, 0.0235f, 0.0196f}},
    {"4005", "Blue Lilac", {0.5137f, 0.3882f, 0.6157f}},
    {"5002", "Ultramarine Blue", {0.1255f, 0.1294f, 0.3098f}},
    {"5005", "Signal Blue", {0.0824f, 0.2824f, 0.5373f}},
    {"5017", "Traffic Blue", {0.0000f, 0.3569f, 0.5490f}},
    {"5018", "Turquoise Blue", {0.0196f, 0.5451f, 0.5490f}},
    {"6002", "Leaf Green", {0.1529f, 0.3843f, 0.1882f}},
    {"6005", "Moss Green", {0.0588f, 0.2627f, 0.2118f}},
    {"6018", "Yellow Green", {0.3412f, 0.6510f, 0.2235f}},
    {"7015", "Slate Grey", {0.3059f, 0.3294f, 0.3216f}},
    {"7016", "Anthracite Grey", {0.2196f, 0.2431f, 0.2588f}},
    {"7024", "Graphite Grey", {0.2706f, 0.2863f, 0.3059f}},
    {"7035", "Light Grey", {0.8431f, 0.8431f, 0.8431f}},
    {"7040", "Window Grey", {0.6157f, 0.6392f, 0.6510f}},
    {"8017", "Chocolate Brown", {0.2706f, 0.1961f, 0.1804f}},
    {"9001", "Cream", {0.9137f, 0.8784f, 0.8235f}},
    {"9003", "Signal White", {0.9569f, 0.9569f, 0.9569f}},
    {"9005", "Jet Black", {0.0392f, 0.0392f, 0.0510f}},
    {"9010", "Pure White", {0.9686f, 0.9765f, 0.9373f}},
};

int closest_ral_index(Color color) {
    int result = 0;
    float best = std::numeric_limits<float>::max();
    for (int i = 0; i < static_cast<int>(std::size(kRalColors)); ++i) {
        const float dr = color.r - kRalColors[i].color.r;
        const float dg = color.g - kRalColors[i].color.g;
        const float db = color.b - kRalColors[i].color.b;
        const float distance = dr * dr + dg * dg + db * db;
        if (distance < best) {
            best = distance;
            result = i;
        }
    }
    return result;
}

QString texture_library_path() {
    const QString application_dir = QCoreApplication::applicationDirPath();
    const QString application_texture =
        QDir(application_dir).filePath(QStringLiteral("texture"));
    if (QDir(application_texture).exists()) {
        return QDir::cleanPath(application_texture);
    }

    // During development a Debug build may share the texture library that
    // is packaged with the neighbouring Release build.
    const QString release_texture = QDir(application_dir).absoluteFilePath(
        QStringLiteral("../Release/texture"));
    if (QDir(release_texture).exists()) {
        return QDir::cleanPath(release_texture);
    }

    QDir().mkpath(application_texture);
    return QDir::cleanPath(application_texture);
}

template <typename Base>
class DraggableMaterialView : public Base {
public:
    using MaterialResolver = std::function<bool(Material*)>;

    explicit DraggableMaterialView(QWidget* parent = nullptr)
        : Base(parent) {
        this->setDragEnabled(true);
        this->setDragDropMode(QAbstractItemView::DragOnly);
    }

    void SetMaterialResolver(MaterialResolver resolver) {
        material_resolver_ = std::move(resolver);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            drag_start_pos_ = event->pos();
            drag_start_index_ = this->indexAt(event->pos());
        }
        Base::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!(event->buttons() & Qt::LeftButton) || !drag_start_index_.isValid()) {
            Base::mouseMoveEvent(event);
            return;
        }
        if ((event->pos() - drag_start_pos_).manhattanLength() < QApplication::startDragDistance()) {
            Base::mouseMoveEvent(event);
            return;
        }

        this->setCurrentIndex(drag_start_index_);
        StartMaterialDrag();
        drag_start_index_ = QModelIndex();
    }

    void startDrag(Qt::DropActions supported_actions) override {
        (void)supported_actions;
        StartMaterialDrag();
    }

private:
    void StartMaterialDrag() {
        Material material;
        if (!material_resolver_ || !material_resolver_(&material)) {
            return;
        }

        auto* mime_data = new QMimeData;
        mime_data->setData(MaterialDrag::MimeType(), MaterialDrag::Encode(material));

        auto* drag = new QDrag(this);
        drag->setMimeData(mime_data);
        const QPixmap sphere = MaterialDrag::SpherePixmap(material, 58, true);
        drag->setPixmap(sphere);
        drag->setHotSpot(QPoint(sphere.width() / 2, sphere.height() / 2));
        drag->exec(Qt::CopyAction);
    }

    MaterialResolver material_resolver_;
    QPoint drag_start_pos_;
    QModelIndex drag_start_index_;
};

using DraggableMaterialListWidget = DraggableMaterialView<QListWidget>;
using DraggableMaterialTreeWidget = DraggableMaterialView<QTreeWidget>;

QColor to_qcolor(Color color) {
    return QColor::fromRgbF(std::clamp(color.r, 0.0f, 1.0f),
                            std::clamp(color.g, 0.0f, 1.0f),
                            std::clamp(color.b, 0.0f, 1.0f));
}

QPixmap material_sphere_pixmap(const Material& material, int size, bool selected) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal inset = selected ? 2.0 : 8.0;
    const QRectF sphere(inset, inset, size - inset * 2.0, size - inset * 2.0);
    const QColor diffuse = to_qcolor(material.diffuse);
    const QColor ambient = to_qcolor(material.ambient);

    QRadialGradient gradient(sphere.center() - QPointF(size * 0.17, size * 0.22), size * 0.62);
    gradient.setColorAt(0.0, diffuse.lighter(172));
    gradient.setColorAt(0.45, diffuse);
    gradient.setColorAt(1.0, ambient.darker(155));

    painter.setPen(QPen(selected ? QColor(255, 255, 255) : QColor(205, 205, 198), selected ? 3 : 2));
    painter.setBrush(gradient);
    painter.drawEllipse(sphere);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 62));
    painter.drawEllipse(QRectF(sphere.left() + size * 0.19, sphere.top() + size * 0.16, size * 0.15, size * 0.11));
    painter.end();
    return pixmap;
}

QString color_style(Color color) {
    return QString("background-color: rgb(%1, %2, %3); border: 1px solid #777;")
        .arg(static_cast<int>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f))
        .arg(static_cast<int>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f))
        .arg(static_cast<int>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f));
}

Color color_from_button(QPushButton* button) {
    const QColor color = button->property("materialColor").value<QColor>();
    return {static_cast<float>(color.redF()), static_cast<float>(color.greenF()), static_cast<float>(color.blueF())};
}

QIcon brush_icon() {
    QPixmap pixmap(42, 28);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(120, 80, 28), 3));
    painter.drawLine(QPointF(25, 6), QPointF(38, 2));
    painter.setPen(QPen(QColor(70, 70, 70), 2));
    painter.drawLine(QPointF(10, 22), QPointF(25, 8));
    painter.setBrush(QColor(235, 235, 235));
    painter.drawPolygon(QPolygonF({QPointF(6, 25), QPointF(15, 17), QPointF(21, 22), QPointF(12, 27)}));
    painter.setPen(QPen(QColor(255, 40, 25), 3));
    painter.drawLine(QPointF(4, 6), QPointF(22, 16));
    painter.end();
    return QIcon(pixmap);
}

QIcon pipette_icon() {
    QPixmap pixmap(42, 28);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(84, 92, 98), 3));
    painter.drawLine(QPointF(13, 22), QPointF(31, 4));
    painter.setBrush(QColor(210, 210, 215));
    painter.drawEllipse(QPointF(31, 4), 4, 4);
    painter.setPen(QPen(QColor(120, 150, 45), 3));
    painter.drawLine(QPointF(9, 25), QPointF(15, 19));
    painter.end();
    return QIcon(pixmap);
}
}

MaterialEditorDialog::MaterialEditorDialog(const QString& library_path,
                                           const std::vector<Material>& document_materials,
                                           bool has_selection,
                                           const Material* initial_material,
                                           QWidget* parent)
    : QDialog(parent),
      library_path_(library_path),
      document_materials_(document_materials) {
    if (initial_material) {
        initial_material_ = *initial_material;
        has_initial_material_ = true;
    }

    setWindowTitle("Material Editor");
    resize(760, 520);

    auto* main_layout = new QVBoxLayout(this);
    auto* splitter = new QSplitter(this);
    main_layout->addWidget(splitter, 1);

    auto* browser = new QWidget(splitter);
    auto* browser_layout = new QVBoxLayout(browser);
    browser_layout->setContentsMargins(0, 0, 0, 0);
    browser_layout->setSpacing(6);

    auto* document_label = new QLabel("Materials", browser);
    browser_layout->addWidget(document_label);

    material_browser_ = new MaterialSphereBrowser(browser);
    material_browser_->setFixedWidth(260);
    material_browser_->setMinimumHeight(260);
    browser_layout->addWidget(material_browser_, 1);

    auto* material_buttons_row = new QWidget(browser);
    auto* material_buttons_layout = new QHBoxLayout(material_buttons_row);
    material_buttons_layout->setContentsMargins(0, 0, 0, 0);
    material_buttons_layout->setSpacing(4);
    auto* new_button = new QPushButton("New", material_buttons_row);
    auto* library_button = new QPushButton("Library...", material_buttons_row);
    auto* export_button = new QPushButton("Export", material_buttons_row);
    auto* import_button = new QPushButton("Import", material_buttons_row);
    auto* object_button = new QPushButton("Object", material_buttons_row);
    material_buttons_layout->addWidget(new_button);
    material_buttons_layout->addWidget(library_button);
    material_buttons_layout->addWidget(export_button);
    material_buttons_layout->addWidget(import_button);
    material_buttons_layout->addWidget(object_button);
    browser_layout->addWidget(material_buttons_row);

    auto* action_buttons_row = new QWidget(browser);
    auto* action_buttons_layout = new QHBoxLayout(action_buttons_row);
    action_buttons_layout->setContentsMargins(0, 0, 0, 0);
    action_buttons_layout->setSpacing(4);
    apply_button_ = new QPushButton("Apply to Selection", action_buttons_row);
    auto* brush_button = new QPushButton(action_buttons_row);
    auto* color_button = new QPushButton("Color...", action_buttons_row);
    auto* pipette_button = new QPushButton(action_buttons_row);
    brush_button->setIcon(brush_icon());
    brush_button->setIconSize(QSize(42, 28));
    brush_button->setToolTip("Paint selected objects with current material");
    pipette_button->setIcon(pipette_icon());
    pipette_button->setIconSize(QSize(42, 28));
    pipette_button->setToolTip("Pick material from selected object");
    apply_button_->setEnabled(has_selection);
    action_buttons_layout->addWidget(apply_button_);
    action_buttons_layout->addWidget(brush_button);
    action_buttons_layout->addWidget(color_button);
    action_buttons_layout->addWidget(pipette_button);
    browser_layout->addWidget(action_buttons_row);

    document_materials_list_ = new DraggableMaterialListWidget(browser);
    document_materials_list_->setViewMode(QListView::IconMode);
    document_materials_list_->setMovement(QListView::Static);
    document_materials_list_->setResizeMode(QListView::Adjust);
    document_materials_list_->setWrapping(true);
    document_materials_list_->setSpacing(0);
    document_materials_list_->setIconSize(QSize(64, 64));
    document_materials_list_->setGridSize(QSize(68, 68));
    document_materials_list_->setUniformItemSizes(true);
    document_materials_list_->setTextElideMode(Qt::ElideRight);
    document_materials_list_->setFixedWidth(206);
    document_materials_list_->setMinimumHeight(206);
    document_materials_list_->setStyleSheet(
        "QListWidget { background: #050505; border: 1px solid #008b8b; }"
        "QListWidget::item { color: transparent; border: 1px solid #008b8b; }"
        "QListWidget::item:selected { background: #101010; }"
    );
    document_materials_list_->hide();

    material_tree_ = new DraggableMaterialTreeWidget(browser);
    material_tree_->setHeaderLabel("Library");
    material_tree_->header()->setStretchLastSection(true);
    material_tree_->setFixedWidth(206);
    material_tree_->hide();
    splitter->addWidget(browser);

    auto* editor = new QWidget(splitter);
    auto* editor_layout = new QVBoxLayout(editor);
    splitter->addWidget(editor);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    editor_layout->addLayout(form);

    name_edit_ = new QLineEdit(editor);
    id_edit_ = new QLineEdit(editor);
    form->addRow("Name", name_edit_);
    id_edit_->hide();

    coating_group_ = new QGroupBox("Configurable coating", editor);
    auto* coating_layout = new QGridLayout(coating_group_);
    ral_combo_ = new QComboBox(coating_group_);
    for (const RalColor& ral : kRalColors) {
        ral_combo_->addItem(
            QString("RAL %1 — %2").arg(ral.code, ral.name),
            QString::fromLatin1(ral.code));
    }
    lacquered_check_ = new QCheckBox("Glossy lacquer coat", coating_group_);
    film_type_label_ = new QLabel("Film type", coating_group_);
    film_type_combo_ = new QComboBox(coating_group_);
    film_type_combo_->addItem("Matte opaque", 0);
    film_type_combo_->addItem("Translucent glossy", 1);
    coating_layout->addWidget(new QLabel("RAL colour", coating_group_), 0, 0);
    coating_layout->addWidget(ral_combo_, 0, 1);
    coating_layout->addWidget(lacquered_check_, 1, 0, 1, 2);
    coating_layout->addWidget(film_type_label_, 2, 0);
    coating_layout->addWidget(film_type_combo_, 2, 1);
    coating_group_->hide();
    form->addRow(coating_group_);

    auto* color_row = new QWidget(editor);
    auto* color_layout = new QHBoxLayout(color_row);
    color_layout->setContentsMargins(0, 0, 0, 0);
    ambient_button_ = new QPushButton("Ambient", color_row);
    diffuse_button_ = new QPushButton("Diffuse", color_row);
    emission_button_ = new QPushButton("Emission", color_row);
    color_layout->addWidget(ambient_button_);
    color_layout->addWidget(diffuse_button_);
    color_layout->addWidget(emission_button_);
    form->addRow("Colors", color_row);

    const auto add_spin = [editor](double min, double max, double step, int decimals) {
        auto* spin = new QDoubleSpinBox(editor);
        spin->setRange(min, max);
        spin->setSingleStep(step);
        spin->setDecimals(decimals);
        spin->setKeyboardTracking(false);
        return spin;
    };
    alpha_spin_ = add_spin(0.0, 1.0, 0.02, 2);
    specular_spin_ = add_spin(0.0, 2.0, 0.02, 2);
    shininess_spin_ = add_spin(1.0, 256.0, 1.0, 0);
    reflectivity_spin_ = add_spin(0.0, 1.0, 0.02, 2);
    roughness_spin_ = add_spin(0.04, 1.0, 0.02, 2);
    metallic_spin_ = add_spin(0.0, 1.0, 0.02, 2);
    coat_weight_spin_ = add_spin(0.0, 1.0, 0.05, 2);
    coat_roughness_spin_ = add_spin(0.01, 1.0, 0.01, 2);
    normal_strength_spin_ = add_spin(0.0, 4.0, 0.05, 2);
    displacement_scale_spin_ = add_spin(0.0, 0.25, 0.005, 3);
    form->addRow(new DragSpinBoxLabel("Alpha", alpha_spin_, editor), alpha_spin_);
    form->addRow(new DragSpinBoxLabel("Specular", specular_spin_, editor), specular_spin_);
    form->addRow(new DragSpinBoxLabel("Shininess", shininess_spin_, editor), shininess_spin_);
    form->addRow(new DragSpinBoxLabel("Reflectivity", reflectivity_spin_, editor), reflectivity_spin_);
    form->addRow(new DragSpinBoxLabel("PBR Roughness", roughness_spin_, editor), roughness_spin_);
    form->addRow(new DragSpinBoxLabel("PBR Metallic", metallic_spin_, editor), metallic_spin_);
    form->addRow(new DragSpinBoxLabel("Coat", coat_weight_spin_, editor), coat_weight_spin_);
    form->addRow(new DragSpinBoxLabel("Coat Roughness", coat_roughness_spin_, editor), coat_roughness_spin_);
    form->addRow(new DragSpinBoxLabel("Normal strength", normal_strength_spin_, editor), normal_strength_spin_);
    form->addRow(new DragSpinBoxLabel("Parallax depth", displacement_scale_spin_, editor), displacement_scale_spin_);

    auto* textures_group = new QGroupBox("Textures", editor);
    auto* textures_layout = new QGridLayout(textures_group);
    color_texture_edit_ = new QLineEdit(textures_group);
    light_texture_edit_ = new QLineEdit(textures_group);
    bump_texture_edit_ = new QLineEdit(textures_group);
    normal_texture_edit_ = new QLineEdit(textures_group);
    roughness_texture_edit_ = new QLineEdit(textures_group);
    metallic_texture_edit_ = new QLineEdit(textures_group);
    displacement_texture_edit_ = new QLineEdit(textures_group);
    const auto add_texture_row = [this, textures_layout, textures_group](int row, const QString& label, QLineEdit* edit) {
        auto* browse = new QPushButton("...", textures_group);
        browse->setFixedWidth(32);
        textures_layout->addWidget(new QLabel(label, textures_group), row, 0);
        textures_layout->addWidget(edit, row, 1);
        textures_layout->addWidget(browse, row, 2);
        connect(browse, &QPushButton::clicked, this, [this, edit]() {
            BrowseTexture(edit);
        });
    };
    add_texture_row(0, "Color", color_texture_edit_);
    add_texture_row(1, "Light", light_texture_edit_);
    add_texture_row(2, "Bump", bump_texture_edit_);
    add_texture_row(3, "Normal GL", normal_texture_edit_);
    add_texture_row(4, "Roughness", roughness_texture_edit_);
    add_texture_row(5, "Metallic", metallic_texture_edit_);
    add_texture_row(6, "Displacement", displacement_texture_edit_);
    texture_offset_u_spin_ = add_spin(-10000.0, 10000.0, 0.05, 4);
    texture_offset_v_spin_ = add_spin(-10000.0, 10000.0, 0.05, 4);
    texture_scale_u_spin_ = add_spin(-10000.0, 10000.0, 0.05, 4);
    texture_scale_v_spin_ = add_spin(-10000.0, 10000.0, 0.05, 4);
    texture_rotation_spin_ = add_spin(-3600.0, 3600.0, 1.0, 2);
    texture_rotate_90_check_ = new QCheckBox("Rotate texture 90°", textures_group);
    texture_fit_to_surface_check_ = new QCheckBox("Fit one image to each surface", textures_group);

    auto* uv_layout = new QGridLayout();
    uv_layout->setContentsMargins(0, 6, 0, 0);
    uv_layout->addWidget(new DragSpinBoxLabel("Offset U", texture_offset_u_spin_, textures_group), 0, 0);
    uv_layout->addWidget(texture_offset_u_spin_, 0, 1);
    uv_layout->addWidget(new DragSpinBoxLabel("Offset V", texture_offset_v_spin_, textures_group), 0, 2);
    uv_layout->addWidget(texture_offset_v_spin_, 0, 3);
    uv_layout->addWidget(new DragSpinBoxLabel("Scale U", texture_scale_u_spin_, textures_group), 1, 0);
    uv_layout->addWidget(texture_scale_u_spin_, 1, 1);
    uv_layout->addWidget(new DragSpinBoxLabel("Scale V", texture_scale_v_spin_, textures_group), 1, 2);
    uv_layout->addWidget(texture_scale_v_spin_, 1, 3);
    uv_layout->addWidget(new DragSpinBoxLabel("Rotate", texture_rotation_spin_, textures_group), 2, 0);
    uv_layout->addWidget(texture_rotation_spin_, 2, 1);
    uv_layout->addWidget(texture_rotate_90_check_, 2, 2, 1, 2);
    uv_layout->addWidget(texture_fit_to_surface_check_, 3, 0, 1, 4);
    textures_layout->addLayout(uv_layout, 7, 0, 1, 3);
    editor_layout->addWidget(textures_group);
    editor_layout->addStretch(1);

    auto* buttons = new QDialogButtonBox(this);
    auto* reload_button = buttons->addButton("Reload Library", QDialogButtonBox::ResetRole);
    auto* save_button = buttons->addButton("Save to Document", QDialogButtonBox::ApplyRole);
    auto* close_button = buttons->addButton(QDialogButtonBox::Close);
    main_layout->addWidget(buttons);

    connect(ambient_button_, &QPushButton::clicked, this, [this]() { PickColor(ambient_button_); });
    connect(diffuse_button_, &QPushButton::clicked, this, [this]() { PickColor(diffuse_button_); });
    connect(emission_button_, &QPushButton::clicked, this, [this]() { PickColor(emission_button_); });
    connect(ral_combo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { ApplyCoatingControls(); });
    connect(lacquered_check_, &QCheckBox::toggled,
            this, [this](bool) { ApplyCoatingControls(); });
    connect(film_type_combo_, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { ApplyCoatingControls(); });
    connect(new_button, &QPushButton::clicked, this, [this]() { CreateNewMaterial(); });
    connect(library_button, &QPushButton::clicked, this, [this]() { LoadLibrary(); });
    connect(export_button, &QPushButton::clicked, this, [this]() { ExportCurrentMaterial(); });
    connect(import_button, &QPushButton::clicked, this, [this]() { ImportMaterialFromFile(); });
    connect(object_button, &QPushButton::clicked, this, [this]() { emit RequestSelectedObjectMaterial(); });
    connect(reload_button, &QPushButton::clicked, this, [this]() { LoadLibrary(); });
    connect(save_button, &QPushButton::clicked, this, [this]() { SaveCurrentMaterial(); });
    connect(apply_button_, &QPushButton::clicked, this, [this]() { ApplyCurrentMaterial(); });
    connect(brush_button, &QPushButton::clicked, this, [this]() { emit RequestPaintMaterial(EditorMaterial()); });
    connect(color_button, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, "Material Color", "Color dialog will be connected in the next step.");
    });
    connect(pipette_button, &QPushButton::clicked, this, [this]() { emit RequestPickObjectMaterial(); });
    connect(close_button, &QPushButton::clicked, this, &QDialog::accept);
    connect(name_edit_, &QLineEdit::textEdited, this, [this]() { CommitEditorChanges(); });
    connect(color_texture_edit_, &QLineEdit::textEdited, this, [this]() { CommitEditorChanges(); });
    connect(light_texture_edit_, &QLineEdit::textEdited, this, [this]() { CommitEditorChanges(); });
    connect(bump_texture_edit_, &QLineEdit::textEdited, this, [this]() { CommitEditorChanges(); });
    connect(normal_texture_edit_, &QLineEdit::textEdited, this, [this]() { CommitEditorChanges(); });
    connect(roughness_texture_edit_, &QLineEdit::textEdited, this, [this]() { CommitEditorChanges(); });
    connect(metallic_texture_edit_, &QLineEdit::textEdited, this, [this]() { CommitEditorChanges(); });
    connect(displacement_texture_edit_, &QLineEdit::textEdited, this, [this]() { CommitEditorChanges(); });
    connect(alpha_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(specular_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(shininess_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(reflectivity_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(roughness_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(metallic_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(coat_weight_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(coat_roughness_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(normal_strength_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(displacement_scale_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(texture_offset_u_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(texture_offset_v_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(texture_scale_u_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(texture_scale_v_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(texture_rotation_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double) { CommitEditorChanges(); });
    connect(texture_rotate_90_check_, &QCheckBox::toggled, this, [this](bool checked) {
        texture_rotation_spin_->setValue(checked ? 90.0 : 0.0);
    });
    connect(texture_rotation_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double angle) {
        const double normalized = std::fmod(std::fmod(angle, 360.0) + 360.0, 360.0);
        const QSignalBlocker blocker(texture_rotate_90_check_);
        texture_rotate_90_check_->setChecked(std::abs(normalized - 90.0) < 0.001);
    });
    connect(texture_fit_to_surface_check_, &QCheckBox::toggled, this, [this](bool) { CommitEditorChanges(); });
    connect(document_materials_list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) {
            return;
        }
        const int index = item->data(kDocumentIndexRole).toInt();
        if (index < 0 || index >= static_cast<int>(document_materials_.size())) {
            return;
        }
        LoadMaterialToEditor(document_materials_[index], {});
        PopulateDocumentMaterials();
    });
    connect(material_tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        const QVariant document_index_data = item ? item->data(0, kDocumentIndexRole) : QVariant();
        if (document_index_data.isValid()) {
            const int index = document_index_data.toInt();
            if (index >= 0 && index < static_cast<int>(document_materials_.size())) {
                LoadMaterialToEditor(document_materials_[index], {});
                PopulateDocumentMaterials();
            }
            return;
        }

        const QVariant index_data = item ? item->data(0, kEntryIndexRole) : QVariant();
        if (!index_data.isValid()) {
            return;
        }
        const int index = index_data.toInt();
        const auto& entries = library_.Entries();
        if (index < 0 || index >= static_cast<int>(entries.size())) {
            return;
        }
        LoadMaterialToEditor(entries[index].material, entries[index].file_path);
    });
    connect(material_browser_, &MaterialSphereBrowser::MaterialSelected, this, [this](int, const Material& material, const QString& material_file_path) {
        LoadMaterialToEditor(material, material_file_path);
        PopulateDocumentMaterials();
    });
    static_cast<DraggableMaterialListWidget*>(document_materials_list_)->SetMaterialResolver([this](Material* material) {
        QListWidgetItem* item = document_materials_list_->currentItem();
        if (!item || !material) {
            return false;
        }
        const int index = item->data(kDocumentIndexRole).toInt();
        if (index < 0 || index >= static_cast<int>(document_materials_.size())) {
            return false;
        }
        *material = document_materials_[index];
        return true;
    });
    static_cast<DraggableMaterialTreeWidget*>(material_tree_)->SetMaterialResolver([this](Material* material) {
        QTreeWidgetItem* item = material_tree_->currentItem();
        if (!item || !material) {
            return false;
        }
        const QVariant index_data = item->data(0, kEntryIndexRole);
        if (!index_data.isValid()) {
            return false;
        }
        const int index = index_data.toInt();
        const auto& entries = library_.Entries();
        if (index < 0 || index >= static_cast<int>(entries.size())) {
            return false;
        }
        *material = entries[index].material;
        return true;
    });

    LoadLibrary();
    PopulateTree();
    PopulateDocumentMaterials();
    if (has_initial_material_) {
        LoadMaterialToEditor(initial_material_, {});
        SelectDocumentMaterialById(initial_material_.id);
    }
}

void MaterialEditorDialog::LoadLibrary() {
    current_file_path_.clear();
    if (!library_.Load(library_path_)) {
        QMessageBox::warning(this, "Material Library", QString("Cannot load material library:\n%1").arg(library_path_));
        return;
    }
    PopulateTree();
}

void MaterialEditorDialog::PopulateTree() {
    material_tree_->clear();

    std::map<QString, QTreeWidgetItem*> category_items;
    const auto& entries = library_.Entries();
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const auto& entry = entries[i];
        QTreeWidgetItem* category_item = nullptr;
        auto existing = category_items.find(entry.category);
        if (existing == category_items.end()) {
            category_item = new QTreeWidgetItem(material_tree_);
            category_item->setText(0, "Library / " + entry.category);
            existing = category_items.emplace(entry.category, category_item).first;
        }
        category_item = existing->second;
        auto* material_item = new QTreeWidgetItem(category_item);
        material_item->setText(0, QString::fromStdString(entry.material.name));
        material_item->setData(0, kEntryIndexRole, i);
    }

    material_tree_->expandAll();
    if (selected_document_material_id_ == 0 && !document_materials_.empty()) {
        LoadMaterialToEditor(document_materials_.front(), {});
    } else if (selected_document_material_id_ == 0 && !entries.empty()) {
        LoadMaterialToEditor(entries.front().material, entries.front().file_path);
    }
}

void MaterialEditorDialog::PopulateDocumentMaterials() {
    if (!document_materials_list_) {
        return;
    }

    if (material_browser_) {
        std::vector<MaterialSphereItem> items;
        items.reserve(document_materials_.size());
        for (int i = 0; i < static_cast<int>(document_materials_.size()); ++i) {
            items.push_back({document_materials_[static_cast<size_t>(i)], {}, i, true});
        }
        material_browser_->SetItems(std::move(items));
        material_browser_->SetSelectedMaterialId(selected_document_material_id_);
    }

    document_materials_list_->clear();
    for (int i = 0; i < static_cast<int>(document_materials_.size()); ++i) {
        const Material& material = document_materials_[i];
        const bool selected = material.id == selected_document_material_id_;
        auto* item = new QListWidgetItem(QIcon(material_sphere_pixmap(material, 76, selected)), QString::fromStdString(material.name), document_materials_list_);
        item->setData(kDocumentIndexRole, i);
        item->setToolTip(QString("%1 [%2]").arg(QString::fromStdString(material.name)).arg(material.id));
        if (selected) {
            item->setSelected(true);
        }
    }
}

void MaterialEditorDialog::SelectDocumentMaterialById(unsigned long id) {
    selected_document_material_id_ = id;
    PopulateDocumentMaterials();
}

void MaterialEditorDialog::LoadMaterialToEditor(const Material& material, const QString& file_path) {
    loading_editor_ = true;
    const QString candidate_path =
        file_path.isEmpty() ? QString::fromStdString(material.source_file_path) : file_path;
    current_file_path_ =
        QFileInfo(candidate_path).suffix().compare("d3mat", Qt::CaseInsensitive) == 0
        ? candidate_path
        : QString{};
    selected_document_material_id_ = material.id;
    name_edit_->setText(QString::fromStdString(material.name));
    id_edit_->setText(QString::number(material.id));
    SetColorButton(ambient_button_, material.ambient);
    SetColorButton(diffuse_button_, material.diffuse);
    SetColorButton(emission_button_, material.emission);
    alpha_spin_->setValue(material.alpha);
    specular_spin_->setValue(material.specular);
    shininess_spin_->setValue(material.shininess);
    reflectivity_spin_->setValue(material.reflectivity);
    roughness_spin_->setValue(material.roughness);
    metallic_spin_->setValue(material.metallic);
    coat_weight_spin_->setValue(material.coat_weight);
    coat_roughness_spin_->setValue(material.coat_roughness);
    normal_strength_spin_->setValue(material.normal_strength);
    displacement_scale_spin_->setValue(material.displacement_scale);
    color_texture_edit_->setText(QString::fromStdString(material.color_texture_path));
    light_texture_edit_->setText(QString::fromStdString(material.light_texture_path));
    bump_texture_edit_->setText(QString::fromStdString(material.bump_texture_path));
    normal_texture_edit_->setText(QString::fromStdString(material.normal_texture_path));
    roughness_texture_edit_->setText(QString::fromStdString(material.roughness_texture_path));
    metallic_texture_edit_->setText(QString::fromStdString(material.metallic_texture_path));
    displacement_texture_edit_->setText(QString::fromStdString(material.displacement_texture_path));
    texture_offset_u_spin_->setValue(material.texture_offset_u);
    texture_offset_v_spin_->setValue(material.texture_offset_v);
    texture_scale_u_spin_->setValue(material.texture_scale_u);
    texture_scale_v_spin_->setValue(material.texture_scale_v);
    texture_rotation_spin_->setValue(material.texture_rotation_degrees);
    {
        const double normalized = std::fmod(
            std::fmod(static_cast<double>(material.texture_rotation_degrees), 360.0) + 360.0,
            360.0);
        texture_rotate_90_check_->setChecked(std::abs(normalized - 90.0) < 0.001);
    }
    texture_fit_to_surface_check_->setChecked(material.texture_fit_to_surface);
    UpdateCoatingControls(material, candidate_path);
    loading_editor_ = false;
}

void MaterialEditorDialog::UpdateCoatingControls(
    const Material& material, const QString& source_path) {
    const QString identity = source_path + " "
        + QString::fromStdString(material.name);
    if (identity.contains("Powder Coating", Qt::CaseInsensitive)) {
        coating_family_ = 1;
    } else if (identity.contains("Oracal Film", Qt::CaseInsensitive)) {
        coating_family_ = 2;
    } else {
        coating_family_ = 0;
    }

    coating_group_->setVisible(coating_family_ != 0);
    if (coating_family_ == 0) {
        return;
    }
    coating_group_->setTitle(
        coating_family_ == 1 ? "Powder coating" : "Oracal film");
    ral_combo_->setCurrentIndex(closest_ral_index(material.diffuse));
    lacquered_check_->setVisible(coating_family_ == 1);
    film_type_label_->setVisible(coating_family_ == 2);
    film_type_combo_->setVisible(coating_family_ == 2);
    lacquered_check_->setChecked(material.coat_weight > 0.4f);
    film_type_combo_->setCurrentIndex(material.alpha < 0.99f ? 1 : 0);
}

void MaterialEditorDialog::ApplyCoatingControls() {
    if (loading_editor_ || coating_family_ == 0
        || ral_combo_->currentIndex() < 0) {
        return;
    }

    loading_editor_ = true;
    // A RAL/finish change creates a document variant.  Keep the shipped
    // library swatch untouched so all 24 reference colours remain available.
    if (!current_file_path_.isEmpty()) {
        current_file_path_.clear();
        selected_document_material_id_ = 0;
        id_edit_->clear();
    }
    const int ral_index = std::clamp(
        ral_combo_->currentIndex(), 0,
        static_cast<int>(std::size(kRalColors)) - 1);
    const RalColor& ral = kRalColors[ral_index];
    SetColorButton(diffuse_button_, ral.color);
    SetColorButton(ambient_button_, {
        ral.color.r * 0.22f,
        ral.color.g * 0.22f,
        ral.color.b * 0.22f});

    QString name;
    if (coating_family_ == 1) {
        const bool lacquered = lacquered_check_->isChecked();
        name = QString("Powder Coating RAL %1 %2%3")
            .arg(ral.code, ral.name,
                 lacquered ? " Lacquered" : "");
        alpha_spin_->setValue(1.0);
        specular_spin_->setValue(lacquered ? 0.82 : 0.32);
        shininess_spin_->setValue(lacquered ? 110.0 : 42.0);
        reflectivity_spin_->setValue(lacquered ? 0.14 : 0.04);
        roughness_spin_->setValue(lacquered ? 0.19 : 0.64);
        metallic_spin_->setValue(0.0);
        coat_weight_spin_->setValue(lacquered ? 0.88 : 0.0);
        coat_roughness_spin_->setValue(lacquered ? 0.055 : 0.12);
        normal_strength_spin_->setValue(0.12);
        displacement_scale_spin_->setValue(0.0);
        normal_texture_edit_->setText(
            "Powder Coating/Textures/Powder_Wrinkle_NormalGL.png");
        texture_scale_u_spin_->setValue(0.4);
        texture_scale_v_spin_->setValue(0.4);
    } else {
        const bool translucent = film_type_combo_->currentData().toInt() == 1;
        name = QString("Oracal Film %1 RAL %2")
            .arg(translucent ? "Translucent Glossy" : "Matte",
                 ral.code);
        alpha_spin_->setValue(translucent ? 0.58 : 1.0);
        specular_spin_->setValue(translucent ? 0.70 : 0.28);
        shininess_spin_->setValue(translucent ? 96.0 : 38.0);
        reflectivity_spin_->setValue(translucent ? 0.12 : 0.03);
        roughness_spin_->setValue(translucent ? 0.18 : 0.68);
        metallic_spin_->setValue(0.0);
        coat_weight_spin_->setValue(translucent ? 0.72 : 0.0);
        coat_roughness_spin_->setValue(translucent ? 0.055 : 0.12);
        normal_strength_spin_->setValue(0.0);
        displacement_scale_spin_->setValue(0.0);
        color_texture_edit_->clear();
        normal_texture_edit_->clear();
        roughness_texture_edit_->clear();
        metallic_texture_edit_->clear();
        displacement_texture_edit_->clear();
    }
    name_edit_->setText(name);
    loading_editor_ = false;
    CommitEditorChanges();
}

void MaterialEditorDialog::SetCurrentMaterial(const Material& material, const QString& file_path) {
    bool replaced = false;
    if (material.id != 0) {
        for (Material& existing : document_materials_) {
            if (existing.id == material.id) {
                existing = material;
                replaced = true;
                break;
            }
        }
    }
    if (!replaced) {
        document_materials_.push_back(material);
    }
    LoadMaterialToEditor(material, file_path);
    PopulateDocumentMaterials();
}

Material MaterialEditorDialog::EditorMaterial() const {
    Material material;
    material.name = name_edit_->text().trimmed().toStdString();
    material.id = selected_document_material_id_;
    material.ambient = ButtonColor(ambient_button_);
    material.diffuse = ButtonColor(diffuse_button_);
    material.emission = ButtonColor(emission_button_);
    material.alpha = static_cast<float>(alpha_spin_->value());
    material.specular = static_cast<float>(specular_spin_->value());
    material.shininess = static_cast<float>(shininess_spin_->value());
    material.reflectivity = static_cast<float>(reflectivity_spin_->value());
    material.roughness = static_cast<float>(roughness_spin_->value());
    material.metallic = static_cast<float>(metallic_spin_->value());
    material.coat_weight = static_cast<float>(coat_weight_spin_->value());
    material.coat_roughness = static_cast<float>(coat_roughness_spin_->value());
    material.normal_strength = static_cast<float>(normal_strength_spin_->value());
    material.displacement_scale = static_cast<float>(displacement_scale_spin_->value());
    material.color_texture_path = color_texture_edit_->text().trimmed().toStdString();
    material.light_texture_path = light_texture_edit_->text().trimmed().toStdString();
    material.bump_texture_path = bump_texture_edit_->text().trimmed().toStdString();
    material.normal_texture_path = normal_texture_edit_->text().trimmed().toStdString();
    material.roughness_texture_path = roughness_texture_edit_->text().trimmed().toStdString();
    material.metallic_texture_path = metallic_texture_edit_->text().trimmed().toStdString();
    material.displacement_texture_path = displacement_texture_edit_->text().trimmed().toStdString();
    material.texture_offset_u = static_cast<float>(texture_offset_u_spin_->value());
    material.texture_offset_v = static_cast<float>(texture_offset_v_spin_->value());
    material.texture_scale_u = static_cast<float>(texture_scale_u_spin_->value());
    material.texture_scale_v = static_cast<float>(texture_scale_v_spin_->value());
    material.texture_rotation_degrees = static_cast<float>(texture_rotation_spin_->value());
    material.texture_fit_to_surface = texture_fit_to_surface_check_->isChecked();
    material.source_file_path = current_file_path_.toStdString();
    return material;
}

void MaterialEditorDialog::SetColorButton(QPushButton* button, Color color) {
    button->setProperty("materialColor", QColor::fromRgbF(color.r, color.g, color.b));
    button->setStyleSheet(color_style(color));
}

Color MaterialEditorDialog::ButtonColor(QPushButton* button) const {
    return color_from_button(button);
}

void MaterialEditorDialog::PickColor(QPushButton* button) {
    const Color current = ButtonColor(button);
    const QColor picked = QColorDialog::getColor(QColor::fromRgbF(current.r, current.g, current.b), this, "Material Color");
    if (!picked.isValid()) {
        return;
    }
    SetColorButton(button, {static_cast<float>(picked.redF()), static_cast<float>(picked.greenF()), static_cast<float>(picked.blueF())});
    CommitEditorChanges();
}

void MaterialEditorDialog::BrowseTexture(QLineEdit* edit) {
    const QString start_dir = texture_library_path();
    const QString path = QFileDialog::getOpenFileName(this, "Select Texture", start_dir, "Images (*.png *.jpg *.jpeg *.bmp *.tga);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }
    edit->setText(QDir(library_path_).relativeFilePath(path));
    CommitEditorChanges();
}

void MaterialEditorDialog::CreateNewMaterial() {
    unsigned long next_id = 1;
    for (const Material& existing : document_materials_) {
        next_id = std::max(next_id, existing.id + 1);
    }

    Material material = Material::DefaultWhite();
    material.id = next_id;
    material.name = QString("Material %1").arg(next_id).toStdString();
    document_materials_.push_back(material);
    emit SaveMaterialToDocument(material);
    PopulateTree();
    LoadMaterialToEditor(material, {});
    SelectDocumentMaterialById(material.id);
}

void MaterialEditorDialog::ExportCurrentMaterial() {
    Material material = EditorMaterial();
    if (material.name.empty()) {
        material.name = "Material";
    }

    QString default_name = QString::fromStdString(material.name);
    default_name.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
    const QString start_path = QDir(library_path_).filePath(default_name + ".d3mat");
    QString path = QFileDialog::getSaveFileName(this, "Export Material", start_path, "Dom3D Material (*.d3mat);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += ".d3mat";
    }

    QString error;
    if (!library_.SaveMaterial(path, material, &error)) {
        QMessageBox::critical(this, "Export Material", error);
        return;
    }
}

void MaterialEditorDialog::ImportMaterialFromFile() {
    const QString path = QFileDialog::getOpenFileName(this, "Import Material", library_path_, "Dom3D Material (*.d3mat);;All files (*.*)");
    if (path.isEmpty()) {
        return;
    }

    Material material;
    QString error;
    if (!library_.LoadMaterial(path, material, &error)) {
        QMessageBox::critical(this, "Import Material", error);
        return;
    }

    if (material.id == 0) {
        unsigned long next_id = 1;
        for (const Material& existing : document_materials_) {
            next_id = std::max(next_id, existing.id + 1);
        }
        material.id = next_id;
    }
    SetCurrentMaterial(material, path);
    emit SaveMaterialToDocument(material);
}

void MaterialEditorDialog::SaveCurrentMaterial() {
    CommitEditorChanges();
}

void MaterialEditorDialog::CommitEditorChanges() {
    if (loading_editor_) {
        return;
    }

    Material material = EditorMaterial();
    if (material.id == 0) {
        unsigned long next_id = 1;
        for (const Material& existing : document_materials_) {
            next_id = std::max(next_id, existing.id + 1);
        }
        material.id = next_id;
        id_edit_->setText(QString::number(material.id));
        selected_document_material_id_ = material.id;
    }
    material.source_file_path = current_file_path_.toStdString();

    bool replaced = false;
    for (Material& existing : document_materials_) {
        if (existing.id == material.id && material.id != 0) {
            existing = material;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        document_materials_.push_back(material);
    }

    if (!current_file_path_.isEmpty()
        && QFileInfo(current_file_path_).suffix().compare("d3mat", Qt::CaseInsensitive) == 0) {
        QString error;
        if (!library_.SaveMaterial(current_file_path_, material, &error)) {
            QMessageBox::critical(this, "Save Material", error);
            return;
        }
        emit LibraryMaterialSaved(current_file_path_);
    }

    emit SaveMaterialToDocument(material);
    SelectDocumentMaterialById(material.id);
}

void MaterialEditorDialog::ApplyCurrentMaterial() {
    emit ApplyMaterialToSelected(EditorMaterial());
}
