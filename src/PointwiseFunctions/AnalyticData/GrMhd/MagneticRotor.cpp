// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "PointwiseFunctions/AnalyticData/GrMhd/MagneticRotor.hpp"

#include <cmath>  // IWYU pragma: keep
#include <ostream>
#include <pup.h>

#include "DataStructures/DataVector.hpp"  // IWYU pragma: keep
#include "DataStructures/Tensor/EagerMath/DotProduct.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "PointwiseFunctions/Hydro/LorentzFactor.hpp"
#include "PointwiseFunctions/Hydro/SpecificEnthalpy.hpp"
#include "PointwiseFunctions/Hydro/Tags.hpp"
#include "Utilities/ConstantExpressions.hpp"  // IWYU pragma: keep
#include "Utilities/GenerateInstantiations.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/MakeWithValue.hpp"
#include "Utilities/Math.hpp"  // IWYU pragma: keep

// IWYU pragma: no_include <complex>

namespace {
template <typename DataType>
Scalar<DataType> compute_piecewise(const tnsr::I<DataType, 3>& x,
                                   const double rotor_radius,
                                   const double rotor_value,
                                   const double background_value) {
  const DataType cylindrical_radius =
      sqrt(square(get<0>(x)) + square(get<1>(x)));
  return Scalar<DataType>(rotor_value -
                          (rotor_value - background_value) *
                              step_function(cylindrical_radius - rotor_radius));
}
}  // namespace

namespace grmhd::AnalyticData {

MagneticRotor::MagneticRotor(
    const double rotor_radius, const double rotor_density,
    const double background_density, const double pressure,
    const double angular_velocity, const std::array<double, 3>& magnetic_field,
    const double adiabatic_index, const Options::Context& context)
    : rotor_radius_(rotor_radius),
      rotor_density_(rotor_density),
      background_density_(background_density),
      pressure_(pressure),
      angular_velocity_(angular_velocity),
      magnetic_field_(magnetic_field),
      adiabatic_index_(adiabatic_index),
      equation_of_state_(adiabatic_index) {
  if (fabs(rotor_radius * angular_velocity) >= 1.0) {
    PARSE_ERROR(context,
                "MagneticRotor expects RotorRadius * | AngularVelocity | < 1, "
                "but RotorRadius = "
                    << rotor_radius
                    << " and AngularVelocity = " << angular_velocity);
  }
}

MagneticRotor::MagneticRotor(CkMigrateMessage* msg) : InitialData(msg) {}

void MagneticRotor::pup(PUP::er& p) {
  InitialData::pup(p);
  p | rotor_radius_;
  p | rotor_density_;
  p | background_density_;
  p | pressure_;
  p | angular_velocity_;
  p | magnetic_field_;
  p | adiabatic_index_;
  p | equation_of_state_;
  p | background_spacetime_;
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::RestMassDensity<DataType>>
MagneticRotor::variables(
    const tnsr::I<DataType, 3>& x,
    tmpl::list<hydro::Tags::RestMassDensity<DataType>> /*meta*/) const {
  return compute_piecewise(x, rotor_radius_, rotor_density_,
                           background_density_);
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::SpatialVelocity<DataType, 3>>
MagneticRotor::variables(
    const tnsr::I<DataType, 3>& x,
    tmpl::list<hydro::Tags::SpatialVelocity<DataType, 3>> /*meta*/) const {
  Scalar<DataType> angular_velocity =
      compute_piecewise(x, rotor_radius_, angular_velocity_, 0.0);
  auto spatial_velocity = make_with_value<tnsr::I<DataType, 3>>(x, 0.0);
  get<0>(spatial_velocity) = -get<1>(x) * get(angular_velocity);
  get<1>(spatial_velocity) = get<0>(x) * get(angular_velocity);
  return spatial_velocity;
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::SpecificInternalEnergy<DataType>>
MagneticRotor::variables(
    const tnsr::I<DataType, 3>& x,
    tmpl::list<hydro::Tags::SpecificInternalEnergy<DataType>> /*meta*/) const {
  return equation_of_state_.specific_internal_energy_from_density_and_pressure(
      get<hydro::Tags::RestMassDensity<DataType>>(
          variables(x, tmpl::list<hydro::Tags::RestMassDensity<DataType>>{})),
      get<hydro::Tags::Pressure<DataType>>(
          variables(x, tmpl::list<hydro::Tags::Pressure<DataType>>{})));
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::Pressure<DataType>> MagneticRotor::variables(
    const tnsr::I<DataType, 3>& x,
    tmpl::list<hydro::Tags::Pressure<DataType>> /*meta*/) const {
  return make_with_value<Scalar<DataType>>(x, pressure_);
  ;
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::MagneticField<DataType, 3>>
MagneticRotor::variables(
    const tnsr::I<DataType, 3>& x,
    tmpl::list<hydro::Tags::MagneticField<DataType, 3>> /*meta*/) const {
  auto magnetic_field = make_with_value<tnsr::I<DataType, 3>>(get<0>(x), 0.0);
  get<0>(magnetic_field) = gsl::at(magnetic_field_, 0);
  get<1>(magnetic_field) = gsl::at(magnetic_field_, 1);
  get<2>(magnetic_field) = gsl::at(magnetic_field_, 2);
  return magnetic_field;
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::DivergenceCleaningField<DataType>>
MagneticRotor::variables(
    const tnsr::I<DataType, 3>& x,
    tmpl::list<hydro::Tags::DivergenceCleaningField<DataType>> /*meta*/) const {
  return {make_with_value<Scalar<DataType>>(x, 0.0)};
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::LorentzFactor<DataType>>
MagneticRotor::variables(
    const tnsr::I<DataType, 3>& x,
    tmpl::list<hydro::Tags::LorentzFactor<DataType>> /*meta*/) const {
  using velocity_tag = hydro::Tags::SpatialVelocity<DataType, 3>;
  const auto velocity =
      get<velocity_tag>(variables(x, tmpl::list<velocity_tag>{}));
  return {hydro::lorentz_factor(dot_product(velocity, velocity))};
}

template <typename DataType>
tuples::TaggedTuple<hydro::Tags::SpecificEnthalpy<DataType>>
MagneticRotor::variables(
    const tnsr::I<DataType, 3>& x,
    tmpl::list<hydro::Tags::SpecificEnthalpy<DataType>> /*meta*/) const {
  using density_tag = hydro::Tags::RestMassDensity<DataType>;
  using energy_tag = hydro::Tags::SpecificInternalEnergy<DataType>;
  using pressure_tag = hydro::Tags::Pressure<DataType>;
  const auto data =
      variables(x, tmpl::list<density_tag, energy_tag, pressure_tag>{});
  return hydro::relativistic_specific_enthalpy(
      get<density_tag>(data), get<energy_tag>(data), get<pressure_tag>(data));
}

PUP::able::PUP_ID MagneticRotor::my_PUP_ID = 0;

bool operator==(const MagneticRotor& lhs, const MagneticRotor& rhs) {
  // there is no comparison operator for the EoS, but should be okay as
  // the adiabatic_indexs are compared
  return lhs.rotor_radius_ == rhs.rotor_radius_ and
         lhs.rotor_density_ == rhs.rotor_density_ and
         lhs.background_density_ == rhs.background_density_ and
         lhs.pressure_ == rhs.pressure_ and
         lhs.angular_velocity_ == rhs.angular_velocity_ and
         lhs.magnetic_field_ == rhs.magnetic_field_ and
         lhs.adiabatic_index_ == rhs.adiabatic_index_ and
         lhs.background_spacetime_ == rhs.background_spacetime_;
}

bool operator!=(const MagneticRotor& lhs, const MagneticRotor& rhs) {
  return not(lhs == rhs);
}

#define DTYPE(data) BOOST_PP_TUPLE_ELEM(0, data)
#define TAG(data) BOOST_PP_TUPLE_ELEM(1, data)

#define INSTANTIATE_SCALARS(_, data)                                           \
  template tuples::TaggedTuple<TAG(data) < DTYPE(data)> >                      \
      MagneticRotor::variables(const tnsr::I<DTYPE(data), 3>& x,               \
                               tmpl::list<TAG(data) < DTYPE(data)> > /*meta*/) \
          const;

GENERATE_INSTANTIATIONS(
    INSTANTIATE_SCALARS, (double, DataVector),
    (hydro::Tags::RestMassDensity, hydro::Tags::SpecificInternalEnergy,
     hydro::Tags::Pressure, hydro::Tags::DivergenceCleaningField,
     hydro::Tags::LorentzFactor, hydro::Tags::SpecificEnthalpy))

#define INSTANTIATE_VECTORS(_, data)                         \
  template tuples::TaggedTuple<TAG(data) < DTYPE(data), 3> > \
      MagneticRotor::variables(                              \
          const tnsr::I<DTYPE(data), 3>& x,                  \
          tmpl::list<TAG(data) < DTYPE(data), 3> > /*meta*/) const;

GENERATE_INSTANTIATIONS(INSTANTIATE_VECTORS, (double, DataVector),
                        (hydro::Tags::SpatialVelocity,
                         hydro::Tags::MagneticField))

#undef DTYPE
#undef TAG
#undef INSTANTIATE_SCALARS
#undef INSTANTIATE_VECTORS
}  // namespace grmhd::AnalyticData
