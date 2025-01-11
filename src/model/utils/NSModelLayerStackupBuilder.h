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

    IdType AddPolygon(IdType netId, IdType matId, NPlygon polygon, bool isHole, Float elevation, Float thickness);
    void AddShape(IdType netId, EMaterialId solidMat, EMaterialId holeMat, CPtr<EShape> shape, EFloat elevation, EFloat thickness);
    // bool AddPowerBlock(EMaterialId matId, EPolygonData polygon,  EScenarioId scen, SPtr<ELookupTable1D> power, EFloat elevation, EFloat thickness, EFloat pwrPosition = 0.1, EFloat pwrThickness = 0.1);
    void AddComponent(CId<Component> component);
    // void AddBondwire(ELayerCutModel::Bondwire bw);
    // void AddImprintBox(const EBox2D & box);
    // CPtr<LayoutRetriever> GetLayoutRetriever() const; 

private:
    LayerStackupModelBuilder::Height GetHeight(EFloat height) const;
    LayerStackupModelBuilder::LayerRange GetLayerRange(EFloat elevation, EFloat thickness) const;

private:
    CId<Layout> m_layout;
    Ptr<Model> m_model;
    UPtr<LayoutRetriever> m_retriever;
};

inline LayerStackupModelBuilder::Height LayerStackupModelBuilder::GetHeight(EFloat height) const
{
    return m_model->GetHeight(height);
}

inline LayerStackupModelBuilder::LayerRange LayerStackupModelBuilder::GetLayerRange(EFloat elevation, EFloat thickness) const
{
    return m_model->GetLayerRange(elevation, thickness);
}

} // namespace utils;
} // namespace heat::model
} // namespace nano