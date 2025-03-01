#pragma once
#include "basic/NSHeatCommon.hpp"
#include <nano/common>
#include <nano/fwd>

#include "generic/geometry/PolygonMerge.hpp"

namespace nano::heat::model::utils {

namespace pkg = nano::package;
class LayoutPolygonMerger
{
public:
    using Attribute = Arr2<Index>;//net, material
    using Settings = LayoutPolygonMergeSettings;
    using LayerMerger = generic::geometry::PolygonMerger<Attribute, NCoord>;
    using PolygonData = typename LayerMerger::PolygonData;
    explicit LayoutPolygonMerger(CId<pkg::Layout> layout, Settings settings);
    virtual ~LayoutPolygonMerger() = default;

    void Merge();
    bool GetMergedPolygons(Index layer, Vec<CPtr<PolygonData>> & polygons) const;
    bool WritePNG(std::string_view dirname, size_t width) const;

private:
    void FillPolygonsFromLayout();
    void FillOneShape(Index layer, Index net, Index mat, CPtr<Shape> shape);
    void MergeLayers();
    void MergeOneLayer(Ptr<LayerMerger> merger);
    bool WritePNG(Ptr<LayerMerger> merger, std::string_view filename, size_t width) const;

private:
    CId<pkg::Layout> m_layout;
    LayoutPolygonMergeSettings m_settings;
    HashMap<Index, UPtr<LayerMerger>> m_mergers;//[layer, merger]
};

} // namespace nano::heat::model::utils