# Open StreetMap Drive Development Plan

This plan breaks the remaining work into clear, separately committable milestones. Each milestone should be implemented as its own branch/commit so the repo history stays easy to follow.

## Ground rules

- Prefer open/licensable data sources.
- Record every data source in `README.md` and, later, `docs/DATA_SOURCES.md`.
- Do not commit proprietary imagery or scraped Street View data.
- Keep Vulkan code heavily commented and beginner-friendly.
- Keep systems modular: rendering, map import, physics, UI, gameplay, and assets should stay separate.

## 1. HUD

Goal: add a simple racing HUD overlay.

Initial HUD fields:

- Speed: mph and km/h.
- RPM: simulated from speed, gear ratio, and throttle.
- Gear: automatic placeholder gearbox for now.
- Optional debug fields: frame time, position, heading.

Implementation plan:

1. Add a `game::VehicleTelemetry` struct.
2. Extend `physics::Vehicle` with speed, rpm, gear, throttle/brake state.
3. Add a Vulkan HUD renderer path.
4. Start with simple vector/bitmap digits, then move to font rendering later.
5. Document how the HUD render pass/pipeline works.

Suggested commits:

- `Add vehicle telemetry model`
- `Render basic Vulkan HUD overlay`
- `Document HUD rendering path`

## 2. Basic car visual

Goal: draw a visible low-poly player car so the follow camera has a target.

Initial asset style:

- Low-poly wedge/sedan shape.
- Single mesh generated in code or loaded from a tiny OBJ/glTF file.
- Simple late-90s color/material palette.

Implementation plan:

1. Add `assets/vehicles/basic_car.obj` or procedural mesh code.
2. Add model transform support: translation, rotation, scale.
3. Add a second Vulkan pipeline or reuse mesh pipeline with per-object push constants.
4. Render car at `Vehicle::position()` with `Vehicle::headingRadians()`.
5. Add comments explaining vertex buffers, index buffers, and push constants.

Suggested commits:

- `Add low-poly car mesh asset`
- `Render player car mesh`
- `Document mesh transform pipeline`

## 3. Real map import

Goal: replace the tiny sample with real Greenwood, Arkansas map data.

Preferred data sources:

- OpenStreetMap Overpass API for roads/buildings/landuse/water/tree rows where available.
- USGS National Map / 3DEP for elevation data.
- USGS National Land Cover Database or similar open land cover data for environment classification.
- Mapillary or KartaView only as visual reference/metadata where licensing permits. Do not scrape or bake restricted imagery.

Google note:

- Google Street View imagery/geometry should not be used as game asset source unless a valid license/API permission explicitly allows it. The README must state this clearly.

Implementation plan:

1. Add `tools/fetch_osm_greenwood.py` using Overpass API.
2. Save imported OSM data under `assets/maps/generated/` or document that large data is locally generated and ignored by git.
3. Expand parser support:
   - roads/highways
   - buildings
   - landuse/natural tags
   - water features
   - barriers/fences if present
4. Add attribution metadata file.
5. Update README with exact source URLs, dates, and license notes.

Suggested commits:

- `Add Greenwood OSM fetch tool`
- `Import real Greenwood road extract`
- `Document map data sources and licenses`

## 4a. More realistic car graphics

Goal: improve the car toward a late-90s/PS1 Gran Turismo-inspired look.

Art direction:

- Low/moderate polygon count.
- Hard-edged silhouette with simple curved impression.
- Glossy paint, dark windows, simple wheels.
- Optional vertex-color lighting aesthetic.

Implementation plan:

1. Replace wedge car with a cleaner low-poly coupe/sedan mesh.
2. Add multiple materials: body paint, glass, tire rubber, wheel metal, lights.
3. Add simple directional lighting and material parameters.
4. Add fake reflections/environment tint later.
5. Document asset pipeline: OBJ/glTF import, coordinate conventions, scale.

Suggested commits:

- `Add late-90s styled low-poly car asset`
- `Add basic material support for vehicle rendering`
- `Document vehicle asset pipeline`

## 4b. More realistic environment graphics

Goal: make the rural environment look geographically plausible and eventually highly realistic.

Data-driven environment layers:

- Roads from OpenStreetMap.
- Terrain elevation from USGS 3DEP DEM.
- Land cover from USGS/NLCD or equivalent open raster data.
- Buildings/fences/water/forest/fields from OSM where available.
- Procedural vegetation placed from OSM/natural/landuse and land cover data.

Graphics roadmap:

1. Add terrain plane/grid.
2. Import DEM heightmap and displace terrain.
3. Add grass/soil/asphalt materials.
4. Add tree/grass instancing.
5. Add sky, sun, shadows, fog/haze.
6. Add rural props: signs, mailboxes, utility poles, fences.
7. Add road shoulders, ditches, lane paint, intersections.
8. Add PBR-ish materials, HDR, tonemapping, shadow maps.

Suggested commits:

- `Add terrain mesh system`
- `Add USGS elevation import notes/tooling`
- `Render land-cover based terrain materials`
- `Add procedural rural vegetation`
- `Add sun lighting and shadows`
- `Document environment generation pipeline`

## 5. Cleanup and documentation

Goal: make the project teach Vulkan clearly to a developer with C++ and some OpenGL knowledge.

Documentation targets:

- `docs/ARCHITECTURE.md`: how modules fit together.
- `docs/VULKAN_OVERVIEW.md`: Vulkan concepts used here.
- `docs/RENDERER_WALKTHROUGH.md`: instance/device/swapchain/render pass/pipeline/buffers.
- `docs/MAP_PIPELINE.md`: OSM import, coordinate conversion, road mesh generation.
- `docs/PHYSICS.md`: vehicle model and future improvements.
- `docs/ASSET_PIPELINE.md`: meshes, shaders, textures, generated data.
- `docs/ROADMAP.md`: keep updated as completed.

Code cleanup targets:

- Wrap Vulkan handles in RAII classes.
- Add named helper types for buffers, pipelines, shaders, swapchain.
- Improve error messages.
- Add validation layer support in debug builds.
- Add frame-in-flight support instead of one global fence/semaphore set.
- Add resize-safe swapchain recreation.
- Add consistent comments before each Vulkan setup block.

Suggested commits:

- `Add Vulkan beginner documentation`
- `Refactor Vulkan buffers into RAII wrapper`
- `Add debug validation layer support`
- `Document map and asset pipelines`

## Repo publishing/privacy

Current repo exists on GitHub. At final milestone completion, switch it to private if desired:

```bash
gh repo edit wrrath/open-streetmap-drive --visibility private --accept-visibility-change-consequences
```

Do not make the repo private until you are ready, because visibility changes can affect sharing and cloning.
