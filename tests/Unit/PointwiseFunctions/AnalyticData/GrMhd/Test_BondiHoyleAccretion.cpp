// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Framework/TestingFramework.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <tuple>

#include "DataStructures/DataBox/Prefixes.hpp"  // IWYU pragma: keep
#include "DataStructures/DataVector.hpp"        // IWYU pragma: keep
#include "DataStructures/Tensor/TypeAliases.hpp"
#include "Framework/CheckWithRandomValues.hpp"
#include "Framework/SetupLocalPythonEnvironment.hpp"
#include "Framework/TestCreation.hpp"
#include "Framework/TestHelpers.hpp"
#include "Parallel/RegisterDerivedClassesWithCharm.hpp"
#include "PointwiseFunctions/AnalyticData/GrMhd/BondiHoyleAccretion.hpp"
#include "PointwiseFunctions/Hydro/Tags.hpp"
#include "PointwiseFunctions/InitialDataUtilities/InitialData.hpp"
#include "PointwiseFunctions/InitialDataUtilities/Tags/InitialData.hpp"
#include "Utilities/TMPL.hpp"
#include "Utilities/TaggedTuple.hpp"

namespace {

struct BondiHoyleAccretionProxy : grmhd::AnalyticData::BondiHoyleAccretion {
  using grmhd::AnalyticData::BondiHoyleAccretion::BondiHoyleAccretion;

  template <typename DataType>
  using hydro_variables_tags =
      tmpl::list<hydro::Tags::RestMassDensity<DataType>,
                 hydro::Tags::SpatialVelocity<DataType, 3>,
                 hydro::Tags::SpecificInternalEnergy<DataType>,
                 hydro::Tags::Pressure<DataType>,
                 hydro::Tags::LorentzFactor<DataType>,
                 hydro::Tags::SpecificEnthalpy<DataType>>;

  template <typename DataType>
  using grmhd_variables_tags =
      tmpl::push_back<hydro_variables_tags<DataType>,
                      hydro::Tags::MagneticField<DataType, 3>,
                      hydro::Tags::DivergenceCleaningField<DataType>>;

  template <typename DataType>
  tuples::tagged_tuple_from_typelist<hydro_variables_tags<DataType>>
  hydro_variables(const tnsr::I<DataType, 3, Frame::Inertial>& x) const {
    return variables(x, hydro_variables_tags<DataType>{});
  }

  template <typename DataType>
  tuples::tagged_tuple_from_typelist<grmhd_variables_tags<DataType>>
  grmhd_variables(const tnsr::I<DataType, 3, Frame::Inertial>& x) const {
    return variables(x, grmhd_variables_tags<DataType>{});
  }
};

void test_create_from_options() {
  Parallel::register_classes_with_charm<
      grmhd::AnalyticData::BondiHoyleAccretion>();
  const std::unique_ptr<evolution::initial_data::InitialData> option_solution =
      TestHelpers::test_option_tag_factory_creation<
          evolution::initial_data::OptionTags::InitialData,
          grmhd::AnalyticData::BondiHoyleAccretion>(
          "BondiHoyleAccretion:\n"
          "  BhMass: 1.0\n"
          "  BhDimlessSpin: 0.23\n"
          "  RestMassDensity: 2.7\n"
          "  FlowSpeed: 0.34\n"
          "  MagFieldStrength: 5.76\n"
          "  PolytropicConstant: 30.0\n"
          "  PolytropicExponent: 1.5");
  const auto deserialized_option_solution =
      serialize_and_deserialize(option_solution);
  const auto& accretion =
      dynamic_cast<const grmhd::AnalyticData::BondiHoyleAccretion&>(
          *deserialized_option_solution);
  CHECK(accretion == grmhd::AnalyticData::BondiHoyleAccretion(
                         1.0, 0.23, 2.7, 0.34, 5.76, 30.0, 1.5));
}

void test_move() {
  grmhd::AnalyticData::BondiHoyleAccretion accretion(0.2, 0.12, 1.1, 0.63, 3.1,
                                                     251.4, 1.4);
  grmhd::AnalyticData::BondiHoyleAccretion accretion_copy(0.2, 0.12, 1.1, 0.63,
                                                          3.1, 251.4, 1.4);
  test_move_semantics(std::move(accretion), accretion_copy);  //  NOLINT
}

void test_serialize() {
  grmhd::AnalyticData::BondiHoyleAccretion accretion(0.2, 0.12, 1.1, 0.63, 3.1,
                                                     133.7, 1.65);
  test_serialization(accretion);
}

template <typename DataType>
void test_variables(const DataType& used_for_size) {
  const double bh_mass = 1.6;
  const double bh_dimless_spin = 0.24;
  const double rest_mass_density = 1.42;
  const double flow_speed = 0.87;
  const double mag_field_strength = 2.3;
  const double polytropic_constant = 25.0;
  const double polytropic_exponent = 4.0 / 3.0;
  const auto member_variables = std::make_tuple(
      bh_mass, bh_dimless_spin, rest_mass_density, flow_speed,
      mag_field_strength, polytropic_constant, polytropic_exponent);

  BondiHoyleAccretionProxy accretion(
      bh_mass, bh_dimless_spin, rest_mass_density, flow_speed,
      mag_field_strength, polytropic_constant, polytropic_exponent);

  pypp::check_with_random_values<1>(
      &BondiHoyleAccretionProxy::hydro_variables<DataType>, accretion,
      "PointwiseFunctions.AnalyticData.GrMhd.BondiHoyleAccretion",
      {"rest_mass_density", "spatial_velocity", "specific_internal_energy",
       "pressure", "lorentz_factor", "specific_enthalpy"},
      {{{-10., 10.}}}, member_variables, used_for_size);

  pypp::check_with_random_values<1>(
      &BondiHoyleAccretionProxy::grmhd_variables<DataType>, accretion,
      "PointwiseFunctions.AnalyticData.GrMhd.BondiHoyleAccretion",
      {"rest_mass_density", "spatial_velocity", "specific_internal_energy",
       "pressure", "lorentz_factor", "specific_enthalpy", "magnetic_field",
       "divergence_cleaning_field"},
      {{{-10., 10.}}}, member_variables, used_for_size);
}

}  // namespace

SPECTRE_TEST_CASE("Unit.PointwiseFunctions.AnalyticData.GrMhd.BondiHoyle",
                  "[Unit][PointwiseFunctions]") {
  pypp::SetupLocalPythonEnvironment local_python_env{""};

  test_create_from_options();
  test_serialize();
  test_move();

  test_variables(std::numeric_limits<double>::signaling_NaN());
  test_variables(DataVector(5));

  CHECK_THROWS_WITH(
      TestHelpers::test_creation<grmhd::AnalyticData::BondiHoyleAccretion>(
          "BhMass: -1.345\n"
          "BhDimlessSpin: 0.3\n"
          "RestMassDensity: 1.32\n"
          "FlowSpeed: 4.3\n"
          "MagFieldStrength: 0.52\n"
          "PolytropicConstant: 0.12\n"
          "PolytropicExponent: 1.5"),
      Catch::Contains("Value -1.345 is below the lower bound of 0"));
  CHECK_THROWS_WITH(
      TestHelpers::test_creation<grmhd::AnalyticData::BondiHoyleAccretion>(
          "BhMass: 4.4231\n"
          "BhDimlessSpin: -1.23\n"
          "RestMassDensity: 0.31\n"
          "FlowSpeed: 0.11\n"
          "MagFieldStrength: 2.4\n"
          "PolytropicConstant: 300.0\n"
          "PolytropicExponent: 1.4"),
      Catch::Contains("Value -1.23 is below the lower bound of -1"));
  CHECK_THROWS_WITH(
      TestHelpers::test_creation<grmhd::AnalyticData::BondiHoyleAccretion>(
          "BhMass: 0.654\n"
          "BhDimlessSpin: 3.99\n"
          "RestMassDensity: 5.234\n"
          "FlowSpeed: 0.543\n"
          "MagFieldStrength: -0.352\n"
          "PolytropicConstant: 80.123\n"
          "PolytropicExponent: 1.66"),
      Catch::Contains("Value 3.99 is above the upper bound of 1"));
  CHECK_THROWS_WITH(
      TestHelpers::test_creation<grmhd::AnalyticData::BondiHoyleAccretion>(
          "BhMass: 12.34\n"
          "BhDimlessSpin: 0.99\n"
          "RestMassDensity: -4.21\n"
          "FlowSpeed: 1.3\n"
          "MagFieldStrength: 0.21\n"
          "PolytropicConstant: 54.16\n"
          "PolytropicExponent: 1.598"),
      Catch::Contains("Value -4.21 is below the lower bound of 0"));
  CHECK_THROWS_WITH(
      TestHelpers::test_creation<grmhd::AnalyticData::BondiHoyleAccretion>(
          "BhMass: 0.765\n"
          "BhDimlessSpin: -0.324\n"
          "RestMassDensity: 156.2\n"
          "FlowSpeed: 0.653\n"
          "MagFieldStrength: 1.454\n"
          "PolytropicConstant: -1.52\n"
          "PolytropicExponent: 2.0"),
      Catch::Contains("Value -1.52 is below the lower bound of 0"));
  CHECK_THROWS_WITH(
      TestHelpers::test_creation<grmhd::AnalyticData::BondiHoyleAccretion>(
          "BhMass: 4.21\n"
          "BhDimlessSpin: -0.11\n"
          "RestMassDensity: 0.43\n"
          "FlowSpeed: 0.435\n"
          "MagFieldStrength: 3.44\n"
          "PolytropicConstant: 0.653\n"
          "PolytropicExponent: 0.123"),
      Catch::Contains("Value 0.123 is below the lower bound of 1"));
}
