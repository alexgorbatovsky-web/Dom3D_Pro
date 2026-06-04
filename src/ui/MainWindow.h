#pragma once

#include "OpenGLViewport.h"
#include "PropertyPanel.h"
#include "ToolRegistry.h"

#include "../ObjIO.h"
#include "../ProjectIO.h"

#include <QIcon>
#include <QMainWindow>

#include <string>
#include <vector>

class QAction;
class QDockWidget;
class QPushButton;
class QTabBar;
class QTreeWidget;
class QToolBar;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void CreateActions();
    void CreateDocks();
    void CreateToolsPanel(QDockWidget* dock);
    void RefreshSceneTree();
    void SetTool(ToolMode tool, const QString& status_text);
    void BeginTransformTool(TransformOperation operation);
    void ActivateParametricTool(const std::string& tool_id);
    void ClearActiveProperties();
    void AcceptActiveProperties();
    void CancelActiveProperties();
    void ShowPropertyPanelAtCursor(const QString& title);
    void RegisterToolAction(QAction* action, const std::string& key);
    void RegisterToolButton(QPushButton* button, const std::string& key);
    void UpdateActiveToolUi(const std::string& key);
    void UpdateToolAvailability();
    QIcon ToolIcon(const std::string& key) const;
    void NewProject();
    void OpenProject();
    void SaveProject();
    void ShowPreferences();
    void ImportObj();
    void ExportObj();
    void DeleteSelected();

    OpenGLViewport* viewport_ = nullptr;
    QTreeWidget* scene_tree_ = nullptr;
    PropertyPanel* property_panel_ = nullptr;
    QDockWidget* properties_dock_ = nullptr;
    QTabBar* tool_tabs_ = nullptr;
    QToolBar* tab_toolbar_ = nullptr;
    QToolBar* main_toolbar_ = nullptr;
    std::vector<QAction*> tool_actions_;
    std::vector<QPushButton*> tool_buttons_;
    std::string active_tool_key_ = "orbit";

    CAlfaDoc document_;
    ProjectIO project_io_;
    ObjIO obj_io_;
    ToolRegistry tool_registry_;
    ActiveParametricObject active_parametric_object_;
    std::string project_path_;
};
