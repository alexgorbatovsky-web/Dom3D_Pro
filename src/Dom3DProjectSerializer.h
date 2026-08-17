#pragma once

#include "CAlfaDoc.h"
#include "Common.h"

#include <QString>
#include <QImage>

#include <functional>

struct ProjectViewState {
    Camera camera{};
    bool has_camera = false;
    bool orthographic_projection = false;
    bool has_orthographic_projection = false;
    OrbitMode orbit_mode = OrbitMode::CAD;
    bool has_orbit_mode = false;
    bool show_coordinate_axes = true;
    bool has_show_coordinate_axes = false;
    bool show_floor_grid = true;
    bool has_show_floor_grid = false;
    bool xy_plane_view = false;
    bool has_xy_plane_view = false;
};

class Dom3DProjectSerializer {
public:
    using ProgressCallback = std::function<void(int, const QString&)>;

    bool Save(const QString& path,
              const CAlfaDoc& document,
              const QString& active_room,
              const ProjectViewState& view_state,
              const QImage& thumbnail,
              QString& error) const;
    bool Load(const QString& path,
              CAlfaDoc& document,
              QString& active_room,
              ProjectViewState& view_state,
              QString& error,
              const ProgressCallback& progress = {}) const;
    bool LoadThumbnail(const QString& path, QImage& thumbnail, QString& error) const;
    bool ImportPart(const QString& path,
                    CAlfaDoc& document,
                    const QString& part_name,
                    Vec3 insertion_point,
                    Vec3 scale,
                    bool file_linked,
                    bool wrap_as_part,
                    QString& error) const;
    bool SaveSelection(const QString& path,
                       CAlfaDoc& document,
                       const std::vector<size_t>& selected_indices,
                       const QString& active_room,
                       const ProjectViewState& view_state,
                       const QImage& thumbnail,
                       QString& error) const;
};
