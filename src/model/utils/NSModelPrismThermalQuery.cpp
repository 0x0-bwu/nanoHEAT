#include "NSModelPrismThermalQuery.h"
namespace nano::heat::model::utils {

using namespace generic::geometry;
PrismThermalModelQuery::PrismThermalModelQuery(CRef<Model> model, bool lazyBuild)
 : m_model(model)
{
    if (not lazyBuild) {
        if (auto threads = nano::thread::Threads(); threads > 1) {
            auto pool = nano::thread::Pool();
            for (size_t i = 0; i < m_model.TotalLayers(); ++i)
                pool.Submit(std::bind(&PrismThermalModelQuery::BuildLayerIndexTree, this, i));
            pool.Submit(std::bind(&PrismThermalModelQuery::BuildIndexTree, this));
            pool.Wait();
        }
        else {
            for (size_t i = 0; i < m_model.TotalLayers(); ++i)
                BuildLayerIndexTree(i);
            BuildIndexTree();
        }
    }
}

void PrismThermalModelQuery::SearchPrismInstances(const NBox2D & area, Vec<RtVal> & results) const
{
    results.clear();
    auto rtree = BuildIndexTree();
    rtree->query(boost::geometry::index::covered_by(area), std::back_inserter(results));
}

void PrismThermalModelQuery::SearchPrismInstances(Index layer, const NBox2D & area, Vec<RtVal> & results) const
{
    results.clear();
    auto rtree = BuildLayerIndexTree(layer);
    rtree->query(boost::geometry::index::covered_by(area), std::back_inserter(results));
}

void PrismThermalModelQuery::SearchNearestPrismInstances(Index layer, const NCoord2D & pt, size_t k, Vec<RtVal> & results) const
{
    results.clear();
    auto rtree = BuildLayerIndexTree(layer);
    rtree->query(boost::geometry::index::nearest(pt, k), std::back_inserter(results));
}

Index PrismThermalModelQuery::NearestLayer(Float height) const
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

CPtr<PrismThermalModelQuery::Rtree> PrismThermalModelQuery::BuildIndexTree() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (nullptr == m_rtree) {
        m_rtree.reset(new Rtree);
        const auto & prisms = m_model->prisms;
        const auto & triangulation = *m_model.GetLayerPrismTemplate(0);
        for (size_t i = 0; i < prisms.size(); ++i) {
            const auto & prism = prisms.at(i);
            const auto & element = m_model.GetPrismElement(prism.layer, prism.element);
            auto point = tri::TriangulationUtility<NCoord2D>::GetCenter(triangulation, element.templateId).Cast<NCoord>();
            m_rtree->insert(std::make_pair(point, i));
        }
    }
    return m_rtree.get();
}

CPtr<PrismThermalModelQuery::Rtree> PrismThermalModelQuery::BuildLayerIndexTree(Index layer) const
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
