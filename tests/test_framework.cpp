#include "test_framework.hpp"

#include <cmath>
#include <exception>
#include <iostream>

namespace sdptest {

std::vector<Case>& registry() {
  static std::vector<Case> cases;
  return cases;
}

int g_checks = 0;

Registrar::Registrar(const char* suite, const char* name, std::function<void()> fn) {
  registry().push_back(Case{suite, name, std::move(fn)});
}

void fail(const std::string& message, const char* file, int line) {
  throw Failure{std::string(file) + ":" + std::to_string(line) + ": " + message};
}

void check(bool ok, const char* expr, const char* file, int line) {
  ++g_checks;
  if (!ok) fail(std::string("CHECK failed: ") + expr, file, line);
}

void check_near(double a, double b, double tol, const char* expr, const char* file, int line) {
  ++g_checks;
  if (!(std::fabs(a - b) <= tol)) {
    fail(std::string("CHECK_NEAR failed: ") + expr + "  ->  |" + repr(a) + " - " + repr(b) +
             "| = " + repr(std::fabs(a - b)) + " > " + repr(tol),
         file, line);
  }
}

int runAll(const std::string& filter) {
  int passed = 0;
  int failed = 0;
  int filtered = 0;
  std::string current_suite;

  for (const auto& test_case : registry()) {
    const std::string full = test_case.suite + "." + test_case.name;
    if (!filter.empty() && full.find(filter) == std::string::npos) {
      ++filtered;
      continue;
    }
    if (test_case.suite != current_suite) {
      current_suite = test_case.suite;
      std::cout << std::endl << "[" << current_suite << "]" << std::endl;
    }
    try {
      test_case.fn();
      ++passed;
      std::cout << "  pass   " << test_case.name << std::endl;
    } catch (const Failure& failure) {
      ++failed;
      std::cout << "  FAIL   " << test_case.name << std::endl
                << "         " << failure.message << std::endl;
    } catch (const std::exception& error) {
      ++failed;
      std::cout << "  FAIL   " << test_case.name << std::endl
                << "         unexpected exception: " << error.what() << std::endl;
    }
  }

  std::cout << std::endl
            << passed << " passed, " << failed << " failed, " << g_checks << " checks";
  if (filtered > 0) std::cout << " (" << filtered << " filtered out)";
  std::cout << std::endl;
  return failed == 0 ? 0 : 1;
}

}  // namespace sdptest
