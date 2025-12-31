#ifndef BMSSP_H
#define BMSSP_H

#include <algorithm>
#include <climits>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <stdint.h> // for int64_t
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ns3 {

// Define the core types used by BMSSP
// 注意：使用 BmsspLength 避免与 NS-3 的 Length 类冲突
using Vertex = int;
using BmsspLength = int64_t; // NS-3 建议用 int64_t 替代 long long
const BmsspLength BMSSP_INF = LLONG_MAX;

// 边结构体
struct BmsspEdge
{
    int to;
    int weight;
    int next;
};

// ============ BlockHeap Data Structure ============
// Implementation of Lemma 3.3 from the paper

namespace BlockHeap
{

// Forward declaration
template <typename K, typename V>
struct BlockHeapBlock;

// Node in the linked list (renamed to avoid conflict with ns3::Node)
template <typename K, typename V>
struct BlockHeapNode
{
    K key;
    V value;
    BlockHeapBlock<K, V>* block;  // Pointer to the block containing this node
    BlockHeapNode* prev;
    BlockHeapNode* next;

    BlockHeapNode(K k, V v, BlockHeapBlock<K, V>* b = nullptr)
        : key(k),
          value(v),
          block(b),
          prev(nullptr),
          next(nullptr)
    {
    }
};

// Block in D0 or D1 (renamed to avoid conflict)
template <typename K, typename V>
struct BlockHeapBlock
{
    BlockHeapNode<K, V>* head; // First node in the block
    BlockHeapNode<K, V>* tail; // Last node in the block
    int size;         // Number of nodes in the block
    V upperBound;     // Upper bound for D1 blocks
    bool owns_nodes;  // Whether this block owns the nodes (for memory management)

    BlockHeapBlock()
        : head(nullptr),
          tail(nullptr),
          size(0),
          upperBound(V()),
          owns_nodes(true)
    {
    }

    ~BlockHeapBlock()
    {
        // Only delete nodes if we own them
        if (owns_nodes)
        {
            BlockHeapNode<K, V>* curr = head;
            while (curr != nullptr)
            {
                BlockHeapNode<K, V>* next = curr->next;
                delete curr;
                curr = next;
            }
        }
        head = nullptr;
        tail = nullptr;
        size = 0;
    }
};

// The main BlockHeap data structure
template <typename K, typename V>
class BlockHeapDS
{
  private:
    int M;     // Block size parameter
    V globalB; // Global upper bound
    int N;     // Expected number of insertions

    // D0: blocks from batch prepends
    std::vector<BlockHeapBlock<K, V>*> D0;

    // D1: blocks from individual insertions
    std::vector<BlockHeapBlock<K, V>*> D1;

    // BST for upper bounds of D1 blocks (using std::set)
    std::set<std::pair<V, int>> D1Bounds; // (upperBound, blockIndex)

    // Map from key to its node for O(1) lookup (for duplicate key handling)
    std::map<K, BlockHeapNode<K, V>*> keyToNode;

    // Track total size for D1
    int D1TotalSize;

    // Track minimum value of each block for O(1) min lookup
    std::set<std::pair<V, int>> D0MinValues;  // (minValue, blockIndex) for D0
    std::set<std::pair<V, int>> D1MinValues;  // (minValue, blockIndex) for D1

    // Helper function to update block minimum
    void UpdateBlockMin(BlockHeapBlock<K, V>* block, int blockIdx, bool isD0)
    {
        auto& minSet = isD0 ? D0MinValues : D1MinValues;

        // Remove old entry if exists (any value with this blockIdx)
        auto oldIt = minSet.lower_bound({std::numeric_limits<V>::lowest(), blockIdx});
        if (oldIt != minSet.end() && oldIt->second == blockIdx)
        {
            minSet.erase(oldIt);
        }

        // Add new entry if block is non-empty
        if (block && block->head)
        {
            // Find minimum value in this block (blocks are sorted, so head is min)
            minSet.insert({block->head->value, blockIdx});
        }
    }

  public:
    BlockHeapDS(int m, V B, int n)
        : M(m),
          globalB(B),
          N(n),
          D1TotalSize(0)
    {
    }

    ~BlockHeapDS()
    {
        for (auto b : D0)
        {
            delete b;
        }
        for (auto b : D1)
        {
            delete b;
        }
    }

    // Initialize(M, B) - D0 is empty, D1 has a single empty block with upper bound B
    void Initialize()
    {
        BlockHeapBlock<K, V>* initBlock = new BlockHeapBlock<K, V>();
        initBlock->upperBound = globalB;
        D1.push_back(initBlock);
        D1Bounds.insert({globalB, 0});
        D1MinValues.insert({globalB, 0});  // Initial empty block has bound B
    }

    // Delete(key, value) - O(1) for deletion using node->block
    void Delete(K key, V value)
    {
        auto it = keyToNode.find(key);
        if (it == keyToNode.end())
        {
            return;
        }

        BlockHeapNode<K, V>* node = it->second;
        BlockHeapBlock<K, V>* block = node->block;

        // Remove from linked list using the block pointer
        if (node->prev)
        {
            node->prev->next = node->next;
        }
        else
        {
            block->head = node->next;
        }

        if (node->next)
        {
            node->next->prev = node->prev;
        }
        else
        {
            block->tail = node->prev;
        }

        block->size--;

        // Update block minimum tracking (need to find block index)
        // Find block in D1 (most common case for Delete)
        auto it1 = std::find(D1.begin(), D1.end(), block);
        if (it1 != D1.end())
        {
            int blockIdx = static_cast<int>(std::distance(D1.begin(), it1));
            UpdateBlockMin(block, blockIdx, false);
        }
        else
        {
            // Check D0
            auto it0 = std::find(D0.begin(), D0.end(), block);
            if (it0 != D0.end())
            {
                int blockIdx = static_cast<int>(std::distance(D0.begin(), it0));
                UpdateBlockMin(block, blockIdx, true);
            }
        }

        // Remove from keyToNode and delete
        keyToNode.erase(it);
        delete node;
    }

    // Delete node from a specific block
    void DeleteFromBlock(BlockHeapBlock<K, V>* block, BlockHeapNode<K, V>* node)
    {
        if (node->prev)
        {
            node->prev->next = node->next;
        }
        else
        {
            block->head = node->next;
        }

        if (node->next)
        {
            node->next->prev = node->prev;
        }
        else
        {
            block->tail = node->prev;
        }

        block->size--;
        keyToNode.erase(node->key);
        delete node;
    }

    // Quickselect: O(n) algorithm to find k-th smallest element
    // Returns index of the k-th smallest element (0-based)
    // Partitions the array such that:
    //   - elements [0, pivot_idx-1] are <= pivot
    //   - elements [pivot_idx+1, end] are >= pivot
    size_t QuickselectPartition(std::vector<BlockHeapNode<K, V>*>& nodes, size_t left, size_t right, size_t k)
    {
        while (left < right && nodes.size() > 0)
        {
            // Use median-of-three pivot selection
            size_t mid = left + (right - left) / 2;
            auto compare = [](const BlockHeapNode<K, V>* a, const BlockHeapNode<K, V>* b) {
                if (a->value != b->value)
                    return a->value < b->value;
                return a->key < b->key;
            };

            // Sort left, mid, right to get median
            if (compare(nodes[right], nodes[left]))
                std::swap(nodes[left], nodes[right]);
            if (compare(nodes[mid], nodes[left]))
                std::swap(nodes[left], nodes[mid]);
            if (compare(nodes[right], nodes[mid]))
                std::swap(nodes[mid], nodes[right]);

            // Use mid as pivot
            V pivotValue = nodes[mid]->value;
            K pivotKey = nodes[mid]->key;
            std::swap(nodes[mid], nodes[right]);

            // Partition
            size_t i = left;
            for (size_t j = left; j < right; j++)
            {
                bool less = (nodes[j]->value < pivotValue) ||
                           (nodes[j]->value == pivotValue && nodes[j]->key < pivotKey);
                if (less)
                {
                    std::swap(nodes[i], nodes[j]);
                    i++;
                }
            }
            std::swap(nodes[i], nodes[right]);

            // Check if pivot is at position k
            if (k == i)
            {
                return i;
            }
            else if (k < i)
            {
                right = i - 1;
            }
            else
            {
                left = i + 1;
            }
        }
        return left;
    }

    // Find median using Quickselect and partition
    std::vector<BlockHeapNode<K, V>*> FindMedianAndPartition(BlockHeapBlock<K, V>* block)
    {
        std::vector<BlockHeapNode<K, V>*> nodes;
        BlockHeapNode<K, V>* curr = block->head;
        while (curr != nullptr)
        {
            nodes.push_back(curr);
            curr = curr->next;
        }

        if (nodes.size() <= 1)
        {
            return nodes;
        }

        // Find median index using Quickselect - O(n)
        size_t mid = nodes.size() / 2;
        if (nodes.size() > 0)
        {
            QuickselectPartition(nodes, 0, nodes.size() - 1, mid);
        }

        // Now nodes is partitioned around the median
        // elements [0, mid-1] <= nodes[mid]
        // elements [mid+1, end] >= nodes[mid]
        // But the partition isn't fully sorted, so we need to collect second half

        std::vector<BlockHeapNode<K, V>*> firstHalf;
        std::vector<BlockHeapNode<K, V>*> secondHalf;

        // Use stable partition based on comparison with median
        auto* medianNode = nodes[mid];
        V medianValue = medianNode->value;
        K medianKey = medianNode->key;

        for (size_t i = 0; i < nodes.size(); i++)
        {
            bool less = (nodes[i]->value < medianValue) ||
                       (nodes[i]->value == medianValue && nodes[i]->key < medianKey);
            if (less || (nodes[i] == medianNode && firstHalf.size() < mid))
            {
                firstHalf.push_back(nodes[i]);
            }
            else
            {
                secondHalf.push_back(nodes[i]);
            }
        }

        // Ensure first half has exactly mid elements
        while (firstHalf.size() > mid)
        {
            secondHalf.push_back(firstHalf.back());
            firstHalf.pop_back();
        }
        while (firstHalf.size() < mid && !secondHalf.empty())
        {
            firstHalf.push_back(secondHalf.back());
            secondHalf.pop_back();
        }

        // Rebuild block with first half (sorted for consistency)
        std::sort(firstHalf.begin(), firstHalf.end(), [](const BlockHeapNode<K, V>* a, const BlockHeapNode<K, V>* b) {
            if (a->value != b->value)
                return a->value < b->value;
            return a->key < b->key;
        });

        block->head = nullptr;
        block->tail = nullptr;
        block->size = 0;

        for (auto* node : firstHalf)
        {
            node->prev = block->tail;
            node->next = nullptr;
            if (block->tail)
            {
                block->tail->next = node;
            }
            else
            {
                block->head = node;
            }
            block->tail = node;
            block->size++;
        }

        return secondHalf;
    }

    // Split a block that exceeds M elements
    void Split(int blockIdx)
    {
        if (blockIdx >= (int)D1.size())
        {
            return;
        }

        BlockHeapBlock<K, V>* block = D1[blockIdx];
        if (block->size <= M)
        {
            return;
        }

        // Remove old upper bound
        auto it = D1Bounds.find({block->upperBound, blockIdx});
        if (it != D1Bounds.end())
        {
            D1Bounds.erase(it);
        }

        // Partition and get nodes for second block
        std::vector<BlockHeapNode<K, V>*> secondHalfNodes = FindMedianAndPartition(block);

        // Create new block with second half
        BlockHeapBlock<K, V>* newBlock = new BlockHeapBlock<K, V>();
        newBlock->upperBound = block->upperBound;

        for (auto* node : secondHalfNodes)
        {
            // Update block pointer when moving to new block
            node->block = newBlock;
            node->prev = newBlock->tail;
            node->next = nullptr;
            if (newBlock->tail)
            {
                newBlock->tail->next = node;
            }
            else
            {
                newBlock->head = node;
            }
            newBlock->tail = node;
            newBlock->size++;
        }

        // Update upper bounds
        if (block->tail)
        {
            block->upperBound = block->tail->value;
        }
        if (newBlock->tail)
        {
            newBlock->upperBound = std::max(newBlock->upperBound, newBlock->tail->value);
        }

        // Insert new block
        D1.insert(D1.begin() + blockIdx + 1, newBlock);

        // Rebuild BST
        RebuildBoundsBST();

        // Rebuild D1MinValues
        D1MinValues.clear();
        for (size_t i = 0; i < D1.size(); i++)
        {
            if (D1[i]->head)
            {
                D1MinValues.insert({D1[i]->head->value, static_cast<int>(i)});
            }
        }
    }

    // Rebuild the BST for D1 upper bounds
    void RebuildBoundsBST()
    {
        D1Bounds.clear();
        for (size_t i = 0; i < D1.size(); i++)
        {
            D1Bounds.insert({D1[i]->upperBound, (int)i});
        }
    }

    // Insert(key, value)
    void Insert(K key, V value)
    {
        // Check if key already exists
        auto it = keyToNode.find(key);
        if (it != keyToNode.end())
        {
            if (value >= it->second->value)
            {
                return;
            }
            Delete(key, it->second->value);
        }

        // Find the appropriate block in D1 first
        int targetBlockIdx = 0;
        auto ubIt = D1Bounds.lower_bound({value, 0});
        if (ubIt != D1Bounds.end())
        {
            targetBlockIdx = ubIt->second;
        }
        else
        {
            targetBlockIdx = (int)D1.size() - 1;
        }

        BlockHeapBlock<K, V>* block = D1[targetBlockIdx];

        // Create new node with block pointer
        BlockHeapNode<K, V>* newNode = new BlockHeapNode<K, V>(key, value, block);
        keyToNode[key] = newNode;

        // Add node to block
        newNode->prev = block->tail;
        if (block->tail)
        {
            block->tail->next = newNode;
        }
        else
        {
            block->head = newNode;
        }
        block->tail = newNode;
        block->size++;
        D1TotalSize++;

        // Update upper bound if needed
        if (value > block->upperBound)
        {
            auto oldIt = D1Bounds.find({block->upperBound, targetBlockIdx});
            if (oldIt != D1Bounds.end())
            {
                D1Bounds.erase(oldIt);
            }
            block->upperBound = value;
            D1Bounds.insert({block->upperBound, targetBlockIdx});
        }

        // Check if split is needed
        if (block->size > M)
        {
            Split(targetBlockIdx);
        }

        // Update block minimum tracking
        UpdateBlockMin(block, targetBlockIdx, false);
    }

    // BatchPrepend(L)
    void BatchPrepend(const std::vector<std::pair<K, V>>& L)
    {
        if (L.empty())
        {
            return;
        }

        // Sort L by value
        std::vector<std::pair<K, V>> sortedL = L;
        std::sort(sortedL.begin(), sortedL.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second)
            {
                return a.second < b.second;
            }
            return a.first < b.first;
        });

        // Handle duplicates and partition into blocks
        std::vector<std::vector<std::pair<K, V>>> partitions;
        std::vector<std::pair<K, V>> currentPartition;
        std::set<K> seenKeys;

        for (const auto& kv : sortedL)
        {
            K key = kv.first;
            V value = kv.second;

            if (seenKeys.count(key))
            {
                continue;
            }
            seenKeys.insert(key);

            auto it = keyToNode.find(key);
            if (it != keyToNode.end() && value >= it->second->value)
            {
                continue;
            }

            currentPartition.push_back({key, value});

            if ((int)currentPartition.size() >= M / 2)
            {
                partitions.push_back(currentPartition);
                currentPartition.clear();
            }
        }

        if (!currentPartition.empty())
        {
            partitions.push_back(currentPartition);
        }

        // Create blocks from partitions
        for (auto it = partitions.rbegin(); it != partitions.rend(); ++it)
        {
            BlockHeapBlock<K, V>* newBlock = new BlockHeapBlock<K, V>();

            for (const auto& kv : *it)
            {
                K key = kv.first;
                V value = kv.second;

                auto oldIt = keyToNode.find(key);
                if (oldIt != keyToNode.end())
                {
                    Delete(key, oldIt->second->value);
                }

                BlockHeapNode<K, V>* newNode = new BlockHeapNode<K, V>(key, value, newBlock);
                keyToNode[key] = newNode;

                newNode->prev = newBlock->tail;
                if (newBlock->tail)
                {
                    newBlock->tail->next = newNode;
                }
                else
                {
                    newBlock->head = newNode;
                }
                newBlock->tail = newNode;
                newBlock->size++;
            }

            D0.insert(D0.begin(), newBlock);
        }

        // Rebuild D0MinValues indices after insert
        D0MinValues.clear();
        for (size_t i = 0; i < D0.size(); i++)
        {
            if (D0[i]->head)
            {
                D0MinValues.insert({D0[i]->head->value, static_cast<int>(i)});
            }
        }
    }

    // Pull() - Return up to M smallest keys with separating bound
    std::pair<std::vector<K>, V> Pull()
    {
        std::vector<K> result;
        std::vector<BlockHeapNode<K, V>*> toDelete;

        // Collect elements from D0 and D1
        int collected = 0;
        std::vector<BlockHeapNode<K, V>*> collectedNodes;

        for (auto block : D0)
        {
            BlockHeapNode<K, V>* curr = block->head;
            while (curr != nullptr && collected < M)
            {
                collectedNodes.push_back(curr);
                collected++;
                curr = curr->next;
            }
            if (collected >= M)
            {
                break;
            }
        }

        for (auto block : D1)
        {
            BlockHeapNode<K, V>* curr = block->head;
            while (curr != nullptr && collected < M)
            {
                collectedNodes.push_back(curr);
                collected++;
                curr = curr->next;
            }
            if (collected >= M)
            {
                break;
            }
        }

        if (collectedNodes.empty())
        {
            return {result, globalB};
        }

        // If total collected <= M, return all
        if (collected <= M)
        {
            for (auto node : collectedNodes)
            {
                result.push_back(node->key);
                toDelete.push_back(node);
            }

            for (auto node : toDelete)
            {
                for (auto b : D0)
                {
                    BlockHeapNode<K, V>* curr = b->head;
                    while (curr)
                    {
                        if (curr == node)
                        {
                            DeleteFromBlock(b, node);
                            goto deleted;
                        }
                        curr = curr->next;
                    }
                }
                for (auto b : D1)
                {
                    BlockHeapNode<K, V>* curr = b->head;
                    while (curr)
                    {
                        if (curr == node)
                        {
                            DeleteFromBlock(b, node);
                            goto deleted;
                        }
                        curr = curr->next;
                    }
                }
            deleted:;
            }

            CleanEmptyBlocks();
            return {result, globalB};
        }

        // Need to select exactly M smallest elements
        std::sort(collectedNodes.begin(),
                  collectedNodes.end(),
                  [](const BlockHeapNode<K, V>* a, const BlockHeapNode<K, V>* b) {
                      if (a->value != b->value)
                      {
                          return a->value < b->value;
                      }
                      return a->key < b->key;
                  });

        for (int i = 0; i < M && i < (int)collectedNodes.size(); i++)
        {
            result.push_back(collectedNodes[i]->key);
            toDelete.push_back(collectedNodes[i]);
        }

        // Find smallest remaining value using auxiliary sets
        V minRemaining = globalB;
        bool foundRemaining = false;

        // Check D0 minimum first
        if (!D0MinValues.empty())
        {
            minRemaining = D0MinValues.begin()->first;
            foundRemaining = true;
        }

        // Check D1 minimum
        if (!D1MinValues.empty())
        {
            V d1Min = D1MinValues.begin()->first;
            if (!foundRemaining || d1Min < minRemaining)
            {
                minRemaining = d1Min;
                foundRemaining = true;
            }
        }

        // Verify the minimum is not one of the deleted nodes
        if (foundRemaining && !toDelete.empty())
        {
            std::unordered_set<BlockHeapNode<K, V>*> deletedSet(toDelete.begin(), toDelete.end());

            // If minimum is deleted, find next minimum
            // Check all blocks to find first non-deleted minimum
            V actualMin = globalB;
            bool foundActual = false;

            for (auto block : D0)
            {
                BlockHeapNode<K, V>* curr = block->head;
                while (curr)
                {
                    if (deletedSet.find(curr) == deletedSet.end())
                    {
                        if (!foundActual || curr->value < actualMin)
                        {
                            actualMin = curr->value;
                            foundActual = true;
                            break;
                        }
                    }
                    curr = curr->next;
                }
                if (foundActual) break;
            }

            if (!foundActual)
            {
                for (auto block : D1)
                {
                    BlockHeapNode<K, V>* curr = block->head;
                    while (curr)
                    {
                        if (deletedSet.find(curr) == deletedSet.end())
                        {
                            if (!foundActual || curr->value < actualMin)
                            {
                                actualMin = curr->value;
                                foundActual = true;
                                break;
                            }
                        }
                        curr = curr->next;
                    }
                    if (foundActual) break;
                }
            }

            if (foundActual)
            {
                minRemaining = actualMin;
            }
        }

        // Delete all pulled nodes
        for (auto node : toDelete)
        {
            for (auto b : D0)
            {
                BlockHeapNode<K, V>* curr = b->head;
                while (curr)
                {
                    if (curr == node)
                    {
                        DeleteFromBlock(b, node);
                        goto deleted2;
                    }
                    curr = curr->next;
                }
            }
            for (auto b : D1)
            {
                BlockHeapNode<K, V>* curr = b->head;
                while (curr)
                {
                    if (curr == node)
                    {
                        DeleteFromBlock(b, node);
                        goto deleted2;
                    }
                    curr = curr->next;
                }
            }
        deleted2:;
        }

        CleanEmptyBlocks();

        V x = foundRemaining ? minRemaining : globalB;
        return {result, x};
    }

    // Clean up empty blocks
    void CleanEmptyBlocks()
    {
        // Clean D0 - collect empty blocks first
        std::vector<BlockHeapBlock<K, V>*> emptyBlocks;
        for (auto b : D0)
        {
            if (b->size == 0)
            {
                emptyBlocks.push_back(b);
            }
        }
        for (auto b : emptyBlocks)
        {
            delete b;
        }
        // Then remove from vector
        D0.erase(std::remove_if(D0.begin(), D0.end(), [](BlockHeapBlock<K, V>* b) { return b->size == 0; }),
                 D0.end());

        // Clean D1 - collect empty blocks first
        std::vector<int> emptyBlockIdxs;
        for (size_t i = 0; i < D1.size(); i++)
        {
            if (D1[i]->size == 0)
            {
                emptyBlockIdxs.push_back((int)i);
            }
        }

        for (int idx : emptyBlockIdxs)
        {
            delete D1[idx];
            D1[idx] = nullptr;
        }

        auto newEndD1 = std::remove(D1.begin(), D1.end(), nullptr);
        D1.erase(newEndD1, D1.end());

        if (!emptyBlockIdxs.empty())
        {
            RebuildBoundsBST();
        }

        // Rebuild min value sets
        D0MinValues.clear();
        for (size_t i = 0; i < D0.size(); i++)
        {
            if (D0[i]->head)
            {
                D0MinValues.insert({D0[i]->head->value, static_cast<int>(i)});
            }
        }

        D1MinValues.clear();
        for (size_t i = 0; i < D1.size(); i++)
        {
            if (D1[i]->head)
            {
                D1MinValues.insert({D1[i]->head->value, static_cast<int>(i)});
            }
        }
    }

    bool IsEmpty() const
    {
        for (auto b : D0)
        {
            if (b->size > 0)
            {
                return false;
            }
        }
        for (auto b : D1)
        {
            if (b->size > 0)
            {
                return false;
            }
        }
        return true;
    }
};

} // namespace BlockHeap

// =========================================================
// BmsspSolver 类定义 (用来包裹原本的全局算法)
// =========================================================

class BmsspSolver
{
public:
    BmsspSolver();
    ~BmsspSolver();

    // 初始化图大小
    void Init(int n);
    // 添加边
    void AddEdge(int u, int v, int weight);
    // 运行算法 (计算单源最短路)
    void Run(int sourceNode);
    // 获取结果
    BmsspLength GetDistance(int node);
    // 获取下一跳节点ID（从 target 回溯到 source 的直连子节点）
    int GetNextHop(int target);

private:
    // 原来的全局变量 -> 现在的成员变量
    int n; 
    int k, t, l; // 算法参数

    // 以前是 extern int head[], extern Edge edge[]
    // 现在用 vector 管理动态内存
    std::vector<int> head;
    std::vector<BmsspEdge> edge;
    int num_edge;

    // 以前是 extern Length dis[]
    std::vector<BmsspLength> dis;
    
    // 路径追踪：parent[i] 表示从 source 到 i 的最短路径上，i 的前驱节点
    // parent[source] = source (根节点)
    std::vector<int> parent;
    
    // 记录 source 节点，用于 GetNextHop
    int m_source;

    using VertexSet = std::vector<Vertex>;

    // 内部算法函数 (对应你论文里的 Algorithm 1, 2, 3)
    void ComputeParameters();
    
    std::pair<VertexSet, VertexSet> 
    FindPivots(BmsspLength B, const VertexSet& S);
    
    std::pair<BmsspLength, VertexSet> 
    BaseCase(BmsspLength B, const VertexSet& S);
    
    std::pair<BmsspLength, VertexSet> 
    BMSSP(int level, BmsspLength B, const VertexSet& S);
};

} // namespace ns3

#endif // BMSSP_H
