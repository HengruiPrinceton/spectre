// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "PointwiseFunctions/AnalyticSolutions/GrMhd/KomissarovShock.hpp"

#include <pup.h>

#include "DataStructures/DataVector.hpp"  // IWYU pragma: keep
#include "DataStructures/Tensor/EagerMath/DotProduct.hpp"
#include "DataStructures/Tensor/Tensor.hpp"  // IWYU pragma: keep
#include "PointwiseFunctions/Hydro/LorentzFactor.hpp"
#include "PointwiseFunctions/Hydro/SpecificEnthalpy.hpp"
#include "PointwiseFunctions/Hydro/Tags.hpp"
#include "Utilities/GenerateInstantiations.hpp"
#include "Utilities/MakeWithValue.hpp"
#include "Utilities/Math.hpp"  // IWYU pragma: keep

// IWYU pragma: no_forward_declare Tensor

namespace {
template <typename DataType>
Scalar<DataType> compute_piecewise(const tnsr::I<DataType, 3>& x,
                                   const double shock_position,
                                   const double left_value,
                                   const double right_value) {
  return Scalar<DataType>(left_value -
                          (left_value - right_value) *
                              step_function(get<0>(x) - shock_position));
}

template <typename DataType>
tnsr::I<DataType, 3> compute_piecewise_vector(
    const tnsr::I<DataType, 3>& x, const double shock_position,
    const std::array<double, 3>& left_value,
    const std::array<double, 3>& right_value) {
  return tnsr::I<DataType, 3>{
      {{left_value[0] - (left_value[0] - right_value[0]) *
                            step_function(get<0>(x) - shock_position),
        left_value[1] - (left_value[1] - right_value[1]) *
                            step_function(get<0>(x) - shock_position),
        left_value[2] - (left_value[2] - right_value[2]) *
                            step_function(get<0>(x) - shock_position)}}};
}
}  // namespace

namespace grmhd::Solutions {

KomissarovShock::KomissarovShock(
    const double adiabatic_index, const double left_rest_mass_density,
    const double right_rest_mass_density, const double left_pressure,
    const double right_pressure,
    const std::array<double, 3>& left_spatial_velocity,
    const std::array<double, 3>& right_spatial_velocity,
    const std::array<double, 3>& left_magnetic_field,
    const std::array<double, 3>& right_magnetic_field, const double shock_speed)
    : equation_of_state_(adiabatic_index),
      adiabatic_index_(adiabatic_index),
      left_rest_mass_density_(left_rest_mass_density),
      right_rest_mass_density_(right_rest_mass_density),
      left_pressure_(left_pressure),
      right_pressure_(right_pressure),
      left_spatial_velocity_(left_spatial_velocity),
      right_spatial_velocity_(right_spatial_velocity),
      left_magnetic_field_(left_magnetic_field),
      right_magnetic_field_(right_magnetic_field),
      shock_speed_(shock_speed) {}

KomissarovShock::KomissarovShock(CkMigrateMessage* msg) : InitialData(msg) {}

void KomissarovShock::pup(PUP::er& p) {
  InitialData::pup(p);
  p | adiabatic_index_;
  p | left_rest_mass_density_;
  p | right_rest_mass_density_;
  p | left_pressure_;
  p | right_pressure_;
  p | left_spatial_velocity_;
  p | right_spatial_velocity_;
  p | left_magnetic_field_;
  p | right_magnetic_field_;
  p | shock_speed_;
  p | equation_of_state_;
  p | background_spacetime_;
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::RestMassDensity<DataType>>
KomissarovShock::variables(
    const tnsr::I<DataType, 3>& x, const double t,
    tmpl::list<hydro::Tags::RestMassDensity<DataType>> /*meta*/) const {
  return compute_piecewise(x, t * shock_speed_, left_rest_mass_density_,
                           right_rest_mass_density_);
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::SpatialVelocity<DataType, 3>>
KomissarovShock::variables(
    const tnsr::I<DataType, 3>& x, const double t,
    tmpl::list<hydro::Tags::SpatialVelocity<DataType, 3>> /*meta*/) const {
  return compute_piecewise_vector(x, t * shock_speed_, left_spatial_velocity_,
                                  right_spatial_velocity_);
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::SpecificInternalEnergy<DataType>>
KomissarovShock::variables(
    const tnsr::I<DataType, 3>& x, const double t,
    tmpl::list<hydro::Tags::SpecificInternalEnergy<DataType>> /*meta*/) const {
  return equation_of_state_.specific_internal_energy_from_density_and_pressure(
      get<hydro::Tags::RestMassDensity<DataType>>(variables(
          x, t, tmpl::list<hydro::Tags::RestMassDensity<DataType>>{})),
      get<hydro::Tags::Pressure<DataType>>(
          variables(x, t, tmpl::list<hydro::Tags::Pressure<DataType>>{})));
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::Pressure<DataType>> KomissarovShock::variables(
    const tnsr::I<DataType, 3>& x, const double t,
    tmpl::list<hydro::Tags::Pressure<DataType>> /*meta*/) const {
  return compute_piecewise(x, t * shock_speed_, left_pressure_,
                           right_pressure_);
  ;
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::MagneticField<DataType, 3>>
KomissarovShock::variables(
    const tnsr::I<DataType, 3>& x, const double t,
    tmpl::list<hydro::Tags::MagneticField<DataType, 3>> /*meta*/) const {
  return compute_piecewise_vector(x, t * shock_speed_, left_magnetic_field_,
                                  right_magnetic_field_);
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::DivergenceCleaningField<DataType>>
KomissarovShock::variables(
    const tnsr::I<DataType, 3>& x, const double /*t*/,
    tmpl::list<hydro::Tags::DivergenceCleaningField<DataType>> /*meta*/) const {
  return {make_with_value<Scalar<DataType>>(x, 0.0)};
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::LorentzFactor<DataType>>
KomissarovShock::variables(
    const tnsr::I<DataType, 3>& x, const double t,
    tmpl::list<hydro::Tags::LorentzFactor<DataType>> /*meta*/) const {
  const auto spatial_velocity = get<hydro::Tags::SpatialVelocity<DataType, 3>>(
      variables(x, t, tmpl::list<hydro::Tags::SpatialVelocity<DataType, 3>>{}));
  return {
      hydro::lorentz_factor(dot_product(spatial_velocity, spatial_velocity))};
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::SpecificEnthalpy<DataType>>
KomissarovShock::variables(
    const tnsr::I<DataType, 3>& x, const double t,
    tmpl::list<hydro::Tags::SpecificEnthalpy<DataType>> /*meta*/) const {
  using density_tag = hydro::Tags::RestMassDensity<DataType>;
  using energy_tag = hydro::Tags::SpecificInternalEnergy<DataType>;
  using pressure_tag = hydro::Tags::Pressure<DataType>;
  const auto data =
      variables(x, t, tmpl::list<density_tag, energy_tag, pressure_tag>{});
  return hydro::relativistic_specific_enthalpy(
      get<density_tag>(data), get<energy_tag>(data), get<pressure_tag>(data));
}

PUP::able::PUP_ID KomissarovShock::my_PUP_ID = 0;

bool operator==(const KomissarovShock& lhs, const KomissarovShock& rhs) {
  return lhs.adiabatic_index_ == rhs.adiabatic_index_ and
         lhs.left_rest_mass_density_ == rhs.left_rest_mass_density_ and
         lhs.right_rest_mass_density_ == rhs.right_rest_mass_density_ and
         lhs.left_pressure_ == rhs.left_pressure_ and
         lhs.right_pressure_ == rhs.right_pressure_ and
         lhs.left_spatial_velocity_ == rhs.left_spatial_velocity_ and
         lhs.right_spatial_velocity_ == rhs.right_spatial_velocity_ and
         lhs.left_magnetic_field_ == rhs.left_magnetic_field_ and
         lhs.right_magnetic_field_ == rhs.right_magnetic_field_ and
         lhs.shock_speed_ == rhs.shock_speed_;
}

bool operator!=(const KomissarovShock& lhs, const KomissarovShock& rhs) {
  return not(lhs == rhs);
}

#define DTYPE(data) BOOST_PP_TUPLE_ELEM(0, data)
#define TAG(data) BOOST_PP_TUPLE_ELEM(1, data)

#define INSTANTIATE_SCALARS(_, data)                                     \
  template tuples::TaggedTuple<TAG(data) < DTYPE(data)> >                \
      KomissarovShock::variables(const tnsr::I<DTYPE(data), 3>&, double, \
                                 tmpl::list<TAG(data) < DTYPE(data)> >) const;

GENERATE_INSTANTIATIONS(
    INSTANTIATE_SCALARS, (double, DataVector),
    (hydro::Tags::RestMassDensity, hydro::Tags::SpecificInternalEnergy,
     hydro::Tags::Pressure, hydro::Tags::DivergenceCleaningField,
     hydro::Tags::LorentzFactor, hydro::Tags::SpecificEnthalpy))

#define INSTANTIATE_VECTORS(_, data)                                       \
  template tuples::TaggedTuple<TAG(data) < DTYPE(data), 3> >               \
      KomissarovShock::variables(const tnsr::I<DTYPE(data), 3>&, double,   \
                                 tmpl::list<TAG(data) < DTYPE(data), 3> >) \
          const;

GENERATE_INSTANTIATIONS(INSTANTIATE_VECTORS, (double, DataVector),
                        (hydro::Tags::SpatialVelocity,
                         hydro::Tags::MagneticField))

#undef DTYPE
#undef TAG
#undef INSTANTIATE_SCALARS
#undef INSTANTIATE_VECTORS
}  // namespace grmhd::Solutions
