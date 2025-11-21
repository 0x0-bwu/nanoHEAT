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

using Points = Vec<NCoord2D>;
using Polygons = Vec<NPolygon>;
using PrismTemplate = generic::geometry::tri::Triangulation<NCoord2D>;

class GMshUtils
{
public:
    struct MeshSettings
    {
        Float minAlpha;
        NCoord minLen;
        NCoord maxLen;
        std::string workDir;
    };
    static bool WriteGeoFile(const Polygons & polygons, const Points & steinerPoints, const MeshSettings & meshSettings);
    static bool ReadMshFile(std::string_view filename, PrismTemplate & triangulation);


};


} // namespace nano::heat::model::utils