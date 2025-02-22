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

    UPtr<Network> Build(const Vec<Scalar> & iniT) const;

protected:
    virtual void BuildPrismElement(const Vec<Scalar> & iniT, Ptr<Network> network, Index start, Index end) const;
    virtual void ApplyBlockBCs(Ptr<Network> network) const;
    void BuildLineElement(const Vec<Scalar> & iniT, Ptr<Network> network) const;

    Arr3<Float> GetMatThermalConductivity(Index matId, Float refT) const;
    Float GetMatMassDensity(Index matId, Float refT) const;
    Float GetMatSpecificHeat(Index matId, Float refT) const;
    Float GetMatResistivity(Index matId, Float refT) const;

    /// global index
    const FCoord3D & GetPrismVertexPoint(Index index, Index iv) const;
    FCoord2D GetPrismVertexPoint2D(Index index, Index iv) const;
    FCoord2D GetPrismCenterPoint2D(Index index) const;

    /// unit: SI
    Float64 GetPrismCenterDist2Side(Index index, Index ie) const;
    Float64 GetPrismEdgeLength(Index index, Index ie) const;
    Float64 GetPrismSideArea(Index index, Index ie) const;
    Float64 GetPrismTopBotArea(Index index) const;
    Float64 GetPrismVolume(Index index) const;
    Float64 GetPrismHeight(Index index) const;

    Float64 GetLineJouleHeat(Index index, Float refT) const;
    Float64 GetLineVolume(Index index) const;
    Float64 GetLineLength(Index index) const;
    Float64 GetLineArea(Index index) const;

protected:
    CPtr<ModelType> m_model;
};

} // namespace solver::utils
} // namespace nano::heat
