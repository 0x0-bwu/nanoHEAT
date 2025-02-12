#pragma once
#include "basic/NSHeatCommon.hpp"

namespace nano::heat {

namespace model { 
class PrismThermalModel;
class PrismStackupThermalModel;
} // namespace model

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

class PrismStackupThermalSimulation
{
public:
    PrismStackupThermalSimulation(CPtr<model::PrismStackupThermalModel> model, CRef<PrismThermalSimulationSetup> setup);

    Arr2<Float> RunStatic(Vec<Float> & temperature) const;
    Arr2<Float> RunTransient(CRef<ThermalTransientExcitation> excitation) const;
private:
    CPtr<model::PrismStackupThermalModel> m_model;
    CRef<PrismThermalSimulationSetup> m_setup;
};

} // namespace simulation
} // namespace nano::heat