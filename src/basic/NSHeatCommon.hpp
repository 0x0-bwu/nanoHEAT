#pragma once
#include <nano/common>
#include "NSHeatAlias.hpp"

#include <functional>

namespace nano::heat {

enum class Orientation { Top, Bot };

struct ThermalBoundaryCondition
{
    enum class Type { HTC/*W/(m^2*K)*/, HEAT_FLUX/*W/m^2*/, TEMPERATURE/*Kelvin*/ };
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

    bool isValid() const { return nano::isValid(value); }

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
        layerTransitionRatio = 0;
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
        (Float, minAlpha),// unit: degree
        (Float, minLen),
        (Float, maxLen),
        (Float, tolerance),
        (size_t, maxIter)
    );

    PrismMeshSettings()
    {
        NS_INIT_HANA_STRUCT(*this);
        imprintUpperLayer = true;
        minAlpha = 15;
        maxIter = 1e5;
        minLen = 1e-3;
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
        (Arr2<BC>, uniformBCs),//[top, bot]
        (Arr2<BlockBCs>, blockBCs),//[top, bot]
        (TempUnit, envTemperature)
    );
    BoundaryCondtionSettings()
    {
        NS_INIT_HANA_STRUCT(*this);
        envTemperature = TempUnit(25, TempUnit::Unit::Celsius);
    }

    void SetTopUniformBC(BC::Type type, Float value)
    {
        uniformBCs[0] = BC{type, value};
    }

    void SetBotUniformBC(BC::Type type, Float value)
    {
        uniformBCs[1] = BC{type, value};
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

struct PrismThermalSimulationSetup
{
    BOOST_HANA_DEFINE_STRUCT(PrismThermalSimulationSetup,
        (Vec<FCoord3D>, monitors),
        (TempUnit, envTemperature),
        (Index, maxIteration)
    );
    PrismThermalSimulationSetup()
    {
        NS_INIT_HANA_STRUCT(*this);
        envTemperature = TempUnit(25, TempUnit::Unit::Celsius);
        maxIteration = 10;
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

struct ThermalNetworkStaticSolverSettings
{
    BOOST_HANA_DEFINE_STRUCT(ThermalNetworkStaticSolverSettings,
        (bool, maximumRes),
        (bool, dumpHotmap),
        (bool, dumpResult),
        (Float, residual),
        (Index, maxIter),
        (Vec<Index>, probs),
        (TempUnit, envT)
    );
    ThermalNetworkStaticSolverSettings()
    {
        NS_INIT_HANA_STRUCT(*this);
        maximumRes = true;
        dumpHotmap = true;
        dumpResult = true;
        residual = 1e-1;
        maxIter = 10;
        envT = TempUnit(25, TempUnit::Unit::Celsius);
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

using ThermalTransientExcitation = std::function<Float(Float, ScenarioId)>; // ratio = f(t, scenarid), range=[0, 1]
} // namespace nano::heat