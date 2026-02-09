// =========================================================
// FILE: src/state/jmt/tree_cache.hpp
// PURPOSE: LRU cache for JMT nodes (Phase 1)
// CRITICAL: Required for performance in TEE environments
// =========================================================

#pragma once

#include "node_type.hpp"
#include "node_type_hash.hpp"
#include <unordered_map>
#include <list>
#include <mutex>
#include <shared_mutex>

namespace glofica::xook {

/// @brief LRU cache for tree nodes
/// 
/// In TEE environments (SGX), EPC memory is limited (~128MB).
/// TreeCache keeps hot paths in memory while evicting cold nodes.
class TreeCache {
private:
    size_t capacity_;
    
    // LRU list (most recent at front)
    std::list<NodeKey> lru_list_;
    
    // Map: NodeKey → (Node, Iterator to LRU list)
    using ListIter = std::list<NodeKey>::iterator;
    std::unordered_map<NodeKey, std::pair<Node, ListIter>> cache_map_;
    
    // Thread-safety for Parallel VM
    mutable std::shared_mutex mutex_;
    
public:
    explicit TreeCache(size_t capacity = 100000) : capacity_(capacity) {}
    
    virtual ~TreeCache() = default;

    /// @brief Get node (promotes to MRU)
    virtual std::optional<Node> get(const NodeKey& key) {
        // LRU requires list modification, so we need exclusive lock
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            return std::nullopt;
        }
        
        // Move to front (O(1) splice)
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second.second);
        
        return it->second.first;
    }
    
    /// @brief Put node (evicts LRU if at capacity)
    virtual void put(const NodeKey& key, const Node& node) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Update existing and move to front
            lru_list_.splice(lru_list_.begin(), lru_list_, it->second.second);
            it->second.first = node;
            return;
        }
        
        // Evict LRU if at capacity
        if (cache_map_.size() >= capacity_) {
            NodeKey last = lru_list_.back();
            lru_list_.pop_back();
            cache_map_.erase(last);
        }
        
        // Insert new at front
        lru_list_.push_front(key);
        cache_map_.emplace(key, std::make_pair(node, lru_list_.begin()));
    }
    
    /// @brief Clear cache (useful between blocks)
    virtual void clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        cache_map_.clear();
        lru_list_.clear();
    }
    
    /// @brief Get cache size
    [[nodiscard]] virtual size_t size() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return cache_map_.size();
    }
    
    /// @brief Get capacity
    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }
};

} // namespace glofica::xook
