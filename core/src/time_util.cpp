#include "time_util.hpp"

#include <cstdlib>
#include <ctime>

namespace sdp {

std::string utcNowIso() {
  const std::time_t now = std::time(nullptr);
  std::tm parts{};
#if defined(_WIN32)
  gmtime_s(&parts, &now);
#else
  gmtime_r(&now, &parts);
#endif
  char buffer[32] = {};
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &parts);
  return std::string(buffer);
}

std::int64_t parseIsoSeconds(const std::string& text) {
  if (text.size() < 19) return -1;
  std::tm parts{};
  parts.tm_year = std::atoi(text.substr(0, 4).c_str()) - 1900;
  parts.tm_mon = std::atoi(text.substr(5, 2).c_str()) - 1;
  parts.tm_mday = std::atoi(text.substr(8, 2).c_str());
  parts.tm_hour = std::atoi(text.substr(11, 2).c_str());
  parts.tm_min = std::atoi(text.substr(14, 2).c_str());
  parts.tm_sec = std::atoi(text.substr(17, 2).c_str());
  if (parts.tm_year < 70) return -1;
#if defined(_WIN32)
  return static_cast<std::int64_t>(_mkgmtime(&parts));
#else
  return static_cast<std::int64_t>(timegm(&parts));
#endif
}

bool isWithinLastDays(const std::string& timestamp, int days) {
  if (days <= 0) return true;
  const std::int64_t when = parseIsoSeconds(timestamp);
  if (when < 0) return true;
  const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));
  return when >= now - static_cast<std::int64_t>(days) * 86400;
}

}  // namespace sdp
