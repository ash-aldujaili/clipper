/**
 * @file py_clipper.cpp
 * @brief Python bindings for CLIPPER
 * @author Parker Lusk <plusk@mit.edu>
 * @date 28 January 2021
 */

#include <cstdint>
#include <sstream>

#include <Eigen/Dense>

#include <pybind11/pybind11.h>
#include <pybind11/eigen.h>
#include <pybind11/stl.h>

#include "clipper/clipper.h"
#include "clipper/utils.h"
#include "clipper/invariants/builtins.h"

#include "trampolines.h"

namespace py = pybind11;
using namespace pybind11::literals;

void pybind_invariants(py::module& m)
{
  m.doc() = "Invariants are quantities that do not change under the"
            "transformation between two sets of objects. They are used to"
            "build a consistency graph. Some built-in invariants are provided.";

  using namespace clipper::invariants;

  //
  // Base Invariants
  //

  py::class_<Invariant, PyInvariant<>, std::shared_ptr<Invariant>>(m, "Invariant")
    .def(py::init<>());
  py::class_<PairwiseInvariant, Invariant, PyPairwiseInvariant<>, std::shared_ptr<PairwiseInvariant>>(m, "PairwiseInvariant")
    .def(py::init<>())
    .def("__call__", &clipper::invariants::PairwiseInvariant::operator());

  //
  // Euclidean Distance
  //

  py::class_<EuclideanDistance::Params>(m, "EuclideanDistanceParams")
    .def(py::init<>())
    .def("__repr__", [](const EuclideanDistance::Params &params) {
       std::ostringstream repr;
       repr << "<EuclideanDistanceParams : sigma=" << params.sigma;
       repr << " epsilon=" << params.epsilon;
       repr << " mindist=" << params.mindist << ">";
       return repr.str();
    })
    .def_readwrite("sigma", &clipper::invariants::EuclideanDistance::Params::sigma)
    .def_readwrite("epsilon", &clipper::invariants::EuclideanDistance::Params::epsilon)
    .def_readwrite("mindist", &clipper::invariants::EuclideanDistance::Params::mindist);

  py::class_<EuclideanDistance, PairwiseInvariant, PyPairwiseInvariant<EuclideanDistance>, std::shared_ptr<EuclideanDistance>>(m, "EuclideanDistance")
    .def(py::init<const EuclideanDistance::Params&>());

  //
  // Point-Normal Distance
  //

  py::class_<PointNormalDistance::Params>(m, "PointNormalDistanceParams")
    .def(py::init<>())
    .def("__repr__", [](const PointNormalDistance::Params &params) {
       std::ostringstream repr;
       repr << "<PointNormalDistanceParams : sigp=" << params.sigp;
       repr << " epsp=" << params.epsp << " sign=" << params.sign;
       repr << " epsn=" << params.epsn << ">";
       return repr.str();
    })
    .def_readwrite("sigp", &clipper::invariants::PointNormalDistance::Params::sigp)
    .def_readwrite("epsp", &clipper::invariants::PointNormalDistance::Params::epsp)
    .def_readwrite("sign", &clipper::invariants::PointNormalDistance::Params::sign)
    .def_readwrite("epsn", &clipper::invariants::PointNormalDistance::Params::epsn);

  py::class_<PointNormalDistance, PairwiseInvariant, PyPairwiseInvariant<PointNormalDistance>, std::shared_ptr<PointNormalDistance>>(m, "PointNormalDistance")
    .def(py::init<const PointNormalDistance::Params&>());
}

PYBIND11_MODULE(clipper, m)
{
  m.doc() = "CLIPPER is a graph-theoretic framework for robust data association";
  m.attr("__version__") = PROJECT_VERSION;

  py::module m_invariants = m.def_submodule("invariants");
  pybind_invariants(m_invariants);

  py::class_<clipper::Params>(m, "Params")
    .def(py::init<>())
    .def("__repr__", [](const clipper::Params &params) {
       std::ostringstream repr;
       repr << "<Parameters for CLIPPER dense cluster solver>";
       return repr.str();
    })
    .def_readwrite("tol_u", &clipper::Params::tol_u)
    .def_readwrite("tol_F", &clipper::Params::tol_F)
    .def_readwrite("tol_Fop", &clipper::Params::tol_Fop)
    .def_readwrite("maxiniters", &clipper::Params::maxiniters)
    .def_readwrite("maxoliters", &clipper::Params::maxoliters)
    .def_readwrite("beta", &clipper::Params::beta)
    .def_readwrite("maxlsiters", &clipper::Params::maxlsiters)
    .def_readwrite("eps", &clipper::Params::eps);

  py::class_<clipper::Solution>(m, "Solution")
    .def(py::init<>())
    .def("__repr__", [](const clipper::Solution &soln) {
       std::ostringstream repr;
       repr << "<CLIPPER dense cluster solution>";
       return repr.str();
    })
    .def_readwrite("t", &clipper::Solution::t)
    .def_readwrite("ifinal", &clipper::Solution::ifinal)
    .def_readwrite("nodes", &clipper::Solution::nodes)
    .def_readwrite("u", &clipper::Solution::u)
    .def_readwrite("score", &clipper::Solution::score);

  m.def("create_all_to_all", clipper::createAllToAll,
    "n1"_a, "n2"_a,
    "Create an all-to-all hypothesis for association. Useful for the case of"
    " no prior information or putative associations.");

  m.def("score_pairwise_consistency", [](const clipper::invariants::PairwiseInvariantPtr& invariant,
                        const clipper::invariants::Data& D1, const clipper::invariants::Data& D2,
                        clipper::Association& A) {

    // indicates if calls to invariant can be made in parallel. This is primarily to
    // prevent GIL-related resource deadlocking for derived classes in Python. See also
    // https://github.com/pybind/pybind11/issues/813
    // Python extended c++ classes will inherit from PyPairwiseInvariant
    bool parallelize = (std::dynamic_pointer_cast<PyPairwiseInvariant<>>(invariant)) ? false : true;
    auto ret = clipper::scorePairwiseConsistency(invariant, D1, D2, A, parallelize);
    return ret;

  }, py::call_guard<py::gil_scoped_release>(),
    "invariant"_a, "D1"_a.noconvert(), "D2"_a.noconvert(), "A"_a,
    "Scores consistency between pairs of associations in A");

  m.def(
      "score_sparse_pairwise_consistency",
      [](const clipper::invariants::PairwiseInvariantPtr &invariant,
         const clipper::invariants::Data &D1,
         const clipper::invariants::Data &D2, clipper::Association &A) {
        // indicates if calls to invariant can be made in parallel. This is
        // primarily to prevent GIL-related resource deadlocking for derived
        // classes in Python. See also
        // https://github.com/pybind/pybind11/issues/813
        // Python extended c++ classes will inherit from PyPairwiseInvariant
        bool parallelize =
            (std::dynamic_pointer_cast<PyPairwiseInvariant<>>(invariant))
                ? false
                : true;
        // hacky way
        auto ret = clipper::scoreSparsePairwiseConsistency(*invariant, D1, D2,
                                                           A, parallelize);
        return ret;
      },
      py::call_guard<py::gil_scoped_release>(), "invariant"_a,
      "D1"_a.noconvert(), "D2"_a.noconvert(), "A"_a,
      "Scores sparse consistency between pairs of associations in A");



  m.def("find_dense_cluster",
    py::overload_cast<const Eigen::MatrixXd&, const Eigen::MatrixXd&,
          const clipper::Params&>(clipper::findDenseCluster<Eigen::MatrixXd>),
    "M"_a.noconvert(), "C"_a.noconvert(), "params"_a);

  m.def("find_dense_cluster",
    py::overload_cast<const Eigen::MatrixXd&, const Eigen::MatrixXd&, const Eigen::VectorXd&,
          const clipper::Params&>(clipper::findDenseCluster<Eigen::MatrixXd>),
    "M"_a.noconvert(), "C"_a.noconvert(), "u0"_a, "params"_a);


    m.def("find_dense_cluster_of_sparse_graph",
        py::overload_cast<const Eigen::SparseMatrix<double> &,
                          const Eigen::SparseMatrix<double> &,
                          const clipper::Params &>(
            clipper::findDenseClusterOfSparseGraph),
        "M"_a.noconvert(), "C"_a.noconvert(), "params"_a);

  m.def("select_inlier_associations", &clipper::selectInlierAssociations,
    "soln"_a, "A"_a);
}