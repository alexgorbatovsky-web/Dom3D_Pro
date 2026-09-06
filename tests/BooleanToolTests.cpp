#include "ui/MainWindow.h"
#include "ui/BooleanDialog.h"
#include "solid/Solid.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}
}

int TestBooleanTool(int argc, char** argv) {
    std::cerr << "Initializing Boolean UI test...\n";
    QApplication app(argc, argv);
    QTemporaryDir settings;
    require(settings.isValid(), "Missing temporary settings directory.");
    QCoreApplication::setOrganizationName("Dom3DBooleanTests");
    QCoreApplication::setApplicationName("BooleanToolTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    std::cerr << "Creating the main window...\n";
    MainWindow window;
    auto* viewport = window.findChild<OpenGLViewport*>();
    QAction* boolean = nullptr;
    QAction* select = nullptr;
    for (auto* action : window.findChildren<QAction*>()) {
        if (action->property("toolKey").toString() == "boolean") boolean = action;
        if (action->property("toolKey").toString() == "select") select = action;
    }
    require(viewport && boolean && select, "Missing Boolean UI actions.");
    viewport->resize(800, 600);
    Camera camera;
    camera.orientation = {1, 0, 0, 0};
    camera.target = {7.5f, 5, 5};
    camera.distance = 40;
    viewport->SetCamera(camera);
    viewport->SetOrthographicProjection(true);
    const auto click = [&](Vec3 world) {
        DomPoint position;
        QtSceneRenderer renderer;
        require(renderer.WorldToScreen(world, viewport->GetCamera(), true,
                    viewport->width(), viewport->height(), position), "Cannot project test body.");
        const QPointF point(position.x, position.y);
        QMouseEvent press(QEvent::MouseButtonPress, point, point,
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, point, point,
                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &release);
    };
    for (int type : {1, 0, 2}) {
        std::cerr << "Preparing bodies for operation " << type << "...\n";
        CAlfaDoc document;
        const size_t initial_count = document.GetObjects().size();
        for (double x : {0.0, 5.0}) {
            TopoDS_Shape shape = BRepPrimAPI_MakeBox(gp_Pnt(x, 0, 0), 10, 10, 10).Shape();
            auto solid = std::make_unique<CSolid>(shape);
            solid->SetParametricOperation(0, "SolidBox", "Box",
                {{"width", 10}, {"depth", 10}, {"height", 10}});
            require(solid->ReBuldMesh(), "Could not prepare test body.");
            document.AddObject(std::move(solid), false);
        }
        viewport->SetDocument(&document);
        std::cerr << "Starting Boolean...\n";
        boolean->trigger();
        auto* options = window.findChild<BooleanDialog*>("BooleanToolOptions");
        auto* combo = window.findChild<QComboBox*>("BooleanOperationType");
        require(viewport->CurrentTool() == ToolMode::Boolean && options && combo
                    && options->isVisible() && !options->isModal()
                    && options->findChildren<QDialogButtonBox*>().isEmpty(),
                "Boolean must begin body picking immediately, without an OK dialog.");
        combo->setCurrentIndex((type + 1) % 3);
        require(window.statusBar()->currentMessage().contains("first body"),
                "Changing type before the first click must keep first-body selection.");
        std::cerr << "Picking first body...\n";
        click({2, 5, 10});
        require(window.statusBar()->currentMessage().contains("second body"),
                "The first click must select the base body.");
        combo->setCurrentIndex(type);
        require(window.statusBar()->currentMessage().contains("second body"),
                "Changing type must retain the selected base body.");
        click({2, 5, 10});
        require(document.GetObjects().size() == initial_count + 2
                    && viewport->CurrentTool() == ToolMode::Boolean,
                "Clicking the first body again must not execute the operation.");
        std::cerr << "Picking second body...\n";
        click({13, 5, 10});
        require(document.GetObjects().size() == initial_count + 1
                    && viewport->CurrentTool() == ToolMode::Select && !options->isVisible(),
                "The second body click must execute Boolean and close its options.");
        auto* result = dynamic_cast<CSolid*>(document.GetObjects()[initial_count].get());
        require(result && result->GetNumOperations() == 2, "Boolean history was not recorded.");
        const auto* operation = result->GetOperation(1);
        bool saved_type = false;
        for (const auto& parameter : operation->Parameters)
            if (parameter.id == "operation") saved_type = parameter.value == type;
        require(operation->ToolId == "boolean" && saved_type,
                "History parameters must contain the type chosen after the first click.");
        GProp_GProps volume;
        BRepGProp::VolumeProperties(result->m_Shape, volume);
        require(std::fabs(volume.Mass() - (type == 0 ? 1500.0 : 500.0)) < 1.0e-5,
                "Boolean geometry used the wrong operation.");
        viewport->SetDocument(nullptr);
    }
    auto* repeat = window.findChild<QAction*>("RepeatLastCommandAction");
    require(repeat && repeat->isEnabled(), "Boolean must be repeatable.");
    repeat->trigger();
    auto* options = window.findChild<BooleanDialog*>("BooleanToolOptions");
    require(options->isVisible() && options->SelectedOperation() == BooleanDialog::Operation::Common
                && viewport->CurrentTool() == ToolMode::Boolean,
            "Repeat must immediately restart with the last chosen type.");
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(viewport, &escape);
    require(!options->isVisible() && viewport->CurrentTool() == ToolMode::Select,
            "Viewport Escape must close Boolean options.");
    boolean->trigger();
    options->close();
    require(viewport->CurrentTool() == ToolMode::Select, "Closing options must cancel picking.");
    boolean->trigger();
    select->trigger();
    require(!options->isVisible(), "Changing tools must hide Boolean options.");
    std::cout << "Boolean immediate picking and live operation tests passed.\n";
    return 0;
}
