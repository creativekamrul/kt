# Performance notes

All measurements are from the development machine, whose working volume has
unusually slow fsync behaviour (a virtual/network volume). The **absolute** numbers
will be much better on an SSD; the **ordering** and the mechanism are the point.

## The problem: three commits per answered puzzle

One answered puzzle writes:

1. the attempt (telemetry, the source of truth),
2. the progression row (altitude, zone),
3. the skill state (bandit belief).

Written as three auto-committed statements, that is three fsyncs per answer. Measured
end to end in the game: an SQLite-backed simulation run had not completed a single arm
of 90,000 answers after about five minutes - tens of milliseconds per answer, i.e. a
visibly laggy game.

## The fix (two changes, both structural)

1. **One transaction per answer.** IDataStore gained beginBatch/endBatch, and
   SessionRunner wraps the three writes of an answer in a StoreBatch RAII scope. The
   in-memory store treats them as no-ops.
2. **WAL mode plus synchronous = NORMAL.** The rollback journal's create/delete/fsync per
   commit is what dominates on this volume. NORMAL durability means an OS crash cannot
   corrupt the database but a power cut can lose the last transaction - the right trade
   for a children's game, and documented in SqliteOptions.

Note on the design document: it advises WAL "only if multi-process access is ever
needed". The measurement says otherwise on this hardware, so WAL is on by default and
can be turned off via SqliteOptions. The reasoning is recorded here rather than assumed.

## Mechanism measurement

tools/bench_sqlite.py reproduces the per-answer write pattern (one INSERT, two UPSERTs)
in four configurations. 200 answers each:

| Configuration | ms per answer | answers/s |
|---|---|---|
| autocommit, rollback journal | 195.2 | 5 |
| autocommit, WAL | 0.74 | 1357 |
| one transaction, rollback journal | 199.9 | 5 |
| one transaction, WAL | 0.22 | 4491 |

On this volume the journal mode is decisive (roughly 900x end to end for the batched
comparison), and batching then adds a further factor of about three on top of WAL. On an
SSD the absolute costs shrink dramatically; the lesson that transfers is that a
per-answer commit path must be batched.

The script measures a Python re-implementation of the same write pattern, so it
characterises the *mechanism*. The product-level numbers are below.

## Product-level numbers

36,000 answers (60 children x 120 answers x 5 policies) including engine inference,
level generation and persistence:

| Store | Wall time | ms per answer |
|---|---|---|
| SQLite (batched, WAL) | 76.8 s | 2.13 |
| in-memory | 2.4 s | 0.065 |

The in-memory store is used by the unit tests and by the bulk simulation because it is
about 30x faster; the SQLite path is used by the game and by a dedicated simulation run
that proves the two produce identical output.

## Storage parity

The same simulation run against both stores produced **byte-identical** CSVs
(verified with a file comparison), which is the strongest available evidence that the
storage abstraction is sound and that the in-memory store used for the bulk of the
evidence is a faithful stand-in.

## Where the remaining time goes, and what is next

For the game itself the relevant figure is 2.1 ms per answer against a human response
time of seconds - persistence is not remotely the bottleneck. If a future build needs
more (for example a class of 30 children writing to one file), the obvious steps are:

* keep the SQLite connection open for the process lifetime (already the case) and cache
  the prepared statements (currently prepared per call);
* keep the write batch open across a whole session and commit at the session boundary,
  accepting the loss of at most one session on a power cut;
* move analytics and export to a separate read-only connection, which WAL makes easy.
