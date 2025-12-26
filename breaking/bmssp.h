#ifndef BMSSP_H
#define BMSSP_H

#include <algorithm>
#include <climits>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Define the core types used by BMSSP
using Vertex = int;
using Length = long long;
const Length INF = LLONG_MAX;

// ============ BlockHeap Data Structure ============
// Implementation of Lemma 3.3 from the paper

namespace BlockHeap
{

// Node in the linked list
template <typename K, typename V>
struct Node
{
    K key;
    V value;
    Node* prev;
    Node* next;

    Node(K k, V v)
        : key(k),
          value(v),
          prev(nullptr),
          next(nullptr)
    {
    }
};

// Block in D0 or D1
template <typename K, typename V>
struct Block
{
    Node<K, V>* head; // First node in the block
    Node<K, V>* tail; // Last node in the block
    int size;         // Number of nodes in the block
    V upperBound;     // Upper bound for D1 blocks
    bool owns_nodes;  // Whether this block owns the nodes (for memory management)

    Block()
        : head(nullptr),
          tail(nullptr),
          size(0),
          upperBound(V()),
          owns_nodes(true)
    {
    }

    ~Block()
    {
        // Only delete nodes if we own them
        if (owns_nodes)
        {
            Node<K, V>* curr = head;
            while (curr != nullptr)
            {
                Node<K, V>* next = curr->next;
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
    std::vector<Block<K, V>*> D0;

    // D1: blocks from individual insertions
    std::vector<Block<K, V>*> D1;

    // BST for upper bounds of D1 blocks (using std::set)
    std::set<std::pair<V, int>> D1Bounds; // (upperBound, blockIndex)

    // Map from key to its node for O(1) lookup (for duplicate key handling)
    std::map<K, Node<K, V>*> keyToNode;

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
        Block<K, V>* initBlock = new Block<K, V>();
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

        Node<K, V>* node = it->second;

        // Find which block contains this node and remove it
        bool found = false;
        for (auto b : D1)
        {
            Node<K, V>* curr = b->head;
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
                Node<K, V>* curr = b->head;
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
    void DeleteFromBlock(Block<K, V>* block, Node<K, V>* node)
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
    // Returns: second half of nodes (these will be moved to a new block)
    // Uses nth_element for O(n) average time complexity (as mentioned in paper)
    std::vector<Node<K, V>*> FindMedianAndPartition(Block<K, V>* block)
    {
        std::vector<Node<K, V>*> nodes;
        Node<K, V>* curr = block->head;
        while (curr != nullptr)
        {
            nodes.push_back(curr);
            curr = curr->next;
        }

        if (nodes.size() <= 1)
        {
            return nodes;
        }

        // Find median using nth_element (O(n) average, as mentioned in paper)
        size_t mid = nodes.size() / 2;
        std::nth_element(nodes.begin(), nodes.begin() + mid, nodes.end(),
            [](const Node<K, V>* a, const Node<K, V>* b) {
                if (a->value != b->value)
                {
                    return a->value < b->value;
                }
                return a->key < b->key;
            });

        // Now partition into first half (<= median) and second half (> median)
        // First sort just the first half to maintain order within the block
        std::sort(nodes.begin(), nodes.begin() + mid, [](const Node<K, V>* a, const Node<K, V>* b) {
            if (a->value != b->value)
            {
                return a->value < b->value;
            }
            return a->key < b->key;
        });

        // Collect second half and sort it too
        std::vector<Node<K, V>*> secondHalf(nodes.begin() + mid, nodes.end());
        std::sort(secondHalf.begin(), secondHalf.end(), [](const Node<K, V>* a, const Node<K, V>* b) {
            if (a->value != b->value)
            {
                return a->value < b->value;
            }
            return a->key < b->key;
        });

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

        Block<K, V>* block = D1[blockIdx];
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
        std::vector<Node<K, V>*> secondHalfNodes = FindMedianAndPartition(block);

        // Create new block with second half - this new block will own these nodes
        Block<K, V>* newBlock = new Block<K, V>();
        newBlock->upperBound = block->upperBound;
        // owns_nodes is true by default, which is correct for the new block

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
        Node<K, V>* newNode = new Node<K, V>(key, value);
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

        Block<K, V>* block = D1[targetBlockIdx];

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
        std::unordered_set<K> seenKeys;

        for (const auto& kv : sortedL)
        {
            K key = kv.first;
            V value = kv.second;

            // Skip duplicate keys in this batch
            if (!seenKeys.insert(key).second)
            {
                continue;
            }

            // Check if existing value is better
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
                seenKeys.clear();
            }
        }

        if (!currentPartition.empty())
        {
            partitions.push_back(currentPartition);
        }

        // Create blocks from partitions (in reverse order to prepend)
        for (auto it = partitions.rbegin(); it != partitions.rend(); ++it)
        {
            Block<K, V>* newBlock = new Block<K, V>();

            for (const auto& kv : *it)
            {
                K key = kv.first;
                V value = kv.second;

                auto oldIt = keyToNode.find(key);
                if (oldIt != keyToNode.end())
                {
                    Delete(key, oldIt->second->value);
                }

                Node<K, V>* newNode = new Node<K, V>(key, value);
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

    // Helper: Remove and delete a node from a specific block
    // Returns true if the node was found and deleted
    bool RemoveNodeFromBlock(Block<K, V>* block, Node<K, V>* node)
    {
        if (block->head == node)
        {
            block->head = node->next;
            if (block->head)
            {
                block->head->prev = nullptr;
            }
            if (block->tail == node)
            {
                block->tail = nullptr;
            }
            block->size--;
            delete node;
            return true;
        }

        // Search in the middle of the list
        Node<K, V>* curr = block->head;
        while (curr && curr->next != node)
        {
            curr = curr->next;
        }

        if (curr && curr->next == node)
        {
            curr->next = node->next;
            if (node->next)
            {
                node->next->prev = curr;
            }
            if (block->tail == node)
            {
                block->tail = curr;
            }
            block->size--;
            delete node;
            return true;
        }

        return false;
    }

    // Helper: Remove and delete a node from D0 or D1 blocks
    void RemoveAndDeleteNode(Node<K, V>* node)
    {
        // Try D0 first
        for (auto b : D0)
        {
            if (RemoveNodeFromBlock(b, node))
            {
                return;
            }
        }
        // Then D1
        for (auto b : D1)
        {
            if (RemoveNodeFromBlock(b, node))
            {
                return;
            }
        }
    }

    // Pull() - Return up to M smallest keys with separating bound
    std::pair<std::vector<K>, V> Pull()
    {
        std::vector<K> result;
        std::vector<Node<K, V>*> toDelete;
        std::unordered_set<Node<K, V>*> deleteSet; // For O(1) lookup

        // Collect elements from D0 and D1
        int collected = 0;
        std::vector<Node<K, V>*> collectedNodes;

        for (auto block : D0)
        {
            Node<K, V>* curr = block->head;
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
            Node<K, V>* curr = block->head;
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
            }

            // Remove from keyToNode map first
            for (auto node : collectedNodes)
            {
                keyToNode.erase(node->key);
            }

            // Then remove from blocks and delete nodes
            for (auto node : collectedNodes)
            {
                RemoveAndDeleteNode(node);
            }

            CleanEmptyBlocks();
            return {result, globalB};
        }

        // Need to select exactly M smallest elements
        std::sort(collectedNodes.begin(),
                  collectedNodes.end(),
                  [](const Node<K, V>* a, const Node<K, V>* b) {
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
            deleteSet.insert(collectedNodes[i]);
        }

        // Find smallest remaining value BEFORE deleting
        V minRemaining = globalB;
        bool foundRemaining = false;

        for (auto block : D0)
        {
            Node<K, V>* curr = block->head;
            while (curr != nullptr)
            {
                if (deleteSet.find(curr) == deleteSet.end() && curr->value < minRemaining)
                {
                    minRemaining = curr->value;
                    foundRemaining = true;
                }
                curr = curr->next;
            }
        }

        for (auto block : D1)
        {
            Node<K, V>* curr = block->head;
            while (curr != nullptr)
            {
                if (deleteSet.find(curr) == deleteSet.end() && curr->value < minRemaining)
                {
                    minRemaining = curr->value;
                    foundRemaining = true;
                }
                curr = curr->next;
            }
        }

        // Remove from keyToNode map first
        for (auto node : toDelete)
        {
            keyToNode.erase(node->key);
        }

        // Then remove from blocks and delete nodes
        for (auto node : toDelete)
        {
            RemoveAndDeleteNode(node);
        }

        CleanEmptyBlocks();

        V x = foundRemaining ? minRemaining : globalB;
        return {result, x};
    }

    // Clean up empty blocks
    void CleanEmptyBlocks()
    {
        // Clean D0 - collect empty blocks first
        std::vector<Block<K, V>*> emptyBlocks;
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
        D0.erase(std::remove_if(D0.begin(), D0.end(), [](Block<K, V>* b) { return b->size == 0; }),
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

// ============ BMSSP Algorithm ============

namespace BMSSP
{

// Edge structure - must match verify.cpp
struct Edge
{
    int to;
    int weight;
    int next;
};

// Graph state - these must be set by the caller
extern int n; // Number of vertices
extern int k; // Parameter k = floor(log^(1/3)(n))
extern int t; // Parameter t = floor(log^(2/3)(n))
extern int l; // Number of levels = ceil(log(n)/t)

extern Length dis[]; // Distance array
extern int num_edge; // Number of edges
extern int head[];   // Adjacency list head
extern Edge edge[];  // Edge array

using VertexSet = std::vector<Vertex>;

void ComputeParameters();
std::pair<VertexSet, VertexSet> FindPivots(Length B, const VertexSet& S);
std::pair<Length, VertexSet> BaseCase(Length B, const VertexSet& S);
std::pair<Length, VertexSet> BMSSP(int level, Length B, const VertexSet& S);

} // namespace BMSSP

#endif // BMSSP_H
