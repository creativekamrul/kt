# ---------------------------------------------------------------------------
# Optional graphical shell (SFML 2.6). Not built by default:
#
#     python tools/build.py --shell
#
# The shell is thin on purpose: it draws, it reads input, and then it calls the
# same SessionRunner the console shell calls. All of the layout and scene logic
# it uses lives in sdp::view (part of the portable core) and is unit tested
# without a window, so this directory contains no learning logic and no geometry
# worth testing.
# ---------------------------------------------------------------------------
if(EXISTS "${PROJECT_SOURCE_DIR}/.deps/SFML/CMakeLists.txt")
  set(SFML_FOUND FALSE)
else()
  find_package(SFML 2.6 COMPONENTS graphics window system QUIET)
endif()

if(NOT SFML_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.deps/SFML/CMakeLists.txt")
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build dependencies statically" FORCE)
  set(SFML_BUILD_AUDIO OFF CACHE BOOL "Audio is not used by the existing shell" FORCE)
  set(SFML_BUILD_NETWORK OFF CACHE BOOL "KT runs offline" FORCE)
  set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "Skip SFML examples" FORCE)
  set(SFML_BUILD_DOC OFF CACHE BOOL "Skip SFML documentation" FORCE)
  # SFML's prebuilt MinGW FreeType uses a different CRT from Zig. Build the
  # font library with our compiler when its source checkout is available.
  if(EXISTS "${PROJECT_SOURCE_DIR}/.deps/freetype/CMakeLists.txt")
    foreach(feature ZLIB BZIP2 PNG HARFBUZZ BROTLI)
      set(FT_DISABLE_${feature} ON CACHE BOOL "Keep the local font build self-contained" FORCE)
    endforeach()
    add_subdirectory("${PROJECT_SOURCE_DIR}/.deps/freetype" "${CMAKE_BINARY_DIR}/freetype")
    # DLL version resources are unnecessary in the static font library and
    # would otherwise require a separate Windows resource compiler.
    if(WIN32)
      get_target_property(kt_freetype_sources freetype SOURCES)
      list(FILTER kt_freetype_sources EXCLUDE REGEX "[.]rc$")
      set_property(TARGET freetype PROPERTY SOURCES ${kt_freetype_sources})
    endif()
    set(FREETYPE_LIBRARY "${CMAKE_BINARY_DIR}/freetype/${CMAKE_STATIC_LIBRARY_PREFIX}freetype${CMAKE_STATIC_LIBRARY_SUFFIX}" CACHE FILEPATH "Locally built FreeType archive" FORCE)
    set(FREETYPE_INCLUDE_DIR_ft2build "${PROJECT_SOURCE_DIR}/.deps/freetype/include" CACHE PATH "FreeType headers" FORCE)
    set(FREETYPE_INCLUDE_DIR_freetype2 "${PROJECT_SOURCE_DIR}/.deps/freetype/include" CACHE PATH "FreeType headers" FORCE)
  endif()
  add_subdirectory("${PROJECT_SOURCE_DIR}/.deps/SFML" "${CMAKE_BINARY_DIR}/sfml")
  if(TARGET freetype)
    add_dependencies(sfml-graphics freetype)
  endif()
  set(SFML_FOUND TRUE)
  set(SFML_VERSION "2.6.2 (local source)")
endif()

if(NOT SFML_FOUND)
  message(FATAL_ERROR "SFML 2.6 is required for --shell. Install it or clone the 2.6.2 tag into .deps/SFML.")
endif()

add_executable(sdp_shell src/main_sfml.cpp src/trail_sound.cpp src/standalone.cpp)
if(WIN32)
  target_link_libraries(sdp_shell PRIVATE winmm)
  if(TARGET sfml-main)
    set_target_properties(sdp_shell PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(sdp_shell PRIVATE sfml-main)
  endif()
endif()
target_link_libraries(sdp_shell PRIVATE sdp::core)
if(SFML_INCLUDE_DIR)
  target_include_directories(sdp_shell PRIVATE ${SFML_INCLUDE_DIR})
endif()

if(TARGET sfml-graphics)
  target_link_libraries(sdp_shell PRIVATE sfml-graphics sfml-window sfml-system)
elseif(TARGET SFML::Graphics)
  target_link_libraries(sdp_shell PRIVATE SFML::Graphics SFML::Window SFML::System)
endif()

message(STATUS "SDP100: graphical shell enabled (SFML ${SFML_VERSION})")

if(SDP_BUILD_TESTS AND TARGET sfml-graphics)
  add_executable(kt_glasswind_tests ../tests/test_glasswind.cpp)
  target_include_directories(kt_glasswind_tests PRIVATE src)
  target_link_libraries(kt_glasswind_tests PRIVATE sdp::core sfml-graphics sfml-window sfml-system)
  add_test(NAME glasswind_routes_and_guardian COMMAND kt_glasswind_tests)
  set_tests_properties(glasswind_routes_and_guardian PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
  add_executable(kt_onboarding_tests ../tests/test_onboarding.cpp)
  target_include_directories(kt_onboarding_tests PRIVATE src)
  target_link_libraries(kt_onboarding_tests PRIVATE sdp::core sfml-graphics sfml-window sfml-system)
  add_test(NAME first_steps_on_the_mountain COMMAND kt_onboarding_tests)
  set_tests_properties(first_steps_on_the_mountain PROPERTIES WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endif()
