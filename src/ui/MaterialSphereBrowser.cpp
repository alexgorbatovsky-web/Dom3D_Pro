#include "MaterialSphereBrowser.h"

#include "MaterialDrag.h"

#include <QApplication>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

namespace {
constexpr int kCellSize = 86;
constexpr int kBaseSphereSize = 74;
constexpr int kSelectedSphereSize = 85;
constexpr int kBorder = 1;
const QColor kBackground(5, 8, 8);
const QColor kGrid(0, 150, 150);
}

MaterialSphereBrowser::MaterialSphereBrowser(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(kCellSize * 3 + 2, kCellSize * 3 + 2);
    setMouseTracking(true);
    setFocusPolicy(Qt::WheelFocus);
}

void MaterialSphereBrowser::SetItems(std::vector<MaterialSphereItem> items) {
    items_ = std::move(items);
    if (selected_index_ >= static_cast<int>(items_.size())) {
        selected_index_ = items_.empty() ? -1 : 0;
    }
    ClampScroll();
    update();
}

void MaterialSphereBrowser::SetSelectedMaterialId(unsigned long id) {
    selected_index_ = -1;
    if (id != 0) {
        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            if (items_[i].material.id == id) {
                selected_index_ = i;
                break;
            }
        }
    }
    update();
}

void MaterialSphereBrowser::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), kBackground);

    const int cols = ColumnCount();
    const int rows = std::max(RowCount(), height() / kCellSize + 1);
    painter.setPen(QPen(kGrid, 1));
    for (int col = 0; col <= cols; ++col) {
        const int x = col * kCellSize;
        painter.drawLine(x, 0, x, height());
    }
    for (int row = 0; row <= rows; ++row) {
        const int y = row * kCellSize - scroll_y_ % kCellSize;
        painter.drawLine(0, y, width(), y);
    }

    for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
        const QRect cell = CellRect(i);
        if (!cell.intersects(rect().adjusted(0, -kSelectedSphereSize, 0, kSelectedSphereSize))) {
            continue;
        }

        const bool selected = i == selected_index_;
        const int sphere_size = selected ? kSelectedSphereSize : kBaseSphereSize;
        const QPixmap sphere = MaterialDrag::SpherePixmap(items_[i].material, sphere_size, selected, items_[i].material_file_path);
        const QPoint center = cell.center();
        painter.drawPixmap(center.x() - sphere.width() / 2, center.y() - sphere.height() / 2, sphere);
    }

    painter.setPen(QPen(kGrid, kBorder));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void MaterialSphereBrowser::wheelEvent(QWheelEvent* event) {
    scroll_y_ -= event->angleDelta().y() / 2;
    ClampScroll();
    update();
    event->accept();
}

void MaterialSphereBrowser::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    drag_start_pos_ = event->pos();
    drag_start_index_ = IndexAt(event->pos());
    if (drag_start_index_ >= 0) {
        selected_index_ = drag_start_index_;
        const auto& item = items_[static_cast<size_t>(selected_index_)];
        emit MaterialSelected(selected_index_, item.material, item.material_file_path);
        update();
    }
}

void MaterialSphereBrowser::mouseMoveEvent(QMouseEvent* event) {
    if (!(event->buttons() & Qt::LeftButton) || drag_start_index_ < 0) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    if ((event->pos() - drag_start_pos_).manhattanLength() < QApplication::startDragDistance()) {
        return;
    }

    StartDrag(drag_start_index_);
    drag_start_index_ = -1;
}

void MaterialSphereBrowser::resizeEvent(QResizeEvent*) {
    ClampScroll();
}

int MaterialSphereBrowser::ColumnCount() const {
    return std::max(1, width() / kCellSize);
}

int MaterialSphereBrowser::RowCount() const {
    const int cols = ColumnCount();
    return (static_cast<int>(items_.size()) + cols - 1) / cols;
}

int MaterialSphereBrowser::ContentHeight() const {
    return RowCount() * kCellSize;
}

QRect MaterialSphereBrowser::CellRect(int index) const {
    const int cols = ColumnCount();
    const int row = index / cols;
    const int col = index % cols;
    return QRect(col * kCellSize, row * kCellSize - scroll_y_, kCellSize, kCellSize);
}

int MaterialSphereBrowser::IndexAt(const QPoint& point) const {
    const int cols = ColumnCount();
    const int col = point.x() / kCellSize;
    const int row = (point.y() + scroll_y_) / kCellSize;
    if (col < 0 || col >= cols || row < 0) {
        return -1;
    }
    const int index = row * cols + col;
    return index >= 0 && index < static_cast<int>(items_.size()) ? index : -1;
}

void MaterialSphereBrowser::ClampScroll() {
    const int max_scroll = std::max(0, ContentHeight() - height());
    scroll_y_ = std::clamp(scroll_y_, 0, max_scroll);
}

void MaterialSphereBrowser::StartDrag(int index) {
    if (index < 0 || index >= static_cast<int>(items_.size())) {
        return;
    }

    const auto& item = items_[static_cast<size_t>(index)];
    auto* mime_data = new QMimeData;
    mime_data->setData(MaterialDrag::MimeType(), MaterialDrag::Encode(item.material));

    auto* drag = new QDrag(this);
    drag->setMimeData(mime_data);
    const QPixmap sphere = MaterialDrag::SpherePixmap(item.material, 58, true, item.material_file_path);
    drag->setPixmap(sphere);
    drag->setHotSpot(QPoint(sphere.width() / 2, sphere.height() / 2));
    drag->exec(Qt::CopyAction);
}
