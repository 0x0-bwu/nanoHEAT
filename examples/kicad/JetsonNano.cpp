#include "simulation/NSSimulationPrismThermal.h"
#include "model/NSModel.h"
#include <nano/db>
using namespace nano;
using namespace nano::heat;
using namespace nano::package;

namespace detail {

void SetupMaterials(Id<Package> pkg)
{
    pkg->SetMaterialLib(nano::Create<MaterialLib>("mat_lib"));
    assert(pkg->GetMaterialLib());
    auto matLib = pkg->GetMaterialLib();
    assert(matLib and matLib->GetName() == "mat_lib");
    
    auto matAl = nano::Create<Material>("Al");
    matAl->SetProperty(Material::THERMAL_CONDUCTIVITY, nano::Create<MaterialPropValue>(238));
    matAl->SetProperty(Material::SPECIFIC_HEAT, nano::Create<MaterialPropValue>(880));
    matAl->SetProperty(Material::MASS_DENSITY, nano::Create<MaterialPropValue>(2700));
    matAl->SetProperty(Material::RESISTIVITY, nano::Create<MaterialPropValue>(2.82e-8));
    matLib->AddMaterial(matAl);

    auto matCu = nano::Create<Material>("Cu");
    matCu->SetProperty(Material::THERMAL_CONDUCTIVITY, nano::Create<MaterialPropPolynomial>(Vec<Vec<Float>>{{437.6, -0.165, 1.825e-4, -1.427e-7, 3.979e-11}}));
    matCu->SetProperty(Material::SPECIFIC_HEAT, nano::Create<MaterialPropPolynomial>(Vec<Vec<Float>>{{342.8, 0.134, 5.535e-5, -1.971e-7, 1.141e-10}}));
    matCu->SetProperty(Material::MASS_DENSITY, nano::Create<MaterialPropValue>(8850));
    matLib->AddMaterial(matCu);

    auto matAir = nano::Create<Material>("Air");
    matAir->SetMaterialType(MaterialType::FLUID);
    matAir->SetProperty(Material::THERMAL_CONDUCTIVITY, nano::Create<MaterialPropValue>(0.026));
    matAir->SetProperty(Material::SPECIFIC_HEAT, nano::Create<MaterialPropValue>(1.003));
    matAir->SetProperty(Material::MASS_DENSITY, nano::Create<MaterialPropValue>(1.225));
    matLib->AddMaterial(matAir);

    auto matSiC = nano::Create<Material>("SiC");
    matSiC->SetProperty(Material::THERMAL_CONDUCTIVITY, nano::Create<MaterialPropPolynomial>(Vec<Vec<Float>>{{1860, -11.7, 0.03442, -4.869e-5, 2.675e-8}}));
    matSiC->SetProperty(Material::SPECIFIC_HEAT, nano::Create<MaterialPropPolynomial>(Vec<Vec<Float>>{{-3338, 33.12, -0.1037, 0.0001522, -8.553e-8}}));
    matSiC->SetProperty(Material::MASS_DENSITY, nano::Create<MaterialPropValue>(3210));
    matLib->AddMaterial(matSiC);

    auto matAlN = nano::Create<Material>("AlN");
    matAlN->SetProperty(Material::THERMAL_CONDUCTIVITY, nano::Create<MaterialPropPolynomial>(Vec<Vec<Float>>{{421.7867, -1.1262, 0.001}}));
    matAlN->SetProperty(Material::SPECIFIC_HEAT, nano::Create<MaterialPropPolynomial>(Vec<Vec<Float>>{{170.2, -2.018, 0.032, -8.957e-5, 1.032e-7, -4.352e-11}}));
    matAlN->SetProperty(Material::MASS_DENSITY, nano::Create<MaterialPropValue>(3260));
    matLib->AddMaterial(matAlN);
    
    auto matSolder = nano::Create<Material>("Solder");
    matSolder->SetProperty(Material::THERMAL_CONDUCTIVITY, nano::Create<MaterialPropValue>(55));
    matSolder->SetProperty(Material::SPECIFIC_HEAT, nano::Create<MaterialPropValue>(218));
    matSolder->SetProperty(Material::MASS_DENSITY, nano::Create<MaterialPropValue>(7800));
    matSolder->SetProperty(Material::RESISTIVITY, nano::Create<MaterialPropValue>(11.4e-8));
    matLib->AddMaterial(matSolder);

    auto matFR4 = nano::Create<Material>("FR4");
    matFR4->SetProperty(Material::THERMAL_CONDUCTIVITY, nano::Create<MaterialPropValue>(0.3));
    matFR4->SetProperty(Material::SPECIFIC_HEAT, nano::Create<MaterialPropValue>(700));
    matFR4->SetProperty(Material::MASS_DENSITY, nano::Create<MaterialPropValue>(1700));
    matLib->AddMaterial(matFR4);
}

} // namespace detail

int main()
{
    nano::SetCurrentDir(generic::fs::DirName(__FILE__).string() + "/../../test/data/package/jetson-nano-baseboard");
    Database::Create("jetson-nano-baseboard");

    auto filename = std::string(nano::CurrentDir()) + ".kicad_pcb";
    auto package = extension::CreateFromKiCad(filename.c_str());
    auto layout = package->GetTop()->GetCell()->GetLayout().ConstCast();
    NS_ASSERT(layout);

    auto bbox = layout->GetBoundary()->GetBBox();
    const auto & coordUnit = layout->GetCoordUnit();
    NS_TRACE("design length: %1%, width: %2%", coordUnit.toUnit(bbox.Length()), coordUnit.toUnit(bbox.Width()));

    detail::SetupMaterials(package);
    auto matCu = package->FindMaterial("Cu"); assert(matCu);
    auto matAir = package->FindMaterial("Air"); assert(matAir);
    auto matFR4 = package->FindMaterial("FR4"); assert(matFR4);
    auto matSiC = package->FindMaterial("SiC"); assert(matSiC);
    auto matSolder = package->FindMaterial("Solder"); assert(matSolder);

    auto layerIter = package->GetStackupLayerIter();
    while (auto stackupLayer = layerIter.Next()) {
        stackupLayer->SetConductingMaterial(matCu);
        stackupLayer->SetDielectricMaterial(matFR4);
    }

    auto psIter = package->GetPadstackIter();
    while (auto ps = psIter.Next()) {
        ps->SetMaterial(matCu);
    }
    
    auto fpCellIter = package->GetFootprintCellIter();
    while (auto fpCell = fpCellIter.Next()) {
        fpCell->SetMaterial(matSiC);
        if (0 == fpCell->GetHeight())
            fpCell->SetHeight(1);
    }

    auto powerLut = nano::Create<LookupTable1D>(
    Vec<Float>{TempUnit(25).inKelvins()}, Vec<Float>{5});
    auto lossPower = nano::Create<power::LossPower>("power", ScenarioId(0), powerLut);

    auto compIter = layout->GetComponentIter();
    while (auto comp = compIter.Next()) {
        auto mountingLayer = comp->GetAssemblyLayer();
        NS_ASSERT(mountingLayer);
        auto footprint = mountingLayer->GetFootprint().ConstCast();
        footprint->SetSolderMaterial(matSolder);
        footprint->SetSolderFillingMaterial(matSolder);//wbtest todo;
        footprint->SetSolderBallBumpThickness(0.1);
        comp->Bind<power::LossPower>(lossPower);
    }

    PrismThermalModelExtractionSettings settings;

    auto & layerSettings = settings.layerSettings;
    layerSettings.addCircleCenterAsSteinerPoint = true;
    layerSettings.mergeSettings.cleanPolygonPoints = true;
    layerSettings.mergeSettings.cleanPointDistance = 1e-3;

    auto & meshSettings = settings.meshSettings;
    meshSettings.minAlpha = 15;
    meshSettings.minLen = 1e-1;
    meshSettings.maxLen = 10.0;
    meshSettings.tolerance = 0;
    meshSettings.maxIter = 1e4;
    meshSettings.dumpMeshFile = true;
    meshSettings.preSplitEdge = true;
    meshSettings.reportMeshQuality = true;
    auto & bcSettings = settings.bcSettings;
    bcSettings.SetTopUniformBC(ThermalBoundaryCondition::Type::HTC, 5000);
    bcSettings.SetBotUniformBC(ThermalBoundaryCondition::Type::HTC, 5000);

    nano::thread::SetThreads(1);//for debug
    auto model = model::CreatePrismStackupThermalModel(layout, settings);
    assert(model);

    auto getDieMonitors = [&] {
        Vec<FCoord3D> monitors;
        return monitors;
    };

    PrismThermalSimulationSetup setup;
    setup.monitors = getDieMonitors();
    heat::simulation::PrismStackupThermalSimulation simulation(model.get(), setup);

    Vec<Float> temperature;
    auto range = simulation.RunStatic(temperature);
    std::cout << "temperature range: " << range[0] << ", " << range[1] << std::endl;
    Database::Shutdown();
}