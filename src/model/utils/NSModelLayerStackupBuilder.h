#pragma once
#include "model/NSModelLayerStackup.h"

namespace nano {

namespace package::utils { class LayoutRetriever; }

namespace heat::model {

class LayerStackupModel;

namespace utils {

using namespace package;
class LayerStackupModelBuilder
{
public:
    using Model = LayerStackupModel;
    using Settings = LayerStackupModelBuildSettings;
    using LayoutRetriever = package::utils::LayoutRetriever;
    UPtr<Model> Build(CId<Layout> layout, Settings settings);

private:

    IdType AddPolygon(IdType netId, IdType matId, NPolygon polygon, bool isHole, Float elevation, Float thickness);
    void AddShape(IdType netId, IdType solidMat, IdType holeMat, CId<Shape> shape, Float elevation, Float thickness);
    bool AddPowerBlock(IdType matId, NPolygon polygon, ScenarioId scen, IdType powerLut, Float elevation, Float thickness, Float pwrPosition = 0.1, Float pwrThickness = 0.1);
    void AddComponent(CId<Component> component);

private:
    LayerStackupModel::Height GetHeight(Float height) const;
    LayerStackupModel::LayerRange GetLayerRange(Float elevation, Float thickness) const;

private:
    CId<Layout> m_layout;
    Ptr<Model> m_model;
    UPtr<LayoutRetriever> m_retriever;
};

inline LayerStackupModel::Height LayerStackupModelBuilder::GetHeight(Float height) const
{
    return m_model->GetHeight(height);
}

inline LayerStackupModel::LayerRange LayerStackupModelBuilder::GetLayerRange(Float elevation, Float thickness) const
{
    return m_model->GetLayerRange(elevation, thickness);
}

} // namespace utils;
} // namespace heat::model
} // namespace nano