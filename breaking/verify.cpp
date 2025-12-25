#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdio>
#include <queue>
#include <random>
#include <vector>

// Include BMSSP header first to get type definitions
#include "bmssp.h"

// ============ Graph Structure ============
int n, m, s;
int num_edge;
int head[1000005];

struct Edge
{
    Vertex to;
    int weight, next;
} edge[1000005];

void
add_edge(const int& u, const int& v, const int& w)
{
    edge[++num_edge] = {v, w, head[u]};
    head[u] = num_edge;
}

void
reset_graph()
{
    num_edge = 0;
    for (int i = 0; i <= n; i++)
    {
        head[i] = 0;
    }
}

// ============ Classic Dijkstra ============
void
dijkstra(int source, Length dis[])
{
    using P = std::pair<Length, int>;
    std::priority_queue<P, std::vector<P>, std::greater<P>> pq;

    for (int i = 1; i <= n; i++)
    {
        dis[i] = INF;
    }
    dis[source] = 0;
    pq.push({0, source});

    while (!pq.empty())
    {
        auto [d, u] = pq.top();
        pq.pop();

        if (d > dis[u])
        {
            continue;
        }

        for (int ei = head[u]; ei; ei = edge[ei].next)
        {
            Vertex v = edge[ei].to;
            int w = edge[ei].weight;

            if (dis[u] != INF && dis[u] + w < dis[v])
            {
                dis[v] = dis[u] + w;
                pq.push({dis[v], v});
            }
        }
    }
}

// Define BMSSP namespace externs - these reference the global graph state
namespace BMSSP
{
int n;
int k = 1;
int t = 1;
int l = 1;
Length dis[1000005];
int num_edge;
int head[1000005];
Edge edge[1000005];
} // namespace BMSSP

// ============ Random Graph Generator ============
class RandomGraphGenerator
{
  private:
    std::mt19937 rng;

  public:
    RandomGraphGenerator()
        : rng(std::chrono::steady_clock::now().time_since_epoch().count())
    {
    }

    int rand_int(int min_val, int max_val)
    {
        return std::uniform_int_distribution<int>(min_val, max_val)(rng);
    }

    // Generate a random connected graph
    void generate_random_connected(int num_vertices,
                                   int num_edges,
                                   int max_weight,
                                   std::vector<std::tuple<int, int, int>>& out_edges)
    {
        out_edges.clear();

        // First, create a random tree to ensure connectivity
        std::vector<int> perm;
        for (int i = 1; i <= num_vertices; i++)
        {
            perm.push_back(i);
        }
        std::shuffle(perm.begin(), perm.end(), rng);

        // Create tree edges
        for (int i = 1; i < num_vertices; i++)
        {
            int u = perm[rand_int(0, i - 1)];
            int v = perm[i];
            int w = rand_int(1, max_weight);
            out_edges.push_back({u, v, w});
        }

        // Add remaining random edges
        int remaining_edges = num_edges - (num_vertices - 1);
        for (int i = 0; i < remaining_edges; i++)
        {
            int u = rand_int(1, num_vertices);
            int v = rand_int(1, num_vertices);
            int w = rand_int(1, max_weight);
            if (u != v)
            {
                out_edges.push_back({u, v, w});
            }
        }
    }

    // Generate a grid graph
    void generate_grid(int rows,
                       int cols,
                       int max_weight,
                       std::vector<std::tuple<int, int, int>>& out_edges)
    {
        out_edges.clear();

        auto idx = [&](int r, int c) { return r * cols + c + 1; };

        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < cols; c++)
            {
                int u = idx(r, c);
                if (c + 1 < cols)
                {
                    int v = idx(r, c + 1);
                    int w = rand_int(1, max_weight);
                    out_edges.push_back({u, v, w});
                }
                if (r + 1 < rows)
                {
                    int v = idx(r + 1, c);
                    int w = rand_int(1, max_weight);
                    out_edges.push_back({u, v, w});
                }
            }
        }
    }

    // Generate a star graph
    void generate_star(int num_vertices,
                       int max_weight,
                       std::vector<std::tuple<int, int, int>>& out_edges)
    {
        out_edges.clear();
        int center = 1;
        for (int i = 2; i <= num_vertices; i++)
        {
            int w = rand_int(1, max_weight);
            out_edges.push_back({center, i, w});
        }
    }

    // Generate a path graph
    void generate_path(int num_vertices,
                       int max_weight,
                       std::vector<std::tuple<int, int, int>>& out_edges)
    {
        out_edges.clear();
        for (int i = 1; i < num_vertices; i++)
        {
            int w = rand_int(1, max_weight);
            out_edges.push_back({i, i + 1, w});
        }
    }

    // Generate a complete graph (small n)
    void generate_complete(int num_vertices,
                           int max_weight,
                           std::vector<std::tuple<int, int, int>>& out_edges)
    {
        out_edges.clear();
        for (int i = 1; i <= num_vertices; i++)
        {
            for (int j = i + 1; j <= num_vertices; j++)
            {
                int w = rand_int(1, max_weight);
                out_edges.push_back({i, j, w});
            }
        }
    }

    // Generate a tree with random structure
    void generate_random_tree(int num_vertices,
                              int max_weight,
                              std::vector<std::tuple<int, int, int>>& out_edges)
    {
        out_edges.clear();
        for (int i = 2; i <= num_vertices; i++)
        {
            int parent = rand_int(1, i - 1);
            int w = rand_int(1, max_weight);
            out_edges.push_back({parent, i, w});
        }
    }
};

// ============ Verifier ============
class Verifier
{
  private:
    Length dijkstra_dis[1000005];

  public:
    bool verify(int test_case)
    {
        // Run Dijkstra
        dijkstra(s, dijkstra_dis);

        // Setup BMSSP state
        BMSSP::n = n;
        BMSSP::num_edge = num_edge;
        for (int i = 0; i <= n; i++)
        {
            BMSSP::head[i] = head[i];
            BMSSP::dis[i] = INF;
        }
        for (int i = 1; i <= num_edge; i++)
        {
            BMSSP::edge[i].to = edge[i].to;
            BMSSP::edge[i].weight = edge[i].weight;
            BMSSP::edge[i].next = edge[i].next;
        }

        BMSSP::dis[s] = 0;
        BMSSP::ComputeParameters();

        // Run BMSSP
        std::vector<Vertex> S;
        S.push_back(s);
        auto [B_prime, U] = BMSSP::BMSSP(BMSSP::l, INF, S);

        // Compare
        bool passed = true;
        int mismatch_count = 0;

        for (int i = 1; i <= n; i++)
        {
            bool dijk_unreachable = (dijkstra_dis[i] == INF);
            bool break_unreachable = (BMSSP::dis[i] == INF);

            if (dijk_unreachable != break_unreachable)
            {
                if (mismatch_count < 10)
                {
                    printf("  Vertex %d: Dijkstra reachable=%d, BMSSP reachable=%d\n",
                           i,
                           !dijk_unreachable,
                           !break_unreachable);
                }
                passed = false;
                mismatch_count++;
            }
            else if (!dijk_unreachable && dijkstra_dis[i] != BMSSP::dis[i])
            {
                if (mismatch_count < 10)
                {
                    printf("  Vertex %d: Dijkstra=%lld, BMSSP=%lld (diff=%lld)\n",
                           i,
                           dijkstra_dis[i],
                           BMSSP::dis[i],
                           BMSSP::dis[i] - dijkstra_dis[i]);
                }
                passed = false;
                mismatch_count++;
            }
        }

        if (passed)
        {
            printf("PASS Test %d: n=%d, m=%d, s=%d\n", test_case, n, m, s);
        }
        else
        {
            printf("FAIL Test %d: n=%d, m=%d, s=%d (%d mismatches)\n",
                   test_case,
                   n,
                   m,
                   s,
                   mismatch_count);
        }

        return passed;
    }
};

// ============ Test Runner ============
int
main(int argc, char* argv[])
{
    RandomGraphGenerator gen;
    Verifier verifier;

    int num_tests = 100;
    int max_n = 50;
    int max_weight = 100;

    if (argc >= 2)
    {
        num_tests = std::atoi(argv[1]);
    }
    if (argc >= 3)
    {
        max_n = std::atoi(argv[2]);
    }
    if (argc >= 4)
    {
        max_weight = std::atoi(argv[3]);
    }

    printf("========================================\n");
    printf("BMSSP Verification against Dijkstra\n");
    printf("========================================\n");
    printf("Tests: %d, Max n: %d, Max weight: %d\n\n", num_tests, max_n, max_weight);

    int passed = 0;
    int failed = 0;
    int test_num = 0;

    // Define test distribution: (type_name, test_count)
    // Focus on random connected (60%) and complete (20%), fewer boundary cases
    int random_connected_count = num_tests / 2;
    int complete_count = num_tests / 5;
    int boundary_count = num_tests / 20; // for each of grid, star, path, tree

    // Add remaining tests to random connected
    int total_accounted = random_connected_count + complete_count + boundary_count * 4;
    random_connected_count += (num_tests - total_accounted);

    std::vector<std::pair<std::string, int>> test_distribution = {
        {"Random Connected", random_connected_count},
        {"Complete", complete_count},
        {"Grid", boundary_count},
        {"Star", boundary_count},
        {"Path", boundary_count},
        {"Random Tree", boundary_count}};

    for (const auto& [type_name, type_count] : test_distribution)
    {
        if (type_count == 0)
        {
            continue;
        }
        printf("\n--- Testing %s (%d tests) ---\n", type_name.c_str(), type_count);

        for (int i = 0; i < type_count; i++)
        {
            test_num++;
            reset_graph();

            std::vector<std::tuple<int, int, int>> edges;
            int cur_n = 0, cur_m = 0;

            if (type_name == "Random Connected")
            {
                cur_n = 3 + gen.rand_int(0, max_n - 3);
                // Limit edges: use max_n * 10 as a reasonable upper bound instead of n^2/2
                int max_possible = (cur_n * (cur_n - 1) / 2) - (cur_n - 1);
                int max_m = std::min(max_n * 10, max_possible); // Much smaller limit
                cur_m = (cur_n - 1) + gen.rand_int(0, max_m);
                gen.generate_random_connected(cur_n, cur_m, max_weight, edges);
            }
            else if (type_name == "Grid")
            {
                int rows = 2 + gen.rand_int(0, 9);
                int cols = 2 + gen.rand_int(0, 9);
                gen.generate_grid(rows, cols, max_weight, edges);
                cur_n = rows * cols;
                cur_m = edges.size();
            }
            else if (type_name == "Star")
            {
                cur_n = 3 + gen.rand_int(0, max_n - 3);
                gen.generate_star(cur_n, max_weight, edges);
                cur_m = edges.size();
            }
            else if (type_name == "Path")
            {
                cur_n = 3 + gen.rand_int(0, max_n - 3);
                gen.generate_path(cur_n, max_weight, edges);
                cur_m = edges.size();
            }
            else if (type_name == "Complete")
            {
                cur_n = 3 + gen.rand_int(0, std::min(15, max_n - 3));
                gen.generate_complete(cur_n, max_weight, edges);
                cur_m = edges.size();
            }
            else if (type_name == "Random Tree")
            {
                cur_n = 3 + gen.rand_int(0, max_n - 3);
                gen.generate_random_tree(cur_n, max_weight, edges);
                cur_m = edges.size();
            }

            n = cur_n;
            m = cur_m;
            s = gen.rand_int(1, n);

            for (const auto& [u, v, w] : edges)
            {
                add_edge(u, v, w);
            }

            if (verifier.verify(test_num))
            {
                passed++;
            }
            else
            {
                failed++;
                // Print graph for debugging
                printf("  Graph type: %s\n", type_name.c_str());
                printf("  Graph (first 20 edges):\n");
                int count = 0;
                for (const auto& [u, v, w] : edges)
                {
                    if (count++ >= 20)
                    {
                        break;
                    }
                    printf("    %d -> %d (w=%d)\n", u, v, w);
                }
                if (edges.size() > 20)
                {
                    printf("    ... (%d more edges)\n", (int)edges.size() - 20);
                }
                break;
            }
        }
    }

    printf("\n========================================\n");
    printf("Summary: %d passed, %d failed\n", passed, failed);
    printf("========================================\n");

    return failed > 0 ? 1 : 0;
}
