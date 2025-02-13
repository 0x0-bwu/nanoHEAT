#include "NSModelPrismStackupThermalBuilder.h"
#include "NSModelPrismMeshGenerator.h"
#include "NSModelLayerStackupQuery.h"
#include "NSModelLayerStackup.h"
#include "NSModel.h"

namespace nano::heat::model::utils {

using namespace generic::geometry;
inline static constexpr auto NO_NEIGHBOR = generic::geometry::tri::noNeighbor;

PrismStackupThermalModelBuilder::PrismStackupThermalModelBuilder(Ref<Model> model) : m_model(model) {}

bool PrismStackupThermalModelBuilder::Build(CId<Layout> layout, Settings settings)
{
    if (not layout) return false;

    m_model.Reset();
    auto stackupModel = CreateLayerStackupModel(layout, settings.layerSettings);
    if (not stackupModel) return false;

    return Build(layout, stackupModel.get(), settings.meshSettings, settings.bcSettings);
}

bool PrismStackupThermalModelBuilder::Build(CId<Layout> layout, CPtr<LayerStackupModel> stackupModel, PrismMeshSettings meshSettings, BoundaryCondtionSettings bcSettings)
{
    if (not layout or not stackupModel) return false;

    const auto & coordUnit = layout->GetCoordUnit();
    Vec<Vec<NPolygon>> layerPolygons{stackupModel->GetLayerPolygons(0)};
    Vec<SPtr<PrismTemplate>> prismTemplates{std::make_shared<PrismTemplate>()};
    HashMap<Index, Index> layer2Template{{0, 0}};
    for (Index i = 1; i < stackupModel->TotalLayers(); ++i) {
        auto indices = stackupModel->GetLayerPolygonIds(i);
        if (indices != stackupModel->GetLayerPolygonIds(i - 1)) {
            auto polygons = stackupModel->GetLayerPolygons(i);
            if (meshSettings.imprintUpperLayer) {
                const auto & upperLyr = layerPolygons.back();
                polygons.insert(polygons.end(), upperLyr.begin(), upperLyr.end());
            }
            layerPolygons.emplace_back(std::move(polygons));
            prismTemplates.emplace_back(new PrismTemplate);
        }
        layer2Template.emplace(i, prismTemplates.size() - 1);
    }
    
    Vec<NCoord2D> steinerPoints;//todo
    NS_TRACE("generate mesh for %1% layers", prismTemplates.size());
    if (nano::thread::Threads() > 1) {
        Vec<std::string> workDirs;
        auto pool = nano::thread::Pool();
        for (Index i = 0; i < prismTemplates.size(); ++i)
            workDirs.emplace_back(std::string(nano::CurrentDir()) + "/mesh" + std::to_string(i));
        for (Index i = 0; i < prismTemplates.size(); ++i)
            pool.Submit(std::bind(GenerateMesh, std::ref(layerPolygons.at(i)), std::ref(steinerPoints), std::ref(coordUnit), 
                        std::ref(meshSettings), std::ref(*prismTemplates.at(i)), workDirs.at(i).c_str()));
        pool.Wait();
    }
    else {
        for (size_t i = 0; i < prismTemplates.size(); ++i) {
            auto workDir = std::string(nano::CurrentDir()) + "/mesh" + std::to_string(i);
            GenerateMesh(layerPolygons.at(i), steinerPoints, coordUnit, meshSettings, *prismTemplates.at(i), workDir.c_str());
        }
    }

    for (size_t i = 0; i < stackupModel->TotalLayers(); ++i) {
        m_model.SetLayerPrismTemplate(i, prismTemplates.at(layer2Template.at(i)));
        PrismLayer prismLayer(i);
        [[maybe_unused]] auto check = stackupModel->GetLayerHeightThickness(i, prismLayer.elevation, prismLayer.thickness); { NS_ASSERT(check) }
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
    auto buildOnePrismLayer = [&](size_t index) {
        auto & prismLayer = m_model->layers.at(index);
        auto & idMap = templateIdMap.emplace(prismLayer.id, HashMap<Index, Index>{}).first->second;    
        auto triangulation = m_model.GetLayerPrismTemplate(index);
        for (size_t it = 0; it < triangulation->triangles.size(); ++it) {
            NS_ASSERT(stackupModel->hasPolygon(prismLayer.id))
            auto ctPoint = tri::TriangulationUtility<NCoord2D>::GetCenter(*triangulation, it).Cast<NCoord>();
            auto pid = query.SearchPolygon(prismLayer.id, ctPoint);
            if (INVALID_INDEX == pid) continue;;
            if (INVALID_INDEX == stackupModel->GetMaterialId(pid)) continue;
            if (fluidMats.count(stackupModel->GetMaterialId(pid))) continue;

            auto & ele = prismLayer.AddElement(it);
            idMap.emplace(it, ele.id);
            ele.matId = stackupModel->GetMaterialId(pid);
            ele.netId = stackupModel->GetNetId(pid);
            auto iter = powerBlocks.find(pid);
            if (iter != powerBlocks.cend() &&
                prismLayer.id == stackupModel->GetLayerIndexByHeight(iter->second.range.high)) {
                auto area = tri::TriangulationUtility<NCoord2D>::GetTriangleArea(*triangulation, it);
                ele.powerRatio = area / stackupModel->GetAllPolygons().at(pid).Area();
                ele.scenId = iter->second.scen;
                ele.powerLutId = iter->second.powerLut;
            }
        }
        NS_TRACE("layer %1%'s total elements: %2%", index, prismLayer.elements.size());
    };

    for (size_t index = 0; index < m_model.TotalLayers(); ++index)
        buildOnePrismLayer(index);

    //build connection
    for (Index index = 0; index < m_model.TotalLayers(); ++index) {
        auto & layer = m_model->layers.at(index);
        auto & elements = layer.elements;
        const auto & currIdMap = templateIdMap.at(layer.id);
        auto triangulation = m_model.GetLayerPrismTemplate(index);
        for (auto & ele : elements) {
            //layer neighbors
            const auto & triangle = triangulation->triangles.at(ele.templateId);
            for (size_t nid = 0; nid < triangle.neighbors.size(); ++nid) {
                if (NO_NEIGHBOR == triangle.neighbors.at(nid)) continue;
                auto iter = currIdMap.find(triangle.neighbors.at(nid));
                if (iter != currIdMap.cend()) ele.neighbors[nid] = iter->second;
            }
            ele.neighbors[PrismElement::TOP_NEIGHBOR_INDEX] = NO_NEIGHBOR;
            ele.neighbors[PrismElement::BOT_NEIGHBOR_INDEX] = NO_NEIGHBOR;
        }
    }

    auto scaleH2Unit = coordUnit.Scale2Unit();
    auto scale2Meter = coordUnit.toUnit(coordUnit.toCoord(1), CoordUnit::Unit::Meter);

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
