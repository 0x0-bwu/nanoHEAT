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

Index LayerStackupModelQuery::SearchPolygon(Index layer, const NCoord2D & pt) const
{
    if (not m_model.hasPolygon(layer)) return INVALID_INDEX;

    Vec<RtVal> results;
    m_rtrees.at(layer)->query(boost::geometry::index::intersects(NBox2D(pt, pt)), std::back_inserter(results));
    if (results.empty()) return INVALID_INDEX;
    const auto & polygons = m_model->polygons;
    auto cmp = [&](auto i1, auto i2){ 
        if (polygons.at(i1).Area() > polygons.at(i2).Area()) return true;
        if (auto mat = m_model.GetMaterialId(i1); INVALID_INDEX == mat) return true;
        return false;
    };
    std::priority_queue<size_t, Vec<size_t>, decltype(cmp)> pq(cmp);
    for (const auto & result : results) {
        if (generic::geometry::Contains(polygons.at(result.second), pt))
            pq.emplace(result.second);
    }
    if (not pq.empty()) return pq.top();
    return INVALID_INDEX;
}

} //namespace nano::heat::model::utils
