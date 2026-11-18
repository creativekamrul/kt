#pragma once
// ===========================================================================
// A deliberately tiny xUnit-style harness. Single header + one .cpp, no
// external download, works with every compiler. Keeps the offline-build
// promise of section 11 of the design document.
// ===========================================================================

#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "sdp/types.hpp"

namespace sdptest {

struct Failure {
  std::string message;
};

struct Case {
  std::string suite;
  std::string name;
  std::function<void()> fn;
};

std::vector<Case>& registry();

struct Registrar {
  Registrar(const char* suite, const char* name, std::function<void()> fn);
};

extern int g_checks;

[[noreturn]] void fail(const std::string& message, const char* file, int line);
void check(bool ok, const char* expr, const char* file, int line);
void check_near(double a, double b, double tol, const char* expr, const char* file, int line);

template <typename T>
std::string repr(const T& value) {
  std::ostringstream os;
  os << value;
  return os.str();
}
inline std::string repr(std::string_view v) { return std::string(v); }
inline std::string repr(bool v) { return v ? "true" : "false"; }
inline std::string repr(double v) {
  std::ostringstream os;
  os.precision(12);
  os << v;
  return os.str();
}
inline std::string repr(const sdp::SubSkill& s) { return s.id; }

template <typename A, typename B>
void check_eq(const A& a, const B& b, const char* expr, const char* file, int line) {
  ++g_checks;
  if (!(a == b)) fail(std::string(expr) + "  ->  " + repr(a) + " != " + repr(b), file, line);
}

template <typename A, typename B>
void check_ne(const A& a, const B& b, const char* expr, const char* file, int line) {
  ++g_checks;
  if (a == b) fail(std::string(expr) + "  ->  both sides are " + repr(a), file, line);
}

int runAll(const std::string& filter);

}  // namespace sdptest

#define SDP_TEST(suite_name, case_name)                                            static void suite_name##_##case_name();                                          static ::sdptest::Registrar sdp_reg_##suite_name##_##case_name(                       #suite_name, #case_name, suite_name##_##case_name);                          static void suite_name##_##case_name()

#define CHECK(expr) ::sdptest::check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define CHECK_EQ(a, b) ::sdptest::check_eq((a), (b), #a " == " #b, __FILE__, __LINE__)
#define CHECK_NE(a, b) ::sdptest::check_ne((a), (b), #a " != " #b, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, tol)   ::sdptest::check_near((a), (b), (tol), #a " ~= " #b, __FILE__, __LINE__)
