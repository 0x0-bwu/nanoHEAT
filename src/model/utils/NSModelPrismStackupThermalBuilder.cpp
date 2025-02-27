#include "NSModelPrismStackupThermalBuilder.h"
#include "NSModelPrismMeshGenerator.h"
#include "NSModelLayerStackupQuery.h"
#include "NSModelLayerStackup.h"
#include "NSModel.h"

#include "generic/geometry/BooleanOperation.hpp"

namespace nano::heat::model::utils {

using namespace generic::geometry;
inline static constexpr auto NO_NEIGHBOR = generic::geometry::tri::noNeighbor;

PrismStackupThermalModelBuilder::PrismStackupThermalModelBuilder(Ref<Model> model) : m_model(model)
{
    m_query.reset(new PrismStackupThermalModelQuery(m_model));
}

bool PrismStackupThermalModelBuilder::Build(CId<Layout> layout, Settings settings)
{
    if (not layout) return false;

    m_model.Reset();
    auto stackupModel = CreateLayerStackupModel(layout, settings.layerSettings);
    if (not stackupModel) return false;

    return Build(layout, stackupModel.get(), settings.meshSettings, settings.bcSettings);
}

bool PrismStackupThermalModelBuilder::Build(CId<Layout> layout, CPtr<LayerStackupModel> stackupModel, PrismMeshSettings meshSettings, BoundaryCondtionSettings bcSettings)
{
    if (not layout or not stackupModel) return false;

    const auto & coordUnit = layout->GetCoordUnit();
    Vec<Vec<NPolygon>> layerPolygons{stackupModel->GetLayerPolygons(0)};
    Vec<SPtr<PrismTemplate>> prismTemplates{std::make_shared<PrismTemplate>()};
    HashMap<Index, Index> layer2Template{{0, 0}};
    for (Index i = 1; i < stackupModel->TotalLayers(); ++i) {
        auto indices = stackupModel->GetLayerPolygonIds(i);
        if (indices != stackupModel->GetLayerPolygonIds(i - 1)) {
            auto polygons = stackupModel->GetLayerPolygons(i);
            if (meshSettings.imprintUpperLayer) {
                const auto & upperLyr = layerPolygons.back();
                polygons.insert(polygons.end(), upperLyr.begin(), upperLyr.end());
            }
            layerPolygons.emplace_back(std::move(polygons));
            prismTemplates.emplace_back(new PrismTemplate);
        }
        layer2Template.emplace(i, prismTemplates.size() - 1);
    }
    
    Vec<NCoord2D> steinerPoints;//todo
    NS_TRACE("generate mesh for %1% layers", prismTemplates.size());
    if (nano::thread::Threads() > 1) {
        Vec<std::string> workDirs;
        auto pool = nano::thread::Pool();
        for (Index i = 0; i < prismTemplates.size(); ++i)
            workDirs.emplace_back(std::string(nano::CurrentDir()) + "/mesh" + std::to_string(i + 1));
        for (Index i = 0; i < prismTemplates.size(); ++i)
            pool.Submit(std::bind(GenerateMesh, std::ref(layerPolygons.at(i)), std::ref(steinerPoints), std::ref(coordUnit), 
                        std::ref(meshSettings), std::ref(*prismTemplates.at(i)), workDirs.at(i).c_str()));
        pool.Wait();
    }
    else {
        for (size_t i = 0; i < prismTemplates.size(); ++i) {
            auto workDir = std::string(nano::CurrentDir()) + "/mesh" + std::to_string(i);
            GenerateMesh(layerPolygons.at(i), steinerPoints, coordUnit, meshSettings, *prismTemplates.at(i), workDir.c_str());
        }
    }

    // for debug
    {
        // for (size_t i = 0; i < prismTemplates.size(); ++i) {
        //     auto & triangulation = *prismTemplates.at(i);
        //     tri::TriangleEvaluator<NCoord2D> eval(triangulation);
        //     NS_TRACE("layer %1% min edge: %2%", i + 1, eval.MinEdgeLength());
        // }
    }

    for (size_t i = 0; i < stackupModel->TotalLayers(); ++i) {
        m_model.SetLayerPrismTemplate(i, prismTemplates.at(layer2Template.at(i)));
        PrismLayer prismLayer(i);
        [[maybe_unused]] auto check = stackupModel->GetLayerHeightThickness(i, prismLayer.elevation, prismLayer.thickness); { NS_ASSERT(check) }
        m_model.AppendLayer(std::move(prismLayer));
    }

    HashSet<Index> fluidMats;
    auto matIter = layout->GetMaterialIter();
    while (auto mat = matIter.Next()) {
        if (MaterialType::FLUID == mat->GetMaterialType())
            fluidMats.insert(Index(mat));
    }

    LayerStackupModelQuery query(*stackupModel);
    const auto & powerBlocks = stackupModel->GetAllPowerBlocks();
    HashMap<Index, HashMap<Index, Index>> templateIdMap;
    auto buildOnePrismLayer = [&](size_t index) {
        auto & prismLayer = m_model->layers.at(index);
        auto & idMap = templateIdMap.emplace(prismLayer.id, HashMap<Index, Index>{}).first->second;    
        auto triangulation = m_model.GetLayerPrismTemplate(index);
        for (size_t it = 0; it < triangulation->triangles.size(); ++it) {
            NS_ASSERT(stackupModel->hasPolygon(prismLayer.id))
            auto ctPoint = tri::TriangulationUtility<NCoord2D>::GetCenter(*triangulation, it).Cast<NCoord>();
            auto pid = query.SearchPolygon(prismLayer.id, ctPoint);
            if (INVALID_INDEX == pid) continue;;
            if (INVALID_INDEX == stackupModel->GetMaterialId(pid)) continue;
            if (fluidMats.count(stackupModel->GetMaterialId(pid))) continue;

            auto & ele = prismLayer.AddElement(it);
            idMap.emplace(it, ele.id);
            ele.matId = stackupModel->GetMaterialId(pid);
            ele.netId = stackupModel->GetNetId(pid);
            auto iter = powerBlocks.find(pid);
            if (iter != powerBlocks.cend() &&
                prismLayer.id == stackupModel->GetLayerIndexByHeight(iter->second.range.high)) {
                auto area = tri::TriangulationUtility<NCoord2D>::GetTriangleArea(*triangulation, it);
                ele.powerRatio = area / stackupModel->GetAllPolygons().at(pid).Area();
                ele.scenId = iter->second.scen;
                ele.powerLutId = iter->second.powerLut;
            }
        }
        NS_TRACE("layer %1%'s total elements: %2%", index, prismLayer.elements.size());
    };

    for (size_t index = 0; index < m_model.TotalLayers(); ++index)
        buildOnePrismLayer(index);

    //build connection
    for (Index index = 0; index < m_model.TotalLayers(); ++index) {
        auto & layer = m_model->layers.at(index);
        auto & elements = layer.elements;
        const auto & currIdMap = templateIdMap.at(layer.id);
        auto triangulation = m_model.GetLayerPrismTemplate(index);
        for (auto & ele : elements) {
            //layer neighbors
            const auto & triangle = triangulation->triangles.at(ele.templateId);
            for (size_t nid = 0; nid < triangle.neighbors.size(); ++nid) {
                if (NO_NEIGHBOR == triangle.neighbors.at(nid)) continue;
                auto iter = currIdMap.find(triangle.neighbors.at(nid));
                if (iter != currIdMap.cend()) ele.neighbors[nid] = iter->second;
            }
            ele.neighbors[PrismElement::TOP_NEIGHBOR_INDEX] = ele.id;
            ele.neighbors[PrismElement::BOT_NEIGHBOR_INDEX] = ele.id;
        }
    }

    auto scaleH2Unit = coordUnit.Scale2Unit();
    auto scale2Meter = coordUnit.toUnit(coordUnit.toCoord(1), CoordUnit::Unit::Meter);
    this->BuildPrismModel(scaleH2Unit, scale2Meter);
    this->AddBondingWires(stackupModel);
    NS_TRACE("total elements: %1%, prism: %2%, line: %3%", 
    m_model.TotalElements(), m_model.TotalPrismElements(), m_model.TotalLineElements());

    // bc
    if (bcSettings.uniformBCs[0].isValid())
        m_model.SetUniformBC(Orientation::TOP, bcSettings.uniformBCs[0]);
    if (bcSettings.uniformBCs[1].isValid())
        m_model.SetUniformBC(Orientation::BOT, bcSettings.uniformBCs[1]);
        
    for (const auto & block : bcSettings.blockBCs[0]) {
        if (not block.second.isValid()) continue;
        m_model.AddBlockBC(Orientation::TOP, coordUnit.toCoord(block.first), block.second);
    }
    for (const auto & block : bcSettings.blockBCs[1]) {
        if (not block.second.isValid()) continue;
        m_model.AddBlockBC(Orientation::BOT, coordUnit.toCoord(block.first), block.second);
    }
        
    if (meshSettings.dumpMeshFile) { 
        auto meshFile = std::string(nano::CurrentDir()) + "/mesh.vtk";
        m_model.WriteVTK<Float>(meshFile);
    }
    m_model->layout = layout;
    return true;
}

void PrismStackupThermalModelBuilder::BuildPrismModel(Float scaleH2Unit, Float scale2Meter)
{
    m_model->scaleH2Unit = scaleH2Unit;
    m_model->scale2Meter = scale2Meter;
    m_model->indexOffset = Vec<Index>{0};
    for (Index i = 0; i < m_model.TotalLayers(); ++i)
        m_model->indexOffset.emplace_back(m_model->indexOffset.back() + m_model->layers.at(i).TotalElements());
    
    const auto total = m_model->indexOffset.back();
    m_model->prisms.reserve(total);
    m_model->points.resize(total * 6);
    for (size_t i = 0; i < total; ++i) {
        auto [lyrIdx, eleIdx] = m_model.PrismLocalIndex(i);
        auto & instance = m_model->prisms.emplace_back(lyrIdx, eleIdx);

        //points
        for (size_t v = 0; v < 6; ++v) {
            m_model->points[6 * i + v] = m_model.GetPoint(lyrIdx, eleIdx, v);
            instance.vertices[v] = 6 * i + v;
        }

        //side neighbors
        const auto & element = m_model.GetPrismElement(lyrIdx, eleIdx);
        for (size_t n = 0; n < 3; ++n) {
            if (auto nid = element.neighbors[n]; NO_NEIGHBOR != nid) {
                auto nb = m_model.GlobalIndex(lyrIdx, element.neighbors.at(n));
                instance.neighbors[n] = nb;
            }
        }
    }

    if (auto threads = nano::thread::Threads(); threads > 1) {
        auto pool = nano::thread::Pool();
        const auto & offset = m_model->indexOffset;
        for (Index i = 0; i < offset.size() - 1; ++i)
            pool.Submit(std::bind(&PrismStackupThermalModelBuilder::BuildPrismInstanceTopBotNeighbors, this, offset.at(i), offset.at(i + 1)));
        pool.Wait();
    }
    else BuildPrismInstanceTopBotNeighbors(0, m_model.TotalPrismElements());
}

void PrismStackupThermalModelBuilder::BuildPrismInstanceTopBotNeighbors(Index start, Index end)
{
    auto getIntersectArea = [](const auto & t1, const auto & t2) {
        Vec<NPolygon> output;
        generic::geometry::boolean::Intersect(t1, t2, output);
        return std::accumulate(output.begin(), output.end(), Float(0), [](auto sum, const auto & p) { return sum + std::abs(p.Area()); });
    };

    using RtVal = utils::PrismStackupThermalModelQuery::RtVal;
    for (Index i = start; i < end; ++i) {
        auto & instance = m_model->prisms.at(i);
        auto [lyrIdx, eleIdx] = m_model.PrismLocalIndex(i);
        auto triangle1 = m_query->GetPrismInstanceTemplate(i);
        auto triangle1Area = triangle1.Area();
        if (m_model.isTopLayer(lyrIdx))
            instance.neighbors[PrismElement::TOP_NEIGHBOR_INDEX] = NO_NEIGHBOR;
        else {
            auto topLyr = lyrIdx - 1;
            Vec<RtVal> results;
            m_query->IntersectsPrismInstances(topLyr, i, results);
            for (Index j = 0; j < results.size(); ++j) {
                auto triangle2 = m_query->GetPrismInstanceTemplate(results.at(j).second);
                if (auto area = getIntersectArea(triangle1, triangle2); area > 0)
                    instance.contacts.front().emplace_back(results.at(j).second, Float(area) / triangle1Area);
            }
            if (instance.contacts.empty())
                instance.neighbors[PrismElement::TOP_NEIGHBOR_INDEX] = NO_NEIGHBOR;
            else instance.neighbors[PrismElement::TOP_NEIGHBOR_INDEX] = i;
        }

        if (m_model.isBotLayer(lyrIdx))
            instance.neighbors[PrismElement::BOT_NEIGHBOR_INDEX] = NO_NEIGHBOR;
        else {
            auto botLyr = lyrIdx + 1;
            Vec<RtVal> results;
            m_query->IntersectsPrismInstances(botLyr, i, results);
            for (Index j = 0; j < results.size(); ++j) {
                auto triangle2 = m_query->GetPrismInstanceTemplate(results.at(j).second);
                if (auto area = getIntersectArea(triangle1, triangle2); area > 0) {
                    instance.contacts.back().emplace_back(results.at(j).second, Float(area) / triangle1Area);
                }
            }
            if (instance.contacts.empty())
                instance.neighbors[PrismElement::BOT_NEIGHBOR_INDEX] = NO_NEIGHBOR;
            else instance.neighbors[PrismElement::BOT_NEIGHBOR_INDEX] = i;
        }
    }
}

void PrismStackupThermalModelBuilder::AddBondingWires(CPtr<LayerStackupModel> stackupModel)
{
    for (const auto & bw : stackupModel->GetAllBondingWires()) {
        const auto & pts = bw.pt2ds;
        NS_ASSERT(pts.size() == bw.heights.size());
        for (size_t curr = 0; curr < pts.size() - 1; ++curr) {
            auto next = curr + 1;
            auto p1 = FCoord3D(pts.at(curr)[0] * m_model->scaleH2Unit, pts.at(curr)[1] * m_model->scaleH2Unit, bw.heights.at(curr));
            auto p2 = FCoord3D(pts.at(next)[0] * m_model->scaleH2Unit, pts.at(next)[1] * m_model->scaleH2Unit, bw.heights.at(next));
            auto & line = m_model.AddLineElement(std::move(p1), std::move(p2), bw.netId, bw.matId, bw.radius, bw.current, bw.scenario);
            //connection
            if (0 == curr) {
                Vec<PrismStackupThermalModelQuery::RtVal> results;
                auto layer = stackupModel->GetLayerIndexByHeight(stackupModel->GetHeight(bw.heights.front()));
                m_query->SearchNearestPrismInstances(layer, pts.at(curr), 1, results);
                NS_ASSERT(results.size() == 1);
                std::for_each(results.begin(), results.end(), [&](const auto & r) { line.neighbors.front().push_back(r.second); });
            }
            else {
                auto & prevLine = m_model->lines.at(m_model->lines.size() - 2);
                line.neighbors.front().emplace_back(prevLine.id);
                prevLine.neighbors.back().emplace_back(line.id);
            }
            if (next == pts.size() - 1) {
                Vec<PrismStackupThermalModelQuery::RtVal> results;
                auto layer = stackupModel->GetLayerIndexByHeight(stackupModel->GetHeight(bw.heights.back()));
                m_query->SearchNearestPrismInstances(layer, pts.at(next), 1, results);
                NS_ASSERT(results.size() == 1);
                std::for_each(results.begin(), results.end(), [&](const auto & r) { line.neighbors.back().push_back(r.second); });
            }
        }
    }
}

} // namespace nano::heat::model::utils
