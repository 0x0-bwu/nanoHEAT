#include "NSModelPrismThermalBuilder.h"
#include "NSModelLayerStackupQuery.h"
#include "NSModel.h"

#include "generic/geometry/GeometryIO.hpp"
#include "generic/geometry/Mesh2D.hpp"
namespace nano::heat::model::utils {

inline static constexpr auto NO_NEIGHBOR = generic::geometry::tri::noNeighbor;

using namespace generic::geometry;

bool GenerateMesh(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints, const CoordUnit & coordUnit, 
                  const PrismMeshSettings & meshSettings, typename PrismThermalModel::PrismTemplate & triangulation)
{
    auto minAlpha = generic::math::Rad(meshSettings.minAlpha);
    auto minLen = coordUnit.toCoord(meshSettings.minLen);
    auto maxLen = coordUnit.toCoord(meshSettings.maxLen);
    auto tolerance = coordUnit.toCoord(meshSettings.tolerance);
    using namespace generic::geometry;
    mesh2d::IndexEdgeList edges;
    mesh2d::Point2DContainer points;
    mesh2d::Segment2DContainer segments;
    mesh2d::ExtractIntersections(polygons, segments);
    mesh2d::ExtractTopology(segments, points, edges);
    points.reserve(points.size() + steinerPoints.size());
    points.insert(points.end(), steinerPoints.begin(), steinerPoints.end());
    mesh2d::MergeClosePointsAndRemapEdge(points, edges, tolerance);
    mesh2d::TriangulatePointsAndEdges(points, edges, triangulation);
    mesh2d::TriangulationRefinement(triangulation, minAlpha, minLen, maxLen, meshSettings.maxIter);
    if (meshSettings.dumpMeshFile) {
        auto filename = std::string(nano::CurrentDir().data()) + "/mesh.png";
        GeometryIO::WritePNG(filename, triangulation, 4096);
    }
    return true;
}

PrismThermalModelBuilder::PrismThermalModelBuilder(Ref<Model> model) : m_model(model) {}

bool PrismThermalModelBuilder::Build(CId<Layout> layout, Settings settings)
{
    if (not layout) return false;

    m_model.Reset();

    auto stackupModel = CreateLayerStackupModel(layout, settings.layerSettings);
    if (not stackupModel) return false;

    auto triangulation = std::make_shared<typename PrismThermalModel::PrismTemplate>();
    const auto & coordUnit = layout->GetCoordUnit();
    if (not GenerateMesh(stackupModel->GetAllPolygons(), stackupModel->GetSteinerPoints(), coordUnit, settings.meshSettings, *triangulation)) return false;
    NS_TRACE("total mesh elements: %1%", triangulation->triangles.size());

    for (IdType layer = 0; layer < stackupModel->TotalLayers(); ++layer) {
        m_model.SetLayerPrismTemplate(layer, triangulation);
        PrismLayer prismLayer(layer);
        [[maybe_unused]] auto check = 
        stackupModel->GetLayerHeightThickness(layer, prismLayer.elevation, prismLayer.thickness);
        NS_ASSERT(check);
        m_model.AppendLayer(std::move(prismLayer));
    }

    HashSet<IdType> fluidMats;
    auto matIter = layout->GetMaterialIter();
    while (auto mat = matIter.Next()) {
        if (MaterialType::FLUID == mat->GetMaterialType())
            fluidMats.insert(IdType(mat));
    }

    LayerStackupModelQuery query(*stackupModel);
    const auto & powerBlocks = stackupModel->GetAllPowerBlocks();
    HashMap<IdType, HashMap<IdType, IdType>> templateIdMap;
    auto buildOnePrismLayer = [&](IdType index) {
        auto & prismLayer = m_model->layers.at(index);
        auto & idMap = templateIdMap.emplace(prismLayer.id, HashMap<IdType, IdType>()).first->second;
        auto triangulation = m_model.GetLayerPrismTemplate(index);
        for (size_t it = 0; it < triangulation->triangles.size(); ++it) {
            NS_ASSERT(stackupModel->hasPolygon(prismLayer.id));
            auto ctPoint = tri::TriangulationUtility<NCoord2D>::GetCenter(*triangulation, it).Cast<NCoord>();
            auto pid = query.SearchPolygon(prismLayer.id, ctPoint);
            if (pid == INVALID_ID) continue;;
            if (fluidMats.count(stackupModel->GetMaterialId(pid))) continue;
            if (INVALID_ID == stackupModel->GetMaterialId(pid)) continue;

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

    for (IdType index = 0; index < m_model.TotalLayers(); ++index)
        buildOnePrismLayer(index);
    
    //build connection
    for (IdType index = 0; index < m_model.TotalLayers(); ++index) {
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
    auto scale2Meter = coordUnit.toUnit(coordUnit.toCoord(1), CoordUnit::Unit::Meter);
    m_model.BuildPrismModel(scaleH2Unit, scale2Meter);
    NS_TRACE("total prism elements: %1%", m_model.TotalPrismElements());

    m_model.AddBondingWires(stackupModel.get());
    NS_TRACE("total line elements: %1%", m_model.TotalLineElements());

    // bc
    const auto & bcSettings = settings.bcSettings;
    if (bcSettings.uniformBCs[0].isValid())
        m_model.SetUniformBC(Orientation::Top, bcSettings.uniformBCs[0]);
    if (bcSettings.uniformBCs[1].isValid())
        m_model.SetUniformBC(Orientation::Bot, bcSettings.uniformBCs[1]);
        
    for (const auto & block : bcSettings.blockBCs[0]) {
        if (not block.second.isValid()) continue;
        m_model.AddBlockBC(Orientation::Top, coordUnit.toCoord(block.first), block.second);
    }
    for (const auto & block : bcSettings.blockBCs[1]) {
        if (not block.second.isValid()) continue;
        m_model.AddBlockBC(Orientation::Bot, coordUnit.toCoord(block.first), block.second);
    }
        
    if (settings.meshSettings.dumpMeshFile) { 
        auto meshFile = std::string(nano::CurrentDir().data()) + "/mesh.vtk";
        // io::GenerateVTKFile<Float>(meshFile, *m_model);
    }
    return true;
}


} // namespace nano::heat::model::utils