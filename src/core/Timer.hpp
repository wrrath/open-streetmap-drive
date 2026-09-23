#pragma once
#include <chrono>

namespace osm_drive::core {

class Timer {
public:
    using clock = std::chrono::steady_clock;

    float restartSeconds() {
        const auto now = clock::now();
        const std::chrono::duration<float> delta = now - last_;
        last_ = now;
        return delta.count();
    }

private:
    clock::time_point last_ = clock::now();
};

} // namespace osm_drive::core
