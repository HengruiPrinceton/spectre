// Distributed under the MIT License.
// See LICENSE.txt for details.

#pragma once

#include <vector>

#include "ApparentHorizons/ComputeHorizonVolumeQuantities.hpp"
#include "ApparentHorizons/ComputeHorizonVolumeQuantities.tpp"
#include "ApparentHorizons/ComputeItems.hpp"
#include "ApparentHorizons/Tags.hpp"
#include "Domain/Creators/RegisterDerivedWithCharm.hpp"
#include "Domain/Creators/TimeDependence/RegisterDerivedWithCharm.hpp"
#include "Domain/FunctionsOfTime/RegisterDerivedWithCharm.hpp"
#include "Evolution/DiscontinuousGalerkin/Limiters/Tags.hpp"
#include "Evolution/Executables/GrMhd/GhValenciaDivClean/GhValenciaDivCleanBase.hpp"
#include "Evolution/Systems/GeneralizedHarmonic/BoundaryCorrections/RegisterDerived.hpp"
#include "Evolution/Systems/GeneralizedHarmonic/ConstraintDamping/RegisterDerivedWithCharm.hpp"
#include "Evolution/Systems/GeneralizedHarmonic/Tags.hpp"
#include "Evolution/Systems/GrMhd/GhValenciaDivClean/BoundaryCorrections/RegisterDerived.hpp"
#include "Evolution/Systems/GrMhd/ValenciaDivClean/Tags.hpp"
#include "Evolution/VariableFixing/Tags.hpp"
#include "Options/FactoryHelpers.hpp"
#include "Options/Options.hpp"
#include "Options/Protocols/FactoryCreation.hpp"
#include "Parallel/RegisterDerivedClassesWithCharm.hpp"
#include "ParallelAlgorithms/Interpolation/Actions/CleanUpInterpolator.hpp"
#include "ParallelAlgorithms/Interpolation/Actions/InitializeInterpolationTarget.hpp"
#include "ParallelAlgorithms/Interpolation/Actions/InterpolationTargetReceiveVars.hpp"
#include "ParallelAlgorithms/Interpolation/Actions/InterpolatorReceivePoints.hpp"
#include "ParallelAlgorithms/Interpolation/Actions/InterpolatorReceiveVolumeData.hpp"
#include "ParallelAlgorithms/Interpolation/Actions/InterpolatorRegisterElement.hpp"
#include "ParallelAlgorithms/Interpolation/Actions/TryToInterpolate.hpp"
#include "ParallelAlgorithms/Interpolation/Callbacks/ErrorOnFailedApparentHorizon.hpp"
#include "ParallelAlgorithms/Interpolation/Callbacks/FindApparentHorizon.hpp"
#include "ParallelAlgorithms/Interpolation/Callbacks/ObserveTimeSeriesOnSurface.hpp"
#include "ParallelAlgorithms/Interpolation/Events/Interpolate.hpp"
#include "ParallelAlgorithms/Interpolation/InterpolationTarget.hpp"
#include "ParallelAlgorithms/Interpolation/Interpolator.hpp"
#include "ParallelAlgorithms/Interpolation/Protocols/InterpolationTargetTag.hpp"
#include "ParallelAlgorithms/Interpolation/Tags.hpp"
#include "ParallelAlgorithms/Interpolation/Targets/ApparentHorizon.hpp"
#include "Time/StepControllers/Factory.hpp"
#include "Time/Tags.hpp"
#include "Utilities/Blas.hpp"
#include "Utilities/ErrorHandling/Error.hpp"
#include "Utilities/ErrorHandling/FloatingPointExceptions.hpp"
#include "Utilities/ProtocolHelpers.hpp"
#include "Utilities/TMPL.hpp"

template <typename InitialData, typename... InterpolationTargetTags>
struct EvolutionMetavars
    : public virtual GhValenciaDivCleanDefaults,
      public GhValenciaDivCleanTemplateBase<
          EvolutionMetavars<InitialData, InterpolationTargetTags...>> {
  static constexpr Options::String help{
      "Evolve the Valencia formulation of the GRMHD system with divergence "
      "cleaning, coupled to a dynamic spacetime evolved with the Generalized "
      "Harmonic formulation\n"
      "on a domain with a single horizon and corresponding excised region"};

  struct AhA : tt::ConformsTo<intrp::protocols::InterpolationTargetTag> {
    using temporal_id = ::Tags::Time;
    using tags_to_observe =
        tmpl::list<StrahlkorperGr::Tags::AreaCompute<domain_frame>>;
    using compute_vars_to_interpolate = ah::ComputeHorizonVolumeQuantities;
    using vars_to_interpolate_to_target = tmpl::list<
        gr::Tags::SpatialMetric<volume_dim, domain_frame, DataVector>,
        gr::Tags::InverseSpatialMetric<volume_dim, domain_frame>,
        gr::Tags::ExtrinsicCurvature<volume_dim, domain_frame>,
        gr::Tags::SpatialChristoffelSecondKind<volume_dim, domain_frame>>;
    using compute_items_on_target = tmpl::append<
        tmpl::list<StrahlkorperGr::Tags::AreaElementCompute<domain_frame>>,
        tags_to_observe>;
    using compute_target_points =
        intrp::TargetPoints::ApparentHorizon<AhA, ::Frame::Inertial>;
    using post_interpolation_callback =
        intrp::callbacks::FindApparentHorizon<AhA, ::Frame::Inertial>;
    using horizon_find_failure_callback =
        intrp::callbacks::ErrorOnFailedApparentHorizon;
    using post_horizon_find_callbacks = tmpl::list<
        intrp::callbacks::ObserveTimeSeriesOnSurface<tags_to_observe, AhA>>;
  };

  using interpolation_target_tags = tmpl::list<AhA>;
  using interpolator_source_vars =
      tmpl::list<gr::Tags::SpacetimeMetric<volume_dim, domain_frame>,
                 GeneralizedHarmonic::Tags::Pi<volume_dim, domain_frame>,
                 GeneralizedHarmonic::Tags::Phi<volume_dim, domain_frame>>;

  using observe_fields = typename GhValenciaDivCleanTemplateBase<
      EvolutionMetavars>::observe_fields;

  struct factory_creation
      : tt::ConformsTo<Options::protocols::FactoryCreation> {
    using factory_classes = Options::add_factory_classes<
        typename GhValenciaDivCleanTemplateBase<
            EvolutionMetavars>::factory_creation::factory_classes,
        tmpl::pair<Event, tmpl::list<intrp::Events::Interpolate<
                              3, AhA, interpolator_source_vars>>>>;
  };

  using initial_data =
      typename GhValenciaDivCleanTemplateBase<EvolutionMetavars>::initial_data;
  using initial_data_tag = typename GhValenciaDivCleanTemplateBase<
      EvolutionMetavars>::initial_data_tag;

  using const_global_cache_tags = tmpl::flatten<tmpl::list<
      tmpl::conditional_t<evolution::is_numeric_initial_data_v<initial_data>,
                          tmpl::list<>, initial_data_tag>,
      grmhd::ValenciaDivClean::Tags::ConstraintDampingParameter,
      GeneralizedHarmonic::ConstraintDamping::Tags::DampingFunctionGamma0<
          volume_dim, Frame::Grid>,
      GeneralizedHarmonic::ConstraintDamping::Tags::DampingFunctionGamma1<
          volume_dim, Frame::Grid>,
      GeneralizedHarmonic::ConstraintDamping::Tags::DampingFunctionGamma2<
          volume_dim, Frame::Grid>>>;

  using observed_reduction_data_tags = observers::collect_reduction_data_tags<
      tmpl::at<typename factory_creation::factory_classes, Event>>;

  using dg_registration_list = typename GhValenciaDivCleanTemplateBase<
      EvolutionMetavars>::dg_registration_list;

  template <typename ParallelComponent>
  struct registration_list {
    using type = std::conditional_t<
        std::is_same_v<ParallelComponent,
                       typename GhValenciaDivCleanTemplateBase<
                           EvolutionMetavars>::dg_element_array_component>,
        dg_registration_list, tmpl::list<>>;
  };

  using component_list =
      tmpl::push_back<typename GhValenciaDivCleanTemplateBase<
                          EvolutionMetavars>::component_list,
                      intrp::InterpolationTarget<EvolutionMetavars, AhA>>;
};

static const std::vector<void (*)()> charm_init_node_funcs{
    &setup_error_handling,
    &disable_openblas_multithreading,
    &domain::creators::register_derived_with_charm,
    &domain::creators::time_dependence::register_derived_with_charm,
    &domain::FunctionsOfTime::register_derived_with_charm,
    &grmhd::GhValenciaDivClean::BoundaryCorrections::
        register_derived_with_charm,
    &GeneralizedHarmonic::ConstraintDamping::register_derived_with_charm,
    &Parallel::register_factory_classes_with_charm<metavariables>};

static const std::vector<void (*)()> charm_init_proc_funcs{
    &enable_floating_point_exceptions};
