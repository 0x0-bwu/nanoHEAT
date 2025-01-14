#pragma once
#include <nano/common>
#include <list>

#include "basic/NSHeatCommon.hpp"
namespace nano::heat::model {

namespace utils { class LayerStackupModelBuilder; }

class LayerStackupModel
{
public:
    // friend class utils::LayerStackupModelQuery;
    friend class utils::LayerStackupModelBuilder;
    using Height = int;
    using PolygonIds = std::vector<IdType>;
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
        IdType polygon{std::numeric_limits<IdType>::max()};
        LayerRange range;
        ScenarioId scen;
        IdType powerLut;
        PowerBlock(IdType polygon, LayerRange range, ScenarioId scenario, IdType powerLut)
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
        IdType netId{INVALID_ID};
        IdType matId{INVALID_ID};
        ScenarioId scenario;
        std::vector<Float> heights;
        std::vector<NCoord2D> pt2ds;
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
    void Reset() { *this = LayerStackupModel(); }
    bool WritePNG(std::string_view filename, size_t witth = 1024) const;
    void BuildLayerPolygonLUT(Float transitionRatio);
    size_t TotalLayers() const;
    bool hasPolygon(IdType layer) const;
    bool GetLayerHeightThickness(IdType layer, Float & elevation, Float & thickness) const;
    IdType GetLayerIndexByHeight(Height height) const;

    const NPolygon & GetLayoutBoundary() const;
    const auto & GetAllPowerBlocks() const;
    const auto & GetAllPolygons() const;
    const auto & GetSteinerPoints() const;
    const auto & GetAllBondingWires() const;
    SPtr<PolygonIds> GetLayerPolygonIds(IdType layer) const;
    IdType GetMaterialId(IdType polygon) const;
    IdType GetNetId(IdType polygon) const;

    Height GetHeight(Float height) const;
    LayerRange GetLayerRange(Float elevation, Float thickness) const;
    std::vector<NPolygon> GetLayerPolygons(IdType layer) const;

    static bool SliceOverheightLayers(std::list<LayerRange> & ranges, Float ratio);

private:
    NS_SERIALIZATION_FUNCTIONS_DECLARATION;
    NS_CLASS_MEMBERS_DEFINE(
    (std::vector<IdType>, nets),
    (std::vector<IdType>, materials),
    (std::vector<NPolygon>, polygons),
    (std::vector<NCoord2D>, steinerPoints),
    (std::vector<LayerRange>, layerRanges),
    (std::vector<BondingWire>, bondingWires),
    (HashMap<IdType, PowerBlock>, powerBlocks),
    (LayerStackupModelBuildSettings, settings),
    (HashMap<IdType, SPtr<PolygonIds>>, layerPolygons),
    (HashMap<Height, IdType>, height2indices),
    (std::vector<Height>, layerOrder),
    (Float, vScale2Int))
};

inline IdType LayerStackupModel::GetNetId(IdType pid) const
{
    return m_.nets.at(pid);
}

inline IdType LayerStackupModel::GetMaterialId(IdType pid) const
{
    return m_.materials.at(pid);
}

inline LayerStackupModel::Height LayerStackupModel::GetHeight(Float height) const
{
    return std::round(height * m_.vScale2Int);
}

} // namespace nano::heat::model
NS_SERIALIZATION_CLASS_EXPORT_KEY(nano::heat::model::LayerStackupModel)
