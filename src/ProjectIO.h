#pragma once

#include "CAlfaDoc.h"

#include <string>

class ProjectIO {
public:
    bool Save(const std::string& path, const CAlfaDoc& document, std::string& error) const;
    bool Load(const std::string& path, CAlfaDoc& document, std::string& error) const;
};
