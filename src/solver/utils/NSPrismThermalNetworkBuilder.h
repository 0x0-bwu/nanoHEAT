#pragma once
#include "basic/NSHeatCommon.hpp"
#include "solver/network/NSThermalNetwork.hpp"

namespace nano::heat {
namespace model { class PrismThermalModel; }
namespace solver::utils {

struct ThermalNetworkBuildSummary
{
    size_t totalNodes = 0;
    size_t fixedTNodes = 0;
    size_t boundaryNodes = 0;
    double iHeatFlow = 0, oHeatFlow = 0, jouleHeat = 0;
    void Reset() { *this = ThermalNetworkBuildSummary{}; }
};

template <typename Scalar>
class PrismThermalNetworkBuilder
{
public:
    mutable ThermalNetworkBuildSummary summary;
    using ModelType = model::PrismThermalModel;
    using Network = network::ThermalNetwork<Scalar>;
    explicit PrismThermalNetworkBuilder(CPtr<ModelType> model);

    UPtr<Network > Build(const Vec<Scalar> & iniT) const;

protected:
    virtual void BuildPrismElement(const Vec<Scalar> & iniT, Ptr<Network> network, IdType start, IdType end) const;
    virtual void ApplyBlockBCs(Ptr<Network> network) const;
    void BuildLineElement(const Vec<Scalar> & iniT, Ptr<Network> network) const;

    Arr3<Float> GetMatThermalConductivity(IdType matId, Float refT) const;
    Float GetMatMassDensity(IdType matId, Float refT) const;
    Float GetMatSpecificHeat(IdType matId, Float refT) const;
    Float GetMatResistivity(IdType matId, Float refT) const;

    /// global index
    const FCoord3D & GetPrismVertexPoint(IdType index, IdType iv) const;
    FCoord2D GetPrismVertexPoint2D(IdType index, IdType iv) const;
    FCoord2D GetPrismCenterPoint2D(IdType index) const;

    /// unit: SI
    Float GetPrismCenterDist2Side(IdType index, IdType ie) const;
    Float GetPrismEdgeLength(IdType index, IdType ie) const;
    Float GetPrismSideArea(IdType index, IdType ie) const;
    Float GetPrismTopBotArea(IdType index) const;
    Float GetPrismVolume(IdType index) const;
    Float GetPrismHeight(IdType index) const;

    Float GetLineJouleHeat(IdType index, Float refT) const;
    Float GetLineVolume(IdType index) const;
    Float GetLineLength(IdType index) const;
    Float GetLineArea(IdType index) const;

protected:
    CPtr<ModelType> m_model;
};

} // namespace solver::utils
} // namespace nano::heat
