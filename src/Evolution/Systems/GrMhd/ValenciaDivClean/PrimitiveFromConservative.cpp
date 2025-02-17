// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Evolution/Systems/GrMhd/ValenciaDivClean/PrimitiveFromConservative.hpp"

#include <iomanip>
#include <limits>
#include <optional>
#include <ostream>

#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tags/TempTensor.hpp"
#include "DataStructures/Tensor/EagerMath/DotProduct.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "DataStructures/Variables.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/KastaunEtAl.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/NewmanHamlin.hpp"  // IWYU pragma: keep
#include "Evolution/Systems/GrMhd/ValenciaDivClean/PalenzuelaEtAl.hpp"  // IWYU pragma: keep
#include "Evolution/Systems/GrMhd/ValenciaDivClean/PrimitiveRecoveryData.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/Tags.hpp"  // IWYU pragma: keep
#include "PointwiseFunctions/GeneralRelativity/IndexManipulation.hpp"
#include "PointwiseFunctions/GeneralRelativity/Tags.hpp"  // IWYU pragma: keep
#include "PointwiseFunctions/Hydro/SpecificEnthalpy.hpp"
#include "PointwiseFunctions/Hydro/Tags.hpp"  // IWYU pragma: keep
#include "Utilities/ConstantExpressions.hpp"
#include "Utilities/ErrorHandling/Error.hpp"
#include "Utilities/GenerateInstantiations.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/TMPL.hpp"

// IWYU pragma: no_include <array>

// IWYU pragma: no_forward_declare EquationsOfState::EquationOfState
// IWYU pragma: no_forward_declare Tensor

namespace grmhd::ValenciaDivClean {

template <typename OrderedListOfPrimitiveRecoverySchemes, bool ErrorOnFailure>
template <size_t ThermodynamicDim>
bool PrimitiveFromConservative<OrderedListOfPrimitiveRecoverySchemes,
                               ErrorOnFailure>::
    apply(const gsl::not_null<Scalar<DataVector>*> rest_mass_density,
          const gsl::not_null<Scalar<DataVector>*> specific_internal_energy,
          const gsl::not_null<tnsr::I<DataVector, 3, Frame::Inertial>*>
              spatial_velocity,
          const gsl::not_null<tnsr::I<DataVector, 3, Frame::Inertial>*>
              magnetic_field,
          const gsl::not_null<Scalar<DataVector>*> divergence_cleaning_field,
          const gsl::not_null<Scalar<DataVector>*> lorentz_factor,
          const gsl::not_null<Scalar<DataVector>*> pressure,
          const gsl::not_null<Scalar<DataVector>*> specific_enthalpy,
          const Scalar<DataVector>& tilde_d,
          const Scalar<DataVector>& tilde_tau,
          const tnsr::i<DataVector, 3, Frame::Inertial>& tilde_s,
          const tnsr::I<DataVector, 3, Frame::Inertial>& tilde_b,
          const Scalar<DataVector>& tilde_phi,
          const tnsr::ii<DataVector, 3, Frame::Inertial>& spatial_metric,
          const tnsr::II<DataVector, 3, Frame::Inertial>& inv_spatial_metric,
          const Scalar<DataVector>& sqrt_det_spatial_metric,
          const EquationsOfState::EquationOfState<true, ThermodynamicDim>&
              equation_of_state) {
  get(*divergence_cleaning_field) =
      get(tilde_phi) / get(sqrt_det_spatial_metric);
  for (size_t i = 0; i < 3; ++i) {
    magnetic_field->get(i) = tilde_b.get(i) / get(sqrt_det_spatial_metric);
  }
  const size_t size = get<0>(tilde_b).size();
  Variables<
      tmpl::list<::Tags::TempScalar<0>, ::Tags::TempScalar<1>,
                 ::Tags::TempScalar<2>, ::Tags::TempScalar<3>,
                 ::Tags::TempScalar<4>, ::Tags::TempI<5, 3, Frame::Inertial>>>
      temp_buffer(size);

  DataVector& total_energy_density =
      get(get<::Tags::TempScalar<0>>(temp_buffer));
  total_energy_density =
      (get(tilde_tau) + get(tilde_d)) / get(sqrt_det_spatial_metric);

  tnsr::I<DataVector, 3, Frame::Inertial>& tilde_s_upper =
      get<::Tags::TempI<5, 3, Frame::Inertial>>(temp_buffer);
  raise_or_lower_index(make_not_null(&tilde_s_upper), tilde_s,
                       inv_spatial_metric);

  Scalar<DataVector>& momentum_density_squared =
      get<::Tags::TempScalar<1>>(temp_buffer);
  dot_product(make_not_null(&momentum_density_squared), tilde_s, tilde_s_upper);
  get(momentum_density_squared) /= square(get(sqrt_det_spatial_metric));

  Scalar<DataVector>& momentum_density_dot_magnetic_field =
      get<::Tags::TempScalar<2>>(temp_buffer);
  dot_product(make_not_null(&momentum_density_dot_magnetic_field), tilde_s,
              *magnetic_field);
  get(momentum_density_dot_magnetic_field) /= get(sqrt_det_spatial_metric);

  Scalar<DataVector>& magnetic_field_squared =
      get<::Tags::TempScalar<3>>(temp_buffer);
  dot_product(make_not_null(&magnetic_field_squared), *magnetic_field,
              *magnetic_field, spatial_metric);

  DataVector& rest_mass_density_times_lorentz_factor =
      get(get<::Tags::TempScalar<4>>(temp_buffer));
  rest_mass_density_times_lorentz_factor =
      get(tilde_d) / get(sqrt_det_spatial_metric);

  for (size_t s = 0; s < total_energy_density.size(); ++s) {
    std::optional<PrimitiveRecoverySchemes::PrimitiveRecoveryData>
        primitive_data = std::nullopt;
    tmpl::for_each<OrderedListOfPrimitiveRecoverySchemes>(
        [&pressure, &primitive_data, &total_energy_density,
         &momentum_density_squared, &momentum_density_dot_magnetic_field,
         &magnetic_field_squared, &rest_mass_density_times_lorentz_factor,
         &equation_of_state, &s](auto scheme) {
          using primitive_recovery_scheme = tmpl::type_from<decltype(scheme)>;
          if (not primitive_data.has_value()) {
            primitive_data =
                primitive_recovery_scheme::template apply<ThermodynamicDim>(
                    get(*pressure)[s], total_energy_density[s],
                    get(momentum_density_squared)[s],
                    get(momentum_density_dot_magnetic_field)[s],
                    get(magnetic_field_squared)[s],
                    rest_mass_density_times_lorentz_factor[s],
                    equation_of_state);
          }
        });

    if (primitive_data.has_value()) {
      get(*rest_mass_density)[s] = primitive_data.value().rest_mass_density;
      const double coefficient_of_b =
          get(momentum_density_dot_magnetic_field)[s] /
          (primitive_data.value().rho_h_w_squared *
           (primitive_data.value().rho_h_w_squared +
            get(magnetic_field_squared)[s]));
      const double coefficient_of_s =
          1.0 / (get(sqrt_det_spatial_metric)[s] *
                 (primitive_data.value().rho_h_w_squared +
                  get(magnetic_field_squared)[s]));
      for (size_t i = 0; i < 3; ++i) {
        spatial_velocity->get(i)[s] =
            coefficient_of_b * magnetic_field->get(i)[s] +
            coefficient_of_s * tilde_s_upper.get(i)[s];
      }
      get(*lorentz_factor)[s] = primitive_data.value().lorentz_factor;
      get(*pressure)[s] = primitive_data.value().pressure;
    } else {
      if constexpr (ErrorOnFailure) {
        ERROR("All primitive inversion schemes failed at s = "
              << s << ".\n"
              << std::setprecision(17)
              << "total_energy_density = " << total_energy_density[s] << "\n"
              << "momentum_density_squared = "
              << get(momentum_density_squared)[s] << "\n"
              << "momentum_density_dot_magnetic_field = "
              << get(momentum_density_dot_magnetic_field)[s] << "\n"
              << "magnetic_field_squared = " << get(magnetic_field_squared)[s]
              << "\n"
              << "rest_mass_density_times_lorentz_factor = "
              << rest_mass_density_times_lorentz_factor[s] << "\n"
              << "previous_rest_mass_density = " << get(*rest_mass_density)[s]
              << "\n"
              << "previous_pressure = " << get(*pressure)[s] << "\n"
              << "previous_lorentz_factor = " << get(*lorentz_factor)[s]
              << "\n");
      } else {
        return false;
      }
    }
  }
  if constexpr (ThermodynamicDim == 1) {
    *specific_internal_energy =
        equation_of_state.specific_internal_energy_from_density(
            *rest_mass_density);
  } else if constexpr (ThermodynamicDim == 2) {
    *specific_internal_energy =
        equation_of_state.specific_internal_energy_from_density_and_pressure(
            *rest_mass_density, *pressure);
  }
  *specific_enthalpy = hydro::relativistic_specific_enthalpy(
      *rest_mass_density, *specific_internal_energy, *pressure);
  return true;
}
}  // namespace grmhd::ValenciaDivClean

#define RECOVERY(data) BOOST_PP_TUPLE_ELEM(0, data)
#define ERROR_ON_FAILURE(data) BOOST_PP_TUPLE_ELEM(1, data)

#define INSTANTIATION(_, data)                                        \
  template struct grmhd::ValenciaDivClean::PrimitiveFromConservative< \
      RECOVERY(data), ERROR_ON_FAILURE(data)>;

using NewmanHamlinThenPalenzuelaEtAl = tmpl::list<
    grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::NewmanHamlin,
    grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::PalenzuelaEtAl>;
using KastaunThenNewmanThenPalenzuela = tmpl::list<
    grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::KastaunEtAl,
    grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::NewmanHamlin,
    grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::PalenzuelaEtAl>;

GENERATE_INSTANTIATIONS(
    INSTANTIATION,
    (tmpl::list<
         grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::NewmanHamlin>,
     tmpl::list<
         grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::PalenzuelaEtAl>,
     NewmanHamlinThenPalenzuelaEtAl),
    (true, false))

#undef INSTANTIATION

#define THERMODIM(data) BOOST_PP_TUPLE_ELEM(2, data)

#define INSTANTIATION(_, data)                                                \
  template bool grmhd::ValenciaDivClean::PrimitiveFromConservative<           \
      RECOVERY(data), ERROR_ON_FAILURE(data)>::                               \
      apply<THERMODIM(data)>(                                                 \
          const gsl::not_null<Scalar<DataVector>*> rest_mass_density,         \
          const gsl::not_null<Scalar<DataVector>*> specific_internal_energy,  \
          const gsl::not_null<tnsr::I<DataVector, 3, Frame::Inertial>*>       \
              spatial_velocity,                                               \
          const gsl::not_null<tnsr::I<DataVector, 3, Frame::Inertial>*>       \
              magnetic_field,                                                 \
          const gsl::not_null<Scalar<DataVector>*> divergence_cleaning_field, \
          const gsl::not_null<Scalar<DataVector>*> lorentz_factor,            \
          const gsl::not_null<Scalar<DataVector>*> pressure,                  \
          const gsl::not_null<Scalar<DataVector>*> specific_enthalpy,         \
          const Scalar<DataVector>& tilde_d,                                  \
          const Scalar<DataVector>& tilde_tau,                                \
          const tnsr::i<DataVector, 3, Frame::Inertial>& tilde_s,             \
          const tnsr::I<DataVector, 3, Frame::Inertial>& tilde_b,             \
          const Scalar<DataVector>& tilde_phi,                                \
          const tnsr::ii<DataVector, 3, Frame::Inertial>& spatial_metric,     \
          const tnsr::II<DataVector, 3, Frame::Inertial>& inv_spatial_metric, \
          const Scalar<DataVector>& sqrt_det_spatial_metric,                  \
          const EquationsOfState::EquationOfState<true, THERMODIM(data)>&     \
              equation_of_state);

GENERATE_INSTANTIATIONS(
    INSTANTIATION,
    (tmpl::list<grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::KastaunEtAl>,
     tmpl::list<
         grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::NewmanHamlin>,
     tmpl::list<
         grmhd::ValenciaDivClean::PrimitiveRecoverySchemes::PalenzuelaEtAl>,
     NewmanHamlinThenPalenzuelaEtAl, KastaunThenNewmanThenPalenzuela),
    (true, false), (1, 2))

#undef INSTANTIATION
#undef THERMODIM
#undef RECOVERY
