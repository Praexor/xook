
// =========================================================
// FILE: src/xook/node_type.hpp
// PURPOSE: XOOK Node Structures with SparseBitmap Innovation
// INNOVATION: High-Security Quantum-Safe Hashes (64B)
// =========================================================

#pragma once

#include "nibble_path.hpp"
#include "sparse_bitmap.hpp"
#include "../common/hash.hpp"
#include <array>
#include <optional>
#include <variant>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace glofica::xook {

// Domain separators for cryptographic hashing
inline const std::string XOOK_INTERNAL_NODE_DOMAIN = "GLOFICA_InternalNode_V2_PQ";
inline const std::string XOOK_LEAF_NODE_DOMAIN     = "GLOFICA_LeafNode_V2_PQ";

/// @brief Information about a child node (hash + version)
struct ChildInfo {
    Hash hash;
    uint64_t version;
};

/// @brief Internal node in XOOK Merkle Tree
/// 
/// Quantum-Safe Version: Uses 64-byte hashes for full 256-bit security 
/// against quantum collision attacks.
struct InternalNode {
    SparseBitmap bitmap;
    std::vector<ChildInfo> children;
    
    [[nodiscard]] std::optional<ChildInfo> get_child(uint8_t nibble) const {
        if (!bitmap.exists(nibble)) return std::nullopt;
        return children[bitmap.get_index(nibble)];
    }
    
    void set_child(uint8_t nibble, const Hash& hash, uint64_t version) {
        ChildInfo info{hash, version};
        if (!bitmap.exists(nibble)) {
            bitmap.set(nibble);
            auto idx = bitmap.get_index(nibble);
            children.insert(children.begin() + idx, info);
        } else {
            auto idx = bitmap.get_index(nibble);
            children[idx] = info;
        }
    }
    
    /// @brief Canonical serialization for Quantum-Safe nodes
    /// Format:
    /// - 2 bytes: bitmap
    /// - N×(64+8) bytes: (hash + version)
    [[nodiscard]] Bytes serialize_canonical() const {
        Bytes buffer;
        const size_t CHILD_RECORD_SIZE = 64 + 8;
        buffer.reserve(2 + children.size() * CHILD_RECORD_SIZE);
        
        uint16_t mask = bitmap.raw_mask();
        buffer.push_back(static_cast<uint8_t>(mask & 0xFF));
        buffer.push_back(static_cast<uint8_t>((mask >> 8) & 0xFF));
        
        for (const auto& child : children) {
            buffer.insert(buffer.end(), child.hash.begin(), child.hash.end());
            for (int i = 0; i < 8; ++i) {
                buffer.push_back(static_cast<uint8_t>((child.version >> (i * 8)) & 0xFF));
            }
        }
        return buffer;
    }
    
    [[nodiscard]] Hash hash() const {
        Bytes final_buffer;
        final_buffer.reserve(XOOK_INTERNAL_NODE_DOMAIN.size() + 2 + children.size() * 72);
        final_buffer.insert(final_buffer.end(), XOOK_INTERNAL_NODE_DOMAIN.begin(), XOOK_INTERNAL_NODE_DOMAIN.end());
        auto serialized = serialize_canonical();
        final_buffer.insert(final_buffer.end(), serialized.begin(), serialized.end());
        return hash::blake3(final_buffer);
    }
    
    [[nodiscard]] bool is_empty() const { return bitmap.empty(); }
    [[nodiscard]] size_t child_count() const { return bitmap.total_children(); }
};

/// @brief Leaf node in Quantum-Safe XOOK
struct LeafNode {
    Hash account_key;
    Hash value_hash;
    
    [[nodiscard]] Bytes serialize_canonical() const {
        Bytes buffer;
        buffer.reserve(128); // 64 + 64
        buffer.insert(buffer.end(), account_key.begin(), account_key.end());
        buffer.insert(buffer.end(), value_hash.begin(), value_hash.end());
        return buffer;
    }
    
    [[nodiscard]] Hash hash() const {
        Bytes final_buffer;
        final_buffer.reserve(XOOK_LEAF_NODE_DOMAIN.size() + 128);
        final_buffer.insert(final_buffer.end(), XOOK_LEAF_NODE_DOMAIN.begin(), XOOK_LEAF_NODE_DOMAIN.end());
        auto serialized = serialize_canonical();
        final_buffer.insert(final_buffer.end(), serialized.begin(), serialized.end());
        return hash::blake3(final_buffer);
    }
};

using Node = std::variant<InternalNode, LeafNode>;

struct NodeKey {
    uint64_t version;
    NibblePath nibble_path;
    
    auto operator<=>(const NodeKey& other) const {
        if (auto cmp = version <=> other.version; cmp != 0) return cmp;
        return nibble_path <=> other.nibble_path;
    }
    
    bool operator==(const NodeKey& other) const = default;

    [[nodiscard]] Bytes serialize() const {
        Bytes res;
        res.reserve(12 + nibble_path.size());
        for(int i=0; i<8; i++) res.push_back((version >> (i*8)) & 0xFF);
        size_t len = nibble_path.size();
        for(int i=0; i<4; i++) res.push_back((len >> (i*8)) & 0xFF);
        const auto& pb = nibble_path.bytes();
        res.insert(res.end(), pb.begin(), pb.end());
        return res;
    }

    static std::optional<NodeKey> deserialize(const Bytes& bytes) {
        if (bytes.size() < 12) return std::nullopt;
        NodeKey res;
        res.version = 0;
        for(int m=0; m<8; m++) res.version |= static_cast<uint64_t>(bytes[m]) << (m*8);
        size_t len = 0;
        for(int m=0; m<4; m++) len |= static_cast<size_t>(bytes[8+m]) << (m*8);
        if (bytes.size() < 12 + (len + 1) / 2) return std::nullopt;
        Bytes path_bytes(bytes.begin() + 12, bytes.end());
        res.nibble_path = NibblePath::from_bytes(path_bytes, len);
        return res;
    }
};

[[nodiscard]] inline Bytes serialize_node(const Node& node) {
    if (std::holds_alternative<InternalNode>(node)) {
        return std::get<InternalNode>(node).serialize_canonical();
    } else {
        return std::get<LeafNode>(node).serialize_canonical();
    }
}

[[nodiscard]] inline Hash hash_node(const Node& node) {
    if (std::holds_alternative<InternalNode>(node)) {
        return std::get<InternalNode>(node).hash();
    } else {
        return std::get<LeafNode>(node).hash();
    }
}

} // namespace glofica::xook
