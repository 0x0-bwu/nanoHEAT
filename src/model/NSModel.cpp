#include "NSModel.h"
#include "utils/NSModelLayerStackupBuilder.h"
#include "utils/NSModelPrismThermalBuilder.h"
namespace nano::heat::model {

UPtr<LayerStackupModel> CreateLayerStackupModel(CId<package::Layout> layout, LayerStackupModelExtractionSettings settings)
{
    auto model = std::make_unique<LayerStackupModel>();
    if (utils::LayerStackupModelBuilder(*model).Build(layout, std::move(settings)))
        return model;
    return nullptr;
}

UPtr<PrismThermalModel> CreatePrismThermalModel(CId<package::Layout> layout, PrismThermalModelExtractionSettings settings)
{
    auto model = std::make_unique<PrismThermalModel>();
    if (utils::PrismThermalModelBuilder(*model).Build(layout, std::move(settings)))
        return model;
    return nullptr;
}

UPtr<PrismThermalModel> CreatePrismThermalModel(CId<package::Layout> layout, CPtr<LayerStackupModel> stackupModel, PrismMeshSettings meshSettings, BoundaryCondtionSettings bcSettings)
{
    auto model = std::make_unique<PrismThermalModel>();
    if (utils::PrismThermalModelBuilder(*model).Build(layout, stackupModel, std::move(meshSettings), std::move(bcSettings)))
        return model;
    return nullptr;
}


} // namespace nano::heat::model