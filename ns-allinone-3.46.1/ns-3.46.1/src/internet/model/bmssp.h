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

// Node in the linked list (renamed to avoid conflict with ns3::Node)
template <typename K, typename V>
struct BlockHeapNode
{
    K key;
    V value;
    BlockHeapNode* prev;
    BlockHeapNode* next;

    BlockHeapNode(K k, V v)
        : key(k),
          value(v),
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
    }

    // Delete(key, value) - O(1) for deletion
    void Delete(K key, V value)
    {
        auto it = keyToNode.find(key);
        if (it == keyToNode.end())
        {
            return;
        }

        BlockHeapNode<K, V>* node = it->second;

        // Find which block contains this node and remove it
        bool found = false;
        for (auto b : D1)
        {
            BlockHeapNode<K, V>* curr = b->head;
            while (curr)
            {
                if (curr == node)
                {
                    // Remove from linked list
                    if (curr->prev)
                    {
                        curr->prev->next = curr->next;
                    }
                    else
                    {
                        b->head = curr->next;
                    }
                    if (curr->next)
                    {
                        curr->next->prev = curr->prev;
                    }
                    else
                    {
                        b->tail = curr->prev;
                    }
                    b->size--;
                    found = true;
                    break;
                }
                curr = curr->next;
            }
            if (found)
            {
                break;
            }
        }
        if (!found)
        {
            for (auto b : D0)
            {
                BlockHeapNode<K, V>* curr = b->head;
                while (curr)
                {
                    if (curr == node)
                    {
                        // Remove from linked list
                        if (curr->prev)
                        {
                            curr->prev->next = curr->next;
                        }
                        else
                        {
                            b->head = curr->next;
                        }
                        if (curr->next)
                        {
                            curr->next->prev = curr->prev;
                        }
                        else
                        {
                            b->tail = curr->prev;
                        }
                        b->size--;
                        found = true;
                        break;
                    }
                    curr = curr->next;
                }
                if (found)
                {
                    break;
                }
            }
        }

        // Remove from keyToNode first
        keyToNode.erase(it);
        // Then delete the node
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

    // Find median of a linked list and partition
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

        // Sort by value (and key for ties)
        std::sort(nodes.begin(), nodes.end(), [](const BlockHeapNode<K, V>* a, const BlockHeapNode<K, V>* b) {
            if (a->value != b->value)
            {
                return a->value < b->value;
            }
            return a->key < b->key;
        });

        // Find median
        size_t mid = nodes.size() / 2;

        // Partition: first half goes to first block, rest to second
        std::vector<BlockHeapNode<K, V>*> secondHalf;

        for (size_t i = 0; i < nodes.size(); i++)
        {
            if (i < mid)
            {
                // Stays in first block
            }
            else
            {
                secondHalf.push_back(nodes[i]);
            }
        }

        // Rebuild block with first half
        block->head = nullptr;
        block->tail = nullptr;
        block->size = 0;

        for (size_t i = 0; i < mid; i++)
        {
            auto* node = nodes[i];
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

        // Create new node
        BlockHeapNode<K, V>* newNode = new BlockHeapNode<K, V>(key, value);
        keyToNode[key] = newNode;

        // Find the appropriate block in D1
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

                BlockHeapNode<K, V>* newNode = new BlockHeapNode<K, V>(key, value);
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

        // Find smallest remaining value
        V minRemaining = globalB;
        bool foundRemaining = false;

        for (auto block : D0)
        {
            BlockHeapNode<K, V>* curr = block->head;
            while (curr != nullptr)
            {
                bool isDeleted = false;
                for (auto delNode : toDelete)
                {
                    if (curr == delNode)
                    {
                        isDeleted = true;
                        break;
                    }
                }
                if (!isDeleted && curr->value < minRemaining)
                {
                    minRemaining = curr->value;
                    foundRemaining = true;
                }
                curr = curr->next;
            }
        }

        for (auto block : D1)
        {
            BlockHeapNode<K, V>* curr = block->head;
            while (curr != nullptr)
            {
                bool isDeleted = false;
                for (auto delNode : toDelete)
                {
                    if (curr == delNode)
                    {
                        isDeleted = true;
                        break;
                    }
                }
                if (!isDeleted && curr->value < minRemaining)
                {
                    minRemaining = curr->value;
                    foundRemaining = true;
                }
                curr = curr->next;
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
