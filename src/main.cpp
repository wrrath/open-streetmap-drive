#include "core/Log.hpp"
#include "game/Game.hpp"
#include <exception>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    try {
        std::filesystem::path mapPath = std::string(OSM_DRIVE_ASSET_DIR) + "/maps/greenwood_sample.osm";
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
