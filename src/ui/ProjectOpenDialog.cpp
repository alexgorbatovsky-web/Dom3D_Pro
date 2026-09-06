#include "ProjectOpenDialog.h"

#ifdef Q_OS_WIN
// Include the full Shell API before the renderer's lean Windows headers.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shlobj.h>
#endif

#include "../Dom3DProjectSerializer.h"

#include <QCursor>
#include <QActionGroup>
#include <QCache>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QGuiApplication>
#include <QFutureWatcher>
#include <QImageReader>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPromise>
#include <QQueue>
#include <QResizeEvent>
#include <QScreen>
#include <QSettings>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStandardPaths>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QTimer>
#include <QThreadPool>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QXmlStreamReader>

#include <algorithm>
#include <memory>

class ProjectThumbnailLabel final : public QLabel {
public:
    explicit ProjectThumbnailLabel(QWidget* parent) : QLabel(parent) {
        setObjectName("ProjectPreviewImage");
        setAlignment(Qt::AlignCenter);
        setWordWrap(true);
        setMargin(12);
        setMinimumSize(220, 180);
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
        setFrameShape(QFrame::StyledPanel);
        setBackgroundRole(QPalette::Base);
        setAutoFillBackground(true);
    }

    void SetImage(const QImage& image, const QString& empty_text) {
        original_ = QPixmap::fromImage(image);
        if (original_.isNull()) {
            clear();
            setText(empty_text);
        } else {
            UpdatePixmap();
        }
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QLabel::resizeEvent(event);
        if (!original_.isNull()) UpdatePixmap();
    }

private:
    void UpdatePixmap() {
        setPixmap(original_.scaled(
            (contentsRect().size() - QSize(2 * margin(), 2 * margin())).expandedTo(QSize(1, 1)),
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    QPixmap original_;
};

namespace {
constexpr int kPlacePathRole = Qt::UserRole + 1;
constexpr int kPinnedPlaceRole = Qt::UserRole + 2;
constexpr int kSeparatorPlaceRole = Qt::UserRole + 3;

struct NavigationPlace {
    QString name;
    QString path;
};

#ifdef Q_OS_WIN
QIcon WindowsNavigationIcon(const wchar_t* parsing_name, const QIcon& fallback) {
    QIcon result = fallback;
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    PIDLIST_ABSOLUTE item = nullptr;
    if (SUCCEEDED(SHParseDisplayName(parsing_name, nullptr, &item, 0, nullptr))) {
        SHFILEINFOW info{};
        if (SHGetFileInfoW(reinterpret_cast<LPCWSTR>(item), 0, &info, sizeof(info),
                           SHGFI_PIDL | SHGFI_ICON | SHGFI_SMALLICON) && info.hIcon) {
            result = QIcon(QPixmap::fromImage(QImage::fromHICON(info.hIcon)));
            DestroyIcon(info.hIcon);
        }
    }
    CoTaskMemFree(item);
    if (SUCCEEDED(initialized)) CoUninitialize();
    return result;
}

QList<NavigationPlace> WindowsQuickAccessFolders() {
    QList<NavigationPlace> result;
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    PIDLIST_ABSOLUTE quick_access = nullptr;
    IShellFolder* folder = nullptr;
    IEnumIDList* entries = nullptr;
    // This is the same shell namespace used by Explorer and Windows' native
    // open dialog, so its order and pinned folders remain owned by Windows.
    if (SUCCEEDED(SHParseDisplayName(
            L"shell:::{679f85cb-0220-4080-b29b-5540cc05aab6}", nullptr,
            &quick_access, 0, nullptr))
        && SUCCEEDED(SHBindToObject(nullptr, quick_access, nullptr,
                                   IID_IShellFolder, reinterpret_cast<void**>(&folder)))
        && folder->EnumObjects(nullptr, SHCONTF_FOLDERS, &entries) == S_OK
        && entries) {
        PITEMID_CHILD child = nullptr;
        while (entries->Next(1, &child, nullptr) == S_OK) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(SHCreateItemWithParent(quick_access, folder, child,
                                                IID_IShellItem,
                                                reinterpret_cast<void**>(&item)))) {
                PWSTR filesystem_path = nullptr;
                PWSTR display_name = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &filesystem_path))
                    && QFileInfo(QString::fromWCharArray(filesystem_path)).isDir()) {
                    item->GetDisplayName(SIGDN_NORMALDISPLAY, &display_name);
                    const QString path = QDir::cleanPath(QString::fromWCharArray(filesystem_path));
                    const QString name = display_name
                        ? QString::fromWCharArray(display_name) : QFileInfo(path).fileName();
                    result.append({name, path});
                }
                CoTaskMemFree(filesystem_path);
                CoTaskMemFree(display_name);
                item->Release();
            }
            CoTaskMemFree(child);
        }
    }
    if (entries) entries->Release();
    if (folder) folder->Release();
    CoTaskMemFree(quick_access);
    if (SUCCEEDED(initialized)) CoUninitialize();
    return result;
}
#endif

QList<NavigationPlace> StandardNavigationPlaces() {
    QList<NavigationPlace> result;
    const auto add = [&](const QString& name, QStandardPaths::StandardLocation location) {
        const QString path = QStandardPaths::writableLocation(location);
        if (!path.isEmpty() && QDir(path).exists()) result.append({name, QDir::cleanPath(path)});
    };
    add(ProjectOpenDialog::tr("Desktop"), QStandardPaths::DesktopLocation);
    add(ProjectOpenDialog::tr("Downloads"), QStandardPaths::DownloadLocation);
    add(ProjectOpenDialog::tr("Documents"), QStandardPaths::DocumentsLocation);
    add(ProjectOpenDialog::tr("Pictures"), QStandardPaths::PicturesLocation);
    add(ProjectOpenDialog::tr("Music"), QStandardPaths::MusicLocation);
    add(ProjectOpenDialog::tr("Videos"), QStandardPaths::MoviesLocation);
    return result;
}

class NavigationPlaceDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        if (index.data(kSeparatorPlaceRole).toBool()) {
            painter->save();
            painter->setPen(option.palette.color(QPalette::Midlight));
            painter->drawLine(option.rect.left() + 8, option.rect.center().y(),
                              option.rect.right() - 8, option.rect.center().y());
            painter->restore();
            return;
        }
        QStyleOptionViewItem item(option);
        item.rect.adjust(24, 0, index.data(kPinnedPlaceRole).toBool() ? -22 : -4, 0);
        QStyledItemDelegate::paint(painter, item, index);
        if (!index.data(kPinnedPlaceRole).toBool()) return;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(option.rect.right() - 12, option.rect.center().y());
        painter->rotate(45);
        painter->setPen(QPen(option.palette.color(QPalette::Mid), 1));
        painter->setBrush(option.palette.color(QPalette::Mid));
        painter->drawPolygon(QPolygonF{QPointF(-2, -4), QPointF(2, -4),
            QPointF(2, 0), QPointF(3, 2), QPointF(-3, 2), QPointF(-2, 0)});
        painter->drawLine(QPointF(0, 2), QPointF(0, 6));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override {
        if (index.data(kSeparatorPlaceRole).toBool()) return QSize(1, 14);
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), 32);
    }
};

// Decode only the small metadata thumbnail, never the scene geometry. All I/O
// runs in a worker; QPixmap/QIcon creation and model notifications stay on the UI thread.
QImage ReadBrowserThumbnail(const QString& path) {
    if (QFileInfo(path).suffix().compare("dom3d", Qt::CaseInsensitive) == 0) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        QXmlStreamReader xml(&file);
        if (!xml.readNextStartElement() || xml.name() != QLatin1String("dom3dProject")) return {};
        while (!xml.atEnd() && file.pos() < 16 * 1024 * 1024) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("metadata")) {
                while (xml.readNextStartElement()) {
                    if (xml.name() == QLatin1String("thumbnail")) {
                        const auto encoded = xml.readElementText();
                        if (xml.hasError()) return {};
                        const QImage image = QImage::fromData(QByteArray::fromBase64(encoded.toLatin1()), "PNG");
                        return image.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    }
                    xml.skipCurrentElement();
                }
                return {};
            }
            // Metadata precedes the scene in the project format.
            if (xml.isStartElement()) return {};
        }
        return {};
    }
    QImageReader reader(path);
    if (!QImageReader::supportedImageFormats().contains(QFileInfo(path).suffix().toLower().toLatin1())) return {};
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid()) reader.setScaledSize(size.scaled(256, 256, Qt::KeepAspectRatio));
    return reader.read();
}

QImage ReadFolderThumbnail(const QString& path, const QPromise<QImage>& promise) {
    // A bounded, nonrecursive scan avoids traversing entire project libraries.
    QDirIterator entries(path, QDir::Files | QDir::NoSymLinks | QDir::Readable);
    for (int count = 0; count < 256 && entries.hasNext() && !promise.isCanceled(); ++count) {
        const QImage image = ReadBrowserThumbnail(entries.next());
        if (image.isNull()) continue;
        QImage folder(256, 256, QImage::Format_ARGB32_Premultiplied);
        folder.fill(Qt::transparent);
        QPainter painter(&folder);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor("#e8ad20"), 2));
        painter.setBrush(QColor("#ffc437"));
        painter.drawRoundedRect(QRectF(10, 35, 86, 46), 10, 10);
        painter.drawRoundedRect(QRectF(10, 57, 236, 168), 10, 10);
        const QRect area(22, 70, 212, 135);
        const QImage fitted = image.scaled(area.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        painter.fillRect(area, QColor("#fff5d5"));
        painter.drawImage(QPoint(area.center().x() - fitted.width() / 2,
                                area.center().y() - fitted.height() / 2), fitted);
        painter.setBrush(QColor("#ffdb72"));
        painter.drawRoundedRect(QRectF(10, 185, 236, 40), 9, 9);
        return folder;
    }
    return {};
}

// Tiles and Content show secondary information alongside the file icon.
class FileInformationDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyleOptionViewItem item(option);
        initStyleOption(&item, index);
        const QString name = item.text;
        const QIcon icon = item.icon;
        item.text.clear();
        item.icon = {};
        const QWidget* widget = option.widget;
        widget->style()->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);
        const bool content = widget->property("projectContentView").toBool();
        const int icon_size = 48;
        const QRect icon_rect(option.rect.left() + 8,
            option.rect.center().y() - icon_size / 2, icon_size, icon_size);
        icon.paint(painter, icon_rect, Qt::AlignCenter,
                   option.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled);
        const QRect text_rect = option.rect.adjusted(icon_size + 20, 8, -8, -8);
        const int line_height = option.fontMetrics.height();
        const QString type = index.siblingAtColumn(2).data().toString();
        const QString size = index.siblingAtColumn(1).data().toString();
        painter->save();
        painter->setFont(option.font);
        painter->setPen(option.palette.color(option.state & QStyle::State_Selected
            ? QPalette::HighlightedText : QPalette::Text));
        const auto draw_line = [&](int line, const QString& text) {
            painter->drawText(QRect(text_rect.left(), text_rect.top() + line * line_height,
                text_rect.width(), line_height), Qt::AlignLeft | Qt::AlignVCenter,
                option.fontMetrics.elidedText(text, Qt::ElideRight, text_rect.width()));
        };
        draw_line(0, name);
        draw_line(1, size.isEmpty() ? type : type + "  |  " + size);
        if (content) draw_line(2, index.siblingAtColumn(3).data().toString());
        painter->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override {
        const bool content = option.widget->property("projectContentView").toBool();
        return QSize(content ? 320 : 270, std::max(72, option.fontMetrics.height() * (content ? 3 : 2) + 20));
    }
};

class FolderSearchProxy final : public QSortFilterProxyModel {
public:
    explicit FolderSearchProxy(QObject* parent) : QSortFilterProxyModel(parent) {
        connect(&worker_, &QFutureWatcher<QImage>::finished, this, [this] {
            const Request request = active_;
            const QImage image = worker_.result();
            thumbnails_.insert(request.key, new QIcon(image.isNull() ? QIcon() : QIcon(QPixmap::fromImage(image))));
            pending_.remove(request.key);
            busy_ = false;
            const QModelIndex index = mapFromSource(request.index);
            if (index.isValid()) emit dataChanged(index, index, {Qt::DecorationRole});
            StartNextThumbnail();
        });
    }
    ~FolderSearchProxy() override { worker_.cancel(); }

    void SetThumbnailDirectory(const QString& directory) {
        thumbnail_directory_ = QDir(directory).absolutePath();
        while (!requests_.isEmpty()) pending_.remove(requests_.dequeue().key);
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (role == Qt::DecorationRole && index.column() == 0) {
            const auto* files = qobject_cast<const QFileSystemModel*>(sourceModel());
            if (files && files->fileInfo(mapToSource(index)).absolutePath() == thumbnail_directory_) {
                const QModelIndex source = mapToSource(index);
                const QFileInfo file = files->fileInfo(source);
                const QString key = file.absoluteFilePath() + '\n' + QString::number(file.size())
                    + ':' + QString::number(file.lastModified().toMSecsSinceEpoch());
                if (const auto* icon = thumbnails_.object(key)) {
                    if (!icon->isNull()) return *icon;
                } else if (!pending_.contains(key)) {
                    auto* self = const_cast<FolderSearchProxy*>(this);
                    self->pending_.insert(key);
                    self->requests_.enqueue({key, file.absoluteFilePath(), file.isDir(), source});
                    self->StartNextThumbnail();
                }
            }
        }
        if (role == Qt::DisplayRole && index.column() == 2) {
            const auto* files = qobject_cast<const QFileSystemModel*>(sourceModel());
            if (files) {
                const QString suffix = files->fileInfo(mapToSource(index)).suffix().toLower();
                if (suffix == "dom3d") return ProjectOpenDialog::tr("Dom3D Project");
                if (suffix == "d3dm" || suffix == "wrk") return ProjectOpenDialog::tr("Legacy Dom3D Project");
            }
        }
        return QSortFilterProxyModel::data(index, role);
    }
private:
    struct Request {
        QString key;
        QString path;
        bool folder;
        QPersistentModelIndex index;
    };
    void StartNextThumbnail() {
        if (busy_ || requests_.isEmpty()) return;
        active_ = requests_.dequeue();
        busy_ = true;
        auto promise = std::make_shared<QPromise<QImage>>();
        promise->start();
        worker_.setFuture(promise->future());
        const QString path = active_.path;
        const bool folder = active_.folder;
        // No dialog/model pointers cross the thread boundary, so closing the
        // dialog cancels queued work without waiting for a filesystem read.
        QThreadPool::globalInstance()->start([promise, path, folder] {
            QImage image;
            if (!promise->isCanceled())
                image = folder ? ReadFolderThumbnail(path, *promise) : ReadBrowserThumbnail(path);
            promise->addResult(image);
            promise->finish();
        });
    }
    mutable QCache<QString, QIcon> thumbnails_{512};
    QSet<QString> pending_;
    QQueue<Request> requests_;
    Request active_;
    QFutureWatcher<QImage> worker_;
    bool busy_ = false;
    QString thumbnail_directory_;
protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        const auto* files = qobject_cast<const QFileSystemModel*>(sourceModel());
        // Searching filenames must not hide folders or ancestors of the root.
        if (files && files->isDir(files->index(row, 0, parent))) return true;
        return QSortFilterProxyModel::filterAcceptsRow(row, parent);
    }
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override {
        const auto* files = qobject_cast<const QFileSystemModel*>(sourceModel());
        if (files && files->isDir(left) != files->isDir(right))
            return sortOrder() == Qt::AscendingOrder ? files->isDir(left) : files->isDir(right);
        return QSortFilterProxyModel::lessThan(left, right);
    }
};
}

ProjectOpenDialog::ProjectOpenDialog(const QString& directory, QWidget* parent)
    : QFileDialog(parent, tr("Open Dom3D Project"), directory) {
    setObjectName("ProjectOpenDialog");
    // Keep the preview inside the browser; a native QFileDialog cannot embed Qt widgets.
    setOption(QFileDialog::DontUseNativeDialog, true);
    setAcceptMode(QFileDialog::AcceptOpen);
    setFileMode(QFileDialog::ExistingFile);
    setNameFilters({tr("Dom3D Project (*.dom3d)"),
                    tr("Legacy Dom3D Project (*.d3dm *.wrk)"), tr("All files (*.*)")});
    setViewMode(QFileDialog::Detail);
    setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    setSizeGripEnabled(true);

    auto* proxy = new FolderSearchProxy(this);
    proxy->SetThumbnailDirectory(directory);
    proxy->setFilterKeyColumn(0);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    setProxyModel(proxy);
    connect(this, &QFileDialog::directoryEntered, proxy, &FolderSearchProxy::SetThumbnailDirectory);

    if (auto* list = findChild<QListView*>("listView")) {
        default_list_delegate_ = list->itemDelegate();
        information_delegate_ = new FileInformationDelegate(list);
    }
    // Replace Qt's two fixed view buttons with one Explorer-style menu.
    for (const char* name : {"listModeButton", "detailModeButton"}) {
        if (auto* button = findChild<QToolButton*>(name)) button->hide();
    }
    auto* view_button = new QToolButton(this);
    view_button->setObjectName("ProjectViewButton");
    view_button->setText(tr("View"));
    view_button->setToolTip(tr("Change file view"));
    view_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    view_button->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    view_button->setPopupMode(QToolButton::InstantPopup);
    auto* view_menu = new QMenu(view_button);
    view_menu->setObjectName("ProjectViewMenu");
    view_button->setMenu(view_menu);
    view_actions_ = new QActionGroup(this);
    view_actions_->setExclusive(true);
    const QStringList views{tr("Extra large icons"), tr("Large icons"), tr("Medium icons"),
        tr("Small icons"), tr("List"), tr("Details"), tr("Tiles"), tr("Content")};
    for (int mode = 0; mode < views.size(); ++mode) {
        if (mode == 4 || mode == 6) view_menu->addSeparator();
        auto* action = view_menu->addAction(views[mode]);
        action->setCheckable(true);
        action->setData(mode);
        view_actions_->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode] { SetBrowserView(mode); });
    }

    browser_splitter_ = findChild<QSplitter*>("splitter");
    if (browser_splitter_ && browser_splitter_->count() >= 2) {
        places_ = new QListWidget(this);
        places_->setObjectName("ProjectPlacesList");
        places_->setFrameShape(QFrame::StyledPanel);
        places_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        places_->setSelectionMode(QAbstractItemView::SingleSelection);
        places_->setItemDelegate(new NavigationPlaceDelegate(places_));
        places_->setIconSize(QSize(16, 16));
        places_->setMinimumWidth(160);
        QFileIconProvider icons;
        QSet<QString> added_paths;
        const auto add_place = [&](const QString& name, const QString& path,
                                   const QIcon& icon, bool pinned) {
            if (path.isEmpty() || !QFileInfo(path).isDir()) return;
            const QString key = QDir::cleanPath(path).toLower();
            if (added_paths.contains(key)) return;
            added_paths.insert(key);
            auto* item = new QListWidgetItem(icon, name, places_);
            item->setData(kPlacePathRole, QDir::cleanPath(path));
            item->setData(kPinnedPlaceRole, pinned);
            item->setToolTip(QDir::toNativeSeparators(path));
        };
        const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        QIcon home_icon = style()->standardIcon(QStyle::SP_DirHomeIcon);
        QIcon gallery_icon = icons.icon(QFileInfo(pictures));
        QIcon one_drive_icon = icons.icon(QFileIconProvider::Folder);
#ifdef Q_OS_WIN
        home_icon = WindowsNavigationIcon(L"shell:::{679f85cb-0220-4080-b29b-5540cc05aab6}", home_icon);
        gallery_icon = WindowsNavigationIcon(L"shell:::{e88865ea-0e1c-4e20-9aa6-edcd0212c87c}", gallery_icon);
        one_drive_icon = WindowsNavigationIcon(L"shell:::{018D5C66-4533-4307-9B53-224DE2ED1FE6}", one_drive_icon);
#endif
        add_place(tr("Home"), home, home_icon, false);
        add_place(tr("Gallery"), pictures, gallery_icon, false);
        const QString one_drive = qEnvironmentVariable("OneDrive");
        if (!one_drive.isEmpty())
            add_place(QFileInfo(one_drive).fileName(), one_drive, one_drive_icon, false);
        auto* separator = new QListWidgetItem(places_);
        separator->setData(kSeparatorPlaceRole, true);
        separator->setFlags(Qt::NoItemFlags);
        // Gallery intentionally points at Pictures too; Explorer presents it
        // once as a shell destination and once among pinned filesystem folders.
        added_paths.clear();

        QList<NavigationPlace> quick_access;
#ifdef Q_OS_WIN
        quick_access = WindowsQuickAccessFolders();
#endif
        if (quick_access.isEmpty()) quick_access = StandardNavigationPlaces();
        for (const auto& place : quick_access)
            add_place(place.name, place.path, icons.icon(QFileInfo(place.path)), true);
        for (const QFileInfo& drive : QDir::drives())
            add_place(drive.absoluteFilePath(), drive.absoluteFilePath(), icons.icon(drive), false);

        QWidget* old_sidebar = browser_splitter_->replaceWidget(0, places_);
        old_sidebar->hide();
        old_sidebar->setParent(this); // QFileDialog still owns its private sidebar model.
        const auto navigate = [this](QListWidgetItem* item) {
            const QString path = item->data(kPlacePathRole).toString();
            if (path.isEmpty() || !QDir(path).exists()) return;
            setDirectory(path);
            // Programmatic setDirectory does not emit the user-navigation
            // signal that resets the search, thumbnail queue and preview.
            emit directoryEntered(this->directory().absolutePath());
        };
        connect(places_, &QListWidget::itemActivated, this, navigate);
        connect(places_, &QListWidget::itemClicked, this, navigate);
        connect(this, &QFileDialog::directoryEntered, this, &ProjectOpenDialog::UpdatePlaceSelection);

        auto* files_panel = new QWidget(this);
        files_panel->setMinimumWidth(320);
        auto* files_layout = new QVBoxLayout(files_panel);
        files_layout->setContentsMargins(0, 0, 0, 0);
        files_layout->setSpacing(8);
        auto* search_row = new QHBoxLayout;
        auto* files_title = new QLabel(tr("Files"), files_panel);
        search_row->addWidget(files_title);
        search_row->addStretch();
        search_ = new QLineEdit(files_panel);
        search_->setObjectName("ProjectFolderSearch");
        search_->setPlaceholderText(tr("Search this folder"));
        search_->setToolTip(tr("Filter filenames in the current folder"));
        search_->setClearButtonEnabled(true);
        search_->setMinimumWidth(180);
        search_->setMaximumWidth(320);
        search_row->addWidget(search_, 1);
        search_row->addWidget(view_button);
        files_layout->addLayout(search_row);
        QWidget* files_view = browser_splitter_->replaceWidget(1, files_panel);
        files_layout->addWidget(files_view, 1);
        connect(search_, &QLineEdit::textChanged, proxy, &QSortFilterProxyModel::setFilterFixedString);
    }

    auto* preview_panel = new QWidget(this);
    preview_panel->setObjectName("ProjectPreviewPanel");
    preview_panel->setMinimumWidth(250);
    auto* preview_layout = new QVBoxLayout(preview_panel);
    preview_layout->setContentsMargins(10, 0, 0, 0);
    preview_layout->setSpacing(10);
    auto* title = new QLabel(tr("Preview"), preview_panel);
    QFont heading_font = title->font();
    heading_font.setBold(true);
    title->setFont(heading_font);
    preview_layout->addWidget(title);
    preview_ = new ProjectThumbnailLabel(preview_panel);
    preview_layout->addWidget(preview_, 1);
    preview_name_ = new QLabel(preview_panel);
    preview_name_->setObjectName("ProjectPreviewName");
    preview_name_->setFont(heading_font);
    preview_name_->setTextFormat(Qt::PlainText);
    preview_name_->setWordWrap(true);
    preview_layout->addWidget(preview_name_);
    preview_details_ = new QLabel(preview_panel);
    preview_details_->setWordWrap(true);
    preview_layout->addWidget(preview_details_);
    if (browser_splitter_) {
        browser_splitter_->addWidget(preview_panel);
        browser_splitter_->setChildrenCollapsible(false);
        browser_splitter_->setHandleWidth(7);
        browser_splitter_->setStretchFactor(0, 0);
        browser_splitter_->setStretchFactor(1, 3);
        browser_splitter_->setStretchFactor(2, 1);
        browser_splitter_->setSizes({190, 820, 340});
    } else {
        layout()->addWidget(preview_panel);
    }

    if (auto* view = findChild<QTreeView*>("treeView")) {
        auto* header = view->header();
        header->setStretchLastSection(false);
        header->setSectionResizeMode(0, QHeaderView::Stretch);
        header->resizeSection(1, 95);
        header->resizeSection(2, 125);
        header->resizeSection(3, 170);
        header->moveSection(header->visualIndex(3), 1);
        header->moveSection(header->visualIndex(2), 2);
    }

    QSettings settings;
    settings.beginGroup("files/openProjectDialog");
    if (settings.contains("browserState")) restoreState(settings.value("browserState").toByteArray());
    setDirectory(directory);
    QScreen* target_screen = QGuiApplication::screenAt(QCursor::pos());
    if (!target_screen) target_screen = screen();
    const QSize available = target_screen->availableGeometry().size();
    setMinimumSize(std::min(980, available.width() - 40), std::min(600, available.height() - 40));
    resize(std::min(1500, available.width() * 92 / 100), std::min(900, available.height() * 90 / 100));
    if (settings.contains("geometry")) restoreGeometry(settings.value("geometry").toByteArray());
    if (browser_splitter_ && settings.contains("splitterState"))
        browser_splitter_->restoreState(settings.value("splitterState").toByteArray());
    SetBrowserView(settings.value("viewMode", 5).toInt());

    preview_timer_ = new QTimer(this);
    preview_timer_->setSingleShot(true);
    preview_timer_->setInterval(120);
    connect(preview_timer_, &QTimer::timeout, this, &ProjectOpenDialog::LoadPreview);
    connect(this, &QFileDialog::currentChanged, this, &ProjectOpenDialog::QueuePreview);
    connect(this, &QFileDialog::directoryEntered, this, [this] {
        if (search_) search_->clear();
        QueuePreview({});
    });
    QueuePreview({});
    UpdatePlaceSelection(directory);
}

void ProjectOpenDialog::UpdatePlaceSelection(const QString& path) {
    if (!places_) return;
    auto* current = places_->currentItem();
    if (current && QFileInfo(current->data(kPlacePathRole).toString()) == QFileInfo(path)) {
        current->setSelected(true);
        return;
    }
    places_->clearSelection();
    for (int row = 0; row < places_->count(); ++row) {
        auto* item = places_->item(row);
        if (QFileInfo(item->data(kPlacePathRole).toString()) == QFileInfo(path)) {
            places_->setCurrentItem(item);
            break;
        }
    }
}

void ProjectOpenDialog::SetBrowserView(int mode) {
    browser_view_ = mode >= 0 && mode <= 7 ? mode : 5;
    for (auto* action : view_actions_->actions())
        action->setChecked(action->data().toInt() == browser_view_);
    setViewMode(browser_view_ == 5 ? QFileDialog::Detail : QFileDialog::List);
    auto* list = findChild<QListView*>("listView");
    if (!list) return;
    const bool information = browser_view_ >= 6;
    list->setItemDelegate(information ? information_delegate_ : default_list_delegate_);
    list->setProperty("projectContentView", browser_view_ == 7);
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setUniformItemSizes(true);
    list->setSpacing(4);
    list->setGridSize({});
    list->setWordWrap(false);
    list->setTextElideMode(Qt::ElideRight);
    list->setViewMode(QListView::ListMode);
    list->setFlow(QListView::TopToBottom);
    list->setWrapping(false);
    if (browser_view_ <= 2) {
        const int sizes[]{160, 96, 48};
        const int size = sizes[browser_view_];
        list->setViewMode(QListView::IconMode);
        list->setMovement(QListView::Static);
        list->setIconSize(QSize(size, size));
        list->setFlow(QListView::LeftToRight);
        list->setWrapping(true);
        list->setWordWrap(true);
        list->setGridSize(QSize(std::max(112, size + 28), size + 48));
    } else if (browser_view_ == 3 || browser_view_ == 4) {
        list->setIconSize(QSize(16, 16));
        list->setFlow(browser_view_ == 3 ? QListView::LeftToRight : QListView::TopToBottom);
        list->setWrapping(true);
        list->setGridSize(QSize(210, std::max(26, list->fontMetrics().height() + 8)));
    } else if (information) {
        list->setIconSize(QSize(48, 48));
        if (browser_view_ == 6) {
            list->setFlow(QListView::LeftToRight);
            list->setWrapping(true);
            list->setGridSize(QSize(270, std::max(76, list->fontMetrics().height() * 2 + 24)));
        }
    }
    list->doItemsLayout();
    if (list->currentIndex().isValid()) list->scrollTo(list->currentIndex());
}

void ProjectOpenDialog::QueuePreview(const QString& path) {
    preview_timer_->stop();
    preview_path_ = path;
    preview_name_->clear();
    preview_name_->setToolTip({});
    preview_details_->clear();
    const QFileInfo file(path);
    if (path.isEmpty() || !file.isFile()) {
        preview_->SetImage({}, tr("Select a project to see its thumbnail"));
        return;
    }
    preview_name_->setText(file.fileName());
    preview_name_->setToolTip(file.absoluteFilePath());
    preview_details_->setText(locale().formattedDataSize(file.size()) + "\n"
        + file.lastModified().toString("dd MMM yyyy, HH:mm"));
    const bool supported = file.suffix().compare("dom3d", Qt::CaseInsensitive) == 0;
    preview_->SetImage({}, supported ? tr("Loading preview...") : tr("No thumbnail"));
    if (supported) preview_timer_->start();
}

void ProjectOpenDialog::LoadPreview() {
    QImage image;
    QString error;
    Dom3DProjectSerializer serializer;
    if (!serializer.LoadThumbnail(preview_path_, image, error)) image = {};
    preview_->SetImage(image, tr("No thumbnail"));
}

void ProjectOpenDialog::done(int result) {
    QSettings settings;
    settings.beginGroup("files/openProjectDialog");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("browserState", saveState());
    settings.setValue("viewMode", browser_view_);
    if (browser_splitter_) settings.setValue("splitterState", browser_splitter_->saveState());
    QFileDialog::done(result);
}
