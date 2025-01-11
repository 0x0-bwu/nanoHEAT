#pragma once
#include <core/common>
#include "NSHeatAlias.hpp"
namespace nano::heat {

struct LayerStackupModelBuildSettings
{
    bool dumpSketchImg = true;
    bool addCircleCenterAsSteinerPoint{false};
    size_t layerCutPrecision = 6;
    Float layerTransitionRatio = 2;
    std::vector<FBox2D> imprintBoxes;
#ifdef NANO_BOOST_SERIALIZATION_SUPPORT
    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        NS_UNUSED(version);
        ar & boost::serialization::make_nvp("dump_sketch_img"                   , dumpSketchImg                );
        ar & boost::serialization::make_nvp("add_circle_center_as_steiner_point", addCircleCenterAsSteinerPoint);
        ar & boost::serialization::make_nvp("layer_cut_precision"               , layerCutPrecision            );
        ar & boost::serialization::make_nvp("layer_transition_ratio"            , layerTransitionRatio         );
        ar & boost::serialization::make_nvp("imprint_boxes"                     , imprintBoxes                 );
    }
#endif//NANO_BOOST_SERIALIZATION_SUPPORT
};

} // namespace nano::heat