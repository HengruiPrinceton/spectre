// Distributed under the MIT License.
// See LICENSE.txt for details.

/// \file
/// Defines MathFunctions::PowX.

#pragma once

#include <pup.h>

#include "Options/Options.hpp"
#include "Parallel/CharmPupable.hpp"
#include "PointwiseFunctions/MathFunctions/MathFunction.hpp"  // IWYU pragma: keep
#include "Utilities/TMPL.hpp"

/// \cond
class DataVector;
/// \endcond

namespace MathFunctions {
template <size_t VolumeDim, typename Fr>
class PowX;

/*!
 * \ingroup MathFunctionsGroup
 * \brief Power of X \f$f(x)=x^X\f$
 */
template <typename Fr>
class PowX<1, Fr> : public MathFunction<1, Fr> {
 public:
  struct Power {
    using type = int;
    static constexpr Options::String help = {
        "The power that the double is raised to."};
  };
  using options = tmpl::list<Power>;

  static constexpr Options::String help = {
      "Raises the input value to a given power"};

  PowX() = default;
  ~PowX() override = default;
  PowX(const PowX& /*rhs*/) = delete;
  PowX& operator=(const PowX& /*rhs*/) = delete;
  PowX(PowX&& /*rhs*/) = default;
  PowX& operator=(PowX&& /*rhs*/) = default;

  WRAPPED_PUPable_decl_base_template(SINGLE_ARG(MathFunction<1, Fr>),
                                     PowX);  // NOLINT

  explicit PowX(int power);

  explicit PowX(CkMigrateMessage* /*unused*/) {}

  double operator()(const double& x) const override;
  DataVector operator()(const DataVector& x) const override;

  double first_deriv(const double& x) const override;
  DataVector first_deriv(const DataVector& x) const override;

  double second_deriv(const double& x) const override;
  DataVector second_deriv(const DataVector& x) const override;

  double third_deriv(const double& x) const override;
  DataVector third_deriv(const DataVector& x) const override;

  // NOLINTNEXTLINE(google-runtime-references)
  void pup(PUP::er& p) override;

 private:
  double power_{};
  friend bool operator==(const PowX& lhs, const PowX& rhs) {
    return lhs.power_ == rhs.power_;
  }

  template <typename T>
  T apply_call_operator(const T& x) const;
  template <typename T>
  T apply_first_deriv(const T& x) const;
  template <typename T>
  T apply_second_deriv(const T& x) const;
  template <typename T>
  T apply_third_deriv(const T& x) const;
};

template <typename Fr>
bool operator!=(const PowX<1, Fr>& lhs, const PowX<1, Fr>& rhs) {
  return not(lhs == rhs);
}
}  // namespace MathFunctions

/// \cond
template <typename Fr>
PUP::able::PUP_ID MathFunctions::PowX<1, Fr>::my_PUP_ID = 0;  // NOLINT
/// \endcond
