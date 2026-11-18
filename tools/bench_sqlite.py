"""Measures the write strategy the game uses, on whatever volume is current.

Mirrors the per-answer work of SqliteStore: one attempts INSERT, one skill_state
UPSERT and one progression UPSERT, either auto-committed or inside one transaction.

    python tools/bench_sqlite.py [answers]

Files are named uniquely and left in out/ (gitignored); this is a developer
measurement script, not part of the build.
"""
import sqlite3
import sys
import time

ANSWERS = int(sys.argv[1]) if len(sys.argv) > 1 else 200

SCHEMA = """
CREATE TABLE attempts (id INTEGER PRIMARY KEY, child_id INTEGER NOT NULL, sub_skill TEXT,
                       difficulty REAL, correct INTEGER, spec_json TEXT, created_at TEXT);
CREATE TABLE skill_state (child_id INTEGER NOT NULL, sub_skill TEXT NOT NULL,
                          difficulty REAL, alpha REAL, beta REAL, updated_at TEXT,
                          PRIMARY KEY (child_id, sub_skill));
CREATE TABLE progression (child_id INTEGER PRIMARY KEY, altitude INTEGER, zone_id INTEGER,
                          updated_at TEXT);
"""

INSERT_ATTEMPT = ("INSERT INTO attempts(child_id, sub_skill, difficulty, correct, spec_json,"
                  " created_at) VALUES(1,'math.addition',0.5,1,'{}','2026-01-01 00:00:00')")
UPSERT_SKILL = ("INSERT INTO skill_state VALUES(1,'math.addition',0.5,3.0,2.0,'x')"
                " ON CONFLICT(child_id, sub_skill) DO UPDATE SET difficulty=excluded.difficulty")
UPSERT_PROGRESS = ("INSERT INTO progression VALUES(1,1,0,'x')"
                   " ON CONFLICT(child_id) DO UPDATE SET altitude=excluded.altitude")


def run(tag, journal_mode, batched):
    connection = sqlite3.connect("out/bench_%s.db" % tag)
    connection.executescript(SCHEMA)
    if journal_mode == "wal":
        connection.execute("PRAGMA journal_mode=WAL")
        connection.execute("PRAGMA synchronous=NORMAL")
    else:
        connection.execute("PRAGMA synchronous=FULL")
    connection.commit()

    start = time.perf_counter()
    for _ in range(ANSWERS):
        if batched:
            connection.execute("BEGIN")
        connection.execute(INSERT_ATTEMPT)
        connection.execute(UPSERT_SKILL)
        connection.execute(UPSERT_PROGRESS)
        if batched:
            connection.execute("COMMIT")
        else:
            connection.commit()
    elapsed = time.perf_counter() - start
    connection.close()
    return elapsed


configs = (
    ("autocommit_journal", "journal", False),
    ("autocommit_wal", "wal", False),
    ("batched_journal", "journal", True),
    ("batched_wal", "wal", True),
)

print("answers per configuration:", ANSWERS)
print()
print("%-22s %10s %16s %8s" % ("configuration", "seconds", "ms per answer", "answers/s"))
print("-" * 62)
times = {}
for tag, journal, batched in configs:
    elapsed = run(tag, journal, batched)
    times[tag] = elapsed
    print("%-22s %10.2f %16.3f %8.0f"
          % (tag.replace("_", " "), elapsed, 1000.0 * elapsed / ANSWERS, ANSWERS / elapsed))

print()
print("autocommit journal vs batched WAL: %.0fx faster"
      % (times["autocommit_journal"] / times["batched_wal"]))
