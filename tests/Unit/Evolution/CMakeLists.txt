# Distributed under the MIT License.
# See LICENSE.txt for details.

add_subdirectory(Actions)
add_subdirectory(Ader)
add_subdirectory(BoundaryConditions)
add_subdirectory(DgSubcell)
add_subdirectory(DiscontinuousGalerkin)
add_subdirectory(EventsAndDenseTriggers)
add_subdirectory(Initialization)
add_subdirectory(Systems)
add_subdirectory(VariableFixing)

set(LIBRARY "Test_Evolution")

set(LIBRARY_SOURCES
  Test_BoundaryCorrectionTags.cpp
  Test_ComputeTags.cpp
  Test_NumericInitialData.cpp
  Test_Protocols.cpp
  Test_TagsDomain.cpp
  )

add_test_library(
  ${LIBRARY}
  "Evolution"
  "${LIBRARY_SOURCES}"
  "Boost::boost;Evolution;Utilities"
  )
