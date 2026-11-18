"""Vendor the SQLite amalgamation into third_party/sqlite/.

Keeps the build dependency-free: sqlite3.c/sqlite3.h are compiled straight into
the project, so no system libsqlite3-dev is required on any target machine.
Run:  python tools/fetch_sqlite.py
"""
from __future__ import annotations

import io
import re
import sys
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "third_party" / "sqlite"
DOWNLOAD_PAGE = "https://www.sqlite.org/download.html"


def fetch(url: str) -> bytes:
    print(f"  GET {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "sdp100-vendor-script"})
    with urllib.request.urlopen(req, timeout=120) as resp:
        return resp.read()


def main() -> int:
    print("Locating current SQLite amalgamation ...")
    page = fetch(DOWNLOAD_PAGE).decode("utf-8", "replace")
    matches = re.findall(r"(\d{4}/sqlite-amalgamation-(\d+)\.zip)", page)
    if not matches:
        print("ERROR: could not find an amalgamation zip on the download page", file=sys.stderr)
        return 1
    rel, version = matches[0]
    year = rel.split("/")[0]
    url = f"https://www.sqlite.org/{year}/{Path(rel).name}"
    print(f"  version {version}")

    blob = fetch(url)
    DEST.mkdir(parents=True, exist_ok=True)
    extracted = []
    with zipfile.ZipFile(io.BytesIO(blob)) as zf:
        for name in ("sqlite3.c", "sqlite3.h", "sqlite3ext.h"):
            member = next((n for n in zf.namelist() if n.endswith("/" + name)), None)
            if member is None:
                print(f"WARNING: {name} missing from archive", file=sys.stderr)
                continue
            (DEST / name).write_bytes(zf.read(member))
            extracted.append(name)

    (DEST / "VERSION.txt").write_text(
        f"SQLite amalgamation {version}\n"
        f"Source: {url}\n"
        "Public domain (https://www.sqlite.org/copyright.html)\n",
        encoding="utf-8",
    )
    print(f"Vendored into {DEST}: {', '.join(extracted)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
