#pragma once
#include <boost/geometry/index/rtree.hpp>
#include "model/NSModelPrismStackupThermal.h"

namespace nano::heat::model::utils {

class PrismStackupThermalModelQuery
{
public:
    using RtVal = std::pair<NCoord2D, size_t>;
    using Rtree = boost::geometry::index::rtree<RtVal, boost::geometry::index::rstar<8>>;
    using Model = nano::heat::model::PrismStackupThermalModel;
    explicit PrismStackupThermalModelQuery(CRef<Model> model, bool lazyBuild = false);
    virtual ~PrismStackupThermalModelQuery() = default;

    NTriangle2D GetPrismInstanceTemplate(size_t pid) const;

    void IntersectsPrismInstances(Index layer, Index pid, Vec<RtVal> & results) const;
    void SearchPrismInstances(Index layer, const NBox2D & area, Vec<RtVal> & results) const;
    void SearchNearestPrismInstances(Index layer, const NCoord2D & pt, size_t k, Vec<RtVal> & results) const;
    Index NearestLayer(Float height) const;

protected:
    CPtr<Rtree> BuildLayerIndexTree(Index layer) const;
protected:
    CRef<Model> m_model;

    mutable std::mutex m_mutex;
    mutable HashMap<Index, SPtr<Rtree>> m_lyrRtrees;
};

} // namespace nano::heat::model::utils