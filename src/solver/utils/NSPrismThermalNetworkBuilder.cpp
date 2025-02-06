#include "NSPrismThermalNetworkBuilder.h"
#include "model/utils/NSModelPrismThermalQuery.h"
#include <nano/core/common>
#include <nano/core/basic>

namespace nano::heat::solver::utils {

template <typename Scalar>
PrismThermalNetworkBuilder<Scalar>::PrismThermalNetworkBuilder(CPtr<ModelType> model) : m_model(model) { NS_ASSERT(m_model); }

template <typename Scalar>
UPtr<typename PrismThermalNetworkBuilder<Scalar>::Network> PrismThermalNetworkBuilder<Scalar>::Build(const Vec<Scalar> & iniT) const
{
    const size_t size = m_model->TotalElements();
    NS_ASSERT(size == iniT.size());

    summary.Reset();
    summary.totalNodes = size;
    auto network = std::make_unique<Network>(size);

    if (auto threads = nano::Threads(); threads > 1) {
        auto & pool = nano::Pool();
        size_t size = m_model->TotalPrismElements();
        size_t blocks = threads * 2;
        size_t blockSize = size / blocks;

        size_t begin = 0;
        for(size_t i = 0; i < blocks && blockSize > 0; ++i){
            size_t end = begin + blockSize;
            pool.Submit(std::bind(&PrismThermalNetworkBuilder::BuildPrismElement, this, std::ref(iniT), network.get(), begin, end));
            begin = end;
        }
        size_t end = size;
        if(begin != end)
            pool.Submit(std::bind(&PrismThermalNetworkBuilder::BuildPrismElement, this, std::ref(iniT), network.get(), begin, end));        
    }
    else BuildPrismElement(iniT, network.get(), 0, m_model->TotalPrismElements());
    
    BuildLineElement(iniT, network.get());
    ApplyBlockBCs(network.get());
    network->BuildIndexMap();
    return network;
}

template <typename Scalar>
void PrismThermalNetworkBuilder<Scalar>::BuildPrismElement(const Vec<Scalar> & iniT, Ptr<Network> network, size_t start, size_t end) const
{
    auto topBC = m_model->GetUniformBC(Orientation::Top);
    auto botBC = m_model->GetUniformBC(Orientation::Bot);
    
    for (size_t i = start; i < end; ++i) {
        const auto & inst = m_model->GetPrism(i);
        const auto & element = m_model->GetPrismElement(inst.layer, inst.element);
        if (auto lut = CId<LookupTable>(element.powerLutId); lut) {
            auto p = lut->Lookup(iniT.at(i));
            p *= element.powerRatio;
            summary.iHeatFlow += p;
            network->AddHF(i, p);
            network->SetScenario(i, element.scenId);
        }

        auto c = GetMatSpecificHeat(element.matId, iniT.at(i));
        auto rho = GetMatMassDensity(element.matId, iniT.at(i));
        auto vol = GetPrismVolume(i);
        network->SetC(i, c * rho * vol);

        auto k = GetMatThermalConductivity(element.matId, iniT.at(i));
        auto ct = GetPrismCenterPoint2D(i);

        const auto & neighbors = inst.neighbors;
        //edges
        for (size_t ie = 0; ie < 3; ++ie) {
            auto vArea = GetPrismSideArea(i, ie);
            if (auto nid = neighbors.at(ie); INVALID_INDEX == nid) {
                //todo, side bc
            }
            else if (i < nid) { //one way
                const auto & nb = m_model->GetPrism(nid);
                const auto & nbEle = m_model->GetPrismElement(nb.layer, nb.element);
                auto ctNb = GetPrismCenterPoint2D(nid);
                auto vec = ctNb - ct;
                auto dist = vec.Norm2() * m_model->UnitScale2Meter();
                auto kxy = 0.5 * (k[0] + k[1]);
                auto dist2edge = GetPrismCenterDist2Side(i, ie);
                auto r1 = dist2edge / kxy / vArea;

                auto kNb = GetMatThermalConductivity(nbEle.matId, iniT.at(nid));
                auto kNbXY = 0.5 * (kNb[0] + kNb[1]);
                auto r2 = (dist - dist2edge) / kNbXY / vArea;
                network->SetR(i, nid, r1 + r2);
            }
        }
        auto height = GetPrismHeight(i);
        auto hArea = GetPrismTopBotArea(i);
        //top
        auto nTop = neighbors.at(model::PrismElement::TOP_NEIGHBOR_INDEX);
        if (INVALID_INDEX == nTop) {
            if (nullptr != topBC && topBC->isValid()) {
                if (ThermalBoundaryCondition::Type::HTC == topBC->type) {
                    network->SetHTC(i, topBC->value * hArea);
                    summary.boundaryNodes += 1;
                }
                else if (ThermalBoundaryCondition::Type::HEAT_FLUX == topBC->type) {
                    auto heatFlow = topBC->value * hArea;
                    network->SetHF(i, heatFlow);
                    if (heatFlow > 0)
                        summary.iHeatFlow += heatFlow;
                    else summary.oHeatFlow += heatFlow;
                }
                else if (ThermalBoundaryCondition::Type::TEMPERATURE == topBC->type) {
                    network->SetT(i, TempUnit::Celsius2Kelvins(topBC->value));
                    summary.fixedTNodes += 1;
                }
            }
        }
        else if (i < nTop) {
            const auto & nb = m_model->GetPrism(nTop);
            const auto & nbEle = m_model->GetPrismElement(nb.layer, nb.element);
            auto hNb = GetPrismHeight(nTop);
            auto kNb = GetMatThermalConductivity(nbEle.matId, iniT.at(nTop));
            auto r = (0.5 * height / k[2] + 0.5 * hNb / kNb[2]) / hArea;
            network->SetR(i, nTop, r);
        }
        //bot
        auto nBot = neighbors.at(model::PrismElement::BOT_NEIGHBOR_INDEX);
        if (INVALID_INDEX == nBot) {
            if (nullptr != botBC && botBC->isValid()) {
                if (ThermalBoundaryCondition::Type::HTC == botBC->type) {
                    network->SetHTC(i, botBC->value * hArea);
                    summary.boundaryNodes += 1;
                }
                else if (ThermalBoundaryCondition::Type::HEAT_FLUX == botBC->type) {
                    auto heatFlow = botBC->value * hArea;
                    network->SetHF(i, heatFlow);
                    if (heatFlow > 0)
                        summary.iHeatFlow += heatFlow;
                    else summary.oHeatFlow += heatFlow;
                }
                else if (ThermalBoundaryCondition::Type::TEMPERATURE == botBC->type) {
                    network->SetT(i, TempUnit::Celsius2Kelvins(botBC->value));
                    summary.fixedTNodes += 1;
                }
            }
        }
        else if (i < nBot) {
            const auto & nb = m_model->GetPrism(nBot);
            const auto & nbEle = m_model->GetPrismElement(nb.layer, nb.element);
            auto hNb = GetPrismHeight(nBot);
            auto kNb = GetMatThermalConductivity(nbEle.matId, iniT.at(nBot));
            auto r = (0.5 * height / k[2] + 0.5 * hNb / kNb[2]) / hArea;
            network->SetR(i, nBot, r);
        }
    }
}

template <typename Scalar>
void PrismThermalNetworkBuilder<Scalar>::BuildLineElement(const Vec<Scalar> & iniT, Ptr<Network> network) const
{
    for (size_t i = 0; i < m_model->TotalLineElements(); ++i) {
        const auto & line = m_model->GetLineElement(i);
        auto index = line.id;
        auto rho = GetMatMassDensity(line.matId, iniT.at(index));
        auto c = GetMatSpecificHeat(line.matId, iniT.at(index));
        auto v = GetLineVolume(index);
        network->SetC(index, c * rho * v);

        network->SetScenario(index, line.scenId);
        if (auto jh = GetLineJouleHeat(index, iniT.at(index)); jh > 0) {
            network->AddHF(index, jh);
            summary.iHeatFlow += jh;
            summary.jouleHeat += jh;
        }
        
        auto k = GetMatThermalConductivity(line.matId, iniT.at(index));
        auto aveK = (k[0] + k[1] + k[2]) / 3;
        auto area = GetLineArea(index);
        auto l = GetLineLength(index);

        auto setR = [&](size_t nbIndex) {
            if (m_model->isPrism(nbIndex)) {
                auto r = 0.5 * l / aveK / area;
                network->SetR(nbIndex, index, r);
            }
            else if (index < nbIndex) {
                const auto & lineNb = m_model->GetLineElement(m_model->LineLocalIndex(nbIndex));
                auto kNb = GetMatThermalConductivity(lineNb.matId, iniT.at(index));
                auto aveKNb = (kNb[0] + kNb[1] + kNb[2]) / 3;
                auto areaNb = GetLineArea(nbIndex);
                auto lNb = GetLineLength(nbIndex);
                auto r = 0.5 * l / aveK / area + 0.5 * lNb / aveKNb / areaNb;
                network->SetR(index, nbIndex, r);
            }
        };
        for (auto nb : line.neighbors.front()) setR(nb);
        for (auto nb : line.neighbors.back()) setR(nb);
    }
}

template <typename Scalar>
void PrismThermalNetworkBuilder<Scalar>::ApplyBlockBCs(Ptr<Network> network) const
{
    const auto & topBCs = m_model->GetBlockBCs(Orientation::Top);
    const auto & botBCs = m_model->GetBlockBCs(Orientation::Bot);
    if (topBCs.empty() && botBCs.empty()) return;

    model::utils::PrismThermalModelQuery query(*m_model);
    using RtVal = model::utils::PrismThermalModelQuery::RtVal;

    auto applyBlockBC = [&](const auto & block, bool isTop)
    {
        Vec<RtVal> results;
        if (not block.second.isValid()) return;
        auto value = block.second.value;
        for (size_t lyr = 0; lyr <  m_model->TotalLayers(); ++lyr) {
            query.SearchPrismInstances(lyr, block.first, results);
            if (results.empty()) continue;
            for (const auto & result : results) {
                const auto & prism = m_model->GetPrism(result.second);
                const auto & element = m_model->GetPrismElement(prism.layer, prism.element);
                auto nid = isTop ? model::PrismElement::TOP_NEIGHBOR_INDEX : 
                                   model::PrismElement::BOT_NEIGHBOR_INDEX ;
                if (element.neighbors.at(nid) != INVALID_INDEX) continue;
                auto area = GetPrismTopBotArea(result.second);
                if (ThermalBoundaryCondition::Type::HEAT_FLUX == block.second.type) {
                    auto heatFlow = value * area;
                    network->SetHF(result.second, heatFlow);
                    if (heatFlow > 0)
                        summary.iHeatFlow += heatFlow;
                    else summary.oHeatFlow += heatFlow;
                }
                else if (ThermalBoundaryCondition::Type::HTC == block.second.type) {
                    network->SetHTC(result.second, value);
                    summary.boundaryNodes += 1;
                }
                else if (ThermalBoundaryCondition::Type::TEMPERATURE == block.second.type) { 
                    network->SetT(result.second, TempUnit::Celsius2Kelvins(value));
                    summary.fixedTNodes += 1;
                }
            }    
        }
    };

    for (const auto & block : topBCs)
        applyBlockBC(block, true);
    for (const auto & block : botBCs)
        applyBlockBC(block, false);
}

template <typename Scalar>
const FCoord3D & PrismThermalNetworkBuilder<Scalar>::GetPrismVertexPoint(Index index, Index iv) const
{
    return m_model->GetPoint(m_model->GetPrism(index).vertices.at(iv));
}

template <typename Scalar>
FCoord2D PrismThermalNetworkBuilder<Scalar>::GetPrismVertexPoint2D(Index index, Index iv) const
{
    const auto & pt3d = GetPrismVertexPoint(index, iv);
    return FCoord2D(pt3d[0], pt3d[1]);
}

template <typename Scalar>
FCoord2D PrismThermalNetworkBuilder<Scalar>::GetPrismCenterPoint2D(Index index) const
{
    auto p0 = GetPrismVertexPoint2D(index, 0);
    auto p1 = GetPrismVertexPoint2D(index, 1);
    auto p2 = GetPrismVertexPoint2D(index, 2);
    return generic::geometry::Triangle2D<FCoord>(p0, p1, p2).Center();
}

template <typename Scalar>
Float PrismThermalNetworkBuilder<Scalar>::GetPrismCenterDist2Side(Index index, Index ie) const
{
    ie %= 3;
    auto ct = GetPrismCenterPoint2D(index);
    auto p1 = GetPrismVertexPoint2D(index, ie);
    auto p2 = GetPrismVertexPoint2D(index, (ie + 1) % 3);
    auto distSq = generic::geometry::PointLineDistanceSq(ct, p1, p2);
    return std::sqrt(distSq) * m_model->UnitScale2Meter();
}

template <typename Scalar>
Float PrismThermalNetworkBuilder<Scalar>::GetPrismEdgeLength(Index index, Index ie) const
{
    ie %= 3;
    auto p1 = GetPrismVertexPoint2D(index, ie);
    auto p2 = GetPrismVertexPoint2D(index, (ie + 1) % 3);
    auto dist = generic::geometry::Distance(p1, p2);
    return dist * m_model->UnitScale2Meter();
}

template <typename Scalar>
Float PrismThermalNetworkBuilder<Scalar>::GetPrismSideArea(Index index, Index ie) const
{
    auto h = GetPrismHeight(index);
    auto w = GetPrismEdgeLength(index, ie);
    return h * w;
}

template <typename Scalar>
Float PrismThermalNetworkBuilder<Scalar>::GetPrismTopBotArea(Index index) const
{
    const auto & points = m_model->GetPoints();
    const auto & vs = m_model->GetPrism(index).vertices;
    auto area = generic::geometry::Triangle3D<FCoord>(points.at(vs[0]), points.at(vs[1]), points.at(vs[2])).Area();
    return area * m_model->UnitScale2Meter(2);
}

template <typename Scalar>
Float PrismThermalNetworkBuilder<Scalar>::GetPrismVolume(Index index) const
{
    return GetPrismTopBotArea(index) * GetPrismHeight(index);
}

template <typename Scalar>
Float PrismThermalNetworkBuilder<Scalar>::GetPrismHeight(Index index) const
{
    const auto & prism = m_model->GetPrism(index);
    return m_model->GetLayer(prism.layer).thickness * m_model->UnitScale2Meter();
}

template <typename Scalar>
Float PrismThermalNetworkBuilder<Scalar>::GetLineJouleHeat(Index index, Float refT) const
{
    const auto & line = m_model->GetLineElement(m_model->LineLocalIndex(index));
    if (generic::math::EQ<Float>(line.current, 0)) return 0;
    auto rho = GetMatResistivity(line.matId, refT);
    return rho * GetLineLength(index) * line.current * line.current / GetLineArea(index);
}

template <typename Scalar>
Float PrismThermalNetworkBuilder<Scalar>::GetLineVolume(Index index) const
{
    return GetLineArea(index) * GetLineLength(index);
}

template <typename Scalar>
Float PrismThermalNetworkBuilder<Scalar>::GetLineLength(Index index) const
{
    const auto & line = m_model->GetLineElement(m_model->LineLocalIndex(index));
    const auto & p1 = m_model->GetPoint(line.endPts.front());
    const auto & p2 = m_model->GetPoint(line.endPts.back());
    return generic::geometry::Distance(p1, p2) * m_model->UnitScale2Meter();
}

template <typename Scalar>
Float PrismThermalNetworkBuilder<Scalar>::GetLineArea(Index index) const
{
    const auto & line = m_model->GetLineElement(m_model->LineLocalIndex(index));
    return generic::math::pi * std::pow(line.radius * m_model->UnitScale2Meter(), 2);
}

template <typename Scalar>
Arr3<Float> PrismThermalNetworkBuilder<Scalar>::GetMatThermalConductivity(Index matId, Float refT) const
{
    Arr3<Float> result{0, 0, 0};
    auto mat = CId<Material>(matId); { NS_ASSERT(mat); }
    auto prop = mat->GetProperty(Material::Prop::THERMAL_CONDUCTIVITY); { NS_ASSERT(prop); }
    for (size_t i = 0; i < result.size(); ++i) {
        [[maybe_unused]] auto check = prop->GetAnisotropicProperty(refT, i, result[i]);
        NS_ASSERT(check);
    }
    return result;
}

template <typename Scalar>
Float PrismThermalNetworkBuilder<Scalar>::GetMatMassDensity(Index matId, Float refT) const
{
    Float result{0};
    auto mat = CId<Material>(matId); { NS_ASSERT(mat); }
    auto prop = mat->GetProperty(Material::Prop::MASS_DENSITY); { NS_ASSERT(prop); }
    [[maybe_unused]] auto check = prop->GetSimpleProperty(refT, result); { NS_ASSERT(check); }
    return result;
}

template <typename Scalar>
Float PrismThermalNetworkBuilder<Scalar>::GetMatSpecificHeat(Index matId, Float refT) const
{
    Float result{0};
    auto mat = CId<Material>(matId); { NS_ASSERT(mat); }
    auto prop = mat->GetProperty(Material::Prop::SPECIFIC_HEAT); { NS_ASSERT(prop); }
    [[maybe_unused]] auto check = prop->GetSimpleProperty(refT, result); { NS_ASSERT(check); }
    return result;
}

template <typename Scalar>
Float PrismThermalNetworkBuilder<Scalar>::GetMatResistivity(Index matId, Float refT) const
{
    Float result{0};
    auto mat = Id<Material>(matId); { NS_ASSERT(mat); }
    auto prop = mat->GetProperty(Material::Prop::RESISTIVITY); { NS_ASSERT(prop); }
    [[maybe_unused]] auto check = prop->GetSimpleProperty(refT, result); { NS_ASSERT(check); }
    return result;
}

template class PrismThermalNetworkBuilder<Float32>;
template class PrismThermalNetworkBuilder<Float64>;

} // namespace nano::heat::solver::utils