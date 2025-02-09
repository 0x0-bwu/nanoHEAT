#pragma once
#include "basic/NSHeatAlias.hpp"

#include "generic/math/MathUtility.hpp"
#include "generic/circuit/MNA.hpp"
#include <numeric>
namespace nano::heat::solver::network {

template <typename Scalar>
class ThermalNetwork
{
public:
    inline constexpr static Scalar MIN_R = std::numeric_limits<Scalar>::epsilon();
    inline constexpr static Scalar UNKNOWN_T = INVALID_FLOAT;
    struct Node
    {
        Index scen = INVALID_INDEX;
        Scalar t = UNKNOWN_T;//unit: K
        Scalar c = 0;//unit: J/K
        Scalar hf = 0;//unit: W
        Scalar htc = 0;//unit: W/m^2-K
        HashMap<Index, Scalar> ns;//unit: K/W
    };
    
    explicit ThermalNetwork(size_t nodes)
    {
        m_nodes.assign(nodes, Node());
    }

    bool isSource(Index nid) const
    {
        auto & node = m_nodes[nid];
        if (0 != node.hf) return true;
        if (0 != node.htc) return true;
        for (auto & [n, r] : node.ns) {
            if (m_nodes[n].t != UNKNOWN_T)
                return true;
        }
        return false;
    }

    Node  & operator [] (size_t i) { return m_nodes[i]; }
    const Node & operator [] (size_t i) const { return m_nodes[i]; }

    void SetT(Index node, Scalar t) { m_nodes[node].t = t; }
    Scalar GetT(Index ndoe) const { return m_nodes[ndoe].t; }

    void SetHF(Index node, Scalar hf) { m_nodes[node].hf  = hf; }
    void AddHF(Index node, Scalar hf) { m_nodes[node].hf += hf; }
    Scalar GetHF(Index node) const { return m_nodes[node].hf; }

    void SetHTC(Index node, Scalar htc) { m_nodes[node].htc = htc; }
    Scalar GetHTC(Index node) const { return m_nodes[node].htc; }

    void SetC(Index node, Scalar c) { m_nodes[node].c = c; } 
    Scalar GetC(Index node) const { return m_nodes[node].c; }

    void SetScenario(Index node, Index scen) { m_nodes[node].scen = scen; }
    Index GetScenario(Index node) const { return m_nodes[node].scen; }

    void SetR(Index node1, Index node2, Scalar r)
    {
        r = std::max(r, MIN_R);
        if (node1 > node2) std::swap(node1, node2);
        auto & n1 = m_nodes[node1];
        if (auto iter = n1.ns.find(node2); iter != n1.ns.cend())
            iter->second = 1 / (1 / r + 1 / iter->second);
        else n1.ns.emplace(node2, r);
    }

    void BuildIndexMap()
    {
        m_nmMap.clear();
        m_mnMap.clear();
        m_nmMap.reserve(m_nodes.size());
        m_mnMap.reserve(m_nodes.size());
        Index nId{0}, mId{0};
        for (auto & node : m_nodes) {
            if (node.t != UNKNOWN_T) {
                nId++; continue;
            }
            m_nmMap.emplace(nId, mId);
            m_mnMap.emplace(mId, nId);
            mId++; nId++;
        }
    }

    Index NodeId(Index mId) const { return m_mnMap.at(mId); }
    Index MatrixId(Index nId) const { return m_nmMap.at(nId); }

    size_t NodeSize() const { return m_nodes.size(); }
    size_t MatrixSize() const { return m_nmMap.size(); }
    size_t SourceSize() const
    {
        size_t src = 0;
        for (size_t i = 0; i < m_nodes.size(); ++i) {
            if (isSource(i)) src++;
        }
        return src;
    }

private:
    Vec<Node> m_nodes;
    HashMap<Index, Index> m_nmMap;
    HashMap<Index, Index> m_mnMap;
};

using namespace generic::ckt;

template <typename Scalar>
inline DenseVector<Scalar> makeRhs(const ThermalNetwork<Scalar> & network, Scalar refT)
{
    const auto ms = network.MatrixSize();
    const auto ss = network.SourceSize();
    DenseVector<Scalar> rhs(ss);
    for (size_t mid = 0, s = 0; mid < ms; ++mid) {
        auto nid = network.NodeId(mid);
        const auto & node = network[nid];
        if (network.isSource(nid)) {
            rhs[s] = node.hf + node.htc * refT;
            for (const auto & [nnid, r] : node.ns) {
                auto & nnode = network[nnid];
                if (nnode.t != network.UNKNOWN_T)
                    rhs[s] += nnode.t / r;
            }
            s++;
        }
    }
    return rhs;
}

template <typename Scalar>
inline MNA<SparseMatrix<Scalar>> makeMNA(const ThermalNetwork<Scalar> & network, const Vec<Index> & probs = {})
{
    using Matrix = SparseMatrix<Scalar>;
    using Triplets = Vec<Eigen::Triplet<Scalar>>;

    MNA<Matrix> m;
    const size_t ms = network.MatrixSize();
    const size_t ss = network.SourceSize();
    Triplets tG, tC, tB;
    m.G = Matrix(ms, ms);
    m.C = Matrix(ms, ms);
    m.B = Matrix(ms, ss);
    for (size_t mid = 0, s = 0; mid < ms; ++mid) {
        auto nid = network.NodeId(mid);
        const auto & node = network[nid];
        for (const auto & [nnid, r] : node.ns) {
            const auto & nnode = network[nnid];
            if (nnode.t != network.UNKNOWN_T)
                mna::Stamp(tG, mid, 1 / r);
            else {
                auto nmid = network.MatrixId(nnid);
                mna::Stamp(tG, mid, nmid, 1 / r);
            }
        }
        if (node.htc != 0) mna::Stamp(tG, mid, node.htc);
        if (node.c > 0) mna::Stamp(tC, mid, node.c);
        if (network.isSource(nid)) tB.emplace_back(mid, s++, 1);
    }
    m.G.setFromTriplets(tG.begin(), tG.end());
    m.C.setFromTriplets(tC.begin(), tC.end());
    m.B.setFromTriplets(tB.begin(), tB.end());

    if (probs.empty()) {
        m.L = Matrix(ms, ms);
        m.L.setIdentity();
    }
    else {
        size_t j{0};
        Triplets tL;
        for (auto p : probs) {
            NS_ASSERT(network[p].t != network.UNKNOWN_T);
            auto mid = network.MatrixId(p);
            tL.emplace_back(mid, j++, 1);
        }
        m.L = Matrix(ms, probs.size());
        m.L.setFromTriplets(tL.begin(), tL.end());
    }
    return m;
}

} // namespace nano::heat::solver::network