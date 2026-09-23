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

    throttle_ = std::clamp(input.throttle, 0.0f, 1.0f);
    brake_ = std::clamp(input.brake, 0.0f, 1.0f);
    const float acceleration = throttle_ * engineAcceleration - brake_ * brakeAcceleration - speed_ * rollingDrag;
    speed_ = std::max(0.0f, speed_ + acceleration * dt);

    // Placeholder five-speed automatic. RPM follows road speed through a
    // simple per-gear ratio, with throttle adding a little engine flare.
    constexpr float shiftSpeeds[] {0.0f, 8.0f, 15.0f, 23.0f, 31.0f};
    gear_ = 1;
    while (gear_ < 5 && speed_ >= shiftSpeeds[gear_]) ++gear_;
    constexpr float rpmPerMeterPerSecond[] {0.0f, 620.0f, 390.0f, 275.0f, 210.0f, 170.0f};
    rpm_ = std::clamp(900.0f + speed_ * rpmPerMeterPerSecond[gear_] + throttle_ * 350.0f, 900.0f, 7000.0f);

    const float steer = std::clamp(input.steering, -1.0f, 1.0f) * maxSteerRadians;
    const float yawRate = speed_ > 0.05f ? (speed_ / wheelBase) * std::tan(steer) : 0.0f;
    headingRadians_ += yawRate * dt;

    position_.x += std::sin(headingRadians_) * speed_ * dt;
    position_.z += std::cos(headingRadians_) * speed_ * dt;
}

} // namespace osm_drive::physics
