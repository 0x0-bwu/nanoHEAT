#pragma once
#include <nano/common>
#include <list>

#include "basic/NSHeatCommon.hpp"
namespace nano::heat::model {

namespace utils {
class LayerStackupModelBuilder;
class LayerStackupModelQuery;
} // namespace utils

class LayerStackupModel
{
public:
    friend class utils::LayerStackupModelQuery;
    friend class utils::LayerStackupModelBuilder;
    using Height = int;
    using PolygonIds = Vec<Index>;
    using Settings = nano::heat::LayerStackupModelExtractionSettings;
    struct LayerRange
    {
#ifdef NANO_BOOST_SERIALIZATION_SUPPORT
        friend class boost::serialization::access;
        template <typename Archive>
        void serialize(Archive & ar, const unsigned int version)
        {
            NS_UNUSED(version);
            ar & boost::serialization::make_nvp("high", high);
            ar & boost::serialization::make_nvp("low" , low );
        }
#endif//NANO_BOOST_SERIALIZATION_SUPPORT
        Height high = -std::numeric_limits<Height>::max();
        Height low  =  std::numeric_limits<Height>::max();
        LayerRange() = default;
        LayerRange(Height high, Height low) : high(high), low(low) {}
        bool isValid() const { return high > low; }
        Height Thickness() const { return high - low; }
    };

    struct PowerBlock
    {
        Index polygon{std::numeric_limits<Index>::max()};
        LayerRange range;
        ScenarioId scen;
        Index powerLut;
        PowerBlock(Index polygon, LayerRange range, ScenarioId scenario, Index powerLut)
         : polygon(polygon), range(range), scen(scenario), powerLut(powerLut) {}
        PowerBlock() = default;
#ifdef NANO_BOOST_SERIALIZATION_SUPPORT
    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        NS_UNUSED(version);
        ar & boost::serialization::make_nvp("polygon"  , polygon );
        ar & boost::serialization::make_nvp("range"    , range   );
        ar & boost::serialization::make_nvp("scenario" , scen    );
        ar & boost::serialization::make_nvp("power_lut", powerLut);
    }
#endif//NANO_BOOST_SERIALIZATION_SUPPORT
    };
    
    struct BondingWire
    {
        Float radius{0};
        Float current{0};
        Index netId{INVALID_INDEX};
        Index matId{INVALID_INDEX};
        ScenarioId scenario;
        Vec<Float> heights;
        Vec<NCoord2D> pt2ds;
#ifdef NANO_BOOST_SERIALIZATION_SUPPORT
        friend class boost::serialization::access;
        template <typename Archive>
        void serialize(Archive & ar, const unsigned int version)
        {
            NS_UNUSED(version);
            ar & boost::serialization::make_nvp("current" , current );
            ar & boost::serialization::make_nvp("radius"  , radius  );
            ar & boost::serialization::make_nvp("net"     , netId   );
            ar & boost::serialization::make_nvp("mat"     , matId   );
            ar & boost::serialization::make_nvp("scenario", scenario);
            ar & boost::serialization::make_nvp("heights" , heights );
            ar & boost::serialization::make_nvp("points"  , pt2ds   );
        }
#endif//NANO_BOOST_SERIALIZATION_SUPPORT
    };
    LayerStackupModel();
    virtual ~LayerStackupModel() = default;
    void Reset() { *this = LayerStackupModel(); }
#ifdef NANO_BOOST_SERIALIZATION_SUPPORT
    bool Save(std::string_view filename, ArchiveFormat fmt) const;
    bool Load(std::string_view filename, ArchiveFormat fmt);
#endif//NANO_BOOST_SERIALIZATION_SUPPORT
    bool WritePNG(std::string_view filename, size_t witth = 1024) const;
    void BuildLayerPolygonLUT(Float transitionRatio);
    size_t TotalLayers() const;
    bool hasPolygon(Index layer) const;
    bool GetLayerHeightThickness(Index layer, Float & elevation, Float & thickness) const;
    Index GetLayerIndexByHeight(Height height) const;

    const NPolygon & GetLayoutBoundary() const;
    const auto & GetAllPowerBlocks() const { return m_.powerBlocks; }
    const auto & GetAllPolygons() const { return m_.polygons; }
    const auto & GetSteinerPoints() const { return m_.steinerPoints; }
    const auto & GetAllBondingWires() const { return m_.bondingWires; }
    SPtr<PolygonIds> GetLayerPolygonIds(Index layer) const;
    Index GetMaterialId(Index polygon) const;
    Index GetNetId(Index polygon) const;

    Height GetHeight(Float height) const;
    LayerRange GetLayerRange(Float elevation, Float thickness) const;
    Vec<NPolygon> GetLayerPolygons(Index layer) const;
    static bool SliceOverheightLayers(std::list<LayerRange> & ranges, Float ratio);

private:
    NS_SERIALIZATION_FUNCTIONS_DECLARATION;
    NS_CLASS_MEMBERS_DEFINE(
    (Vec<Index>, nets),
    (Vec<Index>, materials),
    (Vec<NPolygon>, polygons),
    (Vec<NCoord2D>, steinerPoints),
    (Vec<LayerRange>, layerRanges),
    (Vec<BondingWire>, bondingWires),
    (HashMap<Index, PowerBlock>, powerBlocks),
    (HashMap<Index, SPtr<PolygonIds>>, layerPolygons),
    (HashMap<Height, Index>, height2indices),
    (Vec<Height>, layerOrder),
    (Float, vScale2Int),
    (Settings, settings))
};

inline Index LayerStackupModel::GetNetId(Index pid) const
{
    return m_.nets.at(pid);
}

inline Index LayerStackupModel::GetMaterialId(Index pid) const
{
    return m_.materials.at(pid);
}

inline LayerStackupModel::Height LayerStackupModel::GetHeight(Float height) const
{
    return std::round(height * m_.vScale2Int);
}

} // namespace nano::heat::model
NS_SERIALIZATION_CLASS_EXPORT_KEY(nano::heat::model::LayerStackupModel)
