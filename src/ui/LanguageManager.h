#pragma once

#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>

class QAction;
class QMenu;
class QWidget;

class LanguageManager : public QObject {
    Q_OBJECT

public:
    static LanguageManager& Instance();

    QString Text(const QString& id, const QString& fallback = {}) const;
    QString Translate(const QString& source) const;
    QString CurrentLanguage() const;
    QStringList AvailableLanguages() const;
    QString LanguageDisplayName(const QString& file_stem) const;
    bool SetLanguage(const QString& file_stem);

    void BindText(QAction* action, const QString& id, const QString& fallback);
    void BindText(QMenu* menu, const QString& id, const QString& fallback);
    void BindText(QWidget* widget, const QString& id, const QString& fallback);
    void BindToolTip(QWidget* widget, const QString& id, const QString& fallback);
    void Apply(QObject* root) const;

signals:
    void LanguageChanged();

private:
    struct TextTemplate {
        QString id;
        QString source;
        QRegularExpression expression;
        QHash<int, int> capture_for_placeholder;
    };

    explicit LanguageManager(QObject* parent = nullptr);
    bool eventFilter(QObject* watched, QEvent* event) override;
    QString LanguagesPath() const;
    bool LoadFile(const QString& path, QHash<QString, QString>& values) const;
    void RebuildSourceIndex(const QHash<QString, QString>& previous_values);
    void TranslateObject(QObject* object) const;

    QString current_language_ = "English";
    QHash<QString, QString> values_;
    QHash<QString, QString> english_values_;
    QHash<QString, QString> source_ids_;
    QVector<TextTemplate> templates_;
    quint64 translation_generation_ = 0;
    mutable bool translating_ = false;
};

inline QString DomText(const QString& id, const QString& fallback) {
    return LanguageManager::Instance().Text(id, fallback);
}

inline QString DomTranslate(const QString& source) {
    return LanguageManager::Instance().Translate(source);
}
