# Open StreetMap Drive

A Vulkan/C++ racing-game prototype that turns real rural road data into a drivable course. The starter map is Greenwood, Arkansas, using OpenStreetMap data.

## Goals

- Real-world roads from OpenStreetMap (`.osm` XML now; `.pbf` later).
- Vulkan renderer with a clean, expandable architecture.
- Realistic driving direction: physically-inspired vehicle model, rural terrain, high-quality materials/lighting roadmap.
- Optional street-level imagery integration via legally usable providers such as Mapillary or KartaView. Google Street View is **not** included because its terms generally do not permit extracting imagery/geometry for game assets without a specific license/API use case.

## Current prototype

This repository provides a clean C++20/Vulkan foundation:

- GLFW window and Vulkan instance/device/swapchain setup.
- Road mesh upload to Vulkan vertex/index buffers and rendering through a simple graphics pipeline when `glslc` is available at CMake configure time.
- Procedural terrain grid with grass/soil materials and a data-source interface for future elevation and land-cover rasters.
- Game loop with fixed timestep physics.
- OSM XML loader for highways around Greenwood, AR.
- Coordinate conversion from latitude/longitude to local meters.
- Road graph and simple road mesh generation.
- Basic vehicle controller/physics scaffolding.
- Expandable renderer, world, map, and physics modules.

## Requirements

- CMake 3.22+
- C++20 compiler
- Vulkan SDK (`VULKAN_SDK` configured)
- `glslc` or `glslangValidator` for road mesh shaders (`sudo apt install glslc` or `sudo apt install glslang-tools` on Ubuntu/WSL)
- Linux packages commonly needed: `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/osm_drive
```

## Getting real Greenwood OSM data

A tiny synthetic sample is included at `assets/maps/greenwood_sample.osm`. Fetch a
real Greenwood, Arkansas extract (roads plus future environment layers) with:

```bash
python3 tools/fetch_osm_greenwood.py
./build/osm_drive --validate-map assets/maps/generated/greenwood.osm
./build/osm_drive assets/maps/generated/greenwood.osm
```

The generated map and per-download attribution metadata are ignored by Git to
avoid committing a large, frequently changing database extract. The fetch tool,
exact bounding box/query, data provenance, and reuse obligations are documented
in [`docs/DATA_SOURCES.md`](docs/DATA_SOURCES.md).

## Documentation

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — module boundaries and end-to-end data flow.
- [`docs/VULKAN_OVERVIEW.md`](docs/VULKAN_OVERVIEW.md) — beginner-friendly Vulkan setup and frame flow.
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — planned features.
- [`docs/DEVELOPMENT_PLAN.md`](docs/DEVELOPMENT_PLAN.md) — milestone plan.

## Data and license notes

See [`docs/DATA_SOURCES.md`](docs/DATA_SOURCES.md) for intended USGS 3DEP elevation and Annual NLCD land-cover sources, metadata requirements, and links. The current terrain is generated in code and adds no external asset license.

- OpenStreetMap data is © OpenStreetMap contributors and licensed under the
  [ODbL 1.0](https://opendatacommons.org/licenses/odbl/1-0/). See
  [`docs/DATA_SOURCES.md`](docs/DATA_SOURCES.md) for source URLs, retrieval
  metadata, and the current inventory of included data.
- Google Street View assets are not used. Do not scrape or commit its imagery or
  geometry; use only sources whose licenses permit the intended game/runtime use.
