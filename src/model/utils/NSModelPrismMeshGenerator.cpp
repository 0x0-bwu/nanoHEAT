#include "NSModelPrismMeshGenerator.h"

#include <filesystem>
#include <sstream>
#include <fstream>
#include <cstdlib>

namespace nano::heat::model::utils {

bool WriteGmshGeoFile(const Points & points, const Edges & edges, const Float minAlpha, 
                      const NCoord minLen, const NCoord maxLen, const std::string & filename)
{
    std::ofstream out(filename);
    if (not out.is_open()) {
        NS_TRACE("Failed to open .geo file for writing: %1%", filename);
        return false;
    }

    // Write mesh size parameters
    out << "General.NumThreads = 0;\n"; // Auto-detect CPU cores
    out << "// Mesh size settings\n";
    out << "Mesh.CharacteristicLengthMin = " << minLen << ";\n";
    out << "Mesh.CharacteristicLengthMax = " << maxLen << ";\n";
    out << "Mesh.Algorithm = 6; // Frontal-Delaunay for 2D\n";
    out << "Mesh.Optimize = 1; // Optimize mesh\n";
    out << "Mesh.OptimizeNetgen = 1;\n";
    out << "\n";

    // Write points
    size_t index = 0;
    for (const auto & p : points) {
        out << Fmt2Str("Point(%1%) = {%2%, %3%, 0, 1};\n", ++index, p[0], p[1]);
    }

    // Create bounding box points
    auto corner = index + 1;
    auto box = Extent(points);
    box.Scale(1.1);
    out << Fmt2Str("Point(%1%) = {%2%, %3%, 0, 1};\n", corner + 0, box[0][0], box[0][1]);
    out << Fmt2Str("Point(%1%) = {%2%, %3%, 0, 1};\n", corner + 1, box[1][0], box[0][1]);
    out << Fmt2Str("Point(%1%) = {%2%, %3%, 0, 1};\n", corner + 2, box[1][0], box[1][1]);
    out << Fmt2Str("Point(%1%) = {%2%, %3%, 0, 1};\n", corner + 3, box[0][0], box[1][1]);
    out << "\n";

    // Write edges
    index = 0;
    for (const auto & e : edges) {
        out << Fmt2Str("Line(%1%) = {%2%, %3%};\n", ++index, e.v1() + 1, e.v2() + 1);
    }
    
    // Write bounding box edges
    auto line = index + 1;
    out << Fmt2Str("Line(%1%) = {%2%, %3%};\n", ++index, corner + 0, corner + 1);
    out << Fmt2Str("Line(%1%) = {%2%, %3%};\n", ++index, corner + 1, corner + 2);
    out << Fmt2Str("Line(%1%) = {%2%, %3%};\n", ++index, corner + 2, corner + 3);
    out << Fmt2Str("Line(%1%) = {%2%, %3%};\n", ++index, corner + 3, corner + 0);
    out << "\n";

    // Create surface
    out << Fmt2Str("Line Loop(1) = {%1%, %2%, %3%, %4%};\n", line + 0, line + 1, line + 2, line + 3);
    out << "Plane Surface(1) = {1};\n\n";

    // Embed constraint lines in surface
    for (size_t i = 0; i < edges.size(); ++i) {
        out << Fmt2Str("Line{%1%} In Surface{1};\n", i + 1);
    }
    
    out << "\n";
    out << "Physical Surface(\"Domain\") = {1};\n";
    out.close(); 
    
    return true;
}

bool ReadGmshMshFile(const std::string & mshFilePath, PrismTemplate & triangulation)
{    
    std::ifstream in(mshFilePath);
    if (not in.is_open()) {
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
    HashMap<size_t, size_t> gmshIdToPointIdx;

    while (std::getline(in, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (line.empty()) continue;
        
        // Parse $Nodes section
        if (line == "$Nodes") {
            inNodes = true;
            std::getline(in, line);
            numNodes = std::stoull(line);
            
            for (size_t i = 0; i < numNodes; ++i) {
                std::getline(in, line);
                std::istringstream iss(line);
                size_t nodeId;
                NCoord x, y, z;
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
            std::getline(in, line);
            numElements = std::stoull(line);
            
            for (size_t i = 0; i < numElements; ++i) {
                std::getline(in, line);
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

    in.close();

    // Build triangle neighbor relationships
    using EdgeToTriMap = generic::topology::UndirectedIndexEdgeMap<size_t>;
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
    
    NS_TRACE("Read mesh from Gmsh: %1% nodes, %2% triangles", 
             triangulation.points.size(), triangulation.triangles.size());
    
    return true;
}

bool MeshPreprocess(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints,
                    const CoordUnit & coordUnit, const PrismMeshSettings & meshSettings, 
                    const NCoord maxLen, const NCoord tolerance, Edges & edges, Points & points)
{
    Segments segments, intersections;
    auto hull = ConvexHull(polygons);
    mesh2d::ExtractSegments(hull, segments);
    mesh2d::ExtractSegments(polygons, segments);
    mesh2d::ExtractIntersections(segments, intersections);
    mesh2d::ExtractTopology(intersections, points, edges);
    points.reserve(points.size() + steinerPoints.size());
    points.insert(points.end(), steinerPoints.begin(), steinerPoints.end());
    if (meshSettings.preSplitEdge)
        mesh2d::SplitOverlengthEdges(points, edges, maxLen);
    if (meshSettings.addBalancedPoints)
        mesh2d::AddPointsFromBalancedQuadTree(ConvexHull(polygons), points, 10, nano::thread::Threads());
    mesh2d::MergeClosePointsAndRemapEdge(points, edges, tolerance);
    return true;
    return true;
}

bool GenerateMeshInternal(const Points & points, const Edges & edges, PrismTemplate & triangulation,
                          const Float minAlpha, const NCoord minLen, const NCoord maxLen, const Index maxIter)
{
    mesh2d::TriangulatePointsAndEdges(points, edges, triangulation);
    mesh2d::TriangulationRefinement(triangulation, minAlpha, minLen, maxLen, maxIter);
    return true;
}

bool GenerateMeshGmsh(const Points & points, const Edges & edges, PrismTemplate & triangulation,
                      const Float minAlpha, const NCoord minLen, const NCoord maxLen,
                      const PrismMeshSettings & meshSettings, std::string_view workDir)
{
    namespace fs = std::filesystem;
    fs::create_directories(workDir);
    
    std::string iFilename = std::string(workDir) + "/mesh.geo";
    std::string oFilename = std::string(workDir) + "/mesh.msh";
    
    if (not WriteGmshGeoFile(points, edges, minAlpha, minLen, maxLen, iFilename)) {
        return false;
    }
    
    // Verify that the .geo file was successfully created
    if (!fs::exists(iFilename)) {
        NS_TRACE("Failed to create .geo file: %1%", iFilename);
        return false;
    }
    
    std::string cmd = meshSettings.mesher + " -2 -format msh2 -o " + oFilename + " " + iFilename + " -v 0";
    NS_TRACE("Calling Gmsh: %1%", cmd);
    
    int result = std::system(cmd.c_str());
    
    // Detailed error information
    if (result != 0) {
        int exitCode = WEXITSTATUS(result);
        NS_TRACE("Gmsh failed with return code: %1% (exit code: %2%)", result, exitCode);
        
        if (exitCode == 1) {
            NS_TRACE("Common causes:");
            NS_TRACE("  - Gmsh not found in PATH");
            NS_TRACE("  - Invalid .geo file syntax");
            NS_TRACE("  - Permission denied for output directory");
        }
        
        // Try running without silent mode to get error information
        std::string verboseCmd = meshSettings.mesher + " -2 -format msh2 -o " + oFilename + " " + iFilename;
        NS_TRACE("Try running manually: %1%", verboseCmd);
        
        return false;
    }
    
    if (not ReadGmshMshFile(oFilename, triangulation)) {
        NS_TRACE("Failed to read Gmsh .msh file");
        return false;
    }
    
    return true;
}

bool MeshPostprocess(const PrismTemplate & triangulation, const PrismMeshSettings & meshSettings, 
                     std::string_view workDir)
{
    if (meshSettings.dumpMeshFile) {
        NS_TRACE("Writing mesh file to %1%, total triangles: %2%", workDir, triangulation.triangles.size());
        GeometryIO::WritePNG(std::string(workDir) + "/meshOut.png", triangulation, 4096);
    }

    if (meshSettings.reportMeshQuality) {
        tri::TriangleEvaluator<NCoord2D> evaluator(triangulation, {}, {});
        auto results = evaluator.Report();
        NS_TRACE("Mesh quality:");
        NS_TRACE("  Total nodes: %1%, total elements: %2%", results.nodes, results.elements);
        NS_TRACE("  Min angle: %1%, max angle: %2%", results.minAngle, results.maxAngle);
        NS_TRACE("  Min edge length: %1%, max edge length: %2%", results.minEdgeLen, results.maxEdgeLen);
        NS_TRACE("  Angle histogram: [%1%]", fmt::Fmt2Str(results.triAngleHistogram, ","));
        NS_TRACE("  Edge length histogram: [%1%]", fmt::Fmt2Str(results.triEdgeLenHistogram, ","));
    }
    
    return true;
}

bool GenerateMesh(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints, 
                  const CoordUnit & coordUnit, const PrismMeshSettings & meshSettings,
                  PrismTemplate & triangulation, std::string_view workDir)
{
    auto minAlpha = math::Rad(meshSettings.minAlpha);
    auto minLen = coordUnit.toCoord(meshSettings.minLen);
    auto maxLen = coordUnit.toCoord(meshSettings.maxLen);
    auto tolerance = coordUnit.toCoord(meshSettings.tolerance);
    
    if (meshSettings.dumpMeshFile) {
        NS_TRACE("Mesh dir: %1%, minAlpha: %2%, minLen: %3%, maxLen: %4%, tolerance: %5%", 
                 workDir, minAlpha, minLen, maxLen, tolerance);
        GeometryIO::WritePNG(std::string(workDir) + "/meshIn.png", polygons.begin(), polygons.end(), 4096);
        GeometryIO::WriteWKT<NPolygon>(std::string(workDir) + "/meshIn.wkt", polygons.begin(), polygons.end());
    }
    
    Edges edges;
    Points points;
    
    if (not MeshPreprocess(polygons, steinerPoints, coordUnit, meshSettings, 
                           maxLen, tolerance, edges, points)) {
        NS_TRACE("Mesh preprocess failed");
        return false;
    }

    bool success = false;
    if (MesherType::GMSH == meshSettings.mesherType) {
        success = GenerateMeshGmsh(points, edges, triangulation, minAlpha, minLen, maxLen, meshSettings, workDir);
    }
    else {
        success = GenerateMeshInternal(points, edges, triangulation, minAlpha, minLen, maxLen, meshSettings.maxIter);
    }

    if (not success) {
        NS_TRACE("Mesh generation failed");
        return false;
    }

    if (not MeshPostprocess(triangulation, meshSettings, workDir)) {
        NS_TRACE("Mesh postprocess failed");
        return false;
    }
    
    return true;
}

} // namespace nano::heat::model::utils