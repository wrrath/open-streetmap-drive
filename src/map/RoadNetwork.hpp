#pragma once
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace osm_drive::map {

struct RoadNode {
    long long id = 0;
    double latitude = 0.0;
    double longitude = 0.0;
    glm::vec2 meters {0.0f, 0.0f};
};

struct RoadWay {
    long long id = 0;
    std::string name;
    std::string highwayClass;
    float widthMeters = 6.0f;
    std::vector<RoadNode> nodes;
};

struct RoadVertex {
    glm::vec3 position {0.0f};
    glm::vec3 normal {0.0f, 1.0f, 0.0f};
    glm::vec2 uv {0.0f};
};

struct RoadMesh {
    std::vector<RoadVertex> vertices;
    std::vector<std::uint32_t> indices;
};

class RoadNetwork {
public:
    void addWay(RoadWay way) { ways_.push_back(std::move(way)); }
    [[nodiscard]] const std::vector<RoadWay>& ways() const { return ways_; }
    [[nodiscard]] RoadMesh buildRoadMesh() const;

private:
    std::vector<RoadWay> ways_;
};

} // namespace osm_drive::map
