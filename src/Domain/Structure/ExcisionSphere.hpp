// Distributed under the MIT License.
// See LICENSE.txt for details.

/// \file
/// Defines class ExcisionSphere.

#pragma once

#include <array>
#include <cstddef>
#include <iosfwd>
#include <limits>
#include <unordered_map>

#include "Domain/Structure/Direction.hpp"
#include "Utilities/MakeArray.hpp"

/// \cond
namespace PUP {
class er;
}  // namespace PUP
/// \endcond

/// \ingroup ComputationalDomainGroup
/// The excision sphere information of a computational domain.
/// The excision sphere is assumed to be a coordinate sphere in the
/// grid frame.
///
/// \tparam VolumeDim the volume dimension.
template <size_t VolumeDim>
class ExcisionSphere {
 public:
  /// Constructor
  ///
  /// \param radius the radius of the excision sphere in the
  /// computational domain.
  /// \param center the coordinate center of the excision sphere
  /// in the computational domain.
  /// \param abutting_directions the set of blocks that touch the excision
  /// sphere, along with the direction in which they touch it.
  ExcisionSphere(
      double radius, std::array<double, VolumeDim> center,
      std::unordered_map<size_t, Direction<VolumeDim>> abutting_directions);

  /// Default constructor needed for Charm++ serialization.
  ExcisionSphere() = default;
  ~ExcisionSphere() = default;
  ExcisionSphere(const ExcisionSphere<VolumeDim>& /*rhs*/) = default;
  ExcisionSphere(ExcisionSphere<VolumeDim>&& /*rhs*/) = default;
  ExcisionSphere<VolumeDim>& operator=(
      const ExcisionSphere<VolumeDim>& /*rhs*/) = default;
  ExcisionSphere<VolumeDim>& operator=(ExcisionSphere<VolumeDim>&& /*rhs*/) =
      default;

  /// The radius of the ExcisionSphere.
  double radius() const { return radius_; }

  /// The coodinate center of the ExcisionSphere.
  const std::array<double, VolumeDim>& center() const { return center_; }

  /// The set of blocks that touch the excision sphere, along with the direction
  /// in which they touch it
  const std::unordered_map<size_t, Direction<VolumeDim>>& abutting_directions()
      const {
    return abutting_directions_;
  }

  // NOLINTNEXTLINE(google-runtime-references)
  void pup(PUP::er& p);

 private:
  double radius_{std::numeric_limits<double>::signaling_NaN()};
  std::array<double, VolumeDim> center_{
      make_array<VolumeDim>(std::numeric_limits<double>::signaling_NaN())};
  std::unordered_map<size_t, Direction<VolumeDim>> abutting_directions_{};
};

template <size_t VolumeDim>
std::ostream& operator<<(std::ostream& os,
                         const ExcisionSphere<VolumeDim>& excision_sphere);

template <size_t VolumeDim>
bool operator==(const ExcisionSphere<VolumeDim>& lhs,
                const ExcisionSphere<VolumeDim>& rhs);

template <size_t VolumeDim>
bool operator!=(const ExcisionSphere<VolumeDim>& lhs,
                const ExcisionSphere<VolumeDim>& rhs);
