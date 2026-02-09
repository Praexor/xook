// =========================================================
// FILE: src/state/jmt/node_serde.hpp
// PURPOSE: Node serialization/deserialization
// PHASE: 2 (Week 3-4)
// =========================================================

#pragma once

#include "node_type.hpp"

namespace glofica::xook {

/// @brief Serialize node with type prefix
inline glofica::Bytes serialize_node_with_prefix(const Node& node) {
    glofica::Bytes result;
    
    if (std::holds_alternative<InternalNode>(node)) {
        result.push_back(0x01);  // Internal node marker
        auto serialized = std::get<InternalNode>(node).serialize_canonical();
        result.insert(result.end(), serialized.begin(), serialized.end());
    } else {
        result.push_back(0x02);  // Leaf node marker
        auto serialized = std::get<LeafNode>(node).serialize_canonical();
        result.insert(result.end(), serialized.begin(), serialized.end());
    }
    
    return result;
}

/// @brief Deserialize node from bytes
inline std::optional<Node> deserialize_node_from_bytes(const glofica::Bytes& bytes) {
    if (bytes.empty()) {
        return std::nullopt;
    }
    
    uint8_t type = bytes[0];
    
    if (type == 0x01) {
        // Internal node
        if (bytes.size() < 3) return std::nullopt;
        
        size_t pos = 1; // Start after type byte
        
        // Deserialize InternalNode
        InternalNode internal;
        
        // Read bitmap (2 bytes, little-endian)
        uint16_t bitmap_mask = bytes[pos] | (bytes[pos + 1] << 8);
        pos += 2;
        
        // Create SparseBitmap from mask
        internal.bitmap = SparseBitmap(bitmap_mask);
        
        // Read child data (only for existing children)
        size_t num_children = internal.bitmap.total_children();
        internal.children.reserve(num_children);
        
        for (size_t i = 0; i < num_children; ++i) {
            // Each child has 64 bytes (hash) + 8 bytes (version) = 72 bytes
            if (pos + 72 > bytes.size()) {
                return std::nullopt; // Truncated InternalNode data
            }
            
            ChildInfo info;
            // Read hash (64 bytes)
            std::copy(bytes.begin() + pos, bytes.begin() + pos + 64, info.hash.begin());
            pos += 64;
            
            // Read version (8 bytes, little-endian)
            info.version = 0;
            for (int bit = 0; bit < 8; ++bit) {
                info.version |= static_cast<uint64_t>(bytes[pos + bit]) << (bit * 8);
            }
            pos += 8;
            
            internal.children.push_back(info);
        }

        // CRITICAL FIX: Strict length check
        if (pos != bytes.size()) {
            return std::nullopt; // Extra bytes found (non-canonical)
        }
        
        return internal;
        
    } else if (type == 0x02) {
        // Leaf node
        // CRITICAL FIX: Strict length check
        if (bytes.size() != 129) return std::nullopt;  // Exact length: 1 + 64 + 64
        
        LeafNode leaf;
        
        // Read account key (64 bytes)
        std::copy(bytes.begin() + 1,
                 bytes.begin() + 65,
                 leaf.account_key.begin());
        
        // Read value hash (64 bytes)
        std::copy(bytes.begin() + 65,
                 bytes.begin() + 129,
                 leaf.value_hash.begin());
        
        return leaf;
    }
    
    return std::nullopt;
}

} // namespace glofica::xook
