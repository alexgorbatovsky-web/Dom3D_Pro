#pragma once

#include "../Material.h"

#include <QWidget>

#include <vector>

struct MaterialSphereItem {
    Material material;
    QString material_file_path;
    int source_index = -1;
    bool document_material = false;
};

class MaterialSphereBrowser : public QWidget {
    Q_OBJECT

public:
    explicit MaterialSphereBrowser(QWidget* parent = nullptr);

    void SetItems(std::vector<MaterialSphereItem> items);
    void SetSelectedMaterialId(unsigned long id);
    int SelectedIndex() const { return selected_index_; }

signals:
    void MaterialSelected(int index, const Material& material, const QString& material_file_path);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    int ColumnCount() const;
    int RowCount() const;
    int ContentHeight() const;
    QRect CellRect(int index) const;
    int IndexAt(const QPoint& point) const;
    void ClampScroll();
    void StartDrag(int index);

    std::vector<MaterialSphereItem> items_;
    int selected_index_ = -1;
    int scroll_y_ = 0;
    QPoint drag_start_pos_;
    int drag_start_index_ = -1;
};
