#include "NSModelLayerStackupBuilder.h"
#include "NSModelLayerStackup.h"
#include <core/package>

namespace nano::heat::model::utils {

UPtr<LayerStackupModelBuilder::Model> LayerStackupModelBuilder::Build(CId<Layout> layout, Settings settings)
{
    auto model = std::make_unique<Model>();
    m_model = model.get();
    m_model->m_settings = std::move(settings);
    m_model->m_vScale2Int = std::pow(10, m_model->m_settings.layerCutPrecision);

    m_layout = layout;
    m_retriever = std::make_unique<LayoutRetriever>(m_layout);

    [[maybe_unused]] bool check{false};
    Float elevation{0}, thickness{0};
    auto layerIter = m_layout->GetStackupLayerIter();
    while (auto layer = layerIter.Next()) {
        auto dieMat = layer->GetDielectricMaterial();
        check = m_retriever->GetStackupLayerHeightThickness(layer, elevation, thickness);
        NS_ASSERT_MSG(check, "failed to get stackup height thickness");
        AddShape(INVALID_ID, IdType(dieMat), INVALID_ID, m_layout->GetBoundary(), elevation, thickness);
    }

    auto compIter = m_layout->GetComponentIter();
    while (auto comp = compIter.Next()) {
        AddComponent(comp);
    }
}


IdType LayerStackupModelBuilder::AddPolygon(IdType netId, IdType matId, NPlygon polygon, bool isHole, Float elevation, Float thickness)
{
    auto layerRange = GetLayerRange(elevation, thickness);
    if (not layerRange.isValid()) return invalidIndex;
    if (isHole == polygon.isCCW()) polygon.Reverse();
    m_model->m_ranges.emplace_back(std::move(layerRange));
    m_model->m_polygons.emplace_back(std::move(polygon));
    m_model->m_materials.emplace_back(matId);
    m_model->m_nets.emplace_back(netId);
    return m_model->m_polygons.size() - 1;
}

void LayerStackupModelBuilder::AddShape(IdType netId, EMaterialId solidMat, EMaterialId holeMat, CId<Shape> shape, EFloat elevation, EFloat thickness)
{
    if (m_model->m_settings.addCircleCenterAsSteinerPoint) {
        if (ShapeType::CIRCLE == shape->GetType()) {
            auto circle = CId<Circle>(shape);
            m_model->m_steinerPoints.emplace_back(circle->GetCenter());
        }
    }
    if (shape->hasHole()) {
        auto pwh = shape->GetContour();
        AddPolygon(netId, solidMat, std::move(pwh.outline), false, elevation, thickness);
        for (auto iter = pwh.ConstBeginHoles(); iter != pwh.ConstEndHoles(); ++iter)
            AddPolygon(netId, holeMat, std::move(*iter), true, elevation, thickness);
    }
    else AddPolygon(netId, solidMat, shape->GetContour().outline, false, elevation, thickness);
}

void LayerStackupModelBuilder::AddComponent(CId<Component> component)
{
    if (component->isBlackBox()) {
        Float elevation{0}, thickness{0};
        auto boundary = component->GetBoundary()->GetContour();
        auto material = component->GetMaterial();
        [[maybe_unused]] auto check = m_retriever->GetComponentHeightThickness(component, elevation, thickness);
        NS_ASSERT_MSG(check, "failed to get component height thickness");
    }
    else {
        NS_ASSERT_MSG(false, "todo");
    }
}


} // namespace nano::heat::model::utils