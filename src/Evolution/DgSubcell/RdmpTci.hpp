// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "DataStructures/Variables.hpp"
#include "Evolution/DgSubcell/Tags/Inactive.hpp"
#include "Utilities/ErrorHandling/Assert.hpp"
#include "Utilities/TMPL.hpp"

namespace evolution::dg::subcell {
/*!
 * \brief Troubled cell indicator using a relaxed discrete maximum principle,
 * comparing the candidate solution with the past solution in the element and
 * its neighbors.
 *
 * Let the candidate solution be denoted by \f$u^\star_{\alpha}(t^{n+1})\f$.
 * Then the RDMP requires that
 *
 * \f{align*}{
 *   \min_{\forall\mathcal{N}}\left(u_{\alpha}(t^n)\right)
 *   - \delta_\alpha
 *   \le
 *   u^\star_{\alpha}(t^{n+1})
 *   \le
 *   \max_{\forall\mathcal{N}}
 *   \left(u_{\alpha}(t^n)\right) + \delta_\alpha
 * \f}
 *
 * where \f$\mathcal{N}\f$ are either the Neumann or Voronoi neighbors and the
 * element itself,  and \f$\delta_\alpha\f$ is a parameter defined below that
 * relaxes the discrete maximum principle (DMP). When computing
 * \f$\max(u_\alpha)\f$ and \f$\min(u_\alpha)\f$ over a DG element that is not
 * using subcells we first project the DG solution to the subcells and then
 * compute the maximum and minimum over *both* the DG grid and the subcell grid.
 * However, when a DG element is using subcells we compute the maximum and
 * minimum of \f$u_\alpha(t^n)\f$ over the subcells only. Note that the maximum
 * and minimum values of \f$u^\star_\alpha\f$ are always computed over both the
 * DG and the subcell grids, even when using the RDMP to check if the
 * reconstructed DG solution would be admissible.
 *
 * The parameter \f$\delta_\alpha\f$ is given by:
 *
 * \f{align*}{
 *   \delta_\alpha =
 *   \max\left(\delta_{0},\epsilon
 *   \left(\max_{\forall\mathcal{N}}\left(u_{\alpha}(t^n)\right)
 *   - \min_{\forall\mathcal{N}}\left(u_{\alpha}(t^n)\right)\right)
 *   \right),
 * \f}
 *
 * where we typically take \f$\delta_{0}=10^{-4}\f$ and \f$\epsilon=10^{-3}\f$.
 */
template <typename... EvolvedVarsTags>
bool rdmp_tci(const Variables<tmpl::list<EvolvedVarsTags...>>&
                  active_grid_candidate_evolved_vars,
              const Variables<tmpl::list<Tags::Inactive<EvolvedVarsTags>...>>&
                  inactive_grid_candidate_evolved_vars,
              const std::vector<double>& max_of_past_variables,
              const std::vector<double>& min_of_past_variables,
              const double rdmp_delta0, const double rdmp_epsilon) {
  bool cell_is_troubled = false;
  size_t component_index = 0;
  tmpl::for_each<tmpl::list<EvolvedVarsTags...>>(
      [&active_grid_candidate_evolved_vars, &cell_is_troubled, &component_index,
       &inactive_grid_candidate_evolved_vars, &max_of_past_variables,
       &min_of_past_variables, rdmp_delta0, rdmp_epsilon](auto tag_v) {
        if (cell_is_troubled) {
          return;
        }
        using std::max;
        using std::min;

        using tag = tmpl::type_from<decltype(tag_v)>;
        using inactive_tag = Tags::Inactive<tag>;
        const auto& active_var = get<tag>(active_grid_candidate_evolved_vars);
        const auto& inactive_var =
            get<inactive_tag>(inactive_grid_candidate_evolved_vars);
        const size_t number_of_components = active_var.size();
        ASSERT(number_of_components == inactive_var.size(),
               "The active and inactive vars must have the same type of tensor "
               "and therefore the same number of components.");

        for (size_t tensor_storage_index = 0;
             tensor_storage_index < number_of_components;
             ++tensor_storage_index) {
          ASSERT(not cell_is_troubled,
                 "If a cell has already been marked as troubled during the "
                 "RDMP TCI, we should not be continuing to check other "
                 "variables.");
          const double max_active = max(active_var[tensor_storage_index]);
          const double min_active = min(active_var[tensor_storage_index]);
          const double max_inactive = max(inactive_var[tensor_storage_index]);
          const double min_inactive = min(inactive_var[tensor_storage_index]);
          const double delta =
              max(rdmp_delta0,
                  rdmp_epsilon * (max_of_past_variables[component_index] -
                                  min_of_past_variables[component_index]));
          cell_is_troubled =
              max(max_active, max_inactive) >
                  max_of_past_variables[component_index] + delta or
              min(min_active, min_inactive) <
                  min_of_past_variables[component_index] - delta;
          if (cell_is_troubled) {
            return;
          }
          ++component_index;
        }
      });
  return cell_is_troubled;
}

/*!
 * \brief get the max and min of each component of the active and inactive
 * variables. If `include_inactive_grid` is `false` then only the max over the
 * `active_grid_evolved_vars` for each component is returned.
 */
template <typename... EvolvedVarsTags>
std::pair<std::vector<double>, std::vector<double>> rdmp_max_min(
    const Variables<tmpl::list<EvolvedVarsTags...>>& active_grid_evolved_vars,
    const Variables<tmpl::list<Tags::Inactive<EvolvedVarsTags>...>>&
        inactive_grid_evolved_vars,
    const bool include_inactive_grid) {
  std::vector<double> max_of_vars(
      active_grid_evolved_vars.number_of_independent_components,
      std::numeric_limits<double>::min());
  std::vector<double> min_of_vars(
      active_grid_evolved_vars.number_of_independent_components,
      std::numeric_limits<double>::max());
  size_t component_index = 0;
  tmpl::for_each<tmpl::list<EvolvedVarsTags...>>(
      [&active_grid_evolved_vars, &component_index, &inactive_grid_evolved_vars,
       &include_inactive_grid, &max_of_vars, &min_of_vars](auto tag_v) {
        using std::max;
        using std::min;

        using tag = tmpl::type_from<decltype(tag_v)>;
        const auto& active_var = get<tag>(active_grid_evolved_vars);
        const size_t number_of_components_in_tensor = active_var.size();
        for (size_t tensor_storage_index = 0;
             tensor_storage_index < number_of_components_in_tensor;
             ++tensor_storage_index) {
          ASSERT(component_index < max_of_vars.size() and
                     component_index < min_of_vars.size(),
                 "The component index into the variables is out of bounds.");
          max_of_vars[component_index] = max(active_var[tensor_storage_index]);
          min_of_vars[component_index] = min(active_var[tensor_storage_index]);
          if (include_inactive_grid) {
            using inactive_tag = Tags::Inactive<tag>;
            const auto& inactive_var =
                get<inactive_tag>(inactive_grid_evolved_vars);
            max_of_vars[component_index] =
                max(max_of_vars[component_index],
                    max(inactive_var[tensor_storage_index]));
            min_of_vars[component_index] =
                min(min_of_vars[component_index],
                    min(inactive_var[tensor_storage_index]));
          }
          ++component_index;
        }
      });
  return {std::move(max_of_vars), std::move(min_of_vars)};
}

/*!
 * \brief Check if the current variables satisfy the RDMP. Returns `true` if the
 * cell is troubled.
 *
 * Let the candidate solution be denoted by \f$u^\star_{\alpha}(t^{n+1})\f$.
 * Then the RDMP requires that
 *
 * \f{align*}{
 *   \min_{\forall\mathcal{N}}\left(u_{\alpha}(t^n)\right)
 *   - \delta_\alpha
 *   \le
 *   u^\star_{\alpha}(t^{n+1})
 *   \le
 *   \max_{\forall\mathcal{N}}
 *   \left(u_{\alpha}(t^n)\right) + \delta_\alpha
 * \f}
 *
 * where \f$\mathcal{N}\f$ are either the Neumann or Voronoi neighbors and the
 * element itself,  and \f$\delta_\alpha\f$ is a parameter defined below that
 * relaxes the discrete maximum principle (DMP). When computing
 * \f$\max(u_\alpha)\f$ and \f$\min(u_\alpha)\f$ over a DG element that is not
 * using subcells we first project the DG solution to the subcells and then
 * compute the maximum and minimum over *both* the DG grid and the subcell grid.
 * However, when a DG element is using subcells we compute the maximum and
 * minimum of \f$u_\alpha(t^n)\f$ over the subcells only. Note that the maximum
 * and minimum values of \f$u^\star_\alpha\f$ are always computed over both the
 * DG and the subcell grids, even when using the RDMP to check if the
 * reconstructed DG solution would be admissible.
 *
 * The parameter \f$\delta_\alpha\f$ is given by:
 *
 * \f{align*}{
 *   \delta_\alpha =
 *   \max\left(\delta_{0},\epsilon
 *   \left(\max_{\forall\mathcal{N}}\left(u_{\alpha}(t^n)\right)
 *   - \min_{\forall\mathcal{N}}\left(u_{\alpha}(t^n)\right)\right)
 *   \right),
 * \f}
 *
 * where we typically take \f$\delta_{0}=10^{-4}\f$ and \f$\epsilon=10^{-3}\f$.
 */
bool rdmp_tci(const std::vector<double>& max_of_current_variables,
              const std::vector<double>& min_of_current_variables,
              const std::vector<double>& max_of_past_variables,
              const std::vector<double>& min_of_past_variables,
              double rdmp_delta0, double rdmp_epsilon);
}  // namespace evolution::dg::subcell
