# ---------------------------------------------------------------------------
# Vendored dependencies (see THIRD_PARTY.md for licences).
# SQLite is compiled from the public-domain amalgamation so that no system
# package is ever required on a target machine (schools / offline laptops).
# ---------------------------------------------------------------------------
add_library(sqlite3 STATIC sqlite/sqlite3.c)
add_library(sdp::sqlite3 ALIAS sqlite3)

target_include_directories(sqlite3 SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sqlite)

target_compile_definitions(sqlite3 PRIVATE
  SQLITE_THREADSAFE=1
  SQLITE_DQS=0
  SQLITE_OMIT_LOAD_EXTENSION
  SQLITE_DEFAULT_FOREIGN_KEYS=1
  SQLITE_USE_URI=0
)

if(NOT MSVC)
  target_compile_options(sqlite3 PRIVATE -w)
endif()

# nlohmann/json is header-only and MIT licensed; MIT is publication-safe.
add_library(nlohmann_json INTERFACE)
add_library(sdp::json ALIAS nlohmann_json)
target_include_directories(nlohmann_json SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/json)
