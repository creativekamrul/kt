#pragma once
// Internal helpers (not part of the public API): UTC timestamps in the exact
// format SQLite's datetime('now') produces, so the same strings work in both
// the SQLite store and the in-memory store.
#include <cstdint>
#include <string>

namespace sdp {

/// "YYYY-MM-DD HH:MM:SS" in UTC.
std::string utcNowIso();

/// Seconds since the Unix epoch, or -1 when the text cannot be parsed.
std::int64_t parseIsoSeconds(const std::string& text);

/// True when the timestamp falls inside the last N days (unparseable
/// timestamps are kept rather than silently dropped).
bool isWithinLastDays(const std::string& timestamp, int days);

}  // namespace sdp
