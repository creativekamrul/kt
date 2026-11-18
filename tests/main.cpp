#include "test_framework.hpp"

#include <string>

int main(int argc, char** argv) {
  const std::string filter = argc > 1 ? argv[1] : std::string{};
  return sdptest::runAll(filter);
}
