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

using namespace generic;
using namespace generic::fmt;
using namespace generic::geometry;
using Edges = mesh2d::Edges;
using Points = mesh2d::Points;
using Segments = mesh2d::Segments;
using PrismTemplate = tri::Triangulation<NCoord2D>;

/**
 * @brief Write Gmsh .geo file from points and edges
 * @param points Point coordinates
 * @param edges Edge connectivity
 * @param minAlpha Minimum angle constraint
 * @param minLen Minimum element length
 * @param maxLen Maximum element length
 * @param filename Output .geo file path
 * @return true if successful
 */
bool WriteGmshGeoFile(const Points & points, const Edges & edges, const Float minAlpha, 
                      const NCoord minLen, const NCoord maxLen, const std::string & filename);

/**
 * @brief Read Gmsh .msh file and populate triangulation
 * @param mshFilePath Input .msh file path
 * @param triangulation Output triangulation structure
 * @return true if successful
 */
bool ReadGmshMshFile(const std::string & mshFilePath, PrismTemplate & triangulation);

/**
 * @brief Preprocess mesh by extracting segments and merging close points
 * @param polygons Input polygons
 * @param steinerPoints Additional Steiner points
 * @param coordUnit Coordinate unit conversion
 * @param meshSettings Mesh generation settings
 * @param maxLen Maximum edge length
 * @param tolerance Tolerance for merging close points
 * @param edges Output edge list
 * @param points Output point list
 * @return true if successful
 */
bool MeshPreprocess(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints,
                    const CoordUnit & coordUnit, const PrismMeshSettings & meshSettings, 
                    const NCoord maxLen, const NCoord tolerance, Edges & edges, Points & points);

/**
 * @brief Generate mesh using internal triangulation algorithm
 * @param points Input points
 * @param edges Input edges
 * @param triangulation Output triangulation
 * @param minAlpha Minimum angle constraint
 * @param minLen Minimum element length
 * @param maxLen Maximum element length
 * @param maxIter Maximum refinement iterations
 * @return true if successful
 */
bool GenerateMeshInternal(const Points & points, const Edges & edges, PrismTemplate & triangulation,
                          const Float minAlpha, const NCoord minLen, const NCoord maxLen, const Index maxIter);

/**
 * @brief Generate mesh using Gmsh external mesher
 * @param points Input points
 * @param edges Input edges
 * @param triangulation Output triangulation
 * @param minAlpha Minimum angle constraint
 * @param minLen Minimum element length
 * @param maxLen Maximum element length
 * @param meshSettings Mesh generation settings
 * @param workDir Working directory for temporary files
 * @return true if successful
 */
bool GenerateMeshGmsh(const Points & points, const Edges & edges, PrismTemplate & triangulation,
                      const Float minAlpha, const NCoord minLen, const NCoord maxLen,
                      const PrismMeshSettings & meshSettings, std::string_view workDir);

/**
 * @brief Postprocess mesh: dump files and report quality metrics
 * @param triangulation Input triangulation
 * @param meshSettings Mesh generation settings
 * @param workDir Working directory for output files
 * @return true if successful
 */
bool MeshPostprocess(const PrismTemplate & triangulation, const PrismMeshSettings & meshSettings, 
                     std::string_view workDir);

/**
 * @brief Main entry point for mesh generation
 * @param polygons Input polygons
 * @param steinerPoints Additional Steiner points
 * @param coordUnit Coordinate unit conversion
 * @param meshSettings Mesh generation settings
 * @param triangulation Output triangulation
 * @param workDir Working directory
 * @return true if successful
 */
bool GenerateMesh(const Vec<NPolygon> & polygons, const Vec<NCoord2D> & steinerPoints, 
                  const CoordUnit & coordUnit, const PrismMeshSettings & meshSettings,
                  PrismTemplate & triangulation, std::string_view workDir);

} // namespace nano::heat::model::utils