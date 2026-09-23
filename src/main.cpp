#include "core/Log.hpp"
#include "game/Game.hpp"
#include "map/OsmLoader.hpp"
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        std::filesystem::path mapPath = std::string(OSM_DRIVE_ASSET_DIR) + "/maps/greenwood_sample.osm";
        const bool validateOnly = argc > 1 && std::string(argv[1]) == "--validate-map";
        if (validateOnly) {
            if (argc != 3) {
                throw std::runtime_error("Usage: osm_drive --validate-map <map.osm>");
            }
            mapPath = argv[2];
            const auto network = osm_drive::map::OsmLoader().load(mapPath);
            if (network.ways().empty()) {
                throw std::runtime_error("Map contains no supported drivable highway ways");
            }
            osm_drive::core::log(osm_drive::core::LogLevel::Info,
                                 "Validated " + std::to_string(network.ways().size()) + " road ways");
            return 0;
        }
        if (argc > 1) {
            mapPath = argv[1];
        }
        osm_drive::game::Game game(mapPath);
        game.run();
        return 0;
    } catch (const std::exception& error) {
        osm_drive::core::log(osm_drive::core::LogLevel::Error, error.what());
        return 1;
    }
}
