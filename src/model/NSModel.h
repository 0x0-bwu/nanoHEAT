#pragma once
#include "basic/NSHeatCommon.hpp"
#include "model/NSModelLayerStackup.h"
#include "model/NSModelPrismThermal.h"
#include <nano/fwd>
namespace nano::heat::model {

UPtr<LayerStackupModel> CreateLayerStackupModel(CId<package::Layout> layout, LayerStackupModelExtractionSettings settings);
UPtr<PrismThermalModel> CreatePrismThermalModel(CId<package::Layout> layout, PrismThermalModelExtractionSettings settings);


} // namespace nano::heat::model