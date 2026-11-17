cmake_minimum_required(VERSION 3.16)
project(skytrace VERSION 3.0.0 LANGUAGES C)

# ISO C11 core; POSIX is used only for file-system durability (fsync, rename, fcntl locks, readdir).
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Werror -pedantic)

option(SKYTRACE_REQUIRE_BQN "Fail configuration when the BQN interpreter is missing" ON)
option(SKYTRACE_REQUIRE_PROLOG "Fail configuration when SWI-Prolog is missing" ON)

add_library(skytrace_core STATIC
  src/core/error.c src/core/rng.c src/core/rng_tables.c src/core/sha256.c src/core/tri.c src/core/io.c src/core/json.c src/core/linalg.c
  src/evidence/case.c src/evidence/store.c src/evidence/journal.c
  src/graph/csr.c src/graph/records.c
  src/sensor/sensor_graph.c
  src/fusion/constant_turn.c
  src/braid/fibonacci.c src/braid/burau.c src/braid/features.c
  src/quantum/quantum.c src/quantum/entropy.c
  src/research/npcompat.c src/research/ridge.c src/research/param_recovery.c src/research/studies_pr.c
  src/research/task_information.c src/research/joint_fusion.c src/research/speculative_math.c src/research/compare.c src/research/numerical.c)
target_include_directories(skytrace_core PUBLIC include)
target_link_libraries(skytrace_core PUBLIC m)

add_executable(skytrace src/cli/main.c src/cli/cases.c src/cli/reports.c src/cli/research.c src/cli/research_export.c)
target_link_libraries(skytrace PRIVATE skytrace_core)

enable_testing()
foreach(t core evidence graph sensor fusion braid quantum)
  add_executable(test_${t} tests/native/test_${t}.c)
  target_link_libraries(test_${t} PRIVATE skytrace_core)
  add_test(NAME native_${t} COMMAND test_${t} ${CMAKE_SOURCE_DIR})
endforeach()
add_executable(test_cli tests/native/test_cli.c)
target_link_libraries(test_cli PRIVATE skytrace_core)
add_test(NAME native_cli COMMAND test_cli $<TARGET_FILE:skytrace> ${CMAKE_SOURCE_DIR})

# Research ports (formerly Python under research/): unit-test ports, bit-exact RNG streams, and
# agreement of every report with its committed results file (tolerances in research/PORTING.md).
foreach(t research_rng research_pr research_ti_sm)
  add_executable(test_${t} tests/native/test_${t}.c)
  target_link_libraries(test_${t} PRIVATE skytrace_core)
  add_test(NAME native_${t} COMMAND test_${t} ${CMAKE_SOURCE_DIR})
endforeach()
add_executable(test_research_jf tests/native/test_research_jf.c)
target_link_libraries(test_research_jf PRIVATE skytrace_core)
add_test(NAME native_research_jf COMMAND test_research_jf ${CMAKE_SOURCE_DIR} $<TARGET_FILE:skytrace>)
foreach(s parameter_recovery robustness graph_budget task_information source_fusion joint_fusion numerical release22)
  add_test(NAME research_${s}_vs_committed COMMAND skytrace research-check ${s})
  set_tests_properties(research_${s}_vs_committed PROPERTIES ENVIRONMENT SKYTRACE_ROOT=${CMAKE_SOURCE_DIR})
endforeach()

add_test(NAME cli_braid_check COMMAND skytrace braid-check)
add_test(NAME cli_quantum_check COMMAND skytrace quantum-check)
add_test(NAME cli_fit_vs_python_reference COMMAND skytrace fit
  ${CMAKE_SOURCE_DIR}/fixtures/reference/physical_fixture.seeds0-11.json
  ${CMAKE_SOURCE_DIR}/research/release_22/results.synthetic.json)

find_program(BQN_EXE NAMES bqn BQN)
if(BQN_EXE)
  add_test(NAME bqn_export COMMAND skytrace export ${CMAKE_BINARY_DIR}/interchange)
  add_test(NAME bqn_tests COMMAND ${BQN_EXE} ${CMAKE_SOURCE_DIR}/bqn/tests/run.bqn ${CMAKE_BINARY_DIR}/interchange)
  set_tests_properties(bqn_export PROPERTIES FIXTURES_SETUP interchange)
  set_tests_properties(bqn_tests PROPERTIES FIXTURES_REQUIRED interchange)
  add_test(NAME bqn_research_export COMMAND skytrace research-export ${CMAKE_BINARY_DIR}/research_interchange)
  add_test(NAME bqn_research COMMAND ${BQN_EXE} ${CMAKE_SOURCE_DIR}/bqn/tests/research.bqn ${CMAKE_BINARY_DIR}/research_interchange)
  set_tests_properties(bqn_research_export PROPERTIES FIXTURES_SETUP research_interchange)
  set_tests_properties(bqn_research PROPERTIES FIXTURES_REQUIRED research_interchange)
elseif(SKYTRACE_REQUIRE_BQN)
  message(FATAL_ERROR "BQN interpreter (CBQN) not found; BQN tests cannot be reported as passing")
endif()

find_program(SWIPL_EXE NAMES swipl)
if(SWIPL_EXE)
  add_test(NAME prolog_plunit COMMAND ${SWIPL_EXE} -q ${CMAKE_SOURCE_DIR}/prolog/tests/run_tests.pl ${CMAKE_SOURCE_DIR})
elseif(SKYTRACE_REQUIRE_PROLOG)
  message(FATAL_ERROR "SWI-Prolog not found; plunit tests cannot be reported as passing")
endif()

# ---------------------------------------------------------------- formal checks
option(SKYTRACE_REQUIRE_Z3 "Fail configuration when z3 is missing" OFF)
option(SKYTRACE_REQUIRE_SAW "Fail configuration when SAW or clang is missing" OFF)

find_program(Z3_EXE NAMES z3)
if(Z3_EXE)
  # Each lemma file prints one label line followed by the solver answer; the expected answers are
  # spelled out so a `sat` (counterexample) or an `unknown` fails the test.
  add_test(NAME smt_containment_walk_n6 COMMAND ${Z3_EXE} ${CMAKE_SOURCE_DIR}/proofs/smt/containment_walk_n6.smt2)
  set_tests_properties(smt_containment_walk_n6 PROPERTIES
    PASS_REGULAR_EXPRESSION "lemma 1[^\n]*\nunsat\nlemma 2[^\n]*\nunsat\nlemma 3[^\n]*\nunsat"
    FAIL_REGULAR_EXPRESSION "error|unknown")
  add_test(NAME smt_kernel_policy_bv32 COMMAND ${Z3_EXE} ${CMAKE_SOURCE_DIR}/proofs/smt/kernel_policy_bv32.smt2)
  set_tests_properties(smt_kernel_policy_bv32 PROPERTIES
    PASS_REGULAR_EXPRESSION "lemma 1[^\n]*\nunsat\nlemma 2[^\n]*\nunsat\nlemma 3[^\n]*\nunsat\nlemma 4[^\n]*\nunsat\nsanity[^\n]*\nsat"
    FAIL_REGULAR_EXPRESSION "error|unknown")
elseif(SKYTRACE_REQUIRE_Z3)
  message(FATAL_ERROR "z3 not found; SMT lemmas cannot be reported as proved")
endif()

# SAW resolves bitcode paths relative to the script, so bitcode is always emitted under
# <source>/build/proofs (rewrite) and <source>/kernel/build (original kernel), whatever the binary dir.
find_program(SAW_EXE NAMES saw)
find_program(CLANG_EXE NAMES clang clang-18 clang-17 clang-16)
if(SAW_EXE AND CLANG_EXE)
  add_test(NAME saw_bitcode COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/build/proofs ${CMAKE_SOURCE_DIR}/kernel/build)
  add_test(NAME saw_bitcode_rewrite COMMAND ${CLANG_EXE} -g -O0 -std=c11 -emit-llvm -c -I${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/core/tri.c -o ${CMAKE_SOURCE_DIR}/build/proofs/tri.bc)
  add_test(NAME saw_bitcode_kernel COMMAND ${CLANG_EXE} -g -O0 -emit-llvm -c
    ${CMAKE_SOURCE_DIR}/kernel/src/skytrace.c -o ${CMAKE_SOURCE_DIR}/kernel/build/skytrace.bc)
  set_tests_properties(saw_bitcode PROPERTIES FIXTURES_SETUP saw_dirs)
  set_tests_properties(saw_bitcode_rewrite saw_bitcode_kernel PROPERTIES FIXTURES_REQUIRED saw_dirs FIXTURES_SETUP saw_bc)
  add_test(NAME saw_closure_checked COMMAND ${SAW_EXE} ${CMAKE_SOURCE_DIR}/proofs/saw/closure_checked.saw)
  add_test(NAME saw_rewrite COMMAND ${SAW_EXE} ${CMAKE_SOURCE_DIR}/proofs/saw/rewrite.saw)
  set_tests_properties(saw_closure_checked saw_rewrite PROPERTIES FIXTURES_REQUIRED saw_bc FAIL_REGULAR_EXPRESSION "Invalid|unsolved|Error")
elseif(SKYTRACE_REQUIRE_SAW)
  message(FATAL_ERROR "SAW or clang not found; SAW proofs cannot be reported as proved")
endif()
