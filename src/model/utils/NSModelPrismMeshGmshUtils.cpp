#include "NSModelPrismMeshGmshUtils.h"
#include "generic/tools/FileSystem.hpp"
namespace nano::heat::model::utils {

using namespace generic;
using namespace generic::fs;
using namespace generic::fmt;
using namespace generic::geometry;

bool GMshUtils::WriteGeoFile(const Polygon & outline, const Polygons & shapes, const Points & steinerPoints, const MeshSettings & meshSettings)
{
    if (not CreateDir(meshSettings.workDir)) {
        NS_TRACE("Failed to create mesh work directory: %1%", meshSettings.workDir);
        return false;
    }

    std::string filename = meshSettings.workDir + "/mesh.geo";
    std::ofstream out(filename);
    if (not out.is_open()) {
        NS_TRACE("Failed to open .geo file for writing: %1%", filename);
        return false;
    }

    // Write mesh size parameters
    out << "General.NumThreads = 0;\n"; // Auto-detect CPU cores
    if (meshSettings.minLen > 0)
        out << "Mesh.CharacteristicLengthMin = " << meshSettings.minLen << ";\n";
    if (meshSettings.maxLen > 0)
        out << "Mesh.CharacteristicLengthMax = " << meshSettings.maxLen << ";\n";
    out << "Mesh.Algorithm = 6; // Frontal-Delaunay for 2D\n";
    out << "Mesh.Optimize = 1; // Optimize mesh\n";
    out << "Mesh.OptimizeNetgen = 1;\n";
    out << "\n";

    size_t pointId{0};
    out << "// outline points\n";
    for (const auto & p : outline.GetPoints()) {
        out << Fmt2Str("Point(%1%) = {%2%, %3%, 0, 1};\n", ++pointId, p[0], p[1]);
    }

    out << "\n// shape points\n";
    for (const auto & shape : shapes) {
        for (const auto & p : shape.GetPoints()) {
            out << Fmt2Str("Point(%1%) = {%2%, %3%, 0, 1};\n", ++pointId, p[0], p[1]);
        }
    }

    out << "\n// steiner points\n";
    size_t startSteinerId = pointId + 1;
    for (const auto & p : steinerPoints) {
        out << Fmt2Str("Point(%1%) = {%2%, %3%, 0, 1};\n", ++pointId, p[0], p[1]);
    }
    out << "\n";

    Index edgeId{0};
    for (size_t i = 0; i < outline.Size(); ++i) {
        size_t v1 = i + 1;
        size_t v2 = (i + 1) % outline.Size() + 1;
        out << Fmt2Str("Line(%1%) = {%2%, %3%};\n", ++edgeId, v1, v2);
    }
    out << "\n";
    out << Fmt2Str("Line Loop(1) = {");
    for (size_t i = 0; i < outline.Size(); ++i) {
        out << edgeId - outline.Size() + i + 1;
        if (i < outline.Size() - 1) out << ", ";
    }
    out << "};\n";
    out << "Plane Surface(1) = {1};\n";
    out << "Physical Surface(\"domain\") = {1};\n\n";

    out << "// shape edges\n";
    pointId = outline.Size() + 1;
    for (const auto & shape : shapes) {
        const auto & points = shape.GetPoints();
        for (size_t i = 0; i < points.size(); ++i) {
            size_t v1 = pointId + i;
            size_t v2 = pointId + (i + 1) % points.size();
            out << Fmt2Str("Line(%1%) = {%2%, %3%};\n", ++edgeId, v1, v2);
        }
        pointId += points.size();
        out << '\n';
    }

    edgeId = outline.Size();
    for (const auto & shape : shapes) {
        out << "Line{";
        const auto & points = shape.GetPoints();
        for (size_t i = 0; i < points.size(); ++i) {
            out << ++edgeId;
            if (i < points.size() - 1) out << ", ";
        }
        out << "} In Surface{1};\n";
    }
    
    out << "\n// steiner points\n";
    for (size_t i = 0; i < steinerPoints.size(); ++i) {
        out << Fmt2Str("Point{%1%} In Surface{1};\n", startSteinerId + i);
    }
    out.close();
    return true;
}

bool GMshUtils::ReadMshFile(std::string_view filename, PrismTemplate & triangulation)
{    
    std::ifstream in(filename.data());
    if (not in.is_open()) {
        NS_TRACE("Failed to open .msh file for reading: %1%", filename.data());
        return false;
    }
    
    triangulation.points.clear();
    triangulation.vertices.clear();
    triangulation.triangles.clear();
    triangulation.fixedEdges.clear();
    
    std::string line;
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
            continue;
        }
        // Parse $Elements section
        else if (line == "$Elements") {
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
            break;
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


} // namespace nano::heat::model::utils