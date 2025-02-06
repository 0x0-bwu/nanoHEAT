#pragma once
#include <boost/geometry/index/rtree.hpp>
#include "model/NSModelPrismThermal.h"

namespace nano::heat::model::utils { 

class PrismThermalModelQuery
{
public:
    using RtVal = std::pair<NCoord2D, size_t>;
    using Rtree = boost::geometry::index::rtree<RtVal, boost::geometry::index::rstar<8>>;
    using Model = nano::heat::model::PrismThermalModel;
    explicit PrismThermalModelQuery(CRef<Model> model, bool lazyBuild = true);
    virtual ~PrismThermalModelQuery() = default;
    void SearchPrismInstances(const NBox2D & area, Vec<RtVal> & results) const;
    void SearchPrismInstances(Index layer, const NBox2D & area, Vec<RtVal> & results) const;
    void SearchNearestPrismInstances(Index layer, const NCoord2D & pt, size_t k, Vec<RtVal> & results) const;
    Index NearestLayer(Float height) const;

protected:
    CPtr<Rtree> BuildIndexTree() const;
    CPtr<Rtree> BuildLayerIndexTree(Index layer) const;
protected:
    CRef<Model> m_model;

    mutable std::mutex m_mutex;
    mutable UPtr<Rtree> m_rtree{nullptr};
    mutable HashMap<Index, SPtr<Rtree>> m_lyrRtrees;
};

} // namespace nano::heat::model::utils
