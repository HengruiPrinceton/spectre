// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <memory>
#include <optional>
#include <pup.h>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "DataStructures/DataBox/Prefixes.hpp"
#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "DataStructures/Variables.hpp"
#include "Domain/CoordinateMaps/CoordinateMap.hpp"
#include "Domain/CoordinateMaps/Tags.hpp"
#include "Domain/ElementMap.hpp"
#include "Domain/FunctionsOfTime/FunctionOfTime.hpp"
#include "Domain/FunctionsOfTime/Tags.hpp"
#include "Domain/Structure/Direction.hpp"
#include "Domain/Structure/DirectionMap.hpp"
#include "Domain/Tags.hpp"
#include "Evolution/BoundaryConditions/Type.hpp"
#include "Evolution/DgSubcell/GhostZoneLogicalCoordinates.hpp"
#include "Evolution/DgSubcell/SliceData.hpp"
#include "Evolution/DgSubcell/SliceTensor.hpp"
#include "Evolution/DgSubcell/Tags/Coordinates.hpp"
#include "Evolution/DgSubcell/Tags/Mesh.hpp"
#include "Evolution/Systems/Burgers/BoundaryConditions/BoundaryCondition.hpp"
#include "Evolution/Systems/Burgers/FiniteDifference/Factory.hpp"
#include "Evolution/Systems/Burgers/FiniteDifference/Reconstructor.hpp"
#include "Evolution/Systems/Burgers/FiniteDifference/Tags.hpp"
#include "Evolution/Systems/Burgers/Tags.hpp"
#include "Evolution/TypeTraits.hpp"
#include "NumericalAlgorithms/Spectral/Mesh.hpp"
#include "Options/Options.hpp"
#include "Parallel/CharmPupable.hpp"
#include "PointwiseFunctions/AnalyticData/Tags.hpp"
#include "PointwiseFunctions/AnalyticSolutions/AnalyticSolution.hpp"
#include "Time/Tags.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/TMPL.hpp"

namespace Burgers::BoundaryConditions {
/*!
 * \brief Sets Dirichlet boundary conditions using the analytic solution or
 * analytic data.
 */
class DirichletAnalytic final : public BoundaryCondition {
 private:
  using flux_tag =
      ::Tags::Flux<Burgers::Tags::U, tmpl::size_t<1>, Frame::Inertial>;

 public:
  using options = tmpl::list<>;
  static constexpr Options::String help{
      "DirichletAnalytic boundary conditions setting the value of U to "
      "the analytic solution or analytic data."};

  DirichletAnalytic() = default;
  DirichletAnalytic(DirichletAnalytic&&) = default;
  DirichletAnalytic& operator=(DirichletAnalytic&&) = default;
  DirichletAnalytic(const DirichletAnalytic&) = default;
  DirichletAnalytic& operator=(const DirichletAnalytic&) = default;
  ~DirichletAnalytic() override = default;

  explicit DirichletAnalytic(CkMigrateMessage* msg);

  WRAPPED_PUPable_decl_base_template(
      domain::BoundaryConditions::BoundaryCondition, DirichletAnalytic);

  auto get_clone() const -> std::unique_ptr<
      domain::BoundaryConditions::BoundaryCondition> override;

  static constexpr evolution::BoundaryConditions::Type bc_type =
      evolution::BoundaryConditions::Type::Ghost;

  void pup(PUP::er& p) override;

  using dg_interior_evolved_variables_tags = tmpl::list<>;
  using dg_interior_temporary_tags =
      tmpl::list<domain::Tags::Coordinates<1, Frame::Inertial>>;
  using dg_gridless_tags =
      tmpl::list<::Tags::Time, ::Tags::AnalyticSolutionOrData>;

  template <typename AnalyticSolutionOrData>
  std::optional<std::string> dg_ghost(
      const gsl::not_null<Scalar<DataVector>*> u,
      const gsl::not_null<tnsr::I<DataVector, 1, Frame::Inertial>*> flux_u,
      const std::optional<
          tnsr::I<DataVector, 1, Frame::Inertial>>& /*face_mesh_velocity*/,
      const tnsr::i<DataVector, 1, Frame::Inertial>& /*normal_covector*/,
      const tnsr::I<DataVector, 1, Frame::Inertial>& coords,
      [[maybe_unused]] const double time,
      const AnalyticSolutionOrData& analytic_solution_or_data) const {
    if constexpr (is_analytic_solution_v<AnalyticSolutionOrData>) {
      *u = get<Burgers::Tags::U>(analytic_solution_or_data.variables(
          coords, time, tmpl::list<Burgers::Tags::U>{}));
    } else {
      *u = get<Burgers::Tags::U>(analytic_solution_or_data.variables(
          coords, tmpl::list<Burgers::Tags::U>{}));
    }
    flux_impl(flux_u, *u);
    return {};
  }

  using fd_interior_evolved_variables_tags = tmpl::list<>;
  using fd_interior_temporary_tags =
      tmpl::list<evolution::dg::subcell::Tags::Mesh<1>>;
  using fd_gridless_tags =
      tmpl::list<::Tags::Time, domain::Tags::FunctionsOfTime,
                 domain::Tags::ElementMap<1, Frame::Grid>,
                 domain::CoordinateMaps::Tags::CoordinateMap<1, Frame::Grid,
                                                             Frame::Inertial>,
                 fd::Tags::Reconstructor, ::Tags::AnalyticSolutionOrData>;

  template <typename AnalyticSolutionOrData>
  void fd_ghost(const gsl::not_null<Scalar<DataVector>*> u,
                const Direction<1>& direction, const Mesh<1> subcell_mesh,
                const double time,
                const std::unordered_map<
                    std::string,
                    std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>&
                    functions_of_time,
                const ElementMap<1, Frame::Grid>& logical_to_grid_map,
                const domain::CoordinateMapBase<Frame::Grid, Frame::Inertial,
                                                1>& grid_to_inertial_map,
                const fd::Reconstructor& reconstructor,
                const AnalyticSolutionOrData& analytic_solution_or_data) const {
    const size_t ghost_zone_size{reconstructor.ghost_zone_size()};

    const auto ghost_logical_coords =
        evolution::dg::subcell::fd::ghost_zone_logical_coordinates(
            subcell_mesh, ghost_zone_size, direction);

    const auto ghost_inertial_coords = grid_to_inertial_map(
        logical_to_grid_map(ghost_logical_coords), time, functions_of_time);

    // Compute U according to analytic solution or data
    if constexpr (std::is_base_of_v<MarkAsAnalyticSolution,
                                    AnalyticSolutionOrData>) {
      *u = get<Burgers::Tags::U>(analytic_solution_or_data.variables(
          ghost_inertial_coords, time, tmpl::list<Burgers::Tags::U>{}));
    } else {
      *u = get<Burgers::Tags::U>(analytic_solution_or_data.variables(
          ghost_inertial_coords, tmpl::list<Burgers::Tags::U>{}));
    }
  }

 private:
  static void flux_impl(
      gsl::not_null<tnsr::I<DataVector, 1, Frame::Inertial>*> flux,
      const Scalar<DataVector>& u_analytic);
};
}  // namespace Burgers::BoundaryConditions
