// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "ApparentHorizons/ObjectLabel.hpp"
#include "ControlSystem/ApparentHorizons/Measurements.hpp"
#include "ControlSystem/Averager.hpp"
#include "ControlSystem/Component.hpp"
#include "ControlSystem/ControlErrors/Expansion.hpp"
#include "ControlSystem/ControlErrors/Rotation.hpp"
#include "ControlSystem/ControlErrors/Translation.hpp"
#include "ControlSystem/Controller.hpp"
#include "ControlSystem/DataVectorHelpers.hpp"
#include "ControlSystem/Systems/Expansion.hpp"
#include "ControlSystem/Systems/Rotation.hpp"
#include "ControlSystem/Systems/Translation.hpp"
#include "ControlSystem/Tags.hpp"
#include "ControlSystem/Tags/MeasurementTimescales.hpp"
#include "ControlSystem/TimescaleTuner.hpp"
#include "ControlSystem/UpdateControlSystem.hpp"
#include "DataStructures/DataBox/Tag.hpp"
#include "DataStructures/DataVector.hpp"
#include "DataStructures/LinkedMessageId.hpp"
#include "DataStructures/LinkedMessageQueue.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "Domain/Domain.hpp"
#include "Domain/FunctionsOfTime/FixedSpeedCubic.hpp"
#include "Domain/FunctionsOfTime/FunctionOfTime.hpp"
#include "Domain/FunctionsOfTime/PiecewisePolynomial.hpp"
#include "Domain/FunctionsOfTime/QuaternionFunctionOfTime.hpp"
#include "Domain/FunctionsOfTime/RegisterDerivedWithCharm.hpp"
#include "Domain/FunctionsOfTime/Tags.hpp"
#include "Domain/Structure/Direction.hpp"
#include "Domain/Tags.hpp"
#include "Framework/ActionTesting.hpp"
#include "Framework/TestCreation.hpp"
#include "Framework/TestingFramework.hpp"
#include "IO/Observer/ObserverComponent.hpp"
#include "IO/Observer/Tags.hpp"
#include "Options/ParseOptions.hpp"
#include "Parallel/CreateFromOptions.hpp"
#include "Parallel/GlobalCache.hpp"
#include "Parallel/ParallelComponentHelpers.hpp"
#include "Parallel/Phase.hpp"
#include "Parallel/PhaseDependentActionList.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/MakeArray.hpp"
#include "Utilities/StdArrayHelpers.hpp"
#include "Utilities/TMPL.hpp"
#include "Utilities/TaggedTuple.hpp"

/// \cond
namespace OptionTags {
struct InitialTime;
}  // namespace OptionTags
/// \endcond

namespace control_system::TestHelpers {
template <typename ControlSystem>
using init_simple_tags =
    tmpl::list<control_system::Tags::Averager<ControlSystem>,
               control_system::Tags::TimescaleTuner<ControlSystem>,
               control_system::Tags::Controller<ControlSystem>,
               control_system::Tags::ControlError<ControlSystem>,
               control_system::Tags::WriteDataToDisk,
               control_system::Tags::IsActive<ControlSystem>,
               typename ControlSystem::MeasurementQueue>;

template <typename Metavariables, typename ControlSystem>
struct MockControlComponent {
  using array_index = int;
  using component_being_mocked = ControlComponent<Metavariables, ControlSystem>;
  using metavariables = Metavariables;
  using chare_type = ActionTesting::MockSingletonChare;

  using system = ControlSystem;

  using simple_tags = init_simple_tags<ControlSystem>;

  using phase_dependent_action_list = tmpl::list<Parallel::PhaseActions<
      Parallel::Phase::Initialization,
      tmpl::list<ActionTesting::InitializeDataBox<simple_tags>>>>;
};

template <typename Metavars>
struct MockElementComponent {
  using array_index = int;
  using chare_type = ActionTesting::MockArrayChare;

  using metavariables = Metavars;

  using simple_tags = tmpl::list<>;

  using const_global_cache_tags =
      tmpl::list<domain::Tags::Domain<Metavars::volume_dim>>;

  using mutable_global_cache_tags =
      tmpl::list<domain::Tags::FunctionsOfTimeInitialize,
                 control_system::Tags::MeasurementTimescales>;

  using phase_dependent_action_list = tmpl::list<
      Parallel::PhaseActions<Parallel::Phase::Initialization, tmpl::list<>>>;
};

template <typename Metavars>
struct MockObserverWriter {
  using component_being_mocked = observers::ObserverWriter<Metavars>;
  using replace_these_simple_actions = tmpl::list<>;
  using with_these_simple_actions = tmpl::list<>;

  using const_global_cache_tags =
      tmpl::list<observers::Tags::ReductionFileName>;

  using initialization_tags = tmpl::list<>;
  using simple_tags = tmpl::list<observers::Tags::H5FileLock>;

  using metavariables = Metavars;
  using chare_type = ActionTesting::MockNodeGroupChare;
  using array_index = int;

  using phase_dependent_action_list = tmpl::list<Parallel::PhaseActions<
      Parallel::Phase::Initialization,
      tmpl::list<ActionTesting::InitializeDataBox<simple_tags>>>>;
};

template <size_t TranslationDerivOrder, size_t RotationDerivOrder,
          size_t ExpansionDerivOrder>
struct MockMetavars {
  static constexpr size_t volume_dim = 3;

  using metavars = MockMetavars<TranslationDerivOrder, RotationDerivOrder,
                                ExpansionDerivOrder>;

  using observed_reduction_data_tags = tmpl::list<>;

  static constexpr bool using_expansion = ExpansionDerivOrder != 0;
  static constexpr bool using_rotation = RotationDerivOrder != 0;
  static constexpr bool using_translation = TranslationDerivOrder != 0;

  // Even if we aren't using certain control systems, we still need valid deriv
  // orders because everything is constructed by default in the SystemHelper.
  // The bool above just determines if the functions of time are actually
  // created or not because that's what matters
  static constexpr size_t exp_deriv_order =
      using_expansion ? ExpansionDerivOrder : 2;
  static constexpr size_t rot_deriv_order =
      using_rotation ? RotationDerivOrder : 2;
  static constexpr size_t trans_deriv_order =
      using_translation ? TranslationDerivOrder : 2;

  using element_component = MockElementComponent<metavars>;

  using expansion_system = control_system::Systems::Expansion<exp_deriv_order>;
  using rotation_system = control_system::Systems::Rotation<rot_deriv_order>;
  using translation_system =
      control_system::Systems::Translation<trans_deriv_order>;

  using control_systems = tmpl::flatten<tmpl::list<
      tmpl::conditional_t<using_expansion, expansion_system, tmpl::list<>>,
      tmpl::conditional_t<using_rotation, rotation_system, tmpl::list<>>,
      tmpl::conditional_t<using_translation, translation_system,
                          tmpl::list<>>>>;

  using expansion_component = MockControlComponent<metavars, expansion_system>;
  using rotation_component = MockControlComponent<metavars, rotation_system>;
  using translation_component =
      MockControlComponent<metavars, translation_system>;

  using observer_component = MockObserverWriter<metavars>;

  using control_components = tmpl::flatten<tmpl::list<
      tmpl::conditional_t<using_expansion, expansion_component, tmpl::list<>>,
      tmpl::conditional_t<using_rotation, rotation_component, tmpl::list<>>,
      tmpl::conditional_t<using_translation, translation_component,
                          tmpl::list<>>>>;

  using component_list = tmpl::flatten<
      tmpl::list<observer_component, element_component, control_components>>;
};

template <typename ExpansionSystem>
double initialize_expansion_functions_of_time(
    const gsl::not_null<std::unordered_map<
        std::string, std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>*>
        functions_of_time,
    const double initial_time,
    const std::unordered_map<std::string, double>& initial_expiration_times) {
  constexpr size_t deriv_order = ExpansionSystem::deriv_order;
  const double initial_expansion = 1.0;
  const double expansion_velocity_outer_boundary = 0.0;
  const double decay_timescale_outer_boundary = 0.05;
  auto init_func_expansion =
      make_array<deriv_order + 1, DataVector>(DataVector{1, 0.0});
  init_func_expansion[0][0] = initial_expansion;

  const std::string expansion_name = ExpansionSystem::name();
  (*functions_of_time)[expansion_name] = std::make_unique<
      domain::FunctionsOfTime::PiecewisePolynomial<deriv_order>>(
      initial_time, init_func_expansion,
      initial_expiration_times.at(expansion_name));
  (*functions_of_time)[expansion_name + "OuterBoundary"s] =
      std::make_unique<domain::FunctionsOfTime::FixedSpeedCubic>(
          initial_expansion, initial_time, expansion_velocity_outer_boundary,
          decay_timescale_outer_boundary);

  return 1.0;
}

template <typename RotationSystem>
double initialize_rotation_functions_of_time(
    const gsl::not_null<std::unordered_map<
        std::string, std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>*>
        functions_of_time,
    const double initial_time,
    const std::unordered_map<std::string, double>& initial_expiration_times) {
  constexpr size_t deriv_order = RotationSystem::deriv_order;

  const double initial_omega_z = 0.01;
  auto init_func_rotation =
      make_array<deriv_order + 1, DataVector>(DataVector{3, 0.0});
  init_func_rotation[1][2] = initial_omega_z;
  auto init_quaternion = make_array<1, DataVector>(DataVector{4, 0.0});
  init_quaternion[0][0] = 1.0;

  const std::string rotation_name = RotationSystem::name();
  (*functions_of_time)[rotation_name] = std::make_unique<
      domain::FunctionsOfTime::QuaternionFunctionOfTime<deriv_order>>(
      initial_time, init_quaternion, init_func_rotation,
      initial_expiration_times.at(rotation_name));

  return 1.0;
}

template <typename TranslationSystem>
double initialize_translation_functions_of_time(
    const gsl::not_null<std::unordered_map<
        std::string, std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>*>
        functions_of_time,
    const double initial_time,
    const std::unordered_map<std::string, double>& initial_expiration_times) {
  constexpr size_t deriv_order = TranslationSystem::deriv_order;

  auto init_func_translation =
      make_array<deriv_order + 1, DataVector>(DataVector{3, 0.0});

  const std::string translation_name = TranslationSystem::name();
  (*functions_of_time)[translation_name] = std::make_unique<
      domain::FunctionsOfTime::PiecewisePolynomial<deriv_order>>(
      initial_time, init_func_translation,
      initial_expiration_times.at(translation_name));

  if (initial_expiration_times.count("Rotation") == 0) {
    auto local_init_func_rotation =
        make_array<1, DataVector>(DataVector{4, 0.0});
    local_init_func_rotation[0][0] = 1.0;
    (*functions_of_time)["Rotation"] =
        std::make_unique<domain::FunctionsOfTime::PiecewisePolynomial<0>>(
            initial_time, local_init_func_rotation,
            std::numeric_limits<double>::infinity());
  }
  if (initial_expiration_times.count("Expansion") == 0) {
    auto local_init_func_expansion =
        make_array<1, DataVector>(DataVector{1, 0.0});
    local_init_func_expansion[0][0] = 1.0;
    (*functions_of_time)["Expansion"] =
        std::make_unique<domain::FunctionsOfTime::PiecewisePolynomial<0>>(
            initial_time, local_init_func_expansion,
            std::numeric_limits<double>::infinity());
  }

  return 1.0;
}

template <typename ElementComponent, typename Metavars, typename F,
          typename CoordMap>
std::pair<std::array<double, 3>, std::array<double, 3>>
grid_frame_horizon_centers_for_basic_control_systems(
    const double time, ActionTesting::MockRuntimeSystem<Metavars>& runner,
    const F position_function, const CoordMap& coord_map) {
  auto& cache = ActionTesting::cache<ElementComponent>(runner, 0);
  const auto& functions_of_time =
      Parallel::get<domain::Tags::FunctionsOfTime>(cache);

  // This whole switching between tensors and arrays is annoying and
  // clunky, but it's the best that could be done at the moment without
  // changing BinaryTrajectories to return tensors, which doesn't seem like
  // a good idea.

  std::pair<std::array<double, 3>, std::array<double, 3>> positions =
      position_function(time);

  // Covert arrays to tensor so we can pass them into the coordinate map
  const tnsr::I<double, 3, Frame::Inertial> inertial_position_of_a(
      positions.first);
  const tnsr::I<double, 3, Frame::Inertial> inertial_position_of_b(
      positions.second);

  // Convert to "grid coordinates"
  const auto grid_position_of_a_tnsr =
      *coord_map.inverse(inertial_position_of_a, time, functions_of_time);
  const auto grid_position_of_b_tnsr =
      *coord_map.inverse(inertial_position_of_b, time, functions_of_time);

  // Convert tensors back to arrays so we can pass them to the control
  // systems. Just reuse `positions`
  for (size_t i = 0; i < 3; i++) {
    gsl::at(positions.first, i) = grid_position_of_a_tnsr.get(i);
    gsl::at(positions.second, i) = grid_position_of_b_tnsr.get(i);
  }

  return positions;
}

template <typename ElementComponent, typename Metavars, typename F,
          typename CoordMap>
std::pair<Strahlkorper<Frame::Grid>, Strahlkorper<Frame::Grid>>
build_horizons_for_basic_control_systems(
    const double time, ActionTesting::MockRuntimeSystem<Metavars>& runner,
    const F position_function, const CoordMap& coord_map) {
  const auto positions =
      grid_frame_horizon_centers_for_basic_control_systems<ElementComponent>(
          time, runner, position_function, coord_map);

  // Construct strahlkorpers to pass to control systems. Only the centers
  // matter.
  Strahlkorper<Frame::Grid> horizon_a{2, 2, 1.0, positions.first};
  Strahlkorper<Frame::Grid> horizon_b{2, 2, 1.0, positions.second};

  return std::make_pair<Strahlkorper<Frame::Grid>, Strahlkorper<Frame::Grid>>(
      std::move(horizon_a), std::move(horizon_b));
}

/*!
 * \brief Helper struct for testing basic control systems
 *
 * To signify which control systems you want, set the corresponding
 * DerivOrder. To turn control systems off, put 0 for their DerivOrder in the
 * templates of the metavariables.
 *
 * Ideally we'd construct the runner here and just pass that to the test to
 * simplify as must of the work as possible, but MockRuntimeSystems aren't
 * copy- or move-able so we have to make the necessary info available. The
 * simplest way to do this was to have functions that return references to the
 * member variables.
 *
 * \note Translation control isn't supported yet. It will be added in the
 * future.
 */
template <typename Metavars>
struct SystemHelper {
 private:
  template <typename System>
  struct LocalTag {
    using type = tuples::tagged_tuple_from_typelist<init_simple_tags<System>>;
  };
  using AllTags = tuples::tagged_tuple_from_typelist<tmpl::transform<
      typename Metavars::control_systems, tmpl::bind<LocalTag, tmpl::_1>>>;

 public:
  using control_systems = typename Metavars::control_systems;
  using element_component = typename Metavars::element_component;
  using control_components = typename Metavars::control_components;

  // Members that may be moved out of this struct once they are
  // constructed
  auto& domain() { return domain_; }
  auto& initial_functions_of_time() { return initial_functions_of_time_; }
  auto& initial_measurement_timescales() {
    return initial_measurement_timescales_;
  }

  // Members that won't be moved out of this struct
  template <typename System>
  const auto& init_tuple() {
    return get<LocalTag<System>>(all_init_tags_);
  }
  const auto& horizon_a() { return horizon_a_; }
  const auto& horizon_b() { return horizon_b_; }
  template <typename System>
  std::string name() {
    return System::name();
  }

  /*!
   * \brief Setup the test.
   *
   * The function `initialize_functions_of_time` must take a
   * `const gsl::not_null<std::unordered_map<std::string,
   * std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>*>` for the
   * function of time map, a `const double` for the initial time, and a `const
   * std::unordered_map<std::string, double>&` for the expiration times. This
   * function will initialize the functions of time and must return a double
   * that represents the radius of the excision spheres. Some existing functions
   * are given in the `control_system::TestHelpers` namespace for Expansion,
   * Rotation, and Translation.
   */
  template <typename F>
  void setup_control_system_test(const double initial_time,
                                 const double initial_separation,
                                 const std::string& option_string,
                                 const F initialize_functions_of_time) {
    initial_time_ = initial_time;

    parse_options(option_string);
    // Initial parameters needed. Expiration times would normally be set during
    // option parsing, and measurement timescales during initialization so we
    // have to do them manually here instead.
    std::unordered_map<std::string, double> initial_expiration_times{};
    tmpl::for_each<control_systems>([this,
                                     &initial_expiration_times](auto system_v) {
      using system = tmpl::type_from<decltype(system_v)>;

      auto& init_tuple = get<LocalTag<system>>(all_init_tags_);
      auto& averager = get<control_system::Tags::Averager<system>>(init_tuple);
      const auto& controller =
          get<control_system::Tags::Controller<system>>(init_tuple);
      const auto& tuner =
          get<control_system::Tags::TimescaleTuner<system>>(init_tuple);

      const std::array<DataVector, 1> measurement_timescale{
          {control_system::calculate_measurement_timescales(controller,
                                                            tuner)}};
      averager.assign_time_between_measurements(min(measurement_timescale[0]));

      const double initial_expiration_time =
          controller.get_update_fraction() * min(tuner.current_timescale());

      initial_measurement_timescales_[name<system>()] =
          std::make_unique<domain::FunctionsOfTime::PiecewisePolynomial<0>>(
              initial_time_, measurement_timescale, initial_expiration_time);
      initial_expiration_times[name<system>()] = initial_expiration_time;
    });

    const double excision_radius =
        initialize_functions_of_time(make_not_null(&initial_functions_of_time_),
                                     initial_time_, initial_expiration_times);

    // We don't need a real domain, just one that has the correct excision
    // sphere centers because the control errors use the `excision_spheres()`
    // member of a domain to get the centers. The names are chosen to match the
    // BinaryCompactObject domain, which the control errors were based on and
    // have these specific names hard-coded into them.
    domain_ =
        Domain<3>{{},
                  {},
                  {{"ObjectAExcisionSphere",
                    ExcisionSphere<3>{excision_radius,
                                      {{-0.5 * initial_separation, 0.0, 0.0}},
                                      {{0, Direction<3>::lower_zeta()},
                                       {1, Direction<3>::lower_zeta()},
                                       {2, Direction<3>::lower_zeta()},
                                       {3, Direction<3>::lower_zeta()},
                                       {4, Direction<3>::lower_zeta()},
                                       {5, Direction<3>::lower_zeta()}}}},
                   {"ObjectBExcisionSphere",
                    ExcisionSphere<3>{excision_radius,
                                      {{+0.5 * initial_separation, 0.0, 0.0}},
                                      {{0, Direction<3>::lower_zeta()},
                                       {1, Direction<3>::lower_zeta()},
                                       {2, Direction<3>::lower_zeta()},
                                       {3, Direction<3>::lower_zeta()},
                                       {4, Direction<3>::lower_zeta()},
                                       {5, Direction<3>::lower_zeta()}}}}}};
  }

  /*!
   * \brief Actually run the control system test
   *
   * The `horizon_function` should return a
   * `std::pair<Strahlkorper<Frame::Grid>, Strahlkorper<Frame::Grid>>`
   * representing the two horizons in the grid frame. This means the user is
   * responsible for doing any coordinate transformations inside
   * `horizon_function` as this function won't do any. The `number_of_horizons`
   * is used to determine if we actually use both horizon "measurements" as some
   * control systems may only need one. If only one horizon is used, the default
   * is to use AhA.
   *
   * For the basic control systems, a common function is defined for you:
   * `control_system::TestHelpers::build_horizons_for_basic_control_systems()`.
   */
  template <typename Generator, typename F>
  void run_control_system_test(
      ActionTesting::MockRuntimeSystem<Metavars>& runner,
      const double final_time, gsl::not_null<Generator*> generator,
      const F horizon_function, const size_t number_of_horizons) {
    double time = initial_time_;
    std::optional<double> prev_time{};
    double dt = 0.0;

    auto& cache = ActionTesting::cache<element_component>(runner, 0);
    const auto& measurement_timescales =
        Parallel::get<control_system::Tags::MeasurementTimescales>(cache);

    // Start loop
    while (time < final_time) {
      // Setup the measurement Id. This would normally be created in the control
      // system event.
      const LinkedMessageId<double> measurement_id{time, prev_time};

      // Get horizons in grid frame
      std::tie(horizon_a_, horizon_b_) = horizon_function(time);

      // Apply measurements
      tmpl::for_each<control_components>([this, &runner, &generator,
                                          &measurement_id, &cache,
                                          &number_of_horizons](
                                             auto control_component) {
        using component = tmpl::type_from<decltype(control_component)>;
        using system = typename component::system;
        // Even if we only have 1 horizon, we still apply both measurements
        // because the BothHorizons measurement will always send both regardless
        // of if both are needed.
        system::process_measurement::apply(
            ah::BothHorizons::FindHorizon<::ah::ObjectLabel::A>{}, horizon_a_,
            cache, measurement_id);
        system::process_measurement::apply(
            ah::BothHorizons::FindHorizon<::ah::ObjectLabel::B>{}, horizon_b_,
            cache, measurement_id);
        CHECK(ActionTesting::number_of_queued_simple_actions<component>(
                  runner, 0) == number_of_horizons);

        if (number_of_horizons > 1) {
          // We invoke a random measurement because during a normal simulation
          // we don't know which measurement will reach the control system
          // first because of charm++ communication
          ActionTesting::invoke_random_queued_simple_action<control_components>(
              make_not_null(&runner), generator,
              ActionTesting::array_indices_with_queued_simple_actions<
                  control_components>(make_not_null(&runner)));
        }
        ActionTesting::invoke_queued_simple_action<component>(
            make_not_null(&runner), 0);
      });

      // At this point, the control systems for each transformation should have
      // done their thing and updated the functions of time (if they had enough
      // data).

      // Our dt is set by the smallest measurement timescale. The control system
      // updates these timescales when it updates the functions of time
      prev_time = time;
      dt = std::numeric_limits<double>::max();
      for (auto& [name, measurement_timescale] : measurement_timescales) {
        // Avoid compiler warning with gcc-7
        (void)name;
        dt = std::min(dt, min(measurement_timescale->func(time)[0]));
      }
      time += dt;
    }

    // Get horizons at final time in grid frame
    std::tie(horizon_a_, horizon_b_) = horizon_function(time);
  }

 private:
  template <typename Component>
  using option_tag = control_system::OptionTags::ControlSystemInputs<
      typename Component::system>;
  using option_list = tmpl::push_back<
      tmpl::remove_duplicates<tmpl::transform<
          control_components, tmpl::bind<option_tag, tmpl::_1>>>,
      control_system::OptionTags::WriteDataToDisk, ::OptionTags::InitialTime>;
  template <typename System>
  using creatable_tags =
      tmpl::list_difference<init_simple_tags<System>,
                            tmpl::list<typename System::MeasurementQueue>>;

  void parse_options(const std::string& option_string) {
    Options::Parser<option_list> parser{"Peter Parker the option parser."};
    parser.parse(option_string);
    const tuples::tagged_tuple_from_typelist<option_list> options =
        parser.template apply<option_list, Metavars>([](auto... args) {
          return tuples::tagged_tuple_from_typelist<option_list>(
              std::move(args)...);
        });

    tmpl::for_each<control_systems>([this, options](auto system_v) {
      using system = tmpl::type_from<decltype(system_v)>;

      tuples::tagged_tuple_from_typelist<creatable_tags<system>> created_tags =
          Parallel::create_from_options<Metavars>(options,
                                                  creatable_tags<system>{});

      get<LocalTag<system>>(all_init_tags_) =
          tuples::tagged_tuple_from_typelist<init_simple_tags<system>>{
              get<control_system::Tags::Averager<system>>(created_tags),
              get<control_system::Tags::TimescaleTuner<system>>(created_tags),
              get<control_system::Tags::Controller<system>>(created_tags),
              get<control_system::Tags::ControlError<system>>(created_tags),
              get<control_system::Tags::WriteDataToDisk>(created_tags), true,
              // Just need an empty queue. It will get filled in as the control
              // system is updated
              LinkedMessageQueue<
                  double,
                  tmpl::list<QueueTags::Center<::ah::ObjectLabel::A>,
                             QueueTags::Center<::ah::ObjectLabel::B>>>{}};
    });
  }

  // Members that may be moved out of this struct once they are
  // constructed
  Domain<3> domain_;
  std::unordered_map<std::string,
                     std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>
      initial_functions_of_time_{};
  std::unordered_map<std::string,
                     std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>
      initial_measurement_timescales_{};

  // Members that won't be moved out of this struct
  AllTags all_init_tags_{};
  Strahlkorper<Frame::Grid> horizon_a_{};
  Strahlkorper<Frame::Grid> horizon_b_{};
  double initial_time_{std::numeric_limits<double>::signaling_NaN()};
};
}  // namespace control_system::TestHelpers
