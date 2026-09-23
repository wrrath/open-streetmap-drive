# Data sources and attribution

## Greenwood, Arkansas OpenStreetMap extract

`tools/fetch_osm_greenwood.py` downloads an OSM XML extract for the bounding box
`35.1750,-94.3000,35.2450,-94.2050` (south, west, north, east). The query requests
roads plus building, land-use, natural, waterway, and barrier ways/relations. The
current renderer loads the highway ways; the other tagged features are retained
for later environment import.

- **Data source:** [OpenStreetMap](https://www.openstreetmap.org/), queried through
  the public [Overpass Kumi interpreter](https://overpass.kumi.systems/api/interpreter)
- **Attribution:** © OpenStreetMap contributors
- **License:** [Open Data Commons Open Database License 1.0
  (ODbL)](https://opendatacommons.org/licenses/odbl/1-0/)
- **Retrieval date:** recorded at fetch time in
  `greenwood.attribution.json` beside the generated extract

Run from the repository root:

```bash
python3 tools/fetch_osm_greenwood.py
./build/osm_drive --validate-map assets/maps/generated/greenwood.osm
./build/osm_drive assets/maps/generated/greenwood.osm
```

The fetcher uses only Python's standard library, validates that the response has
nodes and roads, writes atomically, and records the exact query, endpoint, UTC
retrieval time, and SHA-256 digest. Public Overpass instances are shared
services; avoid repeated downloads and use another Overpass instance with
`--endpoint` if the default is unavailable.

Generated extracts and their metadata are intentionally ignored by Git. OSM is
a live database, these files can be large, and each developer can reproduce an
up-to-date extract. Applications distributing a database made from this data
must preserve the required ODbL attribution and licensing notices.

## Procedural terrain foundation

The checked-in terrain is a tiny procedural grid generated in C++; it contains no
third-party imagery or raster data. Its grass/soil colors and patches are an
artistic baseline, not a representation of current conditions in Greenwood.

Future terrain importers should implement `environment::TerrainDataSource` and
sample into the map's local-meter coordinate system. Intended public sources are:

- **Elevation:** [USGS 3D Elevation Program (3DEP)](https://www.usgs.gov/3d-elevation-program), distributed through [The National Map](https://apps.nationalmap.gov/downloader/). USGS-authored data are generally public domain in the United States; preserve downloaded-product metadata and verify any product-specific notice.
- **Land cover:** [USGS Annual National Land Cover Database](https://www.usgs.gov/centers/eros/science/annual-national-land-cover-database), available through [MRLC](https://www.mrlc.gov/data). Federal source data are generally public domain; retain edition/year, resolution, and acquisition metadata.
- **Roads and optional land-use features:** [OpenStreetMap](https://www.openstreetmap.org/), licensed under the [ODbL](https://www.openstreetmap.org/copyright).

Do not commit large DEM/land-cover rasters. A later import tool should cache them
locally, record source URL, product ID, acquisition date, CRS, and license, then
emit only a compact derived mesh or tile needed by the demo. No proprietary
imagery is required by this terrain foundation.

## Prohibited/prospective imagery sources

No Google Street View imagery or geometry is used or fetched. Do not scrape or
add Google Street View assets. Mapillary or KartaView data mentioned in the
roadmap are prospective sources only; none is included in the repository at this
time. Their licenses and retrieval details must be documented before adding
derived assets.
