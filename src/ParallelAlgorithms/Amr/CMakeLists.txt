# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY ParallelAmr)

add_spectre_library(${LIBRARY} INTERFACE)

target_link_libraries(
  ${LIBRARY}
  INTERFACE
  Amr
  DataStructures
  Initialization
  Utilities
  )

add_subdirectory(Actions)
add_subdirectory(Criteria)
