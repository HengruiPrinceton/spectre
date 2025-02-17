// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Framework/TestingFramework.hpp"

#include <cstddef>
#include <pup.h>
#include <string>

#include "DataStructures/DataBox/DataBox.hpp"
#include "DataStructures/DataBox/Tag.hpp"
#include "DataStructures/DataVector.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "DataStructures/Variables.hpp"
#include "DataStructures/VariablesTag.hpp"
#include "Domain/Tags.hpp"
#include "Evolution/ComputeTags.hpp"
#include "Helpers/DataStructures/DataBox/TestHelpers.hpp"
#include "PointwiseFunctions/AnalyticData/AnalyticData.hpp"
#include "PointwiseFunctions/AnalyticSolutions/AnalyticSolution.hpp"
#include "PointwiseFunctions/AnalyticSolutions/Tags.hpp"
#include "Time/Tags.hpp"
#include "Utilities/TMPL.hpp"

namespace {

struct FieldTag : db::SimpleTag {
  using type = Scalar<DataVector>;
};

struct TestAnalyticSolution : public MarkAsAnalyticSolution {
  static tuples::TaggedTuple<FieldTag> variables(
      const tnsr::I<DataVector, 1>& x, const double t,
      const tmpl::list<FieldTag> /*meta*/) {
    return {Scalar<DataVector>{t * get<0>(x)}};
  }
  void pup(PUP::er& /*p*/) {}  // NOLINT
};

struct TestAnalyticData : public MarkAsAnalyticData {
  static tuples::TaggedTuple<FieldTag> variables(
      const tnsr::I<DataVector, 1>& x, const tmpl::list<FieldTag> /*meta*/) {
    return {Scalar<DataVector>{get<0>(x)}};
  }
  void pup(PUP::er& /*p*/) {}  // NOLINT
};
}  // namespace

SPECTRE_TEST_CASE("Unit.Evolution.ComputeTags", "[Unit][Evolution]") {
  tnsr::I<DataVector, 1, Frame::Inertial> inertial_coords{{{{1., 2., 3., 4.}}}};
  const double current_time = 2.;
  const Variables<tmpl::list<FieldTag>> vars{4, 3.};
  {
    INFO("Test analytic solution");
    const auto box = db::create<
        db::AddSimpleTags<domain::Tags::Coordinates<1, Frame::Inertial>,
                          ::Tags::AnalyticSolution<TestAnalyticSolution>,
                          Tags::Time, ::Tags::Variables<tmpl::list<FieldTag>>>,
        db::AddComputeTags<
            evolution::Tags::AnalyticSolutionsCompute<1, tmpl::list<FieldTag>>,
            Tags::ErrorsCompute<tmpl::list<FieldTag>>>>(
        inertial_coords, TestAnalyticSolution{}, current_time, vars);
    const DataVector expected{2., 4., 6., 8.};
    const DataVector expected_error{1., -1., -3., -5.};
    CHECK_ITERABLE_APPROX(get(get<Tags::Analytic<FieldTag>>(box).value()),
                          expected);
    CHECK_ITERABLE_APPROX(get(get<Tags::Error<FieldTag>>(box).value()),
                          expected_error);
  }
  {
    INFO("Test analytic data");
    const auto box = db::create<
        db::AddSimpleTags<domain::Tags::Coordinates<1, Frame::Inertial>,
                          ::Tags::AnalyticData<TestAnalyticData>, Tags::Time,
                          ::Tags::Variables<tmpl::list<FieldTag>>>,
        db::AddComputeTags<
            evolution::Tags::AnalyticSolutionsCompute<1, tmpl::list<FieldTag>>,
            Tags::ErrorsCompute<tmpl::list<FieldTag>>>>(
        inertial_coords, TestAnalyticData{}, current_time, vars);
    CHECK_FALSE(get<Tags::Analytic<FieldTag>>(box).has_value());
    CHECK_FALSE(get<Tags::Error<FieldTag>>(box).has_value());
  }

  TestHelpers::db::test_compute_tag<
      evolution::Tags::AnalyticSolutionsCompute<1, tmpl::list<FieldTag>>>(
      "AnalyticSolutions");
}
