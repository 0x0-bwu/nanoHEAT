#pragma once
#include <nano/common>
#include "basic/NSHeatCommon.hpp"

namespace nano::heat::model {

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
#ifdef NS_BOOST_SERIALIZATION_SUPPORT
    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        NS_UNUSED(version);
        NS_SERIALIZATION_HANA_STRUCT(ar, *this);
    }
#endif//NS_BOOST_SERIALIZATION_SUPPORT
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

#ifdef NS_BOOST_SERIALIZATION_SUPPORT
    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        NS_UNUSED(version);
        NS_SERIALIZATION_HANA_STRUCT(ar, *this);
    }
#endif//NS_BOOST_SERIALIZATION_SUPPORT
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
#ifdef NS_BOOST_SERIALIZATION_SUPPORT
    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        NS_UNUSED(version);
        NS_SERIALIZATION_HANA_STRUCT(ar, *this);
    }
#endif//NS_BOOST_SERIALIZATION_SUPPORT
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
#ifdef NS_BOOST_SERIALIZATION_SUPPORT
    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        NS_UNUSED(version);
        NS_SERIALIZATION_HANA_STRUCT(ar, *this);
    }
#endif//NS_BOOST_SERIALIZATION_SUPPORT
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
protected:
    PrismInstance()
    {
        NS_INIT_HANA_STRUCT(*this);
        layer = INVALID_ID;
        element = INVALID_ID;
        vertices.fill(INVALID_ID);
        neighbors.fill(INVALID_ID);
    }
public:
    PrismInstance(IdType layer, IdType element) : PrismInstance()
    {
        this->layer = layer;
        this->element = element;
    }

#ifdef NS_BOOST_SERIALIZATION_SUPPORT
    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        NS_UNUSED(version);
        NS_SERIALIZATION_HANA_STRUCT(ar, *this);
    }
#endif//NS_BOOST_SERIALIZATION_SUPPORT
};

struct PrismThermalModel
{
    BOOST_HANA_DEFINE_STRUCT(PrismThermalModel,
        (Float, scaleH2Unit),
        (Float, scale2Meter),
        (CId<pkg::Layout>, layout),
        (Vec<PrismLayer>, layers)
    );

    PrismThermalModel()
    {
        NS_INIT_HANA_STRUCT(*this);
    }
};

} // namespace nano::heat::model