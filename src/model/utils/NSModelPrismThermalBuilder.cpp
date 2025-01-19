#include "NSModelPrismThermalBuilder.h"
#include "NSModel.h"

#include "generic/geometry/GeometryIO.hpp"
#include "generic/geometry/Mesh2D.hpp"
namespace nano::heat::model::utils {


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

    const auto & powerBlocks = stackupModel->GetAllPowerBlocks();
    HashMap<IdType, HashMap<IdType, IdType>> templateIdMap;
    auto buildOnePrismLayer = [&](IdType index) {
        auto & prismLayer = m_model->layers.at(index);
        auto & idMap = templateIdMap.emplace(prismLayer.id, HashMap<IdType, IdType>()).first->second;
        auto triangulation = m_model.GetLayerPrismTemplate(index);
        for (size_t it = 0; it < triangulation->triangles.size(); ++it) {
            NS_ASSERT(stackupModel->hasPolygon(prismLayer.id));
        }
    };

    //todo
    return true;
}


} // namespace nano::heat::model::utils