# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY Evolution)

add_spectre_library(${LIBRARY})

spectre_target_sources(
  ${LIBRARY}
  PRIVATE
  TagsDomain.cpp
  )

spectre_target_headers(
  ${LIBRARY}
  INCLUDE_DIRECTORY ${CMAKE_SOURCE_DIR}/src
  HEADERS
  BoundaryCorrectionTags.hpp
  ComputeTags.hpp
  Evolution.hpp
  NumericInitialData.hpp
  Protocols.hpp
  Tags.hpp
  TagsDomain.hpp
  TypeTraits.hpp
  )

target_link_libraries(
  ${LIBRARY}
  PUBLIC
  Boost::boost
  DataStructures
  DiscontinuousGalerkin
  Domain
  DomainStructure
  ErrorHandling
  InitialDataUtilities
  Options
  Spectral
  Time
  Utilities
  PRIVATE
  LinearOperators
  INTERFACE
  SystemUtilities
  )

add_dependencies(
  ${LIBRARY}
  module_GlobalCache
  )

add_subdirectory(Actions)
add_subdirectory(Ader)
add_subdirectory(BoundaryConditions)
add_subdirectory(Conservative)
add_subdirectory(DgSubcell)
add_subdirectory(DiscontinuousGalerkin)
add_subdirectory(EventsAndDenseTriggers)
add_subdirectory(Executables)
add_subdirectory(Initialization)
add_subdirectory(Systems)
add_subdirectory(VariableFixing)
