#!/usr/bin/env python3
"""Fetch a Greenwood, Arkansas OSM extract from the Overpass API."""

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET

DEFAULT_ENDPOINT = "https://overpass.kumi.systems/api/interpreter"
# A compact area covering Greenwood and nearby rural roads.
BBOX = (35.1750, -94.3000, 35.2450, -94.2050)  # south, west, north, east


def query() -> str:
    bbox = ",".join(str(value) for value in BBOX)
    # Include context layers now so the same extract can feed future renderers.
    return f"""[out:xml][timeout:120];
(
  way[highway]({bbox});
  way[building]({bbox});
  way[landuse]({bbox});
  way[natural]({bbox});
  way[waterway]({bbox});
  way[barrier]({bbox});
  relation[building]({bbox});
  relation[landuse]({bbox});
  relation[natural]({bbox});
  relation[waterway]({bbox});
);
(._;>;);
out meta;
"""


def validate(data: bytes) -> tuple[int, int]:
    root = ET.fromstring(data)
    if root.tag != "osm":
        raise ValueError("response is not OSM XML")
    nodes = len(root.findall("node"))
    roads = sum(
        any(tag.get("k") == "highway" for tag in way.findall("tag"))
        for way in root.findall("way")
    )
    if nodes == 0 or roads == 0:
        raise ValueError("extract contains no nodes or highway ways")
    return nodes, roads


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path,
                        default=Path("assets/maps/generated/greenwood.osm"))
    parser.add_argument("--endpoint", default=DEFAULT_ENDPOINT,
                        help="Overpass interpreter URL")
    args = parser.parse_args()

    request_query = query()
    request = urllib.request.Request(
        args.endpoint,
        data=urllib.parse.urlencode({"data": request_query}).encode(),
        headers={"User-Agent": "osm-drive-map-fetch/1.0 (OpenStreetMap Drive project)"},
    )
    try:
        with urllib.request.urlopen(request, timeout=180) as response:
            data = response.read()
    except urllib.error.URLError as error:
        print(f"fetch failed: {error}", file=sys.stderr)
        return 1

    try:
        nodes, roads = validate(data)
    except (ET.ParseError, ValueError) as error:
        print(f"invalid Overpass response: {error}", file=sys.stderr)
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(dir=args.output.parent, prefix=".greenwood-", suffix=".osm")
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(data)
        os.replace(temporary, args.output)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)

    metadata = {
        "attribution": "© OpenStreetMap contributors",
        "bbox_south_west_north_east": BBOX,
        "downloaded_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "license": "Open Data Commons Open Database License (ODbL) 1.0",
        "license_url": "https://opendatacommons.org/licenses/odbl/1-0/",
        "overpass_endpoint": args.endpoint,
        "query": request_query,
        "sha256": hashlib.sha256(data).hexdigest(),
        "source": "https://www.openstreetmap.org/",
    }
    metadata_path = args.output.with_suffix(".attribution.json")
    metadata_path.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {args.output} ({nodes} nodes, {roads} highway ways)")
    print(f"wrote {metadata_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
