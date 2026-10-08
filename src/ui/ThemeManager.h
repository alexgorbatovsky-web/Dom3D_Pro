#pragma once

#include <QColor>
#include <QPalette>
#include <QString>
#include <QVector>

class QWidget;
class QSettings;

namespace Themes {
struct Scheme {
    QString id;
    QString name;
    QColor window, base, button, text, accent, border;
    QColor activeButton;
};
QVector<Scheme> Presets();
QPalette Palette(const Scheme& scheme);
Scheme Load(QSettings& settings);
void Save(QSettings& settings, const Scheme& scheme);
bool SaveFile(const QString& path, const Scheme& scheme, QString& error);
bool LoadFile(const QString& path, Scheme& scheme, QString& error);
void Apply(const Scheme& scheme);
void Initialize();
void ShowDialog(QWidget* parent);
}
