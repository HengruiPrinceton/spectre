# Distributed under the MIT License.
# See LICENSE.txt for details.

set(LIBRARY "Test_AsynchronousLinearSolvers")

set(LIBRARY_SOURCES
  Test_ElementActions.cpp
  )

add_test_library(
  ${LIBRARY}
  "ParallelAlgorithms/LinearSolver/AsynchronousSolvers"
  "${LIBRARY_SOURCES}"
  ""
  )

target_link_libraries(
  ${LIBRARY}
  PRIVATE
  Convergence
  DataStructures
  IO
  ParallelLinearSolver
  )
