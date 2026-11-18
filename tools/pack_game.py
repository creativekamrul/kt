"""Create a playable Windows ZIP from the local Release build, without player saves."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import os
import tempfile
from embed_game import embed_game

root = Path(__file__).resolve().parents[1]
dest = root / "out" / "KT-Windows.zip"
exe = root / "build/shell/sdp_shell.exe"
if not exe.is_file():
    raise SystemExit("Run kt.cmd build first.")

files = [(exe, "KT.exe"), (root / "LICENSE", "LICENSE.txt")]
for directory in ("content", "shell/assets"):
    for path in sorted((root / directory).rglob("*")):
        if path.is_file() and path.suffix in {".json", ".csv", ".png", ".wav"}:
            files.append((path, path.relative_to(root).as_posix()))
zig = root / ".venv/Lib/site-packages/ziglang/lib"
for src, name in [
    (root / ".deps/SFML/license.md", "SFML.md"),
    (root / ".deps/freetype/docs/FTL.TXT", "FreeType.txt"),
    (root / "third_party/json/LICENSE.MIT", "JSON.txt"),
    (zig / "libc/mingw/COPYING", "MinGW.txt"),
    (zig / "libcxx/LICENSE.TXT", "libcxx.txt"),
    (zig / "libcxxabi/LICENSE.TXT", "libcxxabi.txt"),
    (zig / "libunwind/LICENSE.TXT", "libunwind.txt"),
]:
    files.append((src, "licenses/" + name))

controls = """KT 1.2.0
Double-click KT.exe to play. Saves are in %LOCALAPPDATA%/KT/sdp100.db.
An existing sdp100.db beside KT.exe takes priority for older portable saves.

Select a climber or create a nickname at the lodge. Open Parent report for that
climber to review focused play time, trail answers and recent progress. Return
through Pause > Climbers & parent report.

Press F1 for the field guide, or open it from Pause. Choose Save game in Pause
to save at your current camp. Continue under your nickname to return.
At altitudes 12 and 36 meet the Ember Fox and Bellback Tortoise. Dodge the
marked lane and ring a bell for the fox. Parry on the big star for Bellback.

Walk west from Willow Bay plaza to repair Willow Bridge. Cross to the west
bank and follow the gold marker to Bracken Rise. Choose the fern switchbacks
for an extra heart or the cairn steps for a longer parry opening. Against the
copperhorn, dodge with Space/B, parry with Q/X and strike with E/A. Take the
gold pennants to Glasswind Hollow. Mirror Pools offer a ward and Bell Gallery
offers a longer counterattack. Reflect the moonkite's gold feather, leave its
shadows and strike when it lands. Cleared guardians stay cleared. The north
pass in Willow Bay takes you to your latest unlocked area.
On the upper trail, cross the creek at a
wooden bridge. Follow the blazes, carry supplies, and climb the rope
to the next ledge. Reach altitude 90, walk the summit ridge and raise the flag with E/A.

Keyboard: WASD or arrows to move, E or Space to interact, J for the
journal, Esc to pause or return, F11 to toggle full screen.
Xbox-layout controller: left stick or D-pad to move and navigate menus,
A to interact or select, B to go back, Y for the journal, Start to pause.
Follow the gold marks to Willow Bridge or your current errand. Speak to Nia
for creek bells, Sol for the trail pennant, and Kit for wind moth practice.
Press E or A to talk or parry. Esc or B leaves practice at any time.
Willow Bridge: click a plank and then a gap, drag it, or select both with A.

KT includes an original pixel-art landscape, climber and musical score.
The game uses SFML, FreeType, SQLite, and nlohmann/json. FreeType is
copyright The FreeType Project. See licenses for third-party notices.
"""
dest.parent.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(dir=dest.parent) as folder:
    package = Path(folder) / "KT.exe"
    resources = {name: path.read_bytes() for path, name in files if path != exe}
    resources["PLAY.txt"] = controls.encode("utf-8")
    embed_game(exe, package, resources)
    stage = Path(folder) / dest.name
    with ZipFile(stage, "w", ZIP_DEFLATED) as archive:
        archive.write(package, "KT/KT.exe")
    os.replace(package, dest.parent / "KT.exe")
    os.replace(stage, dest)
print(dest.parent / "KT.exe")
print(dest)
