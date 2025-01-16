#pragma once
#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>
#include <boost/test/test_tools.hpp>
#include <nano/db>

#include "generic/tools/FileSystem.hpp"
#include "generic/math/MathUtility.hpp"

#include "model/NSModel.h"

using namespace nano;
using namespace boost::unit_test;

void t_build_layer_stackup_model()
{
    using namespace nano::heat;
    using namespace nano::package;
    auto filename = generic::fs::DirName(__FILE__).string() + "/data/archive/CAS300M12BM2.xml";
    auto res = Database::Load(filename, ArchiveFormat::XML);
    BOOST_CHECK(res);

    auto pred = [](const auto & p) { return p.GetName() == "CAS300M12BM2"; };
    IdVec<Package, NameLut> packages;
    nano::FindAll<Package>(pred, packages);
    BOOST_CHECK(packages.size() == 1);
    auto pkg = nano::FindOne<Package>(pred);
    BOOST_CHECK(pkg);
    BOOST_CHECK(pkg == packages.Lookup<lut::Name>("CAS300M12BM2"));

    auto layout = pkg->GetTop()->GetFlattenedLayout();
    auto model = model::CreateLayerStackupModel(layout, LayerStackupModelBuildSettings());
    BOOST_CHECK(model);

    Database::Shutdown();
}

test_suite * create_nano_heat_model_test_suite()
{
    test_suite * model_suite = BOOST_TEST_SUITE("s_heat_model_test");
    //
    model_suite->add(BOOST_TEST_CASE(&t_build_layer_stackup_model));
    //
    return model_suite;
}
