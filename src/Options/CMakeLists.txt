# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY Options)

add_spectre_library(${LIBRARY})

spectre_target_sources(
  ${LIBRARY}
  PRIVATE
  Comparator.cpp
  )

spectre_target_headers(
  ${LIBRARY}
  INCLUDE_DIRECTORY ${CMAKE_SOURCE_DIR}/src
  HEADERS
  Auto.hpp
  Comparator.hpp
  Factory.hpp
  FactoryHelpers.hpp
  Options.hpp
  OptionsDetails.hpp
  ParseOptions.hpp
  StdComplex.hpp
  Tags.hpp
  )

target_link_libraries(
  ${LIBRARY}
  PUBLIC
  ErrorHandling
  Utilities
  YamlCpp
  )

add_subdirectory(Protocols)
add_subdirectory(Python)
