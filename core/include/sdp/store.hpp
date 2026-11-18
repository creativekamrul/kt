#pragma once
// ===========================================================================
// sdp/store.hpp - all persistence, behind one seam.
//
// DESIGN RULE (from the design document): no SQL exists outside the SQLite
// implementation. Everything else - engine, session, simulation, shells - talks
// to IDataStore, which is why a future cloud sync or a web build can be added
// without touching the adaptive core.
//
// Two implementations ship:
//   * SqliteStore - the real thing (offline, local, crash-safe persistence)
//   * MemoryStore - tests and the simulation harness (fast, no disk, no cleanup)
//
// NOTE ON THE SCHEMA. The five tables are implemented exactly as specified in
// the design document. That schema has no column for the difficulty tier or the
// checkpoint flag, so both travel inside attempts.spec_json - which is what
// spec_json is for ("exact LevelSpec used (replay/debug)"). Reading an attempt
// back parses it again, so a session really is replayable from the database.
// ===========================================================================

#include <iosfwd>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "sdp/types.hpp"

namespace sdp {

class StoreError : public std::runtime_error {
 public:
  explicit StoreError(const std::string& message) : std::runtime_error(message) {}
};

struct AreaPlay {
  std::string area;
  int seconds = 0;
  std::string last_played;
};

class IDataStore {
 public:
  virtual ~IDataStore() = default;

  // ---- children -----------------------------------------------------------
  virtual int createChild(const std::string& name, int age) = 0;
  virtual std::optional<ChildProfile> child(int child_id) const = 0;
  virtual std::vector<ChildProfile> children() const = 0;
  /// Removes the child and every row that references it (parent-friendly
  /// "start over"). Used by tests and the settings screen.
  virtual void deleteChild(int child_id) = 0;

  // ---- telemetry ----------------------------------------------------------
  virtual void logAttempt(const Attempt& attempt) = 0;
  /// Full log, oldest first. limit > 0 keeps only the most recent N attempts.
  virtual PerformanceHistory history(int child_id, int limit = -1) const = 0;
  virtual PerformanceHistory historyWithin(int child_id, int window_days, int limit = -1) const = 0;
  virtual int attemptCount(int child_id) const = 0;

  // ---- engine state -------------------------------------------------------
  virtual void saveSkillState(int child_id, const SkillState& state) = 0;
  virtual std::vector<SkillState> skillStates(int child_id) const = 0;

  // ---- progression --------------------------------------------------------
  virtual void saveProgression(const ProgressionState& state) = 0;
  virtual std::optional<ProgressionState> progression(int child_id) const = 0;

  virtual void awardMilestone(int child_id, std::string_view milestone) = 0;
  virtual std::vector<Milestone> milestones(int child_id) const = 0;
  virtual bool hasMilestone(int child_id, std::string_view milestone) const = 0;

  virtual void saveCamp(int child_id, const std::string& camp) = 0;
  virtual std::string savedCamp(int child_id) const = 0;

  virtual void recordPlay(int child_id, const std::string& area, int seconds) = 0;
  virtual std::vector<AreaPlay> areaPlay(int child_id) const = 0;

  // ---- analytics ----------------------------------------------------------
  /// The analytics query from the design document: success rate per sub-skill
  /// over the recent window. This is the engine-input view a teacher (or the
  /// report) would read.
  virtual std::vector<SkillSuccessRate> successRates(int child_id, int window_days = 7) const = 0;

  /// Anonymised export: no names, no ages, only a per-child participant code.
  /// One query away, which is the whole argument for keeping state in SQLite.
  virtual std::string exportAttemptsCsv(int child_id) const = 0;

  /// Wipes a child's history while keeping the profile (tests, "start over").
  virtual void resetProgress(int child_id) = 0;

  // ---- write batching -----------------------------------------------------
  // One answered puzzle writes an attempt, the progression row and a skill
  // state. Left as three auto-committed statements, SQLite fsyncs three times
  // per answer, which measured ~37 ms per answer on a slow volume - a visibly
  // laggy game. Batching them into one transaction is the fix; the memory store
  // treats these as no-ops.
  virtual void beginBatch() {}
  virtual void endBatch() {}
};

/// RAII scope for IDataStore::beginBatch/endBatch (exception safe).
class StoreBatch {
 public:
  explicit StoreBatch(IDataStore& store) : store_(&store) { store_->beginBatch(); }
  ~StoreBatch() {
    if (store_ != nullptr) store_->endBatch();
  }
  StoreBatch(const StoreBatch&) = delete;
  StoreBatch& operator=(const StoreBatch&) = delete;

 private:
  IDataStore* store_ = nullptr;
};

struct SqliteOptions {
  /// Write-ahead logging. Recommended even for single-process use: it removes
  /// the rollback-journal fsync from the commit path, which is what made the
  /// unbatched path so slow. Also enables a future read-only analytics view.
  bool wal = true;
  /// NORMAL durability: an OS crash cannot corrupt the database, only a power
  /// cut can lose the last transaction. The right trade for a children's game.
  bool synchronous_normal = true;
};

/// SQLite-backed store. Pass ":memory:" for a throwaway database.
std::unique_ptr<IDataStore> openSqliteStore(const std::string& path,
                                            const SqliteOptions& options = SqliteOptions{});

/// Non-persistent store for tests and the simulation harness.
std::unique_ptr<IDataStore> openMemoryStore();

}  // namespace sdp
