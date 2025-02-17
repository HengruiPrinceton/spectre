// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <boost/preprocessor/arithmetic/sub.hpp>
#include <boost/preprocessor/list/for_each.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/tuple/enum.hpp>
#include <boost/preprocessor/tuple/to_list.hpp>

#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "Parallel/CharmPupable.hpp"
#include "Utilities/TMPL.hpp"
#include "Utilities/TypeTraits.hpp"

/// \cond
namespace EquationsOfState {
template <bool IsRelativistic>
class DarkEnergyFluid;
template <typename ColdEquationOfState>
class HybridEos;
template <bool IsRelativistic>
class IdealFluid;
template <bool IsRelativistic>
class PolytropicFluid;
class Spectral;
template <typename LowDensityEoS>
class Enthalpy;
}  // namespace EquationsOfState
/// \endcond

/// Contains all equations of state, including base class
namespace EquationsOfState {

namespace detail {
template <bool IsRelativistic, size_t ThermodynamicDim>
struct DerivedClasses {};

template <>
struct DerivedClasses<true, 1> {
  using type = tmpl::list<Spectral, Enthalpy<Spectral>, PolytropicFluid<true>>;
};

template <>
struct DerivedClasses<false, 1> {
  using type = tmpl::list<PolytropicFluid<false>>;
};

template <>
struct DerivedClasses<true, 2> {
  using type = tmpl::list<DarkEnergyFluid<true>, IdealFluid<true>,
                          HybridEos<PolytropicFluid<true>>>;
};

template <>
struct DerivedClasses<false, 2> {
  using type = tmpl::list<IdealFluid<false>, HybridEos<PolytropicFluid<false>>>;
};

}  // namespace detail

/*!
 * \ingroup EquationsOfStateGroup
 * \brief Base class for equations of state depending on whether or not the
 * system is relativistic, and the number of independent thermodynamic variables
 * (`ThermodynamicDim`) needed to determine the pressure.
 *
 * The template parameter `IsRelativistic` is `true` for relativistic equations
 * of state and `false` for non-relativistic equations of state.
 */
template <bool IsRelativistic, size_t ThermodynamicDim>
class EquationOfState;

/*!
 * \ingroup EquationsOfStateGroup
 * \brief Base class for equations of state which need one thermodynamic
 * variable in order to determine the pressure.
 *
 * The template parameter `IsRelativistic` is `true` for relativistic equations
 * of state and `false` for non-relativistic equations of state.
 */
template <bool IsRelativistic>
class EquationOfState<IsRelativistic, 1> : public PUP::able {
 public:
  static constexpr bool is_relativistic = IsRelativistic;
  static constexpr size_t thermodynamic_dim = 1;
  using creatable_classes =
      typename detail::DerivedClasses<IsRelativistic, 1>::type;

  EquationOfState() = default;
  EquationOfState(const EquationOfState&) = default;
  EquationOfState& operator=(const EquationOfState&) = default;
  EquationOfState(EquationOfState&&) = default;
  EquationOfState& operator=(EquationOfState&&) = default;
  ~EquationOfState() override = default;

  explicit EquationOfState(CkMigrateMessage* msg) : PUP::able(msg) {}

  WRAPPED_PUPable_abstract(EquationOfState);  // NOLINT

  /// @{
  /*!
   * Computes the pressure \f$p\f$ from the rest mass density \f$\rho\f$.
   */
  virtual Scalar<double> pressure_from_density(
      const Scalar<double>& /*rest_mass_density*/) const = 0;
  virtual Scalar<DataVector> pressure_from_density(
      const Scalar<DataVector>& /*rest_mass_density*/) const = 0;
  /// @}

  /// @{
  /*!
   * Computes the rest mass density \f$\rho\f$ from the specific enthalpy
   * \f$h\f$.
   */
  virtual Scalar<double> rest_mass_density_from_enthalpy(
      const Scalar<double>& /*specific_enthalpy*/) const = 0;
  virtual Scalar<DataVector> rest_mass_density_from_enthalpy(
      const Scalar<DataVector>& /*specific_enthalpy*/) const = 0;
  /// @}

  /// @{
  /*!
   * Computes the specific internal energy \f$\epsilon\f$ from the rest mass
   * density \f$\rho\f$.
   */
  virtual Scalar<double> specific_internal_energy_from_density(
      const Scalar<double>& /*rest_mass_density*/) const = 0;
  virtual Scalar<DataVector> specific_internal_energy_from_density(
      const Scalar<DataVector>& /*rest_mass_density*/) const = 0;
  /// @}

  /// @{
  /*!
   * Computes the temperature \f$T\f$ from the rest mass
   * density \f$\rho\f$.
   */
  virtual Scalar<double> temperature_from_density(
      const Scalar<double>& /*rest_mass_density*/) const {
    return Scalar<double>{0.0};
  }
  virtual Scalar<DataVector> temperature_from_density(
      const Scalar<DataVector>& rest_mass_density) const {
    return Scalar<DataVector>{DataVector{get(rest_mass_density).size(), 0.0}};
  }
  /// @}

  /// @{
  /*!
   * Computes the temperature \f$\T\f$ from the specific internal energy
   * \f$\epsilon\f$.
   */
  virtual Scalar<double> temperature_from_specific_internal_energy(
      const Scalar<double>& /*specific_internal_energy*/) const {
    return Scalar<double>{0.0};
  }
  virtual Scalar<DataVector> temperature_from_specific_internal_energy(
      const Scalar<DataVector>& specific_internal_energy) const {
    return Scalar<DataVector>{
        DataVector{get(specific_internal_energy).size(), 0.0}};
  }
  /// @}

  /// @{
  /*!
   * Computes \f$\chi=\partial p / \partial \rho\f$ from \f$\rho\f$, where
   * \f$p\f$ is the pressure and \f$\rho\f$ is the rest mass density.
   */
  virtual Scalar<double> chi_from_density(
      const Scalar<double>& /*rest_mass_density*/) const = 0;
  virtual Scalar<DataVector> chi_from_density(
      const Scalar<DataVector>& /*rest_mass_density*/) const = 0;
  /// @}

  /// @{
  /*!
   * Computes \f$\kappa p/\rho^2=(p/\rho^2)\partial p / \partial \epsilon\f$
   * from \f$\rho\f$, where \f$p\f$ is the pressure, \f$\rho\f$ is the rest mass
   * density, and \f$\epsilon\f$ is the specific internal energy.
   *
   * The reason for not returning just
   * \f$\kappa=\partial p / \partial \epsilon\f$ is to avoid division by zero
   * for small values of \f$\rho\f$ when assembling the speed of sound with
   * some equations of state.
   */
  virtual Scalar<double> kappa_times_p_over_rho_squared_from_density(
      const Scalar<double>& /*rest_mass_density*/) const = 0;
  virtual Scalar<DataVector> kappa_times_p_over_rho_squared_from_density(
      const Scalar<DataVector>& /*rest_mass_density*/) const = 0;

  /// The lower bound of the rest mass density that is valid for this EOS
  virtual double rest_mass_density_lower_bound() const = 0;

  /// The upper bound of the rest mass density that is valid for this EOS
  virtual double rest_mass_density_upper_bound() const = 0;

  /// The lower bound of the specific internal energy that is valid for this EOS
  /// at the given rest mass density \f$\rho\f$
  virtual double specific_internal_energy_lower_bound(
      const double rest_mass_density) const = 0;

  /// The upper bound of the specific internal energy that is valid for this EOS
  /// at the given rest mass density \f$\rho\f$
  virtual double specific_internal_energy_upper_bound(
      const double rest_mass_density) const = 0;

  /// The lower bound of the specific enthalpy that is valid for this EOS
  virtual double specific_enthalpy_lower_bound() const = 0;
};

/*!
 * \ingroup EquationsOfStateGroup
 * \brief Base class for equations of state which need two independent
 * thermodynamic variables in order to determine the pressure.
 *
 * The template parameter `IsRelativistic` is `true` for relativistic equations
 * of state and `false` for non-relativistic equations of state.
 */
template <bool IsRelativistic>
class EquationOfState<IsRelativistic, 2> : public PUP::able {
 public:
  static constexpr bool is_relativistic = IsRelativistic;
  static constexpr size_t thermodynamic_dim = 2;
  using creatable_classes =
      typename detail::DerivedClasses<IsRelativistic, 2>::type;

  EquationOfState() = default;
  EquationOfState(const EquationOfState&) = default;
  EquationOfState& operator=(const EquationOfState&) = default;
  EquationOfState(EquationOfState&&) = default;
  EquationOfState& operator=(EquationOfState&&) = default;
  ~EquationOfState() override = default;

  explicit EquationOfState(CkMigrateMessage* msg) : PUP::able(msg) {}

  WRAPPED_PUPable_abstract(EquationOfState);  // NOLINT

  /// @{
  /*!
   * Computes the pressure \f$p\f$ from the rest mass density \f$\rho\f$ and the
   * specific internal energy \f$\epsilon\f$.
   */
  virtual Scalar<double> pressure_from_density_and_energy(
      const Scalar<double>& /*rest_mass_density*/,
      const Scalar<double>& /*specific_internal_energy*/) const = 0;
  virtual Scalar<DataVector> pressure_from_density_and_energy(
      const Scalar<DataVector>& /*rest_mass_density*/,
      const Scalar<DataVector>& /*specific_internal_energy*/) const = 0;
  /// @}

  /// @{
  /*!
   * Computes the pressure \f$p\f$ from the rest mass density \f$\rho\f$ and the
   * specific enthalpy \f$h\f$.
   */
  virtual Scalar<double> pressure_from_density_and_enthalpy(
      const Scalar<double>& /*rest_mass_density*/,
      const Scalar<double>& /*specific_enthalpy*/) const = 0;
  virtual Scalar<DataVector> pressure_from_density_and_enthalpy(
      const Scalar<DataVector>& /*rest_mass_density*/,
      const Scalar<DataVector>& /*specific_enthalpy*/) const = 0;
  /// @}

  /// @{
  /*!
   * Computes the specific internal energy \f$\epsilon\f$ from the rest mass
   * density \f$\rho\f$ and the pressure \f$p\f$.
   */
  virtual Scalar<double> specific_internal_energy_from_density_and_pressure(
      const Scalar<double>& /*rest_mass_density*/,
      const Scalar<double>& /*pressure*/) const = 0;
  virtual Scalar<DataVector> specific_internal_energy_from_density_and_pressure(
      const Scalar<DataVector>& /*rest_mass_density*/,
      const Scalar<DataVector>& /*pressure*/) const = 0;
  /// @}

  /// @{
  /*!
   * Computes the temperature \f$T\f$ from the rest mass
   * density \f$\rho\f$ and the specific internal energy \f$\epsilon\f$.
   */
  virtual Scalar<double> temperature_from_density_and_energy(
      const Scalar<double>& /*rest_mass_density*/,
      const Scalar<double>& /*specific_internal_energy*/) const = 0;
  virtual Scalar<DataVector> temperature_from_density_and_energy(
      const Scalar<DataVector>& /*rest_mass_density*/,
      const Scalar<DataVector>& /*specific_internal_energy*/) const = 0;
  /// @}

  /// @{
  /*!
   * Computes the specific internal energy \f$\epsilon\f$ from the rest mass
   * density \f$\rho\f$ and the temperature \f$T\f$.
   */
  virtual Scalar<double> specific_internal_energy_from_density_and_temperature(
      const Scalar<double>& /*rest_mass_density*/,
      const Scalar<double>& /*temperature*/) const = 0;
  virtual Scalar<DataVector>
  specific_internal_energy_from_density_and_temperature(
      const Scalar<DataVector>& /*rest_mass_density*/,
      const Scalar<DataVector>& /*temperature*/) const = 0;
  /// @}

  /// @{
  /*!
   * Computes \f$\chi=\partial p / \partial \rho\f$ from the \f$\rho\f$ and
   * \f$\epsilon\f$, where \f$p\f$ is the pressure, \f$\rho\f$ is the rest mass
   * density, and \f$\epsilon\f$ is the specific internal energy.
   */
  virtual Scalar<double> chi_from_density_and_energy(
      const Scalar<double>& /*rest_mass_density*/,
      const Scalar<double>& /*specific_internal_energy*/) const = 0;
  virtual Scalar<DataVector> chi_from_density_and_energy(
      const Scalar<DataVector>& /*rest_mass_density*/,
      const Scalar<DataVector>& /*specific_internal_energy*/) const = 0;
  /// @}

  /// @{
  /*!
   * Computes \f$\kappa p/\rho^2=(p/\rho^2)\partial p / \partial \epsilon\f$
   * from \f$\rho\f$ and \f$\epsilon\f$, where \f$p\f$ is the pressure,
   * \f$\rho\f$ is the rest mass density, and \f$\epsilon\f$ is the specific
   * internal energy.
   *
   * The reason for not returning just
   * \f$\kappa=\partial p / \partial \epsilon\f$ is to avoid division by zero
   * for small values of \f$\rho\f$ when assembling the speed of sound with
   * some equations of state.
   */
  virtual Scalar<double> kappa_times_p_over_rho_squared_from_density_and_energy(
      const Scalar<double>& /*rest_mass_density*/,
      const Scalar<double>& /*specific_internal_energy*/) const = 0;
  virtual Scalar<DataVector>
  kappa_times_p_over_rho_squared_from_density_and_energy(
      const Scalar<DataVector>& /*rest_mass_density*/,
      const Scalar<DataVector>& /*specific_internal_energy*/) const = 0;
  /// @}

  /// The lower bound of the rest mass density that is valid for this EOS
  virtual double rest_mass_density_lower_bound() const = 0;

  /// The upper bound of the rest mass density that is valid for this EOS
  virtual double rest_mass_density_upper_bound() const = 0;

  /// The lower bound of the specific internal energy that is valid for this EOS
  /// at the given rest mass density \f$\rho\f$
  virtual double specific_internal_energy_lower_bound(
      const double rest_mass_density) const = 0;

  /// The upper bound of the specific internal energy that is valid for this EOS
  /// at the given rest mass density \f$\rho\f$
  virtual double specific_internal_energy_upper_bound(
      const double rest_mass_density) const = 0;

  /// The lower bound of the specific enthalpy that is valid for this EOS
  virtual double specific_enthalpy_lower_bound() const = 0;
};
}  // namespace EquationsOfState

/// \cond
#define EQUATION_OF_STATE_FUNCTIONS_1D                      \
  (pressure_from_density, rest_mass_density_from_enthalpy,  \
   specific_internal_energy_from_density, chi_from_density, \
   kappa_times_p_over_rho_squared_from_density)

#define EQUATION_OF_STATE_FUNCTIONS_2D                                   \
  (pressure_from_density_and_energy, pressure_from_density_and_enthalpy, \
   specific_internal_energy_from_density_and_pressure,                   \
   temperature_from_density_and_energy,                                  \
   specific_internal_energy_from_density_and_temperature,                \
   chi_from_density_and_energy,                                          \
   kappa_times_p_over_rho_squared_from_density_and_energy)

#define EQUATION_OF_STATE_ARGUMENTS_EXPAND(z, n, type) \
  BOOST_PP_COMMA_IF(n) const Scalar<type>&

#define EQUATION_OF_STATE_FORWARD_DECLARE_MEMBERS_HELPER(r, DIM,        \
                                                         FUNCTION_NAME) \
  Scalar<double> FUNCTION_NAME(BOOST_PP_REPEAT(                         \
      DIM, EQUATION_OF_STATE_ARGUMENTS_EXPAND, double)) const override; \
  Scalar<DataVector> FUNCTION_NAME(BOOST_PP_REPEAT(                     \
      DIM, EQUATION_OF_STATE_ARGUMENTS_EXPAND, DataVector)) const override;

/// \endcond

/*!
 * \ingroup EquationsOfStateGroup
 * \brief Macro used to generate forward declarations of member functions in
 * derived classes
 */
#define EQUATION_OF_STATE_FORWARD_DECLARE_MEMBERS(DERIVED, DIM)               \
  BOOST_PP_LIST_FOR_EACH(                                                     \
      EQUATION_OF_STATE_FORWARD_DECLARE_MEMBERS_HELPER, DIM,                  \
      BOOST_PP_TUPLE_TO_LIST(BOOST_PP_TUPLE_ELEM(                             \
          BOOST_PP_SUB(DIM, 1),                                               \
          (EQUATION_OF_STATE_FUNCTIONS_1D, EQUATION_OF_STATE_FUNCTIONS_2D)))) \
                                                                              \
  /* clang-tidy: do not use non-const references */                           \
  void pup(PUP::er& p) override; /* NOLINT */                                 \
                                                                              \
  explicit DERIVED(CkMigrateMessage* msg);

/// \cond
#define EQUATION_OF_STATE_FORWARD_ARGUMENTS(z, n, unused) \
  BOOST_PP_COMMA_IF(n) arg##n

#define EQUATION_OF_STATE_ARGUMENTS_EXPAND_NAMED(z, n, type) \
  BOOST_PP_COMMA_IF(n) const Scalar<type>& arg##n

#define EQUATION_OF_STATE_MEMBER_DEFINITIONS_HELPER(                        \
    TEMPLATE, DERIVED, DATA_TYPE, DIM, FUNCTION_NAME)                       \
  TEMPLATE                                                                  \
  Scalar<DATA_TYPE> DERIVED::FUNCTION_NAME(BOOST_PP_REPEAT(                 \
      DIM, EQUATION_OF_STATE_ARGUMENTS_EXPAND_NAMED, DATA_TYPE)) const {    \
    return FUNCTION_NAME##_impl(                                            \
        BOOST_PP_REPEAT(DIM, EQUATION_OF_STATE_FORWARD_ARGUMENTS, UNUSED)); \
  }

#define EQUATION_OF_STATE_MEMBER_DEFINITIONS_HELPER_2(r, ARGS, FUNCTION_NAME) \
  EQUATION_OF_STATE_MEMBER_DEFINITIONS_HELPER(                                \
      BOOST_PP_TUPLE_ELEM(0, ARGS), BOOST_PP_TUPLE_ELEM(1, ARGS),             \
      BOOST_PP_TUPLE_ELEM(2, ARGS), BOOST_PP_TUPLE_ELEM(3, ARGS),             \
      FUNCTION_NAME)
/// \endcond

#define EQUATION_OF_STATE_MEMBER_DEFINITIONS(TEMPLATE, DERIVED, DATA_TYPE, \
                                             DIM)                          \
  BOOST_PP_LIST_FOR_EACH(                                                  \
      EQUATION_OF_STATE_MEMBER_DEFINITIONS_HELPER_2,                       \
      (TEMPLATE, DERIVED, DATA_TYPE, DIM),                                 \
      BOOST_PP_TUPLE_TO_LIST(BOOST_PP_TUPLE_ELEM(                          \
          BOOST_PP_SUB(DIM, 1),                                            \
          (EQUATION_OF_STATE_FUNCTIONS_1D, EQUATION_OF_STATE_FUNCTIONS_2D))))

/// \cond
#define EQUATION_OF_STATE_FORWARD_DECLARE_MEMBER_IMPLS_HELPER(r, DIM,        \
                                                              FUNCTION_NAME) \
  template <class DataType>                                                  \
  Scalar<DataType> FUNCTION_NAME##_impl(BOOST_PP_REPEAT(                     \
      DIM, EQUATION_OF_STATE_ARGUMENTS_EXPAND, DataType)) const;
/// \endcond

#define EQUATION_OF_STATE_FORWARD_DECLARE_MEMBER_IMPLS(DIM)       \
  BOOST_PP_LIST_FOR_EACH(                                         \
      EQUATION_OF_STATE_FORWARD_DECLARE_MEMBER_IMPLS_HELPER, DIM, \
      BOOST_PP_TUPLE_TO_LIST(BOOST_PP_TUPLE_ELEM(                 \
          BOOST_PP_SUB(DIM, 1),                                   \
          (EQUATION_OF_STATE_FUNCTIONS_1D, EQUATION_OF_STATE_FUNCTIONS_2D))))
