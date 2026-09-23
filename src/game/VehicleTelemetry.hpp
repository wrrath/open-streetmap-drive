#pragma once

namespace osm_drive::game {

// Snapshot passed from gameplay to presentation. Keeping this renderer-neutral
// makes it easy to replace the placeholder vehicle model later.
struct VehicleTelemetry {
    float speedMetersPerSecond = 0.0f;
    float rpm = 900.0f;
    int gear = 1;
    float throttle = 0.0f;
    float brake = 0.0f;
};

} // namespace osm_drive::game
