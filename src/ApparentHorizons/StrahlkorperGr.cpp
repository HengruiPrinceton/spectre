// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "ApparentHorizons/StrahlkorperGr.hpp"

#include <array>
#include <cmath>  // IWYU pragma: keep
#include <cstddef>
#include <utility>

#include "DataStructures/DataVector.hpp"
#include "DataStructures/Matrix.hpp"
#include "DataStructures/Tensor/EagerMath/DeterminantAndInverse.hpp"
#include "DataStructures/Tensor/EagerMath/DotProduct.hpp"
#include "DataStructures/Tensor/Tensor.hpp"
#include "NumericalAlgorithms/LinearAlgebra/FindGeneralizedEigenvalues.hpp"
#include "NumericalAlgorithms/SphericalHarmonics/SpherepackIterator.hpp"
#include "NumericalAlgorithms/SphericalHarmonics/Strahlkorper.hpp"
#include "NumericalAlgorithms/SphericalHarmonics/StrahlkorperFunctions.hpp"
#include "NumericalAlgorithms/SphericalHarmonics/YlmSpherepack.hpp"
#include "PointwiseFunctions/GeneralRelativity/Christoffel.hpp"
#include "PointwiseFunctions/GeneralRelativity/IndexManipulation.hpp"
#include "Utilities/ConstantExpressions.hpp"
#include "Utilities/ContainerHelpers.hpp"
#include "Utilities/ErrorHandling/Assert.hpp"
#include "Utilities/ErrorHandling/Error.hpp"
#include "Utilities/GenerateInstantiations.hpp"
#include "Utilities/Gsl.hpp"
#include "Utilities/MakeWithValue.hpp"
#include "Utilities/StdArrayHelpers.hpp"

// IWYU pragma: no_include <complex>

// IWYU pragma: no_forward_declare Strahlkorper
// IWYU pragma: no_forward_declare Tensor

// Functions used by StrahlkorperGr::dimensionful_spin_magnitude
namespace {
// Find the 2D surface metric by inserting the tangents \f$\partial_\theta\f$
// and \f$\partial_\phi\f$ into the slots of the 3D spatial metric
template <typename Fr>
tnsr::ii<DataVector, 2, Frame::Spherical<Fr>> get_surface_metric(
    const tnsr::ii<DataVector, 3, Fr>& spatial_metric,
    const StrahlkorperTags::aliases::Jacobian<Fr>& tangents,
    const Scalar<DataVector>& sin_theta) {
  auto surface_metric =
      make_with_value<tnsr::ii<DataVector, 2, Frame::Spherical<Fr>>>(
          get<0, 0>(spatial_metric), 0.0);
  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = 0; j < 3; ++j) {
      get<0, 1>(surface_metric) += spatial_metric.get(i, j) *
                                   tangents.get(i, 0) * tangents.get(j, 1) *
                                   get(sin_theta);
    }
    // Use symmetry to sum over fewer terms for the 0,0 and 1,1 components
    get<0, 0>(surface_metric) +=
        spatial_metric.get(i, i) * square(tangents.get(i, 0));
    get<1, 1>(surface_metric) += spatial_metric.get(i, i) *
                                 square(tangents.get(i, 1)) *
                                 square(get(sin_theta));
    for (size_t j = i + 1; j < 3; ++j) {
      get<0, 0>(surface_metric) += 2.0 * spatial_metric.get(i, j) *
                                   tangents.get(i, 0) * tangents.get(j, 0);
      get<1, 1>(surface_metric) += 2.0 * spatial_metric.get(i, j) *
                                   tangents.get(i, 1) * tangents.get(j, 1) *
                                   square(get(sin_theta));
    }
  }
  return surface_metric;
}

// Compute the trace of Christoffel 2nd kind on the horizon
template <typename Fr>
tnsr::I<DataVector, 2, Frame::Spherical<Fr>> get_trace_christoffel_second_kind(
    const tnsr::ii<DataVector, 2, Frame::Spherical<Fr>>& surface_metric,
    const tnsr::II<DataVector, 2, Frame::Spherical<Fr>>& inverse_surface_metric,
    const Scalar<DataVector>& sin_theta, const YlmSpherepack& ylm) {
  const Scalar<DataVector> cos_theta{cos(ylm.theta_phi_points()[0])};

  // Because the surface metric components are not representable in terms
  // of scalar spherical harmonics, you can't naively take first derivatives.
  // To avoid potentially large numerical errors, actually differentiate
  // square(sin_theta) * the metric component, then
  // compute from that the gradient of just the metric component itself.
  //
  // Note: the method implemented here works with YlmSpherepack but will fail
  // for other expansions that, unlike Spherepack, include a collocation
  // point at theta = 0. Before switching to such an expansion, first
  // reimplement this code to avoid dividing by sin(theta).
  //
  // Note: YlmSpherepack gradients are flat-space Pfaffian derivatives.
  auto grad_surface_metric_theta_theta =
      ylm.gradient(square(get(sin_theta)) * get<0, 0>(surface_metric));
  get<0>(grad_surface_metric_theta_theta) /= square(get(sin_theta));
  get<1>(grad_surface_metric_theta_theta) /= square(get(sin_theta));
  get<0>(grad_surface_metric_theta_theta) -=
      2.0 * get<0, 0>(surface_metric) * get(cos_theta) / get(sin_theta);

  auto grad_surface_metric_theta_phi =
      ylm.gradient(get(sin_theta) * get<0, 1>(surface_metric));

  get<0>(grad_surface_metric_theta_phi) /= get(sin_theta);
  get<1>(grad_surface_metric_theta_phi) /= get(sin_theta);
  get<0>(grad_surface_metric_theta_phi) -=
      get<0, 1>(surface_metric) * get(cos_theta) / get(sin_theta);

  auto grad_surface_metric_phi_phi = ylm.gradient(get<1, 1>(surface_metric));

  auto deriv_surface_metric =
      make_with_value<tnsr::ijj<DataVector, 2, Frame::Spherical<Fr>>>(
          get<0, 0>(surface_metric), 0.0);
  // Get the partial derivative of the metric from the Pfaffian derivative
  get<0, 0, 0>(deriv_surface_metric) = get<0>(grad_surface_metric_theta_theta);
  get<1, 0, 0>(deriv_surface_metric) =
      get(sin_theta) * get<1>(grad_surface_metric_theta_theta);
  get<0, 0, 1>(deriv_surface_metric) = get<0>(grad_surface_metric_theta_phi);
  get<1, 0, 1>(deriv_surface_metric) =
      get(sin_theta) * get<1>(grad_surface_metric_theta_phi);
  get<0, 1, 1>(deriv_surface_metric) = get<0>(grad_surface_metric_phi_phi);
  get<1, 1, 1>(deriv_surface_metric) =
      get(sin_theta) * get<1>(grad_surface_metric_phi_phi);

  return trace_last_indices(
      raise_or_lower_first_index(
          gr::christoffel_first_kind(deriv_surface_metric),
          inverse_surface_metric),
      inverse_surface_metric);
}

// I'm going to solve a general eigenvalue problem of the form
// A x = lambda B x, where A and B are NxN, where N is the
// number of elements with l > 0 and l < ntheta - 2,
// i.e. l < l_max + 1 - 2 = l_max - 1. This function computes N.
size_t get_matrix_dimension(const YlmSpherepack& ylm) {
  // If l_max == m_max, there are square(l_max+1) Ylms total
  size_t matrix_dimension = square(ylm.l_max() + 1);
  // If l_max > m_max, there are
  // (l_max - m_max) * (l_max - m_max + 1) fewer Ylms total
  matrix_dimension -=
      (ylm.l_max() - ylm.m_max()) * (ylm.l_max() - ylm.m_max() + 1);
  // The actual matrix dimension is smaller, because we do not count
  // Ylms with l == 0, l == l_max, or l == l_max - 1.
  matrix_dimension -= 4 * ylm.m_max() + 3;
  if (ylm.l_max() == ylm.m_max()) {
    matrix_dimension += 2;
  }
  return matrix_dimension;
}

// Get left matrix A and right matrix B for eigenproblem A x = lambda B x.
template <typename Fr>
void get_left_and_right_eigenproblem_matrices(
    const gsl::not_null<Matrix*> left_matrix,
    const gsl::not_null<Matrix*> right_matrix,
    const tnsr::II<DataVector, 2, Frame::Spherical<Fr>>& inverse_surface_metric,
    const tnsr::I<DataVector, 2, Frame::Spherical<Fr>>&
        trace_christoffel_second_kind,
    const Scalar<DataVector>& sin_theta, const Scalar<DataVector>& ricci_scalar,
    const YlmSpherepack& ylm) {
  const auto grad_ricci_scalar = ylm.gradient(get(ricci_scalar));
  // loop over all terms with 0<l<l_max-1: each makes a column of
  // the matrices for the eigenvalue problem
  size_t column = 0;  // number which column of the matrix we are filling
  for (auto iter_i = SpherepackIterator(ylm.l_max(), ylm.m_max()); iter_i;
       ++iter_i) {
    if (iter_i.l() > 0 and iter_i.l() < ylm.l_max() - 1 and
        iter_i.m() <= iter_i.l()) {
      // Make a spectral vector that's all zeros except for one element,
      // which is 1. This corresponds to the ith Ylm, which I call yi.
      DataVector yi_spectral(ylm.spectral_size(), 0.0);
      yi_spectral[iter_i()] = 1.0;

      // Transform column vector corresponding to
      // a specific Y_lm to physical space.
      const DataVector yi_physical = ylm.spec_to_phys(yi_spectral);

      // In physical space, numerically compute the
      // linear differential operators acting on the
      // ith Y_lm.

      // \nabla^2 Y_lm
      const auto derivs_yi = ylm.first_and_second_derivative(yi_physical);
      auto laplacian_yi =
          make_with_value<Scalar<DataVector>>(ricci_scalar, 0.0);
      get(laplacian_yi) +=
          get<0, 0>(derivs_yi.second) * get<0, 0>(inverse_surface_metric);
      get(laplacian_yi) += 2.0 * get<1, 0>(derivs_yi.second) *
                           get<1, 0>(inverse_surface_metric) * get(sin_theta);
      get(laplacian_yi) += get<1, 1>(derivs_yi.second) *
                           get<1, 1>(inverse_surface_metric) *
                           square(get(sin_theta));
      get(laplacian_yi) -=
          get<0>(derivs_yi.first) * get<0>(trace_christoffel_second_kind);
      get(laplacian_yi) -= get<1>(derivs_yi.first) * get(sin_theta) *
                           get<1>(trace_christoffel_second_kind);

      // \nabla^4 Y_lm
      const auto derivs_laplacian_yi =
          ylm.first_and_second_derivative(get(laplacian_yi));
      auto laplacian_squared_yi =
          make_with_value<Scalar<DataVector>>(ricci_scalar, 0.0);
      get(laplacian_squared_yi) += get<0, 0>(derivs_laplacian_yi.second) *
                                   get<0, 0>(inverse_surface_metric);
      get(laplacian_squared_yi) += 2.0 * get<1, 0>(derivs_laplacian_yi.second) *
                                   get<1, 0>(inverse_surface_metric) *
                                   get(sin_theta);
      get(laplacian_squared_yi) += get<1, 1>(derivs_laplacian_yi.second) *
                                   get<1, 1>(inverse_surface_metric) *
                                   square(get(sin_theta));
      get(laplacian_squared_yi) -= get<0>(derivs_laplacian_yi.first) *
                                   get<0>(trace_christoffel_second_kind);
      get(laplacian_squared_yi) -= get<1>(derivs_laplacian_yi.first) *
                                   get(sin_theta) *
                                   get<1>(trace_christoffel_second_kind);

      // \nabla R \cdot \nabla Y_lm
      auto grad_ricci_scalar_dot_grad_yi =
          make_with_value<Scalar<DataVector>>(ricci_scalar, 0.0);
      get(grad_ricci_scalar_dot_grad_yi) += get<0>(derivs_yi.first) *
                                            get<0>(grad_ricci_scalar) *
                                            get<0, 0>(inverse_surface_metric);
      get(grad_ricci_scalar_dot_grad_yi) +=
          get<0>(derivs_yi.first) * get<1>(grad_ricci_scalar) *
          get<1, 0>(inverse_surface_metric) * get(sin_theta);
      get(grad_ricci_scalar_dot_grad_yi) +=
          get<1>(derivs_yi.first) * get<0>(grad_ricci_scalar) *
          get<1, 0>(inverse_surface_metric) * get(sin_theta);
      get(grad_ricci_scalar_dot_grad_yi) +=
          get<1>(derivs_yi.first) * get<1>(grad_ricci_scalar) *
          get<1, 1>(inverse_surface_metric) * square(get(sin_theta));

      // Assemble the operator making up the eigenproblem's left-hand-side
      auto left_matrix_yi_physical =
          make_with_value<Scalar<DataVector>>(ricci_scalar, 0.0);
      get(left_matrix_yi_physical) = get(laplacian_squared_yi) +
                                     get(ricci_scalar) * get(laplacian_yi) +
                                     get(grad_ricci_scalar_dot_grad_yi);

      // Transform back to spectral space, to get one column each for the left
      // and right matrices for the eigenvalue problem.
      const DataVector left_matrix_yi_spectral =
          ylm.phys_to_spec(get(left_matrix_yi_physical));
      const DataVector right_matrix_yi_spectral =
          ylm.phys_to_spec(get(laplacian_yi));

      // Set the current column of the left and right matrices
      // for the eigenproblem.
      size_t row = 0;
      for (auto iter_j = SpherepackIterator(ylm.l_max(), ylm.m_max()); iter_j;
           ++iter_j) {
        if (iter_j.l() > 0 and iter_j.l() < ylm.l_max() - 1) {
          (*left_matrix)(row, column) = left_matrix_yi_spectral[iter_j()];
          (*right_matrix)(row, column) = right_matrix_yi_spectral[iter_j()];
          ++row;
        }
      }  // loop over rows
      ++column;
    }
  }  // loop over columns
}

// Find the eigenvectors corresponding to the three smallest-magnitude
// eigenvalues.
// Note: uses the fact that eigenvalues should be real
std::array<DataVector, 3> get_eigenvectors_for_3_smallest_magnitude_eigenvalues(
    const DataVector& eigenvalues_real_part, const Matrix& eigenvectors,
    const YlmSpherepack& ylm) {
  size_t index_smallest = 0;
  size_t index_second_smallest = 0;
  size_t index_third_smallest = 0;

  // Simple algorithm that loops over all elements to
  // find indexes of 3 smallest-magnitude eigenvalues
  for (size_t i = 1; i < eigenvalues_real_part.size(); ++i) {
    if (abs(eigenvalues_real_part[i]) <
        abs(eigenvalues_real_part[index_smallest])) {
      index_third_smallest = index_second_smallest;
      index_second_smallest = index_smallest;
      index_smallest = i;
    } else if (i < 2 or abs(eigenvalues_real_part[i]) <
                            abs(eigenvalues_real_part[index_second_smallest])) {
      index_third_smallest = index_second_smallest;
      index_second_smallest = i;
    } else if (i < 3 or abs(eigenvalues_real_part[i]) <
                            abs(eigenvalues_real_part[index_third_smallest])) {
      index_third_smallest = i;
    }
  }

  DataVector smallest_eigenvector(ylm.spectral_size(), 0.0);
  DataVector second_smallest_eigenvector(ylm.spectral_size(), 0.0);
  DataVector third_smallest_eigenvector(ylm.spectral_size(), 0.0);

  size_t row = 0;

  for (auto iter_i = SpherepackIterator(ylm.l_max(), ylm.m_max()); iter_i;
       ++iter_i) {
    if (iter_i.l() > 0 and iter_i.l() < ylm.l_max() - 1) {
      smallest_eigenvector[iter_i()] = eigenvectors(row, index_smallest);
      second_smallest_eigenvector[iter_i()] =
          eigenvectors(row, index_second_smallest);
      third_smallest_eigenvector[iter_i()] =
          eigenvectors(row, index_third_smallest);
      ++row;
    }
  }

  return {{smallest_eigenvector, second_smallest_eigenvector,
           third_smallest_eigenvector}};
}

// This function converts the three eigenvectors with smallest-magnitude
// eigenvalues to physical space to get the spin potentials corresponding to
// the approximate Killing vectors. The potentials are normalized using the
// "Kerr normalization:" the integral of (potential - the potential average)^2
// is set to (horizon area)^3/(48*pi), as it is for Kerr.
std::array<DataVector, 3> get_normalized_spin_potentials(
    const std::array<DataVector, 3>& eigenvectors_for_potentials,
    const YlmSpherepack& ylm, const Scalar<DataVector>& area_element) {
  const double area = ylm.definite_integral(get(area_element).data());

  std::array<DataVector, 3> potentials;

  DataVector temp_integrand(get(area_element));
  for (size_t i = 0; i < 3; ++i) {
    gsl::at(potentials, i) =
        ylm.spec_to_phys(gsl::at(eigenvectors_for_potentials, i));

    temp_integrand = gsl::at(potentials, i) * get(area_element);
    const double potential_average =
        ylm.definite_integral(temp_integrand.data()) / area;

    temp_integrand =
        square(gsl::at(potentials, i) - potential_average) * get(area_element);
    const double potential_norm = ylm.definite_integral(temp_integrand.data());
    gsl::at(potentials, i) *=
        sqrt(cube(area) / (48.0 * square(M_PI) * potential_norm));
  }
  return potentials;
}

// Get the spin magnitude. There are three potentials, each corresponding to an
// approximate Killing vector. The spin for each potential is the surface
// integral of the potential times the spin function. The spin magnitude
// is the Euclidean norm of the spin for each potential.
double get_spin_magnitude(const std::array<DataVector, 3>& potentials,
                          const Scalar<DataVector>& spin_function,
                          const Scalar<DataVector>& area_element,
                          const YlmSpherepack& ylm) {
  double spin_magnitude_squared = 0.0;

  DataVector spin_density(get(area_element));
  for (size_t i = 0; i < 3; ++i) {
    spin_density =
        gsl::at(potentials, i) * get(spin_function) * get(area_element);
    spin_magnitude_squared +=
        square(ylm.definite_integral(spin_density.data()) / (8.0 * M_PI));
  }
  return sqrt(spin_magnitude_squared);
}
}  // namespace

namespace StrahlkorperGr {

template <typename Frame>
void unit_normal_one_form(
    const gsl::not_null<tnsr::i<DataVector, 3, Frame>*> result,
    const tnsr::i<DataVector, 3, Frame>& normal_one_form,
    const DataVector& one_over_one_form_magnitude) {
  *result = normal_one_form;
  for (size_t i = 0; i < 3; ++i) {
    result->get(i) *= one_over_one_form_magnitude;
  }
}

template <typename Frame>
tnsr::i<DataVector, 3, Frame> unit_normal_one_form(
    const tnsr::i<DataVector, 3, Frame>& normal_one_form,
    const DataVector& one_over_one_form_magnitude) {
  tnsr::i<DataVector, 3, Frame> result{};
  unit_normal_one_form(make_not_null(&result), normal_one_form,
                       one_over_one_form_magnitude);
  return result;
}

template <typename Frame>
void grad_unit_normal_one_form(
    const gsl::not_null<tnsr::ii<DataVector, 3, Frame>*> result,
    const tnsr::i<DataVector, 3, Frame>& r_hat,
    const Scalar<DataVector>& radius,
    const tnsr::i<DataVector, 3, Frame>& unit_normal_one_form,
    const tnsr::ii<DataVector, 3, Frame>& d2x_radius,
    const DataVector& one_over_one_form_magnitude,
    const tnsr::Ijj<DataVector, 3, Frame>& christoffel_2nd_kind) {
  destructive_resize_components(result, radius.size());
  const DataVector one_over_radius = 1.0 / get(radius);
  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = i; j < 3; ++j) {  // symmetry
      result->get(i, j) = -one_over_one_form_magnitude *
                          (r_hat.get(i) * r_hat.get(j) * one_over_radius +
                           d2x_radius.get(i, j));
      for (size_t k = 0; k < 3; ++k) {
        result->get(i, j) -=
            unit_normal_one_form.get(k) * christoffel_2nd_kind.get(k, i, j);
      }
    }
    result->get(i, i) += one_over_radius * one_over_one_form_magnitude;
  }
}

template <typename Frame>
tnsr::ii<DataVector, 3, Frame> grad_unit_normal_one_form(
    const tnsr::i<DataVector, 3, Frame>& r_hat,
    const Scalar<DataVector>& radius,
    const tnsr::i<DataVector, 3, Frame>& unit_normal_one_form,
    const tnsr::ii<DataVector, 3, Frame>& d2x_radius,
    const DataVector& one_over_one_form_magnitude,
    const tnsr::Ijj<DataVector, 3, Frame>& christoffel_2nd_kind) {
  tnsr::ii<DataVector, 3, Frame> result{};
  grad_unit_normal_one_form(make_not_null(&result), r_hat, radius,
                            unit_normal_one_form, d2x_radius,
                            one_over_one_form_magnitude, christoffel_2nd_kind);
  return result;
}

template <typename Frame>
void inverse_surface_metric(
    const gsl::not_null<tnsr::II<DataVector, 3, Frame>*> result,
    const tnsr::I<DataVector, 3, Frame>& unit_normal_vector,
    const tnsr::II<DataVector, 3, Frame>& upper_spatial_metric) {
  *result = upper_spatial_metric;
  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = i; j < 3; ++j) {  // Symmetry
      result->get(i, j) -=
          unit_normal_vector.get(i) * unit_normal_vector.get(j);
    }
  }
}

template <typename Frame>
tnsr::II<DataVector, 3, Frame> inverse_surface_metric(
    const tnsr::I<DataVector, 3, Frame>& unit_normal_vector,
    const tnsr::II<DataVector, 3, Frame>& upper_spatial_metric) {
  tnsr::II<DataVector, 3, Frame> result{};
  inverse_surface_metric(make_not_null(&result), unit_normal_vector,
                         upper_spatial_metric);
  return result;
}

template <typename Frame>
void expansion(const gsl::not_null<Scalar<DataVector>*> result,
               const tnsr::ii<DataVector, 3, Frame>& grad_normal,
               const tnsr::II<DataVector, 3, Frame>& inverse_surface_metric,
               const tnsr::ii<DataVector, 3, Frame>& extrinsic_curvature) {
  // If you want the future *ingoing* null expansion,
  // the formula is the same as here except you
  // change the sign on grad_normal just before you
  // subtract the extrinsic curvature.
  // That is, if GsBar is the value of grad_normal
  // at this point in the code, and S^i is the unit
  // spatial normal to the surface,
  // the outgoing expansion is
  // (g^ij - S^i S^j) (GsBar_ij - K_ij)
  // and the ingoing expansion is
  // (g^ij - S^i S^j) (-GsBar_ij - K_ij)
  destructive_resize_components(result, grad_normal.begin()->size());
  get(*result) = 0.0;
  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = 0; j < 3; ++j) {
      get(*result) += inverse_surface_metric.get(i, j) *
                      (grad_normal.get(i, j) - extrinsic_curvature.get(i, j));
    }
  }
}

template <typename Frame>
Scalar<DataVector> expansion(
    const tnsr::ii<DataVector, 3, Frame>& grad_normal,
    const tnsr::II<DataVector, 3, Frame>& inverse_surface_metric,
    const tnsr::ii<DataVector, 3, Frame>& extrinsic_curvature) {
  Scalar<DataVector> result{};
  expansion(make_not_null(&result), grad_normal, inverse_surface_metric,
            extrinsic_curvature);
  return result;
}

template <typename Frame>
void extrinsic_curvature(
    const gsl::not_null<tnsr::ii<DataVector, 3, Frame>*> result,
    const tnsr::ii<DataVector, 3, Frame>& grad_normal,
    const tnsr::i<DataVector, 3, Frame>& unit_normal_one_form,
    const tnsr::I<DataVector, 3, Frame>& unit_normal_vector) {
  Scalar<DataVector> nI_nJ_gradnij(get<0, 0>(grad_normal).size(), 0.0);
  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = 0; j < 3; ++j) {
      get(nI_nJ_gradnij) += unit_normal_vector.get(i) *
                            unit_normal_vector.get(j) * grad_normal.get(i, j);
    }
  }

  *result = grad_normal;
  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = i; j < 3; ++j) {
      result->get(i, j) += unit_normal_one_form.get(i) *
                           unit_normal_one_form.get(j) * get(nI_nJ_gradnij);
      for (size_t k = 0; k < 3; ++k) {
        result->get(i, j) -=
            unit_normal_vector.get(k) *
            (unit_normal_one_form.get(i) * grad_normal.get(j, k) +
             unit_normal_one_form.get(j) * grad_normal.get(i, k));
      }
    }
  }
}

template <typename Frame>
tnsr::ii<DataVector, 3, Frame> extrinsic_curvature(
    const tnsr::ii<DataVector, 3, Frame>& grad_normal,
    const tnsr::i<DataVector, 3, Frame>& unit_normal_one_form,
    const tnsr::I<DataVector, 3, Frame>& unit_normal_vector) {
  tnsr::ii<DataVector, 3, Frame> result{};
  extrinsic_curvature(make_not_null(&result), grad_normal, unit_normal_one_form,
                      unit_normal_vector);
  return result;
}

template <typename Frame>
void ricci_scalar(const gsl::not_null<Scalar<DataVector>*> result,
                  const tnsr::ii<DataVector, 3, Frame>& spatial_ricci_tensor,
                  const tnsr::I<DataVector, 3, Frame>& unit_normal_vector,
                  const tnsr::ii<DataVector, 3, Frame>& extrinsic_curvature,
                  const tnsr::II<DataVector, 3, Frame>& upper_spatial_metric) {
  trace(result, spatial_ricci_tensor, upper_spatial_metric);
  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = 0; j < 3; ++j) {
      get(*result) -= 2.0 * spatial_ricci_tensor.get(i, j) *
                      unit_normal_vector.get(i) * unit_normal_vector.get(j);

      for (size_t k = 0; k < 3; ++k) {
        for (size_t l = 0; l < 3; ++l) {
          // K^{ij} K_{ij} = g^{ik} g^{jl} K_{kl} K_{ij}
          get(*result) -=
              upper_spatial_metric.get(i, k) * upper_spatial_metric.get(j, l) *
              extrinsic_curvature.get(k, l) * extrinsic_curvature.get(i, j);
        }
      }
    }
  }
  get(*result) += square(get(trace(extrinsic_curvature, upper_spatial_metric)));
}

template <typename Frame>
Scalar<DataVector> ricci_scalar(
    const tnsr::ii<DataVector, 3, Frame>& spatial_ricci_tensor,
    const tnsr::I<DataVector, 3, Frame>& unit_normal_vector,
    const tnsr::ii<DataVector, 3, Frame>& extrinsic_curvature,
    const tnsr::II<DataVector, 3, Frame>& upper_spatial_metric) {
  Scalar<DataVector> result{};
  ricci_scalar(make_not_null(&result), spatial_ricci_tensor, unit_normal_vector,
               extrinsic_curvature, upper_spatial_metric);
  return result;
}

template <typename Frame>
void area_element(const gsl::not_null<Scalar<DataVector>*> result,
                  const tnsr::ii<DataVector, 3, Frame>& spatial_metric,
                  const StrahlkorperTags::aliases::Jacobian<Frame>& jacobian,
                  const tnsr::i<DataVector, 3, Frame>& normal_one_form,
                  const Scalar<DataVector>& radius,
                  const tnsr::i<DataVector, 3, Frame>& r_hat) {
  auto cap_theta = make_with_value<tnsr::I<DataVector, 3, Frame>>(r_hat, 0.0);
  auto cap_phi = make_with_value<tnsr::I<DataVector, 3, Frame>>(r_hat, 0.0);

  for (size_t i = 0; i < 3; ++i) {
    cap_theta.get(i) = jacobian.get(i, 0);
    cap_phi.get(i) = jacobian.get(i, 1);
    for (size_t j = 0; j < 3; ++j) {
      cap_theta.get(i) += r_hat.get(i) *
                          (r_hat.get(j) - normal_one_form.get(j)) *
                          jacobian.get(j, 0);
      cap_phi.get(i) += r_hat.get(i) * (r_hat.get(j) - normal_one_form.get(j)) *
                        jacobian.get(j, 1);
    }
  }

  get(*result) = square(get(radius));
  get(*result) *=
      sqrt(get(dot_product(cap_theta, cap_theta, spatial_metric)) *
               get(dot_product(cap_phi, cap_phi, spatial_metric)) -
           square(get(dot_product(cap_theta, cap_phi, spatial_metric))));
}

template <typename Frame>
Scalar<DataVector> area_element(
    const tnsr::ii<DataVector, 3, Frame>& spatial_metric,
    const StrahlkorperTags::aliases::Jacobian<Frame>& jacobian,
    const tnsr::i<DataVector, 3, Frame>& normal_one_form,
    const Scalar<DataVector>& radius,
    const tnsr::i<DataVector, 3, Frame>& r_hat) {
  Scalar<DataVector> result{};
  area_element(make_not_null(&result), spatial_metric, jacobian,
               normal_one_form, radius, r_hat);
  return result;
}

template <typename Frame>
void euclidean_area_element(
    const gsl::not_null<Scalar<DataVector>*> result,
    const StrahlkorperTags::aliases::Jacobian<Frame>& jacobian,
    const tnsr::i<DataVector, 3, Frame>& normal_one_form,
    const Scalar<DataVector>& radius,
    const tnsr::i<DataVector, 3, Frame>& r_hat) {
  auto cap_theta = make_with_value<tnsr::I<DataVector, 3, Frame>>(r_hat, 0.0);
  auto cap_phi = make_with_value<tnsr::I<DataVector, 3, Frame>>(r_hat, 0.0);

  for (size_t i = 0; i < 3; ++i) {
    cap_theta.get(i) = jacobian.get(i, 0);
    cap_phi.get(i) = jacobian.get(i, 1);
    for (size_t j = 0; j < 3; ++j) {
      cap_theta.get(i) += r_hat.get(i) *
                          (r_hat.get(j) - normal_one_form.get(j)) *
                          jacobian.get(j, 0);
      cap_phi.get(i) += r_hat.get(i) * (r_hat.get(j) - normal_one_form.get(j)) *
                        jacobian.get(j, 1);
    }
  }

  get(*result) = square(get(radius));
  get(*result) *= sqrt(get(dot_product(cap_theta, cap_theta)) *
                           get(dot_product(cap_phi, cap_phi)) -
                       square(get(dot_product(cap_theta, cap_phi))));
}

template <typename Frame>
Scalar<DataVector> euclidean_area_element(
    const StrahlkorperTags::aliases::Jacobian<Frame>& jacobian,
    const tnsr::i<DataVector, 3, Frame>& normal_one_form,
    const Scalar<DataVector>& radius,
    const tnsr::i<DataVector, 3, Frame>& r_hat) {
  Scalar<DataVector> result{};
  euclidean_area_element(make_not_null(&result), jacobian, normal_one_form,
                         radius, r_hat);
  return result;
}

template <typename Frame>
double surface_integral_of_scalar(const Scalar<DataVector>& area_element,
                                  const Scalar<DataVector>& scalar,
                                  const Strahlkorper<Frame>& strahlkorper) {
  const DataVector integrand = get(area_element) * get(scalar);
  return strahlkorper.ylm_spherepack().definite_integral(integrand.data());
}

template <typename Frame>
double euclidean_surface_integral_of_vector(
    const Scalar<DataVector>& area_element,
    const tnsr::I<DataVector, 3, Frame>& vector,
    const tnsr::i<DataVector, 3, Frame>& normal_one_form,
    const Strahlkorper<Frame>& strahlkorper) {
  const DataVector integrand =
      get(area_element) * get(dot_product(vector, normal_one_form)) /
      sqrt(get(dot_product(normal_one_form, normal_one_form)));
  return strahlkorper.ylm_spherepack().definite_integral(integrand.data());
}

template <typename Frame>
void spin_function(const gsl::not_null<Scalar<DataVector>*> result,
                   const StrahlkorperTags::aliases::Jacobian<Frame>& tangents,
                   const Strahlkorper<Frame>& strahlkorper,
                   const tnsr::I<DataVector, 3, Frame>& unit_normal_vector,
                   const Scalar<DataVector>& area_element,
                   const tnsr::ii<DataVector, 3, Frame>& extrinsic_curvature) {
  destructive_resize_components(result, get(area_element).size());
  for (auto& component : *result) {
    component = 0.0;
  }

  auto extrinsic_curvature_theta_normal_sin_theta =
      make_with_value<Scalar<DataVector>>(area_element, 0.0);
  auto extrinsic_curvature_phi_normal =
      make_with_value<Scalar<DataVector>>(area_element, 0.0);

  // using result as temporary
  DataVector& extrinsic_curvature_dot_normal = get(*result);
  for (size_t i = 0; i < 3; ++i) {
    extrinsic_curvature_dot_normal =
        extrinsic_curvature.get(i, 0) * get<0>(unit_normal_vector);
    for (size_t j = 1; j < 3; ++j) {
      extrinsic_curvature_dot_normal +=
          extrinsic_curvature.get(i, j) * unit_normal_vector.get(j);
    }

    // Note: I must multiply by sin_theta because
    // I take the phi derivative of this term by using
    // the spherepack gradient, which includes a
    // sin_theta in the denominator of the phi derivative.
    // Will do this outside the i,j loops.
    get(extrinsic_curvature_theta_normal_sin_theta) +=
        extrinsic_curvature_dot_normal * tangents.get(i, 0);

    // Note: I must multiply by sin_theta because tangents.get(i,1)
    // actually contains \partial_\phi / sin(theta), but I want just
    //\partial_\phi. Will do this outside the i,j loops.
    get(extrinsic_curvature_phi_normal) +=
        extrinsic_curvature_dot_normal * tangents.get(i, 1);
  }

  // using result as temporary
  DataVector& sin_theta = get(*result);
  sin_theta = sin(strahlkorper.ylm_spherepack().theta_phi_points()[0]);
  get(extrinsic_curvature_theta_normal_sin_theta) *= sin_theta;
  get(extrinsic_curvature_phi_normal) *= sin_theta;

  // now computing actual result
  get(*result) = (get<0>(strahlkorper.ylm_spherepack().gradient(
                      get(extrinsic_curvature_phi_normal))) -
                  get<1>(strahlkorper.ylm_spherepack().gradient(
                      get(extrinsic_curvature_theta_normal_sin_theta)))) /
                 (sin_theta * get(area_element));
}

template <typename Frame>
Scalar<DataVector> spin_function(
    const StrahlkorperTags::aliases::Jacobian<Frame>& tangents,
    const Strahlkorper<Frame>& strahlkorper,
    const tnsr::I<DataVector, 3, Frame>& unit_normal_vector,
    const Scalar<DataVector>& area_element,
    const tnsr::ii<DataVector, 3, Frame>& extrinsic_curvature) {
  Scalar<DataVector> result{};
  spin_function(make_not_null(&result), tangents, strahlkorper,
                unit_normal_vector, area_element, extrinsic_curvature);
  return result;
}

template <typename Frame>
void dimensionful_spin_magnitude(
    const gsl::not_null<double*> result, const Scalar<DataVector>& ricci_scalar,
    const Scalar<DataVector>& spin_function,
    const tnsr::ii<DataVector, 3, Frame>& spatial_metric,
    const StrahlkorperTags::aliases::Jacobian<Frame>& tangents,
    const Strahlkorper<Frame>& strahlkorper,
    const Scalar<DataVector>& area_element) {
  const Scalar<DataVector> sin_theta{
      sin(strahlkorper.ylm_spherepack().theta_phi_points()[0])};

  const auto surface_metric =
      get_surface_metric(spatial_metric, tangents, sin_theta);
  const auto inverse_surface_metric =
      determinant_and_inverse(surface_metric).second;
  const auto trace_christoffel_second_kind = get_trace_christoffel_second_kind(
      surface_metric, inverse_surface_metric, sin_theta,
      strahlkorper.ylm_spherepack());

  const size_t matrix_dimension =
      get_matrix_dimension(strahlkorper.ylm_spherepack());
  Matrix left_matrix(matrix_dimension, matrix_dimension, 0.0);
  Matrix right_matrix(matrix_dimension, matrix_dimension, 0.0);
  get_left_and_right_eigenproblem_matrices(
      &left_matrix, &right_matrix, inverse_surface_metric,
      trace_christoffel_second_kind, sin_theta, ricci_scalar,
      strahlkorper.ylm_spherepack());

  DataVector eigenvalues_real_part(matrix_dimension, 0.0);
  DataVector eigenvalues_im_part(matrix_dimension, 0.0);
  Matrix eigenvectors(matrix_dimension, matrix_dimension, 0.0);
  find_generalized_eigenvalues(&eigenvalues_real_part, &eigenvalues_im_part,
                               &eigenvectors, left_matrix, right_matrix);

  const std::array<DataVector, 3> smallest_eigenvectors =
      get_eigenvectors_for_3_smallest_magnitude_eigenvalues(
          eigenvalues_real_part, eigenvectors, strahlkorper.ylm_spherepack());

  // Get normalized potentials (Kerr normalization) corresponding to the
  // eigenvectors with three smallest-magnitude eigenvalues.
  const auto potentials = get_normalized_spin_potentials(
      smallest_eigenvectors, strahlkorper.ylm_spherepack(), area_element);

  *result = get_spin_magnitude(potentials, spin_function, area_element,
                               strahlkorper.ylm_spherepack());
}

template <typename Frame>
double dimensionful_spin_magnitude(
    const Scalar<DataVector>& ricci_scalar,
    const Scalar<DataVector>& spin_function,
    const tnsr::ii<DataVector, 3, Frame>& spatial_metric,
    const StrahlkorperTags::aliases::Jacobian<Frame>& tangents,
    const Strahlkorper<Frame>& strahlkorper,
    const Scalar<DataVector>& area_element) {
  double result{};
  dimensionful_spin_magnitude(make_not_null(&result), ricci_scalar,
                              spin_function, spatial_metric, tangents,
                              strahlkorper, area_element);
  return result;
}

template <typename Frame>
void spin_vector(const gsl::not_null<std::array<double, 3>*> result,
                 const double spin_magnitude,
                 const Scalar<DataVector>& area_element,
                 const Scalar<DataVector>& radius,
                 const tnsr::i<DataVector, 3, Frame>& r_hat,
                 const Scalar<DataVector>& ricci_scalar,
                 const Scalar<DataVector>& spin_function,
                 const Strahlkorper<Frame>& strahlkorper) {
  const auto& ylm = strahlkorper.ylm_spherepack();
  // Assert that the DataVectors in area_element, radius,
  // r_hat, ricci_scalar, and r_hat have the same size as the ylm size

  // get the ylm's physical size as a variable to reuse
  const size_t ylm_physical_size = ylm.physical_size();
  ASSERT(get(area_element).size() == ylm_physical_size,
         "area_element size doesn't match ylm physical size: "
             << get(area_element).size() << " vs " << ylm_physical_size);
  ASSERT(get(radius).size() == ylm_physical_size,
         "radius size doesn't match ylm physical size: "
             << get(radius).size() << " vs " << ylm_physical_size);
  ASSERT(get<0>(r_hat).size() == ylm_physical_size and
             get<1>(r_hat).size() == ylm_physical_size and
             get<2>(r_hat).size() == ylm_physical_size,
         "The size of at least one of r_hat's components doesn't match ylm "
         "physical size: "
             << "(" << get<0>(r_hat).size() << ", " << get<1>(r_hat).size()
             << ", " << get<2>(r_hat).size() << ") vs " << ylm_physical_size);
  ASSERT(get(ricci_scalar).size() == ylm_physical_size,
         "ricci_scalar size doesn't match ylm physical size: "
             << get(ricci_scalar).size() << " vs " << ylm_physical_size);
  ASSERT(get(spin_function).size() == ylm_physical_size,
         "spin_function size doesn't match ylm physical size: "
             << get(spin_function).size() << " vs " << ylm_physical_size);

  std::array<double, 3> spin_vector =
      make_array<3>(std::numeric_limits<double>::signaling_NaN());
  auto integrand = make_with_value<Scalar<DataVector>>(get(radius), 0.0);

  for (size_t i = 0; i < 3; ++i) {
    // Compute horizon coordinates with a coordinate center such that
    // the mass dipole moment vanishes.
    get(integrand) =
        get(area_element) * get(ricci_scalar) * r_hat.get(i) * get(radius);
    get(integrand) =
        ylm.definite_integral(get(integrand).data()) / (-8.0 * M_PI);
    get(integrand) += r_hat.get(i) * get(radius);

    // Get a component of a vector in the direction of the spin
    get(integrand) *= get(area_element) * get(spin_function);
    gsl::at(spin_vector, i) = ylm.definite_integral(get(integrand).data());
  }

  // Normalize spin_vector so its magnitude is the magnitude of the spin
  *result = spin_vector * (spin_magnitude / magnitude(spin_vector));
}

template <typename Frame>
std::array<double, 3> spin_vector(const double spin_magnitude,
                                  const Scalar<DataVector>& area_element,
                                  const Scalar<DataVector>& radius,
                                  const tnsr::i<DataVector, 3, Frame>& r_hat,
                                  const Scalar<DataVector>& ricci_scalar,
                                  const Scalar<DataVector>& spin_function,
                                  const Strahlkorper<Frame>& strahlkorper) {
  std::array<double, 3> result{};
  spin_vector(make_not_null(&result), spin_magnitude, area_element, radius,
              r_hat, ricci_scalar, spin_function, strahlkorper);
  return result;
}

double irreducible_mass(const double area) {
  ASSERT(area > 0.0,
         "The area of the horizon must be greater than zero but is " << area);
  return sqrt(area / (16.0 * M_PI));
}

double christodoulou_mass(const double dimensionful_spin_magnitude,
                          const double irreducible_mass) {
  return sqrt(square(irreducible_mass) + (square(dimensionful_spin_magnitude) /
                                          (4.0 * square(irreducible_mass))));
}

void dimensionless_spin_magnitude(const gsl::not_null<double*> result,
                                  const double dimensionful_spin_magnitude,
                                  const double christodoulou_mass) {
  *result = dimensionful_spin_magnitude / square(christodoulou_mass);
}

double dimensionless_spin_magnitude(const double dimensionful_spin_magnitude,
                                    const double christodoulou_mass) {
  double result{};
  dimensionless_spin_magnitude(make_not_null(&result),
                               dimensionful_spin_magnitude, christodoulou_mass);
  return result;
}


template <typename Frame>
void radial_distance(const gsl::not_null<Scalar<DataVector>*> radial_distance,
                     const Strahlkorper<Frame>& strahlkorper_a,
                     const Strahlkorper<Frame>& strahlkorper_b) {
  if (strahlkorper_a.expansion_center() != strahlkorper_b.expansion_center()) {
    ERROR(
        "Currently computing the radial distance between two Strahlkorpers "
        "is only supported if they have the same centers, but the "
        "strahlkorpers provided have centers "
        << strahlkorper_a.expansion_center() << " and "
        << strahlkorper_b.expansion_center());
  }
  get(*radial_distance)
      .destructive_resize(strahlkorper_a.ylm_spherepack().physical_size());
  if (strahlkorper_a.l_max() == strahlkorper_b.l_max() and
      strahlkorper_a.m_max() == strahlkorper_b.m_max()) {
    get(*radial_distance) = get(StrahlkorperFunctions::radius(strahlkorper_a)) -
                            get(StrahlkorperFunctions::radius(strahlkorper_b));
  } else if (strahlkorper_a.l_max() > strahlkorper_b.l_max() or
             (strahlkorper_a.l_max() == strahlkorper_b.l_max() and
              strahlkorper_a.m_max() > strahlkorper_b.m_max())) {
    get(*radial_distance) =
        get(StrahlkorperFunctions::radius(strahlkorper_a)) -
        get(StrahlkorperFunctions::radius(Strahlkorper<Frame>(
            strahlkorper_a.l_max(), strahlkorper_a.m_max(), strahlkorper_b)));
  } else {
    get(*radial_distance) =
        -get(StrahlkorperFunctions::radius(strahlkorper_b)) +
        get(StrahlkorperFunctions::radius(Strahlkorper<Frame>(
            strahlkorper_b.l_max(), strahlkorper_b.m_max(), strahlkorper_a)));
  };
}
}  // namespace StrahlkorperGr

#define FRAME(data) BOOST_PP_TUPLE_ELEM(0, data)
#define INSTANTIATE(_, data)                                                \
  template void StrahlkorperGr::unit_normal_one_form<FRAME(data)>(          \
      const gsl::not_null<tnsr::i<DataVector, 3, FRAME(data)>*> result,     \
      const tnsr::i<DataVector, 3, FRAME(data)>& normal_one_form,           \
      const DataVector& one_over_one_form_magnitude);                       \
  template tnsr::i<DataVector, 3, FRAME(data)>                              \
  StrahlkorperGr::unit_normal_one_form<FRAME(data)>(                        \
      const tnsr::i<DataVector, 3, FRAME(data)>& normal_one_form,           \
      const DataVector& one_over_one_form_magnitude);                       \
  template void StrahlkorperGr::grad_unit_normal_one_form<FRAME(data)>(     \
      gsl::not_null<tnsr::ii<DataVector, 3, FRAME(data)>*> result,          \
      const tnsr::i<DataVector, 3, FRAME(data)>& r_hat,                     \
      const Scalar<DataVector>& radius,                                     \
      const tnsr::i<DataVector, 3, FRAME(data)>& unit_normal_one_form,      \
      const tnsr::ii<DataVector, 3, FRAME(data)>& d2x_radius,               \
      const DataVector& one_over_one_form_magnitude,                        \
      const tnsr::Ijj<DataVector, 3, FRAME(data)>& christoffel_2nd_kind);   \
  template tnsr::ii<DataVector, 3, FRAME(data)>                             \
  StrahlkorperGr::grad_unit_normal_one_form<FRAME(data)>(                   \
      const tnsr::i<DataVector, 3, FRAME(data)>& r_hat,                     \
      const Scalar<DataVector>& radius,                                     \
      const tnsr::i<DataVector, 3, FRAME(data)>& unit_normal_one_form,      \
      const tnsr::ii<DataVector, 3, FRAME(data)>& d2x_radius,               \
      const DataVector& one_over_one_form_magnitude,                        \
      const tnsr::Ijj<DataVector, 3, FRAME(data)>& christoffel_2nd_kind);   \
  template void StrahlkorperGr::inverse_surface_metric<FRAME(data)>(        \
      const gsl::not_null<tnsr::II<DataVector, 3, FRAME(data)>*> result,    \
      const tnsr::I<DataVector, 3, FRAME(data)>& unit_normal_vector,        \
      const tnsr::II<DataVector, 3, FRAME(data)>& upper_spatial_metric);    \
  template tnsr::II<DataVector, 3, FRAME(data)>                             \
  StrahlkorperGr::inverse_surface_metric<FRAME(data)>(                      \
      const tnsr::I<DataVector, 3, FRAME(data)>& unit_normal_vector,        \
      const tnsr::II<DataVector, 3, FRAME(data)>& upper_spatial_metric);    \
  template void StrahlkorperGr::expansion<FRAME(data)>(                     \
      const gsl::not_null<Scalar<DataVector>*> result,                      \
      const tnsr::ii<DataVector, 3, FRAME(data)>& grad_normal,              \
      const tnsr::II<DataVector, 3, FRAME(data)>& inverse_surface_metric,   \
      const tnsr::ii<DataVector, 3, FRAME(data)>& extrinsic_curvature);     \
  template Scalar<DataVector> StrahlkorperGr::expansion<FRAME(data)>(       \
      const tnsr::ii<DataVector, 3, FRAME(data)>& grad_normal,              \
      const tnsr::II<DataVector, 3, FRAME(data)>& inverse_surface_metric,   \
      const tnsr::ii<DataVector, 3, FRAME(data)>& extrinsic_curvature);     \
  template void StrahlkorperGr::extrinsic_curvature<FRAME(data)>(           \
      const gsl::not_null<tnsr::ii<DataVector, 3, FRAME(data)>*> result,    \
      const tnsr::ii<DataVector, 3, FRAME(data)>& grad_normal,              \
      const tnsr::i<DataVector, 3, FRAME(data)>& unit_normal_one_form,      \
      const tnsr::I<DataVector, 3, FRAME(data)>& unit_normal_vector);       \
  template tnsr::ii<DataVector, 3, FRAME(data)>                             \
  StrahlkorperGr::extrinsic_curvature<FRAME(data)>(                         \
      const tnsr::ii<DataVector, 3, FRAME(data)>& grad_normal,              \
      const tnsr::i<DataVector, 3, FRAME(data)>& unit_normal_one_form,      \
      const tnsr::I<DataVector, 3, FRAME(data)>& unit_normal_vector);       \
  template void StrahlkorperGr::ricci_scalar<FRAME(data)>(                  \
      const gsl::not_null<Scalar<DataVector>*> result,                      \
      const tnsr::ii<DataVector, 3, FRAME(data)>& spatial_ricci_tensor,     \
      const tnsr::I<DataVector, 3, FRAME(data)>& unit_normal_vector,        \
      const tnsr::ii<DataVector, 3, FRAME(data)>& extrinsic_curvature,      \
      const tnsr::II<DataVector, 3, FRAME(data)>& upper_spatial_metric);    \
  template Scalar<DataVector> StrahlkorperGr::ricci_scalar<FRAME(data)>(    \
      const tnsr::ii<DataVector, 3, FRAME(data)>& spatial_ricci_tensor,     \
      const tnsr::I<DataVector, 3, FRAME(data)>& unit_normal_vector,        \
      const tnsr::ii<DataVector, 3, FRAME(data)>& extrinsic_curvature,      \
      const tnsr::II<DataVector, 3, FRAME(data)>& upper_spatial_metric);    \
  template void StrahlkorperGr::area_element<FRAME(data)>(                  \
      const gsl::not_null<Scalar<DataVector>*> result,                      \
      const tnsr::ii<DataVector, 3, FRAME(data)>& spatial_metric,           \
      const StrahlkorperTags::aliases::Jacobian<FRAME(data)>& jacobian,     \
      const tnsr::i<DataVector, 3, FRAME(data)>& normal_one_form,           \
      const Scalar<DataVector>& radius,                                     \
      const tnsr::i<DataVector, 3, FRAME(data)>& r_hat);                    \
  template Scalar<DataVector> StrahlkorperGr::area_element<FRAME(data)>(    \
      const tnsr::ii<DataVector, 3, FRAME(data)>& spatial_metric,           \
      const StrahlkorperTags::aliases::Jacobian<FRAME(data)>& jacobian,     \
      const tnsr::i<DataVector, 3, FRAME(data)>& normal_one_form,           \
      const Scalar<DataVector>& radius,                                     \
      const tnsr::i<DataVector, 3, FRAME(data)>& r_hat);                    \
  template void StrahlkorperGr::euclidean_area_element<FRAME(data)>(        \
      const gsl::not_null<Scalar<DataVector>*> result,                      \
      const StrahlkorperTags::aliases::Jacobian<FRAME(data)>& jacobian,     \
      const tnsr::i<DataVector, 3, FRAME(data)>& normal_one_form,           \
      const Scalar<DataVector>& radius,                                     \
      const tnsr::i<DataVector, 3, FRAME(data)>& r_hat);                    \
  template Scalar<DataVector>                                               \
  StrahlkorperGr::euclidean_area_element<FRAME(data)>(                      \
      const StrahlkorperTags::aliases::Jacobian<FRAME(data)>& jacobian,     \
      const tnsr::i<DataVector, 3, FRAME(data)>& normal_one_form,           \
      const Scalar<DataVector>& radius,                                     \
      const tnsr::i<DataVector, 3, FRAME(data)>& r_hat);                    \
  template double StrahlkorperGr::surface_integral_of_scalar(               \
      const Scalar<DataVector>& area_element,                               \
      const Scalar<DataVector>& scalar,                                     \
      const Strahlkorper<FRAME(data)>& strahlkorper);                       \
  template double StrahlkorperGr::euclidean_surface_integral_of_vector(     \
      const Scalar<DataVector>& area_element,                               \
      const tnsr::I<DataVector, 3, FRAME(data)>& vector,                    \
      const tnsr::i<DataVector, 3, FRAME(data)>& normal_one_form,           \
      const Strahlkorper<FRAME(data)>& strahlkorper);                       \
  template void StrahlkorperGr::spin_function<FRAME(data)>(                 \
      const gsl::not_null<Scalar<DataVector>*> result,                      \
      const StrahlkorperTags::aliases::Jacobian<FRAME(data)>& tangents,     \
      const Strahlkorper<FRAME(data)>& strahlkorper,                        \
      const tnsr::I<DataVector, 3, FRAME(data)>& unit_normal_vector,        \
      const Scalar<DataVector>& area_element,                               \
      const tnsr::ii<DataVector, 3, FRAME(data)>& extrinsic_curvature);     \
  template Scalar<DataVector> StrahlkorperGr::spin_function<FRAME(data)>(   \
      const StrahlkorperTags::aliases::Jacobian<FRAME(data)>& tangents,     \
      const Strahlkorper<FRAME(data)>& strahlkorper,                        \
      const tnsr::I<DataVector, 3, FRAME(data)>& unit_normal_vector,        \
      const Scalar<DataVector>& area_element,                               \
      const tnsr::ii<DataVector, 3, FRAME(data)>& extrinsic_curvature);     \
  template void StrahlkorperGr::dimensionful_spin_magnitude<FRAME(data)>(   \
      const gsl::not_null<double*> result,                                  \
      const Scalar<DataVector>& ricci_scalar,                               \
      const Scalar<DataVector>& spin_function,                              \
      const tnsr::ii<DataVector, 3, FRAME(data)>& spatial_metric,           \
      const StrahlkorperTags::aliases::Jacobian<FRAME(data)>& tangents,     \
      const Strahlkorper<FRAME(data)>& strahlkorper,                        \
      const Scalar<DataVector>& area_element);                              \
  template double StrahlkorperGr::dimensionful_spin_magnitude<FRAME(data)>( \
      const Scalar<DataVector>& ricci_scalar,                               \
      const Scalar<DataVector>& spin_function,                              \
      const tnsr::ii<DataVector, 3, FRAME(data)>& spatial_metric,           \
      const StrahlkorperTags::aliases::Jacobian<FRAME(data)>& tangents,     \
      const Strahlkorper<FRAME(data)>& strahlkorper,                        \
      const Scalar<DataVector>& area_element);                              \
  template void StrahlkorperGr::spin_vector<FRAME(data)>(                   \
      const gsl::not_null<std::array<double, 3>*> result,                   \
      const double spin_magnitude, const Scalar<DataVector>& area_element,  \
      const Scalar<DataVector>& radius,                                     \
      const tnsr::i<DataVector, 3, FRAME(data)>& r_hat,                     \
      const Scalar<DataVector>& ricci_scalar,                               \
      const Scalar<DataVector>& spin_function,                              \
      const Strahlkorper<FRAME(data)>& strahlkorper);                       \
  template std::array<double, 3> StrahlkorperGr::spin_vector<FRAME(data)>(  \
      const double spin_magnitude, const Scalar<DataVector>& area_element,  \
      const Scalar<DataVector>& radius,                                     \
      const tnsr::i<DataVector, 3, FRAME(data)>& r_hat,                     \
      const Scalar<DataVector>& ricci_scalar,                               \
      const Scalar<DataVector>& spin_function,                              \
      const Strahlkorper<FRAME(data)>& strahlkorper);                       \
  template void StrahlkorperGr::radial_distance<FRAME(data)>(               \
      const gsl::not_null<Scalar<DataVector>*> radial_distance,             \
      const Strahlkorper<FRAME(data)>& strahlkorper_a,                      \
      const Strahlkorper<FRAME(data)>& strahlkorper_b);
GENERATE_INSTANTIATIONS(INSTANTIATE, (Frame::Grid, Frame::Inertial))
#undef INSTANTIATE
#undef FRAME
