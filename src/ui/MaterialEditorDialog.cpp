#include "../materials/ProceduralMaterialIO.h"
#include "MaterialPreviewGL.h"
#include "../materials/PlasterBaker.h"
#include <QProgressDialog>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QRandomGenerator>
#include <QTimer>
#include <QTabWidget>
#include <QToolButton>
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
#include <QPainterPath>
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

// Named RGB palette from the original Dom material editor.
struct NamedMaterialColor { const char* name; unsigned int rgb; };
constexpr NamedMaterialColor kNamedMaterialColors[] = {
    {"aliceblue", 0xF0F8FF},
    {"antiquewhite", 0xFAEBD7},
    {"aqua", 0x00FFFF},
    {"aquamarine", 0x7FFFD4},
    {"azure", 0xF0FFFF},
    {"beige", 0xF5F5DC},
    {"bisque", 0xFFE4C4},
    {"black", 0x000000},
    {"blanchedalmond", 0xFFEBCD},
    {"blue", 0x0000FF},
    {"blueviolet", 0x8A2BE2},
    {"brown", 0xA52A2A},
    {"burlywood", 0xDEB887},
    {"cadetblue", 0x5F9EA0},
    {"chartreuse", 0x7FFF00},
    {"chocolate", 0xD2691E},
    {"coral", 0xFF7F50},
    {"cornflowerblue", 0x6495ED},
    {"cornsilk", 0xFFF8DC},
    {"crimson", 0xDC143C},
    {"cyan", 0x00FFFF},
    {"darkblue", 0x00008B},
    {"darkcyan", 0x008B8B},
    {"darkgoldenrod", 0xB8860B},
    {"darkgray", 0xA9A9A9},
    {"darkgreen", 0x006400},
    {"darkkhaki", 0xBDB76B},
    {"darkmagenta", 0x8B008B},
    {"darkolivegreen", 0x556B2F},
    {"darkorange", 0xFF8C00},
    {"darkorchid", 0x9932CC},
    {"darkred", 0x8B0000},
    {"darksalmon", 0xE9967A},
    {"darkseagreen", 0x8FBC8F},
    {"darkslateblue", 0x483D8B},
    {"darkslategray", 0x2F4F4F},
    {"darkturquoise", 0x00CED1},
    {"darkviolet", 0x9400D3},
    {"deeppink", 0xFF1493},
    {"deepskyblue", 0x00BFFF},
    {"dimgray", 0x696969},
    {"dodgerblue", 0x1E90FF},
    {"firebrick", 0xB22222},
    {"floralwhite", 0xFFFAF0},
    {"forestgreen", 0x228B22},
    {"fuchsia", 0xFF00FF},
    {"gainsboro", 0xDCDCDC},
    {"ghostwhite", 0xF8F8FF},
    {"gold", 0xFFD700},
    {"goldenrod", 0xDAA520},
    {"gray", 0x808080},
    {"green", 0x008000},
    {"greenyellow", 0xADFF2F},
    {"honeydew", 0xF0FFF0},
    {"hotpink", 0xFF69B4},
    {"indianred", 0xCD5C5C},
    {"indigo", 0x4B0082},
    {"ivory", 0xFFFFF0},
    {"khaki", 0xF0E68C},
    {"lavender", 0xE6E6FA},
    {"lavenderblush", 0xFFF0F5},
    {"lawngreen", 0x7CFC00},
    {"lemonchiffon", 0xFFFACD},
    {"lightblue", 0xADD8E6},
    {"lightcoral", 0xF08080},
    {"lightcyan", 0xE0FFFF},
    {"lightgoldenrodyellow", 0xFAFAD2},
    {"lightgreen", 0x90EE90},
    {"lightgrey", 0xD3D3D3},
    {"lightpink", 0xFFB6C1},
    {"lightsalmon", 0xFFA07A},
    {"lightseagreen", 0x20B2AA},
    {"lightskyblue", 0x87CEFA},
    {"lightslategray", 0x778899},
    {"lightsteelblue", 0xB0C4DE},
    {"lightyellow", 0xFFFFE0},
    {"lime", 0x00FF00},
    {"limegreen", 0x32CD32},
    {"linen", 0xFAF0E6},
    {"magenta", 0xFF00FF},
    {"maroon", 0x800000},
    {"mediumaquamarine", 0x66CDAA},
    {"mediumblue", 0x0000CD},
    {"mediumorchid", 0xBA55D3},
    {"mediumpurple", 0x9370DB},
    {"mediumseagreen", 0x3CB371},
    {"mediumslateblue", 0x7B68EE},
    {"mediumspringgreen", 0x00FA9A},
    {"mediumturquoise", 0x48D1CC},
    {"mediumvioletred", 0xC71585},
    {"midnightblue", 0x191970},
    {"mintcream", 0xF5FFFA},
    {"mistyrose", 0xFFE4E1},
    {"moccasin", 0xFFE4B5},
    {"navajowhite", 0xFFDEAD},
    {"navy", 0x000080},
    {"oldlace", 0xFDF5E6},
    {"olive", 0x808000},
    {"olivedrab", 0x6B8E23},
    {"orange", 0xFFA500},
    {"orangered", 0xFF4500},
    {"orchid", 0xDA70D6},
    {"palegoldenrod", 0xEEE8AA},
    {"palegreen", 0x98FB98},
    {"paleturquoise", 0xAFEEEE},
    {"palevioletred", 0xDB7093},
    {"papayawhip", 0xFFEFD5},
    {"peachpuff", 0xFFDAB9},
    {"peru", 0xCD853F},
    {"pink", 0xFFC0CB},
    {"plum", 0xDDA0DD},
    {"powderblue", 0xB0E0E6},
    {"purple", 0x800080},
    {"red", 0xFF0000},
    {"rosybrown", 0xBC8F8F},
    {"royalblue", 0x4169E1},
    {"saddlebrown", 0x8B4513},
    {"salmon", 0xFA8072},
    {"sandybrown", 0xF4A460},
    {"seagreen", 0x2E8B57},
    {"seashell", 0xFFF5EE},
    {"sienna", 0xA0522D},
    {"silver", 0xC0C0C0},
    {"skyblue", 0x87CEEB},
    {"slateblue", 0x6A5ACD},
    {"slategray", 0x708090},
    {"snow", 0xFFFAFA},
    {"springgreen", 0x00FF7F},
    {"steelblue", 0x4682B4},
    {"tan", 0xD2B48C},
    {"teal", 0x008080},
    {"thistle", 0xD8BFD8},
    {"tomato", 0xFF6347},
    {"turquoise", 0x40E0D0},
    {"violet", 0xEE82EE},
    {"wheat", 0xF5DEB3},
    {"white", 0xFFFFFF},
    {"whitesmoke", 0xF5F5F5},
    {"yellow", 0xFFFF00},
    {"yellowgreen", 0x9ACD32},
};

struct RalColor {
    const char* code;
    const char* name;
    Color color;
};

#include "RalClassicColors.inc"

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
    return MaterialDrag::SpherePixmap(material,size,selected);
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
    QPixmap pixmap(168, 112);
    pixmap.setDevicePixelRatio(4);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    // A light CAD face and a coloured stroke make the target explicit.
    painter.setPen(QPen(QColor(55, 115, 170), 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(227, 243, 252));
    painter.drawPolygon(QPolygonF({{3,15},{19,11},{30,21},{13,26}}));
    painter.setPen(QPen(QColor(0, 155, 207), 4, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(10,21),QPointF(20,18));

    // One coherent brush silhouette: tapered handle, metal ferrule, bristles.
    painter.setPen(QPen(QColor(108, 63, 24), 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(245, 165, 55));
    QPainterPath handle;
    handle.moveTo(23,12);handle.lineTo(32,2);
    handle.cubicTo(34,0,38,3,36,5);
    handle.lineTo(27,15);handle.closeSubpath();
    painter.drawPath(handle);
    painter.setPen(QPen(QColor(62, 79, 95), 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(216, 227, 235));
    painter.drawPolygon(QPolygonF({{23,11},{28,15},{24,19},{19,15}}));
    painter.setBrush(QColor(0, 155, 207));
    QPainterPath bristles;
    bristles.moveTo(19,15);bristles.lineTo(24,19);
    bristles.cubicTo(21,23,18,24,13,23);
    bristles.cubicTo(16,21,15,18,19,15);bristles.closeSubpath();
    painter.drawPath(bristles);
    painter.setPen(QPen(QColor(168, 230, 250), 1, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(19,19),QPointF(17,22));
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
    auto* ral_button = new QPushButton("RAL...", action_buttons_row);
    color_button->setObjectName("NamedMaterialColorButton");
    ral_button->setObjectName("RalMaterialColorButton");
    for (auto* button : {color_button, ral_button})
        button->setFixedWidth(button->fontMetrics().horizontalAdvance("Color...") + 24);
    ral_button->setToolTip("RAL Classic color chart");
    auto* pipette_button = new QPushButton(action_buttons_row);
    brush_button->setIcon(brush_icon());
    brush_button->setIconSize(QSize(42, 28));
    brush_button->setToolTip("Paint a surface under the cursor with the current material");
    pipette_button->setIcon(pipette_icon());
    pipette_button->setIconSize(QSize(42, 28));
    pipette_button->setToolTip("Pick material from selected object");
    for (auto* button : {brush_button, pipette_button})
        button->setFixedWidth(48);
    apply_button_->setEnabled(has_selection);
    action_buttons_layout->addWidget(apply_button_);
    action_buttons_layout->addWidget(brush_button);
    action_buttons_layout->addWidget(color_button);
    action_buttons_layout->addWidget(ral_button);
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

    auto* editor_scroll = new QScrollArea(splitter);
    editor_scroll->setWidgetResizable(true);
    editor_scroll->setFrameShape(QFrame::NoFrame);
    editor_scroll->setMinimumWidth(380);
    auto* editor = new QWidget(editor_scroll);
    auto* editor_layout = new QVBoxLayout(editor);
    editor_scroll->setWidget(editor);
    splitter->addWidget(editor_scroll);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    editor_layout->addLayout(form);

    auto* advanced_toggle = new QToolButton(editor);
    advanced_toggle->setObjectName("MaterialAdvancedToggle");
    advanced_toggle->setText("Advanced");
    advanced_toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    advanced_toggle->setArrowType(Qt::RightArrow);
    advanced_toggle->setCheckable(true);
    advanced_toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    editor_layout->addWidget(advanced_toggle);
    auto* advanced = new QWidget(editor);
    advanced->setObjectName("MaterialAdvancedSettings");
    auto* advanced_layout = new QVBoxLayout(advanced);
    advanced_layout->setContentsMargins(0, 0, 0, 0);
    auto* advanced_form = new QFormLayout();
    advanced_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    advanced_layout->addLayout(advanced_form);
    editor_layout->addWidget(advanced);
    advanced->hide();
    connect(advanced_toggle, &QToolButton::toggled, this, [advanced, advanced_toggle](bool expanded) {
        advanced->setVisible(expanded);
        advanced_toggle->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    });

    name_edit_ = new QLineEdit(editor);
    id_edit_ = new QLineEdit(editor);
    form->addRow("Name", name_edit_);
    auto* plasterButton=new QPushButton("Procedural Plaster...",editor);
    plasterButton->setObjectName("ProceduralPlasterButton");advanced_form->addRow("Surface source",plasterButton);
    connect(plasterButton,&QPushButton::clicked,this,[this,plasterButton](){
        QDialog dialog(this);dialog.setWindowTitle("Procedural Plaster");dialog.resize(820,760);
        auto* layout=new QVBoxLayout(&dialog);
        auto* enabled=new QCheckBox("Use Procedural Plaster",&dialog);layout->addWidget(enabled);
        Material initial=EditorMaterial();enabled->setChecked(initial.plaster.enabled);
        auto* images=new QHBoxLayout;auto* preview=new QLabel(&dialog);
        preview->setFixedSize(280,280);
        auto* presets=new QListWidget(&dialog);presets->setObjectName("plaster_presets");
        presets->setViewMode(QListView::IconMode);presets->setResizeMode(QListView::Adjust);
        presets->setMovement(QListView::Static);presets->setIconSize(QSize(106,82));presets->setGridSize(QSize(116,132));
        presets->setMinimumSize(490,280);presets->setWordWrap(true);
        presets->setStyleSheet("QListWidget {background:#f4f4f4;} QListWidget::item {color:#202020;border:2px solid transparent;} QListWidget::item:selected {border-color:#1687ef;background:#deebfa;}");
        const QStringList names={"Fine / Мелкая","1. Grooves / Борозды","2. Islands / Островки","3. Swirls / Завитки","4. Cross / Перекрёстная","5. Trowel / Затирка","6. Folds / Складки"};
        for(int i=0;i<names.size();++i){Material sample;sample.diffuse={.78f,.73f,.64f};sample.plaster=PlasterPreset(i);
            auto* item=new QListWidgetItem(QIcon(QPixmap::fromImage(RenderMaterialSphereGL(sample,480,true).scaled(144,144,Qt::KeepAspectRatio,Qt::SmoothTransformation))),names[i],presets);
            item->setToolTip("Default appearance / Вид с настройками по умолчанию");}
        presets->setCurrentRow(int(initial.plaster.pattern));
        images->addWidget(preview);images->addWidget(presets,1);layout->addLayout(images);
        auto* presetNote=new QLabel("Choose a type to reset its settings. Base color is preserved. / Выбор вида сбрасывает настройки рисунка, сохраняя цвет.",&dialog);
        presetNote->setWordWrap(true);layout->addWidget(presetNote);
        auto* tabs=new QTabWidget(&dialog);layout->addWidget(tabs);
        auto* surface=new QWidget(tabs);auto* advanced=new QWidget(tabs);
        auto* surfaceForm=new QFormLayout(surface);auto* advancedForm=new QFormLayout(advanced);
        auto addTab=[&](QWidget* widget,const char* title){auto* scroll=new QScrollArea(tabs);scroll->setWidgetResizable(true);scroll->setWidget(widget);tabs->addTab(scroll,title);};
        addTab(surface,"Surface");addTab(advanced,"Advanced");
#define CONTROL(name,value,lo,hi,label) {auto* spin=new QDoubleSpinBox(&dialog);spin->setObjectName("plaster_" #name);spin->setDecimals(3);spin->setRange(lo,hi);spin->setSingleStep(((hi)-(lo))/100);spin->setValue(initial.plaster.name);auto* f=(QString(#name).startsWith("macro")||QString(#name).startsWith("micro"))?advancedForm:surfaceForm;f->addRow(label,spin);}
        DOM_PLASTER_PARAMETERS(CONTROL)
#undef CONTROL
        auto* patternControl=dialog.findChild<QDoubleSpinBox*>("plaster_pattern");
        if(auto* label=surfaceForm->labelForField(patternControl))label->hide();patternControl->hide();
        auto showPatternControls=[&](int type){
            for(const char* name:{"patternSize","patternDepth","patternDensity","patternStretch","patternAngle"}){
                auto* control=dialog.findChild<QDoubleSpinBox*>(QString("plaster_")+name);
                const bool show=type>0 && (QString(name)!="patternStretch" || type==1 || type==4);
                surfaceForm->setRowVisible(control,show);
            }
        };
        showPatternControls(int(initial.plaster.pattern));
        dialog.findChild<QDoubleSpinBox*>("plaster_patternAngle")->setSingleStep(1);
        // Scale is a multiplier: a range-derived step was almost 1.0 and
        // doubled the default grain size with a single click.
        dialog.findChild<QDoubleSpinBox*>("plaster_scale")->setSingleStep(0.01);
        auto* seed=new QDoubleSpinBox(&dialog);seed->setDecimals(0);seed->setRange(0,4294967295.0);seed->setValue(initial.plaster.seed);surfaceForm->addRow("Seed",seed);
        auto* high=new QCheckBox("High quality",&dialog);high->setChecked(initial.plaster.highQuality);surfaceForm->addRow(high);
        auto* random=new QPushButton("Randomize Seed",&dialog);surfaceForm->addRow(random);
        connect(random,&QPushButton::clicked,&dialog,[&](){seed->setValue(QRandomGenerator::global()->generate());});
        auto* color=new QPushButton("Base Color...",&dialog);surfaceForm->addRow(color);
        QTimer timer(&dialog);timer.setSingleShot(true);timer.setInterval(120);
        auto refresh=[&](){
            Material material=EditorMaterial();material.plaster.enabled=enabled->isChecked();material.plaster.seed=std::uint32_t(seed->value());material.plaster.highQuality=high->isChecked();
#define READ_CONTROL(name,defaultValue,lo,hi,label) material.plaster.name=float(dialog.findChild<QDoubleSpinBox*>("plaster_" #name)->value());
            DOM_PLASTER_PARAMETERS(READ_CONTROL)
#undef READ_CONTROL
            plasterButton->setProperty("configuration",EncodePlaster(material));
            if(material.plaster.enabled){findChild<QPushButton*>("ProceduralPerforationButton")->setProperty("configuration",QString());material.fabric.enabled=false;material.metallic=0;metallic_spin_->setValue(0);
                findChild<QPushButton*>("ProceduralFabricButton")->setProperty("configuration",QString());}
            preview->setPixmap(QPixmap::fromImage(RenderMaterialSphereGL(material,280)));
            CommitEditorChanges();
        };
        connect(&timer,&QTimer::timeout,&dialog,refresh);
        connect(presets,&QListWidget::currentRowChanged,&dialog,[&](int index){
            if(index<0)return;const auto p=PlasterPreset(index);
            showPatternControls(index);
#define SET_PLASTER_CONTROL(name,def,lo,hi,label) {auto* spin=dialog.findChild<QDoubleSpinBox*>("plaster_" #name);const QSignalBlocker block(spin);spin->setValue(p.name);}
            DOM_PLASTER_PARAMETERS(SET_PLASTER_CONTROL)
#undef SET_PLASTER_CONTROL
            const Color savedColor=ButtonColor(diffuse_button_);
            high->setChecked(p.highQuality);enabled->setChecked(true);SetColorButton(diffuse_button_,savedColor);timer.start();
        });
        for(auto* spin:dialog.findChildren<QDoubleSpinBox*>())connect(spin,qOverload<double>(&QDoubleSpinBox::valueChanged),&dialog,[&](){timer.start();});
        connect(high,&QCheckBox::toggled,&dialog,[&](){timer.start();});
        connect(enabled,&QCheckBox::toggled,&dialog,[&](bool on){if(on&&!initial.plaster.enabled)SetColorButton(diffuse_button_,{.84f,.84f,.82f});timer.start();});
        connect(color,&QPushButton::clicked,&dialog,[&](){PickColor(diffuse_button_);timer.start();});
        auto* bake=new QPushButton("Export PBR Maps...",&dialog);
        bake->setObjectName("ExportPlasterMapsButton");layout->addWidget(bake);bake->setEnabled(enabled->isChecked());
        connect(enabled,&QCheckBox::toggled,bake,&QWidget::setEnabled);
        connect(bake,&QPushButton::clicked,&dialog,[&](){
            timer.stop();refresh();
            QDialog options(&dialog);options.setWindowTitle("Export Plaster PBR Maps");
            auto* form=new QFormLayout(&options);
            auto* resolution=new QComboBox(&options);
            for(int n:{256,512,1024,2048,4096})resolution->addItem(QString::number(n)+" x "+QString::number(n),n);
            resolution->setCurrentIndex(2);form->addRow("Resolution",resolution);
            auto dimension=[&](const QString& title,double value,double minimum,double maximum){
                auto* spin=new QDoubleSpinBox(&options);spin->setDecimals(3);spin->setRange(minimum,maximum);spin->setValue(value);form->addRow(title,spin);return spin;};
            auto* width=dimension("Width (mm)",100,.1,100000);
            auto* height=dimension("Height (mm)",100,.1,100000);
            auto* x=dimension("Origin X (mm)",0,-100000,100000);
            auto* y=dimension("Origin Y (mm)",0,-100000,100000);
            auto* z=dimension("Origin Z (mm)",0,-100000,100000);
            auto* note=new QLabel("XY surface. Uses the current Seed and quality.\nExports a finite region; edges are not seamless.\nHeight: 16-bit PNG. Normals: OpenGL and DirectX.",&options);
            form->addRow(note);
            auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&options);form->addRow(buttons);
            connect(buttons,&QDialogButtonBox::accepted,&options,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&options,&QDialog::reject);
            if(options.exec()!=QDialog::Accepted)return;
            const QString directory=QFileDialog::getExistingDirectory(&dialog,"Export PBR maps into a new folder");if(directory.isEmpty())return;
            PlasterBakeSettings settings;settings.resolution=resolution->currentData().toInt();settings.widthMm=float(width->value());settings.heightMm=float(height->value());
            settings.originMm={float(x->value()),float(y->value()),float(z->value())};
            QProgressDialog progress("Baking PBR maps...","Cancel",0,100,&dialog);progress.setWindowModality(Qt::WindowModal);progress.setMinimumDuration(0);progress.setAutoClose(false);
            QString package,error;
            const bool ok=BakePlasterMaps(EditorMaterial(),settings,directory,package,error,[&](int value,const QString& label){
                progress.setLabelText(label);progress.setValue(value);QApplication::processEvents();return !progress.wasCanceled();});
            progress.close();
            if(ok)QMessageBox::information(&dialog,"PBR Maps", "Exported maps and FinePlaster.d3mat:\n"+QDir::toNativeSeparators(package));
            else if(error!="Canceled.")QMessageBox::warning(&dialog,"PBR Export",error);
        });
        auto* close=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::accept);
        refresh();dialog.exec();if(timer.isActive()){timer.stop();refresh();}
    });

    auto* fabricButton=new QPushButton("Procedural Fabric...",editor);
    fabricButton->setObjectName("ProceduralFabricButton");advanced_form->addRow("Surface source",fabricButton);
    connect(fabricButton,&QPushButton::clicked,this,[this,fabricButton](){
        QDialog dialog(this);dialog.setWindowTitle("Procedural Fabric");dialog.resize(480,720);
        auto* layout=new QVBoxLayout(&dialog);
        auto* enabled=new QCheckBox("Use Procedural Fabric",&dialog);enabled->setObjectName("fabric_enabled");layout->addWidget(enabled);
        const Material initial=EditorMaterial();enabled->setChecked(initial.fabric.enabled);
        auto* preview=new QLabel(&dialog);preview->setFixedSize(280,280);layout->addWidget(preview,0,Qt::AlignHCenter);
        auto* form=new QFormLayout;layout->addLayout(form);
        auto* preset=new QComboBox(&dialog);preset->setObjectName("fabric_preset");
        preset->addItems({"Linen / Лён","Cotton / Хлопок","Twill / Саржа","Burlap / Мешковина"});
        preset->setCurrentIndex(initial.fabric.weave);form->addRow("Type / Preset",preset);
        auto* useUV=new QCheckBox("Follow texture UV (color + weave)",&dialog);
        useUV->setObjectName("fabric_useUV");useUV->setChecked(initial.fabric.useUV);form->addRow(useUV);
#define FABRIC_CONTROL(name,def,lo,hi,label) {auto* spin=new QDoubleSpinBox(&dialog);spin->setObjectName("fabric_" #name);spin->setDecimals(3);spin->setRange(lo,hi);spin->setSingleStep(QString(#name)=="rotation"?1.0:0.01);spin->setValue(initial.fabric.name);form->addRow(label,spin);}
        DOM_FABRIC_PARAMETERS(FABRIC_CONTROL)
#undef FABRIC_CONTROL
        auto* seed=new QDoubleSpinBox(&dialog);seed->setDecimals(0);seed->setRange(0,4294967295.0);seed->setValue(initial.fabric.seed);form->addRow("Seed",seed);
        auto* color=new QPushButton("Base Color...",&dialog);form->addRow(color);
        auto* note=new QLabel("Thread spacing is in millimeters. Presets reset the weave settings; your base color is preserved.",&dialog);note->setWordWrap(true);layout->addWidget(note);
        QTimer timer(&dialog);timer.setSingleShot(true);timer.setInterval(120);
        auto refresh=[&](){
            Material material=EditorMaterial();material.fabric.enabled=enabled->isChecked();
            material.fabric.useUV=useUV->isChecked();
            material.fabric.weave=preset->currentIndex();material.fabric.seed=std::uint32_t(seed->value());
#define READ_FABRIC_CONTROL(name,def,lo,hi,label) material.fabric.name=float(dialog.findChild<QDoubleSpinBox*>("fabric_" #name)->value());
            DOM_FABRIC_PARAMETERS(READ_FABRIC_CONTROL)
#undef READ_FABRIC_CONTROL
            if(material.fabric.enabled){findChild<QPushButton*>("ProceduralPerforationButton")->setProperty("configuration",QString());material.plaster.enabled=false;material.metallic=0;metallic_spin_->setValue(0);
                findChild<QPushButton*>("ProceduralPlasterButton")->setProperty("configuration",QString());}
            fabricButton->setProperty("configuration",EncodeFabric(material));
            preview->setPixmap(QPixmap::fromImage(RenderMaterialSphereGL(material,280)));CommitEditorChanges();
        };
        connect(&timer,&QTimer::timeout,&dialog,refresh);
        for(auto* spin:dialog.findChildren<QDoubleSpinBox*>())connect(spin,qOverload<double>(&QDoubleSpinBox::valueChanged),&dialog,[&](){timer.start();});
        connect(enabled,&QCheckBox::toggled,&dialog,[&](){timer.start();});
        connect(useUV,&QCheckBox::toggled,&dialog,[&](){timer.start();});
        connect(color,&QPushButton::clicked,&dialog,[&](){PickColor(diffuse_button_);timer.start();});
        connect(preset,qOverload<int>(&QComboBox::activated),&dialog,[&](int index){
            const auto p=FabricPreset(index);
#define SET_FABRIC_CONTROL(name,def,lo,hi,label) dialog.findChild<QDoubleSpinBox*>("fabric_" #name)->setValue(p.name);
            DOM_FABRIC_PARAMETERS(SET_FABRIC_CONTROL)
#undef SET_FABRIC_CONTROL
            enabled->setChecked(true);timer.start();
        });
        auto* close=new QDialogButtonBox(QDialogButtonBox::Close,&dialog);layout->addWidget(close);
        connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::accept);
        refresh();dialog.exec();if(timer.isActive()){timer.stop();refresh();}
    });

    auto* perforationButton=new QPushButton("Perforation / Honeycomb...",editor);
    perforationButton->setObjectName("ProceduralPerforationButton");
    advanced_form->addRow("Surface source",perforationButton);
    connect(perforationButton,&QPushButton::clicked,this,[this](){
        QDialog dialog(this);dialog.setWindowTitle("Perforation / Honeycomb");dialog.resize(410,620);
        auto* layout=new QVBoxLayout(&dialog);
        auto* preview=new QLabel(&dialog);preview->setFixedSize(280,280);layout->addWidget(preview,0,Qt::AlignHCenter);
        Material draft=EditorMaterial();
        if(!draft.perforation.enabled){draft.perforation.enabled=true;draft.roughness=.25f;draft.metallic=.9f;}
        auto* enabled=new QCheckBox("Use perforation",&dialog);enabled->setChecked(true);layout->addWidget(enabled);
        auto* form=new QFormLayout();layout->addLayout(form);
        auto* pattern=new QComboBox(&dialog);pattern->setObjectName("perforation_pattern");
        pattern->addItems({"Perforation / Перфорация","Honeycomb / Соты"});pattern->setCurrentIndex(draft.perforation.pattern);form->addRow("Pattern",pattern);
        auto* useUV=new QCheckBox("Follow surface UV",&dialog);useUV->setChecked(draft.perforation.useUV);form->addRow(useUV);
        useUV->setToolTip("Uses the surface UV chart and UV tile size. Disable for object-space millimeters; projection seams may appear on curved surfaces.");
#define PERFORATION_CONTROL(name,def,lo,hi,label) {auto* spin=new QDoubleSpinBox(&dialog);spin->setObjectName("perforation_" #name);spin->setDecimals(2);spin->setRange(lo,hi);spin->setSingleStep(.1);spin->setKeyboardTracking(false);spin->setValue(draft.perforation.name);form->addRow(label,spin);}
        DOM_PERFORATION_PARAMETERS(PERFORATION_CONTROL)
#undef PERFORATION_CONTROL
        auto* color=new QPushButton("Color...",&dialog);form->addRow(color);
        auto* roughness=new QDoubleSpinBox(&dialog);roughness->setRange(.04,1);roughness->setSingleStep(.02);roughness->setValue(draft.roughness);form->addRow("Roughness",roughness);
        auto* metallic=new QDoubleSpinBox(&dialog);metallic->setRange(0,1);metallic->setSingleStep(.05);metallic->setValue(draft.metallic);form->addRow("Metallic",metallic);
        auto* note=new QLabel("Visual openings; CAD geometry stays unchanged.",&dialog);note->setWordWrap(true);layout->addWidget(note);
        QTimer timer(&dialog);timer.setSingleShot(true);timer.setInterval(100);
        auto read=[&](){
            draft.perforation.enabled=enabled->isChecked();draft.perforation.pattern=pattern->currentIndex();draft.perforation.useUV=useUV->isChecked();
#define READ_PERFORATION_CONTROL(name,def,lo,hi,label) draft.perforation.name=float(dialog.findChild<QDoubleSpinBox*>("perforation_" #name)->value());
            DOM_PERFORATION_PARAMETERS(READ_PERFORATION_CONTROL)
#undef READ_PERFORATION_CONTROL
            draft.roughness=float(roughness->value());draft.metallic=float(metallic->value());
            if(draft.perforation.enabled){draft.fabric.enabled=false;draft.plaster.enabled=false;}
        };
        auto refresh=[&](){read();preview->setPixmap(QPixmap::fromImage(RenderMaterialSphereGL(draft,280)));};
        connect(&timer,&QTimer::timeout,&dialog,refresh);
        for(auto* spin:dialog.findChildren<QDoubleSpinBox*>())connect(spin,qOverload<double>(&QDoubleSpinBox::valueChanged),&dialog,[&](){timer.start();});
        connect(pattern,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&](){timer.start();});
        connect(useUV,&QCheckBox::toggled,&dialog,[&](){timer.start();});
        connect(enabled,&QCheckBox::toggled,&dialog,[&](){timer.start();});
        connect(color,&QPushButton::clicked,&dialog,[&](){
            QColor c=QColorDialog::getColor(QColor::fromRgbF(draft.diffuse.r,draft.diffuse.g,draft.diffuse.b),&dialog);
            if(c.isValid()){draft.diffuse={float(c.redF()),float(c.greenF()),float(c.blueF())};timer.start();}
        });
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        refresh();if(dialog.exec()==QDialog::Accepted){timer.stop();read();LoadMaterialToEditor(draft,current_file_path_);CommitEditorChanges();}
    });

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
    advanced_form->addRow(coating_group_);

    auto* color_row = new QWidget(editor);
    auto* color_layout = new QHBoxLayout(color_row);
    color_layout->setContentsMargins(0, 0, 0, 0);
    ambient_button_ = new QPushButton("Ambient", color_row);
    diffuse_button_ = new QPushButton("Diffuse", color_row);
    emission_button_ = new QPushButton("Emission", color_row);
    color_layout->addWidget(ambient_button_);
    form->addRow("Color", diffuse_button_);
    color_layout->addWidget(emission_button_);
    advanced_form->addRow("Colors", color_row);

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
    advanced_form->addRow(new DragSpinBoxLabel("Specular", specular_spin_, editor), specular_spin_);
    advanced_form->addRow(new DragSpinBoxLabel("Shininess", shininess_spin_, editor), shininess_spin_);
    advanced_form->addRow(new DragSpinBoxLabel("Reflectivity", reflectivity_spin_, editor), reflectivity_spin_);
    form->addRow(new DragSpinBoxLabel("PBR Roughness", roughness_spin_, editor), roughness_spin_);
    form->addRow(new DragSpinBoxLabel("PBR Metallic", metallic_spin_, editor), metallic_spin_);
    advanced_form->addRow(new DragSpinBoxLabel("Coat", coat_weight_spin_, editor), coat_weight_spin_);
    advanced_form->addRow(new DragSpinBoxLabel("Coat Roughness", coat_roughness_spin_, editor), coat_roughness_spin_);
    advanced_form->addRow(new DragSpinBoxLabel("Normal strength", normal_strength_spin_, editor), normal_strength_spin_);
    advanced_form->addRow(new DragSpinBoxLabel("Parallax depth", displacement_scale_spin_, editor), displacement_scale_spin_);

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
    auto* wrapObject=new QCheckBox("Wrap whole object (seam underneath)",textures_group);
    wrapObject->setObjectName("TextureWrapObject");
    wrapObject->setToolTip("Shared top-and-side mapping for cushions aligned to Z. Distortion is concentrated underneath. Applies to all texture maps.");
    connect(wrapObject,&QCheckBox::toggled,this,[this](bool){CommitEditorChanges();});

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
    uv_layout->addWidget(wrapObject,4,0,1,4);
    textures_layout->addLayout(uv_layout, 7, 0, 1, 3);
    advanced_layout->addWidget(textures_group);
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
    connect(ral_button, &QPushButton::clicked, this, [this]() {
        QDialog dialog(this);
        dialog.setObjectName("RalMaterialColorDialog");
        dialog.setWindowTitle("RAL Classic");
        dialog.resize(820,650);dialog.setMinimumSize(460,360);
        auto* layout=new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel("<b>RAL Classic</b> &nbsp; 216 colors",&dialog));
        auto* filters=new QHBoxLayout;
        auto* search=new QLineEdit(&dialog);search->setObjectName("RalColorSearch");
        search->setPlaceholderText("Search RAL number, name or HEX");search->setClearButtonEnabled(true);
        auto* series=new QComboBox(&dialog);series->setObjectName("RalColorSeries");
        series->addItems({"All series","1 — Yellow / beige","2 — Orange","3 — Red","4 — Violet",
            "5 — Blue","6 — Green","7 — Grey","8 — Brown","9 — White / black"});
        filters->addWidget(search,1);filters->addWidget(series);layout->addLayout(filters);
        auto* list=new QListWidget(&dialog);list->setObjectName("RalColorGrid");
        list->setViewMode(QListView::IconMode);list->setMovement(QListView::Static);
        list->setResizeMode(QListView::Adjust);list->setWrapping(true);
        list->setIconSize(QSize(82,46));list->setGridSize(QSize(96,78));
        list->setSpacing(2);list->setUniformItemSizes(true);
        list->setStyleSheet("QListWidget { background: #f4f5f7; border: 1px solid #c8cdd3; }"
            "QListWidget::item { color: #20252b; border: 2px solid transparent; border-radius: 4px; }"
            "QListWidget::item:hover { background: #e0eafa; }"
            "QListWidget::item:selected { background: #dbeaff; border-color: #2874c8; }");
        layout->addWidget(list,1);
        auto* selection=new QLabel("Select a color",&dialog);selection->setWordWrap(true);
        layout->addWidget(selection);
        auto* note=new QLabel("Screen colors are approximate. Use a physical RAL sample for paint matching.",&dialog);
        note->setWordWrap(true);layout->addWidget(note);
        auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
        layout->addWidget(buttons);
        const QColor current=to_qcolor(ButtonColor(diffuse_button_));
        for(int i=0;i<int(std::size(kRalColors));++i) {
            const auto& entry=kRalColors[i];const QColor color=to_qcolor(entry.color);
            QPixmap swatch(82,46);swatch.fill(color);
            QPainter painter(&swatch);painter.setPen(QColor(0,0,0,45));painter.drawRect(0,0,81,45);painter.end();
            auto* item=new QListWidgetItem(QIcon(swatch),QString("RAL %1").arg(entry.code),list);
            item->setData(Qt::UserRole,i);
            item->setToolTip(QString("RAL %1 — %2\n%3").arg(entry.code,entry.name,color.name().toUpper()));
            if(color.rgb()==current.rgb()&&!list->currentItem())list->setCurrentItem(item);
        }
        const auto refresh=[=]() {
            auto* item=list->currentItem();const bool selected=item&&!item->isHidden();
            buttons->button(QDialogButtonBox::Ok)->setEnabled(selected);
            selection->setText(selected?item->toolTip().replace('\n',"   "):"Select a color");
        };
        const auto filter=[=]() {
            for(int i=0;i<list->count();++i) {
                auto* item=list->item(i);const auto& entry=kRalColors[item->data(Qt::UserRole).toInt()];
                item->setHidden((series->currentIndex()>0&&entry.code[0]-'0'!=series->currentIndex())
                    ||!item->toolTip().contains(search->text().trimmed(),Qt::CaseInsensitive));
            }
            refresh();
        };
        connect(search,&QLineEdit::textChanged,&dialog,[=](){filter();});
        connect(series,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[=](){filter();});
        connect(list,&QListWidget::currentItemChanged,&dialog,[=](){refresh();});
        connect(list,&QListWidget::itemDoubleClicked,&dialog,[&dialog](){dialog.accept();});
        connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        refresh();if(list->currentItem())list->scrollToItem(list->currentItem());
        if(dialog.exec()!=QDialog::Accepted||!list->currentItem()||list->currentItem()->isHidden())return;
        const int index=list->currentItem()->data(Qt::UserRole).toInt();
        {const QSignalBlocker blocker(ral_combo_);ral_combo_->setCurrentIndex(index);}
        SetColorButton(diffuse_button_,kRalColors[index].color);
        CommitEditorChanges();
    });
    connect(color_button, &QPushButton::clicked, this, [this]() {
        QDialog dialog(this);
        dialog.setObjectName("NamedMaterialColorDialog");
        dialog.setWindowTitle("Color Material");
        dialog.resize(380, 560);
        auto* layout = new QVBoxLayout(&dialog);
        auto* search = new QLineEdit(&dialog);
        search->setPlaceholderText("Search color name or #RRGGBB");
        search->setClearButtonEnabled(true);
        layout->addWidget(search);
        auto* list = new QListWidget(&dialog);
        list->setIconSize(QSize(48, 24));
        list->setUniformItemSizes(true);
        layout->addWidget(list);
        const QColor current = to_qcolor(ButtonColor(diffuse_button_));
        for (const auto& entry : kNamedMaterialColors) {
            const QColor color = QColor::fromRgb(entry.rgb);
            QPixmap swatch(48, 24); swatch.fill(color);
            QPainter painter(&swatch);
            painter.setPen(QColor(100,100,100)); painter.drawRect(0,0,47,23);
            painter.end();
            auto* item = new QListWidgetItem(QIcon(swatch), QString::fromLatin1(entry.name), list);
            item->setData(Qt::UserRole, color);
            item->setToolTip(color.name().toUpper());
            item->setSizeHint(QSize(0, 30));
            if (!list->currentItem() && color.rgb() == current.rgb()) list->setCurrentItem(item);
        }
        if (list->currentItem()) list->scrollToItem(list->currentItem());
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttons);
        const auto update_ok = [=]() {
            buttons->button(QDialogButtonBox::Ok)->setEnabled(list->currentItem() && !list->currentItem()->isHidden());
        };
        connect(list, &QListWidget::currentItemChanged, &dialog, [=]() { update_ok(); });
        connect(search, &QLineEdit::textChanged, &dialog, [=](const QString& text) {
            for (int i=0; i<list->count(); ++i) {
                auto* item=list->item(i);
                item->setHidden(!item->text().contains(text.trimmed(), Qt::CaseInsensitive)
                    && !item->toolTip().contains(text.trimmed(), Qt::CaseInsensitive));
            }
            update_ok();
        });
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        connect(list, &QListWidget::itemDoubleClicked, &dialog, [&dialog]() { dialog.accept(); });
        update_ok();
        if (dialog.exec() != QDialog::Accepted || !list->currentItem() || list->currentItem()->isHidden()) return;
        const QColor color = list->currentItem()->data(Qt::UserRole).value<QColor>();
        SetColorButton(diffuse_button_, {float(color.redF()), float(color.greenF()), float(color.blueF())});
        CommitEditorChanges();
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
    findChild<QCheckBox*>("TextureWrapObject")->setChecked(material.texture_wrap_object);
    UpdateCoatingControls(material, candidate_path);
    findChild<QPushButton*>("ProceduralPlasterButton")->setProperty("configuration",EncodePlaster(material));
    findChild<QPushButton*>("ProceduralFabricButton")->setProperty("configuration",EncodeFabric(material));
    findChild<QPushButton*>("ProceduralPerforationButton")->setProperty("configuration",EncodePerforation(material));
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
    material.texture_wrap_object=findChild<QCheckBox*>("TextureWrapObject")->isChecked();
    material.source_file_path = current_file_path_.toStdString();
    DecodePlaster(findChild<QPushButton*>("ProceduralPlasterButton")->property("configuration").toString(),material);
    DecodeFabric(findChild<QPushButton*>("ProceduralFabricButton")->property("configuration").toString(),material);
    DecodePerforation(findChild<QPushButton*>("ProceduralPerforationButton")->property("configuration").toString(),material);
    if(!material.perforation.enabled && (material.plaster.enabled || material.fabric.enabled))material.metallic=0;
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
