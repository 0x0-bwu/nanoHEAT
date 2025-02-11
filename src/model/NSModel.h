#pragma once
#include "model/NSModelPrismStackupThermal.h"
#include "model/NSModelLayerStackup.h"
#include "model/NSModelPrismThermal.h"
#include <nano/fwd>
namespace nano::heat::model {

UPtr<LayerStackupModel> CreateLayerStackupModel(CId<package::Layout> layout, LayerStackupModelExtractionSettings settings);
UPtr<PrismThermalModel> CreatePrismThermalModel(CId<package::Layout> layout, PrismThermalModelExtractionSettings settings);
UPtr<PrismThermalModel> CreatePrismThermalModel(CId<package::Layout> layout, CPtr<LayerStackupModel> stackupModel, PrismMeshSettings meshSettings, BoundaryCondtionSettings bcSettings);
UPtr<PrismStackupThermalModel> CreatePrismStackupThermalModel(CId<package::Layout> layout, PrismThermalModelExtractionSettings settings);
UPtr<PrismStackupThermalModel> CreatePrismStackupThermalModel(CId<package::Layout> layout, CPtr<LayerStackupModel> stackupModel, PrismMeshSettings meshSettings, BoundaryCondtionSettings bcSettings);


} // namespace nano::heat::model