#include "NSModelPrismStackupThermal.h"
NS_SERIALIZATION_CLASS_EXPORT_IMP(nano::heat::model::PrismStackupThermalModel)
namespace nano::heat::model {

#ifdef NANO_BOOST_SERIALIZATION_SUPPORT
    
template <typename Archive>
void PrismStackupThermalModel::serialize(Archive & ar, const unsigned int version)
{
    NS_UNUSED(version)
    ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(PrismThermalModel);
}

NS_SERIALIZATION_FUNCTIONS_IMP(PrismStackupThermalModel)
#endif//NANO_BOOST_SERIALIZATION_SUPPORT

PrismStackupThermalModel::PrismStackupThermalModel()
 : PrismThermalModel()
{
}

void PrismStackupThermalModel::SearchElementIndices(const Vec<FCoord3D> & monitors, Vec<Index> & indices) const
{
    // indices.resize(monitors.size());
    // utils::PrismStackupThermalModelQuery query(this);
    // for (size_t i = 0; i < monitors.size(); ++i) {
    //     const auto & point = monitors.at(i);
    //     auto layer = query.NearestLayer(point[2]);
    //     Vec<typename utils::PrismStackupThermalModelQuery::RtVal> results;
    //     NCoord2D p(point[0] / m_.scaleH2Unit, point[1] / m_.scaleH2Unit);
    //     query.SearchNearestPrismInstances(layer, p, 1, results);
    //     indices[i] = results.front().second;
    // }  
}

} // namespace nano::heat::model