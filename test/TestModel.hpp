#pragma once
#include "TestPackage.hpp"
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
    
    LayerStackupModelExtractionSettings settings;
    // settings.addCircleCenterAsSteinerPoint = true;
    auto model = model::CreateLayerStackupModel(layout, settings);
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
    meshSettings.minAlpha = 15;
    meshSettings.minLen = 0.01;
    meshSettings.maxLen = 3.00;
    meshSettings.tolerance = 1e-2;
    meshSettings.maxIter = 1e4;
    meshSettings.dumpMeshFile = true;

    Float htc = 5000;
    BoundaryCondtionSettings bcSettings;
    bcSettings.AddBlockBC(Orientation::Top, FBox2D({-29.35,   4.70}, {-20.35,   8.70}), ThermalBoundaryCondition::Type::HTC, htc);
    bcSettings.AddBlockBC(Orientation::Top, FBox2D({-29.35, - 8.70}, {-20.35, - 4.70}), ThermalBoundaryCondition::Type::HTC, htc);
    bcSettings.AddBlockBC(Orientation::Top, FBox2D({  2.75,  11.50}, {  9.75,  17.00}), ThermalBoundaryCondition::Type::HTC, htc);
    bcSettings.AddBlockBC(Orientation::Top, FBox2D({  2.75, -17.00}, {  9.75, -11.50}), ThermalBoundaryCondition::Type::HTC, htc);
    bcSettings.AddBlockBC(Orientation::Top, FBox2D({- 7.75,  11.50}, {- 2.55,  17.00}), ThermalBoundaryCondition::Type::HTC, htc);
    bcSettings.AddBlockBC(Orientation::Top, FBox2D({- 7.75, -17.00}, {- 2.55, -11.50}), ThermalBoundaryCondition::Type::HTC, htc);
    // bcSettings.SetBotUniformBC(ThermalBoundaryCondition::Type::TEMPERATURE, TempUnit::Celsisu2Kelvin(25));
    auto model = model::CreatePrismThermalModel(layout, &stackupModel, meshSettings, bcSettings);
    BOOST_CHECK(model);

    auto vtkFile = std::string(nano::CurrentDir()) + "/prism.vtk";
    model->WriteVTK<Float>(vtkFile);
    // BOOST_CHECK(model->TotalElements() == 125449);
    // BOOST_CHECK(model->TotalPrismElements() == 123337);
    // BOOST_CHECK(model->TotalLineElements() == 2112);

    auto prismThermalModelFile = std::string(nano::CurrentDir()) + "/model.prism.thermal.bin";
    nano::Save(*model, CURRENT_VERSION.toInt(), prismThermalModelFile, ArchiveFormat::BIN);

    Database::Shutdown();
}

void t_build_prism_thermal_model2()
{
    using namespace nano::heat;
    using namespace nano::package;

    Database::Create("kicad_test1");

    auto filename = generic::fs::DirName(__FILE__).string() + "/data/package/test.kicad_pcb";
    auto package = extension::CreateFromKiCad(filename.c_str());
    auto layout = package->GetTop()->GetCell()->GetLayout().ConstCast();
    NS_ASSERT(layout);

    detail::SetupMaterials(package);
    auto matCu = package->FindMaterial("Cu"); BOOST_CHECK(matCu);
    auto matAir = package->FindMaterial("Air"); BOOST_CHECK(matAir);
    auto matFR4 = package->FindMaterial("FR4"); BOOST_CHECK(matFR4);
    auto matSiC = package->FindMaterial("SiC"); BOOST_CHECK(matSiC);
    auto matSolder = package->FindMaterial("Solder"); BOOST_CHECK(matSolder);

    auto layerIter = package->GetStackupLayerIter();
    while (auto stackupLayer = layerIter.Next()) {
        stackupLayer->SetConductingMaterial(matCu);
        stackupLayer->SetDielectricMaterial(matFR4);
    }

    auto fpCellIter = package->GetFootprintCellIter();
    while (auto fpCell = fpCellIter.Next()) {
        fpCell->SetMaterial(matSiC);
        if (0 == fpCell->GetHeight())
            fpCell->SetHeight(2);
    }

    auto compIter = layout->GetComponentIter();
    while (auto comp = compIter.Next()) {
        auto mountingLayer = comp->GetAssemblyLayer();
        NS_ASSERT(mountingLayer);
        auto footprint = mountingLayer->GetFootprint().ConstCast();
        footprint->SetSolderMaterial(matSolder);
        footprint->SetSolderFillingMaterial(matSolder);//wbtest todo;
        footprint->SetSolderBallBumpThickness(0.1);
    }

    PrismThermalModelExtractionSettings settings;
    settings.layerSettings.addCircleCenterAsSteinerPoint = true;

    auto & meshSettings = settings.meshSettings;
    meshSettings.minAlpha = 15;
    meshSettings.minLen = 0.01;
    meshSettings.maxLen = 3.00;
    meshSettings.tolerance = 1e-2;
    meshSettings.maxIter = 1e4;
    meshSettings.dumpMeshFile = true;

    auto & bcSettings = settings.bcSettings;
    bcSettings.SetTopUniformBC(ThermalBoundaryCondition::Type::HTC, 100);
    bcSettings.SetBotUniformBC(ThermalBoundaryCondition::Type::HTC, 100);

    auto model = model::CreatePrismThermalModel(layout, settings);
    BOOST_CHECK(model);
}

test_suite * create_nano_heat_model_test_suite()
{
    test_suite * model_suite = BOOST_TEST_SUITE("s_heat_model_test");
    //
    model_suite->add(BOOST_TEST_CASE(&t_build_layer_stackup_model));
    model_suite->add(BOOST_TEST_CASE(&t_build_prism_thermal_model));

    // model_suite->add(BOOST_TEST_CASE(&t_build_prism_thermal_model2));
    //
    return model_suite;
}
