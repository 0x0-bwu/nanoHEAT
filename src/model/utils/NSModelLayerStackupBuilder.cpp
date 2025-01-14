#include "NSModelLayerStackupBuilder.h"
#include "NSModelLayerStackup.h"
#include <nano/core/package>
#include <nano/core/power>

namespace nano::heat::model::utils {

LayerStackupModelBuilder::LayerStackupModelBuilder(Ref<Model> model) : m_model(model) {}

bool LayerStackupModelBuilder::Build(CId<Layout> layout, Settings settings)
{
    if (not layout) return false;

    m_model.Reset();
    m_model->settings = std::move(settings);
    m_model->vScale2Int = std::pow(10, m_model->settings.layerCutPrecision);

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

    const auto & coordUnit = m_layout->GetCoordUnit();
    auto scale2Unit = coordUnit.Scale2Unit();
    auto scale2Meter = coordUnit.toUnit<FCoord>(coordUnit.toCoord(1), CoordUnit::Unit::Meter);

    HashMap<CId<StackupLayer>, IdArr2<Material>> layerMaterialMap;
    HashMap<CId<StackupLayer>, Arr2<Float>> layerElevationThicknessMap;
    auto connObjIter = m_layout->GetConnObjIter();
    while (auto connObj = connObjIter.Next()) {
        if (auto bondingWire = connObj->GetBondingWire(); bondingWire) {
            LayerStackupModel::BondingWire bw;
            check = m_retriever->GetBondingWireSegmentsWithMinSeg(bondingWire, bw.pt2ds, bw.heights, 10);
            NS_ASSERT_MSG(check, "failed to get bonding wire segments");
            

        }
        
    }



    return true;
}

IdType LayerStackupModelBuilder::AddPolygon(IdType netId, IdType matId, NPolygon polygon, bool isHole, Float elevation, Float thickness)
{
    auto layerRange = GetLayerRange(elevation, thickness);
    if (not layerRange.isValid()) return INVALID_ID;
    if (isHole == polygon.isCCW()) polygon.Reverse();
    m_model->layerRanges.emplace_back(std::move(layerRange));
    m_model->polygons.emplace_back(std::move(polygon));
    m_model->materials.emplace_back(matId);
    m_model->nets.emplace_back(netId);
    return m_model->polygons.size() - 1;
}

void LayerStackupModelBuilder::AddShape(IdType netId, IdType solidMat, IdType holeMat, CId<Shape> shape, Float elevation, Float thickness)
{
    if (m_model->settings.addCircleCenterAsSteinerPoint) {
        if (ShapeType::CIRCLE == shape->GetType()) {
            auto circle = CId<ShapeCircle>(shape);
            m_model->steinerPoints.emplace_back(circle->GetCenter());
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

bool LayerStackupModelBuilder::AddPowerBlock(IdType matId, NPolygon polygon, ScenarioId scen, IdType powerLut, Float elevation, Float thickness, Float pwrPosition, Float pwrThickness)
{
    auto index = AddPolygon(INVALID_ID, matId, std::move(polygon), false, elevation, thickness);
    if (INVALID_ID == index) return false;
    Float pe = elevation - thickness * pwrPosition;
    Float pt = std::min(thickness * pwrThickness, thickness - elevation + pe);
    m_model->powerBlocks.emplace(index, LayerStackupModel::PowerBlock(index, GetLayerRange(pe, pt), scen, powerLut));
    return true;
}

void LayerStackupModelBuilder::AddComponent(CId<Component> component)
{
    [[maybe_unused]] bool check{false};
    if (component->isBlackBox()) {
        Float elevation{0}, thickness{0};
        auto material = component->GetMaterial();
        auto boundary = component->GetBoundary()->GetOutline();
        check = m_retriever->GetComponentHeightThickness(component, elevation, thickness);
        NS_ASSERT_MSG(check, "failed to get component height thickness");

        if (auto lossPower = component->GetBind<LossPower>(); lossPower) {
            auto scen = lossPower->GetScenario();
            auto lut = lossPower->GetLookupTable();
            check = AddPowerBlock(IdType(material), boundary, scen, lut, elevation, thickness);
            NS_ASSERT_MSG(check, "failed to add power block")
        }
        else AddPolygon(INVALID_ID, IdType(material), boundary, false, elevation, thickness);
        
        check = m_retriever->GetComponentLayerHeightThickness(component->GetAssemblyLayer(), elevation, thickness);
        NS_ASSERT_MSG(check, "failed to get component layer height thickness");
        if (thickness > 0) {
            auto solderMat = component->GetAssemblyLayer()->GetSolderFillingMaterial();
            AddPolygon(INVALID_ID, IdType(solderMat), boundary, false, elevation, thickness);
            //todo, solder ball/bump
        }
    }
    else {
        NS_ASSERT_MSG(false, "todo");
    }
}


} // namespace nano::heat::model::utils