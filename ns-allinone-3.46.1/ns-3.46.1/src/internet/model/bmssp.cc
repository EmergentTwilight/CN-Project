#include "bmssp.h"

#include "ns3/log.h"

#include <cstdio>
#include <queue>
#include <stack>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("BmsspSolver");

// =========================================================
// BmsspSolver 类实现
// =========================================================

// 构造函数
BmsspSolver::BmsspSolver() : num_edge(0), m_source(-1)
{
}

BmsspSolver::~BmsspSolver() 
{
}

// 初始化
void 
BmsspSolver::Init(int num_nodes)
{
    n = num_nodes;
    // 重置数组大小，模拟原来的全局数组
    // 假设节点编号 0 ~ n-1，但如果不确定是否 1-based，多开一点空间没坏处
    head.assign(n + 2, 0); 
    edge.clear();
    edge.push_back({0,0,0}); // 0号边作为 dummy，让下标从1开始，符合链式前向星习惯
    num_edge = 0;
    
    dis.assign(n + 2, BMSSP_INF);
    
    // 初始化 parent 数组，-1 表示未访问
    parent.assign(n + 2, -1);
    
    ComputeParameters();
}

void 
BmsspSolver::AddEdge(int u, int v, int weight)
{
    // 链式前向星加边
    // 注意检查 u 的范围防止越界
    if (u >= (int)head.size()) head.resize(u + 10, 0);
    
    edge.push_back({v, weight, head[u]});
    num_edge++; // 如果 edge 是 vector，num_edge 其实就是 edge.size()-1
    head[u] = num_edge;
}

BmsspLength 
BmsspSolver::GetDistance(int node)
{
    if (node >= 0 && node < (int)dis.size()) return dis[node];
    return BMSSP_INF;
}

// 获取下一跳节点ID：从 target 回溯到 source 的直连子节点
int
BmsspSolver::GetNextHop(int target)
{
    if (target < 0 || target >= (int)parent.size())
    {
        return -1;
    }
    
    // 如果 target 就是 source，返回 -1（不应该查询 source 的下一跳）
    if (target == m_source)
    {
        return -1;
    }
    
    // 如果 target 不可达，返回 -1
    if (parent[target] == -1)
    {
        return -1;
    }
    
    // 从 target 开始回溯，直到找到 source 的直连子节点
    // 即找到第一个 parent 为 source 的节点
    int current = target;
    
    // 如果 target 的 parent 直接就是 source，返回 target
    if (parent[target] == m_source)
    {
        return target;
    }
    
    // 回溯直到找到 source 的直连子节点
    while (current != m_source && parent[current] != -1)
    {
        int next = parent[current];
        
        // 防止循环（虽然理论上不应该发生）
        if (next == target || next == current)
        {
            return -1;
        }
        
        // 如果 next 的 parent 是 source，那么 current 就是 source 的直连子节点
        if (next == m_source)
        {
            return current;
        }
        
        current = next;
    }
    
    return -1;
}

void
BmsspSolver::ComputeParameters()
{
    if (n <= 0) return;
    double logn = std::log2(n);
    k = std::max(1, (int)std::floor(std::pow(logn, 1.0 / 3.0)));
    t = std::max(1, (int)std::floor(std::pow(logn, 2.0 / 3.0)));
    l = std::max(1, (int)std::ceil(logn / (t > 0 ? t : 1)));
}

// FindPivots(B, S) - Algorithm 1 in the paper
std::pair<BmsspSolver::VertexSet, BmsspSolver::VertexSet>
BmsspSolver::FindPivots(BmsspLength B, const VertexSet& S)
{
    VertexSet W;
    VertexSet W_curr;
    std::unordered_set<int> inW;

    // W_0 = S
    for (int v : S)
    {
        W.push_back(v);
        W_curr.push_back(v);
        inW.insert(v);
    }

    // Run k iterations of Bellman-Ford-like relaxation
    for (int iter = 1; iter <= k; iter++)
    {
        VertexSet W_next;

        // Relax all edges from vertices in W_curr
        for (int u : W_curr)
        {
            for (int ei = head[u]; ei; ei = edge[ei].next)
            {
                int v = edge[ei].to;
                int w = edge[ei].weight;

                // Note: using <= for the condition (as in Remark 3.4)
                // But only update if strictly improving
                if (dis[u] != BMSSP_INF && dis[u] + w <= dis[v])
                {
                    BmsspLength newDist = dis[u] + w;
                    if (newDist < dis[v])
                    {
                        dis[v] = newDist;
                        // 更新路径追踪：v 的前驱是 u
                        parent[v] = u;
                    }
                    if (newDist < B && inW.find(v) == inW.end())
                    {
                        W_next.push_back(v);
                        inW.insert(v);
                    }
                }
            }
        }

        // Add W_next to W
        for (int v : W_next)
        {
            W.push_back(v);
        }

        W_curr = W_next;

        // Early exit if |W| > k|S|
        if ((int)W.size() > k * (int)S.size())
        {
            // Return P = S, W as is
            return {S, W};
        }
    }

    // |W| <= k|S|, compute pivots from the forest F
    // F = {(u,v) in E : u,v in W, dis[v] = dis[u] + w_uv}
    // Find roots of trees with >= k vertices

    // Build the forest and compute tree sizes
    std::unordered_map<int, std::vector<int>> children;
    std::unordered_map<int, int> parent;
    std::unordered_set<int> inW_set(W.begin(), W.end());

    for (int u : W)
    {
        for (int ei = head[u]; ei; ei = edge[ei].next)
        {
            int v = edge[ei].to;
            int w = edge[ei].weight;

            if (inW_set.count(v) && dis[v] == dis[u] + w)
            {
                // Edge (u,v) is in the forest
                // Assuming u is parent of v (shortest path tree direction)
                // For simplicity, we add v as child of u
                children[u].push_back(v);
                parent[v] = u;
            }
        }
    }

    // Find roots (vertices with no parent in W)
    VertexSet roots;
    for (int v : W)
    {
        if (parent.find(v) == parent.end())
        {
            roots.push_back(v);
        }
    }

    // Compute tree sizes using DFS
    std::unordered_map<int, int> treeSize;

    std::function<int(int)> dfs = [&](int u) -> int {
        int size = 1;
        for (int v : children[u])
        {
            size += dfs(v);
        }
        treeSize[u] = size;
        return size;
    };

    for (int r : roots)
    {
        dfs(r);
    }

    // Collect pivots: roots with tree size >= k
    VertexSet P;
    std::unordered_set<int> inS(S.begin(), S.end());

    for (int r : roots)
    {
        if (inS.count(r) && treeSize[r] >= k)
        {
            P.push_back(r);
        }
    }

    return {P, W};
}

// BaseCase(B, S) - Algorithm 2 in the paper
// S = {x} is a singleton, and x is complete
std::pair<BmsspLength, BmsspSolver::VertexSet>
BmsspSolver::BaseCase(BmsspLength B, const VertexSet& S)
{
    VertexSet U;

    if (S.empty())
    {
        return {B, U};
    }

    int x = S[0];

    // Binary heap (priority queue) for Dijkstra
    using P = std::pair<BmsspLength, int>;
    std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
    std::unordered_set<int> inHeap;

    pq.push({dis[x], x});
    inHeap.insert(x);

    while (!pq.empty())
    {
        auto [d, u] = pq.top();
        pq.pop();

        // Skip if this is an outdated entry
        if (d != dis[u])
        {
            continue;
        }

        // Add u to U if not already there
        bool alreadyInU = false;
        for (int v : U)
        {
            if (v == u)
            {
                alreadyInU = true;
                break;
            }
        }
        if (!alreadyInU)
        {
            U.push_back(u);
        }

        // Relax edges (per Algorithm 2, line 8)
        // Condition: d[u] + w <= d[v] and d[u] + w < B
        for (int ei = head[u]; ei; ei = edge[ei].next)
        {
            int v = edge[ei].to;
            int w = edge[ei].weight;

            if (dis[u] != BMSSP_INF && dis[u] + w <= dis[v] && dis[u] + w < B)
            {
                BmsspLength newDist = dis[u] + w;
                if (newDist < dis[v])
                {
                    dis[v] = newDist;
                    // 更新路径追踪：v 的前驱是 u
                    parent[v] = u;
                }
                if (inHeap.find(v) == inHeap.end())
                {
                    pq.push({dis[v], v});
                    inHeap.insert(v);
                }
                else
                {
                    // DecreaseKey - push new entry
                    pq.push({dis[v], v});
                }
            }
        }
    }

    // Determine return values based on paper:
    // If |U| <= k, return B' = B, U = U
    // Otherwise (|U| >= k+1), return B' = max_{v in U} d[v], U = {v in U : d[v] < B'}
    if ((int)U.size() <= k)
    {
        return {B, U};
    }

    // Find B' = max_{v in U} dis[v]
    BmsspLength B_prime = 0;
    for (int v : U)
    {
        if (dis[v] > B_prime)
        {
            B_prime = dis[v];
        }
    }

    // U = {v in U : dis[v] < B'}
    VertexSet result;
    for (int v : U)
    {
        if (dis[v] < B_prime)
        {
            result.push_back(v);
        }
    }

    return {B_prime, result};
}

// BMSSP(l, B, S) - Algorithm 3 in the paper
std::pair<BmsspLength, BmsspSolver::VertexSet>
BmsspSolver::BMSSP(int level, BmsspLength B, const VertexSet& S)
{
    VertexSet U;

    // Base case: l = 0
    if (level == 0)
    {
        return BaseCase(B, S);
    }

    // Step 1: Find pivots
    auto [P, W] = FindPivots(B, S);

    // Step 2: Initialize data structure D
    int M = 1 << ((level - 1) * t); // M = 2^{(l-1)*t}
    int N = k * (1 << (level * t)); // Expected insertions: O(k * 2^{l*t})
    BlockHeap::BlockHeapDS<int, BmsspLength> D(M, B, N);
    D.Initialize();

    // Insert each pivot x with value dis[x]
    for (int x : P)
    {
        D.Insert(x, dis[x]);
    }

    // Step 3: Initialize loop variables
    BmsspLength B_prev = B;
    for (int x : P)
    {
        if (dis[x] < B_prev)
        {
            B_prev = dis[x];
        }
    }

    int i = 0;
    BmsspLength B_prime_0 = B_prev;
    BmsspLength B_prime_i = B_prime_0;
    U.clear();

    // Step 4: Main loop
    while ((int)U.size() < k * (1 << (level * t)) && !D.IsEmpty())
    {
        i++;

        // Pull from D
        auto [S_i, B_i] = D.Pull();

        // Recursive call
        auto [B_prime_i_curr, U_i] = BMSSP(level - 1, B_i, S_i);

        B_prime_i = B_prime_i_curr;

        // Add U_i to U
        for (int v : U_i)
        {
            U.push_back(v);
        }

        // Relax edges from U_i
        std::vector<std::pair<int, BmsspLength>> K; // For batch prepend

        for (int u : U_i)
        {
            for (int ei = head[u]; ei; ei = edge[ei].next)
            {
                int v = edge[ei].to;
                int w = edge[ei].weight;

                // Note: using <= for the condition (as in Remark 3.4)
                if (dis[u] != BMSSP_INF && dis[u] + w <= dis[v])
                {
                    BmsspLength newDist = dis[u] + w;

                    if (newDist < dis[v])
                    {
                        dis[v] = newDist;
                        // 更新路径追踪：v 的前驱是 u
                        parent[v] = u;
                    }

                    if (newDist < B)
                    {
                        if (newDist >= B_i)
                        {
                            // Case (a): insert directly
                            D.Insert(v, newDist);
                        }
                        else if (newDist >= B_prime_i)
                        {
                            // Case (b): add to K for batch prepend
                            K.push_back({v, newDist});
                        }
                    }
                }
            }
        }

        // Batch prepend
        // Also include <x, dis[x]> for x in S_i with dis[x] in [B'_i, B_i)
        std::vector<std::pair<int, BmsspLength>> batchPrependList = K;

        for (int x : S_i)
        {
            if (dis[x] >= B_prime_i && dis[x] < B_i)
            {
                batchPrependList.push_back({x, dis[x]});
            }
        }

        if (!batchPrependList.empty())
        {
            D.BatchPrepend(batchPrependList);
        }

        // Check for partial execution
        if ((int)U.size() > k * (1 << (level * t)))
        {
            // Partial execution - return early
            B_prime_i = B_prime_i_curr;

            // Add vertices from W with dis < B_prime_i to U
            for (int w : W)
            {
                if (dis[w] < B_prime_i)
                {
                    bool alreadyInU = false;
                    for (int u : U)
                    {
                        if (u == w)
                        {
                            alreadyInU = true;
                            break;
                        }
                    }
                    if (!alreadyInU)
                    {
                        U.push_back(w);
                    }
                }
            }

            return {B_prime_i, U};
        }
    }

    // Successful execution
    BmsspLength B_final = B_prime_i;

    // Add vertices from W with dis < B_final to U
    for (int w : W)
    {
        if (dis[w] < B_final)
        {
            bool alreadyInU = false;
            for (int u : U)
            {
                if (u == w)
                {
                    alreadyInU = true;
                    break;
                }
            }
            if (!alreadyInU)
            {
                U.push_back(w);
            }
        }
    }

    return {B_final, U};
}

// Run 函数 - 启动算法
void 
BmsspSolver::Run(int sourceNode)
{
    NS_LOG_FUNCTION(this << sourceNode);

    // 保存 source 节点
    m_source = sourceNode;

    // 初始化距离
    std::fill(dis.begin(), dis.end(), BMSSP_INF);
    dis[sourceNode] = 0;
    
    // 初始化 parent 数组
    std::fill(parent.begin(), parent.end(), -1);
    parent[sourceNode] = sourceNode; // 根节点的 parent 是自己

    VertexSet S;
    S.push_back(sourceNode);

    // 启动算法
    BMSSP(l, BMSSP_INF, S);
    
    // 日志输出已移至详细日志模式，减少刷屏
    NS_LOG_LOGIC("BMSSP Finished. Dist to some node: " << dis[0]);
}

} // namespace ns3
