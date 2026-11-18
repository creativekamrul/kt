"""Exercise the freshly built game without touching real player profiles."""
from pathlib import Path
import argparse
import json
import math
import os
import sqlite3
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[1]


def same_summary(actual, expected):
    """Allow only floating-point roundoff across laptop CPUs."""
    if isinstance(expected, dict):
        return isinstance(actual, dict) and actual.keys() == expected.keys() and all(
            same_summary(actual[key], value) for key, value in expected.items())
    if isinstance(expected, float):
        return isinstance(actual, (int, float)) and math.isclose(actual, expected, rel_tol=0, abs_tol=1e-10)
    return actual == expected


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--quick', action='store_true')
    args = parser.parse_args()
    os.chdir(ROOT)
    os.environ['MPLCONFIGDIR'] = str(ROOT / '.cache' / 'matplotlib')
    run_dir = ROOT / 'out' / 'setup-checks' / uuid.uuid4().hex[:12]
    run_dir.mkdir(parents=True)
    report = {'root': str(ROOT), 'quick': args.quick, 'checks': []}

    def run(label, command):
        print(label, flush=True)
        with (run_dir / (label + '.log')).open('w', encoding='utf-8') as log:
            subprocess.run([str(x) for x in command], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
        report['checks'].append(label)

    sim = ROOT / 'build/apps/sdp_sim.exe'
    run('sqlite', [sim, '--children', 9, '--attempts', 30, '--store', 'sqlite', '--out', run_dir / 'sqlite'])
    databases = sorted((run_dir / 'sqlite').glob('*.db'))
    if len(databases) != 5:
        raise RuntimeError('Expected five policy databases')
    for path in databases:
        with sqlite3.connect(path) as db:
            if db.execute('pragma integrity_check').fetchone()[0] != 'ok':
                raise RuntimeError(f'Corrupt database: {path}')
            if db.execute('select count(*) from attempts').fetchone()[0] != 270:
                raise RuntimeError(f'Wrong attempt count: {path}')
    run('cli', [ROOT / 'build/apps/sdp_cli.exe', '--db', run_dir / 'demo.db', '--demo', 10])
    with sqlite3.connect(run_dir / 'demo.db') as db:
        if db.execute('select count(*) from attempts').fetchone()[0] != 10:
            raise RuntimeError('Console demo did not save its session')
    if not args.quick:
        run('main-simulation', [sim, '--children', 300, '--attempts', 300, '--seed', 20260925, '--out', 'out/main'])
        for policy in ('latent', 'thompson_arms', 'ucb1', 'greedy', 'static'):
            fresh = json.loads((ROOT / f'out/main/summary_{policy}.json').read_text())
            baseline = json.loads((ROOT / f'docs/baseline/summary_{policy}.json').read_text())
            if not same_summary(fresh, baseline):
                raise RuntimeError(f'Simulation differs from the verified baseline: {policy}; inspect outputs before changing the baseline')
        report['checks'].append('all-five-baselines-match-within-1e-10')
        run('robustness', [sim, '--children', 200, '--attempts', 250, '--seed', 777001, '--learner-slope', 1.4, '--learner-guess', 0.15, '--out', 'out/steep'])
        run('charts', [sys.executable, 'tools/make_charts.py', '--main', 'out/main', '--steep', 'out/steep', '--out', 'out/figures'])
    report['status'] = 'passed'
    (run_dir / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    (ROOT / 'out/setup-result.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Verification passed:', run_dir, flush=True)


if __name__ == '__main__':
    main()
