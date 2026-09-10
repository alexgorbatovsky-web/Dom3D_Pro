#pragma once

#include <QWidget>
#include <QPolygonF>
#include <QByteArray>

class CAlfaDoc;
class QTabWidget;
class QToolBar;
class DraftingSheetView;

class DraftingWorkspace final : public QWidget {
    Q_OBJECT

public:
    enum class Tool { Select, Line, Rectangle, Ellipse, Text, Dimension, ModelView, HorizontalDimension, VerticalDimension, ParallelDimension, PerpendicularDimension, RadiusDimension, DiameterDimension, AngularDimension };

    explicit DraftingWorkspace(QWidget* parent = nullptr);

    void SetDocument(CAlfaDoc* document);
    void ReloadFromDocument();

    static bool BuildModelView(const CAlfaDoc* document, const QString& projection,
                               double scale, bool includeHidden,
                               QVector<QPolygonF>& visible, QVector<QPolygonF>& hidden,
                               QString& error, QVector<QPolygonF>* centerlines = nullptr);
    // Rebuild cached model projections, preserving sheet layout and annotations.
    static bool RefreshModelViews(CAlfaDoc& document, QString& error);

public slots:
    void AddDrawing();

protected:
    void showEvent(QShowEvent* event) override;

private:
    QByteArray ModelSignature() const;
    QByteArray model_signature_;
    DraftingSheetView* CurrentSheet() const;
    void SetTool(Tool tool);
    void SaveToDocument();
    void RemoveCurrentDrawing();
    void DeleteSelected();
    void ShowPrintPreview();
    void PrintCurrentDrawing();
    void ExportPdf();
    void RefreshViews();
    void UpdateActionState();

    CAlfaDoc* document_ = nullptr;
    Tool current_tool_ = Tool::Select;
    QToolBar* toolbar_ = nullptr;
    QToolBar* tools_toolbar_ = nullptr;
    QTabWidget* sheets_ = nullptr;
};
