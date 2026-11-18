# Laptop kit verification

Verified on 24 October 2026 on the original Windows computer, using a separate
clean copy at `K:\kt\out\relocation test\KT`. This tests relocation and bootstrap;
the owner's physical laptop was not available for testing.

The clean copy initially had no `.tools`, `.venv`, `.deps`, `.cache`, or `build`.
Its folder name included spaces. Setup ran through Windows PowerShell 5.1.

Results:

- All 18 bundled dependency archives/wheels passed SHA-256 checks.
- Official Python 3.13.12 was unpacked locally, without a system installer.
- All 15 pinned Python/build packages installed from local wheels with `--no-index`.
- SFML and FreeType sources were unpacked from the kit; no Git command was needed.
- All C++ targets compiled using the newly installed local compiler.
- 72 tests passed, 0 failed, 49,671 checks.
- All five SQLite policy databases contained 270 attempts and passed integrity checks.
- The console demo saved 10 attempts in a separate test database.
- The full 450,000-attempt main comparison matched all five saved baseline summaries
  (comparison permits only floating-point roundoff of 1e-10).
- The 250,000-attempt robustness comparison completed and all five charts were generated.
- The relocated GUI launched, reported a responding window, and closed with exit code 0.
- Repeating setup with `-ToolsOnly` reused the installed environment without backups
  or reinstalls.

The source copies and bundled dependencies are the same as the final kit; later
documentation and progress-message updates do not change the build or simulation.
The final ZIP is checked for archive integrity and its contents are checked against
KIT_CONTENTS.json before delivery. No personal game database is included.
