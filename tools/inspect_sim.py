"""Quick inspection of a simulation CSV (diagnostics, not part of the build)."""
import collections
import csv
import sys

path = sys.argv[1] if len(sys.argv) > 1 else "out/sim/attempts_static.csv"
child = sys.argv[2] if len(sys.argv) > 2 else "0"

rows = [r for r in csv.DictReader(open(path)) if r["child"] == child]
print("rows for child", child, ":", len(rows))
print("first 40 tiers:", [r["tier"] for r in rows[:40]])
print("tier counts:", dict(sorted(collections.Counter(r["tier"] for r in rows).items())))
print("skill counts:", dict(collections.Counter(r["skill"] for r in rows)))
print("correct rate:", sum(1 for r in rows if r["correct"] == "1") / max(1, len(rows)))
