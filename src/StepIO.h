#pragma once

#include "CAlfaDoc.h"

#include <memory>
#include <string>
#include <vector>

class CSolid;

class StepIO {
public:
    bool Import(const std::string& path, std::vector<std::unique_ptr<CSolid>>& solids, std::string& error) const;
    bool Export(const std::string& path, const CAlfaDoc& document, std::string& error) const;
};
