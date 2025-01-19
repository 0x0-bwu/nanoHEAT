#include "NSModelPrismThermal.h"
NS_SERIALIZATION_CLASS_EXPORT_IMP(nano::heat::model::PrismThermalModel)

namespace nano::heat::model {

inline static constexpr auto NO_NEIGHBOR = generic::geometry::tri::noNeighbor;
#ifdef NANO_BOOST_SERIALIZATION_SUPPORT
    
template <typename Archive>
void PrismThermalModel::serialize(Archive & ar, const unsigned int version)
{
    NS_UNUSED(version)
    NS_SERIALIZATION_CLASS_MEMBERS(ar);
}
    
NS_SERIALIZATION_FUNCTIONS_IMP(PrismThermalModel)
#endif//NANO_BOOST_SERIALIZATION_SUPPORT

PrismThermalModel::PrismThermalModel()
{
    NS_CLASS_MEMBERS_INITIALIZE
    m_.blockBCs.emplace(Orientation::Top, Vec<BlockBC>());
    m_.blockBCs.emplace(Orientation::Bot, Vec<BlockBC>());
}

void PrismThermalModel::SetLayerPrismTemplate(IdType layer, SPtr<PrismTemplate> prismTemplate)
{
    m_.prismTemplates.emplace(layer, prismTemplate);
}

SPtr<PrismThermalModel::PrismTemplate> PrismThermalModel::GetLayerPrismTemplate(IdType layer) const
{
    auto iter = m_.prismTemplates.find(layer);
    return iter != m_.prismTemplates.cend() ? iter->second : nullptr;
}

void PrismThermalModel::AddBlockBC(Orientation ori, FBox2D box, ThermalBoundaryCondition bc)
{
    m_.blockBCs[ori].emplace_back(box, bc);
}

PrismLayer & PrismThermalModel::AppendLayer(PrismLayer layer)
{
    return m_.layers.emplace_back(std::move(layer));
}

LineElement & PrismThermalModel::AddLineElement(FCoord3D start, FCoord3D end, IdType netId, IdType matId, Float radius, Float current, ScenarioId scenId)
{
    NS_ASSERT_MSG(TotalPrismElements() > 0, "should add after build prism model")
    auto & elem = m_.lines.emplace_back(LineElement());
    elem.id = TotalPrismElements() + m_.lines.size() - 1;
    elem.endPts[0] = AddPoint(std::move(start));
    elem.endPts[1] = AddPoint(std::move(end));
    elem.current = current;
    elem.scenId = scenId;
    elem.radius = radius;
    elem.matId = matId;
    elem.netId = netId;
    return elem;
}

void PrismThermalModel::BuildPrismModel(Float scaleH2Unit, Float scale2Meter)
{
    m_.scaleH2Unit = scaleH2Unit;
    m_.scale2Meter = scale2Meter;
    m_.indexOffset = {0};
    for (IdType i = 0; i < TotalLayers(); ++i)
        m_.indexOffset.emplace_back(m_.indexOffset.back() + m_.layers.at(i).TotalElements());

    const auto & triangles = m_.prismTemplates.at(0)->triangles;
    HashMap<IdType, HashMap<IdType, IdType>> templateIdMap;
    auto getPtIdxMap = [&templateIdMap](IdType layer) -> HashMap<size_t, size_t> & {
        auto iter = templateIdMap.find(layer);
        if (iter == templateIdMap.cend())
            iter = templateIdMap.emplace(layer, HashMap<IdType, IdType>()).first;
        return iter->second;
    }; 

    auto total = TotalPrismElements();
    m_.points.clear();
    m_.prisms.reserve(total);
    for (size_t i = 0; i < total; ++i) {
        auto [lyrIdx, eleIdx] = PrismLocalIndex(i);
        const auto & element = GetPrismElement(lyrIdx, eleIdx);
        auto & instance = m_.prisms.emplace_back(PrismInstance(lyrIdx, eleIdx));

        // points
        auto & topPtIdxMap = getPtIdxMap(lyrIdx);
        auto & botPtIdxMap = getPtIdxMap(lyrIdx + 1);
        const auto & vertices = triangles.at(element.templateId).vertices;
        for (size_t v = 0; v < vertices.size(); ++v) {
            auto topVtxIter = topPtIdxMap.find(vertices[v]);
            if (topVtxIter == topPtIdxMap.cend()) {
                auto ptIdx = AddPoint(GetPoint(lyrIdx, eleIdx, v));
                topVtxIter = topPtIdxMap.emplace(vertices[v], ptIdx).first;
            }
            instance.vertices[v] = topVtxIter->second;
            auto botVtxIter = botPtIdxMap.find(vertices[v]);
            if (botVtxIter == botPtIdxMap.cend()) {
                auto ptIdx = AddPoint(GetPoint(lyrIdx, eleIdx, v + 3));
                botVtxIter = botPtIdxMap.emplace(vertices[v], ptIdx).first;
            }
            instance.vertices[v + 3] = botVtxIter->second;
        }

        // neighbors
        for (size_t n = 0; n < 3; ++n) {
            if (auto nid = element.neighbors.at(n); NO_NEIGHBOR != nid) {
                auto nb = GlobalIndex(lyrIdx, element.neighbors.at(n));
                instance.neighbors[n] = nb;
            }
        }
        // top
        if (auto nid = element.neighbors.at(PrismElement::TOP_NEIGHBOR_INDEX); NO_NEIGHBOR != nid) {
            auto nb = GlobalIndex(lyrIdx - 1, nid);
            instance.neighbors[PrismElement::TOP_NEIGHBOR_INDEX] = nb;
        }
        // bot
        if (auto nid = element.neighbors.at(PrismElement::BOT_NEIGHBOR_INDEX); NO_NEIGHBOR != nid) {
            auto nb = GlobalIndex(lyrIdx + 1, nid);
            instance.neighbors[PrismElement::BOT_NEIGHBOR_INDEX] = nb;
        }
    }
}

void PrismThermalModel::AddBondingWires(CPtr<LayerStackupModel> stackupModel)
{
    //todo
}

IdType PrismThermalModel::AddPoint(FCoord3D point)
{
    m_.points.emplace_back(point);
    return m_.points.size() - 1;
}

FCoord3D PrismThermalModel::GetPoint(IdType lyrId, IdType elemId, IdType vtxId) const
{
    const auto & points = GetLayerPrismTemplate(lyrId)->points;
    const auto & triangles = GetLayerPrismTemplate(lyrId)->triangles;
    const auto & element = m_.layers.at(lyrId).elements.at(elemId);
    const auto & triangle = triangles.at(element.templateId);
    Float height = vtxId < 3 ? m_.layers.at(lyrId).elevation : 
           isBotLayer(lyrId) ? m_.layers.at(lyrId).elevation - m_.layers.at(lyrId).thickness :
                               m_.layers.at(lyrId + 1).elevation;
    vtxId = vtxId % 3;
    const auto & pt2d = points.at(triangle.vertices.at(vtxId));
    return FCoord3D(pt2d[0] * m_.scaleH2Unit, pt2d[1] * m_.scaleH2Unit, height);
}

void PrismThermalModel::SearchElementIndices(const Vec<FCoord3D> & monitors, Vec<IdType> & indices) const
{
    //todo
}
            


} // namespace nano::heat::model