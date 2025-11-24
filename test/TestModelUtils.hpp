#pragma once
#include <nano/db>

#include "generic/math/MathUtility.hpp"
#include "generic/tools/FileSystem.hpp"

#include "model/utils/NSModelPrismMeshGmshUtils.h"

using namespace boost::unit_test;

void t_write_gmsh_geo_file()
{
    using namespace nano;
    using namespace generic;
    using namespace nano::heat::model::utils;
    Polygon outline{{{0, 0}, {5000, 0}, {5000, 5000}, {0, 5000}}};
    Polygon shape1{{{1000, 2000}, {4000, 2000}, {4000, 3000}, {1000, 3000}}};
    Polygon shape2{{{2000, 1000}, {3000, 1000}, {3000, 4000}, {2000, 4000}}};
    Polygons shapes{std::move(shape1), std::move(shape2)};
    Points steinerPoints{{2500, 2500}};
    GMshUtils::MeshSettings meshSettings;
    meshSettings.minAlpha = math::Rad(15.0);
    meshSettings.minLen = 1000;
    meshSettings.maxLen = 5000;
    meshSettings.workDir = fs::DirName(__FILE__).string() + "/data/model/gmsh";
    bool res = GMshUtils::WriteGeoFile(outline, shapes, steinerPoints, meshSettings);
    BOOST_CHECK(res);
    BOOST_CHECK(fs::FileExists(meshSettings.workDir + "/mesh.geo"));
}

void t_read_gmsh_msh_file()
{
    using namespace nano;
    using namespace generic;
    using namespace nano::heat::model::utils;
    GMshUtils::MeshSettings meshSettings;
    meshSettings.workDir = fs::DirName(__FILE__).string() + "/data/model/gmsh";
    std::string mshFile = meshSettings.workDir + "/mesh.msh";
    PrismTemplate triangulation;
    bool res = GMshUtils::ReadMshFile(mshFile, triangulation);
    BOOST_CHECK(res);
    std::cout << "Points: " << triangulation.points.size() << ", Triangles: " << triangulation.triangles.size() << std::endl;
    BOOST_CHECK(triangulation.points.size() == 59);
    BOOST_CHECK(triangulation.triangles.size() == 96);
}

test_suite * create_nano_heat_model_utils_test_suite()
{
    test_suite * model_utils_suite = BOOST_TEST_SUITE("s_heat_model_utils_test");
    //
    model_utils_suite->add(BOOST_TEST_CASE(&t_write_gmsh_geo_file));
    model_utils_suite->add(BOOST_TEST_CASE(&t_read_gmsh_msh_file));
    //
    return model_utils_suite;
}

