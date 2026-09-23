#pragma once
#include <glm/vec3.hpp>

namespace osm_drive::physics {

struct VehicleInput {
    float throttle = 0.0f;
    float brake = 0.0f;
    float steering = 0.0f;
};

class Vehicle {
public:
    void update(const VehicleInput& input, float dt);
    [[nodiscard]] glm::vec3 position() const { return position_; }
    [[nodiscard]] float headingRadians() const { return headingRadians_; }
    [[nodiscard]] float speedMetersPerSecond() const { return speed_; }

private:
    glm::vec3 position_ {0.0f, 0.35f, 0.0f};
    float headingRadians_ = 0.0f;
    float speed_ = 0.0f;
};

} // namespace osm_drive::physics
