// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <stdexcept>

/// \ingroup ErrorHandlingGroup
/// Exception indicating an ASSERT failed
class SpectreAssert : public std::runtime_error {
 public:
  explicit SpectreAssert(const std::string& message) : runtime_error(message) {}
};

/// \ingroup ErrorHandlingGroup
/// Exception indicating an ERROR was triggered
class SpectreError : public std::runtime_error {
 public:
  explicit SpectreError(const std::string& message) : runtime_error(message) {}
};

/// \ingroup ErrorHandlingGroup
/// Exception indicating convergence failure
class convergence_error : public std::runtime_error {
 public:
  explicit convergence_error(const std::string& message)
      : runtime_error(message) {}
};
