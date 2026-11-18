"""KT's local Windows launcher; no environment activation required."""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
os.environ.setdefault("SDP_FORCE_ZIG", "1")
os.environ["PATH"] = str(Path(sys.executable).parent) + os.pathsep + os.environ.get("PATH", "")
os.environ["MPLCONFIGDIR"] = str(ROOT / ".cache" / "matplotlib")
temp = ROOT / ".cache" / "tmp"
temp.mkdir(parents=True, exist_ok=True)
os.environ["TEMP"] = os.environ["TMP"] = str(temp)
os.environ["ZIG_GLOBAL_CACHE_DIR"] = str(ROOT / ".cache" / "zig")

commands = {
    "build": [sys.executable, "tools/build.py", "--shell", "--test"],
    "test": [str(Path(sys.executable).parent / "ctest.exe"), "--test-dir", "build", "--verbose"],
    "package": [sys.executable, "tools/release.py"],
    "play": [str(ROOT / "build/shell/sdp_shell.exe")],
    "cli": [str(ROOT / "build/apps/sdp_cli.exe"), "--db", "out/kt.db"],
    "sim": [str(ROOT / "build/apps/sdp_sim.exe")],
    "charts": [sys.executable, "tools/make_charts.py", "--main", "out/main", "--steep", "out/steep", "--out", "out/figures"],
}
action = sys.argv[1] if len(sys.argv) > 1 else "play"
if action not in commands:
    print("Usage: kt.cmd {build|test|play|cli|sim|charts|package} [extra arguments]")
    raise SystemExit(0 if action == "help" else 2)
(ROOT / "out").mkdir(exist_ok=True)
extra = sys.argv[2:]
if action == "sim":
    for flag, value in {"--children": "300", "--attempts": "300", "--seed": "20260925", "--out": "out/main"}.items():
        if flag not in extra:
            extra += [flag, value]
try:
    raise SystemExit(subprocess.call(commands[action] + extra))
except FileNotFoundError:
    print("Executable missing. Run kt.cmd build first.", file=sys.stderr)
    raise SystemExit(1)
