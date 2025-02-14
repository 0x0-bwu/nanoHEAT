#include "NSModelPrismThermal.h"
NS_SERIALIZATION_CLASS_EXPORT_IMP(nano::heat::model::PrismThermalModel)

#include "utils/NSModelPrismThermalQuery.h"
#include "NSModelLayerStackup.h"

#include "generic/tools/FileSystem.hpp"

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
    m_.blockBCs.emplace(Orientation::TOP, Vec<BlockBC>());
    m_.blockBCs.emplace(Orientation::BOT, Vec<BlockBC>());
}

void PrismThermalModel::SetLayerPrismTemplate(Index layer, SPtr<PrismTemplate> prismTemplate)
{
    m_.prismTemplates.emplace(layer, prismTemplate);
}

SPtr<PrismThermalModel::PrismTemplate> PrismThermalModel::GetLayerPrismTemplate(Index layer) const
{
    auto iter = m_.prismTemplates.find(layer);
    return iter != m_.prismTemplates.cend() ? iter->second : nullptr;
}

void PrismThermalModel::SetUniformBC(Orientation ori, BC bc)
{
    m_.uniformBCs.emplace(ori, std::move(bc));
}

CPtr<PrismThermalModel::BC> PrismThermalModel::GetUniformBC(Orientation ori) const
{
    auto iter = m_.uniformBCs.find(ori);
    if (iter == m_.uniformBCs.cend()) return nullptr;
    if (not iter->second.isValid()) return nullptr;
    return &iter->second;
}

void PrismThermalModel::AddBlockBC(Orientation ori, NBox2D box, BC bc)
{
    m_.blockBCs[ori].emplace_back(std::move(box), std::move(bc));
}

PrismLayer & PrismThermalModel::AppendLayer(PrismLayer layer)
{
    return m_.layers.emplace_back(std::move(layer));
}

LineElement & PrismThermalModel::AddLineElement(FCoord3D start, FCoord3D end, Index netId, Index matId, Float radius, Float current, ScenarioId scenId)
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
    for (Index i = 0; i < TotalLayers(); ++i)
        m_.indexOffset.emplace_back(m_.indexOffset.back() + m_.layers.at(i).TotalElements());

    HashMap<Index, HashMap<Index, Index>> templateIdMap;
    auto getPtIdxMap = [&templateIdMap](Index layer) -> HashMap<size_t, size_t> & {
        auto iter = templateIdMap.find(layer);
        if (iter == templateIdMap.cend())
            iter = templateIdMap.emplace(layer, HashMap<Index, Index>()).first;
        return iter->second;
    }; 

    const auto total = m_.indexOffset.back();
    m_.points.clear();
    m_.prisms.reserve(total);
    for (size_t i = 0; i < total; ++i) {
        auto [lyrIdx, eleIdx] = PrismLocalIndex(i);
        const auto & element = GetPrismElement(lyrIdx, eleIdx);
        const auto & triangles = GetLayerPrismTemplate(lyrIdx)->triangles;
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
    const auto & bws = stackupModel->GetAllBondingWires();
    if (bws.empty()) return;

    utils::PrismThermalModelQuery query(*this);
    for (const auto & bw : bws) {
        const auto & pts = bw.pt2ds;
        NS_ASSERT(pts.size() == bw.heights.size());
        for (size_t curr = 0; curr < pts.size() - 1; ++curr) {
            auto next = curr + 1;
            auto p1 = FCoord3D(pts.at(curr)[0] * m_.scaleH2Unit, pts.at(curr)[1] * m_.scaleH2Unit, bw.heights.at(curr));
            auto p2 = FCoord3D(pts.at(next)[0] * m_.scaleH2Unit, pts.at(next)[1] * m_.scaleH2Unit, bw.heights.at(next));
            auto & line = AddLineElement(std::move(p1), std::move(p2), bw.netId, bw.matId, bw.radius, bw.current, bw.scenario);
            //connection
            if (0 == curr) {
                Vec<utils::PrismThermalModelQuery::RtVal> results;
                auto layer = stackupModel->GetLayerIndexByHeight(stackupModel->GetHeight(bw.heights.front()));
                query.SearchNearestPrismInstances(layer, pts.at(curr), 1, results);
                NS_ASSERT(results.size() == 1)
                std::for_each(results.begin(), results.end(), [&](const auto & r) { line.neighbors.front().push_back(r.second); });
            }
            else {
                auto & prevLine = m_.lines.at(m_.lines.size() - 2);
                line.neighbors.front().emplace_back(prevLine.id);
                prevLine.neighbors.back().emplace_back(line.id);
            }
            if (next == pts.size() - 1) {
                Vec<utils::PrismThermalModelQuery::RtVal> results;
                auto layer = stackupModel->GetLayerIndexByHeight(stackupModel->GetHeight(bw.heights.back()));
                query.SearchNearestPrismInstances(layer, pts.at(next), 1, results);
                NS_ASSERT(results.size() == 1)
                std::for_each(results.begin(), results.end(), [&](const auto & r) { line.neighbors.back().push_back(r.second); });
            }
        }
    }
}

Index PrismThermalModel::AddPoint(FCoord3D point)
{
    m_.points.emplace_back(std::move(point));
    return m_.points.size() - 1;
}

FCoord3D PrismThermalModel::GetPoint(Index lyrId, Index elemId, Index vtxId) const
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

bool PrismThermalModel::isPrism(Index index) const
{
    return index < TotalPrismElements();
}

Float PrismThermalModel::CoordScale2Meter(int order) const
{
    return std::pow(m_.scaleH2Unit * m_.scale2Meter, order);
}

Float PrismThermalModel::UnitScale2Meter(int order) const
{
    return std::pow(m_.scale2Meter, order);
}

void PrismThermalModel::SearchElementIndices(const Vec<FCoord3D> & monitors, Vec<Index> & indices) const
{
    indices.resize(monitors.size());
    utils::PrismThermalModelQuery query(*this);
    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto & point = monitors.at(i);
        auto layer = query.NearestLayer(point[2]);
        Vec<typename utils::PrismThermalModelQuery::RtVal> results;
        NCoord2D p(point[0] / m_.scaleH2Unit, point[1] / m_.scaleH2Unit);
        query.SearchNearestPrismInstances(layer, p, 1, results);
        indices[i] = results.front().second;
    }
}
            
template <typename Scalar>
bool PrismThermalModel::WriteVTK(std::string_view filename, const Vec<Scalar> * temperature, std::string * err) const
{
    if (not generic::fs::CreateDir(generic::fs::DirName(filename))) {
        if (err) *err = "Error: fail to create folder " + generic::fs::DirName(filename).string();
        return false;
    }

    std::ofstream out(filename.data());
    if (not out.is_open()){
        if (err) *err = "Error: fail to open: " + std::string(filename);
        return false;
    }

    char sp(32);
    out << "# vtk DataFile Version 2.0" << NS_EOL;
    out << "Unstructured Grid" << NS_EOL;
    out << "ASCII" << NS_EOL;
    out << "DATASET UNSTRUCTURED_GRID" << NS_EOL;
    
    out << "POINTS" << sp << m_.points.size() << sp << "FLOAT" << NS_EOL;
    for(const auto & point : m_.points){
        out << point[0] << sp << point[1] << sp << point[2] << NS_EOL;
    }

    out << NS_EOL; 
    out << "CELLS" << sp << this->TotalElements() << sp << this->TotalPrismElements() * 7 + this->TotalLineElements() * 3 << NS_EOL;
    for (const auto & prism : m_.prisms) {
        out << '6';
        for (auto vertex : prism.vertices)
            out << sp << vertex;
        out << NS_EOL; 
    }
    for (const auto & line : m_.lines) {
        const auto & endPts = line.endPts;
        out << '2' << sp << endPts.front() << sp << endPts.back() << NS_EOL;
    }
    out << NS_EOL;

    out << "CELL_TYPES" << sp << this->TotalElements() << NS_EOL;
    for (size_t i = 0; i < this->TotalPrismElements(); ++i) out << "13" << NS_EOL;
    for (size_t i = 0; i < this->TotalLineElements(); ++i) out << "3" << NS_EOL;

    if (temperature && temperature->size() == this->TotalElements()) {
        out << "CELL_DATA" << sp << this->TotalElements() << NS_EOL;
        out << "SCALARS SCALARS FLOAT 1 " << NS_EOL;
        out << "LOOKUP_TABLE TEMPERATURE" << NS_EOL;
        for (const auto & t : *temperature) out << t << NS_EOL;

        out << NS_EOL;
        out << "LOOKUP_TABLE TEMPERATURE 100" << NS_EOL;
        int r, g, b;
        for (size_t i = 0; i < 100; ++i) {
            generic::color::RGBFromScalar(i * 0.01, r, g, b);
            out << r / 255.0 << sp << g / 255.0 << sp << b / 255.0 << sp << 1.0 << NS_EOL;
        }
    }

    out.close();
    return true;
}

template bool PrismThermalModel::WriteVTK<Float32>(std::string_view filename, const Vec<Float32> * temperature, std::string * err) const;
template bool PrismThermalModel::WriteVTK<Float64>(std::string_view filename, const Vec<Float64> * temperature, std::string * err) const;

} // namespace nano::heat::model