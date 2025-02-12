#include "NSSolverPrismThermalNetwork.h"
#include "utils/NSPrismStackupThermalNetworkBuilder.h"
#include "utils/NSPrismThermalNetworkBuilder.h"
#include "network/NSThermalNetworkSolver.hpp"
#include "model/NSModelPrismStackupThermal.h"
#include "model/NSModelPrismThermal.h"
#include "model/NSModelTraits.hpp"
namespace nano::heat::solver {

Pair<Set<Index>, Vec<Index>> GetProbsAndPermutation(const Vec<Index> & indices)
{
    Index index{0};
    Set<Index> probs(indices.begin(), indices.end());
    HashMap<Index, Index> indexMap;
    for (auto prob : probs) indexMap.emplace(prob, index++);
    Vec<Index> permutation; permutation.reserve(indices.size());
    for (auto index : indices) permutation.emplace_back(indexMap.at(index));
    return std::make_pair(probs, permutation);
}

template <typename Scalar>
Scalar CalculateResidual(const Vec<Scalar> & v1, const Vec<Scalar> & v2, bool maximumRes)
{
    NS_ASSERT(v1.size() == v2.size())
    Scalar residual = 0;
    size_t size = v1.size();
    for(size_t i = 0; i < size; ++i) {
        if (maximumRes) residual = std::max<Scalar>(std::fabs(v1[i] - v2[i]), residual);
        else residual += std::fabs(v1[i] - v2[i]) / size;
    }
    return residual;
}

template <typename ThermalNetworkBuilder>
bool ThermalNetworkStaticSolver::Solve(CPtr<typename ThermalNetworkBuilder::ModelType> model, Vec<Scalar> & results) const
{
    NS_ASSERT(model);
    auto envT = settings.envT.inKelvins();
    using Model = typename ThermalNetworkBuilder::ModelType;
    results.assign(model::traits::ThermalModelTraits<Model>::Size(*model), envT);
    
    Scalar residual = 0;
    size_t iteration = 0;
    Vec<Scalar> prevRes(results);
    ThermalNetworkBuilder builder(model);
    network::ThermalNetworkStaticSolver<Scalar> solver;
    size_t maxIter = model::traits::ThermalModelTraits<Model>::NeedIteration(*model) ? settings.maxIter : 1;
    do {
        auto network = builder.Build(prevRes);
        NS_ASSERT(network);
        NS_TRACE("total size: %1%", network->MatrixSize());
        NS_TRACE("total joule heat: %1%w", builder.summary.jouleHeat);
        NS_TRACE("intake heat flow: %1%w", builder.summary.iHeatFlow);
        NS_TRACE("outtake heat flow: %1%w", builder.summary.oHeatFlow);
        solver.Solve(*network, envT, results);
        std::swap(prevRes, results);
        residual = CalculateResidual(prevRes, results, settings.maximumRes);

        NS_TRACE("P-T iteration: %1%, Residual: %2%", ++iteration, residual);
        NS_TRACE("max T: %1%C", TempUnit::Kelvins2Celsius(*std::max_element(results.cbegin(), results.cend())));
    } while (residual > settings.residual && --maxIter > 0);

    if (settings.envT.GetUnit() == TempUnit::Unit::Celsius)
        std::for_each(results.begin(), results.end(), [](auto & t) { t = TempUnit::Kelvins2Celsius(t); });
    
    if (settings.dumpResult) {
        auto filename = std::string(nano::CurrentDir()) + "/static.csv";
        std::ofstream out(filename);
        if (out.is_open()) {
            for (auto i : settings.probs)
                out << TempUnit::Kelvins2Celsius(results.at(i)) << ',';
            out << NS_EOL;
            out.close();
        }
    }
    return true;
}

template bool ThermalNetworkStaticSolver::Solve<utils::PrismThermalNetworkBuilder<ThermalNetworkStaticSolver::Scalar>>(CPtr<model::PrismThermalModel> model, Vec<ThermalNetworkStaticSolver::Scalar> & results) const;
template bool ThermalNetworkStaticSolver::Solve<utils::PrismStackupThermalNetworkBuilder<ThermalNetworkStaticSolver::Scalar>>(CPtr<model::PrismStackupThermalModel> model, Vec<ThermalNetworkStaticSolver::Scalar> & results) const;

PrismThermalNetworkStaticSolver::PrismThermalNetworkStaticSolver(CPtr<model::PrismThermalModel> model)
 : m_model(model)
{
}

Arr2<Float> PrismThermalNetworkStaticSolver::Solve(Vec<Float> & temperatures) const
{
    Vec<Scalar> results;
    auto res = ThermalNetworkStaticSolver().Solve<utils::PrismThermalNetworkBuilder<Scalar>>(m_model, results);
    if (not res) return {INVALID_FLOAT, INVALID_FLOAT};

    auto minT = * std::min_element(results.cbegin(), results.cend());
    auto maxT = * std::max_element(results.cbegin(), results.cend());
    temperatures.resize(settings.probs.size());
    for (size_t i = 0; i < settings.probs.size(); ++i)
        temperatures[i] = results.at(settings.probs.at(i));
    
    if (settings.dumpHotmap) {
        auto hotmapFile = std::string(nano::CurrentDir()) + "/hotmap.vtk";
        m_model->WriteVTK<Scalar>(hotmapFile, &results);
    }
    return {minT, maxT};
}

PrismStackupThermalNetworkStaticSolver::PrismStackupThermalNetworkStaticSolver(CPtr<model::PrismStackupThermalModel> model)
 : m_model(model)
{
}

Arr2<Float> PrismStackupThermalNetworkStaticSolver::Solve(Vec<Float> & temperatures) const
{
    Vec<Scalar> results;
    auto res = ThermalNetworkStaticSolver().Solve<utils::PrismStackupThermalNetworkBuilder<Scalar>>(m_model, results);
    if (not res) return {INVALID_FLOAT, INVALID_FLOAT};

    auto minT = * std::min_element(results.cbegin(), results.cend());
    auto maxT = * std::max_element(results.cbegin(), results.cend());
    temperatures.resize(settings.probs.size());
    for (size_t i = 0; i < settings.probs.size(); ++i)
        temperatures[i] = results.at(settings.probs.at(i));
    
    if (settings.dumpHotmap) {
        auto hotmapFile = std::string(nano::CurrentDir()) + "/hotmap.vtk";
        m_model->WriteVTK<Scalar>(hotmapFile, &results);
    }
    return {minT, maxT};
}

} // namespace nano::heat::solver