#pragma once
#include "NSModelPrismThermal.h"

namespace nano::heat::model::traits {

template <typename Model>
struct ThermalModelTraits
{
    static size_t Size(const Model & model) { NS_ASSERT(false); return 0; }
    static bool NeedIteration(const Model & model) { NS_ASSERT(false); return false; }
};


template <>
struct ThermalModelTraits<PrismThermalModel>
{
    static size_t Size(const PrismThermalModel & model) { return model.TotalElements(); }
    static bool NeedIteration(const PrismThermalModel & model) { return true;/*todo*/ }
};

template <>
struct ThermalModelTraits<PrismStackupThermalModel>
{
    static size_t Size(const PrismStackupThermalModel & model) { return model.TotalElements(); }
    static bool NeedIteration(const PrismStackupThermalModel & model) { return true;/*todo*/ }
};

} // namespace nano::heat::model::traits