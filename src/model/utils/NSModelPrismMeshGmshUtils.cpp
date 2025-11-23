#include "NSModelPrismMeshGmshUtils.h"

namespace nano::heat::model::utils {

using namespace generic;
using namespace generic::fmt;
using namespace generic::geometry;

bool GMshUtils::WriteGeoFile(const Polygons & polygons, const Points & steinerPoints, const MeshSettings & meshSettings)
{
    std::ofstream out(std::string(meshSettings.workDir) + "/mesh.geo");
    if (not out.is_open()) {
        NS_TRACE("Failed to open .geo file for writing: %1%", meshSettings.workDir + "/mesh.geo");
        return false;
    }

    out << "General.NumThreads = 0;\n"; // Auto-detect CPU cores
    out << "// Mesh size settings\n";
    out << "Mesh.CharacteristicLengthMin = " << meshSettings.minLen << ";\n";
    out << "Mesh.CharacteristicLengthMax = " << meshSettings.maxLen << ";\n";
    out << "Mesh.Algorithm = 6; // Frontal-Delaunay for 2D\n";
    out << "Mesh.Optimize = 1; // Optimize mesh\n";
    out << "Mesh.OptimizeNetgen = 1;\n";
    out << "\n";

    Index ptIdx{0};
    for (const auto & polygon : polygons) {
        for (const auto & point : polygon) {
            
        }
    }
    auto box = Extent(polygons);
    box.Scale(1.1);

    //TODO


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