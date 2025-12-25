#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using Vertex = int;
using Length = int;
const Length INF = INT_MAX;

// ============ BlockHeap Data Structure (with D0 and D1) ============
// Implementation of Lemma 3.3 from the paper

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

template <typename K, typename V>
struct Block
{
    Node<K, V>* head;
    Node<K, V>* tail;
    int size;
    V upperBound;
    bool owns_nodes;

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

template <typename K, typename V>
class BlockHeapDS
{
  private:
    int M;
    V globalB;
    int N;

    std::vector<Block<K, V>*> D0;
    std::vector<Block<K, V>*> D1;
    std::set<std::pair<V, int>> D1Bounds;
    std::map<K, Node<K, V>*> keyToNode;
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

    void Initialize()
    {
        Block<K, V>* initBlock = new Block<K, V>();
        initBlock->upperBound = globalB;
        D1.push_back(initBlock);
        D1Bounds.insert({globalB, 0});
    }

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

        std::sort(nodes.begin(), nodes.end(), [](const Node<K, V>* a, const Node<K, V>* b) {
            if (a->value != b->value)
            {
                return a->value < b->value;
            }
            return a->key < b->key;
        });

        size_t mid = nodes.size() / 2;
        std::vector<Node<K, V>*> secondHalf;
        for (size_t i = 0; i < nodes.size(); i++)
        {
            if (i >= mid)
            {
                secondHalf.push_back(nodes[i]);
            }
        }

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

        auto it = D1Bounds.find({block->upperBound, blockIdx});
        if (it != D1Bounds.end())
        {
            D1Bounds.erase(it);
        }

        std::vector<Node<K, V>*> secondHalfNodes = FindMedianAndPartition(block);

        Block<K, V>* newBlock = new Block<K, V>();
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

        if (block->tail)
        {
            block->upperBound = block->tail->value;
        }
        if (newBlock->tail)
        {
            newBlock->upperBound = std::max(newBlock->upperBound, newBlock->tail->value);
        }

        D1.insert(D1.begin() + blockIdx + 1, newBlock);
        RebuildBoundsBST();
    }

    void RebuildBoundsBST()
    {
        D1Bounds.clear();
        for (size_t i = 0; i < D1.size(); i++)
        {
            D1Bounds.insert({D1[i]->upperBound, (int)i});
        }
    }

    void Insert(K key, V value)
    {
        auto it = keyToNode.find(key);
        if (it != keyToNode.end())
        {
            if (value >= it->second->value)
            {
                return;
            }
            // Delete old node
            K k = it->first;
            Node<K, V>* oldNode = it->second;
            keyToNode.erase(it);
            // Find and delete from block
            for (auto b : D1)
            {
                Node<K, V>* curr = b->head;
                while (curr)
                {
                    if (curr == oldNode)
                    {
                        DeleteFromBlock(b, oldNode);
                        goto deleted;
                    }
                    curr = curr->next;
                }
            }
            for (auto b : D0)
            {
                Node<K, V>* curr = b->head;
                while (curr)
                {
                    if (curr == oldNode)
                    {
                        DeleteFromBlock(b, oldNode);
                        goto deleted;
                    }
                    curr = curr->next;
                }
            }
        deleted:;
        }

        Node<K, V>* newNode = new Node<K, V>(key, value);
        keyToNode[key] = newNode;

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

        if (block->size > M)
        {
            Split(targetBlockIdx);
        }
    }

    void BatchPrepend(const std::vector<std::pair<K, V>>& L)
    {
        if (L.empty())
        {
            return;
        }

        std::vector<std::pair<K, V>> sortedL = L;
        std::sort(sortedL.begin(), sortedL.end(), [](const auto& a, const auto& b) {
            if (a.second != b.second)
            {
                return a.second < b.second;
            }
            return a.first < b.first;
        });

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
                    Node<K, V>* oldNode = oldIt->second;
                    keyToNode.erase(oldIt);
                    bool deleted = false;
                    for (auto b : D1)
                    {
                        Node<K, V>* curr = b->head;
                        while (curr)
                        {
                            if (curr == oldNode)
                            {
                                DeleteFromBlock(b, oldNode);
                                deleted = true;
                                break;
                            }
                            curr = curr->next;
                        }
                        if (deleted)
                        {
                            break;
                        }
                    }
                    if (!deleted)
                    {
                        for (auto b : D0)
                        {
                            Node<K, V>* curr = b->head;
                            while (curr)
                            {
                                if (curr == oldNode)
                                {
                                    DeleteFromBlock(b, oldNode);
                                    break;
                                }
                                curr = curr->next;
                            }
                        }
                    }
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

    std::pair<std::vector<K>, V> Pull()
    {
        std::vector<K> result;
        std::vector<Node<K, V>*> toDelete;

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
                    Node<K, V>* curr = b->head;
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
                    Node<K, V>* curr = b->head;
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
        }

        V minRemaining = globalB;
        bool foundRemaining = false;

        for (auto block : D0)
        {
            Node<K, V>* curr = block->head;
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
            Node<K, V>* curr = block->head;
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

        for (auto node : toDelete)
        {
            for (auto b : D0)
            {
                Node<K, V>* curr = b->head;
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
                Node<K, V>* curr = b->head;
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

    void CleanEmptyBlocks()
    {
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
        D0.erase(std::remove_if(D0.begin(), D0.end(), [](Block<K, V>* b) { return b->size == 0; }),
                 D0.end());

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

// ============ Binary Heap (for BaseCase Dijkstra) ============
typedef std::pair<Length, Vertex> heap_node;
typedef std::priority_queue<heap_node, std::vector<heap_node>, std::greater<heap_node>> MinHeap;

// ============ Global Graph State ============
int n, m, s, k, t, l;

Length dis[1000005];

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
    edge[++num_edge].to = v;
    edge[num_edge].weight = w;
    edge[num_edge].next = head[u];
    head[u] = num_edge;
}

// ============ FindPivots - Algorithm 1 ============
std::pair<std::vector<Vertex>, std::vector<Vertex>>
FindPivots(Length B, const std::vector<Vertex>& S)
{
    std::unordered_set<Vertex> W(S.begin(), S.end());
    std::unordered_set<Vertex> W_prev(S.begin(), S.end());
    std::unordered_map<Vertex, Vertex> parent;

    for (int i = 1; i <= k; ++i)
    {
        std::unordered_set<Vertex> Wi;
        for (const auto& u : W_prev)
        {
            for (int edge_idx = head[u]; edge_idx; edge_idx = edge[edge_idx].next)
            {
                Vertex v = edge[edge_idx].to;

                if (dis[u] + edge[edge_idx].weight <= dis[v])
                {
                    Length newDist = dis[u] + edge[edge_idx].weight;
                    if (newDist < dis[v])
                    {
                        dis[v] = newDist;
                        parent[v] = u;
                    }
                    if (newDist < B && W.find(v) == W.end())
                    {
                        Wi.insert(v);
                        W.insert(v);
                    }
                }
            }
        }

        if (W.size() >= k * S.size())
        {
            std::vector<Vertex> P = S;
            return std::make_pair(P, std::vector<Vertex>(W.begin(), W.end()));
        }

        W_prev = Wi;
    }

    std::map<Vertex, int> tree_heads;
    std::map<Vertex, int> is_root;
    std::vector<Edge> tree_edges;

    for (const auto& u : W)
    {
        if (parent.find(u) != parent.end() && W.find(parent[u]) != W.end())
        {
            Vertex p = parent[u];
            if (tree_heads.find(p) == tree_heads.end())
            {
                tree_edges.push_back({u, (int)(dis[u] - dis[p]), -1});
            }
            else
            {
                tree_edges.push_back({u, (int)(dis[u] - dis[p]), tree_heads[p]});
            }
            tree_heads[p] = (int)tree_edges.size() - 1;
            is_root[u] = 0;
            if (is_root.find(p) == is_root.end())
            {
                is_root[p] = 1;
            }
        }
    }

    std::unordered_map<Vertex, int> tree_size;

    std::function<int(Vertex)> compute_tree_size = [&](Vertex root) -> int {
        int total_size = 1;
        auto it = tree_heads.find(root);
        if (it == tree_heads.end())
        {
            return total_size;
        }
        for (int edge_idx = it->second; edge_idx != -1; edge_idx = tree_edges[edge_idx].next)
        {
            Vertex child = tree_edges[edge_idx].to;
            total_size += compute_tree_size(child);
        }
        tree_size[root] = total_size;
        return total_size;
    };

    std::vector<Vertex> P;
    for (const auto& u : is_root)
    {
        if (u.second == 1)
        {
            int sz = compute_tree_size(u.first);
            if (sz >= k)
            {
                P.push_back(u.first);
            }
        }
    }

    return std::make_pair(P, std::vector<Vertex>(W.begin(), W.end()));
}

// ============ BaseCase - Algorithm 2 ============
std::pair<Length, std::vector<Vertex>>
BaseCase(Length B, const std::vector<Vertex>& S)
{
    assert(S.size() == 1);
    Vertex x = S[0];

    std::unordered_set<Vertex> U;
    U.insert(x);

    MinHeap H;
    H.push(std::make_pair(dis[x], x));

    while (!H.empty())
    {
        heap_node hn = H.top();
        H.pop();
        Vertex u = hn.second;

        if (hn.first != dis[u])
        {
            continue;
        }

        for (int edge_idx = head[u]; edge_idx; edge_idx = edge[edge_idx].next)
        {
            Vertex v = edge[edge_idx].to;

            if (dis[u] + edge[edge_idx].weight <= dis[v] && dis[u] + edge[edge_idx].weight < B)
            {
                dis[v] = dis[u] + edge[edge_idx].weight;
                H.push(std::make_pair(dis[v], v));
            }
        }
    }

    if ((int)U.size() <= k)
    {
        return std::make_pair(B, std::vector<Vertex>(U.begin(), U.end()));
    }

    Length B_prime = -1;
    for (const auto& u : U)
    {
        if (dis[u] > B_prime)
        {
            B_prime = dis[u];
        }
    }

    std::vector<Vertex> result;
    for (const auto& u : U)
    {
        if (dis[u] < B_prime)
        {
            result.push_back(u);
        }
    }

    return std::make_pair(B_prime, result);
}

// ============ BMSSP - Algorithm 3 ============
std::pair<Length, std::vector<Vertex>>
BMSSP(int level, Length B, const std::vector<Vertex>& S)
{
    if (level == 0)
    {
        return BaseCase(B, S);
    }

    auto pw = FindPivots(B, S);
    std::vector<Vertex> P = pw.first;
    std::vector<Vertex> W = pw.second;

    int M = (int)pow(2, (level - 1) * t);
    int N = k * (1 << (level * t));
    BlockHeapDS<Vertex, Length> D(M, B, N);
    D.Initialize();

    Length B0p = B;
    for (const Vertex& p : P)
    {
        B0p = std::min(B0p, (Length)dis[p]);
        D.Insert(p, dis[p]);
    }

    int i = 0;
    std::unordered_set<Vertex> U;
    Length Bip = B0p;

    while ((int)U.size() < k * (int)pow(2, level * t) && !D.IsEmpty())
    {
        i += 1;

        auto BS = D.Pull();
        std::vector<Vertex> Si = BS.first;
        Length Bi = BS.second;

        std::pair<Length, std::vector<Vertex>> BU = BMSSP(level - 1, Bi, Si);
        Bip = BU.first;
        std::vector<Vertex> Ui = BU.second;

        for (const Vertex& u : Ui)
        {
            U.insert(u);
        }

        std::vector<std::pair<Vertex, Length>> K;

        for (const Vertex& u : Ui)
        {
            for (int edge_idx = head[u]; edge_idx; edge_idx = edge[edge_idx].next)
            {
                Vertex v = edge[edge_idx].to;

                if (dis[u] + edge[edge_idx].weight <= dis[v])
                {
                    Length newDist = dis[u] + edge[edge_idx].weight;
                    if (newDist < dis[v])
                    {
                        dis[v] = newDist;
                    }

                    if (newDist >= Bi && newDist < B)
                    {
                        D.Insert(v, dis[v]);
                    }
                    else if (newDist >= Bip && newDist < Bi)
                    {
                        K.push_back(std::make_pair(v, dis[v]));
                    }
                }
            }
        }

        for (const Vertex& x : Si)
        {
            if (dis[x] >= Bip && dis[x] < Bi)
            {
                K.push_back(std::make_pair(x, dis[x]));
            }
        }

        D.BatchPrepend(K);
    }

    Length Bp = std::min(B, Bip);

    for (const Vertex& x : W)
    {
        if (dis[x] < Bp)
        {
            U.insert(x);
        }
    }

    return std::make_pair(Bp, std::vector<Vertex>(U.begin(), U.end()));
}

// ============ Main Function ============
int
main()
{
    scanf("%d%d%d", &n, &m, &s);
    for (int i = 1; i <= m; ++i)
    {
        int u, v, w;
        scanf("%d%d%d", &u, &v, &w);
        add_edge(u, v, w);
    }

    for (int i = 1; i <= n; ++i)
    {
        dis[i] = INF;
    }
    dis[s] = 0;

    k = std::max(1, (int)std::ceil(std::pow(std::log2(n), 1.0 / 3.0)));
    t = std::max(1, (int)std::floor(std::pow(std::log2(n), 2.0 / 3.0)));
    l = std::max(1, (int)std::ceil(std::log2(n) / t));

    std::vector<Vertex> S;
    S.push_back(s);
    BMSSP(l, INF, S);

    for (int i = 1; i <= n; ++i)
    {
        printf("%d ", dis[i]);
    }
    printf("\n");

    return 0;
}
