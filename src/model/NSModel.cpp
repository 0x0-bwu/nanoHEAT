#include "NSModel.h"
#include "utils/NSModelLayerStackupBuilder.h"
namespace nano::heat::model {

UPtr<LayerStackupModel> CreateLayerStackupModel(CId<package::Layout> layout, LayerStackupModelBuildSettings settings)
{
    auto model = std::make_unique<LayerStackupModel>();
    utils::LayerStackupModelBuilder builder(*model);
    if (builder.Build(layout, settings)) return model;
    return nullptr;
}

} // namespace nano::heat::model