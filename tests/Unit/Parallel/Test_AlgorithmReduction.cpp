// Distributed under the MIT License.
// See LICENSE.txt for details.

#define CATCH_CONFIG_RUNNER

#include "Framework/TestingFramework.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "DataStructures/DataBox/DataBox.hpp"  // IWYU pragma: keep
#include "Helpers/Parallel/RoundRobinArrayElements.hpp"
#include "Parallel/Algorithms/AlgorithmArray.hpp"
#include "Parallel/Algorithms/AlgorithmSingleton.hpp"
#include "Parallel/GlobalCache.hpp"
#include "Parallel/InitializationFunctions.hpp"
#include "Parallel/Invoke.hpp"
#include "Parallel/Local.hpp"
#include "Parallel/Main.hpp"
#include "Parallel/ParallelComponentHelpers.hpp"
#include "Parallel/Phase.hpp"
#include "Parallel/PhaseDependentActionList.hpp"  // IWYU pragma: keep
#include "Parallel/Reduction.hpp"
#include "Utilities/ConstantExpressions.hpp"
#include "Utilities/EqualWithinRoundoff.hpp"
#include "Utilities/ErrorHandling/Error.hpp"
#include "Utilities/ErrorHandling/FloatingPointExceptions.hpp"
#include "Utilities/Functional.hpp"
#include "Utilities/MemoryHelpers.hpp"
#include "Utilities/System/ParallelInfo.hpp"
#include "Utilities/TMPL.hpp"

namespace PUP {
class er;
}  // namespace PUP
namespace db {
template <typename TagsList>
class DataBox;
}  // namespace db
struct TestMetavariables;

// The reason we use a 46 element array is that on Wheeler, the SXS
// supercomputer at Caltech, there are 23 worker threads per node and we want to
// be able to test on two nodes to make sure multinode communication is working
// correctly.
static constexpr int number_of_1d_array_elements = 46;

// [reduce_sum_int_action]
struct ProcessReducedSumOfInts {
  template <typename ParallelComponent, typename DbTags, typename Metavariables,
            typename ArrayIndex>
  static void apply(db::DataBox<DbTags>& /*box*/,
                    const Parallel::GlobalCache<Metavariables>& /*cache*/,
                    const ArrayIndex& /*array_index*/, const int& value) {
    SPECTRE_PARALLEL_REQUIRE(number_of_1d_array_elements *
                                 (number_of_1d_array_elements - 1) / 2 ==
                             value);
  }
};
// [reduce_sum_int_action]

// [reduce_rms_action]
struct ProcessErrorNorms {
  template <typename ParallelComponent, typename DbTags, typename Metavariables,
            typename ArrayIndex>
  static void apply(db::DataBox<DbTags>& /*box*/,
                    const Parallel::GlobalCache<Metavariables>& /*cache*/,
                    const ArrayIndex& /*array_index*/, const int points,
                    const double error_u, const double error_v) {
    SPECTRE_PARALLEL_REQUIRE(number_of_1d_array_elements * 3 == points);
    SPECTRE_PARALLEL_REQUIRE(equal_within_roundoff(
        error_u, sqrt(number_of_1d_array_elements * square(1.0e-3) / points)));
    SPECTRE_PARALLEL_REQUIRE(equal_within_roundoff(
        error_v, sqrt(number_of_1d_array_elements * square(1.0e-4) / points)));
  }
};
// [reduce_rms_action]

struct ProcessCustomReductionAction {
  template <typename ParallelComponent, typename DbTags, typename Metavariables,
            typename ArrayIndex>
  static void apply(db::DataBox<DbTags>& /*box*/,
                    const Parallel::GlobalCache<Metavariables>& /*cache*/,
                    const ArrayIndex& /*array_index*/, int reduced_int,
                    std::unordered_map<std::string, int> reduced_map,
                    std::vector<int>&& reduced_vector) {
    SPECTRE_PARALLEL_REQUIRE(reduced_int == 10);
    SPECTRE_PARALLEL_REQUIRE(reduced_map.at("unity") ==
                             number_of_1d_array_elements - 1);
    SPECTRE_PARALLEL_REQUIRE(reduced_map.at("double") ==
                             2 * number_of_1d_array_elements - 2);
    SPECTRE_PARALLEL_REQUIRE(reduced_map.at("negative") == 0);
    SPECTRE_PARALLEL_REQUIRE(
        reduced_vector ==
        (std::vector<int>{-reduced_int * number_of_1d_array_elements *
                              (number_of_1d_array_elements - 1) / 2,
                          -reduced_int * number_of_1d_array_elements * 10,
                          8 * reduced_int * number_of_1d_array_elements}));
  }
};

template <class Metavariables>
struct SingletonParallelComponent {
  using chare_type = Parallel::Algorithms::Singleton;
  using metavariables = Metavariables;
  using phase_dependent_action_list = tmpl::list<
      Parallel::PhaseActions<Parallel::Phase::Initialization, tmpl::list<>>>;
  using initialization_tags = Parallel::get_initialization_tags<
      Parallel::get_initialization_actions_list<phase_dependent_action_list>>;

  static void execute_next_phase(
      const Parallel::Phase /*next_phase*/,
      const Parallel::CProxy_GlobalCache<Metavariables>& /*global_cache*/) {}
};

template <class Metavariables>
struct ArrayParallelComponent;

struct ArrayReduce {
  template <typename ParallelComponent, typename DbTags, typename Metavariables,
            typename ArrayIndex>
  static void apply(const db::DataBox<DbTags>& /*box*/,
                    const Parallel::GlobalCache<Metavariables>& cache,
                    const ArrayIndex& array_index) {
    static_assert(std::is_same_v<ParallelComponent,
                                 ArrayParallelComponent<TestMetavariables>>,
                  "The ParallelComponent is not deduced to be the right type");
    const auto& my_proxy =
        Parallel::get_parallel_component<ArrayParallelComponent<Metavariables>>(
            cache)[array_index];
    const auto& array_proxy =
        Parallel::get_parallel_component<ArrayParallelComponent<Metavariables>>(
            cache);
    const auto& singleton_proxy = Parallel::get_parallel_component<
        SingletonParallelComponent<Metavariables>>(cache);
    // [contribute_to_reduction_example]
    Parallel::ReductionData<Parallel::ReductionDatum<int, funcl::Plus<>>>
        my_send_int{array_index};
    Parallel::contribute_to_reduction<ProcessReducedSumOfInts>(
        my_send_int, my_proxy, singleton_proxy);
    // [contribute_to_reduction_example]
    // [contribute_to_broadcast_reduction]
    Parallel::contribute_to_reduction<ProcessReducedSumOfInts>(
        my_send_int, my_proxy, array_proxy);
    // [contribute_to_broadcast_reduction]

    // [contribute_to_rms_reduction]
    using RmsRed = Parallel::ReductionDatum<double, funcl::Plus<>,
                                            funcl::Sqrt<funcl::Divides<>>,
                                            std::index_sequence<0>>;
    Parallel::ReductionData<Parallel::ReductionDatum<size_t, funcl::Plus<>>,
                            RmsRed, RmsRed>
        error_reduction{3, square(1.0e-3), square(1.0e-4)};
    Parallel::contribute_to_reduction<ProcessErrorNorms>(error_reduction,
                                                         my_proxy, array_proxy);
    // [contribute_to_rms_reduction]

    std::unordered_map<std::string, int> my_send_map;
    my_send_map["unity"] = array_index;
    my_send_map["double"] = 2 * array_index;
    my_send_map["negative"] = -array_index;
    struct {
      int operator()(const int time_state, const int time) {
        if (time_state != time) {
          ERROR("Tried to reduce from different iteration values "
                << time_state << " and " << time);
        }
        return time_state;
      }
    } check_times_equal;
    struct {
      std::unordered_map<std::string, int> operator()(
          std::unordered_map<std::string, int> state,
          const std::unordered_map<std::string, int>& element) {
        for (const auto& string_int : element) {
          if (string_int.second > state.at(string_int.first)) {
            state[string_int.first] = string_int.second;
          }
        }
        return state;
      }
    } map_combine;
    struct {
      std::vector<int> operator()(std::vector<int> state,
                                  const std::vector<int>& element) {
        for (size_t i = 0; i < state.size(); ++i) {
          state[i] += element[i];
        }
        return state;
      }

    } vector_combine;
    struct {
      std::vector<int> operator()(std::vector<int> data,
                                  const int& first_reduction) {
        std::transform(data.begin(), data.end(), data.begin(),
                       [&first_reduction](const int t) {
                         return -1 * t * first_reduction;
                       });
        return data;
      }
    } vector_finalize;
    using ReductionType = Parallel::ReductionData<
        Parallel::ReductionDatum<int, decltype(check_times_equal)>,
        Parallel::ReductionDatum<std::unordered_map<std::string, int>,
                                 decltype(map_combine)>,
        Parallel::ReductionDatum<std::vector<int>, decltype(vector_combine),
                                 decltype(vector_finalize),
                                 std::index_sequence<0>>>;
    Parallel::contribute_to_reduction<ProcessCustomReductionAction>(
        ReductionType{10, my_send_map, std::vector<int>{array_index, 10, -8}},
        my_proxy, singleton_proxy);
    Parallel::contribute_to_reduction<ProcessCustomReductionAction>(
        ReductionType{10, my_send_map, std::vector<int>{array_index, 10, -8}},
        my_proxy, array_proxy);
  }
};

template <class Metavariables>
struct ArrayParallelComponent {
  using chare_type = Parallel::Algorithms::Array;
  using metavariables = Metavariables;
  using array_index = int;
  using phase_dependent_action_list = tmpl::list<
      Parallel::PhaseActions<Parallel::Phase::Initialization, tmpl::list<>>>;
  using initialization_tags = Parallel::get_initialization_tags<
      Parallel::get_initialization_actions_list<phase_dependent_action_list>>;

  static void allocate_array(
      Parallel::CProxy_GlobalCache<Metavariables>& global_cache,
      const tuples::tagged_tuple_from_typelist<initialization_tags>&
      /*initialization_items*/,
      const std::unordered_set<size_t>& procs_to_ignore = {}) {
    auto& local_cache = *Parallel::local_branch(global_cache);
    auto& array_proxy =
        Parallel::get_parallel_component<ArrayParallelComponent>(local_cache);

    TestHelpers::Parallel::assign_array_elements_round_robin_style(
        array_proxy, static_cast<size_t>(number_of_1d_array_elements),
        static_cast<size_t>(sys::number_of_procs()), {}, global_cache,
        procs_to_ignore);
  }

  static void execute_next_phase(
      const Parallel::Phase next_phase,
      Parallel::CProxy_GlobalCache<Metavariables>& global_cache) {
    auto& local_cache = *Parallel::local_branch(global_cache);
    if (next_phase == Parallel::Phase::Testing) {
      Parallel::simple_action<ArrayReduce>(
          Parallel::get_parallel_component<ArrayParallelComponent>(
              local_cache));
    }
  }
};

struct TestMetavariables {
  using component_list =
      tmpl::list<SingletonParallelComponent<TestMetavariables>,
                 ArrayParallelComponent<TestMetavariables>>;

  static constexpr const char* const help{"Test reductions using Algorithm"};
  static constexpr bool ignore_unrecognized_command_line_options = false;

  static constexpr std::array<Parallel::Phase, 3> default_phase_order{
      {Parallel::Phase::Initialization, Parallel::Phase::Testing,
       Parallel::Phase::Exit}};

  // NOLINTNEXTLINE(google-runtime-references)
  void pup(PUP::er& /*p*/) {}
};

static const std::vector<void (*)()> charm_init_node_funcs{
    &setup_error_handling, &setup_memory_allocation_failure_reporting};
static const std::vector<void (*)()> charm_init_proc_funcs{
    &enable_floating_point_exceptions};

using charmxx_main_component = Parallel::Main<TestMetavariables>;

#include "Parallel/CharmMain.tpp"  // IWYU pragma: keep
