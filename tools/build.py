#!/usr/bin/env python3
"""One-command configure / build / test driver for the SDP 100 project.

Why this exists: the project has to build on a marker's laptop, a lab machine
and a school PC with nothing but Python installed. This script finds a usable
toolchain in this order:

  1. A native cmake plus C/C++ compiler already on PATH (MSVC, GCC or Clang).
  2. Otherwise the pip wheels 'cmake', 'ninja' and 'ziglang', where the
     Zig-bundled clang is fed to CMake through tools/zigcc.cmd and
     tools/zigcxx.cmd. No administrator rights and no Visual Studio required.

Usage:
  python tools/build.py                 # configure + build
  python tools/build.py --test          # configure + build + run unit tests
  python tools/build.py --clean         # wipe the build directory first
  python tools/build.py --shell         # also build the optional SFML shell
  python tools/build.py --debug         # Debug configuration
  python tools/build.py --target sdp_sim --test
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def user_scripts_dirs() -> list[Path]:
    """Directories where 'pip install --user' drops console scripts."""
    dirs: list[Path] = [Path(sys.executable).parent]
    try:
        import sysconfig

        for scheme in ("nt_user", "posix_user"):
            try:
                p = sysconfig.get_path("scripts", scheme=scheme)
                if p:
                    dirs.append(Path(p))
            except Exception:
                pass
    except Exception:
        pass
    dirs.append(Path(sys.executable).parent / "Scripts")
    dirs.append(Path(sys.executable).parent)
    appdata = os.environ.get("APPDATA")
    if appdata:
        dirs.append(Path(appdata) / ("Python%d%d" % (sys.version_info.major, sys.version_info.minor)) / "Scripts")
    return dirs


def find_tool(names: list[str]) -> str | None:
    for name in names:
        found = shutil.which(name)
        if found:
            return found
    for directory in user_scripts_dirs():
        for name in names:
            for suffix in (".exe", ".cmd", ".bat", ""):
                candidate = directory / (name + suffix)
                if candidate.is_file():
                    return str(candidate)
    return None


def pip_install(*packages: str) -> None:
    scope = [] if sys.prefix != sys.base_prefix else ["--user"]
    print("  pip install " + " ".join([*scope, *packages]))
    subprocess.check_call(
        [sys.executable, "-m", "pip", "install", *scope, "--disable-pip-version-check", *packages]
    )


def ensure_tool(names: list[str], wheels: list[str], label: str) -> str:
    tool = find_tool(names)
    if tool:
        return tool
    print("  %s not found - trying the pip wheel(s): %s" % (label, ", ".join(wheels)))
    pip_install(*wheels)
    tool = find_tool(names)
    if not tool:
        sys.exit("ERROR: %s is still unavailable after the install attempt" % label)
    return tool


def zig_version() -> str | None:
    try:
        probe = subprocess.run(
            [sys.executable, "-m", "ziglang", "version"], capture_output=True, text=True
        )
    except Exception:
        return None
    if probe.returncode != 0:
        return None
    return probe.stdout.strip()


def main() -> int:
    # Keep compiler wrappers and CMake on this Python environment, even when
    # invoked without activating the project's virtual environment first.
    os.environ["PATH"] = str(Path(sys.executable).parent) + os.pathsep + os.environ.get("PATH", "")
    cache = ROOT / ".cache"
    temp = cache / "tmp"
    temp.mkdir(parents=True, exist_ok=True)
    os.environ["TEMP"] = str(temp)
    os.environ["TMP"] = str(temp)
    os.environ.setdefault("ZIG_GLOBAL_CACHE_DIR", str(cache / "zig"))
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--test", action="store_true", help="run ctest after building")
    parser.add_argument("--clean", action="store_true", help="remove the build directory first")
    parser.add_argument("--shell", action="store_true", help="also build the optional SFML shell")
    parser.add_argument("--debug", action="store_true", help="Debug instead of Release")
    parser.add_argument("--target", default=None, help="build only this target")
    args = parser.parse_args()

    print("SDP100 build driver")
    cmake = ensure_tool(["cmake"], ["cmake"], "cmake")
    ninja = ensure_tool(["ninja"], ["ninja"], "ninja")
    print("  cmake : %s" % cmake)
    print("  ninja : %s" % ninja)

    extra: list[str] = ["-DCMAKE_MAKE_PROGRAM=" + ninja]

    native = find_tool(["cl", "g++", "clang++"])
    if native and not os.environ.get("SDP_FORCE_ZIG"):
        print("  using the native compiler already on PATH: %s" % native)
    else:
        print("  no native C/C++ compiler on PATH - using the Zig-bundled clang")
        if zig_version() is None:
            pip_install("ziglang==0.13.0")
        version = zig_version()
        if version is None:
            sys.exit("ERROR: could not obtain a C++ compiler (tried PATH, MSVC, and the ziglang wheel)")
        print("  zig   : %s" % version)
        extra += [
            "-DCMAKE_C_COMPILER=" + (ROOT / "tools" / "zigcc.cmd").as_posix(),
            "-DCMAKE_CXX_COMPILER=" + (ROOT / "tools" / "zigcxx.cmd").as_posix(),
            "-DCMAKE_AR=" + (ROOT / "tools" / "zigar.cmd").as_posix(),
            "-DCMAKE_RANLIB=" + (ROOT / "tools" / "zigranlib.cmd").as_posix(),
        ]

    if args.clean and BUILD.exists():
        print("  removing %s" % BUILD)
        shutil.rmtree(BUILD, ignore_errors=True)

    configure = [
        cmake,
        "-S", str(ROOT),
        "-B", str(BUILD),
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=" + ("Debug" if args.debug else "Release"),
        "-DSDP_BUILD_GAMESHELL=" + ("ON" if args.shell else "OFF"),
        *extra,
    ]
    print("  configure: " + " ".join(configure))
    if subprocess.call(configure) != 0:
        return 1

    build = [cmake, "--build", str(BUILD), "--parallel", os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL", "4")]
    if args.target:
        build += ["--target", args.target]
    print("  build: " + " ".join(build))
    if subprocess.call(build) != 0:
        return 1

    if not args.test:
        return 0

    ctest = Path(cmake).with_name("ctest.exe" if os.name == "nt" else "ctest")
    test_cmd = str(ctest) if ctest.is_file() else (find_tool(["ctest"]) or "ctest")
    command = [test_cmd, "--test-dir", str(BUILD), "--output-on-failure"]
    print("  test: " + " ".join(command))
    return subprocess.call(command)


if __name__ == "__main__":
    raise SystemExit(main())
