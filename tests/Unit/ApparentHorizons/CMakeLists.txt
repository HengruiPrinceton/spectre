# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY "Test_ApparentHorizons")

set(LIBRARY_SOURCES
  Test_ApparentHorizonFinder.cpp
  Test_ComputeHorizonVolumeQuantities.cpp
  Test_ComputeItems.cpp
  Test_FastFlow.cpp
  Test_ObjectLabel.cpp
  Test_ObserveCenters.cpp
  Test_StrahlkorperGr.cpp
  Test_StrahlkorperInDifferentFrame.cpp
  Test_Tags.cpp
  )

add_test_library(
  ${LIBRARY}
  "ApparentHorizons"
  "${LIBRARY_SOURCES}"
  ""
  )

target_link_libraries(
  ${LIBRARY}
  PRIVATE
  ApparentHorizons
  ApparentHorizonsHelpers
  GeneralRelativitySolutions
  LinearOperators
  Logging
  ObserverHelpers
  Options
  ParallelInterpolation
  RootFinding
  SphericalHarmonics
  SphericalHarmonicsHelpers
  Utilities
  )

add_dependencies(
  ${LIBRARY}
  module_GlobalCache
  )
