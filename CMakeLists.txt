cmake_minimum_required(VERSION 3.20)
project(sdp100_adaptive_game
        VERSION 1.2.0
        DESCRIPTION "Adaptive learning puzzle game - portable core + shells"
        LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Toolchain-agnostic build. Works with MSVC, GCC, Clang and (for machines with
# no compiler installed) the Zig-bundled clang via tools/build.py.
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SDP_BUILD_TESTS     "Build the unit test suite"                    ON)
option(SDP_BUILD_APPS      "Build console client + simulation harness"    ON)
option(SDP_BUILD_GAMESHELL "Build the SFML graphical shell (needs SFML)"  OFF)
option(SDP_WARN_AS_ERRORS  "Promote compiler warnings to errors"          OFF)

# Unused-parameter noise from the vendored SQLite amalgamation is silenced
# per-target in third_party/CMakeLists.txt, not globally.
add_subdirectory(third_party)
add_subdirectory(core)

if(SDP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(SDP_BUILD_APPS)
  add_subdirectory(apps)
endif()

if(SDP_BUILD_GAMESHELL)
  add_subdirectory(shell)
endif()

message(STATUS "SDP100: C++ standard = ${CMAKE_CXX_STANDARD} (${CMAKE_CXX_COMPILER_ID})")
message(STATUS "SDP100: tests=${SDP_BUILD_TESTS} apps=${SDP_BUILD_APPS} gameshell=${SDP_BUILD_GAMESHELL}")
