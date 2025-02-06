#pragma once
#include <boost/geometry/index/rtree.hpp>
#include "model/NSModelLayerStackup.h"

namespace nano::heat::model::utils {

class LayerStackupModelQuery
{
public:
    using RtVal = std::pair<NBox2D, size_t>;
    using Rtree = boost::geometry::index::rtree<RtVal, boost::geometry::index::rstar<8>>;
    using Model = nano::heat::model::LayerStackupModel;
    explicit LayerStackupModelQuery(CRef<Model> model);
    virtual ~LayerStackupModelQuery() = default;

    Index SearchPolygon(Index layer, const NCoord2D & pt) const;
    
protected:
    CRef<Model> m_model;
    mutable std::unordered_map<Index, std::shared_ptr<Rtree> > m_rtrees;
};
} // namespace nano::heat::model::utils