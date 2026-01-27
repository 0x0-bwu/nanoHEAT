#include "NSModelPrismMeshGenerator.h"

#include "NSModelPrismMeshGmshUtils.h"

namespace nano::heat::model::utils {

using namespace generic;
using namespace generic::geometry;

bool GenerateMeshInternal(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints, 
                  const CoordUnit & coordUnit, const PrismMeshSettings & meshSettings,
                  PrismTemplate & triangulation, std::string_view workDir)
{
    auto minAlpha = meshSettings.minAlpha;
    auto minLen = coordUnit.toCoord(meshSettings.minLen);
    auto maxLen = coordUnit.toCoord(meshSettings.maxLen);
    auto tolerance = coordUnit.toCoord(meshSettings.tolerance);
    Edges edges;
    Points points;
    Segments segments, intersections;
    auto bbox = Extent(polygons.begin(), polygons.end());
    mesh2d::ExtractSegments(toPolygon(bbox), segments);
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
    return true;
}

bool GenerateMeshGmsh(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints, 
                      const CoordUnit & coordUnit, const PrismMeshSettings & meshSettings,
                      PrismTemplate & triangulation, std::string_view workDir)
{
    auto minLen = coordUnit.toCoord(meshSettings.minLen);
    auto maxLen = coordUnit.toCoord(meshSettings.maxLen);
    GMshUtils::MeshSettings settings;
    settings.minLen = minLen;
    settings.maxLen = maxLen;
    settings.workDir = workDir;
    
    auto bbox = Extent(polygons.begin(), polygons.end());
    bbox.Scale(1.1);
    GMshUtils::WriteGeoFile(toPolygon(bbox), polygons, steinerPoints, settings);
    
    bool quiet{false};
    auto geoFile = std::string(workDir) + "/mesh.geo";
    auto mshFile = std::string(workDir) + "/mesh.msh";
    if (not fs::FileExists(geoFile)) {
        NS_TRACE("Failed to generate .geo file %1%", geoFile);
        return false;
    }

    std::string cmd = meshSettings.mesher + " -2 -format msh2 -o " + mshFile + " " + geoFile;
    if (quiet) cmd += " -v 0";
    NS_TRACE("Calling Gmsh: %1%", cmd);
    
    int result = std::system(cmd.c_str());
    
    #ifdef _WIN32
    #define WEXITSTATUS(result) (result)
    #endif

    int exitCode = WEXITSTATUS(result);
    
    // Detailed error information
    if (result != 0) {
        NS_TRACE("Gmsh failed with return code: %1% (exit code: %2%)", result, exitCode);
        
        if (exitCode == 1) {
            NS_TRACE("Common causes:");
            NS_TRACE("  - Gmsh not found in PATH");
            NS_TRACE("  - Invalid .geo file syntax");
            NS_TRACE("  - Permission denied for output directory");
        }
        
        // Try running without silent mode to get error information
        std::string verboseCmd = meshSettings.mesher + " -2 -format msh2 -o " + mshFile + " " + geoFile;
        NS_TRACE("Try running manually: %1%", verboseCmd);
        
        return false;
    }

    return GMshUtils::ReadMshFile(mshFile, triangulation);
}

bool GenerateMesh(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints, 
                  const CoordUnit & coordUnit, const PrismMeshSettings & meshSettings,
                  PrismTemplate & triangulation, std::string_view workDir)
{

    if (meshSettings.dumpMeshFile) {
        NS_TRACE("mesh dir: %1%, minAlpha: %2%, minLen: %3%, maxLen: %4%, tolerance: %5%", 
                 workDir, math::Deg(meshSettings.minAlpha),
                meshSettings.minLen, meshSettings.maxLen, meshSettings.tolerance);
        GeometryIO::WritePNG(std::string(workDir) + "/meshIn.png", polygons.begin(), polygons.end(), 4096);
        GeometryIO::WriteWKT<NPolygon>(std::string(workDir) + "/meshIn.wkt", polygons.begin(), polygons.end());
    }

    bool res{false};
    if (MesherType::INTERNAL_MESHER == meshSettings.mesherType) {
        res = GenerateMeshInternal(polygons, steinerPoints, coordUnit, meshSettings, triangulation, workDir);
    }
    else if (MesherType::GMSH == meshSettings.mesherType) {
        res = GenerateMeshGmsh(polygons, steinerPoints, coordUnit, meshSettings, triangulation, workDir);
    }

    if (not res) {
        NS_ERROR("Failed to generate mesh");
        return false;
    }

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