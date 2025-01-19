#pragma once
#include <nano/common>
#include "NSHeatAlias.hpp"
namespace nano::heat {

enum class Orientation { Top, Bot };


struct ThermalBoundaryCondition
{
    enum class Type { HTC, HEAT_FLUX, /*TEMPERATURE*/ };
    BOOST_HANA_DEFINE_STRUCT(ThermalBoundaryCondition,
        (Type, type),
        (Float, value)
    );
    ThermalBoundaryCondition()
    {
        NS_INIT_HANA_STRUCT(*this);
        type = Type::HTC;
        value = INVALID_FLOAT;
    }
    ThermalBoundaryCondition(Type type, Float value) : ThermalBoundaryCondition()
    {
        this->type = type;
        this->value = value;
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

struct LayerStackupModelExtractionSettings
{
    BOOST_HANA_DEFINE_STRUCT(LayerStackupModelExtractionSettings,
        (bool, dumpPNG),
        (bool, addCircleCenterAsSteinerPoint),
        (size_t, layerCutPrecision),
        (Float, layerTransitionRatio),
        (Vec<FBox2D>, imprintBoxes)
    );
    LayerStackupModelExtractionSettings()
    {
        NS_INIT_HANA_STRUCT(*this);
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

struct PrismMeshSettings
{
    BOOST_HANA_DEFINE_STRUCT(PrismMeshSettings,
        (bool, dumpMeshFile),
        (bool, genMeshByLayer),
        (bool, imprintUpperLayer),
        (Float, minAlpha),
        (Float, minLen),
        (Float, maxLen),
        (Float, tolerance),
        (size_t, maxIter)
    );

    PrismMeshSettings()
    {
        NS_INIT_HANA_STRUCT(*this);
        minAlpha = 20;//degree
        maxLen = std::numeric_limits<Float>::max();
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

struct BoundaryCondtionSettings
{
    using BC = ThermalBoundaryCondition;
    using BlockBC = std::pair<FBox2D, ThermalBoundaryCondition>;
    using BlockBCs = Vec<BlockBC>;
    BOOST_HANA_DEFINE_STRUCT(BoundaryCondtionSettings,
        (BC, topUniformBC),
        (BC, botUniformBC),
        (Arr2<BlockBCs>, blockBCs),//[top, bot]
        (TempUnit, envTemperature)
    );
    BoundaryCondtionSettings()
    {
        NS_INIT_HANA_STRUCT(*this);
        envTemperature = TempUnit(25, TempUnit::Unit::Celsius);
    }

    void AddBlockBC(Orientation ori, FBox2D box, BC::Type type, Float value)
    {
        auto i = Orientation::Top == ori ? 0 : 1;
        blockBCs[i].emplace_back(box, BC{type, value});
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
struct PrismThermalModelExtractionSettings
{

    BOOST_HANA_DEFINE_STRUCT(PrismThermalModelExtractionSettings,
        (PrismMeshSettings, meshSettings),
        (BoundaryCondtionSettings, bcSettings),
        (LayerStackupModelExtractionSettings, layerSettings)
    );

    PrismThermalModelExtractionSettings()
    {
        NS_INIT_HANA_STRUCT(*this);
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