#pragma once

#include "map/RoadNetwork.hpp"
#include <glm/vec2.hpp>
#include <cstdint>

namespace osm_drive::environment {

// Deliberately small land-cover vocabulary at the renderer boundary. Importers can
// translate NLCD/OSM classes into these values without coupling GIS code to Vulkan.
enum class LandCover : std::uint8_t {
    Grass,
    Soil,
};

struct TerrainSample {
    float elevationMeters = 0.0f;
    LandCover landCover = LandCover::Grass;
};

// Hook for a future tiled USGS 3DEP/NLCD source. Coordinates are local map meters,
// matching RoadNode::meters; implementations own reprojection and raster sampling.
class TerrainDataSource {
public:
    virtual ~TerrainDataSource() = default;
    [[nodiscard]] virtual TerrainSample sample(glm::vec2 localMeters) const = 0;
};

struct TerrainGrid {
    float widthMeters = 1200.0f;
    float depthMeters = 1200.0f;
    std::uint32_t columns = 32;
    std::uint32_t rows = 32;
};

// Uses RoadMesh's compact position/normal/uv vertex format. uv.x stores the
// normalized land-cover class and uv.y supplies a low-frequency material pattern.
[[nodiscard]] map::RoadMesh buildTerrainMesh(const TerrainGrid& grid,
                                             const TerrainDataSource& source);

// A no-asset baseline while no downloaded GIS raster is configured.
class ProceduralRuralSource final : public TerrainDataSource {
public:
    [[nodiscard]] TerrainSample sample(glm::vec2 localMeters) const override;
};

} // namespace osm_drive::environment
