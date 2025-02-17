// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <optional>
#include <tuple>
#include <utility>

#include "DataStructures/DataBox/DataBox.hpp"
#include "Evolution/Systems/Cce/Actions/BoundaryComputeAndSendToEvolution.hpp"
#include "Parallel/AlgorithmExecution.hpp"
#include "Parallel/GlobalCache.hpp"
#include "Parallel/Invoke.hpp"
#include "Time/Tags.hpp"

namespace Cce {
namespace Actions {

/*!
 * \ingroup ActionsGroup
 * \brief Requests boundary data be sent from `WorldtubeBoundaryComponent` to
 * `EvolutionComponent` (template parameters).
 *
 * \details Calls the simple action
 * `Cce::Actions::BoundaryComputeAndSendToEvolution<WorldtubeBoundaryComponent,
 * EvolutionComponent>` on the `WorldtubeBoundaryComponent`, which performs
 * boundary computations then sends data to the `EvolutionComponent` via
 * `Cce::Actions::ReceiveWorldtubeData`. Requests the current
 * `Tags::TimeStepId`. For the majority of these requests, it's better to issue
 * them as early as possible to maximize the degree of parallelism for the
 * system, so most calls should use `Cce::Actions::RequestNextBoundaryData`,
 * because it can be called the substep prior to when the data will actually be
 * used.
 *
 * Uses:
 *  - DataBox:
 *    - `Tags::TimeStepId`
 *
 * \ref DataBoxGroup changes
 * - Adds: nothing
 * - Removes: nothing
 * - Modifies: nothing
 */
template <typename WorldtubeBoundaryComponent, typename EvolutionComponent>
struct RequestBoundaryData {
  template <typename DbTags, typename... InboxTags, typename Metavariables,
            typename ArrayIndex, typename ActionList,
            typename ParallelComponent>
  static Parallel::iterable_action_return_t apply(
      db::DataBox<DbTags>& box,
      const tuples::TaggedTuple<InboxTags...>& /*inboxes*/,
      Parallel::GlobalCache<Metavariables>& cache,
      const ArrayIndex& /*array_index*/, const ActionList /*meta*/,
      const ParallelComponent* const /*meta*/) {
    Parallel::simple_action<Actions::BoundaryComputeAndSendToEvolution<
        WorldtubeBoundaryComponent, EvolutionComponent>>(
        Parallel::get_parallel_component<WorldtubeBoundaryComponent>(cache),
        db::get<::Tags::TimeStepId>(box));
    return {Parallel::AlgorithmExecution::Continue, std::nullopt};
  }
};

/*!
 * \ingroup ActionsGroup
 * \brief Requests boundary data be sent from `WorldtubeBoundaryComponent` to
 * `EvolutionComponent`.
 *
 * \details Calls the simple action
 * `Cce::Actions::BoundaryComputeAndSendToEvolution<WorldtubeBoundaryComponent,
 * EvolutionComponent>` on the `WorldtubeBoundaryComponent`, which performs
 * boundary computations then sends data to the `EvolutionComponent` via
 * `Cce::Actions::ReceiveWorldtubeData`. Requests the
 * `Tags::Next<Tags::TimeStepId>` (for the next timestep).
 *
 * Uses:
 *  - DataBox:
 *    - `Tags::Next<Tags::TimeStepId>`
 *
 * \ref DataBoxGroup changes
 * - Adds: nothing
 * - Removes: nothing
 * - Modifies: nothing
 */
template <typename WorldtubeBoundaryComponent, typename EvolutionComponent>
struct RequestNextBoundaryData {
  using const_global_cache_tags =
      tmpl::list<typename WorldtubeBoundaryComponent::end_time_tag>;

  template <typename DbTags, typename... InboxTags, typename Metavariables,
            typename ArrayIndex, typename ActionList,
            typename ParallelComponent>
  static Parallel::iterable_action_return_t apply(
      db::DataBox<DbTags>& box,
      const tuples::TaggedTuple<InboxTags...>& /*inboxes*/,
      Parallel::GlobalCache<Metavariables>& cache,
      const ArrayIndex& /*array_index*/, const ActionList /*meta*/,
      const ParallelComponent* const /*meta*/) {
    // only request the data if the next step is not after the end time.
    if (db::get<::Tags::Next<::Tags::TimeStepId>>(box).substep_time().value() <
        db::get<Tags::EndTime>(box)) {
      Parallel::simple_action<Actions::BoundaryComputeAndSendToEvolution<
          WorldtubeBoundaryComponent, EvolutionComponent>>(
          Parallel::get_parallel_component<WorldtubeBoundaryComponent>(cache),
          db::get<::Tags::Next<::Tags::TimeStepId>>(box));
    }
    return {Parallel::AlgorithmExecution::Continue, std::nullopt};
  }
};
}  // namespace Actions
}  // namespace Cce
