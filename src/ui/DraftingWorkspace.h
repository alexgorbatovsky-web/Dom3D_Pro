#pragma once

#include <QWidget>

class CAlfaDoc;
class QTabWidget;
class QToolBar;
class DraftingSheetView;

class DraftingWorkspace final : public QWidget {
    Q_OBJECT

public:
    enum class Tool { Select, Line, Rectangle, Ellipse, Text, Dimension, ModelView };

    explicit DraftingWorkspace(QWidget* parent = nullptr);

    void SetDocument(CAlfaDoc* document);
    void ReloadFromDocument();

public slots:
    void AddDrawing();

private:
    DraftingSheetView* CurrentSheet() const;
    void SetTool(Tool tool);
    void SaveToDocument();
    void RemoveCurrentDrawing();
    void DeleteSelected();
    void ShowPrintPreview();
    void PrintCurrentDrawing();
    void ExportPdf();
    void UpdateActionState();

    CAlfaDoc* document_ = nullptr;
    Tool current_tool_ = Tool::Select;
    QToolBar* toolbar_ = nullptr;
    QToolBar* tools_toolbar_ = nullptr;
    QTabWidget* sheets_ = nullptr;
};
