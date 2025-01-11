#pragma once
#include "generic/math/LookupTable.hpp"
namespace nano::heat {

template<typename Tag>
using Index = generic::utils::Index<Tag, IdType>;

using ScenarioId = Index<class Scenario>;

using Lut1D = generic::math::LookupTable<Float, 1>;

} // namespace nano::heat