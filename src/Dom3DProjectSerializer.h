#pragma once

#include "CAlfaDoc.h"

#include <QString>

#include <memory>

class QByteArray;
class CSolid;

class Dom3DProjectSerializer {
public:
    bool Save(const QString& path, const CAlfaDoc& document, const QString& active_room, QString& error) const;
    bool Load(const QString& path, CAlfaDoc& document, QString& active_room, QString& error) const;

private:
    bool SaveSolidStep(const CSolid& solid, QByteArray& step_data, QString& error) const;
    bool LoadSolidStep(const QByteArray& step_data, std::unique_ptr<CSolid>& solid, QString& error) const;
};
