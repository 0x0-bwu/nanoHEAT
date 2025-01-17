#pragma once

#include "NSModelLayerStackup.h"
namespace nano::heat::model {

class LayerStackupModel;

UPtr<LayerStackupModel> CreateLayerStackupModel(CId<package::Layout> layout, LayerStackupModelExtractionSettings settings);


} // namespace nano::heat::model