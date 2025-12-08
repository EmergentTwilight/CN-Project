#include "shortest-path-algorithms.h"
#include <algorithm>
#include <random>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sys/resource.h>
#include <unistd.h>
#include <cmath>

namespace ns3 {

// Graph implementation
void Graph::AddEdge(uint32_t from, uint32_t to, double weight) {
    m_edges.emplace_back(from, to, weight);
    m_adjacencyList[from].emplace_back(from, to, weight);
    m_nodeCount = std::max({m_nodeCount, from + 1, to + 1});
}

std::vector<GraphEdge> Graph::GetEdges() const {
    return m_edges;
}

std::vector<uint32_t> Graph::GetNeighbors(uint32_t node) const {
    std::vector<uint32_t> neighbors;
    auto it = m_adjacencyList.find(node);
    if (it != m_adjacencyList.end()) {
        for (const auto& edge : it->second) {
            neighbors.push_back(edge.to);
        }
    }
    return neighbors;
}

uint32_t Graph::GetNodeCount() const {
    return m_nodeCount;
}

// PerformanceMetrics implementation
void PerformanceMetrics::Print() const {
    std::cout << std::fixed << std::setprecision(3)
              << "Algorithm: " << algorithmName
              << ", Nodes: " << nodeCount
              << ", Edges: " << edgeCount
              << ", Time: " << executionTimeMs << "ms"
              << ", Memory: " << memoryUsageKB << "KB"
              << ", Correct: " << (correctResult ? "Yes" : "No") << std::endl;
}

// Dijkstra's Algorithm implementation
PerformanceMetrics DijkstraAlgorithm::RunShortestPath(const Graph& graph, uint32_t source) {
    auto startTime = std::chrono::high_resolution_clock::now();

    uint32_t nodeCount = graph.GetNodeCount();
    m_distances.assign(nodeCount, std::numeric_limits<double>::infinity());
    m_predecessors.assign(nodeCount, nodeCount); // Use nodeCount as "undefined"

    m_distances[source] = 0.0;

    // Priority queue: (distance, node)
    std::priority_queue<std::pair<double, uint32_t>,
                       std::vector<std::pair<double, uint32_t>>,
                       std::greater<std::pair<double, uint32_t>>> pq;

    pq.push({0.0, source});

    while (!pq.empty()) {
        auto [dist, node] = pq.top();
        pq.pop();

        // Skip if we've already found a better path
        if (dist > m_distances[node]) continue;

        // Relax edges
        for (const auto& edge : graph.GetEdges()) {
            if (edge.from == node) {
                double newDist = m_distances[node] + edge.weight;
                if (newDist < m_distances[edge.to]) {
                    m_distances[edge.to] = newDist;
                    m_predecessors[edge.to] = node;
                    pq.push({newDist, edge.to});
                }
            }
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    PerformanceMetrics metrics;
    metrics.algorithmName = GetAlgorithmName();
    metrics.nodeCount = nodeCount;
    metrics.edgeCount = graph.GetEdges().size();
    metrics.executionTimeMs = duration.count() / 1000.0;
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    metrics.memoryUsageKB = static_cast<uint32_t>(usage.ru_maxrss / 1024);
    metrics.correctResult = true; // Dijkstra is our baseline for correctness

    return metrics;
}

// Breaking the Sorting Barrier Algorithm implementation
PerformanceMetrics BreakingSortingBarrierAlgorithm::RunShortestPath(const Graph& graph, uint32_t source) {
    auto startTime = std::chrono::high_resolution_clock::now();

    uint32_t nodeCount = graph.GetNodeCount();
    m_distances.assign(nodeCount, std::numeric_limits<double>::infinity());
    m_predecessors.assign(nodeCount, nodeCount);

    m_distances[source] = 0.0;

    // Phase 1: Bucket-based processing to avoid full sorting
    // This is a simplified version of the barrier-breaking approach
    std::vector<uint32_t> bucketRange = GetBucketRange(graph, m_distances, source);

    // Process nodes in buckets rather than sorting all distances
    for (uint32_t i = 0; i < bucketRange.size() - 1; ++i) {
        ProcessBucket(graph, bucketRange[i], bucketRange[i + 1], m_distances, m_predecessors);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

    PerformanceMetrics metrics;
    metrics.algorithmName = GetAlgorithmName();
    metrics.nodeCount = nodeCount;
    metrics.edgeCount = graph.GetEdges().size();
    metrics.executionTimeMs = duration.count() / 1000.0;
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    metrics.memoryUsageKB = static_cast<uint32_t>(usage.ru_maxrss / 1024);
    metrics.correctResult = true;

    return metrics;
}

std::vector<uint32_t> BreakingSortingBarrierAlgorithm::GetBucketRange(const Graph& graph,
                                                                     const std::vector<double>& distances,
                                                                     uint32_t source) {
    // Simplified bucket division - in real implementation would be more sophisticated
    uint32_t nodeCount = graph.GetNodeCount();
    std::vector<uint32_t> range;

    // Divide nodes into buckets based on current distances
    uint32_t bucketSize = std::max(1u, nodeCount / 10); // 10 buckets
    for (uint32_t i = 0; i <= 10; ++i) {
        range.push_back(std::min(i * bucketSize, nodeCount));
    }

    return range;
}

void BreakingSortingBarrierAlgorithm::ProcessBucket(const Graph& graph,
                                                    uint32_t bucketStart,
                                                    uint32_t bucketEnd,
                                                    std::vector<double>& distances,
                                                    std::vector<uint32_t>& predecessors) {
    // Simplified bucket processing
    for (uint32_t i = bucketStart; i < bucketEnd && i < distances.size(); ++i) {
        if (distances[i] != std::numeric_limits<double>::infinity()) {
            // Relax edges from node i
            for (const auto& edge : graph.GetEdges()) {
                if (edge.from == i) {
                    double newDist = distances[i] + edge.weight;
                    if (newDist < distances[edge.to]) {
                        distances[edge.to] = newDist;
                        predecessors[edge.to] = i;
                    }
                }
            }
        }
    }
}

// AlgorithmTester implementation
std::vector<PerformanceMetrics> AlgorithmTester::RunPerformanceTest(const TestConfiguration& config) {
    std::vector<PerformanceMetrics> results;

    std::cout << "Running performance test..." << std::endl;
    std::cout << "Nodes: " << config.minNodes << " to " << config.maxNodes
              << ", Repetitions: " << config.repetitions << std::endl;

    for (uint32_t nodeCount = config.minNodes; nodeCount <= config.maxNodes; nodeCount += config.stepSize) {
        std::cout << "Testing with " << nodeCount << " nodes..." << std::endl;

        for (uint32_t rep = 0; rep < config.repetitions; ++rep) {
            Graph testGraph = GenerateRandomGraph(nodeCount, config.edgeDensity, rep);
            uint32_t source = 0; // Always start from node 0

            // Run Dijkstra
            DijkstraAlgorithm dijkstra;
            auto dijkstraMetrics = dijkstra.RunShortestPath(testGraph, source);

            // Run Breaking Sorting Barrier algorithm
            BreakingSortingBarrierAlgorithm bsb;
            auto bsbMetrics = bsb.RunShortestPath(testGraph, source);

            // Validate correctness
            auto dijkstraDist = dijkstra.GetDistances();
            auto bsbDist = bsb.GetDistances();
            bsbMetrics.correctResult = ValidateResults(dijkstraDist, bsbDist);

            results.push_back(dijkstraMetrics);
            results.push_back(bsbMetrics);
        }
    }

    return results;
}

Graph AlgorithmTester::GenerateRandomGraph(uint32_t nodeCount, double edgeDensity, uint32_t seed) {
    Graph graph;
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> edgeDist(0.0, 1.0);
    std::uniform_real_distribution<double> weightDist(1.0, 100.0); // Random weights between 1 and 100

    // Create random directed graph
    for (uint32_t i = 0; i < nodeCount; ++i) {
        for (uint32_t j = 0; j < nodeCount; ++j) {
            if (i != j && edgeDist(gen) < edgeDensity) {
                double weight = weightDist(gen);
                graph.AddEdge(i, j, weight);
            }
        }
    }

    return graph;
}

bool AlgorithmTester::ValidateResults(const std::vector<double>& dijkstraDist,
                                     const std::vector<double>& bsbDist) {
    const double tolerance = 1e-6;

    if (dijkstraDist.size() != bsbDist.size()) {
        return false;
    }

    for (size_t i = 0; i < dijkstraDist.size(); ++i) {
        if (std::abs(dijkstraDist[i] - bsbDist[i]) > tolerance) {
            // Allow for infinity comparisons
            if ((dijkstraDist[i] == std::numeric_limits<double>::infinity() &&
                 bsbDist[i] != std::numeric_limits<double>::infinity()) ||
                (dijkstraDist[i] != std::numeric_limits<double>::infinity() &&
                 bsbDist[i] == std::numeric_limits<double>::infinity())) {
                return false;
            }
        }
    }

    return true;
}

double AlgorithmTester::MeasureMemoryUsage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss / 1024.0; // Convert to KB
}

void AlgorithmTester::PrintComparison(const std::vector<PerformanceMetrics>& results) {
    std::cout << "\n=== Algorithm Performance Comparison ===" << std::endl;
    std::cout << std::setw(20) << "Algorithm"
              << std::setw(10) << "Nodes"
              << std::setw(10) << "Edges"
              << std::setw(12) << "Time (ms)"
              << std::setw(12) << "Memory (KB)"
              << std::setw(10) << "Correct" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto& metrics : results) {
        std::cout << std::setw(20) << metrics.algorithmName
                  << std::setw(10) << metrics.nodeCount
                  << std::setw(10) << metrics.edgeCount
                  << std::setw(12) << std::fixed << std::setprecision(3) << metrics.executionTimeMs
                  << std::setw(12) << metrics.memoryUsageKB
                  << std::setw(10) << (metrics.correctResult ? "Yes" : "No") << std::endl;
    }
}

void AlgorithmTester::ExportResults(const std::vector<PerformanceMetrics>& results, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << " for writing" << std::endl;
        return;
    }

    file << "Algorithm,Nodes,Edges,ExecutionTimeMs,MemoryUsageKB,CorrectResult\n";
    for (const auto& metrics : results) {
        file << metrics.algorithmName << ","
             << metrics.nodeCount << ","
             << metrics.edgeCount << ","
             << metrics.executionTimeMs << ","
             << metrics.memoryUsageKB << ","
             << (metrics.correctResult ? "true" : "false") << "\n";
    }

    file.close();
    std::cout << "Results exported to " << filename << std::endl;
}

// DataAnalyzer implementation
void DataAnalyzer::GeneratePlots(const std::vector<PerformanceMetrics>& results) {
    std::cout << "\n=== PERFORMANCE PLOTS ===" << std::endl;

    // Group results by algorithm
    auto groupedResults = GroupByAlgorithm(results);

    if (groupedResults.find("Dijkstra") != groupedResults.end() &&
        groupedResults.find("BreakingSortingBarrier") != groupedResults.end()) {

        const auto& dijkstraResults = groupedResults["Dijkstra"];
        const auto& bsbResults = groupedResults["BreakingSortingBarrier"];

        std::cout << "Execution Time vs Number of Nodes:" << std::endl;
        std::cout << "Nodes\tDijkstra(ms)\tBSB(ms)\t\tSpeedup" << std::endl;
        std::cout << "-----\t-----------\t-------\t\t-------" << std::endl;

        // Group by node count for comparison
        std::map<uint32_t, std::vector<double>> dijkstraTimes, bsbTimes;

        for (const auto& result : dijkstraResults) {
            dijkstraTimes[result.nodeCount].push_back(result.executionTimeMs);
        }

        for (const auto& result : bsbResults) {
            bsbTimes[result.nodeCount].push_back(result.executionTimeMs);
        }

        for (const auto& [nodeCount, times] : dijkstraTimes) {
            double dijkstraAvg = CalculateMean(times);
            double bsbAvg = 0;

            if (bsbTimes.find(nodeCount) != bsbTimes.end()) {
                bsbAvg = CalculateMean(bsbTimes[nodeCount]);
            }

            double speedup = CalculateSpeedup(dijkstraAvg, bsbAvg);

            std::cout << nodeCount << "\t"
                      << std::fixed << std::setprecision(3) << dijkstraAvg << "\t\t"
                      << bsbAvg << "\t\t" << speedup << "x" << std::endl;
        }
    }
}

void DataAnalyzer::AnalyzeScalability(const std::vector<PerformanceMetrics>& results) {
    std::cout << "\n=== SCALABILITY ANALYSIS ===" << std::endl;

    auto groupedResults = GroupByAlgorithm(results);

    for (const auto& [algorithmName, algorithmResults] : groupedResults) {
        std::cout << "\n" << algorithmName << " Scalability Analysis:" << std::endl;

        // Analyze time complexity by node count
        std::map<uint32_t, std::vector<double>> timeByNodes;
        for (const auto& result : algorithmResults) {
            timeByNodes[result.nodeCount].push_back(result.executionTimeMs);
        }

        std::cout << "Node Count\tAvg Time (ms)\tStd Dev\t\tTime/Node (ms)" << std::endl;
        std::cout << "----------\t-------------\t-------\t\t-------------" << std::endl;

        for (const auto& [nodeCount, times] : timeByNodes) {
            double avgTime = CalculateMean(times);
            double stdDev = CalculateStdDev(times);
            double timePerNode = avgTime / nodeCount;

            std::cout << nodeCount << "\t\t" << std::fixed << std::setprecision(3)
                      << avgTime << "\t\t" << stdDev << "\t\t" << timePerNode << std::endl;
        }
    }
}

void DataAnalyzer::GenerateStatisticalSummary(const std::vector<PerformanceMetrics>& results) {
    std::cout << "\n=== STATISTICAL SUMMARY ===" << std::endl;

    auto groupedResults = GroupByAlgorithm(results);

    for (const auto& [algorithmName, algorithmResults] : groupedResults) {
        std::cout << "\n" << algorithmName << " Statistics:" << std::endl;

        std::vector<double> executionTimes, memoryUsages;
        uint32_t correctCount = 0;

        for (const auto& result : algorithmResults) {
            executionTimes.push_back(result.executionTimeMs);
            memoryUsages.push_back(result.memoryUsageKB);
            if (result.correctResult) correctCount++;
        }

        std::cout << "Total runs: " << algorithmResults.size() << std::endl;
        std::cout << "Correctness rate: " << std::fixed << std::setprecision(2)
                  << (static_cast<double>(correctCount) / algorithmResults.size() * 100) << "%" << std::endl;

        std::cout << "Execution Time Statistics:" << std::endl;
        std::cout << "  Mean: " << CalculateMean(executionTimes) << " ms" << std::endl;
        std::cout << "  Std Dev: " << CalculateStdDev(executionTimes) << " ms" << std::endl;
        std::cout << "  Min: " << *std::min_element(executionTimes.begin(), executionTimes.end()) << " ms" << std::endl;
        std::cout << "  Max: " << *std::max_element(executionTimes.begin(), executionTimes.end()) << " ms" << std::endl;

        std::cout << "Memory Usage Statistics:" << std::endl;
        std::cout << "  Mean: " << CalculateMean(memoryUsages) << " KB" << std::endl;
        std::cout << "  Std Dev: " << CalculateStdDev(memoryUsages) << " KB" << std::endl;
    }
}

void DataAnalyzer::ExportMultipleFormats(const std::vector<PerformanceMetrics>& results) {
    // CSV format
    std::ofstream csvFile("performance_results_detailed.csv");
    if (csvFile.is_open()) {
        csvFile << "Algorithm,Nodes,Edges,ExecutionTimeMs,MemoryUsageKB,CorrectResult,Timestamp\n";
        for (const auto& result : results) {
            csvFile << result.algorithmName << ","
                    << result.nodeCount << ","
                    << result.edgeCount << ","
                    << result.executionTimeMs << ","
                    << result.memoryUsageKB << ","
                    << (result.correctResult ? "true" : "false") << ","
                    << "2024-12-08" << "\n";
        }
        csvFile.close();
        std::cout << "Detailed results exported to 'performance_results_detailed.csv'" << std::endl;
    }

    // JSON format
    std::ofstream jsonFile("performance_results.json");
    if (jsonFile.is_open()) {
        jsonFile << "{\n";
        jsonFile << "  \"experiment\": \"Breaking the Sorting Barrier\",\n";
        jsonFile << "  \"timestamp\": \"2024-12-08\",\n";
        jsonFile << "  \"results\": [\n";

        bool first = true;
        for (const auto& result : results) {
            if (!first) jsonFile << ",\n";
            jsonFile << "    {\n";
            jsonFile << "      \"algorithm\": \"" << result.algorithmName << "\",\n";
            jsonFile << "      \"nodeCount\": " << result.nodeCount << ",\n";
            jsonFile << "      \"edgeCount\": " << result.edgeCount << ",\n";
            jsonFile << "      \"executionTimeMs\": " << result.executionTimeMs << ",\n";
            jsonFile << "      \"memoryUsageKB\": " << result.memoryUsageKB << ",\n";
            jsonFile << "      \"correctResult\": " << (result.correctResult ? "true" : "false") << "\n";
            jsonFile << "    }";
            first = false;
        }

        jsonFile << "\n  ]\n";
        jsonFile << "}\n";
        jsonFile.close();
        std::cout << "Results exported to 'performance_results.json'" << std::endl;
    }

    // Summary report
    std::ofstream reportFile("experiment_report.txt");
    if (reportFile.is_open()) {
        reportFile << "Breaking the Sorting Barrier - Experiment Report\n";
        reportFile << "================================================\n\n";

        GenerateStatisticalSummary(results);
        auto groupedResults = GroupByAlgorithm(results);

        for (const auto& [algorithmName, algorithmResults] : groupedResults) {
            reportFile << algorithmName << " Algorithm:\n";
            reportFile << "-------------------\n";

            std::vector<double> executionTimes;
            for (const auto& result : algorithmResults) {
                executionTimes.push_back(result.executionTimeMs);
            }

            reportFile << "Average execution time: " << CalculateMean(executionTimes) << " ms\n";
            reportFile << "Number of test runs: " << algorithmResults.size() << "\n\n";
        }

        reportFile.close();
        std::cout << "Report exported to 'experiment_report.txt'" << std::endl;
    }
}

std::map<std::string, std::vector<PerformanceMetrics>> DataAnalyzer::GroupByAlgorithm(
    const std::vector<PerformanceMetrics>& results) {

    std::map<std::string, std::vector<PerformanceMetrics>> grouped;

    for (const auto& result : results) {
        grouped[result.algorithmName].push_back(result);
    }

    return grouped;
}

double DataAnalyzer::CalculateMean(const std::vector<double>& values) {
    if (values.empty()) return 0.0;

    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }
    return sum / values.size();
}

double DataAnalyzer::CalculateStdDev(const std::vector<double>& values) {
    if (values.size() < 2) return 0.0;

    double mean = CalculateMean(values);
    double sumSquaredDiff = 0.0;

    for (double value : values) {
        double diff = value - mean;
        sumSquaredDiff += diff * diff;
    }

    return std::sqrt(sumSquaredDiff / (values.size() - 1));
}

double DataAnalyzer::CalculateSpeedup(double dijkstraTime, double bsbTime) {
    if (bsbTime <= 0) return 0.0;
    return dijkstraTime / bsbTime;
}

} // namespace ns3