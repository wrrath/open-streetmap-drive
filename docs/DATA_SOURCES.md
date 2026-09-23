# Environment data sources

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
