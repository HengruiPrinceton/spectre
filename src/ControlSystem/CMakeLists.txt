# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY ControlSystem)

add_spectre_library(${LIBRARY})

spectre_target_sources(
  ${LIBRARY}
  PRIVATE
  Averager.cpp
  Controller.cpp
  DataVectorHelpers.cpp
  TimescaleTuner.cpp
  )

spectre_target_headers(
  ${LIBRARY}
  INCLUDE_DIRECTORY ${CMAKE_SOURCE_DIR}/src
  HEADERS
  Averager.hpp
  Component.hpp
  Controller.hpp
  DataVectorHelpers.hpp
  Event.hpp
  InitialExpirationTimes.hpp
  Metafunctions.hpp
  NamespaceDocs.hpp
  RunCallbacks.hpp
  Tags.hpp
  TimescaleTuner.hpp
  Trigger.hpp
  UpdateControlSystem.hpp
  UpdateFunctionOfTime.hpp
  WriteData.hpp
  )

target_link_libraries(
  ${LIBRARY}
  PUBLIC
  Boost::boost
  DataStructures
  Domain
  ErrorHandling
  FunctionsOfTime
  IO
  )

add_subdirectory(Actions)
add_subdirectory(ApparentHorizons)
add_subdirectory(ControlErrors)
add_subdirectory(Protocols)
add_subdirectory(Systems)
add_subdirectory(Tags)
