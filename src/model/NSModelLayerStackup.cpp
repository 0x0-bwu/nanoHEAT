#include "NSModelLayerStackup.h"
NS_SERIALIZATION_CLASS_EXPORT_IMP(nano::heat::model::LayerStackupModel)
#include "generic/geometry/GeometryIO.hpp"
#include <set>
namespace nano::heat::model {


#ifdef NANO_BOOST_SERIALIZATION_SUPPORT
    
template <typename Archive>
void LayerStackupModel::serialize(Archive & ar, const unsigned int version)
{
    NS_UNUSED(version)
    ar & boost::serialization::make_nvp("nets"           , m_nets          );
    ar & boost::serialization::make_nvp("materials"      , m_materials     );
    ar & boost::serialization::make_nvp("polygons"       , m_polygons      );
    ar & boost::serialization::make_nvp("steiner_points" , m_steinerPoints );
    ar & boost::serialization::make_nvp("layer_ranges"   , m_layerRanges   );
    ar & boost::serialization::make_nvp("bonding_wires"  , m_bondingWires  );
    ar & boost::serialization::make_nvp("power_blocks"   , m_powerBlocks   );
    ar & boost::serialization::make_nvp("settings"       , m_settings      );
    ar & boost::serialization::make_nvp("layer_polygons" , m_layerPolygons );
    ar & boost::serialization::make_nvp("height2index"   , m_height2indices);
    ar & boost::serialization::make_nvp("layer_order"    , m_layerOrder    );
    ar & boost::serialization::make_nvp("scale"          , m_vScale2Int    );
}
    
NS_SERIALIZATION_FUNCTIONS_IMP(LayerStackupModel)
#endif//NANO_BOOST_SERIALIZATION_SUPPORT

bool LayerStackupModel::WritePNG(std::string_view filename, size_t width) const
{
    auto shapes = m_polygons;
    for (const auto & bw : m_bondingWires)
        shapes.emplace_back(NPolygon(bw.pt2ds));
    return generic::geometry::GeometryIO::WritePNG(filename, shapes.begin(), shapes.end(), width);
    return false;
}

void LayerStackupModel::BuildLayerPolygonLUT(Float vTransitionRatio)
{
    m_layerOrder.clear();
    m_height2indices.clear();
    std::set<Height> heights;
    for (size_t i = 0; i < m_layerRanges.size(); ++i) {
        if (INVALID_ID == m_materials.at(i)) continue;
        const auto & range = m_layerRanges.at(i);
        if (not range.isValid()) continue;
        heights.emplace(range.high);
        heights.emplace(range.low);
        auto iter = m_powerBlocks.find(i);
        if (iter != m_powerBlocks.cend()) {
            heights.emplace(iter->second.range.high);
            heights.emplace(iter->second.range.low);
        }
    }
    m_layerOrder = std::vector(heights.begin(), heights.end());
    std::reverse(m_layerOrder.begin(), m_layerOrder.end());
    for (size_t i = 0; i < m_layerOrder.size(); ++i)
        m_height2indices.emplace(m_layerOrder.at(i), i);

    m_layerPolygons.clear();
    for (size_t i = 0; i < m_polygons.size(); ++i) {
        const auto & range = m_layerRanges.at(i);
        if (not range.isValid()) continue;
        IdType sLayer = m_height2indices.at(range.high);
        IdType eLayer = std::min(TotalLayers(), m_height2indices.at(range.low));
        for (IdType layer = sLayer; layer < eLayer; ++layer) {
            auto iter = m_layerPolygons.find(layer);
            if (iter == m_layerPolygons.cend())
                iter = m_layerPolygons.emplace(layer, new PolygonIds).first;
            iter->second->emplace_back(i);
        }
    }

    if (generic::math::GT<Float>(vTransitionRatio, 1)) {
        std::list<LayerRange> ranges;
        for (size_t i = 0; i < m_layerOrder.size() - 1; ++i)
            ranges.emplace_back(m_layerOrder.at(i), m_layerOrder.at(i + 1));

        bool sliced = true;
        while (sliced) {
            sliced = SliceOverheightLayers(ranges, vTransitionRatio);
        }
        m_layerOrder.clear();
        m_layerOrder.reserve(ranges.size() + 1);
        for (const auto & range : ranges)
            m_layerOrder.emplace_back(range.high);
        m_layerOrder.emplace_back(ranges.back().low);
        std::unordered_map<size_t, SPtr<std::vector<size_t>> > lyrPolygons;
        for (size_t i = 0; i < m_layerOrder.size() - 1; ++i) {
            auto iter = m_height2indices.find(m_layerOrder.at(i));
            if (iter != m_height2indices.cend())
                lyrPolygons.emplace(i, m_layerPolygons.at(iter->second));
            else lyrPolygons.emplace(i, lyrPolygons.at(i - 1));
        }
        std::swap(m_layerPolygons, lyrPolygons);
        m_height2indices.clear();
        for (size_t i = 0; i < m_layerOrder.size(); ++i)
            m_height2indices.emplace(m_layerOrder.at(i), i);
    }
}

size_t LayerStackupModel::TotalLayers() const
{
    return m_layerOrder.size() - 1;
}

bool LayerStackupModel::hasPolygon(IdType layer) const
{
    return m_layerPolygons.count(layer);
}

bool LayerStackupModel::GetLayerHeightThickness(IdType layer, Float & elevation, Float & thickness) const
{
    if (layer >= TotalLayers()) return false;
    elevation = Float(m_layerOrder.at(layer)) / m_vScale2Int;
    thickness = elevation - Float(m_layerOrder.at(layer + 1)) / m_vScale2Int;
    return true;
}

IdType LayerStackupModel::GetLayerIndexByHeight(Height height) const
{
    auto iter = m_height2indices.find(height);
    if (iter == m_height2indices.cend()) return INVALID_ID;
    return iter->second;
}

const NPolygon & LayerStackupModel::GetLayoutBoundary() const
{
    return m_polygons.front();
}

const auto & LayerStackupModel::GetAllPowerBlocks() const
{
    return m_powerBlocks;
}

const auto & LayerStackupModel::GetAllPolygons() const
{
    return m_polygons;
}

const auto & LayerStackupModel::GetSteinerPoints() const
{
    return m_steinerPoints;
}

const auto & LayerStackupModel::GetAllBondingWires() const
{
    return m_bondingWires;
}

SPtr<LayerStackupModel::PolygonIds> LayerStackupModel::GetLayerPolygonIds(size_t layer) const
{
    auto iter = m_layerPolygons.find(layer);
    NS_ASSERT(iter != m_layerPolygons.cend());
    return iter->second;
}

LayerStackupModel::LayerRange LayerStackupModel::GetLayerRange(Float elevation, Float thickness) const
{
    return LayerRange{GetHeight(elevation), GetHeight(elevation - thickness)};
}

std::vector<NPolygon> LayerStackupModel::GetLayerPolygons(size_t layer) const
{
    auto indices = GetLayerPolygonIds(layer);
    std::vector<NPolygon> polygons; polygons.reserve(indices->size());
    std::transform(indices->begin(), indices->end(), std::back_inserter(polygons), [&](auto i){ return m_polygons.at(i); });
    return polygons;
}

bool LayerStackupModel::SliceOverheightLayers(std::list<LayerRange> & ranges, Float ratio)
{
    auto slice = [](const LayerRange & r)
    {
        Height mid = std::round(0.5 * (r.high + r.low));
        auto res = std::make_pair(r, r);
        res.first.low = mid;
        res.second.high = mid;
        return res;
    };

    bool sliced = false;
    auto curr = ranges.begin();
    for (;curr != ranges.end();){
        auto currH = curr->Thickness(); NS_ASSERT(currH > 0)
        if (curr != ranges.begin()) {
            auto prev = curr; prev--;
            auto prevH = prev->Thickness();
            auto r = currH / (Float)prevH;
            if (generic::math::GT<Float>(r, ratio)){
                auto [top, bot] = slice(*curr);
                curr = ranges.erase(curr);
                curr = ranges.insert(curr, bot);
                curr = ranges.insert(curr, top);
                sliced = true;
                curr++;
            }
        }
        auto next = curr; next++;
        if (next != ranges.end()){
            auto nextH = next->Thickness(); NS_ASSERT(nextH > 0)
            auto r = currH / (Float)nextH;
            if (generic::math::GT<Float>(r, ratio)) {
                auto [top, bot] = slice(*curr);
                curr = ranges.erase(curr);
                curr = ranges.insert(curr, bot);
                curr = ranges.insert(curr, top);
                sliced = true;
                curr++;
            }
        }
        curr++;
    }
    return sliced;   
}

} // namespace nano::heat::model