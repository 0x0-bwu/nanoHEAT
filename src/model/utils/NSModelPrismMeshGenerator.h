#pragma once
#include "basic/NSHeatCommon.hpp"

#include "generic/geometry/GeometryIO.hpp"
#include "generic/geometry/Mesh2D.hpp"

namespace nano::heat::model::utils {

using PrismTemplate = generic::geometry::tri::Triangulation<NCoord2D>;
inline bool GenerateMesh(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints, 
                         const CoordUnit & coordUnit, const PrismMeshSettings & meshSettings, PrismTemplate & triangulation,
                         std::string_view workDir = nano::CurrentDir())
{
    using namespace generic::geometry;
    if (meshSettings.dumpMeshFile) {
        GeometryIO::WritePNG(std::string(workDir) + "/meshIn.png", polygons.begin(), polygons.end(), 4096);
        GeometryIO::WriteWKT<NPolygon>(std::string(workDir) + "/meshIn.wkt", polygons.begin(), polygons.end());
    }
    auto minAlpha = generic::math::Rad(meshSettings.minAlpha);
    auto minLen = coordUnit.toCoord(meshSettings.minLen);
    auto maxLen = coordUnit.toCoord(meshSettings.maxLen);
    auto tolerance = coordUnit.toCoord(meshSettings.tolerance);
    mesh2d::IndexEdgeList edges;
    mesh2d::Point2DContainer points;
    mesh2d::Segment2DContainer segments;
    mesh2d::ExtractIntersections(polygons, segments);
    mesh2d::ExtractTopology(segments, points, edges);
    points.reserve(points.size() + steinerPoints.size());
    points.insert(points.end(), steinerPoints.begin(), steinerPoints.end());
    mesh2d::MergeClosePointsAndRemapEdge(points, edges, tolerance);
    mesh2d::SplitOverlengthEdges(points, edges, maxLen);
    mesh2d::TriangulatePointsAndEdges(points, edges, triangulation);
    // mesh2d::AddPointsFromBalancedQuadTree(ConvexHull(polygons), points, 10, nano::thread::Threads());
    mesh2d::TriangulationRefinement(triangulation, minAlpha, minLen, maxLen, meshSettings.maxIter);
    if (meshSettings.dumpMeshFile)
        GeometryIO::WritePNG(std::string(workDir) + "/meshOut.png", triangulation, 4096);
    return true;
}

} // namespace nano::heat::model::utils