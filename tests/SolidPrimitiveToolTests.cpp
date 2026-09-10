#include "ui/MainWindow.h"
#include "solid/Solid.h"
#include "solid/SurfaceFace.h"
#include "CMesh3D.h"
#include "SmartLine.h"
#include "Dom3DProjectSerializer.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QEventLoop>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QSurfaceFormat>
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
double parameter(const std::vector<ToolParameter>& parameters, const char* id) {
    for (const auto& p : parameters) if (p.id == id) return p.value;
    require(false, id);
    return 0;
}
void click(OpenGLViewport& viewport, Vec3 world) {
    DomPoint position;
    QtSceneRenderer renderer;
    require(renderer.WorldToScreen(world, viewport.GetCamera(), viewport.IsOrthographicProjection(),
        viewport.width(), viewport.height(), position), "Cannot project test point");
    const QPointF point(position.x, position.y);
    QMouseEvent press(QEvent::MouseButtonPress, point, point,
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&viewport, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, point, point,
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&viewport, &release);
}
}

int TestSolidPrimitiveTool(int argc, char** argv) {
    QApplication app(argc, argv);
    if (app.arguments().contains("--primitive-enter-only")) {
        QWidget host;
        PropertyPanel panel(&host);
        OpenGLViewport scene(&host);
        host.show(); panel.show(); scene.show();
        int accepted = 0;
        QObject::connect(&panel, &PropertyPanel::Accepted, &host, [&] { ++accepted; });
        const auto press = [](QWidget* target, int key) {
            QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
            QApplication::sendEvent(target, &event);
        };
        for (const char* id : {"SolidBox", "SolidCylinder"}) {
            ActiveParametricObject object;
            object.tool_id = id;
            object.parameters = {{"height", "Height", 50, 0.1, 100, 0.1}};
            panel.SetActiveObject(object);
            auto* spin = panel.findChild<QDoubleSpinBox*>();
            require(spin, "Primitive height editor missing");
            for (int key : {Qt::Key_Return, Qt::Key_Enter}) {
                const int before = accepted;
                spin->setKeyboardTracking(false);
                spin->findChild<QLineEdit*>()->setText(spin->locale().toString(61.5, 'f', 1));
                press(spin->findChild<QLineEdit*>(), key);
                std::cerr << "enter accepted=" << accepted << " expected=" << before+1
                          << " value=" << parameter(panel.ActiveObject().parameters,"height") << '\n';
                require(accepted == before + 1 && parameter(panel.ActiveObject().parameters,"height") == 61.5,
                        "Enter must commit edited value and accept exactly once");
                press(&scene, key);
                require(accepted == before + 2, "Enter in viewport must confirm visible primitive parameters");
            }
        }
        const int before = accepted;
        panel.hide();
        press(&scene, Qt::Key_Return);
        require(accepted == before, "Hidden primitive panel must not handle Enter");
        panel.show();
        QDialog modal(&host); modal.setModal(true); modal.show(); app.processEvents();
        press(&scene, Qt::Key_Return);
        require(accepted == before, "Another modal dialog must retain its Enter key");
        modal.hide();
        std::cout << "Primitive Enter confirms editor and viewport input without stealing other dialogs\n";
        return 0;
    }
    QTemporaryDir settings;
    require(settings.isValid(), "Missing temporary settings directory");
    QCoreApplication::setOrganizationName("Dom3DPrimitiveTests");
    QCoreApplication::setApplicationName("SolidPrimitiveToolTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    MainWindow window;
    auto* viewport = window.findChild<OpenGLViewport*>();
    auto action = [&window](const char* key) -> QAction* {
        for (auto* candidate : window.findChildren<QAction*>())
            if (candidate->property("toolKey").toString() == key) return candidate;
        require(false, "Missing primitive action");
        return nullptr;
    };
    Camera camera;
    camera.orientation = quaternion_from_axis_angle({0, 0, 1}, 0.6f)
        * quaternion_from_axis_angle({1, 0, 0}, 0.8f);
    camera.target = {10, 10, 0};
    camera.distance = 80;
    viewport->resize(1000, 800);
    viewport->SetCamera(camera);
    viewport->SetOrthographicProjection(true);
    std::vector<ToolParameter> completed;
    QObject::connect(viewport, &OpenGLViewport::SolidBoxRectangleFinished,
        &window, [&](std::vector<ToolParameter> p) { completed = std::move(p); });
    QObject::connect(viewport, &OpenGLViewport::SolidCylinderCircleFinished,
        &window, [&](std::vector<ToolParameter> p) { completed = std::move(p); });
    if (app.arguments().contains("--coordinate-axis-only")) {
        camera.target = {}; camera.distance = 800;
        camera.orientation = kDefaultCameraOrientation;
        viewport->SetCamera(camera);
        viewport->SetCoordinateAxesVisible(false);
        int picked = -1;
        QObject::connect(viewport, &OpenGLViewport::CoordinateAxisSelected, &window, [&](int axis) { picked = axis; });
        for (const char* tool : {"SurfaceOfRevolution", "SurfaceRevolve", "SolidPolyhedronTool"}) {
            window.ClearActiveProperties();
            require(window.document_.CreateSketchPolyline({{10,0,10},{20,0,10},{20,0,20},{10,0,20}},
                    true, "Axis test", {}, {1,0,0}, {0,0,1}), "Axis profile creation failed");
            const auto profileId = window.document_.GetSelectedObject()->m_id;
            window.document_.ClearSelection();
            window.ActivateParametricTool(tool);
            require(window.property_panel_->isVisible(), "Command must wait without preselection");
            window.document_.SelectObjectById(profileId);
            viewport->SelectionChanged();
            auto* combo = window.property_panel_->findChild<QComboBox*>("parameter_axis");
            require(combo && window.property_panel_->isVisible(), "Axis parameter must be available");
            for (int axis : {0,1,2}) {
                picked = -1;
                click(*viewport, axis == 0 ? Vec3{100,0,0} : axis == 1 ? Vec3{0,100,0} : Vec3{0,0,100});
                require(picked == axis, "Viewport must pick the requested axis");
                app.processEvents();
                combo = window.property_panel_->findChild<QComboBox*>("parameter_axis");
                const int expected = std::string(tool) == "SolidPolyhedronTool" && axis == 1 ? 0 : axis;
                require(combo->currentIndex() == expected,
                        "Valid axis must rebuild; invalid axis must restore previous parameters");
            }
            require(!viewport->IsCoordinateAxesVisible(), "Temporary axis display must preserve user preference");
        }
        window.ClearActiveProperties();
        picked = -1;
        click(*viewport, {100,0,0});
        require(picked == -1, "Axis picking must end when parameter editing ends");
        for (const char* tool : {"SolidFrameTool", "SolidWireTool", "SolidPolyhedronTool"}) {
            window.document_.ClearSelection();
            const auto count = window.document_.GetObjects().size();
            window.ActivateParametricTool(tool);
            require(window.active_parametric_object_.transient, "Profile tool must wait for selection");
            window.AcceptActiveProperties();
            require(window.active_parametric_object_.transient, "OK without profile must keep waiting");
            window.CancelActiveProperties();
            require(window.document_.GetObjects().size() == count, "Cancel while waiting must preserve scene objects");
            if (std::string(tool) == "SolidPolyhedronTool") continue;
            require(window.document_.CreateSketchPolyline({{0,0,0},{100,0,0},{100,100,0},{0,100,0}},
                std::string(tool) == "SolidFrameTool", "Pending profile", {}, {1,0,0}, {0,1,0}), "Pending profile fixture failed");
            const auto profile = window.document_.GetSelectedObject()->m_id;
            window.document_.ClearSelection();
            window.ActivateParametricTool(tool);
            window.document_.SelectObjectById(profile);
            viewport->SelectionChanged();
            require(!window.active_parametric_object_.transient && window.document_.GetSelectedSolid(),
                "Profile selection must start pending tool");
            window.AcceptActiveProperties();
        }
        require(window.document_.CreateSketchPolyline({{0,10,0},{20,10,0},{20,30,0},{0,30,0}},
            true, "XY profile", {}, {1,0,0}, {0,1,0}), "XY profile fixture failed");
        window.ActivateParametricTool("SolidPolyhedronTool");
        require(window.active_parametric_object_.transient, "Invalid default axis must keep the tool open");
        click(*viewport, {100,0,0});
        require(!window.active_parametric_object_.transient && window.document_.GetSelectedSolid(),
            "Picking a valid axis must create the waiting polyhedron");
        window.AcceptActiveProperties();
        // Moving only the sketch coordinate-system origin must not move a world axis.
        double referenceVolume = 0;
        for (const auto origin : {CPoint3d{0,0,0}, CPoint3d{5,0,17}}) {
            CAlfaDoc axisDocument;
            require(axisDocument.CreateSketchPolyline({{10,0,30},{20,0,30},{20,0,40},{10,0,40}},
                true, "Shifted sketch origin", origin, {1,0,0}, {0,0,1}), "Shifted sketch creation failed");
            const auto profileId = axisDocument.GetSelectedObject()->m_id;
            require(axisDocument.CreatePolyhedronSolid(profileId, 0, 8), "World X polyhedron creation failed");
            auto* solid = axisDocument.GetSelectedSolid();
            GProp_GProps props;
            BRepGProp::VolumeProperties(solid->m_Shape, props);
            if (referenceVolume == 0) referenceVolume = props.Mass();
            require(std::abs(props.Mass()-referenceVolume) < referenceVolume*1e-7,
                "World X geometry must not depend on sketch origin");
            const auto checkAxis = [](const CSolid& body) {
                require(!body.GetCenterlines().empty(), "Missing polyhedron axis");
                for (const auto& point : body.GetCenterlines().front().points)
                    require(std::abs(point.Y()) < 1e-5 && std::abs(point.Z()) < 1e-5,
                        "Polyhedron centerline must coincide with world X");
            };
            checkAxis(*solid);
            auto edit = window.tool_registry_.ActiveObjectFromDocument(
                axisDocument.GetSelectedObjectIndex(), *solid, 0, &axisDocument);
            require(window.tool_registry_.TryRebuildPolyhedron(edit, axisDocument), "World X rebuild failed");
            solid = axisDocument.GetSelectedSolid();
            checkAxis(*solid);
            BRepGProp::VolumeProperties(solid->m_Shape, props);
            require(std::abs(props.Mass()-referenceVolume) < referenceVolume*1e-7,
                "Rebuild must preserve the world X geometry");
        }
        CAlfaDoc offsetPlane;
        require(offsetPlane.CreateSketchPolyline({{10,5,30},{20,5,30},{20,5,40},{10,5,40}},
            true, "Offset plane", {0,5,0}, {1,0,0}, {0,0,1}), "Offset plane fixture failed");
        require(!offsetPlane.CreatePolyhedronSolid(offsetPlane.GetSelectedObject()->m_id, 0, 8),
            "World axis outside the sketch plane must be rejected");
        CAlfaDoc fixture;
        Dom3DProjectSerializer serializer;
        QString room, error; ProjectViewState view;
        require(serializer.Load(QDir(QCoreApplication::applicationDirPath()).filePath("../../tests/data/mesh-regression/Polyhedron.dom3d"), fixture, room, view, error),
                "Cannot load Polyhedron regression project");
        const auto index = fixture.FindObjectIndexById(3);
        auto active = window.tool_registry_.ActiveObjectFromDocument(index, *fixture.FindObjectById(3), 0, &fixture);
        const auto before = serializer.DocumentFingerprint(fixture);
        require(!fixture.CreatePolyhedronSolid(2, 0, 8), "Creation must reject a crossing axis too");
        require(serializer.DocumentFingerprint(fixture) == before, "Failed creation must preserve the document");
        for (auto& p : active.parameters) if (p.id == "axis") p.value = 0;
        require(!window.tool_registry_.TryRebuildPolyhedron(active, fixture), "Crossing axis must reject invalid polyhedron");
        require(serializer.DocumentFingerprint(fixture) == before, "Rejected rebuild must preserve geometry and history");
        std::cout << "Coordinate axis selection passed for solid/surface Revolve and Polyhedron\n";
        return 0;
    }
    if (app.arguments().contains("--camera-animation-only")) {
        CAlfaDoc document;
        TopoDS_Shape shape = BRepPrimAPI_MakeBox(gp_Pnt(50,50,0),30,30,10).Shape();
        auto body = std::make_unique<CSolid>(shape);
        require(body->ReBuldMesh(), "Hover fixture mesh failed");
        document.AddObject(std::move(body), false);
        OpenGLViewport view;
        view.SetDocument(&document);
        view.resize(800,600);
        view.move(-20000,-20000);
        view.show();
        const auto wait = [](int ms) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); };
        Camera initial;
        initial.orientation = Quaternion{};
        initial.target = {65,65,10};
        initial.distance = 100;
        view.SetCamera(initial);
        view.SetOrthographicProjection(true);
        view.SetReferencePlaneSelection(true);
        wait(30);
        const QPointF pos(400,300);
        QMouseEvent move(QEvent::MouseMove,pos,pos,Qt::NoButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(&view,&move);
        require(view.cursor().shape() == Qt::PointingHandCursor, "Planar body face must react to hover");
        require(!document.HasSelectedSolidFace(), "Hover must not select a face");
        const QString screenshot = qEnvironmentVariable("DOM3D_CAMERA_ANIMATION_SCREENSHOT");
        if (!screenshot.isEmpty()) require(view.grabFramebuffer().save(screenshot + "-hover.png"), "Hover screenshot failed");
        view.SetReferencePlaneSelection(false);
        initial.orientation = kDefaultCameraOrientation;
        view.SetCamera(initial);
        view.BeginSketchOnFace("Animated", {50,50,10},{1,0,0},{0,1,0},{0,0,1},0,-1);
        const auto similarity = [](Quaternion a, Quaternion b) {
            return std::abs(a.w*b.w+a.x*b.x+a.y*b.y+a.z*b.z);
        };
        require(similarity(view.GetCamera().orientation, initial.orientation) > 0.9999f,
                "Sketch camera must start from current orientation");
        wait(180);
        require(similarity(view.GetCamera().orientation, initial.orientation) < 0.999f
                && similarity(view.GetCamera().orientation, Quaternion{}) < 0.999f,
                "Camera must pass through intermediate rotations");
        if (!screenshot.isEmpty()) require(view.grabFramebuffer().save(screenshot + "-turn.png"), "Turn screenshot failed");
        wait(350);
        require(similarity(view.GetCamera().orientation, Quaternion{}) > 0.9999f
                && std::abs(view.GetCamera().distance-initial.distance) < 0.001f,
                "Camera must reach face normal without changing orthographic scale");
        view.SetCamera(initial);
        view.BeginSketchOnFace("Interrupted", {50,50,10},{1,0,0},{0,1,0},{0,0,1},0,-1);
        wait(80);
        Camera manual = view.GetCamera(); manual.distance = 177;
        view.SetCamera(manual);
        wait(500);
        require(view.GetCamera().distance == 177 && similarity(view.GetCamera().orientation, manual.orientation) > 0.9999f,
                "Animation must not overwrite manual camera changes");
        view.SetDocument(nullptr);
        std::cout << "Camera animation and non-destructive face hover passed\n";
        return 0;
    }
    if (app.arguments().contains("--reference-planes-only")) {
        for (bool ortho : {true, false}) {
            OpenGLViewport view;
            Camera before;
            before.target = {240, 180, 30};
            before.distance = 350;
            view.SetCamera(before);
            view.SetOrthographicProjection(ortho);
            const Vec3 focus{240,180,10};
            const float depth = dot(focus - camera_position(before),
                                    rotate(before.orientation, {0,0,-1}));
            const float expected = ortho ? before.distance
                : depth * std::tan(deg_to_rad(before.vertical_fov_degrees) * 0.5f) / 0.42f;
            view.BeginSketchOnFace("Scale", {0,0,10}, {1,0,0}, {0,1,0}, {0,0,1}, 0, -1);
            const auto after = view.GetCamera();
            require(std::abs(after.distance - expected) < 0.001f,
                    "Face sketch must preserve visible scale in either projection");
            require(std::abs(after.target.x - focus.x) < 0.001f
                    && std::abs(after.target.y - focus.y) < 0.001f
                    && std::abs(after.target.z - focus.z) < 0.001f,
                    "Face sketch must keep the viewed area centered on the plane");
        }
        const QString screenshot = qEnvironmentVariable("DOM3D_REFERENCE_PLANES_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            window.move(-20000, -20000);
            window.resize(1100, 800);
            window.show();
            camera.target = {};
            camera.distance = 800;
            viewport->SetCamera(camera);
            action("SolidBox")->trigger();
            app.processEvents();
            require(viewport->grabFramebuffer().save(screenshot), "Reference plane screenshot failed");
            const float half_patch = camera.distance * 0.42f * 105.0f / viewport->height();
            DomPoint hover;
            QtSceneRenderer projector;
            require(projector.WorldToScreen({half_patch, half_patch, 0}, camera, true,
                    viewport->width(), viewport->height(), hover), "Cannot project reference hover");
            const QPointF position(hover.x, hover.y);
            QMouseEvent move(QEvent::MouseMove, position, position, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(viewport, &move);
            require(viewport->cursor().shape() == Qt::PointingHandCursor, "Reference plane hover cursor missing");
            app.processEvents();
            require(viewport->grabFramebuffer().save(screenshot + ".hover.png"), "Reference hover screenshot failed");
        }
        int selected = -1;
        QObject::connect(viewport, &OpenGLViewport::ReferencePlaneSelected, &window,
                         [&](int plane) { selected = plane; });
        for (bool ortho : {true, false}) for (bool cylinder : {false, true}) for (int plane : {0,1,2}) {
            camera.target = {};
            camera.distance = 80;
            camera.orientation = plane == 0 ? Quaternion{}
                : quaternion_from_axis_angle(plane == 1 ? Vec3{1,0,0} : Vec3{0,1,0}, 1.5707963f);
            viewport->SetCamera(camera);
            viewport->SetOrthographicProjection(ortho);
            action(cylinder ? "SolidCylinder" : "SolidBox")->trigger();
            selected = -1;
            completed.clear();
            const auto point = [plane](float u, float v) {
                return plane == 0 ? Vec3{u,v,0} : plane == 1 ? Vec3{u,0,v} : Vec3{0,u,v};
            };
            click(*viewport, point(4,4));
            require(selected == plane, "Viewport plane click must select the facing reference plane");
            auto* combo = window.findChild<QComboBox*>("SolidPlacementType");
            require(combo && combo->currentIndex() == plane, "Plane click must update placement dialog");
            require(completed.empty(), "Plane selection must not finish a primitive");
            click(*viewport, point(4,4));
            require(completed.empty(), "Plane selection must not create the first anchor");
            click(*viewport, point(14,16));
            require(!completed.empty(), "Primitive must finish after two drawing clicks");
            require(parameter(completed, plane == 0 ? "axis.n.z" : plane == 1 ? "axis.n.y" : "axis.n.x") == 1,
                    "Primitive must use the plane selected in the viewport");
            viewport->SetTool(ToolMode::Select);
        }
        viewport->SetTool(ToolMode::Select);
        viewport->SetReferencePlaneSelection(true);
        camera.orientation = Quaternion{};
        viewport->SetCamera(camera);
        selected = -1;
        click(*viewport, {4,4,0});
        require(selected == 0, "Dialog plane selection must work outside primitive tools");
        viewport->SetReferencePlaneSelection(false);
        selected = -1;
        click(*viewport, {4,4,0});
        require(selected == -1, "Reference planes must stop intercepting clicks after cancellation");
        camera.orientation = quaternion_from_axis_angle({0,1,0}, 1.5707963f);
        viewport->SetCamera(camera);
        QTimer::singleShot(0, &window, [&] {
            QDialog* sketch_dialog = nullptr;
            for (auto* dialog : window.findChildren<QDialog*>())
                if (dialog->windowTitle() == "New Sketch" && dialog->isVisible()) sketch_dialog = dialog;
            require(sketch_dialog && !sketch_dialog->isModal(), "Sketch plane dialog must allow viewport input");
            click(*viewport, {0,4,4});
            auto* combo = sketch_dialog->findChild<QComboBox*>();
            require(combo && combo->currentIndex() == 2, "Sketch dialog must follow viewport plane selection");
            require(sketch_dialog->result() == QDialog::Accepted && !sketch_dialog->isVisible(),
                    "Plane click must accept sketch creation without OK");
        });
        action("NewSketch")->trigger();
        require(viewport->CurrentTool() == ToolMode::SketchRectangle,
                "Plane click must immediately start sketch drawing");
        viewport->EndSketch();
        selected = -1;
        click(*viewport, {0,4,4});
        require(selected == -1, "Closing sketch dialog must clear reference planes");
        for (int kind = 0; kind < 3; ++kind) {
            const float x = 50.0f + kind * 60.0f;
            TopoDS_Shape body_shape = BRepPrimAPI_MakeBox(gp_Pnt(x,50,0),30,30,10).Shape();
            auto body = std::make_unique<CSolid>(body_shape);
            require(body->ReBuldMesh(), "Cannot mesh direct face placement fixture");
            window.document_.AddObject(std::move(body), false);
            const auto body_id = window.document_.GetObjects().back()->m_id;
            camera.orientation = Quaternion{};
            camera.target = {x+15,65,10};
            camera.distance = 150;
            viewport->SetCamera(camera);
            viewport->SetOrthographicProjection(true);
            if (kind < 2) {
                action(kind == 0 ? "SolidBox" : "SolidCylinder")->trigger();
                completed.clear();
                click(*viewport, {x+15,65,10});
                require(window.findChild<QComboBox*>("SolidPlacementType")->currentIndex() == 3,
                        "Direct face click must update Placement to Solid Face");
                click(*viewport, {x+8,58,10});
                require(completed.empty(), "Face choice must not create the first primitive anchor");
                click(*viewport, {x+18,68,10});
                require(!completed.empty() && std::abs(parameter(completed,"origin.z")-10) < 0.01,
                        "Direct face primitive must be placed on the clicked face");
                viewport->SetTool(ToolMode::Select);
            } else {
                QTimer::singleShot(0, &window, [&] {
                    QDialog* dialog = nullptr;
                    for (auto* candidate : window.findChildren<QDialog*>())
                        if (candidate->windowTitle() == "New Sketch" && candidate->isVisible()) dialog = candidate;
                    require(dialog, "New sketch plane dialog missing");
                    click(*viewport, {x+15,65,10});
                    require(dialog->result() == QDialog::Accepted && !dialog->isVisible(),
                            "Body face click must accept sketch creation without OK");
                });
                action("NewSketch")->trigger();
                require(viewport->CurrentTool() == ToolMode::SketchRectangle,
                        "Body face click must immediately start sketch drawing");
                auto drawing_camera = viewport->GetCamera();
                drawing_camera.target = {x+15,65,10};
                drawing_camera.distance = 150;
                viewport->SetCamera(drawing_camera);
                click(*viewport, {x+5,55,10});
                click(*viewport, {x+20,70,10});
                const auto* sketch = window.document_.GetSelectedSketch();
                require(sketch && std::abs(sketch->GetCoordinateSystem().origin.z - 10) < 0.01,
                        "Direct face click must immediately create a sketch on that face");
                require(sketch->GetFaceAttachment().body_id == body_id
                        && sketch->GetFaceAttachment().face_index >= 0,
                        "Direct face sketch must retain its body/face attachment");
                viewport->EndSketch();
            }
        }
        {
            CAlfaDoc inclined_document;
            OpenGLViewport inclined;
            inclined.SetDocument(&inclined_document);
            inclined.resize(1000,800);
            const Vec3 origin{12345.125f, -3456.75f, 789.375f};
            const Vec3 u = normalize(Vec3{1,2,0});
            const Vec3 normal = normalize(Vec3{2,-1,3});
            const Vec3 v = normalize(cross(normal,u));
            inclined.BeginSketchOnFace("Inclined rectangle", origin, u, v, normal, 0, -1);
            auto view = inclined.GetCamera(); view.distance = 1000;
            inclined.SetCamera(view);
            const auto before = inclined_document.GetObjects().size();
            click(inclined, origin + u*35 + v*45);
            click(inclined, origin + u*230 + v*175);
            require(inclined_document.GetObjects().size() == before + 1,
                    "Second rectangle click on a translated inclined plane must create a contour");
            const auto* rectangle = inclined_document.GetSelectedSketch();
            require(rectangle && rectangle->IsClosed(), "Face rectangle must form a closed sketch");
        }
        std::cout << "Reference plane selection passed for Box, Cylinder and dialog mode in both projections\n";
        return 0;
    }
    for (bool cylinder : {false, true}) {
        const char* key = cylinder ? "SolidCylinder" : "SolidBox";
        const ToolMode mode = cylinder ? ToolMode::SolidCylinderCircle : ToolMode::SolidBoxRectangle;
        for (int plane : {0, 1, 2}) {
            std::cerr << key << ": live plane " << plane << '\n';
            action(key)->trigger();
            auto* options = window.findChild<QDialog*>("SolidPlacementOptions");
            auto* combo = window.findChild<QComboBox*>("SolidPlacementType");
            require(options && combo && options->isVisible() && !options->isModal()
                && options->findChildren<QDialogButtonBox*>().isEmpty()
                && viewport->CurrentTool() == mode,
                "Primitive must start drawing immediately without OK");
            combo->setCurrentIndex(0);
            // This regression exercises explicit coordinate-plane placement,
            // independent of the automatic face choice tested above.
            viewport->SetSolidPrimitivePlacement(OpenGLViewport::SketchPlane::XY, false);
            completed.clear();
            click(*viewport, {8, 12, 0});
            require(completed.empty(), "First click must only set the anchor");
            combo->setCurrentIndex(plane);
            require(window.statusBar()->currentMessage().contains(cylinder ? "radius" : "opposite"),
                "Changing plane must retain the first click");
            click(*viewport, plane == 0 ? Vec3{18, 22, 0}
                : plane == 1 ? Vec3{18, 0, 6} : Vec3{0, 22, 6});
            require(!completed.empty() && !options->isVisible(),
                "Second click must finish the base and hide placement options");
            const char* normal = plane == 0 ? "axis.n.z" : plane == 1 ? "axis.n.y" : "axis.n.x";
            const char* zero_coordinate = plane == 0 ? "origin.z" : plane == 1 ? "origin.y" : "origin.x";
            require(parameter(completed, normal) == 1
                && std::fabs(parameter(completed, zero_coordinate)) < 1e-5,
                "Saved placement must use the changed plane");
            require(std::fabs(parameter(completed, plane == 2 ? "origin.y" : "origin.x")
                - (plane == 2 ? 12.0 : 8.0)) < 0.5,
                "First click must be projected to the new plane");
            bool saved = false;
            for (auto* panel : window.findChildren<PropertyPanel*>()) {
                const auto& active = panel->ActiveObject();
                if (active.tool_id == key && !active.parameters.empty()) {
                    saved = parameter(active.parameters, normal) == 1;
                }
            }
            if (cylinder) for (auto* panel : window.findChildren<PropertyPanel*>()) {
                if (panel->ActiveObject().tool_id != key) continue;
                for (const char* id : {"diameter", "height"}) for (bool base : {false, true}) {
                    const auto before = panel->ActiveObject();
                    const double old_value = parameter(before.parameters, id);
                    viewport->SolidDimensionGripChanged(int(before.operation_index), id, old_value + 2, false, base);
                    const auto after = panel->ActiveObject();
                    require(std::fabs(parameter(after.parameters,id)-old_value-2)<1e-8,"Cylinder handle did not resize");
                    const bool diameter = std::string(id) == "diameter";
                    const double shift = diameter ? (base ? -1 : 1) : (base ? -2 : 0);
                    for (const char* coord : {"x","y","z"}) {
                        const std::string origin = std::string("origin.")+coord;
                        const std::string axis = std::string(diameter ? "axis.u." : "axis.n.")+coord;
                        require(std::fabs(parameter(after.parameters,origin.c_str())-parameter(before.parameters,origin.c_str())
                            -shift*parameter(before.parameters,axis.c_str()))<1e-7,"Cylinder opposite side moved during resize");
                    }
                    viewport->SolidDimensionGripChanged(int(before.operation_index), id, old_value, true, base);
                }
            }
            require(saved, "Created solid must expose the final placement parameters");
        }
        auto* repeat = window.findChild<QAction*>("RepeatLastCommandAction");
        repeat->trigger();
        auto* options = window.findChild<QDialog*>("SolidPlacementOptions");
        auto* combo = window.findChild<QComboBox*>("SolidPlacementType");
        require(viewport->CurrentTool() == mode && options->isVisible() && combo->currentIndex() == 2,
            "Repeat must restart drawing with the last plane for this primitive");
        options->close();
        require(viewport->CurrentTool() == ToolMode::Select, "Closing placement must cancel drawing");
        action(key)->trigger();
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(viewport, &escape);
        require(!options->isVisible() && viewport->CurrentTool() == ToolMode::Select,
            "Escape before the first click must close placement");
        action(key)->trigger();
        action("select")->trigger();
        require(!options->isVisible(), "Switching tools must hide placement");
    }

    action("SolidBeamTool")->trigger();
    PropertyPanel* beam_panel = nullptr;
    for (auto* panel : window.findChildren<PropertyPanel*>())
        if (panel->ActiveObject().tool_id == "SolidBeamTool") beam_panel = panel;
    require(beam_panel, "Beam must expose its parametric object");
    for (const char* id : {"width", "height", "length", "thick"}) {
        const auto before = beam_panel->ActiveObject();
        const double value = parameter(before.parameters, id);
        viewport->SolidDimensionGripChanged(int(before.operation_index), id, value + 0.5, false, false);
        require(std::abs(parameter(beam_panel->ActiveObject().parameters, id) - value - 0.5) < 1.e-8,
            "Beam dimension grip did not update its parameter");
        viewport->SolidDimensionGripChanged(int(before.operation_index), id, value, true, false);
    }
    action("select")->trigger();

    // Face placement needs a target in the same document as the viewport.
    CAlfaDoc document;
    TopoDS_Shape shape = BRepPrimAPI_MakeBox(30, 30, 10).Shape();
    auto solid = std::make_unique<CSolid>(shape);
    require(solid->ReBuldMesh(), "Could not mesh placement body");
    document.AddObject(std::move(solid), false);
    const auto body_id = document.GetObjects().back()->m_id;
    OpenGLViewport face_viewport;
    face_viewport.SetDocument(&document);
    face_viewport.resize(1000, 800);
    camera.orientation = {1, 0, 0, 0};
    face_viewport.SetCamera(camera);
    face_viewport.SetOrthographicProjection(true);
    QObject::connect(&face_viewport, &OpenGLViewport::SolidBoxRectangleFinished,
        &window, [&](std::vector<ToolParameter> p) { completed = std::move(p); });
    QObject::connect(&face_viewport, &OpenGLViewport::SolidCylinderCircleFinished,
        &window, [&](std::vector<ToolParameter> p) { completed = std::move(p); });
    for (bool cylinder : {false, true}) {
        completed.clear();
        if (cylinder) face_viewport.BeginSolidCylinderCircle(OpenGLViewport::SketchPlane::XY);
        else face_viewport.BeginSolidBoxRectangle(OpenGLViewport::SketchPlane::XY);
        face_viewport.SetSolidPrimitivePlacement(OpenGLViewport::SketchPlane::XY, false);
        click(face_viewport, {8, 12, 0});
        face_viewport.SetSolidPrimitivePlacement(OpenGLViewport::SketchPlane::XY, true);
        click(face_viewport, {15, 15, 10});
        require(completed.empty(), "Face click must only choose the placement face");
        click(face_viewport, {18, 22, 10});
        require(!completed.empty()
            && parameter(completed, "boolean.body_id") == body_id
            && std::fabs(parameter(completed, "origin.z") - 10) < 1e-5,
            "Face placement must preserve the anchor and record its target body");
        require(std::fabs(parameter(completed, "boolean.overlap")
            - 0.001) < 1e-6,
            "Face overlap must be fixed at 0.001 model units");
        {
            const char* height_id = cylinder ? "height" : "depth";
            const double initial_height=parameter(completed,height_id);
            Camera zoomed=camera;zoomed.distance*=2;
            face_viewport.SetCamera(zoomed);
            if(cylinder) face_viewport.BeginSolidCylinderFaceSelection();
            else face_viewport.BeginSolidBoxFaceSelection();
            click(face_viewport,{15,15,10});
            click(face_viewport,{8,12,10});
            click(face_viewport,{18,22,10});
            require(std::fabs(parameter(completed,height_id)-2*initial_height)<1e-5,
                "New box height must scale with the viewport, not stay at five units");
            const double chosen_height=parameter(completed,height_id);
            face_viewport.SetCamera(camera);
            require(parameter(completed,height_id)==chosen_height,"Camera changed an already chosen height");
        }
    }
    std::cout << "Box and Cylinder immediate drawing and live placement tests passed.\n";
    return 0;
}

int TestFacePrimitiveCut(int argc, char** argv) {
    QSurfaceFormat format;
    format.setVersion(2, 1);
    format.setProfile(QSurfaceFormat::CompatibilityProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    if(app.arguments().contains("--orbit-dimensions")) {
        CAlfaDoc local_document; CAlfaDoc* document=&local_document;
        ToolRegistry registry; OpenGLViewport widget; auto* viewport=&widget;
        viewport->SetDocument(document);viewport->resize(1000,800);
        viewport->move(-20000,-20000);viewport->show();
        viewport->SetOrthographicProjection(true);
        Camera view;view.orientation=quaternion_from_axis_angle({0,0,1},-.785398f)
            *quaternion_from_axis_angle({1,0,0},.95f);
        document->GetObjects().clear();
        auto beam_parameters = registry.Find("SolidBeamTool")->defaults;
        for (auto& p : beam_parameters) if(p.id=="length") p.value=40;
        const auto beam = registry.CreateParametricObject("SolidBeamTool", *document, beam_parameters);
        view.target={0,0,20}; view.distance=140;
        viewport->SetCamera(view); viewport->SetTool(ToolMode::Orbit);
        viewport->SetSolidDimensionEdit(beam,"length");
        app.processEvents(); viewport->grabFramebuffer();
        QtSceneRenderer projector; DomPoint start{},end{};
        require(projector.WorldToScreen({14,-10,0},view,true,viewport->width(),viewport->height(),start)
            && projector.WorldToScreen({14,-10,40},view,true,viewport->width(),viewport->height(),end),
            "Cannot project beam dimension");
        const QPointF anchor(end.x,end.y);
        const auto mouse = [&](QEvent::Type type,QPointF pos,Qt::MouseButton button,Qt::MouseButtons buttons) {
            QMouseEvent event(type,pos,pos,button,buttons,Qt::NoModifier);
            QApplication::sendEvent(viewport,&event);
        };
        mouse(QEvent::MouseMove,anchor,Qt::NoButton,Qt::NoButton);
        mouse(QEvent::MouseMove,anchor+QPointF(1,0),Qt::NoButton,Qt::NoButton);
        require(viewport->cursor().shape()==Qt::OpenHandCursor,"Orbit reset dimension hover cursor");
        int dimension_changes=0;
        const auto connection=QObject::connect(viewport,&OpenGLViewport::SolidDimensionGripChanged,
            [&](int,const QString& id,double,bool,bool){if(id=="length")++dimension_changes;});
        const auto before=viewport->GetCamera().orientation;
        const QPointF direction(end.x-start.x,end.y-start.y);
        mouse(QEvent::MouseButtonPress,anchor,Qt::LeftButton,Qt::LeftButton);
        mouse(QEvent::MouseMove,anchor+direction*0.1,Qt::NoButton,Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease,anchor+direction*0.1,Qt::LeftButton,Qt::NoButton);
        const auto after=viewport->GetCamera().orientation;
        require(dimension_changes>0,"Orbit consumed dimension drag");
        require(before.w==after.w&&before.x==after.x&&before.y==after.y&&before.z==after.z,
            "Dragging dimension rotated camera");
        QObject::disconnect(connection);
        mouse(QEvent::MouseButtonPress,{20,20},Qt::LeftButton,Qt::LeftButton);
        mouse(QEvent::MouseMove,{65,40},Qt::NoButton,Qt::LeftButton);
        mouse(QEvent::MouseButtonRelease,{65,40},Qt::LeftButton,Qt::NoButton);
        const auto rotated=viewport->GetCamera().orientation;
        require(after.w!=rotated.w||after.x!=rotated.x||after.y!=rotated.y||after.z!=rotated.z,
            "Orbit stopped working outside dimensions");
        viewport->ClearSolidDimensionEdit();
        std::cout<<"Orbit dimension hover, drag and background navigation passed.\n";
        return 0;
    }

    QTemporaryDir settings;
    QCoreApplication::setOrganizationName("Dom3DPrimitiveTests");
    QCoreApplication::setApplicationName("FacePrimitiveCutTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    MainWindow window;
    CAlfaDoc* document = GetAlfaDoc();
    const bool render = app.arguments().contains("--render");
    if (render) {
        window.resize(1200, 1000);
        window.move(-20000, -20000);
        window.show();
        app.processEvents();
    }
    require(document, "Missing UI document");
    document->GetObjects().clear();
    ToolRegistry registry;
    auto host_parameters = registry.Find("SolidBox")->defaults;
    for (auto& p : host_parameters) {
        if (p.id == "width" || p.id == "height") p.value = 60;
        if (p.id == "depth") p.value = 50;
    }
    const auto host = registry.CreateParametricObject("SolidBox", *document, host_parameters);
    const auto host_id = document->GetObjects()[host.object_index]->m_id;
    auto* viewport = window.findChild<OpenGLViewport*>();
    viewport->resize(1000, 800);
    Camera camera;
    camera.orientation = {1, 0, 0, 0};
    camera.target = {30, 30, 25};
    camera.distance = 150;
    viewport->SetCamera(camera);
    viewport->SetOrthographicProjection(true);
    for (auto* action : window.findChildren<QAction*>()) {
        if (action->property("toolKey").toString() == "SolidBox") { action->trigger(); break; }
    }
    window.findChild<QComboBox*>("SolidPlacementType")->setCurrentIndex(3);
    click(*viewport, {30, 30, 50});
    click(*viewport, {0, 0, 50});
    click(*viewport, {17.892f, 22.259f, 50});
    PropertyPanel* panel = nullptr;
    for (auto* candidate : window.findChildren<PropertyPanel*>()) {
        if (candidate->ActiveObject().tool_id == "SolidBox") { panel = candidate; break; }
    }
    require(panel, "No Box parameters after drawing on face");
    // Both handles resize the box. The base handle must hold the opposite
    // plane fixed by changing the origin together with the dimension.
    for (const auto& pair : {std::pair<const char*,const char*>{"width","axis.u."}, {"height","axis.v."}, {"depth","axis.n."}}) {
        const auto initial=panel->ActiveObject();
        const double original=parameter(initial.parameters,pair.first);
        viewport->SolidDimensionGripChanged(int(initial.operation_index),pair.first,original+2.0,false,false);
        for(const char* coord:{"x","y","z"}) {
            const std::string id=std::string("origin.")+coord;
            require(std::fabs(parameter(panel->ActiveObject().parameters,id.c_str())-parameter(initial.parameters,id.c_str()))<1e-8,
                "Far handle moved the box origin");
        }
        viewport->SolidDimensionGripChanged(int(initial.operation_index),pair.first,original,true,false);
        viewport->SolidDimensionGripChanged(int(initial.operation_index),pair.first,original+2.0,false,true);
        const auto shifted=panel->ActiveObject();
        require(std::fabs(parameter(shifted.parameters,pair.first)-original-2.0)<1e-8,"Base handle did not resize the box");
        for(const char* coord:{"x","y","z"}) {
            const std::string origin=std::string("origin.")+coord,axis=std::string(pair.second)+coord;
            require(std::fabs(parameter(shifted.parameters,origin.c_str())+2.0*parameter(initial.parameters,axis.c_str())
                -parameter(initial.parameters,origin.c_str()))<1e-8,"Base handle moved the opposite box plane");
        }
        viewport->SolidDimensionGripChanged(int(initial.operation_index),pair.first,original,true,true);
    }

    panel->UpdateParameterValue("depth", -8.571);
    panel->ParametersChanged();
    const auto volume = [](const CSolid& solid) {
        GProp_GProps p;
        BRepGProp::VolumeProperties(solid.m_Shape, p);
        return p.Mass();
    };
    const auto check_mesh = [](const CSolid& solid, const char* stage) {
        bool valid = true;
        for (int i = 0; i < solid.GetNumSurfaces(); ++i) {
            auto* surface = solid.GetSurfaceFace(i);
            require(surface && surface->pMesh3D, "Missing rendered surface");
            GProp_GProps p;
            BRepGProp::SurfaceProperties(surface->m_Face, p);
            const auto& vertices = surface->pMesh3D->GetVertices();
            double area = 0;
            for (const auto& f : surface->pMesh3D->GetFaces()) {
                if (f.deleted) continue;
                const auto count = CMesh3D::FaceVertexCount(f);
                const Vec3 a = vertices[CMesh3D::GetFaceVertexIndex(f, 0)];
                for (size_t k = 1; k + 1 < count; ++k) {
                    const Vec3 b = vertices[CMesh3D::GetFaceVertexIndex(f, k)];
                    const Vec3 c = vertices[CMesh3D::GetFaceVertexIndex(f, k + 1)];
                    const Vec3 n = cross(b - a, c - a);
                    area += 0.5 * std::sqrt(dot(n, n));
                }
            }
            std::cerr << stage << " face " << i << " CAD=" << p.Mass() << " mesh=" << area << '\n';
            valid = valid && std::fabs(area - p.Mass()) < std::max(0.01, p.Mass() * 1e-5);
        }
        return valid;
    };
    auto* body = dynamic_cast<CSolid*>(document->FindObjectById(host_id));
    require(body && volume(*body) < 180000, "Negative height must cut the host in preview");
    const double preview_volume = volume(*body);
    const auto capture = [&](const char* stage) {
        if (!render) return;
        Camera view = camera;
        view.orientation = quaternion_from_axis_angle({0, 0, 1}, -0.785398f)
            * quaternion_from_axis_angle({1, 0, 0}, 0.95f);
        viewport->SetCamera(view);
        CSolid::SetDisplayMode(SolidDisplayMode::SurfacesAndEdges);
        CMesh3D::SetDisplayMode(MeshDisplayMode::SurfaceColored);
        viewport->update();
        app.processEvents();
        if (std::string(stage)=="preview") {
            const auto frame=viewport->grabFramebuffer();
            require(frame.save("C:/My_projects/Dom3D_Pro/output/box-union-failed/box-handles.png"),"Cannot capture dimension handles");
            unsigned yellow=0;
            for(int y=60;y<frame.height();++y) for(int x=0;x<frame.width();++x) {
                const auto c=frame.pixelColor(x,y);
                if(c.red()>200 && c.green()>150 && c.blue()<70)++yellow;
            }
            require(yellow>100,"Filled box arrows are not visible in the GL framebuffer");
        }
        require(viewport->CaptureSceneImage(QSize(900, 900)).save(
            QString("C:/My_projects/Dom3D_Pro/tmp/boolean-tool/face-cut-%1.png").arg(stage)),
            "Could not capture face cut");
    };
    capture("preview");
    require(check_mesh(*body, "preview"), "Preview mesh must match the trimmed faces");
    panel->Accepted();
    body = dynamic_cast<CSolid*>(document->FindObjectById(host_id));
    require(body && document->GetObjects().size() == 1
        && std::fabs(volume(*body) - preview_volume) < 0.01,
        "Commit must keep the preview geometry and remove its cutter");
    capture("commit");
    require(check_mesh(*body, "commit"), "Committed mesh must match the trimmed faces");
    if (app.arguments().contains("--quadro")) {
        body->MeshQuadro = true;
        body->MeshQuadroHoleSLX = true;
        require(body->ReBuldMesh(2.0f), "Could not rebuild the cut in Quadro mode");
        capture("quadro");
        require(check_mesh(*body, "quadro"), "Quadro mesh must match the trimmed faces");
    }
    if (render) {
        document->GetObjects().clear();
        TopoDS_Shape shape=BRepPrimAPI_MakeBox(20,20,20).Shape();
        auto block=std::make_unique<CSolid>(shape);require(block->ReBuldMesh(),"Cannot mesh fillet arrow fixture");
        document->AddObject(std::move(block));
        Camera view=camera;view.target={10,10,10};view.distance=65;
        view.orientation=quaternion_from_axis_angle({0,0,1},-0.785398f)*quaternion_from_axis_angle({1,0,0},0.95f);
        viewport->ClearSolidDimensionEdit();viewport->SetCamera(view);
        viewport->SetTool(ToolMode::Select);viewport->SetSelectionMode(SelectionMode::Edge);
        click(*viewport,{7,0,20});
        require(document->HasSelectedSolidEdge(),"Cannot pick fillet arrow fixture edge");
        require(document->BeginLiveFilletSelectedEdges(false)&&document->UpdateLiveFillet(1),"Cannot build fillet arrow fixture");
        ActiveParametricObject fillet;fillet.tool_id="fillet_edge";fillet.object_index=0;
        fillet.parameters=registry.Find("fillet_edge")->defaults;
        for(auto& p:fillet.parameters)if(p.id=="radius")p.value=1;
        viewport->SetSolidDimensionEdit(fillet,"radius");
        app.processEvents();
        const auto frame=viewport->grabFramebuffer();
        require(frame.save("C:/My_projects/Dom3D_Pro/output/box-union-failed/fillet-arrow.png"),"Cannot capture fillet arrow");
        DomPoint pick;QtSceneRenderer projector;
        require(projector.WorldToScreen({7,0,20},view,true,viewport->width(),viewport->height(),pick),"Cannot project picked edge");
        bool yellow=false;
        for(int y=std::max(0,pick.y-5);y<std::min(frame.height(),pick.y+6);++y)
            for(int x=std::max(0,pick.x-5);x<std::min(frame.width(),pick.x+6);++x){auto c=frame.pixelColor(x,y);yellow|=c.red()>245&&c.green()>200&&c.blue()<20;}
        require(yellow,"Fillet arrow does not start at the picked edge");
        const auto tip = [&](const QImage& image) {
            QPointF result;double farthest=0;
            for(int y=std::max(60,pick.y-150);y<std::min(image.height(),pick.y+151);++y)
                for(int x=std::max(0,pick.x-150);x<std::min(image.width(),pick.x+151);++x) {
                    const auto c=image.pixelColor(x,y);
                    if(c.red()>245&&c.green()>200&&c.blue()<20) {
                        const double distance=std::hypot(x-pick.x,y-pick.y);
                        if(distance>farthest){farthest=distance;result=QPointF(x-pick.x,y-pick.y);}
                    }
                }
            return result;
        };
        const auto first_tip=tip(frame);
        DomPoint along;
        require(projector.WorldToScreen({8,0,20},view,true,viewport->width(),viewport->height(),along),"Cannot project edge tangent");
        const double tx=along.x-pick.x,ty=along.y-pick.y;
        require(std::fabs(first_tip.x()*ty-first_tip.y()*tx)/std::hypot(tx,ty)<3,
            "Radius handle does not follow the picked edge tangent");
        // A one-unit radius cannot project longer than one model unit in
        // this orthographic view, regardless of the chosen direction.
        const double pixels_per_unit=viewport->height()/(2.0*view.distance*0.42);
        require(std::hypot(first_tip.x(),first_tip.y())<=pixels_per_unit+3,
            "Radius arrow uses an artificial scale instead of model units");
        require(document->UpdateLiveFillet(2),"Cannot enlarge fillet fixture");
        for(auto& p:fillet.parameters)if(p.id=="radius")p.value=2;
        viewport->SetSolidDimensionEdit(fillet,"radius");app.processEvents();
        const auto larger=viewport->grabFramebuffer();
        const auto second_tip=tip(larger);
        const double first_length=std::hypot(first_tip.x(),first_tip.y()),second_length=std::hypot(second_tip.x(),second_tip.y());
        std::cerr<<"Arrow lengths "<<first_length<<" "<<second_length<<"\n";
        require(first_length>1 && std::fabs(second_length-2*first_length)<4,
            "Radius arrow endpoint did not move with the radius");
        require(std::fabs(first_tip.x()*second_tip.y()-first_tip.y()*second_tip.x())/second_length<4,
            "Radius arrow direction jumped after rebuilding the fillet");
        require(larger.save("C:/My_projects/Dom3D_Pro/output/box-union-failed/fillet-arrow-larger.png"),"Cannot capture enlarged radius handle");

        document->CancelLiveFillet();viewport->ClearSolidDimensionEdit();
        {
        click(*viewport,{7,0,20});
        require(document->HasSelectedSolidEdge(),"Cannot pick fillet arrow fixture edge");
        require(document->BeginLiveChamferSelectedEdges()&&document->UpdateLiveChamfer(1),"Cannot build fillet arrow fixture");
        ActiveParametricObject fillet;fillet.tool_id="ChamferSolid";fillet.object_index=0;
        fillet.parameters=registry.Find("ChamferSolid")->defaults;
        for(auto& p:fillet.parameters)if(p.id=="distance")p.value=1;
        viewport->SetSolidDimensionEdit(fillet,"distance");
        app.processEvents();
        const auto frame=viewport->grabFramebuffer();
        require(frame.save("C:/My_projects/Dom3D_Pro/output/box-union-failed/chamfer-arrow.png"),"Cannot capture fillet arrow");
        DomPoint pick;QtSceneRenderer projector;
        require(projector.WorldToScreen({7,0,20},view,true,viewport->width(),viewport->height(),pick),"Cannot project picked edge");
        bool yellow=false;
        for(int y=std::max(0,pick.y-5);y<std::min(frame.height(),pick.y+6);++y)
            for(int x=std::max(0,pick.x-5);x<std::min(frame.width(),pick.x+6);++x){auto c=frame.pixelColor(x,y);yellow|=c.red()>245&&c.green()>200&&c.blue()<20;}
        require(yellow,"Chamfer arrow does not start at the picked edge");
        const auto tip = [&](const QImage& image) {
            QPointF result;double farthest=0;
            for(int y=std::max(60,pick.y-150);y<std::min(image.height(),pick.y+151);++y)
                for(int x=std::max(0,pick.x-150);x<std::min(image.width(),pick.x+151);++x) {
                    const auto c=image.pixelColor(x,y);
                    if(c.red()>245&&c.green()>200&&c.blue()<20) {
                        const double distance=std::hypot(x-pick.x,y-pick.y);
                        if(distance>farthest){farthest=distance;result=QPointF(x-pick.x,y-pick.y);}
                    }
                }
            return result;
        };
        const auto first_tip=tip(frame);
        DomPoint along;
        require(projector.WorldToScreen({8,0,20},view,true,viewport->width(),viewport->height(),along),"Cannot project edge tangent");
        const double tx=along.x-pick.x,ty=along.y-pick.y;
        require(std::fabs(first_tip.x()*ty-first_tip.y()*tx)/std::hypot(tx,ty)<3,
            "Radius handle does not follow the picked edge tangent");
        // A one-unit radius cannot project longer than one model unit in
        // this orthographic view, regardless of the chosen direction.
        const double pixels_per_unit=viewport->height()/(2.0*view.distance*0.42);
        require(std::hypot(first_tip.x(),first_tip.y())<=pixels_per_unit+3,
            "Radius arrow uses an artificial scale instead of model units");
        require(document->UpdateLiveChamfer(2),"Cannot enlarge chamfer fixture");
        for(auto& p:fillet.parameters)if(p.id=="distance")p.value=2;
        viewport->SetSolidDimensionEdit(fillet,"distance");app.processEvents();
        const auto larger=viewport->grabFramebuffer();
        const auto second_tip=tip(larger);
        const double first_length=std::hypot(first_tip.x(),first_tip.y()),second_length=std::hypot(second_tip.x(),second_tip.y());
        std::cerr<<"Arrow lengths "<<first_length<<" "<<second_length<<"\n";
        require(first_length>1 && std::fabs(second_length-2*first_length)<4,
            "Radius arrow endpoint did not move with the radius");
        require(std::fabs(first_tip.x()*second_tip.y()-first_tip.y()*second_tip.x())/second_length<4,
            "Radius arrow direction jumped after rebuilding the fillet");
        require(larger.save("C:/My_projects/Dom3D_Pro/output/box-union-failed/chamfer-arrow-larger.png"),"Cannot capture enlarged radius handle");

        double dragged_distance = 0;
        bool drag_finished = false;
        const auto drag_connection = QObject::connect(viewport, &OpenGLViewport::SolidDimensionGripChanged,
            &window, [&](int, const QString& id, double value, bool finished, bool) {
                if (id == "distance") { dragged_distance = value; drag_finished = finished; }
            });
        const QPointF direction = second_tip / second_length;
        const QPointF grip = QPointF(pick.x, pick.y) + second_tip - direction * 7.5;
        const QPointF moved = grip + second_tip;
        QMouseEvent press(QEvent::MouseButtonPress, grip, grip, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &press);
        QMouseEvent move(QEvent::MouseMove, moved, moved, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, moved, moved, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(viewport, &release);
        QObject::disconnect(drag_connection);
        require(drag_finished && std::fabs(dragged_distance - 4.0) < 0.3,
            "Chamfer drag distance does not match the projected model scale");
        document->CancelLiveChamfer();viewport->ClearSolidDimensionEdit();
        }

        document->GetObjects().clear();
        TopoDS_Shape cylinder_shape = BRepPrimAPI_MakeCylinder(10,20).Shape();
        auto cylinder_body = std::make_unique<CSolid>(cylinder_shape);
        require(cylinder_body->ReBuldMesh(), "Cannot mesh cylinder handle fixture");
        document->AddObject(std::move(cylinder_body));
        ActiveParametricObject cylinder; cylinder.tool_id="SolidCylinder"; cylinder.object_index=0;
        cylinder.parameters=registry.Find("SolidCylinder")->defaults;
        for(auto& p:cylinder.parameters) {
            if(p.id=="diameter" || p.id=="height") p.value=20;
        }
        view.target={0,0,10};viewport->SetCamera(view);
        viewport->SetSolidDimensionEdit(cylinder,"height");app.processEvents();
        require(viewport->grabFramebuffer().save("C:/My_projects/Dom3D_Pro/output/box-union-failed/cylinder-handles.png"),
            "Cannot capture cylinder handles");
        viewport->ClearSolidDimensionEdit();
        document->GetObjects().clear();
        auto prism_parameters = registry.Find("SolidPrismTool")->defaults;
        for (auto& p : prism_parameters) {
            if (p.id == "length") p.value = 23;
            if (p.id == "height") p.value = 44;
            if (p.id == "axis") p.value = 1;
        }
        const auto prism = registry.CreateParametricObject("SolidPrismTool", *document, prism_parameters);
        view.target={0,22,0};view.distance=140;viewport->SetCamera(view);
        viewport->SetSolidDimensionEdit(prism,"height");app.processEvents();
        require(viewport->grabFramebuffer().save("C:/My_projects/Dom3D_Pro/output/box-union-failed/prism-handles.png"),
            "Cannot capture Prism handles");
        viewport->ClearSolidDimensionEdit();

    }
    std::cout << "Face primitive preview and committed mesh agree.\n";
    return 0;
}
