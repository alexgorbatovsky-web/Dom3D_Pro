#include "CAlfaDoc.h"
#include "CBSpline.h"
#include "UndoRedo.h"
#include "Dom3DProjectSerializer.h"
#include <QApplication>
#include <QTemporaryDir>
#include <cstdlib>
#include <cmath>
#include <iostream>

namespace {
void check(bool ok, const char* message) {
    if (!ok) { std::cerr << message << '\n'; std::exit(EXIT_FAILURE); }
}
bool same(const CPoint3d& a, const CPoint3d& b) {
    return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z) < 1.e-8;
}
}
int TestCurveEndpointLinks(int argc, char** argv) {
    QApplication app(argc, argv);
    CAlfaDoc doc;
    doc.GetObjects().clear();
    const auto add = [&](CPoint3d a, CPoint3d b) {
        auto curve = std::make_unique<CBSpline>();
        curve->AddPoint(a); curve->AddPoint(b);
        auto* ptr = curve.get(); doc.AddObject(std::move(curve), false);
        return ptr->m_id;
    };
    const auto a = add({-20,0,0}, {0,0,0});
    const auto b = add({0,0,0}, {20,10,0});
    const auto c = add({0,0,0}, {0,20,0});
    const auto point = [&](unsigned long id, bool end) {
        auto* curve = dynamic_cast<CBSpline*>(doc.FindObjectById(id));
        check(curve != nullptr, "Missing curve");
        return end ? curve->GetPoints().back() : curve->GetPoints().front();
    };
    doc.SelectObjectById(a);
    doc.SelectObjectById(b, SelectionAction::Add);
    doc.SelectObjectById(c, SelectionAction::Add);
    check(doc.FindTouchingCurveEnds(0.02).size() == 3, "Automatic endpoint matching failed");
    CUndoRedo undo(doc);
    undo.BeginChange();
    check(doc.LinkTouchingCurveEnds(0.02) == 3, "Link creation failed");
    undo.CommitChange("Link ends");
    check(doc.GetObjects().size() == 3, "Linking must not create a bridge");
    check(doc.LinkTouchingCurveEnds(0.02) == 0, "Duplicate endpoint links");
    check(undo.Undo() && doc.GetCurveEndpointLinks().empty(), "Undo linking failed");
    check(undo.Redo() && doc.GetCurveEndpointLinks().size() == 3, "Redo linking failed");
    doc.SelectCurvePoint(doc.FindObjectIndexById(a), 1);
    undo.BeginChange();
    check(doc.MoveSelectedPoint(CPoint3d(4,5,6)), "Move linked endpoint failed");
    undo.CommitChange("Move linked ends");
    check(same(point(a,true),point(b,false)) && same(point(b,false),point(c,false)),
          "Connected group did not follow endpoint move");
    check(undo.Undo() && same(point(b,false), {0,0,0}), "Undo linked move failed");
    check(undo.Redo() && same(point(c,false), {4,5,6}), "Redo linked move failed");
    doc.SelectCurvePoint(doc.FindObjectIndexById(b), 0);
    check(doc.MoveSelectedPoint(CPoint3d(8,9,10)), "Reverse link move failed");
    check(same(point(a,true), {8,9,10}) && same(point(c,false), {8,9,10}),
          "Endpoint link is not bidirectional");
    doc.SelectCurvePoint(doc.FindObjectIndexById(b), 1);
    doc.MoveSelectedPoint(CPoint3d(40,30,20));
    check(same(point(a,true), {8,9,10}), "Unlinked endpoint moved linked group");
    doc.SelectCurvePointsInScreenRect({7,8,9,10}, [](Vec3 p, DomPoint& screen) {
        screen = {static_cast<int>(p.x), static_cast<int>(p.y)}; return true;
    });
    doc.MoveSelectedCurvePoints({1,2,3});
    check(same(point(a,true), {9,11,13}) && same(point(b,false), {9,11,13})
              && same(point(c,false), {9,11,13}), "Selected linked endpoints moved more than once");

    // Gizmo/precise transforms use the Preview/Commit path, not the direct
    // object transform methods. Peers must follow before mouse release.
    doc.SelectObjectById(a);
    undo.Reset();
    for (int operation = 0; operation < 4; ++operation) {
        const CPoint3d before = point(a,true);
        const CPoint3d peer_free_end = point(b,true);
        undo.BeginChange();
        bool preview = false, commit = false;
        if (operation == 0) preview = doc.PreviewMoveSelectedObjects({3,4,5});
        if (operation == 1) preview = doc.PreviewRotateSelectedObjects({}, {0,0,1}, 0.5f);
        if (operation == 2) preview = doc.PreviewScaleSelectedObjects({}, {1,0,0}, 1.5f);
        if (operation == 3) preview = doc.PreviewUniformScaleSelectedObjects({}, 1.25f);
        check(preview && !same(point(a,true),before), "Spline transform preview failed");
        const CPoint3d after = point(a,true);
        check(same(after,point(b,false)) && same(after,point(c,false)),
              "Linked endpoints did not follow transform preview");
        check(same(peer_free_end, point(b,true)), "Transform moved the entire unselected peer");
        if (operation == 0) commit = doc.CommitMoveSelectedSolids({3,4,5});
        if (operation == 1) commit = doc.CommitRotateSelectedSolids({}, {0,0,1}, 0.5f);
        if (operation == 2) commit = doc.CommitScaleSelectedSolids({}, {1,0,0}, 1.5f);
        if (operation == 3) commit = doc.CommitUniformScaleSelectedSolids({}, 1.25f);
        check(commit && same(after,point(a,true)), "Spline commit failed or transformed it twice");
        undo.CommitChange("Transform linked spline");
        check(undo.Undo() && same(point(a,true),before) && same(point(b,false),before),
              "Undo spline transform did not restore linked endpoints");
        check(undo.Redo() && same(point(c,false),after), "Redo linked transform failed");
        check(undo.Undo(), "Restore transform baseline failed");
    }
    QTemporaryDir temp;
    Dom3DProjectSerializer serializer;
    QString error, room; ProjectViewState view;
    check(serializer.Save(temp.filePath("links.dom3d"), doc, "", view, {}, error), "Save links failed");
    CAlfaDoc loaded;
    check(serializer.Load(temp.filePath("links.dom3d"), loaded, room, view, error), "Load links failed");
    check(loaded.GetCurveEndpointLinks().size() == 3, "Links lost on load");
    loaded.SelectCurvePoint(loaded.FindObjectIndexById(c), 0);
    loaded.MoveSelectedPoint(CPoint3d(12,13,14));
    const auto* loaded_a = dynamic_cast<CBSpline*>(loaded.FindObjectById(a));
    check(loaded_a && same(loaded_a->GetPoints().back(), {12,13,14}), "Loaded links do not propagate");
    loaded.SelectObjectById(a); loaded.SelectObjectById(b,SelectionAction::Add);
    loaded.SelectObjectById(c,SelectionAction::Add);
    check(loaded.UnlinkSelectedCurveEnds() == 3, "Unlink failed");
    loaded.SelectCurvePoint(loaded.FindObjectIndexById(c),0);
    loaded.MoveSelectedPoint(CPoint3d(30,40,50));
    check(same(loaded_a->GetPoints().back(), {12,13,14}), "Unlinked curves still follow");
    std::cout << "Curve endpoint link tests passed\n";
    return EXIT_SUCCESS;
}
