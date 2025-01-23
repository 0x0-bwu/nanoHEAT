#pragma once
#include "model/NSModelPrismThermal.h"
#include <nano/core/package>

namespace nano::heat::model {

class LayerStackupModel;
namespace utils {

using namespace package;
class PrismThermalModelBuilder
{
public:
    using Model = nano::heat::model::PrismThermalModel;
    using Settings = typename Model::Settings;
    explicit PrismThermalModelBuilder(Ref<Model> model);
    bool Build(CId<Layout> layout, Settings settings);
    bool Build(CId<Layout> layout, CPtr<LayerStackupModel> stackupModel, PrismMeshSettings meshSettings, BoundaryCondtionSettings bcSettings);
private:
    Ref<Model> m_model;
};

} // namespace utils
} // namespace nano::heat::model
