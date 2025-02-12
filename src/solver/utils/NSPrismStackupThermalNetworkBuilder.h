#pragma once
#include "basic/NSHeatCommon.hpp"
#include "solver/utils/NSPrismThermalNetworkBuilder.h"

namespace nano::heat {

namespace model { class PrismStackupThermalModel; }

namespace solver::utils {

template <typename Scalar>
class PrismStackupThermalNetworkBuilder : public PrismThermalNetworkBuilder<Scalar>
{
public:
    using ModelType = model::PrismStackupThermalModel;
    using Network = network::ThermalNetwork<Scalar>;
    explicit PrismStackupThermalNetworkBuilder(CPtr<ModelType> model);
    virtual ~PrismStackupThermalNetworkBuilder() = default;

private:
    void BuildPrismElement(const Vec<Scalar> & iniT, Ptr<Network> network, Index start, Index end) const override;
    void ApplyBlockBCs(Ptr<Network> network) const override;
};
} // namespace solver::utils

} // namespace nano::heat