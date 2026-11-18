"""Build source and Windows release archives from the project root."""
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory
from zipfile import ZipFile, ZIP_DEFLATED

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "out"
VERSION = re.search(r"\bVERSION (\d+\.\d+\.\d+)", (ROOT/"CMakeLists.txt").read_text()).group(1)
PREFIX = f"KT-{VERSION}"
SOURCE_DIRS = (".github", "apps", "core", "content", "docs", "shell", "tests", "third_party", "tools")
ROOT_FILES = (".gitignore", "README.md", "CMakeLists.txt", "LICENSE", "THIRD_PARTY.md",
              "kt.cmd", "setup.cmd", "requirements-dev.txt", "requirements-dev.lock")

def source_files():
    for name in ROOT_FILES:
        yield ROOT / name
    for directory in SOURCE_DIRS:
        for p in sorted((ROOT/directory).rglob("*")):
            if not p.is_file() or "__pycache__" in p.parts or ".git" in p.parts:
                continue
            if p.suffix.lower() in {".pyc", ".db", ".exe", ".zip", ".dll", ".log"}:
                continue
            yield p

def main():
    subprocess.run([sys.executable, str(ROOT/"tools/pack_game.py")], check=True)
    binary = OUT/f"{PREFIX}-Windows-x64.zip"
    source = OUT/f"{PREFIX}-Source.zip"
    with TemporaryDirectory(dir=OUT) as stage:
        stage=Path(stage)
        shutil.copyfile(OUT/"KT-Windows.zip", stage/binary.name)
        with ZipFile(stage/source.name, "w", ZIP_DEFLATED) as archive:
            for p in source_files():
                archive.write(p, f"{PREFIX}/"+p.relative_to(ROOT).as_posix())
        checks=[f"{hashlib.sha256((OUT/'KT.exe').read_bytes()).hexdigest()}  KT.exe\n"]
        for artifact in (binary,source):
            item=stage/artifact.name
            with ZipFile(item) as archive:
                if archive.testzip(): raise RuntimeError(f"Archive failed verification: {artifact}")
            checks.append(f"{hashlib.sha256(item.read_bytes()).hexdigest()}  {artifact.name}\n")
            item.replace(artifact)
        digest=OUT/f"{PREFIX}-SHA256.txt"
        (stage/digest.name).write_text("".join(checks),encoding="ascii")
        (stage/digest.name).replace(digest)
        for item in (binary,source,digest): print(item)

if __name__ == "__main__":
    main()
