// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <memory>
#include <optional>
#include <pup.h>
#include <string>
#include <type_traits>

#include "DataStructures/DataBox/Prefixes.hpp"
#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "Evolution/BoundaryConditions/Type.hpp"
#include "Evolution/DgSubcell/Tags/Mesh.hpp"
#include "Evolution/Systems/Burgers/BoundaryConditions/BoundaryCondition.hpp"
#include "Evolution/Systems/Burgers/Tags.hpp"
#include "Options/Options.hpp"
#include "Parallel/CharmPupable.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/TMPL.hpp"

/// \cond
template <size_t Dim>
class Direction;
template <size_t Dim>
class Mesh;
/// \endcond

namespace Burgers::BoundaryConditions {
/*!
 * \brief A boundary condition that only verifies that all characteristic speeds
 * are directed out of the domain; no boundary data is altered by this boundary
 * condition.
 */
class Outflow final : public BoundaryCondition {
 public:
  using options = tmpl::list<>;
  static constexpr Options::String help{
      "Outflow boundary condition that only verifies the characteristic speeds "
      "are all directed out of the domain."};

  Outflow() = default;
  Outflow(Outflow&&) = default;
  Outflow& operator=(Outflow&&) = default;
  Outflow(const Outflow&) = default;
  Outflow& operator=(const Outflow&) = default;
  ~Outflow() override = default;

  explicit Outflow(CkMigrateMessage* msg);

  WRAPPED_PUPable_decl_base_template(
      domain::BoundaryConditions::BoundaryCondition, Outflow);

  auto get_clone() const -> std::unique_ptr<
      domain::BoundaryConditions::BoundaryCondition> override;

  static constexpr evolution::BoundaryConditions::Type bc_type =
      evolution::BoundaryConditions::Type::Outflow;

  void pup(PUP::er& p) override;

  using dg_interior_evolved_variables_tags = tmpl::list<Tags::U>;
  using dg_interior_temporary_tags = tmpl::list<>;
  using dg_gridless_tags = tmpl::list<>;

  static std::optional<std::string> dg_outflow(
      const std::optional<tnsr::I<DataVector, 1, Frame::Inertial>>&
          face_mesh_velocity,
      const tnsr::i<DataVector, 1, Frame::Inertial>&
          outward_directed_normal_covector,
      const Scalar<DataVector>& u);

  using fd_interior_evolved_variables_tags = tmpl::list<Tags::U>;
  using fd_interior_temporary_tags =
      tmpl::list<evolution::dg::subcell::Tags::Mesh<1>>;
  using fd_gridless_tags = tmpl::list<>;

  static void fd_outflow(
      gsl::not_null<Scalar<DataVector>*> u, const Direction<1>& direction,
      const std::optional<tnsr::I<DataVector, 1, Frame::Inertial>>&
          face_mesh_velocity,
      const tnsr::i<DataVector, 1, Frame::Inertial>&
          outward_directed_normal_covector,
      const Scalar<DataVector>& u_interior, const Mesh<1>& subcell_mesh);
};
}  // namespace Burgers::BoundaryConditions
