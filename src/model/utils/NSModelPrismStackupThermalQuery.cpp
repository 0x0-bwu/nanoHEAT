#include "NSModelPrismStackupThermalQuery.h"
namespace nano::heat::model::utils {

using namespace generic::geometry;
PrismStackupThermalModelQuery::PrismStackupThermalModelQuery(CRef<Model> model, bool lazyBuild)
 : m_model(model)
{
    if (not lazyBuild) {
        if (auto threads = nano::thread::Threads(); threads > 1) {
            auto pool = nano::thread::Pool();
            for (size_t i = 0; i < m_model.TotalLayers(); ++i)
                pool.Submit(std::bind(&PrismStackupThermalModelQuery::BuildLayerIndexTree, this, i));
            pool.Wait();
        }
        else {
            for (size_t i = 0; i < m_model.TotalLayers(); ++i)
                BuildLayerIndexTree(i);
        }
    }
}

void PrismStackupThermalModelQuery::IntersectsPrismInstances(Index layer, Index pid, Vec<RtVal> & results) const
{
    const auto & instance = m_model.GetPrism(pid);
    const auto & triangulation = *m_model.GetLayerPrismTemplate(instance.layer);
    auto triId = m_model.GetPrismElement(instance.layer, instance.element).templateId;
    auto area = tri::TriangulationUtility<NCoord2D>::GetBondBox(triangulation, triId);
    return SearchPrismInstances(layer, area, results);
}

void PrismStackupThermalModelQuery::SearchPrismInstances(Index layer, const NBox2D & area, Vec<RtVal> & results) const
{
    results.clear();
    auto rtree = BuildLayerIndexTree(layer);
    rtree->query(boost::geometry::index::intersects(area), std::back_inserter(results));
}

void PrismStackupThermalModelQuery::SearchNearestPrismInstances(Index layer, const NCoord2D & pt, size_t k, Vec<RtVal> & results) const
{
    results.clear();
    auto rtree = BuildLayerIndexTree(layer);
    rtree->query(boost::geometry::index::nearest(pt, k), std::back_inserter(results));
}

Index PrismStackupThermalModelQuery::NearestLayer(Float height) const
{
    using namespace generic::math;
    const auto & layers = m_model->layers;
    if (GT(height, layers.front().elevation)) return 0;
    for (size_t i = 0; i < layers.size(); ++i) {
        auto top = layers.at(i).elevation;
        auto bot = top - layers.at(i).thickness;
        if (Within<LCRO>(height, bot, top)) return i;
    }
    return layers.size() - 1;
}

CPtr<PrismStackupThermalModelQuery::Rtree> PrismStackupThermalModelQuery::BuildLayerIndexTree(Index layer) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (auto iter = m_lyrRtrees.find(layer); iter != m_lyrRtrees.cend()) return iter->second.get();

    auto rtree = std::make_shared<Rtree>();
    const auto & prisms = m_model->prisms;
    const auto & indexOffset = m_model->indexOffset;
    const auto & triangulation = *m_model.GetLayerPrismTemplate(layer);
    for (size_t i = indexOffset.at(layer); i < indexOffset.at(layer + 1); ++i) {
        const auto & prism = prisms.at(i); { NS_ASSERT(layer == prism.layer) }
        const auto & element = m_model.GetPrismElement(layer, prism.element);
        auto point = tri::TriangulationUtility<NCoord2D>::GetCenter(triangulation, element.templateId).Cast<NCoord>();
        rtree->insert(std::make_pair(point, i));
    }
    return m_lyrRtrees.emplace(layer, rtree).first->second.get();
}

} // namespace nano::heat::model::utils
