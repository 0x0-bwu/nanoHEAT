#include "NSModelLayoutPolygonMerger.h"
#include <nano/core/package>
#include <nano/core/basic>
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
        auto & pool = nano::thread::Pool();
        for (const auto & merger : m_mergers)
            pool.Submit(std::bind(&LayoutPolygonMerger::MergeOneLayer, this, merger.second.get()));
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

} // namespace nano::heat::model::utils