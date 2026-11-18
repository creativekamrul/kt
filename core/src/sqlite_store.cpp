#include <sqlite3.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "sdp/rng.hpp"  // deriveSeed: stable participant codes for CSV export
#include "sdp/store.hpp"
#include "sdp/types.hpp"
#include "time_util.hpp"

namespace sdp {
namespace {

std::string message(sqlite3* db, const std::string& context) {
  std::string text = context;
  text += ": ";
  text += (db != nullptr) ? sqlite3_errmsg(db) : "no database handle";
  return text;
}

/// RAII prepared statement: the only way SQL is executed in this file.
class Statement {
 public:
  Statement(sqlite3* db, const char* sql) : db_(db) {
    if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK) {
      throw StoreError(message(db, std::string("prepare failed: ") + sql));
    }
  }
  ~Statement() {
    if (stmt_ != nullptr) sqlite3_finalize(stmt_);
  }
  Statement(const Statement&) = delete;
  Statement& operator=(const Statement&) = delete;

  void bind(int index, int value) {
    if (sqlite3_bind_int(stmt_, index, value) != SQLITE_OK) throw StoreError(message(db_, "bind int"));
  }
  void bind(int index, double value) {
    if (sqlite3_bind_double(stmt_, index, value) != SQLITE_OK) {
      throw StoreError(message(db_, "bind double"));
    }
  }
  void bind(int index, const std::string& value) {
    if (sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK) {
      throw StoreError(message(db_, "bind text"));
    }
  }

  bool step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw StoreError(message(db_, "step failed"));
  }
  void run() { step(); }

  int columnInt(int index) const { return sqlite3_column_int(stmt_, index); }
  double columnDouble(int index) const { return sqlite3_column_double(stmt_, index); }
  bool columnIsNull(int index) const { return sqlite3_column_type(stmt_, index) == SQLITE_NULL; }
  std::string columnText(int index) const {
    if (columnIsNull(index)) return std::string();
    const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, index));
    return text != nullptr ? std::string(text) : std::string();
  }

 private:
  sqlite3* db_ = nullptr;
  sqlite3_stmt* stmt_ = nullptr;
};

/// The five tables exactly as specified in the design document. Indexes are
/// added purely for query speed; they change no semantics.
const char* const kSchema = R"SQL(
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS children (
    id         INTEGER PRIMARY KEY,
    name       TEXT NOT NULL,
    age        INTEGER,
    created_at TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS attempts (
    id           INTEGER PRIMARY KEY,
    child_id     INTEGER NOT NULL REFERENCES children(id),
    domain       TEXT NOT NULL,
    sub_skill    TEXT NOT NULL,
    difficulty   REAL NOT NULL,
    correct      INTEGER NOT NULL,
    response_ms  INTEGER,
    spec_json    TEXT,
    created_at   TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS skill_state (
    child_id   INTEGER NOT NULL REFERENCES children(id),
    sub_skill  TEXT NOT NULL,
    difficulty REAL NOT NULL,
    alpha      REAL,
    beta       REAL,
    updated_at TEXT,
    PRIMARY KEY (child_id, sub_skill)
);

CREATE TABLE IF NOT EXISTS progression (
    child_id   INTEGER PRIMARY KEY REFERENCES children(id),
    altitude   INTEGER NOT NULL DEFAULT 0,
    zone_id    INTEGER NOT NULL DEFAULT 0,
    updated_at TEXT
);

CREATE TABLE IF NOT EXISTS milestones (
    child_id   INTEGER NOT NULL REFERENCES children(id),
    milestone  TEXT NOT NULL,
    earned_at  TEXT DEFAULT (datetime('now')),
    PRIMARY KEY (child_id, milestone)
);

CREATE INDEX IF NOT EXISTS idx_attempts_child_created
    ON attempts(child_id, created_at);
)SQL";

}  // namespace

class SqliteStore final : public IDataStore {
 public:
  SqliteStore(const std::string& path, const SqliteOptions& options) : path_(path) {
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    if (sqlite3_open_v2(path.c_str(), &db_, flags, nullptr) != SQLITE_OK) {
      throw StoreError(message(db_, "cannot open database " + path));
    }
    // Per-connection pragma: without it the REFERENCES clauses are decorative.
    char* error = nullptr;
    if (sqlite3_exec(db_, kSchema, nullptr, nullptr, &error) != SQLITE_OK) {
      const std::string text = error != nullptr ? error : "unknown error";
      sqlite3_free(error);
      throw StoreError("schema initialisation failed: " + text);
    }

    // Performance pragmas (see docs/performance.md for the measurements).
    if (options.wal) {
      char* pragma_error = nullptr;
      sqlite3_exec(db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, &pragma_error);
      if (pragma_error != nullptr) sqlite3_free(pragma_error);
    }
    if (options.synchronous_normal) {
      char* pragma_error = nullptr;
      sqlite3_exec(db_, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, &pragma_error);
      if (pragma_error != nullptr) sqlite3_free(pragma_error);
    }
  }

  ~SqliteStore() override {
    if (db_ != nullptr) sqlite3_close(db_);
  }

  SqliteStore(const SqliteStore&) = delete;
  SqliteStore& operator=(const SqliteStore&) = delete;

  int createChild(const std::string& name, int age) override {
    Statement statement(db_,
                        "INSERT INTO children(name, age, created_at) VALUES(?, ?, datetime('now'))");
    statement.bind(1, name);
    statement.bind(2, age);
    statement.run();
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
  }

  std::optional<ChildProfile> child(int child_id) const override {
    Statement statement(db_, "SELECT id, name, COALESCE(age, 0), COALESCE(created_at, '') "
                             "FROM children WHERE id = ?");
    statement.bind(1, child_id);
    if (!statement.step()) return std::nullopt;
    ChildProfile profile;
    profile.id = statement.columnInt(0);
    profile.name = statement.columnText(1);
    profile.age = statement.columnInt(2);
    profile.created_at = statement.columnText(3);
    return profile;
  }

  std::vector<ChildProfile> children() const override {
    Statement statement(db_, "SELECT id, name, COALESCE(age, 0), COALESCE(created_at, '') "
                             "FROM children ORDER BY id ASC");
    std::vector<ChildProfile> result;
    while (statement.step()) {
      ChildProfile profile;
      profile.id = statement.columnInt(0);
      profile.name = statement.columnText(1);
      profile.age = statement.columnInt(2);
      profile.created_at = statement.columnText(3);
      result.push_back(std::move(profile));
    }
    return result;
  }

  void deleteChild(int child_id) override {
    executeTransaction([&] {
      removeAttempts(child_id);
      removeSkillStates(child_id);
      removeProgress(child_id);
      removeMilestones(child_id);
      Statement statement(db_, "DELETE FROM children WHERE id = ?");
      statement.bind(1, child_id);
      statement.run();
    });
  }

  void logAttempt(const Attempt& attempt) override {
    Statement statement(
        db_,
        "INSERT INTO attempts(child_id, domain, sub_skill, difficulty, correct, response_ms, "
        "spec_json, created_at) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, COALESCE(NULLIF(?, ''), datetime('now')))");
    statement.bind(1, attempt.child_id);
    statement.bind(2, std::string(to_string(attempt.domain)));
    statement.bind(3, attempt.skill.id);
    statement.bind(4, attempt.difficulty);
    statement.bind(5, attempt.correct ? 1 : 0);
    statement.bind(6, attempt.response_ms);
    statement.bind(7, attempt_spec_json(attempt));
    statement.bind(8, attempt.created_at);
    statement.run();
  }

  PerformanceHistory history(int child_id, int limit) const override {
    return loadHistory(child_id, -1, limit);
  }

  PerformanceHistory historyWithin(int child_id, int window_days, int limit) const override {
    return loadHistory(child_id, window_days, limit);
  }

  int attemptCount(int child_id) const override {
    Statement statement(db_, "SELECT COUNT(*) FROM attempts WHERE child_id = ?");
    statement.bind(1, child_id);
    if (!statement.step()) return 0;
    return statement.columnInt(0);
  }

  void saveSkillState(int child_id, const SkillState& state) override {
    Statement statement(
        db_,
        "INSERT INTO skill_state(child_id, sub_skill, difficulty, alpha, beta, updated_at) "
        "VALUES(?, ?, ?, ?, ?, datetime('now')) "
        "ON CONFLICT(child_id, sub_skill) DO UPDATE SET "
        "difficulty = excluded.difficulty, alpha = excluded.alpha, beta = excluded.beta, "
        "updated_at = excluded.updated_at");
    statement.bind(1, child_id);
    statement.bind(2, state.skill.id);
    statement.bind(3, state.difficulty);
    statement.bind(4, state.alpha);
    statement.bind(5, state.beta);
    statement.run();
  }

  std::vector<SkillState> skillStates(int child_id) const override {
    Statement statement(db_,
                        "SELECT sub_skill, difficulty, alpha, beta, COALESCE(updated_at, '') "
                        "FROM skill_state WHERE child_id = ? ORDER BY sub_skill ASC");
    statement.bind(1, child_id);
    std::vector<SkillState> result;
    while (statement.step()) {
      SkillState state;
      state.skill = SubSkill{statement.columnText(0)};
      state.difficulty = statement.columnDouble(1);
      state.alpha = statement.columnIsNull(2) ? 1.0 : statement.columnDouble(2);
      state.beta = statement.columnIsNull(3) ? 1.0 : statement.columnDouble(3);
      state.updated_at = statement.columnText(4);
      result.push_back(std::move(state));
    }
    return result;
  }

  void saveProgression(const ProgressionState& state) override {
    Statement statement(
        db_,
        "INSERT INTO progression(child_id, altitude, zone_id, updated_at) "
        "VALUES(?, ?, ?, datetime('now')) "
        "ON CONFLICT(child_id) DO UPDATE SET altitude = excluded.altitude, "
        "zone_id = excluded.zone_id, updated_at = excluded.updated_at");
    statement.bind(1, state.child_id);
    statement.bind(2, state.altitude);
    statement.bind(3, state.zone_id);
    statement.run();
  }

  std::optional<ProgressionState> progression(int child_id) const override {
    Statement statement(db_, "SELECT child_id, altitude, zone_id, COALESCE(updated_at, '') "
                             "FROM progression WHERE child_id = ?");
    statement.bind(1, child_id);
    if (!statement.step()) return std::nullopt;
    ProgressionState state;
    state.child_id = statement.columnInt(0);
    state.altitude = statement.columnInt(1);
    state.zone_id = statement.columnInt(2);
    state.updated_at = statement.columnText(3);
    return state;
  }

  void awardMilestone(int child_id, std::string_view milestone) override {
    Statement statement(db_,
                        "INSERT OR IGNORE INTO milestones(child_id, milestone, earned_at) "
                        "VALUES(?, ?, datetime('now'))");
    statement.bind(1, child_id);
    statement.bind(2, std::string(milestone));
    statement.run();
  }

  std::vector<Milestone> milestones(int child_id) const override {
    Statement statement(db_,
                        "SELECT milestone, COALESCE(earned_at, '') FROM milestones "
                        "WHERE child_id = ? ORDER BY earned_at ASC, milestone ASC");
    statement.bind(1, child_id);
    std::vector<Milestone> result;
    while (statement.step()) {
      Milestone milestone;
      milestone.name = statement.columnText(0);
      milestone.earned_at = statement.columnText(1);
      result.push_back(std::move(milestone));
    }
    return result;
  }

  bool hasMilestone(int child_id, std::string_view milestone) const override {
    Statement statement(db_,
                        "SELECT 1 FROM milestones WHERE child_id = ? AND milestone = ? LIMIT 1");
    statement.bind(1, child_id);
    statement.bind(2, std::string(milestone));
    return statement.step();
  }

  std::vector<SkillSuccessRate> successRates(int child_id, int window_days) const override {
    // The design document's analytics query, verbatim in shape:
    //   SELECT sub_skill, AVG(correct), COUNT(*) FROM attempts
    //   WHERE child_id = ? AND created_at > datetime('now', '-7 days')
    //   GROUP BY sub_skill;
    Statement statement(
        db_,
        "SELECT sub_skill, AVG(correct) AS success_rate, COUNT(*) AS n FROM attempts "
        "WHERE child_id = ? AND created_at > datetime('now', '-' || ? || ' days') "
        "GROUP BY sub_skill ORDER BY sub_skill ASC");
    statement.bind(1, child_id);
    statement.bind(2, window_days);
    std::vector<SkillSuccessRate> result;
    while (statement.step()) {
      SkillSuccessRate rate;
      rate.skill = SubSkill{statement.columnText(0)};
      rate.success_rate = statement.columnDouble(1);
      rate.n = statement.columnInt(2);
      result.push_back(std::move(rate));
    }
    return result;
  }

  std::string exportAttemptsCsv(int child_id) const override {
    Statement statement(
        db_,
        "SELECT sub_skill, domain, difficulty, correct, COALESCE(response_ms, 0), "
        "COALESCE(created_at, '') FROM attempts WHERE child_id = ? ORDER BY created_at ASC, id ASC");
    statement.bind(1, child_id);

    std::ostringstream out;
    const std::string code = "P" + std::to_string(deriveSeed({static_cast<std::uint64_t>(child_id)}) % 100000u);
    out << "participant,sub_skill,domain,difficulty,correct,response_ms,created_at" << (char)10;
    while (statement.step()) {
      out << code << "," << statement.columnText(0) << "," << statement.columnText(1) << ","
          << statement.columnDouble(2) << "," << statement.columnInt(3) << ","
          << statement.columnInt(4) << "," << statement.columnText(5) << (char)10;
    }
    return out.str();
  }

  void resetProgress(int child_id) override {
    executeTransaction([&] {
      removeAttempts(child_id);
      removeSkillStates(child_id);
      removeProgress(child_id);
      removeMilestones(child_id);
    });
  }

  void beginBatch() override {
    if (batch_depth_++ > 0) return;  // already inside a batch
    sqlite3_exec(db_, "BEGIN", nullptr, nullptr, nullptr);
  }

  void endBatch() override {
    if (batch_depth_ == 0) return;
    if (--batch_depth_ > 0) return;
    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
  }

 private:
  template <typename Fn>
  void executeTransaction(Fn&& body) {
    char* error = nullptr;
    if (sqlite3_exec(db_, "BEGIN", nullptr, nullptr, &error) != SQLITE_OK) {
      const std::string text = error != nullptr ? error : "unknown error";
      sqlite3_free(error);
      throw StoreError("BEGIN failed: " + text);
    }
    try {
      body();
    } catch (...) {
      sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
      throw;
    }
    if (sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &error) != SQLITE_OK) {
      const std::string text = error != nullptr ? error : "unknown error";
      sqlite3_free(error);
      throw StoreError("COMMIT failed: " + text);
    }
  }

  void removeAttempts(int child_id) {
    Statement statement(db_, "DELETE FROM attempts WHERE child_id = ?");
    statement.bind(1, child_id);
    statement.run();
  }
  void removeSkillStates(int child_id) {
    Statement statement(db_, "DELETE FROM skill_state WHERE child_id = ?");
    statement.bind(1, child_id);
    statement.run();
  }
  void removeProgress(int child_id) {
    Statement statement(db_, "DELETE FROM progression WHERE child_id = ?");
    statement.bind(1, child_id);
    statement.run();
  }
  void removeMilestones(int child_id) {
    Statement statement(db_, "DELETE FROM milestones WHERE child_id = ?");
    statement.bind(1, child_id);
    statement.run();
  }

  PerformanceHistory loadHistory(int child_id, int window_days, int limit) const {
    const bool use_limit = limit > 0;
    std::string sql =
        "SELECT child_id, domain, sub_skill, difficulty, correct, COALESCE(response_ms, 0), "
        "COALESCE(spec_json, ''), COALESCE(created_at, '') FROM attempts WHERE child_id = ?";
    if (window_days > 0) sql += " AND created_at > datetime('now', '-' || ? || ' days')";
    // Newest-first when limiting, then reversed in C++ so callers always get
    // the log in chronological order.
    sql += use_limit ? " ORDER BY id DESC LIMIT ?" : " ORDER BY id ASC";

    Statement statement(db_, sql.c_str());
    int index = 1;
    statement.bind(index++, child_id);
    if (window_days > 0) statement.bind(index++, window_days);
    if (use_limit) statement.bind(index++, limit);

    std::vector<Attempt> rows;
    while (statement.step()) {
      Attempt attempt;
      attempt.child_id = statement.columnInt(0);
      attempt.domain = domain_from_string(statement.columnText(1)).value_or(Domain::Math);
      attempt.skill = SubSkill{statement.columnText(2)};
      attempt.difficulty = statement.columnDouble(3);
      attempt.correct = statement.columnInt(4) != 0;
      attempt.response_ms = statement.columnInt(5);
      attempt.spec_json = statement.columnText(6);
      attempt.created_at = statement.columnText(7);

      // Recover the tier and the checkpoint flag, which live in spec_json
      // because the schema has no columns for them.
      if (const auto spec = level_spec_from_json(attempt.spec_json)) {
        attempt.tier = spec->diff.tier;
        attempt.checkpoint = spec->kind == LevelKind::Checkpoint;
      }
      rows.push_back(std::move(attempt));
    }

    if (use_limit) std::reverse(rows.begin(), rows.end());
    PerformanceHistory history(child_id);
    for (auto& row : rows) history.add(std::move(row));
    return history;
  }

  sqlite3* db_ = nullptr;
  std::string path_;
  int batch_depth_ = 0;
};

std::unique_ptr<IDataStore> openSqliteStore(const std::string& path,
                                            const SqliteOptions& options) {
  return std::unique_ptr<IDataStore>(new SqliteStore(path, options));
}

}  // namespace sdp
