#include "NSModelLayerStackupQuery.h"

#include "generic/geometry/Utility.hpp"
namespace nano::heat::model::utils {

LayerStackupModelQuery::LayerStackupModelQuery(CRef<Model> model)
 : m_model(model)
{
    const auto & layerPolygons = m_model->layerPolygons;
    for (size_t lyr = 0; lyr < m_model.TotalLayers(); ++lyr) {
        if (lyr > 0 && layerPolygons.at(lyr) == layerPolygons.at(lyr - 1))
            m_rtrees.emplace(lyr, m_rtrees.at(lyr - 1));
        else {
            auto & rtree = m_rtrees.emplace(lyr, std::make_shared<Rtree>()).first->second;
            for (auto i : *layerPolygons.at(lyr)) {
                auto bbox = generic::geometry::Extent(m_model->polygons.at(i));
                rtree->insert(std::make_pair(bbox, i));
            }
        }
    }
}

IdType LayerStackupModelQuery::SearchPolygon(IdType layer, const NCoord2D & pt) const
{
    if (not m_model.hasPolygon(layer)) return INVALID_ID;

    std::vector<RtVal> results;
    m_rtrees.at(layer)->query(boost::geometry::index::intersects(NBox2D(pt, pt)), std::back_inserter(results));
    if (results.empty()) return INVALID_ID;
    const auto & polygons = m_model->polygons;
    auto cmp = [&](auto i1, auto i2){ return polygons.at(i1).Area() > polygons.at(i2).Area(); };
    std::priority_queue<size_t, std::vector<size_t>, decltype(cmp)> pq(cmp);
    for (const auto & result : results) {
        if (generic::geometry::Contains(polygons.at(result.second), pt))
            pq.emplace(result.second);
    }
    if (not pq.empty()) return pq.top();
    return INVALID_ID;
}

} //namespace nano::heat::model::utils
