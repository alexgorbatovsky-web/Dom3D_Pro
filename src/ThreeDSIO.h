#pragma once

#include "CMesh3D.h"

#include <memory>
#include <string>
#include <vector>

class ThreeDSIO {
public:
    bool Import(const std::string& path,
                std::vector<std::unique_ptr<CMesh3D>>& meshes,
                std::string& error) const;
};
