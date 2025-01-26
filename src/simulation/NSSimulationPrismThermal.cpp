#include "NSSimulationPrismThermal.h"
#include "solver/NSSolverPrismThermalNetwork.h"
namespace nano::heat::simulation {

PrismThermalSimulation::PrismThermalSimulation(CPtr<model::PrismThermalModel> model, CRef<PrismThermalSimulationSetup> setup)
 : m_model(model), m_setup(setup)
{
}

Arr2<Float> PrismThermalSimulation::RunStatic(Vec<Float> & temperature) const
{
    solver::PrismThermalNetworkStaticSolver solver(m_model);
    m_model->SearchElementIndices(m_setup.monitors, solver.settings.probs);
    return solver.Solve(temperature);
}

Arr2<Float> PrismThermalSimulation::RunTransient(CRef<ThermalTransientExcitation> excitation) const
{
    return {INVALID_FLOAT, INVALID_FLOAT};
}

} // namespace nano::heat::simulation