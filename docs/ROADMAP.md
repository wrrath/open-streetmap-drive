# Roadmap

## Rendering

- Replace clear-screen prototype with a render graph.
- Add physically based materials, HDR, temporal AA, cascaded shadow maps, volumetric fog, and sky/atmosphere.
- Build road, shoulder, sign, tree, grass, and building geometry from GIS inputs.
- Add terrain height data from USGS or other open DEM sources.

## Map realism

- Support `.osm.pbf` via libosmium or custom import pipeline.
- Generate intersections, lanes, road markings, guard rails, driveways, and rural props.
- Integrate permissible street-level data providers:
  - Mapillary API for imagery-derived references where license permits.
  - KartaView/open imagery where license permits.
- Cache imported data with attribution metadata.

## Driving

- Tire friction curves, suspension, drivetrain, braking, aero drag.
- Force-feedback ready input layer.
- AI racing line generation over OSM road graph.

## Gameplay

- Time trial and checkpoint modes.
- Procedural rural race routes.
- Replay/ghost system.
