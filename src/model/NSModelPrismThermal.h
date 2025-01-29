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
        (IdType, id),
        (IdType, netId),
        (IdType, matId),
        (IdType, scenId),
        (Float, radius),
        (Float, current),
        (Arr2<IdType>, endPts),
        (Arr2<Vec<IdType>>, neighbors)//global index
    );
    LineElement()
    {
        NS_INIT_HANA_STRUCT(*this);
        id = INVALID_ID;
        netId = INVALID_ID;
        matId = INVALID_ID;
        endPts = {INVALID_ID, INVALID_ID};
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
    inline static constexpr IdType TOP_NEIGHBOR_INDEX = 3;
    inline static constexpr IdType BOT_NEIGHBOR_INDEX = 4;
    BOOST_HANA_DEFINE_STRUCT(PrismElement,
        (IdType, id),
        (IdType, netId),
        (IdType, matId),
        (IdType, scenId),
        (IdType, templateId),
        (IdType, powerLutId),
        (Float, powerRatio),
        (Arr5<IdType>, neighbors)
    );
    PrismElement()
    {
        NS_INIT_HANA_STRUCT(*this);
        id = INVALID_ID;
        netId = INVALID_ID;
        matId = INVALID_ID;
        scenId = INVALID_ID;
        templateId = INVALID_ID;
        powerLutId = INVALID_ID;
        neighbors.fill(INVALID_ID);
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
        (IdType, id),
        (Float, elevation),
        (Float, thickness),
        (Vec<PrismElement>, elements)
    );
protected:
    PrismLayer()
    {
        NS_INIT_HANA_STRUCT(*this);
        id = INVALID_ID;
    }
public:
    explicit PrismLayer(IdType id) : PrismLayer()
    {
        this->id = id;
    }

    PrismElement & operator[](IdType idx) { return elements[idx]; }
    const PrismElement & operator[](IdType idx) const { return elements[idx]; }

    PrismElement & AddElement(IdType templateId)
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
        (IdType, id),
        (Float, ratio)
    );
    ContactInstance()
    {
        NS_INIT_HANA_STRUCT(*this);
        id = INVALID_ID;
    }
    ContactInstance(IdType id, Float ratio) : ContactInstance()
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
        (IdType, layer),
        (IdType, element),
        (Arr6<IdType>, vertices), // [top, bot]
        (Arr5<IdType>, neighbors), //[edge1, edge2, edge3, top, bot]
        (Arr2<ContactInstances>, contacts)//[top, bot] only used for stackup prism model
    );

    PrismInstance()
    {
        NS_INIT_HANA_STRUCT(*this);
        layer = INVALID_ID;
        element = INVALID_ID;
        vertices.fill(INVALID_ID);
        neighbors.fill(INVALID_ID);
    }

    PrismInstance(IdType layer, IdType element) : PrismInstance()
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

    void SetLayerPrismTemplate(IdType layer, SPtr<PrismTemplate> prismTemplate);
    SPtr<PrismTemplate> GetLayerPrismTemplate(IdType layer) const;

    void SetUniformBC(Orientation ori, BC bc);
    CPtr<BC> GetUniformBC(Orientation ori) const;
    void AddBlockBC(Orientation ori, NBox2D box, BC bc);
    const Vec<BlockBC> & GetBlockBCs(Orientation ori) const { return m_.blockBCs.at(ori); }

    PrismLayer & AppendLayer(PrismLayer layer);
    const PrismLayer & GetLayer(IdType layer) const { return m_.layers.at(layer); }
    LineElement & AddLineElement(FCoord3D start, FCoord3D end, IdType netId, IdType matId, Float radius, Float current, ScenarioId scenId);
    
    void BuildPrismModel(Float scaleH2Unit, Float scale2Meter);
    void AddBondingWires(CPtr<LayerStackupModel> stackupModel);
    size_t TotalLayers() const { return m_.layers.size(); }
    size_t TotalElements() const { return TotalLineElements() + TotalPrismElements(); }
    size_t TotalLineElements() const { return m_.lines.size(); }
    size_t TotalPrismElements() const { return m_.prisms.size(); }
    IdType GlobalIndex(IdType layer, IdType element) const { return m_.indexOffset.at(layer) + element; }
    Arr2<IdType> PrismLocalIndex(IdType globalIndex) const;//[layer, element]
    IdType LineLocalIndex(IdType globalIndex) const;
    IdType AddPoint(FCoord3D point);
    FCoord3D GetPoint(IdType lyrId, IdType elemId, IdType vtxId) const;
    bool isPrism(IdType index) const;

    const auto & GetPoints() const { return m_.points; }
    const auto & GetPoint(IdType idx) const { return m_.points[idx]; }
    const auto & GetPrism(IdType idx) const { return m_.prisms[idx]; }
    const auto & GetLineElement(IdType idx) const { return m_.lines[idx]; }
    const auto & GetPrismElement(IdType layer, IdType element) const { return m_.layers[layer][element]; }

    Float CoordScale2Meter(int order = 1) const;
    Float UnitScale2Meter(int order = 1) const;  

    bool isTopLayer(IdType layer) const { return 0 == layer; }
    bool isBotLayer(IdType layer) const { return 1 + layer == TotalLayers(); }
    virtual void SearchElementIndices(const Vec<FCoord3D> & monitors, Vec<IdType> & indices) const;
private:
    NS_SERIALIZATION_FUNCTIONS_DECLARATION;
    NS_CLASS_MEMBERS_DEFINE(
        (Float, scaleH2Unit),
        (Float, scale2Meter),
        (CId<pkg::Layout>, layout),
        (PrismThermalModelExtractionSettings, settings),
        (Vec<FCoord3D>, points),
        (Vec<LineElement>, lines),
        (Vec<PrismInstance>, prisms),
        (Vec<IdType>, indexOffset),
        (HashMap<Orientation, BC>, uniformBCs),
        (HashMap<Orientation, Vec<BlockBC>>, blockBCs),
        (HashMap<IdType, SPtr<PrismTemplate>>, prismTemplates),
        (Vec<PrismLayer>, layers)
    );
};

inline Arr2<IdType> PrismThermalModel::PrismLocalIndex(IdType globalIndex) const
{
    IdType lyr = 0;
    while (not (m_.indexOffset.at(lyr) <= globalIndex and globalIndex < m_.indexOffset.at(lyr + 1))) ++lyr;
    return {lyr, globalIndex - m_.indexOffset.at(lyr)};
}

inline IdType PrismThermalModel::LineLocalIndex(IdType globalIndex) const
{
    NS_ASSERT(globalIndex >= TotalPrismElements());
    return globalIndex - TotalPrismElements();
}



} // namespace nano::heat::model
NS_SERIALIZATION_CLASS_EXPORT_KEY(nano::heat::model::PrismThermalModel)