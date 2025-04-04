#pragma once
#include "basic/NSHeatCommon.hpp"

namespace nano::heat {

namespace model { 
class PrismThermalModel;
class PrismStackupThermalModel;
} // namespace model

namespace solver {


class ThermalNetworkStaticSolver
{
public:
    using Scalar = Float32;
    ThermalNetworkStaticSolverSettings settings;

    template <typename ThermalNetworkBuilder>
    bool Solve(CPtr<typename ThermalNetworkBuilder::ModelType> model, Vec<Scalar> & results) const;
};

class PrismThermalNetworkStaticSolver
{
public:
    ThermalNetworkStaticSolverSettings settings;
    using Scalar = ThermalNetworkStaticSolver::Scalar;
    explicit PrismThermalNetworkStaticSolver(CPtr<model::PrismThermalModel> model);

    Arr2<Float> Solve(Vec<Float> & temperatures) const;

private:
    CPtr<model::PrismThermalModel> m_model;
};

class PrismStackupThermalNetworkStaticSolver
{
public:
    ThermalNetworkStaticSolverSettings settings;
    using Scalar = ThermalNetworkStaticSolver::Scalar;
    explicit PrismStackupThermalNetworkStaticSolver(CPtr<model::PrismStackupThermalModel> model);

    Arr2<Float> Solve(Vec<Float> & temperatures) const;

private:
    CPtr<model::PrismStackupThermalModel> m_model;
};

} // namespace solver

} // namespace nano::heat