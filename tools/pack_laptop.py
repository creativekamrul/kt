"""Package a clean laptop kit. Run with the existing development Python."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIRECTORIES = ('.github', 'apps', 'content', 'core', 'docs', 'shell', 'tests', 'third_party', 'tools', 'support')
FILES = ('.gitignore', 'AGENTS.md', 'START_HERE.md', 'CODEX_HANDOFF.md', 'CMakeLists.txt',
         'LICENSE', 'README.md', 'THIRD_PARTY.md', 'requirements-dev.txt',
         'requirements-dev.lock', 'kt.cmd', 'setup.cmd',
         'output/proposal/KT_Project_Proposal.docx',
         'output/proposal/KT_Project_Proposal.pdf')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dest', default='out/laptop-kit/KT')
    parser.add_argument('--zip', dest='zip_path')
    args = parser.parse_args()
    dest = (ROOT / args.dest).resolve()
    if dest.exists():
        raise SystemExit(f'Refusing to overwrite existing kit: {dest}')
    dest.mkdir(parents=True)
    for name in FILES:
        (dest / name).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / name, dest / name)
    for name in DIRECTORIES:
        shutil.copytree(ROOT / name, dest / name,
                        ignore=shutil.ignore_patterns('__pycache__', '*.pyc', '.git'))
    entries = []
    for p in sorted(dest.rglob('*')):
        if p.is_file():
            entries.append({'path': p.relative_to(dest).as_posix(),
                            'sha256': hashlib.sha256(p.read_bytes()).hexdigest(), 'bytes': p.stat().st_size})
    (dest / 'KIT_CONTENTS.json').write_text(json.dumps(entries, indent=2) + '\n')
    print('Ready-to-copy folder:', dest)
    print('Files:', len(entries), 'Size MiB:', round(sum(e['bytes'] for e in entries) / 2**20, 1))
    if args.zip_path:
        archive = (ROOT / args.zip_path).resolve()
        if archive.exists():
            raise SystemExit(f'Refusing to overwrite existing ZIP: {archive}')
        with zipfile.ZipFile(archive, 'w') as z:
            for p in sorted(dest.rglob('*')):
                if p.is_file():
                    compression = zipfile.ZIP_STORED if p.suffix in ('.whl', '.zip', '.nupkg') else zipfile.ZIP_DEFLATED
                    z.write(p, 'KT/' + p.relative_to(dest).as_posix(), compress_type=compression)
        print('ZIP:', archive, 'MiB:', round(archive.stat().st_size / 2**20, 1))


if __name__ == '__main__':
    main()
