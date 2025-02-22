#include "NSPrismStackupThermalNetworkBuilder.h"
#include "model/utils/NSModelPrismStackupThermalQuery.h"
#include "model/NSModelPrismThermal.h"
#include <nano/core/common>
#include <nano/core/basic>

namespace nano::heat::solver::utils {

inline static constexpr auto NO_NEIGHBOR = generic::geometry::tri::noNeighbor;

template <typename Scalar>
PrismStackupThermalNetworkBuilder<Scalar>::PrismStackupThermalNetworkBuilder(CPtr<ModelType> model)
 : PrismThermalNetworkBuilder<Scalar>(model)
{
    NS_ASSERT(this->m_model);
}

template <typename Scalar>
void PrismStackupThermalNetworkBuilder<Scalar>::BuildPrismElement(const Vec<Scalar> & iniT, Ptr<Network> network, Index start, Index end) const
{
    const auto & model = *this->m_model;
    auto topBC = model.GetUniformBC(Orientation::TOP);
    auto botBC = model.GetUniformBC(Orientation::BOT);
    
    auto & summary = PrismThermalNetworkBuilder<Scalar>::summary;
    for (size_t i = start; i < end; ++i) {
        const auto & inst = model.GetPrism(i);
        const auto & element = model.GetPrismElement(inst.layer, inst.element);
        if (auto lut = CId<LookupTable>(element.powerLutId); lut) {
            auto p = lut->Lookup(iniT.at(i), /*extrapolation*/false);
            p *= element.powerRatio;
            summary.iHeatFlow += p;
            network->AddHF(i, p);
            network->SetScenario(i, element.scenId);
        }

        auto c = this->GetMatSpecificHeat(element.matId, iniT.at(i));
        auto rho = this->GetMatMassDensity(element.matId, iniT.at(i));
        auto vol = this->GetPrismVolume(i);
        network->SetC(i, c * rho * vol);

        auto k = this->GetMatThermalConductivity(element.matId, iniT.at(i));
        auto ct = this->GetPrismCenterPoint2D(i);

        const auto & neighbors = inst.neighbors;
        //edges
        for (size_t ie = 0; ie < 3; ++ie) {
            auto vArea = this->GetPrismSideArea(i, ie);
            if (auto nid = neighbors.at(ie); NO_NEIGHBOR == nid) {
                //todo, side bc
            }
            else if (i < nid) { //one way
                const auto & nb = model.GetPrism(nid);
                const auto & nbEle = model.GetPrismElement(nb.layer, nb.element);
                auto ctNb = this->GetPrismCenterPoint2D(nid);
                auto vec = ctNb - ct;
                auto dist = vec.Norm2() * model.UnitScale2Meter();
                auto kxy = 0.5 * (k[0] + k[1]);
                auto dist2edge = this->GetPrismCenterDist2Side(i, ie);
                auto r1 = dist2edge / kxy / vArea;

                auto kNb = this->GetMatThermalConductivity(nbEle.matId, iniT.at(nid));
                auto kNbXY = 0.5 * (kNb[0] + kNb[1]);
                auto r2 = (dist - dist2edge) / kNbXY / vArea;
                network->SetR(i, nid, r1 + r2);
            }
        }
        auto height = this->GetPrismHeight(i);
        auto hArea = this->GetPrismTopBotArea(i);
        //top
        auto nTop = neighbors.at(model::PrismElement::TOP_NEIGHBOR_INDEX);
        if (NO_NEIGHBOR == nTop) {
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
                    network->SetT(i, topBC->value);
                    summary.fixedTNodes += 1;
                }
            }
        }
        else if (i == nTop) {
            Float64 ratio = 1.0;
            for (const auto & contact : inst.contacts.front()) {
                NS_ASSERT(contact.ratio > 0);
                ratio -= contact.ratio;
                if (contact.id < i) continue;
                nTop = contact.id;
                const auto & nb = model.GetPrism(nTop);
                const auto & nbEle = model.GetPrismElement(nb.layer, nb.element);
                auto hNb = this->GetPrismHeight(nTop);
                auto kNb = this->GetMatThermalConductivity(nbEle.matId, iniT.at(nTop));
                auto area = hArea * contact.ratio;
                auto r =  (0.5 * height / k[2] + 0.5 * hNb / kNb[2]) / area;
                // auto r = 0.5 * height / k[2] / hArea + 0.5 * hNb / kNb[2] / GetPrismTopBotArea(nTop);
                network->SetR(i, nTop, r);
            }
            if (ratio > 0 && nullptr != topBC && topBC->isValid()) {
                if (ThermalBoundaryCondition::Type::HTC == topBC->type) {
                    network->SetHTC(i, topBC->value * hArea * ratio);
                    summary.boundaryNodes += 1;
                }
                else if (ThermalBoundaryCondition::Type::HEAT_FLUX == topBC->type) {
                    auto heatFlow = topBC->value * hArea * ratio;
                    network->SetHF(i, heatFlow);
                    if (heatFlow > 0)
                        summary.iHeatFlow += heatFlow;
                    else summary.oHeatFlow += heatFlow;
                }
                // else if (ThermalBoundaryCondition::Type::TEMPERATURE == topBC->type) {
                //     network->SetT(i, topBC->value);
                //     summary.fixedTNodes += 1;
                // }
            }
        }
        else {
            NS_ASSERT(false);
        }
        //bot
        auto nBot = neighbors.at(model::PrismElement::BOT_NEIGHBOR_INDEX);
        if (NO_NEIGHBOR == nBot) {
            if (nullptr != botBC && botBC->isValid()) {
                if (ThermalBoundaryCondition::Type::HTC == botBC->type) {
                    network->SetHTC(i, botBC->value * hArea);
                    summary.boundaryNodes += 1;
                }
                else if (ThermalBoundaryCondition::Type::HEAT_FLUX== botBC->type) {
                    network->SetHF(i, botBC->value);
                    if (botBC->value > 0)
                        summary.iHeatFlow += botBC->value;
                    else summary.oHeatFlow += botBC->value;
                }
                else if (ThermalBoundaryCondition::Type::TEMPERATURE == botBC->type) {
                    network->SetT(i, botBC->value);
                    summary.fixedTNodes += 1;
                }
            }
        }
        else if (i == nBot) {
            Float64 ratio = 1.0;
            for (const auto & contact : inst.contacts.back()) {
                NS_ASSERT(contact.ratio > 0);
                ratio -= contact.ratio;
                if (contact.id < i) continue;
                nBot = contact.id;
                const auto & nb = model.GetPrism(nBot);
                const auto & nbEle = model.GetPrismElement(nb.layer, nb.element);
                auto hNb = this->GetPrismHeight(nBot);
                auto kNb = this->GetMatThermalConductivity(nbEle.matId, iniT.at(nBot));
                auto area = hArea * contact.ratio;
                auto r =  (0.5 * height / k[2] + 0.5 * hNb / kNb[2]) / area;
                // auto r = 0.5 * height / k[2] / hArea + 0.5 * hNb / kNb[2] / GetPrismTopBotArea(nBot);
                network->SetR(i, nBot, r);
            }
            if (ratio > 0 && nullptr != botBC && botBC->isValid()) {
                if (ThermalBoundaryCondition::Type::HTC == botBC->type) {
                    network->SetHTC(i, botBC->value * hArea * ratio);
                    summary.boundaryNodes += 1;
                }
                else if (ThermalBoundaryCondition::Type::HEAT_FLUX == botBC->type) {
                    auto heatFlow = botBC->value * hArea * ratio;
                    network->SetHF(i, heatFlow);
                    if (heatFlow > 0)
                        summary.iHeatFlow += heatFlow;
                    else summary.oHeatFlow += heatFlow;
                }
                // else if (ThermalBoundaryCondition::Type::TEMPERATURE == botBC->type) {
                //     network->SetT(i, botBC->value);
                //     summary.fixedTNodes += 1;
                // }
            }
        }
        else {
            NS_ASSERT(false);
        }
    }
}


template <typename Scalar>
void PrismStackupThermalNetworkBuilder<Scalar>::ApplyBlockBCs(Ptr<Network> network) const
{
    const auto & model = *this->m_model;
    const auto & topBCs = model.GetBlockBCs(Orientation::TOP);
    const auto & botBCs = model.GetBlockBCs(Orientation::BOT);
    if (topBCs.empty() && botBCs.empty()) return;

    model::utils::PrismStackupThermalModelQuery query(dynamic_cast<CRef<model::PrismStackupThermalModel>>(model));
    using RtVal = model::utils::PrismStackupThermalModelQuery::RtVal;
    
    auto & summary = PrismThermalNetworkBuilder<Scalar>::summary;
    auto applyBlockBC = [&](const auto & block, bool isTop)
    {
        std::vector<RtVal> results;
        if (not block.second.isValid()) return;
        auto value = block.second.value;
        if (ThermalBoundaryCondition::Type::HEAT_FLUX == block.second.type)
            value /= block.first.Area() * model.CoordScale2Meter(2);

        for (size_t lyr = 0; lyr <  model.TotalLayers(); ++lyr) {
            query.SearchPrismInstances(lyr, block.first, results);
            if (results.empty()) continue;
            for (const auto & result : results) {
                const auto & prism = model.GetPrism(result.second);
                const auto & element = model.GetPrismElement(prism.layer, prism.element);
                auto nid = isTop ? model::PrismElement::TOP_NEIGHBOR_INDEX : 
                                   model::PrismElement::BOT_NEIGHBOR_INDEX ;
                if (NO_NEIGHBOR != element.neighbors.at(nid)) continue;
                auto area = this->GetPrismTopBotArea(result.second);
                if (ThermalBoundaryCondition::Type::HEAT_FLUX == block.second.type) {
                    auto heatFlow = value * area;
                    network->SetHF(result.second, heatFlow);
                    if (heatFlow > 0)
                        summary.iHeatFlow += heatFlow;
                    else summary.oHeatFlow += heatFlow;
                }
                else if (ThermalBoundaryCondition::Type::HTC == block.second.type) { 
                    network->SetHTC(result.second, value * area);
                    summary.boundaryNodes += 1;
                }
                else if (ThermalBoundaryCondition::Type::TEMPERATURE == block.second.type) { 
                    network->SetT(result.second, value);
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

template class PrismStackupThermalNetworkBuilder<Float32>;
template class PrismStackupThermalNetworkBuilder<Float64>;

} // namespace nano::heat::solver::utils