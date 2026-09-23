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
    [[nodiscard]] float rpm() const { return rpm_; }
    [[nodiscard]] int gear() const { return gear_; }
    [[nodiscard]] float throttle() const { return throttle_; }
    [[nodiscard]] float brake() const { return brake_; }

private:
    glm::vec3 position_ {0.0f, 0.35f, 0.0f};
    float headingRadians_ = 0.0f;
    float speed_ = 0.0f;
    float rpm_ = 900.0f;
    int gear_ = 1;
    float throttle_ = 0.0f;
    float brake_ = 0.0f;
};

} // namespace osm_drive::physics
