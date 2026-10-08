#include "CAlfaDoc.h"
#include <QSettings>
#include "CPolyline.h"
#include "Dom3DProjectSerializer.h"
#include "solid/Solid.h"
#include "solid/AssociativeClone.h"
#include "ui/ToolRegistry.h"
#include "ui/DraftingWorkspace.h"
#include "ui/OpenGLViewport.h"
#include <QApplication>
#include <QDir>
#include <QGraphicsView>
#include <QGraphicsPathItem>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPainter>
#include <QPdfWriter>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QDomDocument>
#include <QFile>
#include <QMouseEvent>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void set(std::vector<ToolParameter>& params, const char* id, double value) {
    for (auto& p : params) if (p.id == id) { p.value = value; return; }
    std::cerr << "Missing parameter: " << id << std::endl;
    throw std::runtime_error(id);
}
CSolid& body(CAlfaDoc& doc, size_t i) {
    auto* solid = dynamic_cast<CSolid*>(doc.GetObjects().at(i).get());
    if (!solid) std::cerr << "Missing solid at index " << i << " of " << doc.GetObjects().size() << std::endl;
    check(solid != nullptr, "Missing solid"); return *solid;
}
void same(const std::vector<SolidCenterline>& a, const std::vector<SolidCenterline>& b) {
    check(a.size() == b.size(), "Centerline count changed");
    for (size_t i = 0; i < a.size(); ++i) {
        check(a[i].id == b[i].id && a[i].kind == b[i].kind && a[i].closed == b[i].closed,
              "Centerline identity changed");
        check(a[i].points.size() == b[i].points.size(), "Centerline samples changed");
        for (size_t j = 0; j < a[i].points.size(); ++j)
            check(a[i].points[j].Distance(b[i].points[j]) < 1e-9, "Centerline coordinates changed");
    }
}
}

int TestSolidCenterlines(int argc, char** argv) try {
    QApplication app(argc, argv);
    QTemporaryDir temporary;
    // The snap assertion must not inherit the user's disabled snapping.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    const QString out = argc > 2 ? QString::fromLocal8Bit(argv[2]) : temporary.path();
    check(QDir().mkpath(out), "Cannot create centerline test output");
    CAlfaDoc doc;
    ToolRegistry tools;
    auto params = tools.Find("SolidCylinder")->defaults;
    set(params, "diameter", 30); set(params, "height", 40);
    const auto active = tools.CreateParametricObject("SolidCylinder", doc, params);
    const size_t index = active.object_index;
    auto& cylinder = body(doc, index);
    check(cylinder.GetCenterlines().size() == 1, "Cylinder axis not generated");
    check(cylinder.GetCenterlines()[0].points[0].Distance(gp_Pnt(0,0,0)) < 1e-8
       && cylinder.GetCenterlines()[0].points[1].Distance(gp_Pnt(0,0,40)) < 1e-8, "Wrong cylinder axis");

    auto hole = tools.Find("SolidHole")->defaults;
    set(hole, "diameter", 4); set(hole, "hole.center.x", 7); set(hole, "hole.center.z", 40);
    const auto through = tools.ApplyHole(doc, cylinder.m_id, hole);
    check(!through.tool_id.empty(), "Through hole failed");
    check(cylinder.GetCenterlines().size() == 2, "Through-hole axis missing");
    check(std::abs(cylinder.GetCenterlines()[1].points[1].Z()) < 1e-6, "Through axis misses exit");
    set(hole, "hole.center.x", -7); set(hole, "hole_type", 1); set(hole, "depth", 10);
    check(!tools.ApplyHole(doc, cylinder.m_id, hole).tool_id.empty(), "Blind hole failed");
    check(std::abs(cylinder.GetCenterlines()[2].points[1].Z() - 30) < 1e-6, "Blind axis ignores depth");
    const auto expected = cylinder.GetCenterlines();
    check(tools.ReplayOperations(index, doc), "Centerline replay failed");
    same(expected, body(doc,index).GetCenterlines());
    check(tools.ReplayOperations(index, doc), "Repeated replay failed");
    same(expected, body(doc,index).GetCenterlines());
    // Changing depth rebuilds that operation instead of appending stale axes.
    auto edit = tools.ActiveObjectFromDocument(index, body(doc,index), 2, &doc);
    set(edit.parameters, "depth", 15); tools.Rebuild(edit, doc);
    check(body(doc,index).GetCenterlines().size() == 3
       && std::abs(body(doc,index).GetCenterlines()[2].points[1].Z() - 25) < 1e-6, "Edited hole axis is stale");

    const auto base = body(doc,index).GetCenterlines();
    Vec3 pickedStart{}, pickedEnd{};
    const auto projectAxis = [](Vec3 p, DomPoint& screen) {
        screen = {int(std::lround(p.x*10)),int(std::lround(p.z*10))}; return true;
    };
    check(doc.FindRotationAxisLineAtScreen({70,200},projectAxis,3,pickedStart,pickedEnd), "Hole axis missing from straight-line search");
    check(std::abs(pickedStart.x-7)<1e-6 && std::abs(pickedEnd.x-7)<1e-6, "Line search selected the wrong axis");
    body(doc,index).SetVisible(false);
    check(!doc.FindRotationAxisLineAtScreen({70,200},projectAxis,3,pickedStart,pickedEnd), "Hidden solid axes remain selectable");
    body(doc,index).SetVisible(true);
    auto clone = body(doc,index).Clone();
    auto* copied = dynamic_cast<CSolid*>(clone.get()); check(copied, "Clone failed");
    same(base, copied->GetCenterlines());
    copied->Translate({10,20,30});
    check(copied->GetCenterlines()[0].points[0].Distance(gp_Pnt(10,20,30)) < 1e-8, "Move left axes behind");
    copied->Rotate({}, {0,1,0}, float(3.14159265358979323846/2));
    check(copied->GetCenterlines()[0].points[0].Distance(gp_Pnt(30,20,-10)) < 1e-4, "Rotate left axes behind");
    copied->Scale({}, {}, 2);
    copied->Mirror({}, {1,0,0});
    check(copied->GetCenterlines()[0].points[0].Distance(gp_Pnt(-60,40,-20)) < 1e-4, "Scale/mirror left axes behind");
    const auto snapshot = copied->GetCenterlines();
    copied->PreviewTranslate({1,2,3}); same(snapshot, copied->GetCenterlines());
    check(copied->GetDisplayCenterlines()[0].points[0].Distance(snapshot[0].points[0].Translated(gp_Vec(1,2,3))) < 1e-8,
          "3D axis did not follow the movement preview");
    check(copied->CommitPreviewTranslate({1,2,3}, false), "Commit move failed");
    check(copied->GetCenterlines()[0].points[0].Distance(snapshot[0].points[0].Translated(gp_Vec(1,2,3))) < 1e-8,
          "Preview commit moved axes incorrectly");
    CAssociativeClone associative(body(doc,index).m_Shape, body(doc,index).m_id);
    check(associative.RebuildFromSource(body(doc,index)), "Associative rebuild failed");
    same(base, associative.GetCenterlines());
    CAlfaDoc booleanDoc;
    auto outer = tools.CreateParametricObject("SolidCylinder",booleanDoc,params);
    auto cutterParams = params; set(cutterParams,"diameter",4); set(cutterParams,"height",80);
    set(cutterParams,"origin.x",7); set(cutterParams,"origin.z",-20);
    auto cutter = tools.CreateParametricObject("SolidCylinder",booleanDoc,cutterParams);
    check(booleanDoc.ApplyBooleanToSolids(outer.object_index,cutter.object_index,BooleanOperation::Cut), "Boolean cut failed");
    const auto booleanAxes = body(booleanDoc,outer.object_index).GetCenterlines();
    check(booleanAxes.size() == 2 && booleanAxes[1].kind == SolidCenterlineKind::HoleAxis,
          "Boolean did not preserve/convert cutter axis");
    check(std::abs(booleanAxes[1].points[0].Z()) < 1e-6 && std::abs(booleanAxes[1].points[1].Z()-40) < 1e-6,
          "Boolean axis was not clipped to the body");
    check(tools.ReplayOperations(outer.object_index,booleanDoc), "Boolean replay failed");
    same(booleanAxes,body(booleanDoc,outer.object_index).GetCenterlines());

    // Drawing uses separate center geometry and does not alter visible/hidden edges.
    QVector<QPolygonF> visible, hidden, centers;
    QString error;
    check(DraftingWorkspace::BuildModelView(&doc, "front", 1, false, visible, hidden, error, &centers), "Front projection failed");
    check(centers.size() == 3 && hidden.empty(), "Missing front axes or hidden toggle affected them");
    check(std::abs(QLineF(centers[0].front(),centers[0].back()).length() - 44) < 1e-5, "Axis must extend 5 percent at each end");
    check(DraftingWorkspace::BuildModelView(&doc, "front", 2, false, visible, hidden, error, &centers), "Scaled projection failed");
    check(std::abs(QLineF(centers[0].front(),centers[0].back()).length() - 22) < 1e-5, "Axis extension must follow view scale");
    check(DraftingWorkspace::BuildModelView(&doc, "top", 1, false, visible, hidden, error, &centers), "Top projection failed");
    check(centers.size() == 6, "Axial view needs three center marks");
    for (int i = 0; i < centers.size(); ++i)
        check(std::abs(QLineF(centers[i].front(),centers[i].back()).length() - (i < 2 ? 33 : 4.4)) < 1e-6,
              "Center cross must extend 5 percent beyond the circle");
    body(doc,index).Translate({0,5,0});
    const auto moved = body(doc,index).GetCenterlines();
    check(tools.ReplayOperations(index,doc), "Transformed body replay failed");
    same(moved,body(doc,index).GetCenterlines());

    auto path = std::make_unique<CPolyline>();
    path->AddPoint({55,0,0}); path->AddPoint({55,0,30}); path->AddPoint({70,0,45});
    auto* path_ptr = path.get(); doc.AddObject(std::move(path));
    check(doc.CreateWireSolid(path_ptr->m_id, 2), "Wire failed");
    auto& wire = body(doc, doc.GetObjects().size()-1);
    check(wire.GetCenterlines().size() == 1 && wire.GetCenterlines()[0].kind == SolidCenterlineKind::Path,
          "Wire center path missing");
    check(wire.GetCenterlines()[0].points.front().Distance(gp_Pnt(55,0,0)) < 1e-8
       && wire.GetCenterlines()[0].points.back().Distance(gp_Pnt(70,0,45)) < 1e-8, "Wrong wire spine");
    check(tools.ReplayOperations(doc.GetObjects().size()-1, doc), "Wire replay failed");

    QJsonArray views;
    views.push_back(QJsonObject{{"id",1},{"type","model_view"},{"projection","front"},{"viewScale",1},
                               {"x1",70},{"y1",75},{"showHidden",false}});
    views.push_back(QJsonObject{{"id",2},{"type","model_view"},{"projection","top"},{"viewScale",1},
                               {"x1",170},{"y1",75},{"showHidden",false}});
    const QJsonObject sheet{{"name","Centerlines"},{"format","A4"},{"landscape",true},{"primitives",views}};
    doc.SetDraftingData(QJsonDocument(QJsonObject{{"sheets",QJsonArray{sheet}}}).toJson(QJsonDocument::Compact).toStdString());
    check(DraftingWorkspace::RefreshModelViews(doc,error), "Cannot refresh centerline drawing");
    check(doc.GetDraftingData().find("centerLines") != std::string::npos, "Drawing did not serialize centerlines");
    Dom3DProjectSerializer serializer;
    ProjectViewState camera;
    check(serializer.Save(out + "/Solid_Centerlines.dom3d", doc, "Solid", camera, {}, error), "Save failed");
    CAlfaDoc loaded; QString room;
    check(serializer.Load(out + "/Solid_Centerlines.dom3d", loaded, room, camera, error), "Reload failed");
    for (size_t i = 0; i < doc.GetObjects().size(); ++i)
        if (dynamic_cast<CSolid*>(doc.GetObjects()[i].get())) same(body(doc,i).GetCenterlines(), body(loaded,i).GetCenterlines());
    check(loaded.GetDraftingData() == doc.GetDraftingData(), "Drawing changed on reload");
    // Old projects acquire metadata without replacing their saved BRep or
    // changing a fillet's historical face indices.
    QFile savedFile(out + "/Solid_Centerlines.dom3d"); check(savedFile.open(QIODevice::ReadOnly), "Cannot read saved fixture");
    QDomDocument legacyXml; check(bool(legacyXml.setContent(savedFile.readAll())), "Cannot parse saved fixture"); savedFile.close();
    auto nodes = legacyXml.elementsByTagName("centerlines");
    while (!nodes.isEmpty()) { auto node = nodes.at(0); node.parentNode().removeChild(node); nodes = legacyXml.elementsByTagName("centerlines"); }
    QFile legacyFile(out + "/legacy.dom3d"); check(legacyFile.open(QIODevice::WriteOnly), "Cannot create legacy fixture");
    legacyFile.write(legacyXml.toByteArray()); legacyFile.close();
    CAlfaDoc migrated; check(serializer.Load(out + "/legacy.dom3d",migrated,room,camera,error), "Legacy migration failed");
    same(body(doc,index).GetCenterlines(),body(migrated,index).GetCenterlines());
    same(body(doc,doc.GetObjects().size()-1).GetCenterlines(),body(migrated,doc.GetObjects().size()-1).GetCenterlines());

    // A current-format but stale empty cache must not suppress solid axes.
    auto stale = QJsonDocument::fromJson(QByteArray::fromStdString(loaded.GetDraftingData())).object();
    auto staleSheets = stale.value("sheets").toArray();
    auto staleSheet = staleSheets[0].toObject(); auto staleViews = staleSheet.value("primitives").toArray();
    for (int i = 0; i < staleViews.size(); ++i) {
        auto view = staleViews[i].toObject(); view.insert("centerLines", QJsonArray{});
        view.insert("centerLinesVersion", 2); staleViews[i] = view;
    }
    staleSheet.insert("primitives", staleViews); staleSheets[0] = staleSheet; stale.insert("sheets", staleSheets);
    loaded.SetDraftingData(QJsonDocument(stale).toJson(QJsonDocument::Compact).toStdString());
    DraftingWorkspace workspace; workspace.SetDocument(&loaded);
    auto* tabs = workspace.findChild<QTabWidget*>(); check(tabs && tabs->count(), "Missing drawing sheet");
    auto* view = tabs->widget(0)->findChild<QGraphicsView*>(); check(view, "Missing drawing view");
    int styled = 0;
    for (auto* item : view->scene()->items()) if (item->data(1).toString() == "centerline") {
        const auto* curve = dynamic_cast<QGraphicsPathItem*>(item);
        check(curve && curve->pen().style() == Qt::CustomDashLine && curve->pen().widthF() < 0.3,
              "Centerline pen is not thin dash-dot"); ++styled;
    }
    check(styled >= 9, "Centerline graphics missing after reload");
    {
        loaded.ClearSelection();
        for (const auto& object : loaded.GetObjects())
            if (auto* solid = dynamic_cast<CSolid*>(object.get())) solid->SetColor({0.69f,0.56f,0.70f});
        OpenGLViewport viewport; viewport.resize(1000,800); viewport.move(-20000,-20000);
        viewport.SetDocument(&loaded); viewport.SetOrthographicProjection(true);
        viewport.SetFloorGridVisible(false); viewport.SetCoordinateAxesVisible(false); viewport.FitToDocument();
        CSolid::SetDisplayMode(SolidDisplayMode::Wireframe);
        viewport.show(); app.processEvents();
        check(viewport.CaptureSceneImage({1000,800}).save(out + "/axes-3d.png"), "3D axis rendering failed");
        QtSceneRenderer projector;
        const auto target = body(loaded,index).GetCenterlines()[0].points.front();
        DomPoint screen{};
        check(projector.WorldToScreen({float(target.X()),float(target.Y()),float(target.Z())},
              viewport.GetCamera(),true,1000,800,screen), "Cannot project axis snap target");
        bool snapped = false; CPoint3d picked;
        QObject::connect(&viewport,&OpenGLViewport::Point3DPicked,[&](CPoint3d p) { snapped = true; picked = p; });
        viewport.BeginPick3DPoint();
        const QPointF click(screen.x,screen.y);
        QMouseEvent press(QEvent::MouseButtonPress,click,click,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(&viewport,&press);
        QMouseEvent release(QEvent::MouseButtonRelease,click,click,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(&viewport,&release);
        if (!snapped || gp_Pnt(picked.x,picked.y,picked.z).Distance(target)>=1e-6)
            std::cerr << "Snap result " << snapped << ": " << picked.x << "," << picked.y << "," << picked.z
                      << " expected " << target.X() << "," << target.Y() << "," << target.Z() << std::endl;
        check(snapped && gp_Pnt(picked.x,picked.y,picked.z).Distance(target)<1e-6,
              "3D point picking did not snap to the axis endpoint");
        viewport.SetDocument(nullptr);
    }
    QImage image(1485,1050,QImage::Format_ARGB32_Premultiplied); image.fill(Qt::white);
    QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
    view->scene()->render(&painter,QRectF(0,0,1485,1050),QRectF(0,0,297,210)); painter.end();
    check(image.save(out + "/drawing.png"), "Cannot save centerline preview");
    QPdfWriter pdf(out + "/drawing.pdf"); pdf.setPageSize(QPageSize(QPageSize::A4));
    pdf.setPageOrientation(QPageLayout::Landscape);
    QPainter paper(&pdf); view->scene()->render(&paper,QRectF(0,0,pdf.width(),pdf.height()),QRectF(0,0,297,210)); paper.end();
    if (argc > 3) {
        CAlfaDoc actual;
        check(serializer.Load(QString::fromLocal8Bit(argv[3]), actual, room, camera, error), "Cannot load actual hole fixture");
        bool fullDepth = false;
        for (const auto& object : actual.GetObjects()) if (const auto* solid = dynamic_cast<const CSolid*>(object.get()))
            for (const auto& axis : solid->GetCenterlines())
                if (axis.kind == SolidCenterlineKind::HoleAxis && axis.id == "hole:1")
                    fullDepth = axis.points.front().Distance(axis.points.back()) > 60;
        check(fullDepth, "Saved through-hole axis still ends inside the actual body");
        DraftingWorkspace actualWorkspace; actualWorkspace.SetDocument(&actual);
        auto* actualTabs = actualWorkspace.findChild<QTabWidget*>();
        check(actualTabs && actualTabs->count(), "Actual drawing missing");
        auto* actualView = actualTabs->widget(actualTabs->count()-1)->findChild<QGraphicsView*>();
        check(actualView, "Actual drawing view missing");
        int axes = 0;
        for (auto* item : actualView->scene()->items()) if (item->data(1).toString() == "centerline") ++axes;
        check(axes >= 8, "Actual hole axes were not recovered on opening");
        QImage preview(1485,1050,QImage::Format_ARGB32_Premultiplied); preview.fill(Qt::white);
        QPainter paint(&preview); paint.setRenderHint(QPainter::Antialiasing);
        actualView->scene()->render(&paint,QRectF(0,0,1485,1050),QRectF(0,0,420,297)); paint.end();
        check(preview.save(out + "/Extrude_And_Hole.png"), "Cannot save actual drawing preview");
        check(DraftingWorkspace::RefreshModelViews(actual,error), "Cannot refresh actual file");
        check(serializer.Save(out + "/Extrude_And_Hole_fixed.dom3d",actual,room,camera,{},error), "Cannot save corrected file");
    }
    std::cout << "Solid axes, hole edits, path, transforms, replay, persistence and drawing passed.\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "SolidCenterlines: " << e.what() << std::endl;
    return 1;
}
