#pragma once
#include "basic/NSHeatCommon.hpp"
#include "model/NSModelPrismThermal.h"


namespace nano::heat {

namespace model { class PrismThermalModel; }

namespace simulation {

class PrismThermalSimulation
{
public:
    PrismThermalSimulation(CPtr<model::PrismThermalModel> model, CRef<PrismThermalSimulationSetup> setup);

    Arr2<Float> RunStatic(Vec<Float> & temperature) const;
    Arr2<Float> RunTransient(CRef<ThermalTransientExcitation> excitation) const;
private:
    CPtr<model::PrismThermalModel> m_model;
    CRef<PrismThermalSimulationSetup> m_setup;
};

} // namespace simulation
} // namespace nano::heat