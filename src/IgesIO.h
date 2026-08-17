#pragma once

#include "CAlfaObject.h"

#include <memory>
#include <string>
#include <vector>

class CAlfaDoc;

class IgesIO {
public:
    bool Import(const std::string& path, std::vector<std::unique_ptr<CAlfaObject>>& objects, std::string& error) const;
    bool Export(const std::string& path, const CAlfaDoc& document, std::string& error) const;
};
