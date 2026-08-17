#pragma once

#include "CAlfaDoc.h"

#include <memory>
#include <string>
#include <vector>

class CMesh3D;

class DxfIO {
public:
    bool Import(const std::string& path,
                std::vector<std::unique_ptr<CAlfaObject>>& objects,
                std::string& error) const;
    bool Export(const std::string& path,
                const CAlfaDoc& document,
                std::string& error) const;
};

class EpsIO {
public:
    bool Import(const std::string& path,
                std::vector<std::unique_ptr<CAlfaObject>>& objects,
                std::string& error) const;
    bool Export(const std::string& path,
                const CAlfaDoc& document,
                std::string& error) const;
};

class HpglIO {
public:
    bool Import(const std::string& path,
                std::vector<std::unique_ptr<CAlfaObject>>& objects,
                std::string& error) const;
    bool Export(const std::string& path,
                const CAlfaDoc& document,
                std::string& error) const;
};

class StlIO {
public:
    bool Import(const std::string& path,
                std::vector<std::unique_ptr<CMesh3D>>& meshes,
                std::string& error) const;
    bool Export(const std::string& path,
                const CAlfaDoc& document,
                std::string& error) const;
};
