// =========================================================
// FILE: src/xook/xook_adapter.hpp
// PURPOSE: Legacy API Bridge for XOOK Merkle Tree
// INNOVATION: Provides 0% error without touching State.cpp
// =========================================================

#pragma once

#include "xook_merkle_tree.hpp"
#include "../common/hash.hpp"
#include "../kv/kv_store.hpp" // Added dependency
#include <memory>
#include <unordered_map>

namespace glofica::xook {

/// @brief Complete adapter providing legacy JMT API over XOOK
/// 
/// Design Pattern: Accumulator + Batch Flush
/// - put() accumulates individual updates
/// - calculate_root() flushes batch to XOOK with deterministic sorting
/// 
/// This allows State.cpp to remain unchanged while getting 0% error rate.
/// @brief Isolated cache overlay to prevent speculative execution from polluting the main cache
class SpeculativeTreeCache : public TreeCache {
private:
    TreeCache* base_cache_;
    std::unordered_map<NodeKey, Node> overlay_;
    std::unordered_map<NodeKey, Node> injected_;

public:
    explicit SpeculativeTreeCache(TreeCache* base) : TreeCache(0), base_cache_(base) {}
    
    void inject_node(const NodeKey& key, const Node& node) {
        injected_[key] = node;
    }

    std::optional<Node> get(const NodeKey& key) override {
        auto it = overlay_.find(key);
        if (it != overlay_.end()) return it->second;
        
        auto it_inj = injected_.find(key);
        if (it_inj != injected_.end()) return it_inj->second;

        return base_cache_ ? base_cache_->get(key) : std::nullopt;
    }

    void put(const NodeKey& key, const Node& node) override {
        overlay_[key] = node;
    }

    void clear() override {
        overlay_.clear();
        injected_.clear();
    }
    
    size_t size() const override {
        return overlay_.size() + injected_.size();
    }
};

class XookAdapter {

private:
    std::unique_ptr<TreeCache> cache_;
    std::unique_ptr<XookTree> tree_;


    /// @brief Reader that delegates to the external KVStore (WAL/Snapshot)
    class ExternalReader : public TreeReader {
    private:
        kv::KVStore* db_;
    public:
        explicit ExternalReader(kv::KVStore* db) : db_(db) {}

        std::optional<glofica::Bytes> get_node_bytes(const NodeKey& key) override {
            if (!db_) return std::nullopt;

            // Serialize key for KVStore lookup:
            // Version (8 bytes) + NibblePath (variable)
             glofica::Bytes key_bytes = key.serialize();

            // Query DB
            return db_->get(key_bytes);
        }
    };
    
    std::shared_ptr<TreeReader> reader_;
    
    // Pending updates accumulator
    std::unordered_map<glofica::Hash, glofica::Bytes, hash::HashPtr> pending_updates_;
    uint64_t current_version_ = 0;
    glofica::Hash last_root_{};
    
public:
    // Accepts pointer to global KVStore
    explicit XookAdapter(kv::KVStore* db = nullptr) {
        // Use ExternalReader if DB provided, otherwise fallback to InMemory (test mode)
        // If db is null (default), ExternalReader handles it by returning nullopt safely.
        reader_ = std::make_shared<ExternalReader>(db);
        
        // Configure cache (100K nodes)
        cache_ = std::make_unique<TreeCache>(100000);
        
        tree_ = std::make_unique<XookTree>(
            reader_.get(), 
            cache_.get()
        );
        
        last_root_.fill(0);
    }
    
    // ===== LEGACY API IMPLEMENTATION =====
    
    /// @brief Legacy put() - accumulates single key-value pair
    /// @param key Account key (will be hashed to Hash)
    /// @param value_hash Hash of the account value
    /// @param version Version number (tracked but batched later)
    void put(const glofica::Bytes& key, const glofica::Hash& value_hash, uint64_t version) {
        // FIXED: Use BLAKE3-512 for deterministic key hashing (Story 22.1)
        // Manual splicing was non-deterministic for 33-byte keys
        glofica::Hash key_hash = hash::blake3(key);
        
        // Store value_hash as bytes (JMT stores values, not hashes)
        glofica::Bytes value_bytes(value_hash.begin(), value_hash.end());
        pending_updates_[key_hash] = value_bytes;
        current_version_ = version;
    }
    
    /// @brief Calculate root purely speculatively (no cache pollution)
    TreeUpdateBatch calculate_root_speculative(
        const std::vector<std::pair<glofica::Bytes, glofica::Hash>>& updates,
        const glofica::Hash& base_root,
        uint64_t version,
        std::optional<uint64_t> base_version = std::nullopt,
        const std::vector<std::pair<glofica::Bytes, glofica::Bytes>>* parent_nodes = nullptr
    ) {
         // Speculative cache
         SpeculativeTreeCache spec_cache(cache_.get());
         
         // Inject parent speculative nodes (if any)
         if (parent_nodes) {
             for (const auto& [node_key_bytes, node_val_bytes] : *parent_nodes) {
                 auto node_key_opt = NodeKey::deserialize(node_key_bytes);
                 auto node_val_opt = deserialize_node_from_bytes(node_val_bytes);
                 if (node_key_opt && node_val_opt) {
                     spec_cache.inject_node(*node_key_opt, *node_val_opt);
                 }
             }
         }
         
         // Speculative tree using existing reader but isolated cache
         XookTree spec_tree(reader_.get(), &spec_cache);
 
         // Convert to JMT format
         std::vector<std::pair<glofica::Hash, std::optional<glofica::Bytes>>> jmt_updates;
         jmt_updates.reserve(updates.size());
         
         for (const auto& [key, value_hash] : updates) {
            // FIXED: Use BLAKE3-512 (Story 22.1)
            glofica::Hash key_hash = hash::blake3(key);
            glofica::Bytes value_bytes(value_hash.begin(), value_hash.end());
            jmt_updates.emplace_back(key_hash, value_bytes);
         }
         
         // Execute speculative update
         auto result = spec_tree.put_value_set(jmt_updates, version, base_root, base_version);
         return result;
    }

    /// @brief Legacy calculate_root() - flushes accumulated updates to JMT
    /// @return Complete batch result including nodes for persistence
    TreeUpdateBatch calculate_root(
        const std::vector<std::pair<glofica::Bytes, glofica::Hash>>& updates,
        const glofica::Hash& base_root,
        uint64_t version,
        std::optional<uint64_t> base_version = std::nullopt
    ) {
        // Merge explicit updates with pending updates
        std::vector<std::pair<glofica::Hash, glofica::Bytes>> batch;
        batch.reserve(updates.size() + pending_updates_.size());
        
        // Add explicit updates
        for (const auto& [key, value_hash] : updates) {
            // FIXED: Use BLAKE3-512 (Story 22.1)
            glofica::Hash key_hash = hash::blake3(key);
            glofica::Bytes value_bytes(value_hash.begin(), value_hash.end());
            batch.emplace_back(key_hash, value_bytes);
        }
        
        // Add pending updates
        for (const auto& [k, v] : pending_updates_) {
            batch.emplace_back(k, v);
        }
        
        // If no updates, return base root
        if (batch.empty()) {
            TreeUpdateBatch empty;
            empty.new_root_hash = base_root;
            return empty;
        }
        
        // Convert to optional format (no deletions in this path)
        std::vector<std::pair<glofica::Hash, std::optional<glofica::Bytes>>> jmt_updates;
        jmt_updates.reserve(batch.size());
        for (const auto& [k, v] : batch) {
            jmt_updates.emplace_back(k, v);
        }
        
        // CRITICAL: Apply batch to JMT (deterministic sorting happens here)
        // Passes base_root and base_version to support correct speculative execution
        auto result = tree_->put_value_set(jmt_updates, version, base_root, base_version);
        
        // Clear pending updates
        pending_updates_.clear();
        current_version_ = version;
        last_root_ = result.new_root_hash;
        
        return result;
    }
    
    /// @brief Get root hash at specific version
    glofica::Hash get_root_hash(uint64_t version) const {
        if (version == current_version_) {
            return last_root_;
        }
        return tree_->get_root_hash(version);
    }
    
    /// @brief Get value at specific key and version
    std::optional<glofica::Hash> get(const glofica::Bytes& key, uint64_t version) const {
        // FIXED: Use BLAKE3-512 for deterministic key hashing (Story 22.1)
        glofica::Hash key_hash = hash::blake3(key);
        
        auto result = tree_->get(key_hash, version);
        if (!result.has_value()) {
            return std::nullopt;
        }
        
        // Convert bytes back to Hash (Full 64B)
        glofica::Hash value_hash;
        if (result->size() >= 64) {
            std::copy(result->begin(), result->begin() + 64, value_hash.begin());
        } else {
            std::fill(value_hash.begin(), value_hash.end(), 0);
            std::copy(result->begin(), result->end(), value_hash.begin());
        }
        
        return value_hash;
    }

    /// @brief Batch update with precomputed hashes (legacy optimization path)
    TreeUpdateBatch update_batch_with_precomputed_hashes(
        const std::vector<std::pair<glofica::Bytes, glofica::Hash>>& updates,
        uint64_t version,
        std::optional<glofica::Hash> base_root = std::nullopt,
        std::optional<uint64_t> base_version = std::nullopt
    ) {
        // Convert to JMT format and apply
        std::vector<std::pair<glofica::Hash, std::optional<glofica::Bytes>>> jmt_updates;
        jmt_updates.reserve(updates.size());
        
        for (const auto& [key, value_hash] : updates) {
            // FIXED: Use BLAKE3-512 (Story 22.1)
            glofica::Hash key_hash = hash::blake3(key);
            
            // Store hash as value bytes
            glofica::Bytes value_bytes(value_hash.begin(), value_hash.end());
            jmt_updates.emplace_back(key_hash, value_bytes);
        }
        
        // Apply batch (Fixed: pass base_root and base_version to support rollback recovery)
        auto result = tree_->put_value_set(jmt_updates, version, base_root, base_version);
        last_root_ = result.new_root_hash;
        current_version_ = version;
        return result;
    }
    
    /// @brief Get cache statistics (for monitoring)
    size_t cache_size() const {
        return cache_->size();
    }
};

} // namespace glofica::xook
