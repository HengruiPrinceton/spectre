// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Framework/TestingFramework.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <pup.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "DataStructures/Index.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "Domain/Block.hpp"
#include "Domain/BoundaryConditions/BoundaryCondition.hpp"
#include "Domain/CoordinateMaps/Affine.hpp"
#include "Domain/CoordinateMaps/CoordinateMap.hpp"
#include "Domain/CoordinateMaps/CoordinateMap.tpp"
#include "Domain/CoordinateMaps/Identity.hpp"
#include "Domain/CoordinateMaps/TimeDependent/Translation.hpp"
#include "Domain/Domain.hpp"
#include "Domain/DomainHelpers.hpp"
#include "Domain/FunctionsOfTime/FunctionOfTime.hpp"
#include "Domain/FunctionsOfTime/PiecewisePolynomial.hpp"
#include "Domain/Structure/BlockNeighbor.hpp"
#include "Domain/Structure/Direction.hpp"
#include "Domain/Structure/DirectionMap.hpp"
#include "Domain/Structure/OrientationMap.hpp"
#include "Domain/Tags.hpp"
#include "Framework/TestHelpers.hpp"
#include "Helpers/Domain/BoundaryConditions/BoundaryCondition.hpp"
#include "Helpers/Domain/DomainTestHelpers.hpp"
#include "Utilities/ErrorHandling/Error.hpp"
#include "Utilities/GetOutput.hpp"
#include "Utilities/MakeVector.hpp"
#include "Utilities/StdHelpers.hpp"

namespace domain {
namespace {
namespace helpers = ::TestHelpers::domain::BoundaryConditions;

void test_1d_domains() {
  using Translation = domain::CoordinateMaps::TimeDependent::Translation<1>;
  using TranslationGridDistorted =
      domain::CoordinateMap<Frame::Grid, Frame::Distorted, Translation>;
  using TranslationDistortedInertial =
      domain::CoordinateMap<Frame::Distorted, Frame::Inertial, Translation>;
  {
    using LogicalToGridCoordinateMap =
        CoordinateMap<Frame::BlockLogical, Frame::Inertial,
                      CoordinateMaps::Identity<1>>;

    using GridToInertialCoordinateMap =
        domain::CoordinateMap<Frame::Grid, Frame::Inertial, Translation>;
    using GridToDistortedCoordinateMap = TranslationGridDistorted;
    using DistortedToInertialCoordinateMap = TranslationDistortedInertial;

    PUPable_reg(SINGLE_ARG(CoordinateMap<Frame::BlockLogical, Frame::Grid,
                                         CoordinateMaps::Identity<1>>));
    PUPable_reg(GridToInertialCoordinateMap);

    PUPable_reg(LogicalToGridCoordinateMap);
    PUPable_reg(SINGLE_ARG(CoordinateMap<Frame::BlockLogical, Frame::Grid,
                                         CoordinateMaps::Identity<1>>));
    PUPable_reg(GridToDistortedCoordinateMap);
    PUPable_reg(DistortedToInertialCoordinateMap);

    PUPable_reg(SINGLE_ARG(CoordinateMap<Frame::BlockLogical, Frame::Inertial,
                                         CoordinateMaps::Affine>));
    PUPable_reg(SINGLE_ARG(CoordinateMap<Frame::BlockLogical, Frame::Grid,
                                         CoordinateMaps::Affine>));
    PUPable_reg(
        SINGLE_ARG(CoordinateMap<Frame::Grid, Frame::Inertial, Translation>));

    // Test construction of two intervals which have anti-aligned logical axes.
    Domain<1> domain_from_corners(
        make_vector<std::unique_ptr<
            CoordinateMapBase<Frame::BlockLogical, Frame::Inertial, 1>>>(
            std::make_unique<CoordinateMap<Frame::BlockLogical, Frame::Inertial,
                                           CoordinateMaps::Affine>>(
                make_coordinate_map<Frame::BlockLogical, Frame::Inertial>(
                    CoordinateMaps::Affine{-1., 1., -2., 0.})),
            std::make_unique<CoordinateMap<Frame::BlockLogical, Frame::Inertial,
                                           CoordinateMaps::Affine>>(
                make_coordinate_map<Frame::BlockLogical, Frame::Inertial>(
                    CoordinateMaps::Affine{-1., 1., 0., 2.}))),
        std::vector<std::array<size_t, 2>>{{{1, 2}}, {{3, 2}}});

    Domain<1> domain_no_corners(
        make_vector<std::unique_ptr<
            CoordinateMapBase<Frame::BlockLogical, Frame::Inertial, 1>>>(
            std::make_unique<CoordinateMap<Frame::BlockLogical, Frame::Inertial,
                                           CoordinateMaps::Affine>>(
                make_coordinate_map<Frame::BlockLogical, Frame::Inertial>(
                    CoordinateMaps::Affine{-1., 1., -2., 0.})),
            std::make_unique<CoordinateMap<Frame::BlockLogical, Frame::Inertial,
                                           CoordinateMaps::Affine>>(
                make_coordinate_map<Frame::BlockLogical, Frame::Inertial>(
                    CoordinateMaps::Affine{-1., 1., 2., 0.}))));

    const OrientationMap<1> unaligned_orientation{{{Direction<1>::lower_xi()}},
                                                  {{Direction<1>::upper_xi()}}};

    const std::vector<DirectionMap<1, BlockNeighbor<1>>> expected_neighbors{
        {{Direction<1>::upper_xi(),
          BlockNeighbor<1>{1, unaligned_orientation}}},
        {{Direction<1>::upper_xi(),
          BlockNeighbor<1>{0, unaligned_orientation}}}};

    const std::vector<std::unordered_set<Direction<1>>> expected_boundaries{
        {Direction<1>::lower_xi()}, {Direction<1>::lower_xi()}};

    const auto expected_stationary_maps = make_vector(
        make_coordinate_map_base<Frame::BlockLogical, Frame::Inertial>(
            CoordinateMaps::Affine{-1., 1., -2., 0.}),
        make_coordinate_map_base<Frame::BlockLogical, Frame::Inertial>(
            CoordinateMaps::Affine{-1., 1., 0., 2.}));

    const auto expected_stationary_maps_no_corners = make_vector(
        make_coordinate_map_base<Frame::BlockLogical, Frame::Inertial>(
            CoordinateMaps::Affine{-1., 1., -2., 0.}),
        make_coordinate_map_base<Frame::BlockLogical, Frame::Inertial>(
            CoordinateMaps::Affine{-1., 1., 2., 0.}));

    const GridToDistortedCoordinateMap translation_grid_to_distorted_map =
        domain::make_coordinate_map<Frame::Grid, Frame::Distorted>(
            Translation{"TranslationGridToDistorted"});
    const DistortedToInertialCoordinateMap
        translation_distorted_to_inertial_map =
            domain::make_coordinate_map<Frame::Distorted, Frame::Inertial>(
                Translation{"TranslationDistortedToInertial"});

    test_domain_construction(domain_from_corners, expected_neighbors,
                             expected_boundaries, expected_stationary_maps);

    test_domain_construction(serialize_and_deserialize(domain_from_corners),
                             expected_neighbors, expected_boundaries,
                             expected_stationary_maps);

    test_domain_construction(domain_no_corners, expected_neighbors,
                             expected_boundaries,
                             expected_stationary_maps_no_corners);

    test_domain_construction(serialize_and_deserialize(domain_no_corners),
                             expected_neighbors, expected_boundaries,
                             expected_stationary_maps_no_corners);

    // Test injection of a translation map.
    REQUIRE(domain_from_corners.blocks().size() == 2);
    REQUIRE(domain_no_corners.blocks().size() == 2);
    domain_from_corners.inject_time_dependent_map_for_block(
        0,
        make_coordinate_map_base<Frame::Grid, Frame::Inertial>(
            Translation{"Translation0"}),
        translation_grid_to_distorted_map.get_clone(),
        translation_distorted_to_inertial_map.get_clone());
    domain_from_corners.inject_time_dependent_map_for_block(
        1,
        make_coordinate_map_base<Frame::Grid, Frame::Inertial>(
            Translation{"Translation1"}),
        translation_grid_to_distorted_map.get_clone(),
        translation_distorted_to_inertial_map.get_clone());

    domain_no_corners.inject_time_dependent_map_for_block(
        0,
        make_coordinate_map_base<Frame::Grid, Frame::Inertial>(
            Translation{"Translation0"}),
        translation_grid_to_distorted_map.get_clone(),
        translation_distorted_to_inertial_map.get_clone());
    domain_no_corners.inject_time_dependent_map_for_block(
        1,
        make_coordinate_map_base<Frame::Grid, Frame::Inertial>(
            Translation{"Translation1"}),
        translation_grid_to_distorted_map.get_clone(),
        translation_distorted_to_inertial_map.get_clone());

    const auto expected_logical_to_grid_maps =
        make_vector(make_coordinate_map_base<Frame::BlockLogical, Frame::Grid>(
                        CoordinateMaps::Affine{-1., 1., -2., 0.}),
                    make_coordinate_map_base<Frame::BlockLogical, Frame::Grid>(
                        CoordinateMaps::Affine{-1., 1., 0., 2.}));
    const auto expected_logical_to_grid_maps_no_corners =
        make_vector(make_coordinate_map_base<Frame::BlockLogical, Frame::Grid>(
                        CoordinateMaps::Affine{-1., 1., -2., 0.}),
                    make_coordinate_map_base<Frame::BlockLogical, Frame::Grid>(
                        CoordinateMaps::Affine{-1., 1., 2., 0.}));
    const auto expected_grid_to_inertial_maps =
        make_vector_coordinate_map_base<Frame::Grid, Frame::Inertial>(
            Translation{"Translation0"}, Translation{"Translation1"});

    std::unordered_map<std::string,
                       std::unique_ptr<domain::FunctionsOfTime::FunctionOfTime>>
        functions_of_time{};
    functions_of_time["Translation0"] =
        std::make_unique<domain::FunctionsOfTime::PiecewisePolynomial<2>>(
            1.0, std::array<DataVector, 3>{{{0.0}, {2.3}, {0.0}}}, 10.0);
    functions_of_time["Translation1"] =
        std::make_unique<domain::FunctionsOfTime::PiecewisePolynomial<2>>(
            1.0, std::array<DataVector, 3>{{{0.0}, {5.3}, {0.0}}}, 10.0);

    test_domain_construction(domain_from_corners, expected_neighbors,
                             expected_boundaries, expected_logical_to_grid_maps,
                             10.0, functions_of_time,
                             expected_grid_to_inertial_maps);
    test_domain_construction(serialize_and_deserialize(domain_from_corners),
                             expected_neighbors, expected_boundaries,
                             expected_logical_to_grid_maps, 10.0,
                             functions_of_time, expected_grid_to_inertial_maps);

    test_domain_construction(domain_no_corners, expected_neighbors,
                             expected_boundaries,
                             expected_logical_to_grid_maps_no_corners, 10.0,
                             functions_of_time, expected_grid_to_inertial_maps);
    test_domain_construction(serialize_and_deserialize(domain_no_corners),
                             expected_neighbors, expected_boundaries,
                             expected_logical_to_grid_maps_no_corners, 10.0,
                             functions_of_time, expected_grid_to_inertial_maps);

    // Test construction from a vector of blocks
    auto vector_of_blocks = [&expected_neighbors]() {
      std::vector<Block<1>> vec;
      vec.emplace_back(Block<1>{
          std::make_unique<CoordinateMap<Frame::BlockLogical, Frame::Inertial,
                                         CoordinateMaps::Affine>>(
              make_coordinate_map<Frame::BlockLogical, Frame::Inertial>(
                  CoordinateMaps::Affine{-1., 1., -2., 0.})),
          0, expected_neighbors[0]});
      vec.emplace_back(Block<1>{
          std::make_unique<CoordinateMap<Frame::BlockLogical, Frame::Inertial,
                                         CoordinateMaps::Affine>>(
              make_coordinate_map<Frame::BlockLogical, Frame::Inertial>(
                  CoordinateMaps::Affine{-1., 1., 0., 2.})),
          1, expected_neighbors[1]});
      return vec;
    }();

    test_domain_construction(Domain<1>{std::move(vector_of_blocks)},
                             expected_neighbors, expected_boundaries,
                             expected_stationary_maps);

    CHECK(get_output(domain_from_corners) ==
          "Domain with 2 blocks:\n" +
              get_output(domain_from_corners.blocks()[0]) + "\n" +
              get_output(domain_from_corners.blocks()[1]) + "\n" +
          "Excision spheres:\n" +
              get_output(domain_from_corners.excision_spheres()) + "\n");

    CHECK(get_output(domain_no_corners) ==
          "Domain with 2 blocks:\n" +
              get_output(domain_from_corners.blocks()[0]) + "\n" +
              get_output(domain_from_corners.blocks()[1]) + "\n" +
          "Excision spheres:\n" +
              get_output(domain_from_corners.excision_spheres()) + "\n");
  }

  {
    // Test construction of a periodic domain
    const auto expected_maps = make_vector(
        make_coordinate_map_base<Frame::BlockLogical, Frame::Inertial>(
            CoordinateMaps::Affine{-1., 1., -2., 2.}));

    const Domain<1> domain{
        make_vector<std::unique_ptr<
            CoordinateMapBase<Frame::BlockLogical, Frame::Inertial, 1>>>(
            std::make_unique<CoordinateMap<Frame::BlockLogical, Frame::Inertial,
                                           CoordinateMaps::Affine>>(
                make_coordinate_map<Frame::BlockLogical, Frame::Inertial>(
                    CoordinateMaps::Affine{-1., 1., -2., 2.}))),
        std::vector<std::array<size_t, 2>>{{{1, 2}}},
        std::vector<PairOfFaces>{{{1}, {2}}}};

    const auto expected_neighbors = []() {
      OrientationMap<1> orientation{{{Direction<1>::lower_xi()}},
                                    {{Direction<1>::lower_xi()}}};
      return std::vector<DirectionMap<1, BlockNeighbor<1>>>{
          {{Direction<1>::lower_xi(), BlockNeighbor<1>{0, orientation}},
           {Direction<1>::upper_xi(), BlockNeighbor<1>{0, orientation}}}};
    }();

    test_domain_construction(domain, expected_neighbors,
                             std::vector<std::unordered_set<Direction<1>>>{1},
                             expected_maps);
  }
}

template <size_t Dim>
auto compute_boundary_conditions(
    const std::vector<std::unordered_set<Direction<Dim>>>&
        external_boundaries) {
  using MapType = DirectionMap<
      Dim, std::unique_ptr<domain::BoundaryConditions::BoundaryCondition>>;
  std::vector<MapType> boundary_conditions{external_boundaries.size()};
  for (size_t i = 0; i < external_boundaries.size(); ++i) {
    const auto& external_boundaries_of_block = external_boundaries[i];
    MapType boundary_conditions_for_block{};
    for (const auto& direction : external_boundaries_of_block) {
      boundary_conditions_for_block.emplace(
          direction,
          std::make_unique<helpers::TestBoundaryCondition<Dim>>(direction));
    }
    boundary_conditions[i] = std::move(boundary_conditions_for_block);
  }
  return boundary_conditions;
}

void test_1d_rectilinear_domains() {
  INFO("Aligned domain.") {
    const std::vector<std::unordered_set<Direction<1>>>
        expected_external_boundaries{
            {{Direction<1>::lower_xi()}}, {}, {{Direction<1>::upper_xi()}}};
    const auto domain = rectilinear_domain<1>(
        Index<1>{3}, std::array<std::vector<double>, 1>{{{0.0, 1.0, 2.0, 3.0}}},
        compute_boundary_conditions(expected_external_boundaries), {}, {},
        {{false}}, {}, true);
    const OrientationMap<1> aligned{};
    std::vector<DirectionMap<1, BlockNeighbor<1>>> expected_block_neighbors{
        {{Direction<1>::upper_xi(), {1, aligned}}},
        {{Direction<1>::lower_xi(), {0, aligned}},
         {Direction<1>::upper_xi(), {2, aligned}}},
        {{Direction<1>::lower_xi(), {1, aligned}}}};
    for (size_t i = 0; i < domain.blocks().size(); i++) {
      CAPTURE(i);
      CHECK(domain.blocks()[i].external_boundaries() ==
            expected_external_boundaries[i]);
      CHECK(domain.blocks()[i].neighbors() == expected_block_neighbors[i]);
      for (const auto& direction : expected_external_boundaries[i]) {
        CAPTURE(direction);
        CHECK(direction ==
              dynamic_cast<const helpers::TestBoundaryCondition<1>&>(
                  *domain.blocks()[i].external_boundary_conditions().at(
                      direction))
                  .direction());
      }
    }
  }
  INFO("Antialigned domain.") {
    const OrientationMap<1> aligned{};
    const OrientationMap<1> antialigned{
        std::array<Direction<1>, 1>{{Direction<1>::lower_xi()}}};
    const std::vector<std::unordered_set<Direction<1>>>
        expected_external_boundaries{
            {{Direction<1>::lower_xi()}}, {}, {{Direction<1>::upper_xi()}}};

    const auto domain = rectilinear_domain<1>(
        Index<1>{3}, std::array<std::vector<double>, 1>{{{0.0, 1.0, 2.0, 3.0}}},
        compute_boundary_conditions(expected_external_boundaries), {},
        std::vector<OrientationMap<1>>{aligned, antialigned, aligned},
        {{false}}, {}, true);
    std::vector<DirectionMap<1, BlockNeighbor<1>>> expected_block_neighbors{
        {{Direction<1>::upper_xi(), {1, antialigned}}},
        {{Direction<1>::lower_xi(), {2, antialigned}},
         {Direction<1>::upper_xi(), {0, antialigned}}},
        {{Direction<1>::lower_xi(), {1, antialigned}}}};
    for (size_t i = 0; i < domain.blocks().size(); i++) {
      INFO(i);
      CHECK(domain.blocks()[i].external_boundaries() ==
            expected_external_boundaries[i]);
      CHECK(domain.blocks()[i].neighbors() == expected_block_neighbors[i]);
      for (const auto& direction : expected_external_boundaries[i]) {
        CAPTURE(direction);
        CHECK(direction ==
              dynamic_cast<const helpers::TestBoundaryCondition<1>&>(
                  *domain.blocks()[i].external_boundary_conditions().at(
                      direction))
                  .direction());
      }
    }
  }
}

void test_2d_rectilinear_domains() {
  const OrientationMap<2> half_turn{std::array<Direction<2>, 2>{
      {Direction<2>::lower_xi(), Direction<2>::lower_eta()}}};
  const OrientationMap<2> quarter_turn_cw{std::array<Direction<2>, 2>{
      {Direction<2>::upper_eta(), Direction<2>::lower_xi()}}};
  const OrientationMap<2> quarter_turn_ccw{std::array<Direction<2>, 2>{
      {Direction<2>::lower_eta(), Direction<2>::upper_xi()}}};
  auto orientations_of_all_blocks =
      std::vector<OrientationMap<2>>{4, OrientationMap<2>{}};
  orientations_of_all_blocks[1] = half_turn;
  orientations_of_all_blocks[2] = quarter_turn_cw;
  orientations_of_all_blocks[3] = quarter_turn_ccw;

  const std::vector<std::unordered_set<Direction<2>>>
      expected_external_boundaries{
          {{Direction<2>::lower_xi(), Direction<2>::lower_eta()}},
          {{Direction<2>::upper_eta(), Direction<2>::lower_xi()}},
          {{Direction<2>::lower_xi(), Direction<2>::lower_eta()}},
          {{Direction<2>::upper_xi(), Direction<2>::lower_eta()}}};

  const auto domain = rectilinear_domain<2>(
      Index<2>{2, 2},
      std::array<std::vector<double>, 2>{{{0.0, 1.0, 2.0}, {0.0, 1.0, 2.0}}},
      compute_boundary_conditions(expected_external_boundaries), {},
      orientations_of_all_blocks);
  std::vector<DirectionMap<2, BlockNeighbor<2>>> expected_block_neighbors{
      {{Direction<2>::upper_xi(), {1, half_turn}},
       {Direction<2>::upper_eta(), {2, quarter_turn_cw}}},
      {{Direction<2>::upper_xi(), {0, half_turn}},
       {Direction<2>::lower_eta(), {3, quarter_turn_cw}}},
      {{Direction<2>::upper_xi(), {0, quarter_turn_ccw}},
       {Direction<2>::upper_eta(), {3, half_turn}}},
      {{Direction<2>::lower_xi(), {1, quarter_turn_ccw}},
       {Direction<2>::upper_eta(), {2, half_turn}}}};
  for (size_t i = 0; i < domain.blocks().size(); i++) {
    INFO(i);
    CHECK(domain.blocks()[i].external_boundaries() ==
          expected_external_boundaries[i]);
    CHECK(domain.blocks()[i].neighbors() == expected_block_neighbors[i]);
    for (const auto& direction : expected_external_boundaries[i]) {
      CAPTURE(direction);
      CHECK(
          direction ==
          dynamic_cast<const helpers::TestBoundaryCondition<2>&>(
              *domain.blocks()[i].external_boundary_conditions().at(direction))
              .direction());
    }
  }
}

void test_3d_rectilinear_domains() {
  const OrientationMap<3> aligned{};
  const OrientationMap<3> quarter_turn_cw_xi{std::array<Direction<3>, 3>{
      {Direction<3>::upper_xi(), Direction<3>::upper_zeta(),
       Direction<3>::lower_eta()}}};
  auto orientations_of_all_blocks =
      std::vector<OrientationMap<3>>{aligned, quarter_turn_cw_xi};

  const std::vector<std::unordered_set<Direction<3>>>
      expected_external_boundaries{
          {{Direction<3>::lower_xi(), Direction<3>::lower_eta(),
            Direction<3>::upper_eta(), Direction<3>::lower_zeta(),
            Direction<3>::upper_zeta()}},
          {{Direction<3>::upper_xi(), Direction<3>::lower_eta(),
            Direction<3>::upper_eta(), Direction<3>::lower_zeta(),
            Direction<3>::upper_zeta()}}};

  const auto domain = rectilinear_domain<3>(
      Index<3>{2, 1, 1},
      std::array<std::vector<double>, 3>{
          {{0.0, 1.0, 2.0}, {0.0, 1.0}, {0.0, 1.0}}},
      compute_boundary_conditions(expected_external_boundaries), {},
      orientations_of_all_blocks);
  std::vector<DirectionMap<3, BlockNeighbor<3>>> expected_block_neighbors{
      {{Direction<3>::upper_xi(), {1, quarter_turn_cw_xi}}},
      {{Direction<3>::lower_xi(), {0, quarter_turn_cw_xi.inverse_map()}}}};
  for (size_t i = 0; i < domain.blocks().size(); i++) {
    INFO(i);
    CHECK(domain.blocks()[i].external_boundaries() ==
          expected_external_boundaries[i]);
    CHECK(domain.blocks()[i].neighbors() == expected_block_neighbors[i]);
    for (const auto& direction : expected_external_boundaries[i]) {
      CAPTURE(direction);
      CHECK(
          direction ==
          dynamic_cast<const helpers::TestBoundaryCondition<3>&>(
              *domain.blocks()[i].external_boundary_conditions().at(direction))
              .direction());
    }
  }
}
}  // namespace

SPECTRE_TEST_CASE("Unit.Domain.Domain", "[Domain][Unit]") {
  test_1d_domains();
  test_1d_rectilinear_domains();
  test_2d_rectilinear_domains();
  test_3d_rectilinear_domains();
}
// [[OutputRegex, Must pass same number of maps as block corner sets]]
[[noreturn]] SPECTRE_TEST_CASE("Unit.Domain.Domain.BadArgs", "[Domain][Unit]") {
  ASSERTION_TEST();
#ifdef SPECTRE_DEBUG
  // NOLINTNEXTLINE(bugprone-unused-raii)
  Domain<1>(make_vector(
                make_coordinate_map_base<Frame::BlockLogical, Frame::Inertial>(
                    CoordinateMaps::Affine{-1., 1., -1., 1.}),
                make_coordinate_map_base<Frame::BlockLogical, Frame::Inertial>(
                    CoordinateMaps::Affine{-1., 1., -1., 1.})),
            std::vector<std::array<size_t, 2>>{{{1, 2}}});
  ERROR("Failed to trigger ASSERT in an assertion test");
#endif
}
}  // namespace domain
