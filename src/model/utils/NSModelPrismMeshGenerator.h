#pragma once
#include "basic/NSHeatCommon.hpp"

#include "generic/geometry/TriangleEvaluator.hpp"
#include "generic/geometry/GeometryIO.hpp"
#include "generic/geometry/Mesh2D.hpp"

namespace nano::heat::model::utils {

using PrismTemplate = generic::geometry::tri::Triangulation<NCoord2D>;
inline bool GenerateMesh(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints, 
                         const CoordUnit & coordUnit, const PrismMeshSettings & meshSettings, PrismTemplate & triangulation,
                         std::string_view workDir = nano::CurrentDir())
{
    using namespace generic;
    using namespace generic::geometry;
    auto minAlpha = math::Rad(meshSettings.minAlpha);
    auto minLen = coordUnit.toCoord(meshSettings.minLen);
    auto maxLen = coordUnit.toCoord(meshSettings.maxLen);
    auto tolerance = coordUnit.toCoord(meshSettings.tolerance);
    if (meshSettings.dumpMeshFile) {
        NS_TRACE("mesh dir: %1%, minAlpha: %2%, minLen: %3%, maxLen: %4%, tolerance: %5%", workDir, minAlpha, minLen, maxLen, tolerance);
        GeometryIO::WritePNG(std::string(workDir) + "/meshIn.png", polygons.begin(), polygons.end(), 4096);
        GeometryIO::WriteWKT<NPolygon>(std::string(workDir) + "/meshIn.wkt", polygons.begin(), polygons.end());
    }
    mesh2d::IndexEdgeList edges;
    mesh2d::Point2DContainer points;
    mesh2d::Segment2DContainer segments, intersections;

    auto bbox = Extent(polygons.begin(), polygons.end());
    mesh2d::ExtractSegment(toPolygon(bbox), segments);
    mesh2d::ExtractSegments(polygons, segments);
    mesh2d::ExtractIntersections(segments, intersections);
    mesh2d::ExtractTopology(intersections, points, edges);
    points.reserve(points.size() + steinerPoints.size());
    points.insert(points.end(), steinerPoints.begin(), steinerPoints.end());
    mesh2d::MergeClosePointsAndRemapEdge(points, edges, tolerance);
    if (meshSettings.preSplitEdge)
        mesh2d::SplitOverlengthEdges(points, edges, maxLen);
    mesh2d::TriangulatePointsAndEdges(points, edges, triangulation);
    if (meshSettings.addBalancedPoints)
        mesh2d::AddPointsFromBalancedQuadTree(ConvexHull(polygons), points, 10, nano::thread::Threads());
    mesh2d::TriangulationRefinement(triangulation, minAlpha, minLen, maxLen, meshSettings.maxIter);

    if (meshSettings.dumpMeshFile) {
        NS_TRACE("writing mesh file to %1%, total triangles: %2%", workDir, triangulation.triangles.size());
        GeometryIO::WritePNG(std::string(workDir) + "/meshOut.png", triangulation, 4096);
    }

    if (meshSettings.reportMeshQuality) {
        tri::TriIdxSet skipT;
        tri::VerIdxSet skipV;
        tri::TriangleEvaluator<NCoord2D> evaluator(triangulation, skipT, skipV);
        auto results = evaluator.Report();
        NS_TRACE("mesh quality:");
        NS_TRACE("total nodes: %1%, total elements: %2%", results.nodes, results.elements);
        NS_TRACE("min angle: %1%, max angle: %2%", results.minAngle, results.maxAngle);
        NS_TRACE("min edge length: %1%, max edge length: %2%", results.minEdgeLen, results.maxEdgeLen);
        NS_TRACE("angle histogram: [%1%]", fmt::Fmt2Str(results.triAngleHistogram, ","));
        NS_TRACE("edge length histogram: [%1%]", fmt::Fmt2Str(results.triEdgeLenHistogram, ","));
    }
    return true;
}

} // namespace nano::heat::model::utils