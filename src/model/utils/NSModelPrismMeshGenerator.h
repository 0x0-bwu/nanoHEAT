#pragma once
#include "basic/NSHeatCommon.hpp"

#include "generic/geometry/TriangleEvaluator.hpp"
#include "generic/geometry/GeometryIO.hpp"
#include "generic/geometry/Mesh2D.hpp"
#include "generic/geometry/Utility.hpp"

#include <filesystem>
#include <sstream>
#include <fstream>
#include <cstdlib>

namespace nano::heat::model::utils {

using PrismTemplate = generic::geometry::tri::Triangulation<NCoord2D>;

inline bool GenerateMeshInternal(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints, 
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
        tri::TriangleEvaluator<NCoord2D> evaluator(triangulation, {}, {});
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

// Helper function to write Gmsh .geo file
inline bool WriteGmshGeoFile(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints,
                              const CoordUnit & coordUnit, const PrismMeshSettings & meshSettings,
                              const std::string & geoFilePath)
{
    using namespace generic::geometry;
    
    std::ofstream geoFile(geoFilePath);
    if (!geoFile.is_open()) {
        NS_TRACE("Failed to open .geo file for writing: %1%", geoFilePath);
        return false;
    }
    
    auto minLen = coordUnit.toCoord(meshSettings.minLen);
    auto maxLen = coordUnit.toCoord(meshSettings.maxLen);
    
    // Write mesh size parameters
    geoFile << "// Mesh size settings\n";
    geoFile << "Mesh.CharacteristicLengthMin = " << minLen << ";\n";
    geoFile << "Mesh.CharacteristicLengthMax = " << maxLen << ";\n";
    geoFile << "Mesh.Algorithm = 6; // Frontal-Delaunay for 2D\n";
    geoFile << "\n";
    
    // Extract all unique points from polygons and create a point index map
    // Using a custom comparator for NCoord2D
    auto pointCompare = [](const NCoord2D & a, const NCoord2D & b) {
        if (a[0] != b[0]) return a[0] < b[0];
        return a[1] < b[1];
    };
    std::map<NCoord2D, size_t, decltype(pointCompare)> pointIndexMap(pointCompare);
    size_t pointCounter = 1; // Gmsh uses 1-based indexing
    
    // Helper to get or create point index
    auto getPointIndex = [&](const NCoord2D & p) -> size_t {
        auto it = pointIndexMap.find(p);
        if (it != pointIndexMap.end()) {
            return it->second;
        }
        size_t idx = pointCounter++;
        pointIndexMap[p] = idx;
        geoFile << "Point(" << idx << ") = {" << p[0] << ", " << p[1] << ", 0};\n";
        return idx;
    };
    
    // Process polygons and create lines
    geoFile << "// Points from polygons\n";
    Vec<Vec<size_t>> polygonLineLoops;
    size_t lineCounter = 1;
    
    for (const auto & polygon : polygons) {
        Vec<size_t> lineIds;
        for (size_t i = 0; i < polygon.Size(); ++i) {
            size_t j = (i + 1) % polygon.Size();
            size_t p1 = getPointIndex(polygon[i]);
            size_t p2 = getPointIndex(polygon[j]);
            
            geoFile << "Line(" << lineCounter << ") = {" << p1 << ", " << p2 << "};\n";
            lineIds.push_back(lineCounter);
            lineCounter++;
        }
        polygonLineLoops.push_back(lineIds);
    }
    
    // Add steiner points
    if (!steinerPoints.empty()) {
        geoFile << "\n// Steiner points\n";
        for (const auto & pt : steinerPoints) {
            getPointIndex(pt);
        }
    }
    
    // Create curve loops and plane surfaces
    geoFile << "\n// Curve loops and surfaces\n";
    size_t loopCounter = 1;
    size_t surfaceCounter = 1;
    
    for (const auto & lineIds : polygonLineLoops) {
        geoFile << "Curve Loop(" << loopCounter << ") = {";
        for (size_t i = 0; i < lineIds.size(); ++i) {
            if (i > 0) geoFile << ", ";
            geoFile << lineIds[i];
        }
        geoFile << "};\n";
        
        geoFile << "Plane Surface(" << surfaceCounter << ") = {" << loopCounter << "};\n";
        loopCounter++;
        surfaceCounter++;
    }
    
    // Add steiner points to the surface mesh
    if (!steinerPoints.empty()) {
        geoFile << "\n// Embed steiner points in surface\n";
        geoFile << "Point{";
        bool first = true;
        for (const auto & pt : steinerPoints) {
            auto it = pointIndexMap.find(pt);
            if (it != pointIndexMap.end()) {
                if (!first) geoFile << ", ";
                geoFile << it->second;
                first = false;
            }
        }
        geoFile << "} In Surface{1};\n";
    }
    
    geoFile.close();
    return true;
}

// Helper function to call Gmsh command line tool
inline bool CallGmshMesher(const std::string & geoFilePath, const std::string & mshFilePath, int dimension = 2)
{
    // Build the gmsh command
    std::stringstream cmd;
    cmd << "gmsh -" << dimension << " -format msh2 -o " << mshFilePath << " " << geoFilePath;
    cmd << " -v 0"; // Quiet mode
    
    NS_TRACE("Calling Gmsh: %1%", cmd.str());
    
    int result = std::system(cmd.str().c_str());
    if (result != 0) {
        NS_TRACE("Gmsh execution failed with code: %1%", result);
        return false;
    }
    
    // Check if output file was created
    if (!std::filesystem::exists(mshFilePath)) {
        NS_TRACE("Gmsh output file not found: %1%", mshFilePath);
        return false;
    }
    
    return true;
}

// Helper function to read Gmsh .msh file and convert to triangulation
inline bool ReadGmshMshFile(const std::string & mshFilePath, PrismTemplate & triangulation)
{
    using namespace generic::geometry;
    
    std::ifstream mshFile(mshFilePath);
    if (!mshFile.is_open()) {
        NS_TRACE("Failed to open .msh file for reading: %1%", mshFilePath);
        return false;
    }
    
    triangulation.points.clear();
    triangulation.vertices.clear();
    triangulation.triangles.clear();
    triangulation.fixedEdges.clear();
    
    std::string line;
    bool inNodes = false;
    bool inElements = false;
    size_t numNodes = 0;
    size_t numElements = 0;
    
    // Map from Gmsh node ID to our point index
    std::map<size_t, size_t> gmshIdToPointIdx;
    
    while (std::getline(mshFile, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (line.empty()) continue;
        
        // Parse $Nodes section
        if (line == "$Nodes") {
            inNodes = true;
            std::getline(mshFile, line);
            numNodes = std::stoull(line);
            
            for (size_t i = 0; i < numNodes; ++i) {
                std::getline(mshFile, line);
                std::istringstream iss(line);
                size_t nodeId;
                NCoord2D::coor_t x, y, z;
                iss >> nodeId >> x >> y >> z;
                
                size_t pointIdx = triangulation.points.size();
                triangulation.points.emplace_back(x, y);
                gmshIdToPointIdx[nodeId] = pointIdx;
                
                // Create vertex for this point
                tri::IndexVertex vertex;
                vertex.index = pointIdx;
                triangulation.vertices.push_back(vertex);
            }
        }
        else if (line == "$EndNodes") {
            inNodes = false;
        }
        // Parse $Elements section
        else if (line == "$Elements") {
            inElements = true;
            std::getline(mshFile, line);
            numElements = std::stoull(line);
            
            for (size_t i = 0; i < numElements; ++i) {
                std::getline(mshFile, line);
                std::istringstream iss(line);
                size_t elemId, elemType, numTags;
                iss >> elemId >> elemType >> numTags;
                
                // Skip tags
                for (size_t t = 0; t < numTags; ++t) {
                    size_t tag;
                    iss >> tag;
                }
                
                // Element type 2 = 3-node triangle
                if (elemType == 2) {
                    size_t n1, n2, n3;
                    iss >> n1 >> n2 >> n3;
                    
                    // Convert Gmsh node IDs to our point indices
                    size_t v1 = gmshIdToPointIdx[n1];
                    size_t v2 = gmshIdToPointIdx[n2];
                    size_t v3 = gmshIdToPointIdx[n3];
                    
                    // Create triangle
                    tri::IndexTriangle triangle;
                    triangle.vertices[0] = v1;
                    triangle.vertices[1] = v2;
                    triangle.vertices[2] = v3;
                    
                    size_t triIdx = triangulation.triangles.size();
                    triangulation.triangles.push_back(triangle);
                    
                    // Update vertex-to-triangle mapping
                    triangulation.vertices[v1].triangles.insert(triIdx);
                    triangulation.vertices[v2].triangles.insert(triIdx);
                    triangulation.vertices[v3].triangles.insert(triIdx);
                }
                // Element type 1 = 2-node line (boundary edge)
                else if (elemType == 1) {
                    size_t n1, n2;
                    iss >> n1 >> n2;
                    
                    size_t v1 = gmshIdToPointIdx[n1];
                    size_t v2 = gmshIdToPointIdx[n2];
                    
                    triangulation.fixedEdges.insert(tri::IndexEdge(v1, v2));
                }
            }
        }
        else if (line == "$EndElements") {
            inElements = false;
        }
    }
    
    mshFile.close();
    
    // Build triangle neighbor relationships
    // For each triangle, find neighbors by looking for triangles that share an edge
    using EdgeToTriMap = std::map<tri::IndexEdge, size_t>;
    EdgeToTriMap edgeToTriangle;
    
    for (size_t triIdx = 0; triIdx < triangulation.triangles.size(); ++triIdx) {
        auto & triangle = triangulation.triangles[triIdx];
        
        // For each edge of the triangle
        for (size_t i = 0; i < 3; ++i) {
            size_t v1 = triangle.vertices[i];
            size_t v2 = triangle.vertices[(i + 1) % 3];
            tri::IndexEdge edge(v1, v2);
            
            // Check if this edge already has a triangle assigned
            auto it = edgeToTriangle.find(edge);
            if (it != edgeToTriangle.end()) {
                // Found neighbor - update both triangles
                size_t neighborIdx = it->second;
                triangle.neighbors[i] = neighborIdx;
                
                // Find the edge index in the neighbor triangle and update it
                auto & neighbor = triangulation.triangles[neighborIdx];
                for (size_t j = 0; j < 3; ++j) {
                    size_t nv1 = neighbor.vertices[j];
                    size_t nv2 = neighbor.vertices[(j + 1) % 3];
                    if (tri::IndexEdge(nv1, nv2) == edge) {
                        neighbor.neighbors[j] = triIdx;
                        break;
                    }
                }
            } else {
                // First triangle with this edge
                edgeToTriangle[edge] = triIdx;
            }
        }
    }
    
    NS_TRACE("Read mesh from Gmsh: %1% nodes, %2% triangles", triangulation.points.size(), triangulation.triangles.size());
    
    return true;
}

inline bool GenerateMeshGmsh(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints, 
                             const CoordUnit & coordUnit, const PrismMeshSettings & meshSettings, PrismTemplate & triangulation,
                             std::string_view workDir = nano::CurrentDir())
{
    using namespace generic;
    using namespace generic::geometry;
    
    NS_TRACE("Starting Gmsh meshing");
    
    // Step 1: Convert input data to Gmsh .geo file
    std::string geoFilePath = std::string(workDir) + "/mesh.geo";
    if (!WriteGmshGeoFile(polygons, steinerPoints, coordUnit, meshSettings, geoFilePath)) {
        NS_TRACE("Failed to write Gmsh .geo file");
        return false;
    }
    
    // Step 2: Call Gmsh to generate mesh
    std::string mshFilePath = std::string(workDir) + "/mesh.msh";
    if (!CallGmshMesher(geoFilePath, mshFilePath, 2)) {
        NS_TRACE("Failed to call Gmsh mesher, falling back to internal mesher");
        // Fall back to internal mesher if Gmsh is not available
        return GenerateMeshInternal(polygons, steinerPoints, coordUnit, meshSettings, triangulation, workDir);
    }
    
    // Step 3: Read Gmsh output and convert to internal triangulation
    if (!ReadGmshMshFile(mshFilePath, triangulation)) {
        NS_TRACE("Failed to read Gmsh mesh file, falling back to internal mesher");
        return GenerateMeshInternal(polygons, steinerPoints, coordUnit, meshSettings, triangulation, workDir);
    }
    
    // Optional: Output mesh visualization
    if (meshSettings.dumpMeshFile) {
        NS_TRACE("Writing mesh visualization to %1%", workDir);
        GeometryIO::WritePNG(std::string(workDir) + "/meshOutGmsh.png", triangulation, 4096);
    }
    
    // Optional: Report mesh quality
    if (meshSettings.reportMeshQuality) {
        tri::TriangleEvaluator<NCoord2D> evaluator(triangulation, {}, {});
        auto results = evaluator.Report();
        NS_TRACE("Gmsh mesh quality:");
        NS_TRACE("total nodes: %1%, total elements: %2%", results.nodes, results.elements);
        NS_TRACE("min angle: %1%, max angle: %2%", results.minAngle, results.maxAngle);
        NS_TRACE("min edge length: %1%, max edge length: %2%", results.minEdgeLen, results.maxEdgeLen);
        NS_TRACE("angle histogram: [%1%]", fmt::Fmt2Str(results.triAngleHistogram, ","));
        NS_TRACE("edge length histogram: [%1%]", fmt::Fmt2Str(results.triEdgeLenHistogram, ","));
    }
    
    return true;
}

inline bool GenerateMesh(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints, 
                         const CoordUnit & coordUnit, const PrismMeshSettings & meshSettings, PrismTemplate & triangulation,
                         std::string_view workDir = nano::CurrentDir())
{
    switch (meshSettings.mesherType) {
        case MesherType::INTERNAL_MESHER:
            return GenerateMeshInternal(polygons, steinerPoints, coordUnit, meshSettings, triangulation, workDir);
        case MesherType::GMSH:
            return GenerateMeshGmsh(polygons, steinerPoints, coordUnit, meshSettings, triangulation, workDir);
        default:
            NS_TRACE("Unknown mesher type, using internal mesher");
            return GenerateMeshInternal(polygons, steinerPoints, coordUnit, meshSettings, triangulation, workDir);
    }
}

} // namespace nano::heat::model::utils