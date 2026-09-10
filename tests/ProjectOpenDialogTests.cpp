#include "ui/ProjectOpenDialog.h"
#include "Dom3DProjectSerializer.h"

#include <QApplication>
#include <QAction>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileSystemModel>
#include <QFontDatabase>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QAbstractItemDelegate>
#include <QStyleOptionViewItem>
#include <QSettings>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QTemporaryDir>
#include <QThread>
#include <QTreeView>
#include <QToolButton>

#include <cstdlib>
#include <functional>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << std::endl;
        std::exit(EXIT_FAILURE);
    }
}
void wait_until(const std::function<bool()>& ready, const char* message) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 3000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    require(ready(), message);
}
}

int TestProjectOpenDialog(int argc, char** argv) {
    QApplication app(argc, argv);
    const int font_id = QFontDatabase::addApplicationFont(
        QDir(qEnvironmentVariable("WINDIR")).filePath("Fonts/segoeui.ttf"));
    if (font_id >= 0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(font_id).first(), 9));
    QTemporaryDir temporary;
    require(temporary.isValid(), "Temporary directory missing");
    QCoreApplication::setOrganizationName("Dom3DProjectOpenTests");
    QCoreApplication::setApplicationName("ProjectOpenDialogTests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    const QString folder = temporary.filePath("Projects");
    require(QDir().mkpath(folder + "/Kitchen"), "Test folder creation failed");
    CAlfaDoc document;
    Dom3DProjectSerializer serializer;
    QString error;
    QImage thumbnail(750, 450, QImage::Format_RGB32);
    thumbnail.fill(QColor(55, 110, 165));
    // Use a real scene thumbnail for the optional UI screenshot.
    const QDir source(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath());
    QImage sample;
    if (serializer.LoadThumbnail(source.filePath("data/mesh-regression/Box_Min_Box_Filled.dom3d"), sample, error)
        && !sample.isNull()) thumbnail = sample;
    const QString project = folder + "/Scene.dom3d";
    require(serializer.Save(project, document, "Solid", {}, thumbnail, error), "Saving preview fixture failed");
    require(serializer.Save(folder + "/Empty.dom3d", document, "Solid", {}, {}, error), "Saving no-preview fixture failed");
    QImage folder_image(160, 100, QImage::Format_RGB32);
    folder_image.fill(QColor(70, 120, 170));
    require(serializer.Save(folder + "/Kitchen/Table.dom3d", document, "Solid", {}, folder_image, error),
            "Saving folder preview fixture failed");
    require(QDir().mkpath(folder + "/Textures") && folder_image.save(folder + "/Textures/Wood.png"),
            "Saving image folder fixture failed");
    require(QDir().mkpath(folder + "/NoPreview"), "Creating empty folder failed");
    require(folder_image.save(folder + "/Picture.png"), "Saving image fixture failed");
    QFile invalid(folder + "/Broken.dom3d");
    require(invalid.open(QIODevice::WriteOnly), "Creating invalid fixture failed");
    invalid.write("not a project");
    invalid.close();

    QSize saved_size;
    QList<int> saved_panes;
    {
        ProjectOpenDialog dialog(folder);
        dialog.show();
        auto* splitter = dialog.findChild<QSplitter*>("splitter");
        auto* search = dialog.findChild<QLineEdit*>("ProjectFolderSearch");
        auto* preview = dialog.findChild<QLabel*>("ProjectPreviewImage");
        auto* view = dialog.findChild<QTreeView*>("treeView");
        auto* places = dialog.findChild<QListWidget*>("ProjectPlacesList");
        auto* proxy = qobject_cast<QSortFilterProxyModel*>(dialog.proxyModel());
        require(splitter && splitter->count() == 3 && search && preview && view && proxy && places,
                "Browser, search and integrated preview must exist");
        require(places->count() >= 6 && places->item(0)->text() == "Home"
                    && places->item(1)->text() == "Gallery",
                "Explorer navigation pane must contain Home, Gallery and filesystem places");
        require(dialog.windowFlags().testFlag(Qt::WindowMaximizeButtonHint), "Dialog must support maximizing");
        require(dialog.viewMode() == QFileDialog::Detail, "Default file view must show details");
        require(dialog.width() >= 740 && dialog.height() >= 550, "Open dialog must not start tiny");
        auto* files = qobject_cast<QFileSystemModel*>(proxy->sourceModel());
        require(files, "Filesystem model missing");
        wait_until([&] { return proxy->mapFromSource(files->index(project)).isValid(); }, "File list did not load");
        view->setCurrentIndex(proxy->mapFromSource(files->index(project)));
        wait_until([&] { return !preview->pixmap().isNull(); }, "Selecting a project must load its preview");
        const double ratio = double(thumbnail.width()) / thumbnail.height();
        require(std::abs(double(preview->pixmap().width()) / preview->pixmap().height() - ratio) < 0.03,
                "Preview must preserve image proportions");

        const auto icon_image = [&](const QString& path) {
            return qvariant_cast<QIcon>(proxy->mapFromSource(files->index(path)).data(Qt::DecorationRole))
                .pixmap(256, 256).toImage();
        };
        const QImage expected = thumbnail.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        wait_until([&] {
            const QImage icon = icon_image(project);
            return icon.size() == expected.size()
                // QPixmap can convert a 16-bit PNG thumbnail to 8-bit pixels.
                && icon.pixelColor(icon.width() / 2, icon.height() / 2).rgba()
                    == expected.pixelColor(expected.width() / 2, expected.height() / 2).rgba();
        }, "Project icons must show the embedded scene thumbnail, not the application icon");
        for (const QString& subfolder : {QString("Kitchen"), QString("Textures")}) {
            wait_until([&] {
                const QImage icon = icon_image(folder + '/' + subfolder);
                return icon.size() == QSize(256, 256) && icon.pixelColor(128, 137) == QColor(70, 120, 170);
            }, "Folder icons must preview contained projects and images");
        }
        for (const QString& name : {QString("Empty.dom3d"), QString("Broken.dom3d"), QString("NoPreview")})
            require(!icon_image(folder + '/' + name).isNull(), "Missing thumbnails must retain a fallback icon");
        dialog.selectNameFilter(dialog.nameFilters().last());
        wait_until([&] {
            const QImage icon = icon_image(folder + "/Picture.png");
            return !icon.isNull() && icon.pixelColor(icon.width() / 2, icon.height() / 2) == QColor(70, 120, 170);
        }, "Image files must show their picture in All files mode");
        dialog.selectNameFilter(dialog.nameFilters().first());
        view->setCurrentIndex(proxy->mapFromSource(files->index(project)));

        search->setText("scene");
        require(proxy->mapFromSource(files->index(project)).isValid(), "Search must find matching files");
        require(!proxy->mapFromSource(files->index(folder + "/Empty.dom3d")).isValid(), "Search must hide nonmatching files");
        require(proxy->mapFromSource(files->index(folder + "/Kitchen")).isValid(), "Search must leave folders navigable");
        search->clear();
        dialog.currentChanged(folder + "/Empty.dom3d");
        wait_until([&] { return preview->text() == "No thumbnail"; }, "Missing preview must clear the old image");
        dialog.currentChanged(folder + "/Broken.dom3d");
        wait_until([&] { return preview->text() == "No thumbnail"; }, "Invalid project must show a safe placeholder");
        dialog.currentChanged(project);
        wait_until([&] { return !preview->pixmap().isNull(); }, "Preview did not reload");
        search->setText("scene");
        dialog.directoryEntered(folder + "/Kitchen");
        require(search->text().isEmpty() && preview->pixmap().isNull(), "Changing folders must clear search and preview");
        dialog.directoryEntered(folder);

        auto* view_button = dialog.findChild<QToolButton*>("ProjectViewButton");
        auto* list = dialog.findChild<QListView*>("listView");
        require(view_button && view_button->menu() && list,
                "Explorer-style View button and list view must exist");
        QList<QAction*> view_actions;
        for (auto* action : view_button->menu()->actions())
            if (!action->isSeparator()) view_actions.append(action);
        require(view_actions.size() == 8, "View menu must contain all eight modes");
        dialog.currentChanged(project);
        wait_until([&] { return !preview->pixmap().isNull(); }, "Preview missing before switching views");
        const QStringList selection = dialog.selectedFiles();
        for (int mode = 0; mode < view_actions.size(); ++mode) {
            view_actions[mode]->trigger();
            if (mode <= 2) {
                // Square folders and landscape previews must reserve a caption.
                for (const QString& name : {QString("Kitchen"), QString("NoPreview"), QString("Scene.dom3d")}) {
                    const QModelIndex index=proxy->mapFromSource(files->index(folder+'/'+name));
                    require(index.isValid(),"Caption fixture index missing");
                    QStyleOptionViewItem option;
                    option.initFrom(list);
                    option.state=QStyle::State_Enabled;
                    option.widget=list;
                    option.font=list->font();
                    option.fontMetrics=QFontMetrics(option.font);
                    option.decorationSize=list->iconSize();
                    option.palette.setColor(QPalette::Text,Qt::black);
                    option.palette.setColor(QPalette::Base,Qt::white);
                    option.rect=QRect(QPoint(0,0),list->itemDelegate()->sizeHint(option,index));
                    require(option.rect.height()<=list->gridSize().height(),"Thumbnail caption exceeds the grid cell");
                    QImage painted(option.rect.size(),QImage::Format_RGB32);painted.fill(Qt::white);
                    QPainter painter(&painted);list->itemDelegate()->paint(&painter,option,index);painter.end();
                    bool has_text=false;
                    for(int y=list->iconSize().height()+8;y<painted.height()-3;++y)
                        for(int x=5;x<painted.width()-5;++x)
                            if(qGray(painted.pixel(x,y))<100)has_text=true;
                    require(has_text,"Folder/file name was clipped below its thumbnail");
                }
            }
            QCoreApplication::processEvents();
            require(view_actions[mode]->isChecked(), "Chosen view mode must be checked");
            require(dialog.viewMode() == (mode == 5 ? QFileDialog::Detail : QFileDialog::List),
                    "View action must switch the visible file browser");
            if (mode <= 2) {
                const int sizes[]{160, 96, 48};
                require(list->viewMode() == QListView::IconMode && list->iconSize().width() == sizes[mode],
                        "Icon view must apply the selected icon size");
            } else if (mode == 3 || mode == 4) {
                require(list->viewMode() == QListView::ListMode && list->iconSize().width() == 16
                    && list->flow() == (mode == 3 ? QListView::LeftToRight : QListView::TopToBottom),
                    "Small icons and List must use distinct layouts");
            } else if (mode >= 6) {
                require(list->property("projectContentView").toBool() == (mode == 7)
                            && list->isWrapping() == (mode == 6),
                        "Tiles and Content must use distinct information layouts");
            }
            require(dialog.selectedFiles() == selection, "Switching views must preserve the selected file");
            require(!preview->pixmap().isNull(), "Switching views must preserve the right preview");
        }
        search->setText("scene");
        require(!proxy->mapFromSource(files->index(folder + "/Empty.dom3d")).isValid(),
                "Search must keep working in Content view");
        search->clear();

        search->setText("scene");
        places->setCurrentRow(0);
        places->itemClicked(places->item(0));
        require(dialog.directory().absolutePath() == QDir::homePath(),
                "Clicking Home must navigate to the home directory");
        require(search->text().isEmpty() && preview->pixmap().isNull(),
                "Place navigation must reset search and preview");
        dialog.setDirectory(folder);
        dialog.directoryEntered(folder);
        wait_until([&] { return proxy->mapFromSource(files->index(project)).isValid(); },
                   "File list must reload after navigation");

        const QString screenshot = qEnvironmentVariable("DOM3D_DIALOG_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            view_actions[1]->trigger();
            dialog.resize(1420, 820);
            splitter->setSizes({180, 850, 360});
            view->setCurrentIndex(proxy->mapFromSource(files->index(project)));
            dialog.currentChanged(project);
            wait_until([&] { return !preview->pixmap().isNull(); }, "Screenshot preview missing");
            QCoreApplication::processEvents();
            require(dialog.grab().save(screenshot), "Could not save dialog screenshot");
            view_button->menu()->popup(view_button->mapToGlobal(QPoint(0, view_button->height())));
            QCoreApplication::processEvents();
            const QFileInfo output(screenshot);
            require(view_button->menu()->grab().save(output.absolutePath() + "/"
                    + output.completeBaseName() + "-menu.png"), "Could not save view menu screenshot");
            view_button->menu()->hide();
        }
        view_actions[7]->trigger();
        dialog.resize(760, 650);
        splitter->setSizes({160, 335, 250});
        QCoreApplication::processEvents();
        saved_size = dialog.size();
        saved_panes = splitter->sizes();
        dialog.reject();
        require(dialog.result() == QDialog::Rejected, "Cancel must not select a file");
    }
    {
        ProjectOpenDialog dialog(folder);
        dialog.show();
        QCoreApplication::processEvents();
        require(dialog.size() == saved_size, "Dialog size must survive reopening after Cancel");
        require(dialog.findChild<QSplitter*>("splitter")->sizes() == saved_panes,
                "Panel widths must survive reopening");
        auto* list = dialog.findChild<QListView*>("listView");
        require(dialog.viewMode() == QFileDialog::List && list->property("projectContentView").toBool()
                    && !list->isWrapping(), "Content view must survive reopening after Cancel");
        auto* menu = dialog.findChild<QToolButton*>("ProjectViewButton")->menu();
        require(menu->actions().last()->isChecked(), "Restored view must be checked in the menu");
        auto* proxy = qobject_cast<QSortFilterProxyModel*>(dialog.proxyModel());
        auto* files = qobject_cast<QFileSystemModel*>(proxy->sourceModel());
        wait_until([&] { return proxy->mapFromSource(files->index(project)).isValid(); },
                   "Reopened file list did not load");
        list->setCurrentIndex(proxy->mapFromSource(files->index(project)));
        QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection);
        if (dialog.result() != QDialog::Accepted || dialog.selectedFiles().first() != project)
            std::cerr << "Open result=" << dialog.result() << ", selected="
                      << dialog.selectedFiles().join(";").toStdString() << ", expected="
                      << project.toStdString() << '\n';
        require(dialog.result() == QDialog::Accepted && dialog.selectedFiles().first() == project,
                "Open must return the selected file");
    }
    {
        ProjectOpenDialog dialog(folder, nullptr, QFileDialog::AcceptSave,
                                 "Wavefront OBJ (*.obj);;Kitchen / Scene GLB (*.glb)");
        dialog.SetExportPreview(thumbnail);
        dialog.selectNameFilter("Kitchen / Scene GLB (*.glb)");
        dialog.setDefaultSuffix("glb");
        dialog.selectFile("New kitchen");
        dialog.show();
        QCoreApplication::processEvents();
        require(dialog.acceptMode() == QFileDialog::AcceptSave && dialog.fileMode() == QFileDialog::AnyFile,
                "Export must allow new files");
        require(dialog.findChild<QSplitter*>("splitter") && dialog.findChild<QToolButton*>("ProjectViewButton"),
                "Export browser controls missing");
        dialog.currentChanged(project);
        require(dialog.findChild<QLabel*>("ProjectPreviewName")->text() == "Current scene",
                "File selection replaced export scene preview");
        const QString screenshot = qEnvironmentVariable("DOM3D_EXPORT_DIALOG_SCREENSHOT");
        if (!screenshot.isEmpty()) require(dialog.grab().save(screenshot), "Export screenshot failed");
        QMetaObject::invokeMethod(&dialog, "accept", Qt::DirectConnection);
        require(dialog.result() == QDialog::Accepted && dialog.selectedFiles().first() == folder + "/New kitchen.glb",
                "Export must return new filename with the chosen suffix");
        require(!QFileInfo::exists(folder + "/New kitchen.glb"), "Dialog must not write export files");
    }
    std::cout << "Project open/export dialog tests passed.\n";
    return 0;
}
