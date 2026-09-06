#include "ui/MainWindow.h"
#include "solid/Solid.h"
#include "solid/SurfaceFace.h"
#include "CMesh3D.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSettings>
#include <QStatusBar>
#include <QTemporaryDir>
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
    require(renderer.WorldToScreen(world, viewport.GetCamera(), true,
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
