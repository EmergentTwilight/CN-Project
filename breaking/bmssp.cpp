#include "bmssp.h"

#include <cstdio>
#include <queue>
#include <stack>
#include <unordered_set>

// ============ BMSSP Algorithm Implementation ============

namespace BMSSP
{

// Helper function to compute k, t, l from n
void
ComputeParameters()
{
    double logn = std::log2(n);
    k = std::max(1, (int)std::floor(std::pow(logn, 1.0 / 3.0)));
    t = std::max(1, (int)std::floor(std::pow(logn, 2.0 / 3.0)));
    l = std::max(1, (int)std::ceil(logn / t));
}

// FindPivots(B, S) - Algorithm 1 in the paper
std::pair<VertexSet, VertexSet>
FindPivots(Length B, const VertexSet& S)
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
                if (dis[u] != INF && dis[u] + w <= dis[v])
                {
                    Length newDist = dis[u] + w;
                    if (newDist < dis[v])
                    {
                        dis[v] = newDist;
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
std::pair<Length, VertexSet>
BaseCase(Length B, const VertexSet& S)
{
    VertexSet U;

    if (S.empty())
    {
        return {B, U};
    }

    int x = S[0];

    // Binary heap (priority queue) for Dijkstra
    using P = std::pair<Length, int>;
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

            if (dis[u] != INF && dis[u] + w <= dis[v] && dis[u] + w < B)
            {
                dis[v] = dis[u] + w;
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
    Length B_prime = 0;
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
std::pair<Length, VertexSet>
BMSSP(int level, Length B, const VertexSet& S)
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
    BlockHeap::BlockHeapDS<int, Length> D(M, B, N);
    D.Initialize();

    // Insert each pivot x with value dis[x]
    for (int x : P)
    {
        D.Insert(x, dis[x]);
    }

    // Step 3: Initialize loop variables
    Length B_prev = B;
    for (int x : P)
    {
        if (dis[x] < B_prev)
        {
            B_prev = dis[x];
        }
    }

    int i = 0;
    Length B_prime_0 = B_prev;
    Length B_prime_i = B_prime_0;
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
        std::vector<std::pair<int, Length>> K; // For batch prepend

        for (int u : U_i)
        {
            for (int ei = head[u]; ei; ei = edge[ei].next)
            {
                int v = edge[ei].to;
                int w = edge[ei].weight;

                // Note: using <= for the condition (as in Remark 3.4)
                if (dis[u] != INF && dis[u] + w <= dis[v])
                {
                    Length newDist = dis[u] + w;

                    if (newDist < dis[v])
                    {
                        dis[v] = newDist;
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
        std::vector<std::pair<int, Length>> batchPrependList = K;

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
    Length B_final = B_prime_i;

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

} // namespace BMSSP
