#pragma once
#include <nano/common>
#include "NSHeatAlias.hpp"
namespace nano::heat {

struct LayerStackupModelExtractionSettings
{
    BOOST_HANA_DEFINE_STRUCT(LayerStackupModelExtractionSettings,
        (bool, dumpPNG),
        (bool, addCircleCenterAsSteinerPoint),
        (size_t, layerCutPrecision),
        (Float, layerTransitionRatio),
        (std::vector<FBox2D>, imprintBoxes)
    );
    LayerStackupModelExtractionSettings()
    {
        dumpPNG = true;
        addCircleCenterAsSteinerPoint = false;
        layerCutPrecision = 6;
        layerTransitionRatio = 2;
    }
#ifdef NANO_BOOST_SERIALIZATION_SUPPORT
    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        NS_UNUSED(version);
        NS_SERIALIZATION_HANA_STRUCT(ar, *this);
    }
#endif//NANO_BOOST_SERIALIZATION_SUPPORT
};

} // namespace nano::heat