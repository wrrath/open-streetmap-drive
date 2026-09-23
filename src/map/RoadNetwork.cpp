#include "map/RoadNetwork.hpp"
#include <glm/geometric.hpp>

namespace osm_drive::map {

RoadMesh RoadNetwork::buildRoadMesh() const {
    RoadMesh mesh;

    for (const RoadWay& way : ways_) {
        if (way.nodes.size() < 2) {
            continue;
        }

        for (std::size_t i = 0; i + 1 < way.nodes.size(); ++i) {
            const glm::vec2 a = way.nodes[i].meters;
            const glm::vec2 b = way.nodes[i + 1].meters;
            const glm::vec2 direction = glm::normalize(b - a);
            const glm::vec2 normal2 {-direction.y, direction.x};
            const float halfWidth = way.widthMeters * 0.5f;

            const float segmentLength = glm::length(b - a);
            const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
            mesh.vertices.push_back({glm::vec3(a.x + normal2.x * halfWidth, 0.0f, a.y + normal2.y * halfWidth), {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}});
            mesh.vertices.push_back({glm::vec3(a.x - normal2.x * halfWidth, 0.0f, a.y - normal2.y * halfWidth), {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}});
            mesh.vertices.push_back({glm::vec3(b.x - normal2.x * halfWidth, 0.0f, b.y - normal2.y * halfWidth), {0.0f, 1.0f, 0.0f}, {1.0f, segmentLength}});
            mesh.vertices.push_back({glm::vec3(b.x + normal2.x * halfWidth, 0.0f, b.y + normal2.y * halfWidth), {0.0f, 1.0f, 0.0f}, {0.0f, segmentLength}});
            mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
    }

    return mesh;
}

} // namespace osm_drive::map
