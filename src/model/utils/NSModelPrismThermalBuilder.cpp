#include "NSModelPrismThermalBuilder.h"
#include "NSModelPrismMeshGenerator.h"
#include "NSModelLayerStackupQuery.h"
#include "NSModel.h"

namespace nano::heat::model::utils {

using namespace generic::geometry;
inline static constexpr auto NO_NEIGHBOR = generic::geometry::tri::noNeighbor;

PrismThermalModelBuilder::PrismThermalModelBuilder(Ref<Model> model) : m_model(model) {}

bool PrismThermalModelBuilder::Build(CId<Layout> layout, Settings settings)
{
    if (not layout) return false;

    m_model.Reset();
    auto stackupModel = CreateLayerStackupModel(layout, settings.layerSettings);
    if (not stackupModel) return false;

    return Build(layout, stackupModel.get(), settings.meshSettings, settings.bcSettings);
}

bool PrismThermalModelBuilder::Build(CId<Layout> layout, CPtr<LayerStackupModel> stackupModel, PrismMeshSettings meshSettings, BoundaryCondtionSettings bcSettings)
{
    if (not layout or not stackupModel) return false;
    
    auto triangulation = std::make_shared<typename PrismThermalModel::PrismTemplate>();
    const auto & coordUnit = layout->GetCoordUnit();
    if (not GenerateMesh(stackupModel->GetAllPolygons(), stackupModel->GetSteinerPoints(), coordUnit, meshSettings, *triangulation)) return false;
    NS_TRACE("total mesh elements: %1%", triangulation->triangles.size());

    for (Index layer = 0; layer < stackupModel->TotalLayers(); ++layer) {
        m_model.SetLayerPrismTemplate(layer, triangulation);
        PrismLayer prismLayer(layer);
        [[maybe_unused]] auto check = 
        stackupModel->GetLayerHeightThickness(layer, prismLayer.elevation, prismLayer.thickness);
        NS_ASSERT(check);
        m_model.AppendLayer(std::move(prismLayer));
    }

    HashSet<Index> fluidMats;
    auto matIter = layout->GetMaterialIter();
    while (auto mat = matIter.Next()) {
        if (MaterialType::FLUID == mat->GetMaterialType())
            fluidMats.insert(Index(mat));
    }

    LayerStackupModelQuery query(*stackupModel);
    const auto & powerBlocks = stackupModel->GetAllPowerBlocks();
    HashMap<Index, HashMap<Index, Index>> templateIdMap;
    auto buildOnePrismLayer = [&](Index index) {
        auto & prismLayer = m_model->layers.at(index);
        auto & idMap = templateIdMap.emplace(prismLayer.id, HashMap<Index, Index>()).first->second;
        auto triangulation = m_model.GetLayerPrismTemplate(index);
        for (size_t it = 0; it < triangulation->triangles.size(); ++it) {
            NS_ASSERT(stackupModel->hasPolygon(prismLayer.id));
            auto ctPoint = tri::TriangulationUtility<NCoord2D>::GetCenter(*triangulation, it).Cast<NCoord>();
            auto pid = query.SearchPolygon(prismLayer.id, ctPoint);
            if (INVALID_INDEX == pid) continue;;
            if (INVALID_INDEX == stackupModel->GetMaterialId(pid)) continue;
            if (fluidMats.count(stackupModel->GetMaterialId(pid))) continue;

            auto & elem = prismLayer.AddElement(it);
            idMap.emplace(it, elem.id);
            elem.matId = stackupModel->GetMaterialId(pid);
            elem.netId = stackupModel->GetNetId(pid);
            auto iter = powerBlocks.find(pid);
            if (iter != powerBlocks.cend() &&
                prismLayer.id == stackupModel->GetLayerIndexByHeight(iter->second.range.high)) {
                auto area = tri::TriangulationUtility<NCoord2D>::GetTriangleArea(*triangulation, it);
                elem.powerRatio = area / stackupModel->GetAllPolygons().at(pid).Area();
                elem.scenId = iter->second.scen;
                elem.powerLutId = iter->second.powerLut;
            }
        }
        NS_TRACE("layer %1%'s total elements: %2%", index, prismLayer.elements.size());
    };

    for (Index index = 0; index < m_model.TotalLayers(); ++index)
        buildOnePrismLayer(index);
    
    //build connection
    for (Index index = 0; index < m_model.TotalLayers(); ++index) {
        auto & layer = m_model->layers.at(index);
        auto & elements = layer.elements;
        const auto & currIdMap = templateIdMap.at(layer.id);
        for (auto & ele : elements) {
            //layer neighbors
            const auto & triangle = triangulation->triangles.at(ele.templateId);
            for (size_t nid = 0; nid < triangle.neighbors.size(); ++nid) {
                if (NO_NEIGHBOR == triangle.neighbors.at(nid)) continue;
                auto iter = currIdMap.find(triangle.neighbors.at(nid));
                if (iter != currIdMap.cend()) ele.neighbors[nid] = iter->second;
            }
        }
        if (not m_model.isBotLayer(index)) {
            auto & lowerLayer = m_model->layers.at(index + 1);
            auto & lowerEles = lowerLayer.elements;
            const auto & lowerIdMap = templateIdMap.at(lowerLayer.id);
            for (auto & ele : elements) {
                auto iter = lowerIdMap.find(ele.templateId);
                if (iter != lowerIdMap.cend()) {
                    auto & lowerEle = lowerEles.at(iter->second);
                    lowerEle.neighbors[PrismElement::TOP_NEIGHBOR_INDEX] = ele.id;
                    ele.neighbors[PrismElement::BOT_NEIGHBOR_INDEX] = lowerEle.id;
                }
            }
        }
    }
    auto scaleH2Unit = coordUnit.Scale2Unit();
    auto scale2Meter = coordUnit.toUnit(coordUnit.toCoord(1), CoordUnit::Unit::METER);
    m_model.BuildPrismModel(scaleH2Unit, scale2Meter);
    m_model.AddBondingWires(stackupModel);
    NS_TRACE("total elements: %1%, prism: %2%, line: %3%", 
    m_model.TotalElements(), m_model.TotalPrismElements(), m_model.TotalLineElements());

    // bc
    if (bcSettings.uniformBCs[0].isValid())
        m_model.SetUniformBC(Orientation::TOP, bcSettings.uniformBCs[0]);
    if (bcSettings.uniformBCs[1].isValid())
        m_model.SetUniformBC(Orientation::BOT, bcSettings.uniformBCs[1]);
        
    for (const auto & block : bcSettings.blockBCs[0]) {
        if (not block.second.isValid()) continue;
        m_model.AddBlockBC(Orientation::TOP, coordUnit.toCoord(block.first), block.second);
    }
    for (const auto & block : bcSettings.blockBCs[1]) {
        if (not block.second.isValid()) continue;
        m_model.AddBlockBC(Orientation::BOT, coordUnit.toCoord(block.first), block.second);
    }
        
    if (meshSettings.dumpMeshFile) { 
        auto meshFile = std::string(nano::CurrentDir()) + "/mesh.vtk";
        m_model.WriteVTK<Float>(meshFile);
    }
    m_model->layout = layout;
    return true;
}


} // namespace nano::heat::model::utils