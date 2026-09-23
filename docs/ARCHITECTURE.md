# Project architecture

This document is a tour of the current prototype for a developer who knows C++ and has used OpenGL. The project is deliberately small: OpenStreetMap data is converted to a road mesh, a simple vehicle is simulated, and Vulkan draws the roads from a follow camera.

## The program at a glance

```text
main.cpp
   |
   v
game::Game -------------------------- owns the main loop
   |                 |                  |
   v                 v                  v
map::OsmLoader   physics::Vehicle   graphics::VulkanRenderer
   |                                    ^
   v                                    |
map::RoadNetwork -- buildRoadMesh() --> RoadMesh
```

The dependencies point mostly in one direction:

- **`game` coordinates systems.** It reads input, advances physics, updates the camera, and asks the renderer for a frame.
- **`map` owns geographic parsing and CPU-side road geometry.** It does not know about Vulkan.
- **`physics` owns vehicle state and movement.** It does not know about GLFW or rendering.
- **`graphics` owns the window and Vulkan objects.** It accepts a plain `RoadMesh` from the map module.
- **`core` contains small shared utilities** for logging and timing.

This separation matters as the prototype grows. For example, a future map importer can change without changing vehicle physics, and a better vehicle model should not need to know how command buffers work.

## Startup and ownership

`src/main.cpp` selects either the bundled sample map or the path supplied as the first command-line argument, constructs `game::Game`, and reports exceptions.

A `Game` owns its major systems by value. C++ initializes members in declaration order, so `renderer_` is constructed first and creates the window/Vulkan context before the `Game` constructor body loads the map. The constructor then follows this path:

1. `map::OsmLoader::load()` reads OSM XML with tinyxml2.
2. `RoadNetwork::buildRoadMesh()` turns road segments into triangles.
3. `VulkanRenderer::setRoadMesh()` copies those vertices and indices into Vulkan buffers.

The renderer destructor waits for the GPU to become idle and destroys resources in an order compatible with their dependencies. Vulkan handles are currently explicit members rather than RAII wrapper types, so `VulkanRenderer` is non-copyable and is responsible for all cleanup.

## Map module: XML to local geometry

Relevant files:

- `src/map/OsmLoader.*`
- `src/map/RoadNetwork.*`
- `assets/maps/greenwood_sample.osm`

OSM stores points as latitude/longitude **nodes** and roads as ordered **ways** that reference those nodes. The loader:

1. reads all nodes into a lookup table;
2. chooses the first node as a local origin;
3. approximates longitude/latitude offsets as local metres;
4. keeps ways whose `highway` tag represents a drivable road;
5. assigns a simple width based on the highway class.

Using local metres avoids sending large world-coordinate values to the renderer. The conversion is a local, flat approximation, not a general map projection; it is adequate for a small sample but should be replaced for large regions.

`buildRoadMesh()` expands every line segment sideways by half the road width. It emits four `RoadVertex` values and six indices (two triangles) per segment. The OSM north/south metre coordinate becomes world Z, longitude becomes world X, and Y is up:

```text
world X = east
world Y = up
world Z = north
units   = metres
```

Segments are independent quads. There is not yet special geometry for joins, intersections, shoulders, elevation, or lane count.

## Physics and game loop

Relevant files:

- `src/game/Game.*`
- `src/physics/Vehicle.*`
- `src/core/Timer.hpp`

`Game::run()` polls GLFW events as often as possible, but advances the vehicle in fixed 1/60-second steps. An accumulator preserves leftover time between frames. A 0.1-second clamp prevents a pause or debugger stop from causing a very large burst of simulation updates.

The vehicle is a simple kinematic bicycle-style model:

- W/S supply throttle and brake;
- A/D supply steering;
- acceleration, braking, and linear rolling drag change forward speed;
- wheelbase and steering angle determine yaw rate;
- heading and speed update X/Z position.

It is scaffolding rather than a tire, suspension, or collision simulation. The vehicle is currently invisible; its transform only drives the follow camera.

After simulation, `Game` gives the latest vehicle position and heading to the renderer, then calls `drawFrame()`. Rendering can therefore happen at a different rate from physics. There is no interpolation between the two latest physics states yet.

## Graphics module

Relevant files:

- `src/graphics/VulkanRenderer.*`
- `assets/shaders/road.vert`
- `assets/shaders/road.frag`

`VulkanRenderer` currently combines window setup, Vulkan lifetime management, GPU upload, camera calculation, and drawing. That keeps the first prototype easy to trace, although these responsibilities should eventually become smaller RAII-backed types.

At initialization it creates, in dependency order:

1. a Vulkan instance and GLFW surface;
2. a physical-device selection, logical device, and graphics/present queue;
3. the swapchain, image views, render pass, and framebuffers;
4. the road graphics pipeline when compiled SPIR-V shaders are available;
5. a command pool, one command buffer per swapchain image, and synchronization objects.

The road mesh uses host-visible, coherent memory for straightforward upload. This is easy to understand, but a production renderer would usually upload through a staging buffer into device-local memory.

Every frame, the renderer builds a follow-camera view/projection matrix, sends it as a small push constant, binds the road pipeline and buffers, and issues one indexed draw. See [VULKAN_OVERVIEW.md](VULKAN_OVERVIEW.md) for the Vulkan concepts and exact frame flow.

## Build-time data flow

CMake fetches GLFW, GLM, and tinyxml2 and locates the Vulkan SDK. If `glslc` or `glslangValidator` is available while CMake configures, it compiles the GLSL files to SPIR-V under `build/shaders/` and makes shader compilation a dependency of the executable.

If neither shader compiler is found, the C++ program can still build. At runtime the renderer logs a warning and clears the window without drawing roads because no graphics pipeline can be created.

## Current boundaries and limitations

These are intentional prototype constraints, not hidden features:

- only one graphics/present queue family is used;
- the first Vulkan physical device is selected without suitability scoring;
- there is one frame in flight, enforced by one fence;
- window resize and out-of-date swapchain recreation are not implemented;
- there is no depth buffer, so future overlapping 3D geometry needs depth support;
- validation layers are not enabled;
- road buffers and most Vulkan handles are manually managed;
- map parsing, mesh generation, and upload happen synchronously at startup;
- there are no automated tests yet.

These constraints make the current control flow compact. They are also natural seams for future work: map jobs can feed immutable CPU meshes to a render-upload queue, and renderer resources can move into dedicated RAII classes without coupling those changes to physics.

## Where to make common changes

| Goal | Start here |
| --- | --- |
| Support another OSM road tag | `OsmLoader.cpp` (`isDrivable`, `roadWidth`) |
| Change road geometry | `RoadNetwork.cpp` |
| Change controls or loop behavior | `Game.cpp` |
| Tune movement | `Vehicle.cpp` |
| Change road vertex attributes | `RoadNetwork.hpp`, `VulkanRenderer.cpp`, and `road.vert` together |
| Change road color/markings | `assets/shaders/road.frag` |
| Add a Vulkan rendering feature | `VulkanRenderer.*` (then split out a focused helper as it grows) |
