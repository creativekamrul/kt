# Third-party components

The project is intended for non-commercial deployment by a public program, and it must
be safe to publish and redistribute. Every dependency is either public domain or a
permissive licence, and every one of them is either vendored into this
repository or optional.

| Component | Version | Licence | How it is used | Vendored? |
|---|---|---|---|---|
| SQLite | 3.53.4 amalgamation | Public domain | all local persistence | yes - third_party/sqlite |
| nlohmann/json | 3.11.3 | MIT | content files, spec_json | yes - third_party/json |
| SFML | 2.6.2 (optional) | zlib | optional graphical shell | local source in .deps/SFML, or system install |
| FreeType | 2.13.3 (optional) | FreeType License (FTL), selected from dual FTL/GPL terms | font rendering through SFML | local source in .deps/freetype; see docs/FTL.TXT in that checkout |

## Why vendoring

third_party/sqlite contains the public-domain SQLite amalgamation (sqlite3.c, sqlite3.h,
sqlite3ext.h) plus a VERSION.txt recording the exact release and download URL.
tools/fetch_sqlite.py re-vendors it. Vendoring means:

* a school PC needs no packages, no developer libraries and no internet to build or run;
* the exact SQLite version in a published build is pinned and auditable;
* the build is reproducible on a machine with nothing installed but Python.

## Notes

* CPython 3.13 with cmake, ninja, matplotlib and ziglang wheels are used by the
  **development scripts** only (tools/). None of them is required to build or run the
  game itself: a user with a normal C++ toolchain can run CMake directly.
* The Zig-bundled clang is used as a fallback compiler on machines that have no
  installed C++ compiler. It is a build-time tool and contributes no code to the
  shipped binary.
* No dependency has a copyleft or network-activation clause; the deployment story
  (offline, local data, no accounts) is consistent with all of them.

## Dependency hygiene

* The core library links only sqlite3, and only privately: sqlite3.h never appears in a
  public header (sdp/store.hpp exposes a pimpl-style interface), so a future web or
  mobile shell would not inherit the dependency.
* SQLite is compiled with SQLITE_THREADSAFE=1, SQLITE_DQS=0,
  SQLITE_OMIT_LOAD_EXTENSION and SQLITE_DEFAULT_FOREIGN_KEYS=1.

KT artwork and audio in `shell/assets` are original project assets. The source for their pixel illustrations, sprite animation and score is in `tools/draw_trail_art.py`, `tools/draw_bay.py`, `tools/draw_people.py`, `tools/draw_bracken.py` and `tools/draw_glasswind.py`.
