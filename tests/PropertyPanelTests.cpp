#include "ui/PropertyPanel.h"
#include "ui/MeasurementUnits.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QKeyEvent>
#include <QLineEdit>
#include "ui/DefaultDialogAccept.h"

// The standalone panel test uses the same Qt type boundary as the application
// without linking the scene renderer or requiring an OpenGL context.
class OpenGLViewport : public QWidget {
    Q_OBJECT
public:
    using QWidget::QWidget;
};

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
}

int main(int argc, char** argv) {
    QApplication application(argc, argv);

    {
        QWidget main;
        OpenGLViewport viewport(&main);
        PropertyPanel editor(&main);
        editor.move(150,0);
        main.resize(600,500);
        main.show(); viewport.show(); editor.show();
        int accepted=0;
        QObject::connect(&editor,&PropertyPanel::Accepted,[&] { ++accepted; });
        const auto enter=[](QWidget* target,int key=Qt::Key_Return,Qt::KeyboardModifiers modifiers=Qt::NoModifier,bool repeat=false) {
            QKeyEvent event(QEvent::KeyPress,key,modifiers,QString(),repeat);
            QApplication::sendEvent(target,&event);
        };
        for(const char* tool: {"SolidBox","SolidCylinder","SolidSphereTool","SolidTorusTool",
                "SolidPrismTool","SolidPolyhedronTool","SolidTwoSketches","SolidExtrudeTool",
                "SolidSweptTool","SolidFrameTool","SurfaceOfRevolution","SolidShell",
                "ThickSolidTool","SolidHole","fillet_edge","fillet_all_edges","ChamferSolid",
                "SolidExtrudeFace","SolidOffsetFace","SolidDraft","SolidSheetBend"}) {
            ActiveParametricObject active;
            active.tool_id=tool;
            ToolParameter p; p.id="length"; p.label="Length"; p.type=ToolParameterType::Number;
            p.minimum=0; p.maximum=1000; p.value=10;
            active.parameters.push_back(p);
            editor.SetActiveObject(active); editor.show();
            QApplication::processEvents();
            QPushButton* ok=nullptr;
            for(auto* b:editor.findChildren<QPushButton*>()) if(b->text()=="OK")ok=b;
            require(ok&&ok->isDefault(),"Parameter OK must be default for every Solid tool");
            auto* spin=editor.findChild<QDoubleSpinBox*>();
            require(spin,"Missing parameter editor");
            auto* text=spin->findChild<QLineEdit*>();
            text->setText("23");
            const int before=accepted;
            enter(text);
            require(accepted==before+1&&spin->value()==23,"Enter must commit typed value and accept once");
            viewport.setFocus(); enter(&viewport,Qt::Key_Enter,Qt::KeypadModifier);
            require(accepted==before+2,"Numpad Enter in viewport must accept Solid parameters");
            enter(&viewport,Qt::Key_Return,Qt::ControlModifier);
            enter(&viewport,Qt::Key_Return,Qt::NoModifier,true);
            require(accepted==before+2,"Modifiers and auto-repeat must not confirm again");
            ok->setEnabled(false); enter(&viewport);
            require(accepted==before+2,"Disabled OK must not accept");
            ok->setEnabled(true);
            QDialog modal(&main); modal.setModal(true); modal.show();
            QApplication::processEvents(); enter(&viewport);
            require(accepted==before+2,"A nested modal must block parent confirmation");
            modal.hide();
            QWidget popup(&main,Qt::Popup); popup.show();
            require(QApplication::activePopupWidget()==&popup,"Popup did not open");
            enter(&viewport);
            require(accepted==before+2,"Popup selection must not confirm the Solid");
            popup.hide();
            QWidget other; OpenGLViewport otherViewport(&other);
            enter(&otherViewport);
            require(accepted==before+2,"Another window must not confirm this Solid");
            editor.hide(); enter(&viewport);
            require(accepted==before+2,"A hidden panel must not accept");
        }
        // Standalone modeless editing dialogs use their real OK callback too.
        QDialog dialog(&main); auto* ok=new QPushButton("OK",&dialog);
        SetDefaultDialogAccept(&dialog,ok);
        int clicks=0; QObject::connect(ok,&QPushButton::clicked,[&]{++clicks;});
        dialog.show(); QApplication::processEvents(); enter(&viewport);
        require(clicks==1,"Modeless edit dialog must accept from viewport");
    }

    ActiveParametricObject active;
    active.tool_id = "cabinet";
    ToolParameter material;
    material.id = "facade_material_id";
    material.label = "Facade material";
    material.type = ToolParameterType::Material;
    material.options = {"Wood"};
    material.option_values = {1.0};
    material.value = 1.0;
    active.parameters.push_back(material);
    ToolParameter body_type;
    body_type.id = "body_type";
    body_type.label = "Body Type";
    body_type.type = ToolParameterType::Combo;
    body_type.options = {
        "Straight", "Corner", "Radius", "Corner-2", "Radius-2",
        "Radius-3", "Radius-4"};
    active.parameters.push_back(body_type);

    PropertyPanel panel;
    panel.SetActiveObject(active);
    int request_count = 0;
    QObject::connect(
        &panel, &PropertyPanel::MaterialLibraryRequested, &panel,
        [&panel, &request_count](const QString&) {
            ++request_count;
            // Reproduce the synchronous rebuild performed by the nested
            // "Other Parameters" panel when the library is requested.
            const ActiveParametricObject copy = panel.ActiveObject();
            panel.SetActiveObject(copy);
        });

    QComboBox* selector = panel.findChild<QComboBox*>(
        QStringLiteral("parameter_facade_material_id"));
    require(selector != nullptr,
            "Material selector was not created in PropertyPanel.");
    selector->setCurrentIndex(selector->count() - 1);
    require(request_count == 1,
            "Material library request was not emitted exactly once.");
    require(panel.findChild<QComboBox*>(
                QStringLiteral("parameter_facade_material_id")) != nullptr,
            "Synchronous PropertyPanel rebuild lost the material selector.");

    ToolParameter body_material = material;
    body_material.id = "body_material_id";
    body_material.label = "Body material";
    active.parameters.push_back(body_material);

    // Facade material must be available directly in the compact cabinet
    // panel. Secondary materials stay in "Other Params..."; requesting their
    // library closes the modal dialog before the outer panel can be rebuilt.
    PropertyPanel compact_panel;
    compact_panel.SetActiveObject(active);
    require(compact_panel.findChild<QComboBox*>(
                QStringLiteral("parameter_facade_material_id")) != nullptr,
            "Facade material is missing from the compact cabinet panel.");
    require(compact_panel.findChild<QComboBox*>(
                QStringLiteral("parameter_body_type")) != nullptr,
            "Body Type is missing from the compact cabinet panel.");
    require(compact_panel.findChild<QComboBox*>(
                QStringLiteral("parameter_body_material_id")) == nullptr,
            "Body material must remain in Other Parameters.");
    QPushButton* other = compact_panel.findChild<QPushButton*>(
        QStringLiteral("other_parameters_button"));
    require(other != nullptr,
            "Compact cabinet did not create the Other Params button.");

    bool nested_request = false;
    QObject::connect(
        &compact_panel, &PropertyPanel::MaterialLibraryRequested,
        &compact_panel, [&nested_request](const QString&) {
            nested_request = true;
        });

    QTimer::singleShot(0, &compact_panel, [&compact_panel]() {
        QComboBox* nested_selector = compact_panel.findChild<QComboBox*>(
            QStringLiteral("parameter_body_material_id"));
        require(nested_selector != nullptr,
                "Nested material selector was not created.");
        nested_selector->setCurrentIndex(nested_selector->count() - 1);
    });
    other->click();

    require(nested_request,
            "Nested panel did not request the material library.");
    compact_panel.SetMaterialParameterValue(
        "body_material_id", 42.0, "Library material");
    const auto assigned = std::find_if(
        compact_panel.ActiveObject().parameters.begin(),
        compact_panel.ActiveObject().parameters.end(),
        [](const ToolParameter& parameter) {
            return parameter.id == "body_material_id";
        });
    require(assigned != compact_panel.ActiveObject().parameters.end()
                && assigned->value == 42.0,
            "Library material was not routed through the nested panel.");

    ActiveParametricObject box;
    box.tool_id = "Box";
    ToolParameter length;
    length.id = "length";
    length.label = "Length";
    length.type = ToolParameterType::Number;
    length.unit = ToolParameterUnit::Length;
    length.minimum = 0.0;
    length.maximum = 1000.0;
    length.step = 0.001;
    length.value = 33.818;
    box.parameters.push_back(length);

    PropertyPanel box_panel;
    box_panel.SetActiveObject(box);
    QDoubleSpinBox* length_editor = box_panel.findChild<QDoubleSpinBox*>(
        QStringLiteral("parameter_length"));
    require(length_editor != nullptr,
            "Length editor was not created in PropertyPanel.");
    require(length_editor->suffix().isEmpty(),
            "Length editor still displays a unit suffix.");

    ActiveParametricObject hole;
    hole.tool_id = "SolidHole";
    for (const std::string& id : {
             "diameter", "depth", "hole.distance1", "hole.distance2"}) {
        ToolParameter parameter;
        parameter.id = id;
        parameter.label = id == "diameter" ? "Diameter"
            : (id == "depth" ? "Depth"
               : (id == "hole.distance1" ? "Distance to Edge 1"
                                           : "Distance to Edge 2"));
        parameter.type = ToolParameterType::Number;
        parameter.unit = ToolParameterUnit::Length;
        parameter.minimum = id == "diameter" || id == "depth" ? 0.001 : 0.0;
        parameter.maximum = 1000000.0;
        parameter.step = 0.1;
        parameter.value = 10.0;
        hole.parameters.push_back(parameter);
    }
    PropertyPanel hole_panel;
    hole_panel.SetActiveObject(hole);
    const auto labels = hole_panel.findChildren<QLabel*>();
    const auto has_drag_label = [&labels](const QString& name) {
        return std::any_of(labels.begin(), labels.end(), [&name](QLabel* label) {
            return label->text() == QString::fromUtf8("◀%1▶").arg(name);
        });
    };
    require(has_drag_label("Diameter"),
            "Hole diameter drag slider was not created.");
    require(has_drag_label("Depth"),
            "Hole depth drag slider was not created.");
    require(has_drag_label("Distance to Edge 1"),
            "Hole first-edge distance drag slider was not created.");
    require(has_drag_label("Distance to Edge 2"),
            "Hole second-edge distance drag slider was not created.");

    return EXIT_SUCCESS;
}

#include "PropertyPanelTests.moc"
