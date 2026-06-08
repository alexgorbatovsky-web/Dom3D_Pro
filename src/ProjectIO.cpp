#include "ProjectIO.h"

#include "CBSpline.h"
#include "CMesh3D.h"

#include <fstream>
#include <memory>
#include <utility>
#include <vector>

bool ProjectIO::Save(const std::string& path, const CAlfaDoc& document, std::string& error) const {
    std::ofstream file(path);
    if (!file) {
        error = "Could not save project file.";
        return false;
    }

    file << "Dom3DProProject 2\n";
    file << "Objects " << document.GetObjects().size() << "\n";
    for (const auto& object : document.GetObjects()) {
        if (!object->Save(file)) {
            error = "Could not write object data.";
            return false;
        }
    }

    return true;
}

bool ProjectIO::Load(const std::string& path, CAlfaDoc& document, std::string& error) const {
    std::ifstream file(path);
    if (!file) {
        error = "Could not open project file.";
        return false;
    }

    std::string header;
    int version = 0;
    file >> header >> version;
    if ((header != "Dom3DProProject" && header != "Dom3DModernProject") || (version != 1 && version != 2)) {
        error = "This is not a Dom3D Pro project file.";
        return false;
    }

    CAlfaDoc::ObjectList loaded;

    if (version == 1) {
        std::string section;
        size_t count = 0;
        file >> section >> count;
        if (section != "CurvePoints") {
            error = "Project file has no curve data.";
            return false;
        }

        CPolyline polyline("Curve 1");
        polyline.SetColor({0.98f, 0.77f, 0.30f});
        for (size_t i = 0; i < count; ++i) {
            CurvePoint point{};
            file >> point.x >> point.z;
            if (!file) {
                error = "Project file is incomplete.";
                return false;
            }
            polyline.AddPoint(point);
        }
        loaded.push_back(std::make_unique<CPolyline>(std::move(polyline)));
    } else {
        std::string section;
        size_t count = 0;
        file >> section >> count;
        if (section != "Polylines" && section != "Objects") {
            error = "Project file has no object data.";
            return false;
        }

        loaded.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            file >> std::ws;
            const int next = file.peek();
            std::unique_ptr<CAlfaObject> object;
            if (next == 'P') {
                auto polyline = std::make_unique<CPolyline>();
                if (!polyline->Load(file)) {
                    error = "Project file has invalid object data.";
                    return false;
                }
                object = std::move(polyline);
            } else if (next == 'B') {
                auto spline = std::make_unique<CBSpline>();
                if (!spline->Load(file)) {
                    error = "Project file has invalid object data.";
                    return false;
                }
                object = std::move(spline);
            } else if (next == 'M') {
                auto mesh = std::make_unique<CMesh3D>();
                if (!mesh->Load(file)) {
                    error = "Project file has invalid object data.";
                    return false;
                }
                object = std::move(mesh);
            } else {
                error = "Project file has invalid object data.";
                return false;
            }
            loaded.push_back(std::move(object));
        }
    }

    document.GetObjects() = std::move(loaded);
    document.ClearSelection();
    if (document.GetObjects().empty()) {
        document.Clear();
    }
    return true;
}
