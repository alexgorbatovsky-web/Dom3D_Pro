#pragma once

#include "CAlfaDoc.h"

#include <functional>
#include <string>

class ProjectIO {
public:
    using ProgressCallback = std::function<void(int, const std::string&)>;

    bool Save(const std::string& path, const CAlfaDoc& document, std::string& error) const;
    bool Load(const std::string& path,
              CAlfaDoc& document,
              std::string& error,
              const ProgressCallback& progress = {}) const;
};
