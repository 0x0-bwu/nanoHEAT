#include "NSSimulationPrismThermal.h"
#include "solver/NSSolverPrismThermalNetwork.h"
#include "model/NSModelPrismStackupThermal.h"
#include "model/NSModelPrismThermal.h"
namespace nano::heat::simulation {

PrismThermalSimulation::PrismThermalSimulation(CPtr<model::PrismThermalModel> model, CRef<PrismThermalSimulationSetup> setup)
    : m_model(model), m_setup(setup)
{
}

Arr2<Float> PrismThermalSimulation::RunStatic(Vec<Float> &temperature) const
{
    solver::PrismThermalNetworkStaticSolver solver(m_model);
    solver.settings.maxIter = m_setup.maxIteration;
    m_model->SearchElementIndices(m_setup.monitors, solver.settings.probs);
    return solver.Solve(temperature);
}

Arr2<Float> PrismThermalSimulation::RunTransient(CRef<ThermalTransientExcitation> excitation) const
{
    return {INVALID_FLOAT, INVALID_FLOAT}; //todo
}

PrismStackupThermalSimulation::PrismStackupThermalSimulation(CPtr<model::PrismStackupThermalModel> model, CRef<PrismThermalSimulationSetup> setup)
 : m_model(model), m_setup(setup)
{
}

Arr2<Float> PrismStackupThermalSimulation::RunStatic(Vec<Float> &temperature) const
{
    solver::PrismStackupThermalNetworkStaticSolver solver(m_model);
    solver.settings.maxIter = m_setup.maxIteration;
    m_model->SearchElementIndices(m_setup.monitors, solver.settings.probs);
    return solver.Solve(temperature);   
}

Arr2<Float> PrismStackupThermalSimulation::RunTransient(CRef<ThermalTransientExcitation> excitation) const
{
    return {INVALID_FLOAT, INVALID_FLOAT}; //todo
}

} // namespace nano::heat::simulation