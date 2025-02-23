#pragma once
#include "TestPackage.hpp"

#include <nano/db>
#include "simulation/NSSimulationPrismThermal.h"
#include "model/NSModel.h"

using namespace boost::unit_test;

void t_prism_thermal_simulation_simple()
{
    using namespace nano;
    using namespace nano::package;
    nano::SetCurrentDir(generic::fs::DirName(__FILE__).string() + "/data/package/simple");
    Database::Create("simple");
    auto pkg = nano::Create<Package>("simple");
    BOOST_CHECK(pkg);

    detail::SetupMaterials(pkg);

    CoordUnit coordUnit(CoordUnit::Unit::Millimeter);
    pkg->SetCoordUnit(coordUnit);

    //layers
    auto matCu = pkg->GetMaterialLib()->FindMaterial("Cu"); BOOST_CHECK(matCu);
    auto topLayer = pkg->AddStackupLayer(nano::Create<StackupLayer>("Top", LayerType::CONDUCTING, 0, 0.3, matCu, matCu));
    
    auto topCell = nano::Create<CircuitCell>("Top", pkg);
    auto layout = topCell->SetLayout(nano::Create<Layout>(CId<CircuitCell>(topCell)));
    pkg->AddCell(topCell);

    auto boundary = nano::Create<ShapeRect>(coordUnit, FCoord2D(0, 0), FCoord2D(10, 10));
    layout->SetBoundary(boundary);

    auto net = nano::Create<Net>("Net", layout);
    layout->AddNet(net);

    auto wire = nano::Create<RoutingWire>(net, topLayer, boundary);
    layout->AddConnObj(wire);

    auto fpCell = nano::Create<FootprintCell>("Comp", pkg);
    pkg->AddCell(fpCell);

    auto fpBoundary = nano::Create<ShapeRect>(coordUnit, FCoord2D(0, 0), FCoord2D(5, 5));
    fpCell->SetBoundary(fpBoundary);
    fpCell->SetComponentType(ComponentType::IC);
    fpCell->SetMaterial(matCu);
    fpCell->SetHeight(0.5);

    auto footprint = nano::Create<Footprint>("Top", fpCell, FootprintLocation::BOT);
    fpCell->AddFootprint(footprint);
    footprint->SetBoundary(fpBoundary);
    footprint->SetSolderBallBumpThickness(0);
    footprint->SetSolderFillingMaterial(matCu);
    footprint->SetSolderMaterial(matCu);

    auto comp = nano::Create<Component>("Comp", fpCell, layout);
    auto compLayer = nano::Create<ComponentLayer>("CompLayer", comp, footprint);
    compLayer->SetConnectedLayer(topLayer);
    comp->AddComponentLayer(compLayer);
    layout->AddComponent(comp);

    //power
    auto powerLut = nano::Create<LookupTable1D>(
        Vec<Float>{TempUnit(25).inKelvins(), TempUnit(125).inKelvins(), TempUnit(150).inKelvins()}, Vec<Float>{20.4, 21.7, 21.8});
    auto lossPower = nano::Create<power::LossPower>("power", ScenarioId(0), powerLut);
    comp->Bind<power::LossPower>(lossPower);

    using namespace nano::heat;
    PrismThermalModelExtractionSettings settings;
    auto & meshSettings = settings.meshSettings;
    meshSettings.minAlpha = 15;
    meshSettings.minLen = 0.01;
    meshSettings.maxLen = 10.0;
    meshSettings.tolerance = 0;
    meshSettings.maxIter = 0;
    meshSettings.dumpMeshFile = true;

    auto & bcSettings = settings.bcSettings;
    bcSettings.SetBotUniformBC(ThermalBoundaryCondition::Type::HTC, 100);

    auto model = model::CreatePrismThermalModel(layout, settings);
    BOOST_CHECK(model);

    nano::thread::SetThreads(1);
    PrismThermalSimulationSetup setup;
    setup.envTemperature = TempUnit(25, TempUnit::Unit::Celsius);
    setup.monitors = {FCoord3D(2.5, 2.5, 0.1)};
    heat::simulation::PrismThermalSimulation simulation(model.get(), setup);

    Vec<Float> temperature;
    auto range = simulation.RunStatic(temperature);
    std::cout << "temperature range: " << range[0] << ", " << range[1] << std::endl;
    Database::Shutdown();
}

void t_prism_thermal_simulation_wolfspeed()
{
    using namespace nano;
    using namespace nano::heat;
    using namespace nano::package;
    auto filename = generic::fs::DirName(__FILE__).string() + "/data/archive/CAS300M12BM2.nano/database.bin";
    auto res = Database::Load(filename, ArchiveFormat::BIN);
    BOOST_CHECK(res);

    unsigned int version{0};
    heat::model::PrismThermalModel model;
    filename = std::string(nano::CurrentDir()) + "/model.prism.thermal.bin";
    res = nano::Load(model, version, filename, ArchiveFormat::BIN);
    BOOST_CHECK(res);
    BOOST_CHECK(model.GetLayout());

    auto getDieMonitors = [&] {
        Vec<FCoord3D> monitors;
        auto layout = model.GetLayout();
        Float elevation{0}, thickness{0};
        package::utils::LayoutRetriever retriever(layout);
        std::vector<std::string> cellInsts{"TopBridge1", "TopBridge2", "BotBridge1", "BotBridge2"};
        std::vector<std::string> components{"Die1", "Die2", "Die3"};
        for (const auto & cellInst : cellInsts) {
            for (const auto & comp : components) {
                auto name = cellInst + HierObj::GetHierSep().data() + comp;
                auto cp = layout->FindComponent(name);
                NS_ASSERT(cp);
                retriever.GetComponentHeightThickness(cp, elevation, thickness);
                auto bonds = cp->GetBoundary(); { NS_ASSERT(bonds); }
                auto ct = generic::geometry::Extent(bonds->GetOutline()).Center();
                auto loc = layout->GetCoordUnit().toUnit(ct);
                monitors.emplace_back(loc[0], loc[1], elevation - 0.1 * thickness);
            }
        }
        return monitors;
    };

    // nano::thread::SetThreads(1);//for debug
    PrismThermalSimulationSetup setup;
    setup.envTemperature = TempUnit(25, TempUnit::Unit::Celsius);
    setup.monitors = getDieMonitors();
    heat::simulation::PrismThermalSimulation simulation(&model, setup);

    Vec<Float> temperature;
    auto range = simulation.RunStatic(temperature);
    std::cout << "temperature range: " << range[0] << ", " << range[1] << std::endl;
    Database::Shutdown();
}

void t_prism_stackup_thermal_simulation_wolfspeed()
{
    using namespace nano;
    using namespace nano::heat;
    using namespace nano::package;
    auto filename = generic::fs::DirName(__FILE__).string() + "/data/archive/CAS300M12BM2.nano/database.bin";
    auto res = Database::Load(filename, ArchiveFormat::BIN);
    BOOST_CHECK(res);

    unsigned int version{0};
    heat::model::PrismStackupThermalModel model;
    filename = std::string(nano::CurrentDir()) + "/model.prism_stackup.thermal.bin";
    res = nano::Load(model, version, filename, ArchiveFormat::BIN);
    BOOST_CHECK(res);
    BOOST_CHECK(model.GetLayout());

    auto getDieMonitors = [&] {
        Vec<FCoord3D> monitors;
        auto layout = model.GetLayout();
        Float elevation{0}, thickness{0};
        package::utils::LayoutRetriever retriever(layout);
        std::vector<std::string> cellInsts{"TopBridge1", "TopBridge2", "BotBridge1", "BotBridge2"};
        std::vector<std::string> components{"Die1", "Die2", "Die3"};
        for (const auto & cellInst : cellInsts) {
            for (const auto & comp : components) {
                auto name = cellInst + HierObj::GetHierSep().data() + comp;
                auto cp = layout->FindComponent(name);
                NS_ASSERT(cp);
                retriever.GetComponentHeightThickness(cp, elevation, thickness);
                auto bonds = cp->GetBoundary(); { NS_ASSERT(bonds); }
                auto ct = generic::geometry::Extent(bonds->GetOutline()).Center();
                auto loc = layout->GetCoordUnit().toUnit(ct);
                monitors.emplace_back(loc[0], loc[1], elevation - 0.1 * thickness);
            }
        }
        return monitors;
    };

    // nano::thread::SetThreads(1);//for debug
    PrismThermalSimulationSetup setup;
    setup.envTemperature = TempUnit(25, TempUnit::Unit::Celsius);
    setup.monitors = getDieMonitors();
    heat::simulation::PrismStackupThermalSimulation simulation(&model, setup);

    Vec<Float> temperature;
    auto range = simulation.RunStatic(temperature);
    std::cout << "temperature range: " << range[0] << ", " << range[1] << std::endl;
    Database::Shutdown();
}

void t_prism_thermal_simulation2()
{
    using namespace nano;
    using namespace nano::heat;
    using namespace nano::package;
    nano::SetCurrentDir(generic::fs::DirName(__FILE__).string() + "/data/package/test");

    auto filename = std::string(nano::CurrentDir()) + "/database.bin";
    auto res = Database::Load(filename, ArchiveFormat::BIN);
    BOOST_CHECK(res);

    unsigned int version{0};
    heat::model::PrismThermalModel model;
    auto modelFile = std::string(nano::CurrentDir()) + "/model.prism.thermal.bin";
    res = nano::Load(model, version, modelFile, ArchiveFormat::BIN);
    BOOST_CHECK(res);
    BOOST_CHECK(model.GetLayout());

    auto getDieMonitors = [&] {
        Vec<FCoord3D> monitors;
        return monitors;
    };

    // nano::thread::SetThreads(1);//for debug
    PrismThermalSimulationSetup setup;
    setup.monitors = getDieMonitors();
    heat::simulation::PrismThermalSimulation simulation(&model, setup);

    Vec<Float> temperature;
    auto range = simulation.RunStatic(temperature);
    std::cout << "temperature range: " << range[0] << ", " << range[1] << std::endl;
    Database::Shutdown();
}

test_suite * create_nano_heat_simulation_test_suite()
{
    test_suite * simulation_suite = BOOST_TEST_SUITE("s_heat_simulation_test");
    //
    simulation_suite->add(BOOST_TEST_CASE(&t_prism_thermal_simulation_simple));
    simulation_suite->add(BOOST_TEST_CASE(&t_prism_thermal_simulation_wolfspeed));
    // simulation_suite->add(BOOST_TEST_CASE(&t_prism_stackup_thermal_simulation_wolfspeed));
    // simulation_suite->add(BOOST_TEST_CASE(&t_prism_thermal_simulation2));
    //
    return simulation_suite;
}