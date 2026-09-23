#pragma once
#include "map/RoadNetwork.hpp"
#include <filesystem>

namespace osm_drive::map {

class OsmLoader {
public:
    [[nodiscard]] RoadNetwork load(const std::filesystem::path& path) const;
};

} // namespace osm_drive::map
