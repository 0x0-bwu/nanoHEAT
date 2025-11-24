#pragma once
#include "basic/NSHeatCommon.hpp"

#include "generic/geometry/TriangleEvaluator.hpp"
#include "generic/geometry/Triangulation.hpp"
#include "generic/geometry/GeometryIO.hpp"
#include "generic/geometry/Mesh2D.hpp"
#include "generic/geometry/Utility.hpp"
#include "generic/tools/Format.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace nano::heat::model::utils {

using namespace generic;
using namespace generic::geometry;
using Edges = mesh2d::Edges;
using Points = mesh2d::Points;
using Segments = mesh2d::Segments;
using PrismTemplate = tri::Triangulation<NCoord2D>;
bool GenerateMesh(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints, 
                  const CoordUnit & coordUnit, const PrismMeshSettings & meshSettings,
                  PrismTemplate & triangulation, std::string_view workDir);

} // namespace nano::heat::model::utils