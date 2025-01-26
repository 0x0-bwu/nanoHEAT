#pragma once
#include "NSThermalNetwork.hpp"
#include "generic/tools/Tools.hpp"
#include "generic/circuit/MNA.hpp"
#include "generic/circuit/MOR.hpp"

#include <boost/numeric/odeint.hpp>

#include <Eigen/IterativeLinearSolvers>
#include <Eigen/Sparse>

namespace nano::heat::solver::network {

using namespace generic;
using namespace generic::ckt;

template <typename Scalar>
class ThermalNetworkStaticSolver
{
public:
    generic::math::la::DenseVector<Scalar> x;
    void Solve(CRef<ThermalNetwork<Scalar>> network, Scalar refT, Vec<Scalar> & result)
    {
        x.resize(network.MatrixSize());
        result.resize(network.NodeSize());
        auto m = makeMNA(network);
        auto rhs = makeRhs(network, refT);
        for (size_t i = 0; i < network.NodeSize(); ++i) {
            auto & node = network[i];
            if (ThermalNetwork<Scalar>::UNKNOWN_T == node.t) {
                continue;
                // x[network.MatrixId(i)] = result[i];
            }
            else result[i] = node.t;
        }
        Eigen::ConjugateGradient<SparseMatrix<Scalar>, Eigen::Lower|Eigen::Upper> solver(m.G);
        // x = m.L * solver.solveWithGauss(rhs, x);
        x = m.L * solver.solve(rhs);
        NS_TRACE("Eigen CG iterations: %1%", solver.iterations());
        NS_TRACE("Eigen CG error: %1%", solver.error());
        for (size_t i = 0; i < x.size(); ++i) {
            auto nid = network.NodeId(i);
            result[nid] = x[i];
        }
    }
};








} // namespace nano::heat::solver::network