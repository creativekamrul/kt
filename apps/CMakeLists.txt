# ---------------------------------------------------------------------------
# Headless applications. Every one of these builds on sdp::core only - no
# graphics, no windowing - which is why they run on any machine, including the
# simulation server used to produce the report's evidence charts.
# ---------------------------------------------------------------------------
add_executable(sdp_content_export content_export/main.cpp)
target_link_libraries(sdp_content_export PRIVATE sdp::core)

# ---------------------------------------------------------------------------
add_executable(sdp_sim simharness/main.cpp)
target_link_libraries(sdp_sim PRIVATE sdp::core)

# ---------------------------------------------------------------------------
add_executable(sdp_cli cli/main.cpp)
target_link_libraries(sdp_cli PRIVATE sdp::core)

# The content directory is resolved at RUNTIME (SDP_CONTENT_DIR env var, then
# "content" beside the executable, then "content" in the working directory), so
# the shipped binary never carries a build-machine path.
