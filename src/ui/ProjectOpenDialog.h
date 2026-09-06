#pragma once

#include <QFileDialog>

class QLabel;
class QActionGroup;
class QAbstractItemDelegate;
class QLineEdit;
class QListWidget;
class QSplitter;
class QTimer;
class ProjectThumbnailLabel;

// A large file browser with an integrated, resizable project preview.
class ProjectOpenDialog : public QFileDialog {
    Q_OBJECT
public:
    explicit ProjectOpenDialog(const QString& directory, QWidget* parent = nullptr);

protected:
    void done(int result) override;

private:
    void QueuePreview(const QString& path);
    void LoadPreview();
    void SetBrowserView(int mode);
    void UpdatePlaceSelection(const QString& path);

    QSplitter* browser_splitter_ = nullptr;
    QListWidget* places_ = nullptr;
    QLineEdit* search_ = nullptr;
    ProjectThumbnailLabel* preview_ = nullptr;
    QLabel* preview_name_ = nullptr;
    QLabel* preview_details_ = nullptr;
    QTimer* preview_timer_ = nullptr;
    QString preview_path_;
    int browser_view_ = 5; // Details; the other modes use QFileDialog's list view.
    QActionGroup* view_actions_ = nullptr;
    QAbstractItemDelegate* default_list_delegate_ = nullptr;
    QAbstractItemDelegate* information_delegate_ = nullptr;
};
