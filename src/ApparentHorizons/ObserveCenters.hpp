// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <tuple>

#include "DataStructures/DataBox/DataBox.hpp"
#include "IO/Observer/ObserverComponent.hpp"
#include "IO/Observer/ReductionActions.hpp"
#include "Parallel/GlobalCache.hpp"
#include "Parallel/Invoke.hpp"
#include "Parallel/ParallelComponentHelpers.hpp"
#include "ParallelAlgorithms/Interpolation/InterpolationTargetDetail.hpp"
#include "Utilities/PrettyType.hpp"

/// \cond
namespace StrahlkorperTags {
template <typename Frame>
struct Strahlkorper;
}  // namespace StrahlkorperTags
namespace Frame {
struct Grid;
struct Inertial;
}  // namespace Frame
/// \endcond

namespace ah {
namespace callbacks {
/*!
 * \brief Writes the center of an apparent horizon to disk in both the
 * Frame::Grid frame and Frame::Inertial frame. Intended to be used in the
 * `post_horizon_find_callbacks` list of an InterpolationTargetTag.
 *
 * The centers will be written to a subfile with the name
 * `/ApparentHorizons/TargetName_Centers.dat` where `TargetName` is the
 * pretty_type::name of the InterpolationTargetTag template parameter.
 *
 * The columns of the dat file are:
 * - %Time
 * - GridCenter_x
 * - GridCenter_y
 * - GridCenter_z
 * - InertialCenter_x
 * - InertialCenter_y
 * - InertialCenter_z
 *
 * \note Requires StrahlkorperTags::Strahlkorper<Frame::Grid> and
 * StrahlkorperTags::Strahlkorper<Frame::Inertial> be in the DataBox of the
 * InterpolationTarget.
 */
template <typename InterpolationTargetTag>
struct ObserveCenters {
  template <typename DbTags, typename Metavariables, typename TemporalId>
  static void apply(const db::DataBox<DbTags>& box,
                    Parallel::GlobalCache<Metavariables>& cache,
                    const TemporalId& temporal_id) {
    using GridTag = StrahlkorperTags::Strahlkorper<Frame::Grid>;
    using InertialTag = StrahlkorperTags::Strahlkorper<Frame::Inertial>;

    const auto& grid_horizon = db::get<GridTag>(box);
    const auto& inertial_horizon = db::get<InertialTag>(box);

    const std::array<double, 3> grid_center = grid_horizon.physical_center();
    const std::array<double, 3> inertial_center =
        inertial_horizon.physical_center();

    // time, grid_x, grid_y, grid_z, inertial_x, inertial_y, inertial_z
    const auto center_tuple = std::make_tuple(
        intrp::InterpolationTarget_detail::get_temporal_id_value(temporal_id),
        grid_center[0], grid_center[1], grid_center[2], inertial_center[0],
        inertial_center[1], inertial_center[2]);

    auto& observer_writer_proxy = Parallel::get_parallel_component<
        observers::ObserverWriter<Metavariables>>(cache);

    const std::string subfile_path{"/ApparentHorizons/" +
                                   pretty_type::name<InterpolationTargetTag>() +
                                   "_Centers"};

    Parallel::threaded_action<
        observers::ThreadedActions::WriteReductionDataRow>(
        // Node 0 is always the writer
        observer_writer_proxy[0], subfile_path, legend_, center_tuple);
  }

 private:
  const static inline std::vector<std::string> legend_{
      {"Time", "GridCenter_x", "GridCenter_y", "GridCenter_z",
       "InertialCenter_x", "InertialCenter_y", "InertialCenter_z"}};
};
}  // namespace callbacks
}  // namespace ah
