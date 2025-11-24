#pragma once
#include "basic/NSHeatCommon.hpp"

#include "generic/geometry/TriangleEvaluator.hpp"
#include "generic/geometry/GeometryIO.hpp"
#include "generic/geometry/Mesh2D.hpp"
#include "generic/geometry/Utility.hpp"
#include "generic/tools/Format.hpp"

#include <string>
#include <string_view>
#include <vector>


namespace nano::heat::model::utils {

using Point = NCoord2D;
using Points = Vec<Point>;
using Polygon = NPolygon;
using Polygons = Vec<NPolygon>;
using PrismTemplate = generic::geometry::tri::Triangulation<NCoord2D>;

class GMshUtils
{
public:
    struct MeshSettings
    {
        Float minAlpha{0};
        NCoord minLen{0};
        NCoord maxLen{0};
        std::string workDir;
    };
    static bool WriteGeoFile(const Polygon & outline, const Polygons & shapes, const Points & steinerPoints, const MeshSettings & meshSettings);
    static bool ReadMshFile(std::string_view filename, PrismTemplate & triangulation);


};


} // namespace nano::heat::model::utils