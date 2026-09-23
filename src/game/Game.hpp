#pragma once
#include "graphics/VulkanRenderer.hpp"
#include "map/RoadNetwork.hpp"
#include "physics/Vehicle.hpp"
#include <filesystem>

namespace osm_drive::game {

class Game {
public:
    explicit Game(std::filesystem::path mapPath);
    void run();

private:
    physics::VehicleInput readInput() const;

    graphics::VulkanRenderer renderer_ {1280, 720, "Open StreetMap Drive"};
    map::RoadNetwork roadNetwork_;
    map::RoadMesh roadMesh_;
    physics::Vehicle player_;
};

} // namespace osm_drive::game
