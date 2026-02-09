// =========================================================
// FILE: src/state/jmt/node_type_hash.hpp
// PURPOSE: std::hash specialization for NodeKey
// CRITICAL: Required for TreeCache (unordered_map)
// =========================================================

#pragma once

#include "node_type.hpp"
#include <functional>

namespace std {

template <>
struct hash<glofica::xook::NodeKey> {
    size_t operator()(const glofica::xook::NodeKey& k) const noexcept {
        // Hash combine: version XOR path
        size_t h1 = std::hash<uint64_t>{}(k.version);
        
        // Hash nibble path bytes
        size_t h2 = 0;
        const auto& bytes = k.nibble_path.bytes();
        for (const auto& byte : bytes) {
            h2 = h2 * 31 + byte;
        }
        
        return h1 ^ (h2 << 1);
    }
};

} // namespace std
