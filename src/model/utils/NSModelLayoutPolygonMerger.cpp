#include "NSModelLayoutPolygonMerger.h"
#include <nano/core/package>
#include <nano/core/basic>

#include "generic/geometry/GeometryIO.hpp"

namespace nano::heat::model::utils {

LayoutPolygonMerger::LayoutPolygonMerger(CId<pkg::Layout> layout) : m_layout(layout)
{
    NS_ASSERT(m_layout);
}

void LayoutPolygonMerger::Merge()
{
    FillPolygonsFromLayout();
    MergeLayers();
}

bool LayoutPolygonMerger::GetMergedPolygons(Index layer, Vec<CPtr<PolygonData>> & polygons) const
{
    auto merger = m_mergers.find(layer);
    if (merger == m_mergers.cend()) return false;
    merger->second->GetAllPolygons(polygons);
    return true;
}

bool LayoutPolygonMerger::WritePNG(std::string_view dirname, size_t width) const
{
    for (auto & [layer, merger] : m_mergers) {
        auto filename = std::string(dirname) + "/merge_layer" + std::to_string(layer) + ".png";
        if (not WritePNG(merger.get(), filename.c_str(), width)) return false;
    }
    return true;
}

void LayoutPolygonMerger::FillPolygonsFromLayout()
{
    Vec<CId<pkg::StackupLayer>> layers;
    auto connObjIter = m_layout->GetConnObjIter();
    while (auto connObj = connObjIter.Next()) {
        auto net = connObj->GetNet();
        if (auto routingWire = connObj->GetRoutingWire(); routingWire) {
            auto layer = routingWire->GetStackupLayer();
            auto shape = routingWire->GetShape();
            auto mat = layer->GetConductingMaterial();
            FillOneShape(Index(layer), Index(net), Index(mat), &(*shape));
        }
        else if (auto padstackInst = connObj->GetPadstackInst(); padstackInst) {
            auto padstack = padstackInst->GetPadstack();
            auto viaShape = padstackInst->GetViaShape();
            auto mat = padstack->GetMaterial();
            padstackInst->GetLayerRange(layers);
            for (const auto & layer : layers) {
                if (auto shape = padstackInst->GetPadShape(layer); shape) {
                    FillOneShape(Index(layer), Index(net), Index(mat), shape.get());
                }
                if (viaShape)
                    FillOneShape(Index(layer), Index(net), Index(mat), viaShape.get());
            }
        }
    }
}

void LayoutPolygonMerger::FillOneShape(Index layer, Index net, Index mat, CPtr<Shape> shape)
{
    NS_ASSERT(shape);
    auto merger = m_mergers.find(layer);
    if (merger == m_mergers.cend()) {
        merger = m_mergers.emplace(layer, std::make_unique<LayerMerger>()).first;
    }
    if (shape->hasHole())
        merger->second->AddObject({net, mat}, shape->GetContour());
    else merger->second->AddObject({net, mat}, shape->GetOutline());
}

void LayoutPolygonMerger::MergeLayers()
{
    if (auto threads = nano::thread::Threads(); threads > 1) {
        auto pool = nano::thread::Pool();
        for (const auto & merger : m_mergers)
            pool.Submit(std::bind(&LayoutPolygonMerger::MergeOneLayer, this, merger.second.get()));
        pool.Wait();
    }
    else {
        for (const auto & merger : m_mergers)
            MergeOneLayer(merger.second.get());
    }
}
void LayoutPolygonMerger::MergeOneLayer(Ptr<LayerMerger> merger)
{
    typename LayerMerger::MergeSettings settings;
    //todo, add settings
    merger->SetMergeSettings(settings);    
    generic::geometry::PolygonMergeRunner runner(*merger, 1);
    runner.Run();
}

bool LayoutPolygonMerger::WritePNG(Ptr<LayerMerger> merger, std::string_view filename, size_t width) const
{
    using PolygonData = typename LayerMerger::PolygonData;

    std::vector<CPtr<PolygonData>> polygons;
    merger->GetAllPolygons(polygons);

    std::vector<NPolygon> outs;
    outs.reserve(polygons.size());
    for (auto polygon : polygons) {
        outs.push_back(polygon->solid);
        for(const auto & hole : polygon->holes) {
            outs.push_back(hole);
        }
    }
    using namespace generic::geometry;
    return GeometryIO::WritePNG(filename, outs.begin(), outs.end(), width);
}

} // namespace nano::heat::model::utils