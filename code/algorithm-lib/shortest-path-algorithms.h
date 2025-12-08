#ifndef SHORTEST_PATH_ALGORITHMS_H
#define SHORTEST_PATH_ALGORITHMS_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include <vector>
#include <map>
#include <queue>
#include <limits>
#include <chrono>
#include <string>

namespace ns3 {

/**
 * \brief Graph structure for shortest path algorithms
 */
struct GraphEdge {
    uint32_t from;
    uint32_t to;
    double weight;

    GraphEdge(uint32_t f, uint32_t t, double w) : from(f), to(t), weight(w) {}
};

class Graph {
public:
    void AddEdge(uint32_t from, uint32_t to, double weight);
    std::vector<GraphEdge> GetEdges() const;
    std::vector<uint32_t> GetNeighbors(uint32_t node) const;
    uint32_t GetNodeCount() const;

private:
    std::vector<GraphEdge> m_edges;
    std::map<uint32_t, std::vector<GraphEdge>> m_adjacencyList;
    uint32_t m_nodeCount = 0;
};

/**
 * \brief Algorithm performance metrics
 */
struct PerformanceMetrics {
    std::string algorithmName;
    uint32_t nodeCount;
    uint32_t edgeCount;
    double executionTimeMs;
    uint32_t memoryUsageKB;
    bool correctResult;

    void Print() const;
};

/**
 * \brief Abstract base class for shortest path algorithms
 */
class ShortestPathAlgorithm {
public:
    virtual ~ShortestPathAlgorithm() = default;
    virtual PerformanceMetrics RunShortestPath(const Graph& graph, uint32_t source) = 0;
    virtual std::string GetAlgorithmName() const = 0;
    virtual std::vector<double> GetDistances() const = 0;
    virtual std::vector<uint32_t> GetPredecessors() const = 0;
};

/**
 * \brief Dijkstra's algorithm implementation
 */
class DijkstraAlgorithm : public ShortestPathAlgorithm {
public:
    PerformanceMetrics RunShortestPath(const Graph& graph, uint32_t source) override;
    std::string GetAlgorithmName() const override { return "Dijkstra"; }
    std::vector<double> GetDistances() const override { return m_distances; }
    std::vector<uint32_t> GetPredecessors() const override { return m_predecessors; }

private:
    std::vector<double> m_distances;
    std::vector<uint32_t> m_predecessors;
};

/**
 * \brief Breaking the Sorting Barrier algorithm implementation
 */
class BreakingSortingBarrierAlgorithm : public ShortestPathAlgorithm {
public:
    PerformanceMetrics RunShortestPath(const Graph& graph, uint32_t source) override;
    std::string GetAlgorithmName() const override { return "BreakingSortingBarrier"; }
    std::vector<double> GetDistances() const override { return m_distances; }
    std::vector<uint32_t> GetPredecessors() const override { return m_predecessors; }

private:
    std::vector<double> m_distances;
    std::vector<uint32_t> m_predecessors;

    // Helper methods for the barrier-breaking algorithm
    std::vector<uint32_t> GetBucketRange(const Graph& graph, const std::vector<double>& distances, uint32_t source);
    void ProcessBucket(const Graph& graph, uint32_t bucketStart, uint32_t bucketEnd,
                      std::vector<double>& distances, std::vector<uint32_t>& predecessors);
};

/**
 * \brief Algorithm performance tester
 */
class AlgorithmTester {
public:
    struct TestConfiguration {
        uint32_t minNodes = 10;
        uint32_t maxNodes = 1000;
        uint32_t stepSize = 50;
        uint32_t repetitions = 5;
        double edgeDensity = 0.2; // Probability of edge between any two nodes
    };

    std::vector<PerformanceMetrics> RunPerformanceTest(const TestConfiguration& config);
    void PrintComparison(const std::vector<PerformanceMetrics>& results);
    void ExportResults(const std::vector<PerformanceMetrics>& results, const std::string& filename);

private:
    Graph GenerateRandomGraph(uint32_t nodeCount, double edgeDensity, uint32_t seed = 0);
    bool ValidateResults(const std::vector<double>& dijkstraDist, const std::vector<double>& bsbDist);
    double MeasureMemoryUsage();
};

/**
 * \brief Data analysis and visualization utilities
 */
class DataAnalyzer {
public:
    /**
     * \brief Generate performance comparison plots
     */
    static void GeneratePlots(const std::vector<PerformanceMetrics>& results);

    /**
     * \brief Analyze scalability characteristics
     */
    static void AnalyzeScalability(const std::vector<PerformanceMetrics>& results);

    /**
     * \brief Generate statistical summary
     */
    static void GenerateStatisticalSummary(const std::vector<PerformanceMetrics>& results);

    /**
     * \brief Export results to multiple formats
     */
    static void ExportMultipleFormats(const std::vector<PerformanceMetrics>& results);

private:
    static std::map<std::string, std::vector<PerformanceMetrics>> GroupByAlgorithm(
        const std::vector<PerformanceMetrics>& results);

    static double CalculateMean(const std::vector<double>& values);
    static double CalculateStdDev(const std::vector<double>& values);
    static double CalculateSpeedup(double dijkstraTime, double bsbTime);
};

} // namespace ns3

#endif /* SHORTEST_PATH_ALGORITHMS_H */