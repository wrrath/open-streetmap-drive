#include "environment/Terrain.hpp"
#include <cmath>
#include <stdexcept>

namespace osm_drive::environment {

map::RoadMesh buildTerrainMesh(const TerrainGrid& grid, const TerrainDataSource& source) {
    if (grid.columns == 0 || grid.rows == 0 || grid.widthMeters <= 0.0f || grid.depthMeters <= 0.0f) {
        throw std::invalid_argument("Terrain grid dimensions must be positive");
    }

    map::RoadMesh mesh;
    const std::uint32_t stride = grid.columns + 1;
    mesh.vertices.reserve(static_cast<std::size_t>(stride) * (grid.rows + 1));
    mesh.indices.reserve(static_cast<std::size_t>(grid.columns) * grid.rows * 6);

    for (std::uint32_t row = 0; row <= grid.rows; ++row) {
        for (std::uint32_t column = 0; column <= grid.columns; ++column) {
            const float x = (static_cast<float>(column) / static_cast<float>(grid.columns) - 0.5f) * grid.widthMeters;
            const float z = (static_cast<float>(row) / static_cast<float>(grid.rows) - 0.5f) * grid.depthMeters;
            const TerrainSample sample = source.sample({x, z});
            const float cover = sample.landCover == LandCover::Soil ? 1.0f : 0.0f;
            mesh.vertices.push_back({{x, sample.elevationMeters - 0.03f, z}, {0.0f, 1.0f, 0.0f},
                                     {cover, (x + z) * 0.025f}});
        }
    }

    for (std::uint32_t row = 0; row < grid.rows; ++row) {
        for (std::uint32_t column = 0; column < grid.columns; ++column) {
            const std::uint32_t a = row * stride + column;
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + stride + 1;
            const std::uint32_t d = a + stride;
            mesh.indices.insert(mesh.indices.end(), {a, d, c, a, c, b});
        }
    }
    return mesh;
}

TerrainSample ProceduralRuralSource::sample(glm::vec2 localMeters) const {
    // Broad deterministic patches suggest pasture and exposed soil without a
    // texture asset. This is visual fallback data, not a geographic claim.
    const float patches = std::sin(localMeters.x * 0.012f) + std::cos(localMeters.y * 0.015f);
    return {0.0f, patches > 1.15f ? LandCover::Soil : LandCover::Grass};
}

} // namespace osm_drive::environment
