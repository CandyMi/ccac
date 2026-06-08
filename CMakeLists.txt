cmake_minimum_required(VERSION 3.14)
project(ccac
  VERSION 1.0.0
  DESCRIPTION "Aho-Corasick multi-pattern matching automaton (header-only C library)"
  LANGUAGES C
)

# ── compile features ────────────────────────────────────────────────────
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── out-of-tree build only ──────────────────────────────────────────────
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-tree builds are forbidden.  Run:\n"
    "  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release\n"
    "  cmake --build build")
endif()

# ── default to Release ──────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING
    "Build type: Debug, Release, RelWithDebInfo, MinSizeRel" FORCE)
endif()

# ── compiler flags ──────────────────────────────────────────────────────
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  add_compile_options(-Wall -Wextra)
endif()

# ── git submodule: init if needed ───────────────────────────────────────
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/3rd/ccalg/include/cchashmap.h")
  message(FATAL_ERROR "Submodule '3rd/ccalg' is missing.\n"
    "  Run:  git submodule update --init --recursive")
endif()

# ── include paths ───────────────────────────────────────────────────────
include_directories(
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/3rd/ccalg/include
)

# ── enable CTest ────────────────────────────────────────────────────────
enable_testing()

# ════════════════════════════════════════════════════════════════════════
#  Test targets
# ════════════════════════════════════════════════════════════════════════

# ── unicode codec test ──────────────────────────────────────────────────
add_executable(test_unicode tests/test_unicode.c)
add_test(NAME unicode  COMMAND test_unicode)
set_tests_properties(unicode PROPERTIES LABELS "unit")

# ── unit test ───────────────────────────────────────────────────────────
add_executable(test_ccac tests/test_ccac.c)
add_test(NAME unit  COMMAND test_ccac)
set_tests_properties(unit PROPERTIES LABELS "unit")

# ── robustness test ─────────────────────────────────────────────────────
add_executable(test_ccac_robust tests/test_ccac_robust.c)
add_test(NAME robust  COMMAND test_ccac_robust)
set_tests_properties(robust PROPERTIES LABELS "robust")

# ════════════════════════════════════════════════════════════════════════
#  Stress test — requires generated data files
# ════════════════════════════════════════════════════════════════════════

add_executable(test_ccac_stress tests/test_ccac_stress.c)

# generate dictionary + stress text (one-shot)
add_custom_command(
  OUTPUT  ${CMAKE_BINARY_DIR}/words.zh.txt
          ${CMAKE_BINARY_DIR}/stress_text.txt
          ${CMAKE_BINARY_DIR}/stress_text.txt.pos
  COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/gen_words.py
  COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/gen_stress_text.py
          ${CMAKE_BINARY_DIR}/words.zh.txt
          ${CMAKE_BINARY_DIR}/stress_text.txt 3
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Generating stress-test data (50k Chinese words, ~3 MB text)"
)

# custom target to trigger generation
add_custom_target(gen_stress_data DEPENDS
  ${CMAKE_BINARY_DIR}/words.zh.txt
  ${CMAKE_BINARY_DIR}/stress_text.txt
  ${CMAKE_BINARY_DIR}/stress_text.txt.pos)

# stress test depends on data generation
add_test(NAME stress
  COMMAND test_ccac_stress
    ${CMAKE_BINARY_DIR}/words.zh.txt
    ${CMAKE_BINARY_DIR}/stress_text.txt
    ${CMAKE_BINARY_DIR}/stress_text.txt.pos)
set_tests_properties(stress PROPERTIES LABELS "stress")
add_dependencies(test_ccac_stress gen_stress_data)

# ── find Python3 (for data generation) ──────────────────────────────────
find_package(Python3 QUIET)

# ════════════════════════════════════════════════════════════════════════
#  Benchmark (optional — needs PCRE2)
# ════════════════════════════════════════════════════════════════════════

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(PCRE2 QUIET libpcre2-8)
endif()

add_executable(bench_ccac bench/bench_ccac.c)

if(PCRE2_FOUND)
  target_include_directories(bench_ccac PRIVATE ${PCRE2_INCLUDE_DIRS})
  target_link_libraries(bench_ccac PRIVATE ${PCRE2_LINK_LIBRARIES})
  target_compile_definitions(bench_ccac PRIVATE PCRE2_CODE_UNIT_WIDTH=8)

  add_custom_target(bench
    COMMAND bench_ccac
      ${CMAKE_BINARY_DIR}/words.zh.txt
      ${CMAKE_BINARY_DIR}/stress_text.txt
    DEPENDS bench_ccac gen_stress_data
    COMMENT "Running benchmark (ccac vs naive vs PCRE2)")
  message(STATUS "PCRE2 found — benchmark target enabled")
else()
  add_custom_target(bench
    COMMAND ${CMAKE_COMMAND} -E echo
      "Benchmark requires PCRE2 (libpcre2-8)."
      "Install: brew install pcre2  /  apt install libpcre2-dev")
  message(STATUS "PCRE2 NOT found — benchmark target is stub only")
endif()

# ════════════════════════════════════════════════════════════════════════
#  Meta targets
# ════════════════════════════════════════════════════════════════════════

# convenience: build everything needed for tests
add_custom_target(check
  DEPENDS test_unicode test_ccac test_ccac_robust test_ccac_stress gen_stress_data
  COMMENT "All test targets built.  Run: ctest --test-dir ${CMAKE_BINARY_DIR} --output-on-failure")

# install headers (optional)
include(GNUInstallDirs)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ccac
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "build" EXCLUDE
  PATTERN "tests" EXCLUDE
  PATTERN "bench" EXCLUDE
  PATTERN "scripts" EXCLUDE
  PATTERN "3rd" EXCLUDE
  PATTERN ".git" EXCLUDE)

# ── summary ─────────────────────────────────────────────────────────────
message(STATUS "")
message(STATUS "ccac ${PROJECT_VERSION}")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C standard  : C${CMAKE_C_STANDARD}")
message(STATUS "  Install dir : ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/ccac")
message(STATUS "")
message(STATUS "  Targets:")
message(STATUS "    make test_ccac / test_ccac_robust / test_ccac_stress")
message(STATUS "    ctest -L unit     — run unit tests (fast)")
message(STATUS "    ctest -L robust   — run robustness tests")
message(STATUS "    ctest -L stress   — run stress test (needs data gen)")
message(STATUS "    make check        — build + run all tests")
message(STATUS "    make bench        — run benchmark" ${PCRE2_TAG})
