# Distributed under the MIT License.
# See LICENSE.txt for details.

add_subdirectory(PhaseControl)

set(ALGORITHM_TEST_LINK_LIBRARIES
  # Link against Boost::program_options for now until we have proper
  # dependency handling for header-only libs
  Actions
  Boost::program_options
  Charmxx::main
  ErrorHandling
  Informer
  IO
  Options
  Parallel
  ParallelHelpers
  PhaseControl
  SystemUtilities
  Utilities
  )

# Add unit tests. For tests that result in a failure it is necessary to
# redirect output from stderr to stdout. However, it was necessary at least on
# some systems to do this redirect inside a shell command.
find_program(SHELL_EXECUTABLE "sh")
if (NOT SHELL_EXECUTABLE)
  message(FATAL_ERROR
    "Could not find 'sh' shell to execute failure tests of algorithms")
endif()

# Run TEST_NAME twice: first with no command-line args, then with the
# `+restart SpectreCheckpoint0` command-line arg to restart from a Charm++
# checkpoint named `SpectreCheckpoint0`. The checkpoint file is then deleted
# to avoid it getting in the way of future test invocations (which would try
# to overwrite the prior checkpoint file and error).
function(add_algorithm_test_with_restart_from_checkpoint TEST_NAME)
  add_standalone_test_executable("Test_${TEST_NAME}")
  target_link_libraries(
    "Test_${TEST_NAME}"
    PRIVATE
    "${ALGORITHM_TEST_LINK_LIBRARIES}")
  # Executable writes checkpoint file into the dir where it runs:
  set(CHECKPOINT
    "${CMAKE_BINARY_DIR}/tests/Unit/Parallel/${TEST_NAME}/SpectreCheckpoint000000")
  add_test(
    NAME "Unit.Parallel.${TEST_NAME}"
    COMMAND
    ${SHELL_EXECUTABLE}
    -c
    "mkdir -p ${TEST_NAME} \
    && cd ${TEST_NAME} \
    && rm -rf ${CHECKPOINT} \
    && ${SPECTRE_TEST_RUNNER} ${CMAKE_BINARY_DIR}/bin/Test_${TEST_NAME} 2>&1 \
    && ${SPECTRE_TEST_RUNNER} ${CMAKE_BINARY_DIR}/bin/Test_${TEST_NAME} \
       +restart ${CHECKPOINT} 2>&1 \
    && rm -rf ${CHECKPOINT} \
    && cd .."
    )
  set_standalone_test_properties("Unit.Parallel.${TEST_NAME}")
endfunction()

function(add_algorithm_test BASE_NAME)
  add_standalone_test("Unit.Parallel.${BASE_NAME}" ${ARGN})
  target_link_libraries(
    "Test_${BASE_NAME}"
    PRIVATE
    "${ALGORITHM_TEST_LINK_LIBRARIES}")
endfunction()

# Test GlobalCache
add_algorithm_test("GlobalCache")
add_charm_module(Test_GlobalCache)

add_dependencies(
  module_Test_GlobalCache
  module_GlobalCache
  )

add_dependencies(
  Test_GlobalCache
  module_Test_GlobalCache
  )

add_algorithm_test("AlgorithmCore")
add_algorithm_test("AlgorithmLocalSyncAction")
add_algorithm_test(
  "AlgorithmNestedApply1"
  REGEX_TO_MATCH
  "Already performing an Action and cannot execute additional Actions \
from inside of an Action. This is only possible if the simple_action \
function is not invoked via a proxy, which we do not allow.")
add_algorithm_test(
  "AlgorithmNestedApply2"
  REGEX_TO_MATCH
  "Already performing an Action and cannot execute additional Actions \
from inside of an Action. This is only possible if the simple_action \
function is not invoked via a proxy, which we do not allow.")
add_algorithm_test("AlgorithmNodelock")
add_algorithm_test("AlgorithmParallel")
add_algorithm_test("AlgorithmReduction")
add_algorithm_test("PhaseChangeMain")
add_algorithm_test(
  "SectionReductions"
  REGEX_TO_MATCH
  "Element 0 received reduction: Counted 2 odd elements.")

add_algorithm_test(
  "AlgorithmGlobalCache"
  INPUT_FILE "Test_AlgorithmGlobalCache.yaml")
add_algorithm_test(
  "AlgorithmPhaseControl"
  INPUT_FILE "Test_AlgorithmPhaseControl.yaml")

add_algorithm_test_with_restart_from_checkpoint("CheckpointRestart")

# Tests that do not require their own Chare setup and can work with the
# unit tests
set(LIBRARY "Test_Parallel")

set(LIBRARY_SOURCES
  Test_GlobalCacheDataBox.cpp
  Test_InboxInserters.cpp
  Test_MemoryMonitor.cpp
  Test_NodeLock.cpp
  Test_Parallel.cpp
  Test_ParallelComponentHelpers.cpp
  Test_Phase.cpp
  Test_PupStlCpp11.cpp
  Test_PupStlCpp17.cpp
  Test_ResourceInfo.cpp
  Test_Serialize.cpp
  Test_TypeTraits.cpp
  )

add_subdirectory(Tags)

add_test_library(
  ${LIBRARY}
  "Parallel"
  "${LIBRARY_SOURCES}"
  "IO;Options;Parallel;SystemUtilities"
  )

add_dependencies(
  ${LIBRARY}
  module_GlobalCache
  )
