# ---------------------------------------------------------------------------
# sdp_core - the portable adaptive-learning core.
# HARD RULE: no graphics, no windowing, no audio, no network. This target is
# what the simulation harness, the tests and every shell (console, SFML, raylib,
# future web build) are built on.
# ---------------------------------------------------------------------------
add_library(sdp_core STATIC
  src/types.cpp
  src/rng.cpp
  src/bandit.cpp
  src/curriculum.cpp
  src/engine.cpp
  src/generator.cpp
  src/json_io.cpp
  src/time_util.cpp
  src/sqlite_store.cpp
  src/memory_store.cpp
  src/content_loader.cpp
  src/mountain.cpp
  src/campaign.cpp
  src/progression.cpp
  src/session.cpp
  src/view.cpp
)

add_library(sdp::core ALIAS sdp_core)

target_include_directories(sdp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# sqlite3 is the only third-party library the core links, and it is vendored
# (public domain), so no system package is required on a target machine.
target_link_libraries(sdp_core PUBLIC sdp::json PRIVATE sdp::sqlite3)

if(MSVC)
  target_compile_options(sdp_core PRIVATE /W4)
else()
  target_compile_options(sdp_core PRIVATE -Wall -Wextra)
endif()

if(SDP_WARN_AS_ERRORS)
  if(MSVC)
    target_compile_options(sdp_core PRIVATE /WX)
  else()
    target_compile_options(sdp_core PRIVATE -Werror)
  endif()
endif()
