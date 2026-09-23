#include "map/OsmLoader.hpp"
#include "core/Log.hpp"
#include <tinyxml2.h>
#include <cmath>
#include <charconv>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace osm_drive::map {
namespace {
constexpr double EarthRadiusMeters = 6378137.0;

float roadWidth(std::string_view highwayClass) {
    if (highwayClass == "motorway" || highwayClass == "trunk") return 12.0f;
    if (highwayClass == "primary" || highwayClass == "secondary") return 8.0f;
    if (highwayClass == "tertiary") return 7.0f;
    if (highwayClass == "residential" || highwayClass == "unclassified") return 5.5f;
    return 4.0f;
}

bool isDrivable(std::string_view highwayClass) {
    static constexpr std::string_view allowed[] = {
        "motorway", "trunk", "primary", "secondary", "tertiary",
        "unclassified", "residential", "service", "living_street"
    };
    for (const auto item : allowed) {
        if (item == highwayClass) return true;
    }
    return false;
}
} // namespace

RoadNetwork OsmLoader::load(const std::filesystem::path& path) const {
    tinyxml2::XMLDocument document;
    if (document.LoadFile(path.string().c_str()) != tinyxml2::XML_SUCCESS) {
        throw std::runtime_error("Failed to load OSM file: " + path.string());
    }

    const auto* osm = document.FirstChildElement("osm");
    if (osm == nullptr) {
        throw std::runtime_error("Invalid OSM file: missing <osm>");
    }

    std::unordered_map<long long, RoadNode> nodes;
    double originLat = 0.0;
    double originLon = 0.0;
    bool haveOrigin = false;

    for (const auto* node = osm->FirstChildElement("node"); node != nullptr; node = node->NextSiblingElement("node")) {
        RoadNode roadNode;
        node->QueryInt64Attribute("id", &roadNode.id);
        node->QueryDoubleAttribute("lat", &roadNode.latitude);
        node->QueryDoubleAttribute("lon", &roadNode.longitude);
        if (!haveOrigin) {
            originLat = roadNode.latitude;
            originLon = roadNode.longitude;
            haveOrigin = true;
        }
        const double latRad = originLat * M_PI / 180.0;
        const double x = (roadNode.longitude - originLon) * M_PI / 180.0 * EarthRadiusMeters * std::cos(latRad);
        const double y = (roadNode.latitude - originLat) * M_PI / 180.0 * EarthRadiusMeters;
        roadNode.meters = {static_cast<float>(x), static_cast<float>(y)};
        nodes.emplace(roadNode.id, roadNode);
    }

    RoadNetwork network;
    for (const auto* way = osm->FirstChildElement("way"); way != nullptr; way = way->NextSiblingElement("way")) {
        RoadWay roadWay;
        way->QueryInt64Attribute("id", &roadWay.id);

        for (const auto* tag = way->FirstChildElement("tag"); tag != nullptr; tag = tag->NextSiblingElement("tag")) {
            const char* key = tag->Attribute("k");
            const char* value = tag->Attribute("v");
            if (key == nullptr || value == nullptr) continue;
            if (std::string_view(key) == "highway") roadWay.highwayClass = value;
            if (std::string_view(key) == "name") roadWay.name = value;
        }

        if (!isDrivable(roadWay.highwayClass)) {
            continue;
        }
        roadWay.widthMeters = roadWidth(roadWay.highwayClass);

        for (const auto* nd = way->FirstChildElement("nd"); nd != nullptr; nd = nd->NextSiblingElement("nd")) {
            long long ref = 0;
            nd->QueryInt64Attribute("ref", &ref);
            if (const auto it = nodes.find(ref); it != nodes.end()) {
                roadWay.nodes.push_back(it->second);
            }
        }

        if (roadWay.nodes.size() >= 2) {
            network.addWay(std::move(roadWay));
        }
    }

    core::log(core::LogLevel::Info, "Loaded OSM road network from " + path.string());
    return network;
}

} // namespace osm_drive::map
