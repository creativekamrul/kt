"""Create a playable Windows ZIP from the local Release build, without player saves."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import os
import tempfile

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

controls = """KT 1.0.0
Extract the whole KT folder to a writable location and open KT.exe.
Completed encounters and achievements save in sdp100.db beside the game.

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
with tempfile.NamedTemporaryFile(dir=dest.parent, suffix=".zip", delete=False) as tmp:
    stage = Path(tmp.name)
try:
    with ZipFile(stage, "w", ZIP_DEFLATED) as archive:
        for path, name in files:
            archive.write(path, "KT/" + name)
        archive.writestr("KT/PLAY.txt", controls)
    os.replace(stage, dest)
finally:
    stage.unlink(missing_ok=True)
print(dest)
