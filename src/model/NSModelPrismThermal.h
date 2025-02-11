#pragma once
#include <nano/common>
#include "basic/NSHeatCommon.hpp"
#include "generic/geometry/Triangulation.hpp"
namespace nano::heat::model {

class LayerStackupModel;
namespace utils {
class PrismThermalModelBuilder;
class PrismThermalModelQuery;
} // namespace utils

namespace pkg = nano::package;

struct LineElement
{
    BOOST_HANA_DEFINE_STRUCT(LineElement,
        (Index, id),
        (Index, netId),
        (Index, matId),
        (Index, scenId),
        (Float, radius),
        (Float, current),
        (Arr2<Index>, endPts),
        (Arr2<Vec<Index>>, neighbors)//global index
    );
    LineElement()
    {
        NS_INIT_HANA_STRUCT(*this);
        id = INVALID_INDEX;
        netId = INVALID_INDEX;
        matId = INVALID_INDEX;
        endPts = {INVALID_INDEX, INVALID_INDEX};
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

struct PrismElement
{
    inline static constexpr Index TOP_NEIGHBOR_INDEX = 3;
    inline static constexpr Index BOT_NEIGHBOR_INDEX = 4;
    BOOST_HANA_DEFINE_STRUCT(PrismElement,
        (Index, id),
        (Index, netId),
        (Index, matId),
        (Index, scenId),
        (Index, templateId),
        (Index, powerLutId),
        (Float, powerRatio),
        (Arr5<Index>, neighbors)
    );
    PrismElement()
    {
        NS_INIT_HANA_STRUCT(*this);
        id = INVALID_INDEX;
        netId = INVALID_INDEX;
        matId = INVALID_INDEX;
        scenId = INVALID_INDEX;
        templateId = INVALID_INDEX;
        powerLutId = INVALID_INDEX;
        neighbors.fill(INVALID_INDEX);
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

struct PrismLayer
{
    BOOST_HANA_DEFINE_STRUCT(PrismLayer,
        (Index, id),
        (Float, elevation),
        (Float, thickness),
        (Vec<PrismElement>, elements)
    );
protected:
    PrismLayer()
    {
        NS_INIT_HANA_STRUCT(*this);
        id = INVALID_INDEX;
    }
public:
    explicit PrismLayer(Index id) : PrismLayer()
    {
        this->id = id;
    }

    PrismElement & operator[](Index idx) { return elements[idx]; }
    const PrismElement & operator[](Index idx) const { return elements[idx]; }

    PrismElement & AddElement(Index templateId)
    {
        auto & elem = elements.emplace_back(PrismElement());
        elem.id = elements.size() - 1;
        elem.templateId = templateId;
        return elem;
    }

    size_t TotalElements() const { return elements.size(); }
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

struct ContactInstance
{
    BOOST_HANA_DEFINE_STRUCT(ContactInstance,
        (Index, id),
        (Float, ratio)
    );
    ContactInstance()
    {
        NS_INIT_HANA_STRUCT(*this);
        id = INVALID_INDEX;
    }
    ContactInstance(Index id, Float ratio) : ContactInstance()
    {
        this->id = id;
        this->ratio = ratio;
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

using ContactInstances = Vec<ContactInstance>;

struct PrismInstance
{
    BOOST_HANA_DEFINE_STRUCT(PrismInstance,
        (Index, layer),
        (Index, element),
        (Arr6<Index>, vertices), // [top, bot]
        (Arr5<Index>, neighbors), //[edge1, edge2, edge3, top, bot]
        (Arr2<ContactInstances>, contacts)//[top, bot] only used for stackup prism model
    );

    PrismInstance()
    {
        NS_INIT_HANA_STRUCT(*this);
        layer = INVALID_INDEX;
        element = INVALID_INDEX;
        vertices.fill(INVALID_INDEX);
        neighbors.fill(INVALID_INDEX);
    }

    PrismInstance(Index layer, Index element) : PrismInstance()
    {
        this->layer = layer;
        this->element = element;
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

class PrismThermalModel
{
public:
    friend class utils::PrismThermalModelQuery;
    friend class utils::PrismThermalModelBuilder;
    using Settings = PrismThermalModelExtractionSettings;
    using BC = typename BoundaryCondtionSettings::BC;
    using BlockBC = std::pair<NBox2D, BC>;
    using PrismTemplate = generic::geometry::tri::Triangulation<NCoord2D>;
    PrismThermalModel();
    virtual ~PrismThermalModel() = default;

    void Reset() { *this = PrismThermalModel(); }
    
    template <typename Scalar>
    bool WriteVTK(std::string_view filename, const Vec<Scalar> * temperature = nullptr, std::string * err = nullptr) const;

    CId<pkg::Layout> GetLayout() const { return m_.layout; }
    
    void SetLayerPrismTemplate(Index layer, SPtr<PrismTemplate> prismTemplate);
    SPtr<PrismTemplate> GetLayerPrismTemplate(Index layer) const;

    void SetUniformBC(Orientation ori, BC bc);
    CPtr<BC> GetUniformBC(Orientation ori) const;
    void AddBlockBC(Orientation ori, NBox2D box, BC bc);
    const Vec<BlockBC> & GetBlockBCs(Orientation ori) const { return m_.blockBCs.at(ori); }

    PrismLayer & AppendLayer(PrismLayer layer);
    const PrismLayer & GetLayer(Index layer) const { return m_.layers.at(layer); }
    LineElement & AddLineElement(FCoord3D start, FCoord3D end, Index netId, Index matId, Float radius, Float current, ScenarioId scenId);
    
    void BuildPrismModel(Float scaleH2Unit, Float scale2Meter);
    void AddBondingWires(CPtr<LayerStackupModel> stackupModel);
    size_t TotalLayers() const { return m_.layers.size(); }
    size_t TotalElements() const { return TotalLineElements() + TotalPrismElements(); }
    size_t TotalLineElements() const { return m_.lines.size(); }
    size_t TotalPrismElements() const { return m_.prisms.size(); }
    Index GlobalIndex(Index layer, Index element) const { return m_.indexOffset.at(layer) + element; }
    Arr2<Index> PrismLocalIndex(Index globalIndex) const;//[layer, element]
    Index LineLocalIndex(Index globalIndex) const;
    Index AddPoint(FCoord3D point);
    FCoord3D GetPoint(Index lyrId, Index elemId, Index vtxId) const;
    bool isPrism(Index index) const;

    const auto & GetPoints() const { return m_.points; }
    const auto & GetPoint(Index idx) const { return m_.points[idx]; }
    const auto & GetPrism(Index idx) const { return m_.prisms[idx]; }
    const auto & GetLineElement(Index idx) const { return m_.lines[idx]; }
    const auto & GetPrismElement(Index layer, Index element) const { return m_.layers[layer][element]; }

    Float CoordScale2Meter(int order = 1) const;
    Float UnitScale2Meter(int order = 1) const;  

    bool isTopLayer(Index layer) const { return 0 == layer; }
    bool isBotLayer(Index layer) const { return 1 + layer == TotalLayers(); }
    virtual void SearchElementIndices(const Vec<FCoord3D> & monitors, Vec<Index> & indices) const;
protected:
    NS_SERIALIZATION_FUNCTIONS_DECLARATION;

protected:
    NS_CLASS_MEMBERS_DEFINE(
        (Float, scaleH2Unit),
        (Float, scale2Meter),
        (CId<pkg::Layout>, layout),
        (PrismThermalModelExtractionSettings, settings),
        (Vec<FCoord3D>, points),
        (Vec<LineElement>, lines),
        (Vec<PrismInstance>, prisms),
        (Vec<Index>, indexOffset),
        (HashMap<Orientation, BC>, uniformBCs),
        (HashMap<Orientation, Vec<BlockBC>>, blockBCs),
        (HashMap<Index, SPtr<PrismTemplate>>, prismTemplates),
        (Vec<PrismLayer>, layers)
    );
};

inline Arr2<Index> PrismThermalModel::PrismLocalIndex(Index globalIndex) const
{
    Index lyr = 0;
    while (not (m_.indexOffset.at(lyr) <= globalIndex and globalIndex < m_.indexOffset.at(lyr + 1))) ++lyr;
    return {lyr, globalIndex - m_.indexOffset.at(lyr)};
}

inline Index PrismThermalModel::LineLocalIndex(Index globalIndex) const
{
    NS_ASSERT(globalIndex >= TotalPrismElements());
    return globalIndex - TotalPrismElements();
}



} // namespace nano::heat::model
NS_SERIALIZATION_CLASS_EXPORT_KEY(nano::heat::model::PrismThermalModel)