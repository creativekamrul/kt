# Bundled Windows x64 development tools

`downloads/python.3.13.12.nupkg` is the official Python build-tools distribution:
https://api.nuget.org/v3-flatcontainer/python/3.13.12/python.3.13.12.nupkg
Python documents this package at https://docs.python.org/3.13/using/windows.html#the-nuget-org-packages

SFML-source.zip and freetype-source.zip contain the original source checkouts
listed in CODEX_HANDOFF.md, including their licenses, with Git metadata omitted.
`wheels/` contains the Windows x64 / CPython 3.13 packages resolved from PyPI
using requirements-dev.lock. Wheels retain their packaged license metadata.
`manifest.json` records SHA-256 checksums for transfer integrity.

These are development tools. The game itself stays C++ and uses local SQLite.
Setup extracts/install tools inside the project; it makes no network calls and
does not require a system Python, Git, Visual Studio, NuGet CLI or API key.

To refresh bundled dependencies intentionally, update requirements-dev.lock,
download wheels matching Python 3.13 on Windows x64, regenerate the source ZIPs
and manifest, then repeat the clean-copy setup test before distributing a kit.
Do not replace only the lockfile while leaving old bundled wheels in place.
