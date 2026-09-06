#include "ui/MainWindow.h"
#include "CPolyline.h"
#include "CPart.h"
#include "solid/Solid.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void answer(QMessageBox::StandardButton button) {
    auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    require(dialog && dialog->button(button), "Expected message box button missing");
    dialog->button(button)->click();
}
}

int TestScenePersistence(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary test directory missing");
    QCoreApplication::setOrganizationName("Dom3DScenePersistenceTests");
    QCoreApplication::setApplicationName("ScenePersistenceTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QTimer::singleShot(45000, [] { require(false, "Unexpected modal dialog / test timeout"); });

    MainWindow window;
    window.auto_save_timer_->stop();
    auto& document = window.document_;
    require(!window.HasUnsavedProjectChanges(), "A fresh scene must be clean");
    QCloseEvent clean_close;
    window.closeEvent(&clean_close);
    require(clean_close.isAccepted(), "A clean scene must close without a prompt");

    const auto add_curve = [&document](const char* name) {
        auto curve = std::make_unique<CPolyline>();
        curve->SetName(name);
        curve->AddPoint(CPoint3d(0, 0, 0));
        curve->AddPoint(CPoint3d(10, 0, 0));
        auto* result = curve.get();
        document.AddObject(std::move(curve));
        return result;
    };
    CPolyline* first = add_curve("Selection A");
    CPolyline* second = add_curve("Visibility B");
    window.scene_tree_figures_filter_->setChecked(true);
    window.scene_tree_layers_filter_->setChecked(true);
    window.scene_tree_parts_filter_->setChecked(true);
    document.SelectObjectById(first->m_id);
    window.RefreshSceneTree();
    const auto row = [&window](const QString& name) {
        for (QTreeWidgetItemIterator it(window.scene_tree_); *it; ++it) {
            if ((*it)->text(1) == name) return *it;
        }
        std::cerr << "Missing row: " << name.toStdString() << '\n';
        require(false, "Scene tree row missing");
        return static_cast<QTreeWidgetItem*>(nullptr);
    };
    const auto click_visibility = [&](const QString& name) {
        auto* item = row(name);
        // Reproduce Qt's selection change before dispatching itemClicked.
        window.scene_tree_->clearSelection();
        item->setSelected(true);
        window.scene_tree_->itemClicked(item, 0);
    };
    click_visibility("Visibility B");
    require(!second->IsVisible() && document.GetSelectedObject() == first,
            "Hiding another object must preserve scene selection");
    require(row("Selection A")->isSelected() && !row("Visibility B")->isSelected(),
            "Visibility clicks must preserve tree selection too");
    click_visibility("Visibility B");
    require(document.GetSelectedObject() == first,
            "Showing another object must preserve selection");
    document.SelectObjectById(second->m_id, SelectionAction::Add);
    click_visibility("Visibility B");
    require(document.GetSelectedObjectCount() == 1 && document.GetSelectedObject() == first,
            "Only the hidden member of a multiselection must be removed");
    click_visibility("Selection A");
    require(!document.HasSelection(), "Hiding the selected object must deselect it");

    first->SetVisible(true);
    second->SetVisible(true);
    auto* layer = document.AddLayer("Visibility layer");
    second->m_LayerID = layer->ID();
    document.SelectObjectById(first->m_id);
    window.RefreshSceneTree();
    click_visibility("Visibility layer");
    require(document.GetSelectedObject() == first, "Hiding an unrelated layer must preserve selection");
    click_visibility("Visibility layer");
    document.SelectObjectById(second->m_id, SelectionAction::Add);
    click_visibility("Visibility layer");
    require(document.GetSelectedObjectCount() == 1 && document.GetSelectedObject() == first,
            "Hiding a layer must remove only its selected objects");
    layer->Visible = true;
    second->SetGroupName("Legacy visibility group");
    window.RefreshSceneTree();
    click_visibility("Legacy visibility group");
    require(document.GetSelectedObject() == first && !second->IsVisible(),
            "Hiding a legacy group must preserve unrelated selection");

    auto part = std::make_unique<CPart>();
    part->SetName("Visibility part");
    document.AddObject(std::move(part));
    document.SelectObjectById(first->m_id);
    window.RefreshSceneTree();
    click_visibility("Parts");
    require(document.GetSelectedObject() == first, "Hiding all parts must preserve unrelated selection");

    std::cerr << "Visibility regressions passed; checking save/close...\n";
    TopoDS_Shape box = BRepPrimAPI_MakeBox(2, 3, 4).Shape();
    auto solid = std::make_unique<CSolid>(box);
    document.AddObject(std::move(solid));
    const auto content = window.dom3d_serializer_.DocumentFingerprint(document);
    require(!content.isEmpty() && content == window.dom3d_serializer_.DocumentFingerprint(document),
            "Solid scene fingerprints must be deterministic");
    document.ClearSelection();
    document.SelectObjectById(first->m_id);
    require(content == window.dom3d_serializer_.DocumentFingerprint(document),
            "Selection must not affect persistent content");
    require(window.HasUnsavedProjectChanges(), "Added geometry must be dirty");

    const auto close_with = [&window](QMessageBox::StandardButton choice) {
        QTimer::singleShot(0, [choice] { answer(choice); });
        QCloseEvent event;
        window.closeEvent(&event);
        return event.isAccepted();
    };
    require(!close_with(QMessageBox::Cancel), "Cancel must block closing");
    require(window.HasUnsavedProjectChanges(), "Cancel must retain the dirty state");
    require(close_with(QMessageBox::Discard), "Discard must allow closing");
    require(window.HasUnsavedProjectChanges(), "Discard must not mark unsaved data as saved");

    QTimer::singleShot(0, [] {
        QTimer::singleShot(0, [] {
            auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
            require(dialog, "Save on an untitled scene must ask for a filename");
            dialog->reject();
        });
        answer(QMessageBox::Save);
    });
    QCloseEvent cancel_save;
    window.closeEvent(&cancel_save);
    require(!cancel_save.isAccepted() && window.HasUnsavedProjectChanges(),
            "Cancelling Save As must keep the program open and dirty");

    window.project_path_ = temporary.filePath("missing/scene.dom3d").toStdString();
    QTimer::singleShot(0, [] {
        QTimer::singleShot(0, [] { answer(QMessageBox::Ok); });
        answer(QMessageBox::Save);
    });
    QCloseEvent failed_save;
    window.closeEvent(&failed_save);
    require(!failed_save.isAccepted() && window.HasUnsavedProjectChanges(),
            "A failed save must keep the program open and dirty");

    const QString saved_path = temporary.filePath("scene.dom3d");
    window.project_path_ = saved_path.toStdString();
    require(close_with(QMessageBox::Save), "Successful Save must allow closing");
    require(QFileInfo::exists(saved_path) && !window.HasUnsavedProjectChanges(),
            "Successful Save must create the file and mark the scene clean");
    window.undo_redo_.Reset();
    require(!window.HasUnsavedProjectChanges(),
            "Building undo snapshots/render caches must not dirty the saved scene");
    Camera camera = window.viewport_->GetCamera();
    camera.distance += 10;
    window.viewport_->SetCamera(camera);
    require(!window.HasUnsavedProjectChanges(), "Camera movement must not dirty the scene");
    first->SetVisible(false);
    require(window.HasUnsavedProjectChanges(), "Visibility-only changes must be dirty");
    first->SetVisible(true);
    require(!window.HasUnsavedProjectChanges(), "Restoring saved visibility must be clean");
    window.undo_redo_.BeginChange();
    first->AddPoint(CPoint3d(20, 0, 0));
    window.undo_redo_.CommitChange("Edit curve");
    require(window.HasUnsavedProjectChanges(), "Geometry edits must be dirty");
    require(window.undo_redo_.Undo(), "Undo failed");
    require(!window.HasUnsavedProjectChanges(), "Undo to saved content must be clean");
    require(window.undo_redo_.Redo() && window.HasUnsavedProjectChanges(),
            "Redo after the save point must be dirty");
    require(window.undo_redo_.Undo() && !window.HasUnsavedProjectChanges(),
            "Undo after Redo must restore the save point");
    document.GetMaterials().front().name += " changed";
    require(window.HasUnsavedProjectChanges(), "Material changes must be dirty");
    window.AutoSaveProject();
    require(!window.HasUnsavedProjectChanges(), "Successful autosave must mark clean");
    document.SetDraftingData("changed drafting data");
    require(window.HasUnsavedProjectChanges(), "Drafting changes must be dirty");
    window.OpenProjectFromPath(saved_path);
    require(!window.HasUnsavedProjectChanges(), "Opening a saved project must be clean");
    window.NewProject();
    require(!window.HasUnsavedProjectChanges(), "New scene must reset the saved baseline");
    std::cout << "Scene persistence tests passed.\n";
    return 0;
}
