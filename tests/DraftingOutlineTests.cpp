#include "ui/DraftingWorkspace.h"
#include "CAlfaDoc.h"
#include "Dom3DProjectSerializer.h"
#include "solid/Solid.h"
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <QApplication>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QTabWidget>
#include <QStackedWidget>
#include <QPainter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <iostream>
#include <stdexcept>
#include <cmath>

namespace {
void check(bool value, const char* message) {
    if (!value) { std::cerr << message << std::endl; throw std::runtime_error(message); }
}
bool has_vertical(const QVector<QPolygonF>& lines, double x, double y1, double y2) {
    for (const auto& line : lines) {
        if (line.size() < 2) continue;
        bool at_x = true;
        double low = line.front().y(), high = low;
        for (const auto& p : line) {
            at_x = at_x && std::abs(p.x() - x) < 1.e-6;
            low = std::min(low, p.y());
            high = std::max(high, p.y());
        }
        if (at_x && std::abs(low - y1) < 1.e-6 && std::abs(high - y2) < 1.e-6) return true;
    }
    return false;
}
void render_sheet(CAlfaDoc& document, const QString& path) {
    DraftingWorkspace workspace;
    workspace.SetDocument(&document);
    auto* tabs = workspace.findChild<QTabWidget*>();
    check(tabs && tabs->count() > 0, "No drawing sheet to render");
    auto* view = tabs->widget(tabs->count() - 1)->findChild<QGraphicsView*>();
    check(view != nullptr, "No drawing viewport");
    auto* scene = view->scene();
    QImage image(1485, 1050, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    // The scene includes one paper width of panning space on each side.
    const QRectF paper(0, 0, scene->sceneRect().width() / 3, scene->sceneRect().height() / 3);
    scene->render(&painter, QRectF(0, 0, image.width(), image.height()), paper);
    painter.end();
    check(image.save(path), "Cannot save drawing preview");
    QImage detail(1485, 620, QImage::Format_ARGB32_Premultiplied);
    detail.fill(Qt::white);
    QPainter detail_painter(&detail);
    detail_painter.setRenderHint(QPainter::Antialiasing);
    scene->render(&detail_painter, QRectF(0, 0, 1485, 620), QRectF(20, 25, 240, 100));
    detail_painter.end();
    check(detail.save(path + ".detail.png"), "Cannot save drawing detail");
}
}

int TestDraftingOutlines(int argc, char** argv) {
    QApplication application(argc, argv);
    check(argc >= 3, "Missing drawing fixture");
    if (application.arguments().contains("--first-show-only")) {
        CAlfaDoc loaded;
        Dom3DProjectSerializer serializer;
        QString room, error;
        ProjectViewState camera;
        check(serializer.Load(QString::fromLocal8Bit(argv[2]), loaded, room, camera, error),
              "Cannot load saved drawing");
        const auto saved = loaded.GetDraftingData();
        QStackedWidget stack;
        stack.resize(1100, 760);
        auto* model = new QWidget(&stack);
        auto* drafting = new DraftingWorkspace(&stack);
        stack.addWidget(model);
        stack.addWidget(drafting);
        stack.setCurrentWidget(model);
        drafting->SetDocument(&loaded);
        const auto settle = [] {
            for (int i = 0; i < 4; ++i) QApplication::processEvents();
        };
        settle(); // Loading's queued Fit runs while Drafting is still hidden.
        stack.show();
        settle();
        stack.setCurrentWidget(drafting);
        settle();
        auto* tabs = drafting->findChild<QTabWidget*>();
        check(tabs && tabs->count() == 2, "Saved sheets were not restored");
        for (int index = 0; index < tabs->count(); ++index) {
            tabs->setCurrentIndex(index);
            settle();
            auto* view = tabs->widget(index)->findChild<QGraphicsView*>();
            check(view && !view->scene()->items().isEmpty(), "Saved drawing is empty");
            const auto rect = view->scene()->sceneRect();
            const double expected = std::min(view->viewport()->width() / (rect.width() / 3 + 16),
                view->viewport()->height() / (rect.height() / 3 + 16));
            std::cout << "sheet=" << index << " scale=" << view->transform().m11()
                      << " expected=" << expected << std::endl;
            check(view->transform().m11() > expected * 0.90
                      && view->transform().m11() < expected * 1.05,
                  "First Drafting show kept the hidden viewport's incorrect fit");
            const QRectF paper(0, 0, rect.width() / 3, rect.height() / 3);
            check(view->mapToScene(view->viewport()->rect()).boundingRect().contains(paper),
                  "Loaded drawing paper is outside the viewport");
            view->scale(1.7, 1.7);
            view->centerOn(paper.center() + QPointF(12, 17));
            const auto zoom = view->transform();
            const auto center = view->mapToScene(view->viewport()->rect().center());
            stack.setCurrentWidget(model);
            settle();
            stack.setCurrentWidget(drafting);
            settle();
            check(view->transform() == zoom
                      && QLineF(center, view->mapToScene(view->viewport()->rect().center())).length() < 1,
                  "Returning to Drafting reset the user's zoom or pan");
        }
        check(loaded.GetDraftingData() == saved, "Showing the drawing rewrote saved projection data");
        std::cout << "Drafting first show passed\n";
        return 0;
    }
    CAlfaDoc cylinder;
    TopoDS_Shape cylinder_shape = BRepPrimAPI_MakeCylinder(10, 20).Shape();
    cylinder.AddObject(std::make_unique<CSolid>(cylinder_shape));
    QVector<QPolygonF> visible, hidden;
    QString error;
    for (const QString& projection : {QString("front"), QString("right")}) {
        check(DraftingWorkspace::BuildModelView(&cylinder, projection, 1, false, visible, hidden, error),
              "Cylinder projection failed");
        check(has_vertical(visible, -10, -10, 10) && has_vertical(visible, 10, -10, 10),
              "Cylinder drawing is missing its two silhouette generators");
    }
    check(DraftingWorkspace::BuildModelView(&cylinder, "front", 2, false, visible, hidden, error),
          "Scaled cylinder projection failed");
    check(has_vertical(visible, -5, -5, 5) && has_vertical(visible, 5, -5, 5),
          "Silhouette does not follow the drawing scale");
    TopoDS_Shape box_shape = BRepPrimAPI_MakeBox(gp_Pnt(5, -30, 5), 10, 10, 10).Shape();
    cylinder.AddObject(std::make_unique<CSolid>(box_shape));
    check(DraftingWorkspace::BuildModelView(&cylinder, "front", 1, true, visible, hidden, error),
          "Occluded cylinder projection failed");
    check(has_vertical(hidden, 7.5, -5, 5), "Occluded silhouette is missing from hidden lines");
    check(!has_vertical(visible, 7.5, -10, 10), "Occluded silhouette was drawn as fully visible");
    check(DraftingWorkspace::BuildModelView(&cylinder, "front", 1, false, visible, hidden, error)
              && hidden.isEmpty(), "Disabled hidden lines retained cached geometry");

    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString room;
    ProjectViewState view;
    check(serializer.Load(QString::fromLocal8Bit(argv[2]), document, room, view, error),
          "Cannot load Wire drawing fixture");
    const auto original = QJsonDocument::fromJson(QByteArray::fromStdString(document.GetDraftingData()));
    const QString output = argc >= 4 ? QString::fromLocal8Bit(argv[3]) : QString();
    if (!output.isEmpty()) {
        QDir().mkpath(output);
        render_sheet(document, output + "/before.png");
    }
    check(DraftingWorkspace::RefreshModelViews(document, error), "Drawing refresh failed");
    const auto updated = QJsonDocument::fromJson(QByteArray::fromStdString(document.GetDraftingData()));
    const auto old_sheets = original.object().value("sheets").toArray();
    const auto new_sheets = updated.object().value("sheets").toArray();
    check(old_sheets.size() == new_sheets.size(), "Refresh changed sheet count");
    int count = 0;
    for (qsizetype s = 0; s < old_sheets.size(); ++s) {
        const auto old_items = old_sheets[s].toObject().value("primitives").toArray();
        const auto new_items = new_sheets[s].toObject().value("primitives").toArray();
        check(old_items.size() == new_items.size(), "Refresh changed annotation count");
        for (qsizetype i = 0; i < old_items.size(); ++i) {
            auto before = old_items[i].toObject(), after = new_items[i].toObject();
            if (before.value("type") != "model_view") {
                check(before == after, "Refresh changed a drawing annotation");
                continue;
            }
            const auto old_lines = before.value("visibleLines").toArray();
            const auto new_lines = after.value("visibleLines").toArray();
            check(new_lines.size() > old_lines.size(), "Wire projection did not gain outline curves");
            // The entire fixture fits in a 135-unit 3D diagonal. Exact spline
            // HLR previously emitted a 500-unit spurious isometric outline.
            const double scale = after.value("viewScale").toDouble();
            for (const auto& line : new_lines) {
                for (const auto& point : line.toArray()) {
                    const auto xy = point.toArray();
                    check(std::abs(xy[0].toDouble()) * scale < 80
                              && std::abs(xy[1].toDouble()) * scale < 80,
                          "A silhouette escaped the projected model bounds");
                }
            }
            check(after.value("hiddenLines").toArray().isEmpty(), "Visible-only view gained hidden lines");
            std::cout << before.value("projection").toString().toStdString()
                      << ": " << old_lines.size() << " -> " << new_lines.size() << " curves\n";
            for (const auto& key : {"visibleLines", "hiddenLines", "centerLines", "centerLinesVersion", "showHidden", "viewCenterX", "viewCenterY"}) {
                before.remove(key);
                after.remove(key);
            }
            check(before == after, "Refresh moved or rescaled a placed view");
            ++count;
        }
    }
    check(count == 5, "Expected two older views and three views on the new sheet");
    if (!output.isEmpty()) {
        render_sheet(document, output + "/after.png");
        QFile json(output + "/drawing.json");
        check(json.open(QIODevice::WriteOnly), "Cannot save updated drawing data");
        json.write(QByteArray::fromStdString(document.GetDraftingData()));
    }
    // Failure must leave the existing drawing intact.
    const auto valid = document.GetDraftingData();
    for (const auto& object : document.GetObjects()) object->SetVisible(false);
    check(!DraftingWorkspace::RefreshModelViews(document, error)
              && document.GetDraftingData() == valid, "Failed refresh discarded saved views");
    std::cout << "Drafting outlines passed\n";
    return 0;
}
