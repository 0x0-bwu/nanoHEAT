#pragma once
#include "NSModelPrismThermal.h"

namespace nano::heat::model {

namespace utils {

class PrismStackupThermalModelQuery;
class PrismStackupThermalModelBuilder;

} // namespace utils
class PrismStackupThermalModel : public PrismThermalModel
{
public:
    friend class utils::PrismStackupThermalModelQuery;
    friend class utils::PrismStackupThermalModelBuilder;
    PrismStackupThermalModel();
    void SearchElementIndices(const Vec<FCoord3D> & monitors, Vec<Index> & indices) const override;
private:
    NS_SERIALIZATION_FUNCTIONS_DECLARATION;
};

} // namespace nano::heat::model
NS_SERIALIZATION_CLASS_EXPORT_KEY(nano::heat::model::PrismStackupThermalModel)