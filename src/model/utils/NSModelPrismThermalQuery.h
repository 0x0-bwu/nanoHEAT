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
    void SearchPrismInstances(const NBox2D & area, std::vector<RtVal> & results) const;
    void SearchPrismInstances(IdType layer, const NBox2D & area, std::vector<RtVal> & results) const;
    void SearchNearestPrismInstances(IdType layer, const NCoord2D & pt, size_t k, std::vector<RtVal> & results) const;
    IdType NearestLayer(Float height) const;

protected:
    CPtr<Rtree> BuildIndexTree() const;
    CPtr<Rtree> BuildLayerIndexTree(IdType layer) const;
protected:
    CRef<Model> m_model;

    mutable std::mutex m_mutex;
    mutable UPtr<Rtree> m_rtree{nullptr};
    mutable HashMap<IdType, SPtr<Rtree>> m_lyrRtrees;
};

} // namespace nano::heat::model::utils
