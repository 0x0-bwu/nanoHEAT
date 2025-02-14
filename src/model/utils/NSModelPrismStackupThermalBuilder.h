#pragma once
#include "model/NSModelPrismStackupThermal.h"
#include "NSModelPrismStackupThermalQuery.h"
#include <nano/core/package>

namespace nano::heat::model {

class LayerStackupModel;
namespace utils {

using namespace package;

class PrismStackupThermalModelBuilder
{
public:
    using Model = nano::heat::model::PrismStackupThermalModel;
    using Settings = typename Model::Settings;
    explicit PrismStackupThermalModelBuilder(Ref<Model> model);
    bool Build(CId<Layout> layout, Settings settings);
    bool Build(CId<Layout> layout, CPtr<LayerStackupModel> stackupModel, PrismMeshSettings meshSettings, BoundaryCondtionSettings bcSettings);

private:
    void BuildPrismModel(Float scaleH2Unit, Float scale2Meter);
    void BuildPrismInstanceTopBotNeighbors(Index start, Index end);
    void AddBondingWires(CPtr<LayerStackupModel> stackupModel);
private:
    Ref<Model> m_model;
    UPtr<PrismStackupThermalModelQuery> m_query;
};

} // namespace utils
} // namespace nano::heat::model
