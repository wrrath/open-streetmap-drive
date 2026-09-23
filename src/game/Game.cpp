#include "game/Game.hpp"
#include "core/Log.hpp"
#include "core/Timer.hpp"
#include "map/OsmLoader.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <sstream>

namespace osm_drive::game {

Game::Game(std::filesystem::path mapPath) {
    map::OsmLoader loader;
    roadNetwork_ = loader.load(mapPath);
    roadMesh_ = roadNetwork_.buildRoadMesh();

    std::ostringstream message;
    message << "Generated road mesh: " << roadMesh_.vertices.size() << " vertices, "
            << roadMesh_.indices.size() << " indices";
    core::log(core::LogLevel::Info, message.str());
    renderer_.setRoadMesh(roadMesh_);
}

void Game::run() {
    core::Timer timer;
    float accumulator = 0.0f;
    constexpr float fixedDt = 1.0f / 60.0f;

    while (!renderer_.shouldClose()) {
        renderer_.pollEvents();
        accumulator += std::min(timer.restartSeconds(), 0.1f);

        while (accumulator >= fixedDt) {
            player_.update(readInput(), fixedDt);
            accumulator -= fixedDt;
        }

        renderer_.setFollowCamera(player_.position(), player_.headingRadians());
        renderer_.setVehicleTelemetry({player_.speedMetersPerSecond(), player_.rpm(), player_.gear(),
                                       player_.throttle(), player_.brake()});
        renderer_.drawFrame();
    }
}

physics::VehicleInput Game::readInput() const {
    physics::VehicleInput input;
    GLFWwindow* window = renderer_.window();
    input.throttle = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ? 1.0f : 0.0f;
    input.brake = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ? 1.0f : 0.0f;
    const bool left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    const bool right = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    input.steering = (left ? -1.0f : 0.0f) + (right ? 1.0f : 0.0f);
    return input;
}

} // namespace osm_drive::game
