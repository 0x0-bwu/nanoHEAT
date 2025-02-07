#include "NSModelLayerStackupBuilder.h"
#include "NSModelLayerStackup.h"
#include <nano/core/power>

#include "generic/geometry/GeometryIO.hpp"
#include "generic/geometry/Utility.hpp"

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
        AddShape(INVALID_INDEX, Index(dieMat), INVALID_INDEX, *m_layout->GetBoundary(), elevation, thickness);
    }

    auto compIter = m_layout->GetComponentIter();
    while (auto comp = compIter.Next()) {
        AddComponent(comp);
    }

    auto connObjIter = m_layout->GetConnObjIter();
    while (auto connObj = connObjIter.Next()) {
        auto netId = Index(connObj->GetNet());
        if (auto bondingWire = connObj->GetBondingWire(); bondingWire) {
            LayerStackupModel::BondingWire bw;
            check = m_retriever->GetBondingWireSegmentsWithMinSeg(bondingWire, bw.pt2ds, bw.heights, 10);
            NS_ASSERT_MSG(check, "failed to get bonding wire segments");
            bw.radius = bondingWire->GetRadius();
            bw.matId = Index(bondingWire->GetMaterial());
            bw.netId = netId;
            //todo joul heating
            AddBondingWire(std::move(bw));

            if (auto sj = bondingWire->GetSolderJoints(); sj) {
                CId<Material> mat;
                if (sj->hasTopSolderBump()) {
                    CId<Material> mat = sj->GetTopSolderBumpMaterial();
                    auto shape = m_retriever->GetBondingWireStartSolderJointParameters(bondingWire, mat, elevation, thickness);
                    AddShape(netId, Index(mat), INVALID_INDEX, *shape, elevation, thickness);
                }
                if (sj->hasBotSolderBall()) {
                    auto shape = m_retriever->GetBondingWireEndSolderJointParameters(bondingWire, mat, elevation, thickness);
                    AddShape(netId, Index(mat), INVALID_INDEX, *shape, elevation, thickness);
                }
            }
        }
        else if (auto routingWire = connObj->GetRoutingWire(); routingWire) {
            auto layer = routingWire->GetStackupLayer();
            auto shape = routingWire->GetShape();
            if (not m_retriever->GetStackupLayerHeightThickness(layer, elevation, thickness)) {
                NS_ASSERT_MSG(false, "failed to get routing wire stackup layer height thickness");
                continue;
            }
            auto condMat = layer->GetConductingMaterial();
            auto dielMat = layer->GetDielectricMaterial();
            AddShape(netId, Index(condMat), Index(dielMat), *shape, elevation, thickness);
        }

        else if (auto padstackInst = connObj->GetPadstackInst(); padstackInst) {
            auto padstack = padstackInst->GetPadstack();
            auto viaShape = padstackInst->GetViaShape();
            auto matId = Index(padstack->GetMaterial());
            auto iter = m_layout->GetStackupLayerIter();
            while (auto layer = iter.Next()) {
                if (padstackInst->isLayerInRange(layer)) {
                    if (auto shape = padstackInst->GetPadShape(layer)) {
                        elevation = layer->GetElevation();
                        thickness = layer->GetThickness();
                        AddShape(netId, matId, INVALID_INDEX, *shape, elevation, thickness);
                    }
                    else if (viaShape)
                        AddShape(netId, matId, INVALID_INDEX, *viaShape, elevation, thickness);
                }
            }
        }
    }

    // imprint box
    const auto & coordUnit = m_layout->GetCoordUnit();
    for (const auto & box : m_model->settings.imprintBoxes)
        AddImprintBox(coordUnit.toCoord(box));

    m_model.BuildLayerPolygonLUT(m_model->settings.layerTransitionRatio);

    if (m_model->settings.dumpPNG) {
        auto filename = std::string(nano::CurrentDir()) + "/stackup.png";
        m_model.WritePNG(filename, 4096);
        for (size_t i = 0; i < m_model.TotalLayers(); ++i) {
            auto filename = std::string(nano::CurrentDir()) + "/stackup_layer" + std::to_string(i) + ".png";
            auto polygons = m_model.GetLayerPolygons(i);
            generic::geometry::GeometryIO::WritePNG(filename, polygons.begin(), polygons.end(), 4096);
        }
    }

    return true;
}

Index LayerStackupModelBuilder::AddPolygon(Index netId, Index matId, NPolygon polygon, bool isHole, Float elevation, Float thickness)
{
    auto layerRange = GetLayerRange(elevation, thickness);
    if (not layerRange.isValid()) return INVALID_INDEX;
    if (isHole == polygon.isCCW()) polygon.Reverse();
    m_model->layerRanges.emplace_back(std::move(layerRange));
    m_model->polygons.emplace_back(std::move(polygon));
    m_model->materials.emplace_back(matId);
    m_model->nets.emplace_back(netId);
    return m_model->polygons.size() - 1;
}

void LayerStackupModelBuilder::AddShape(Index netId, Index solidMat, Index holeMat, CRef<Shape> shape, Float elevation, Float thickness)
{
    if (m_model->settings.addCircleCenterAsSteinerPoint) {
        if (ShapeType::CIRCLE == shape.GetType()) {
            const auto & circle = static_cast<const ShapeCircle &>(shape);
            m_model->steinerPoints.emplace_back(circle.GetCenter());
        }
    }
    if (shape.hasHole()) {
        auto pwh = shape.GetContour();
        AddPolygon(netId, solidMat, std::move(pwh.outline), false, elevation, thickness);
        for (auto iter = pwh.ConstBeginHoles(); iter != pwh.ConstEndHoles(); ++iter)
            AddPolygon(netId, holeMat, std::move(*iter), true, elevation, thickness);
    }
    else AddPolygon(netId, solidMat, shape.GetContour().outline, false, elevation, thickness);
}

bool LayerStackupModelBuilder::AddPowerBlock(Index matId, NPolygon polygon, ScenarioId scen, Index powerLut, Float elevation, Float thickness, Float pwrPosition, Float pwrThickness)
{
    auto index = AddPolygon(INVALID_INDEX, matId, std::move(polygon), false, elevation, thickness);
    if (INVALID_INDEX == index) return false;
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
            check = AddPowerBlock(Index(material), boundary, scen, lut, elevation, thickness);
            NS_ASSERT_MSG(check, "failed to add power block")
        }
        else AddPolygon(INVALID_INDEX, Index(material), boundary, false, elevation, thickness);
        
        check = m_retriever->GetComponentLayerHeightThickness(component->GetAssemblyLayer(), elevation, thickness);
        NS_ASSERT_MSG(check, "failed to get component layer height thickness");
        if (thickness > 0) {
            auto solderMat = component->GetAssemblyLayer()->GetSolderFillingMaterial();
            AddPolygon(INVALID_INDEX, Index(solderMat), boundary, false, elevation, thickness);
            //todo, solder ball/bump
        }
    }
    else {
        NS_ASSERT_MSG(false, "todo");
    }
}

void LayerStackupModelBuilder::AddBondingWire(Model::BondingWire bw)
{
    m_model->bondingWires.emplace_back(std::move(bw));
}

void LayerStackupModelBuilder::AddImprintBox(const NBox2D & box)
{
    m_model->layerRanges.emplace_back(Model::LayerRange());
    m_model->polygons.emplace_back(generic::geometry::toPolygon(box));
    m_model->materials.emplace_back(INVALID_INDEX);
    m_model->nets.emplace_back(INVALID_INDEX);
}


} // namespace nano::heat::model::utils