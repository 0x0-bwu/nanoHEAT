#pragma once
#include "TestCommon.hpp"
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
    auto filename = generic::fs::DirName(__FILE__).string() + "/data/archive/CAS300M12BM2.nano/database.bin";
    auto res = Database::Load(filename, ArchiveFormat::BIN);
    BOOST_CHECK(res);

    auto saveChecksum = nano::test::variables[BOOST_HANA_STRING("package_checksum")];
    auto loadChecksum = Database::Current().Checksum();
    BOOST_CHECK(saveChecksum == loadChecksum);

    auto pred = [](const auto & p) { return p.GetName() == "CAS300M12BM2"; };
    IdVec<Package, NameLut> packages;
    nano::FindAll<Package>(pred, packages);
    BOOST_CHECK(packages.size() == 1);
    auto pkg = nano::Find<Package>(pred);
    BOOST_CHECK(pkg);
    BOOST_CHECK(pkg == packages.Lookup<lut::Name>("CAS300M12BM2"));

    auto layout = pkg->GetTop()->GetFlattenedLayout();
    BOOST_CHECK(layout);
    
    auto model = model::CreateLayerStackupModel(layout, LayerStackupModelExtractionSettings());
    BOOST_CHECK(model);

    auto modelFile = std::string(nano::CurrentDir()) + "/model.stackup.bin";
    model->Save(modelFile, ArchiveFormat::BIN);
    Database::Shutdown();
}

void t_build_prism_thermal_model()
{
    using namespace nano::heat;
    using namespace nano::package;
    auto filename = generic::fs::DirName(__FILE__).string() + "/data/archive/CAS300M12BM2.nano/database.bin";
    auto res = Database::Load(filename, ArchiveFormat::BIN);
    BOOST_CHECK(res);

    auto saveChecksum = nano::test::variables[BOOST_HANA_STRING("package_checksum")];
    auto loadChecksum = Database::Current().Checksum();
    BOOST_CHECK(saveChecksum == loadChecksum);

    auto pkg = nano::Find<Package>([](const auto & p) { return p.GetName() == "CAS300M12BM2"; });
    BOOST_CHECK(pkg);

    auto layout = pkg->GetTop()->GetFlattenedLayout();
    BOOST_CHECK(layout);

    model::LayerStackupModel stackupModel;
    auto stackupModelFile = std::string(nano::CurrentDir()) + "/model.stackup.bin";
    res = stackupModel.Load(stackupModelFile, ArchiveFormat::BIN);
    BOOST_CHECK(res);
    
    PrismMeshSettings meshSettings;
    meshSettings.minAlpha = 20;
    meshSettings.minLen = 1e-3;
    meshSettings.maxLen = 1;
    meshSettings.tolerance = 0;
    meshSettings.maxIter = 1e5;
    meshSettings.dumpMeshFile = true;

    Float htc = 5000;
    BoundaryCondtionSettings bcSettings;
    bcSettings.AddBlockBC(Orientation::Top, FBox2D({-29.35,   4.70}, {-20.35,   8.70}), ThermalBoundaryCondition::Type::HTC, htc);
    bcSettings.AddBlockBC(Orientation::Top, FBox2D({-29.35, - 8.70}, {-20.35, - 4.70}), ThermalBoundaryCondition::Type::HTC, htc);
    bcSettings.AddBlockBC(Orientation::Top, FBox2D({  2.75,  11.50}, {  9.75,  17.00}), ThermalBoundaryCondition::Type::HTC, htc);
    bcSettings.AddBlockBC(Orientation::Top, FBox2D({  2.75, -17.00}, {  9.75, -11.50}), ThermalBoundaryCondition::Type::HTC, htc);
    bcSettings.AddBlockBC(Orientation::Top, FBox2D({- 7.75,  11.50}, {- 2.55,  17.00}), ThermalBoundaryCondition::Type::HTC, htc);
    bcSettings.AddBlockBC(Orientation::Top, FBox2D({- 7.75, -17.00}, {- 2.55, -11.50}), ThermalBoundaryCondition::Type::HTC, htc);
    auto model = model::CreatePrismThermalModel(layout, &stackupModel, meshSettings, bcSettings);
    BOOST_CHECK(model);

    auto vtkFile = std::string(nano::CurrentDir()) + "/prism.vtk";
    model->WriteVTK<Float>(vtkFile);
    Database::Shutdown();
}

test_suite * create_nano_heat_model_test_suite()
{
    test_suite * model_suite = BOOST_TEST_SUITE("s_heat_model_test");
    //
    model_suite->add(BOOST_TEST_CASE(&t_build_layer_stackup_model));
    model_suite->add(BOOST_TEST_CASE(&t_build_prism_thermal_model));
    //
    return model_suite;
}
