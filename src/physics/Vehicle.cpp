#include "physics/Vehicle.hpp"
#include <algorithm>
#include <cmath>

namespace osm_drive::physics {

void Vehicle::update(const VehicleInput& input, float dt) {
    constexpr float engineAcceleration = 8.0f;
    constexpr float brakeAcceleration = 18.0f;
    constexpr float rollingDrag = 0.75f;
    constexpr float wheelBase = 2.7f;
    constexpr float maxSteerRadians = 0.55f;

    const float acceleration = input.throttle * engineAcceleration - input.brake * brakeAcceleration - speed_ * rollingDrag;
    speed_ = std::max(0.0f, speed_ + acceleration * dt);

    const float steer = std::clamp(input.steering, -1.0f, 1.0f) * maxSteerRadians;
    const float yawRate = speed_ > 0.05f ? (speed_ / wheelBase) * std::tan(steer) : 0.0f;
    headingRadians_ += yawRate * dt;

    position_.x += std::sin(headingRadians_) * speed_ * dt;
    position_.z += std::cos(headingRadians_) * speed_ * dt;
}

} // namespace osm_drive::physics
