"""Line counts per area, for the README and the report (dev tool only)."""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
AREAS = {
    "core (portable library)": ["core/include", "core/src"],
    "tests": ["tests"],
    "apps (headless)": ["apps"],
    "shell (graphical, optional)": ["shell"],
    "content (data files)": ["content"],
    "docs": ["docs"],
    "tools (dev scripts)": ["tools"],
}
EXTENSIONS = {".cpp", ".hpp", ".h", ".c", ".py", ".json", ".csv", ".md", ".txt", ".cmake"}

total = 0
for label, directories in AREAS.items():
    count = 0
    for directory in directories:
        base = ROOT / directory
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in EXTENSIONS:
                continue
            if "third_party" in path.parts or "figures" in path.parts:
                continue
            try:
                count += sum(1 for _ in path.open(encoding="utf-8", errors="ignore"))
            except OSError:
                pass
    total += count
    print(f"{label:32s} {count:7d}")
extra = ROOT / "CMakeLists.txt"
if extra.exists():
    print(f"{'build files (root)':32s} {sum(1 for _ in extra.open()):7d}")
print(f"{'TOTAL':32s} {total:7d}")
