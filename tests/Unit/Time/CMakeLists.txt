# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY "Test_Time")

set(LIBRARY_SOURCES
  Test_BoundaryHistory.cpp
  Test_EvolutionOrdering.cpp
  Test_History.cpp
  Test_SelfStart.cpp
  Test_Slab.cpp
  Test_Tags.cpp
  Test_TakeStep.cpp
  Test_Time.cpp
  Test_TimeSequence.cpp
  Test_TimeStepId.cpp
  Test_Utilities.cpp
  )

add_subdirectory(Actions)
add_subdirectory(StepChoosers)
add_subdirectory(StepControllers)
add_subdirectory(TimeSteppers)
add_subdirectory(Triggers)

add_test_library(
  ${LIBRARY}
  "Time"
  "${LIBRARY_SOURCES}"
  ""
  )

target_link_libraries(
  ${LIBRARY}
  PRIVATE
  CoordinateMaps
  Domain
  DomainStructure
  Time
  TimeStepperHelpers
  )

add_dependencies(
  ${LIBRARY}
  module_GlobalCache
  )
