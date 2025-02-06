#pragma once
#include "TestCommon.hpp"

#include <nano/db>
#include "simulation/NSSimulationPrismThermal.h"

using namespace nano;
using namespace boost::unit_test;

void t_prism_thermal_simulation()
{
    using namespace nano::heat;
    using namespace nano::package;
    auto filename = generic::fs::DirName(__FILE__).string() + "/data/archive/CAS300M12BM2.nano/database.bin";
    auto res = Database::Load(filename, ArchiveFormat::BIN);
    BOOST_CHECK(res);

    unsigned int version{0};
    heat::model::PrismThermalModel model;
    auto prismThermalModelFile = std::string(nano::CurrentDir()) + "/model.prism.thermal.bin";
    res = nano::Load(model, version, prismThermalModelFile, ArchiveFormat::BIN);
    BOOST_CHECK(res);

    auto getDieMonitors = [&] {
        Vec<FCoord3D> monitors;
        auto layout = model.GetLayout();
        Float elevation{0}, thickness{0};
        package::utils::LayoutRetriever retriever(layout);
        std::vector<std::string> cellInsts{"TopBridge1", "TopBridge2", "BotBridge1", "BotBridge2"};
        std::vector<std::string> components{"Die1", "Die2", "Die3"};
        return monitors;
    };

    PrismThermalSimulationSetup setup;
    setup.monitors = getDieMonitors();
    heat::simulation::PrismThermalSimulation simulation(&model, setup);

    Vec<Float> temperature;
    // auto minmax = simulation.RunStatic(temperature);
    Database::Shutdown();
}

test_suite * create_nano_heat_simulation_test_suite()
{
    test_suite * simulation_suite = BOOST_TEST_SUITE("s_heat_simulation_test");
    //
    simulation_suite->add(BOOST_TEST_CASE(&t_prism_thermal_simulation));
    //
    return simulation_suite;
}