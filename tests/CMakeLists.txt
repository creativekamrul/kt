# ---------------------------------------------------------------------------
# Zero-dependency unit tests (no GoogleTest download needed - the project must
# build offline on any machine).
# ---------------------------------------------------------------------------
add_executable(sdp_tests
  test_framework.cpp
  main.cpp
  test_rng.cpp
  test_bandit.cpp
  test_engine.cpp
  test_generator.cpp
  test_store.cpp
  test_campaign.cpp
  test_progression.cpp
  test_content.cpp
  test_session.cpp
  test_view.cpp
)

target_link_libraries(sdp_tests PRIVATE sdp::core)
target_compile_features(sdp_tests PRIVATE cxx_std_17)

add_test(NAME sdp_tests COMMAND sdp_tests)

# The content tests load the real shipped content/ directory. It is passed as a
# RUNTIME environment variable because the resolver lives inside sdp_core, so a
# compile definition on this target would have had no effect at all.
set_tests_properties(sdp_tests PROPERTIES
  ENVIRONMENT "SDP_CONTENT_DIR=${CMAKE_SOURCE_DIR}/content")
