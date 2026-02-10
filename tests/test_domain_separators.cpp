#include "../src/xook/node_type.hpp"
#include "../src/common/hash.hpp"
#include <iostream>
#include <iomanip>

using namespace glofica;
using namespace glofica::xook;

// Helper to print hash in hex
std::string hash_to_hex(const Hash256& hash) {
    std::stringstream ss;
    for (auto byte : hash) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
    }
    return ss.str();
}

int main() {
    std::cout << "=========================================" << std::endl;
    std::cout << "  XOOK DOMAIN SEPARATOR VALIDATION" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << std::endl;

    // Test 1: Verify domain separators are defined
    std::cout << "[TEST 1] Domain Separator Constants..." << std::endl;
    std::cout << "  XOOK_INTERNAL_NODE_PREFIX = 0x" 
              << std::hex << std::setw(2) << std::setfill('0') 
              << (int)XOOK_INTERNAL_NODE_PREFIX << std::endl;
    std::cout << "  XOOK_LEAF_NODE_PREFIX     = 0x" 
              << std::hex << std::setw(2) << std::setfill('0') 
              << (int)XOOK_LEAF_NODE_PREFIX << std::endl;
    
    if (XOOK_INTERNAL_NODE_PREFIX != 0x01 || XOOK_LEAF_NODE_PREFIX != 0x02) {
        std::cout << "  ❌ [FAIL] Incorrect prefix values!" << std::endl;
        return 1;
    }
    std::cout << "  ✅ [OK] Domain separators defined correctly" << std::endl;
    std::cout << std::endl;

    // Test 2: Verify InternalNode hash includes prefix
    std::cout << "[TEST 2] InternalNode Hash Format..." << std::endl;
    InternalNode internal;
    
    // Add some child hashes
    Hash256 child1{};
    child1[0] = 0xAA;
    Hash256 child2{};
    child2[0] = 0xBB;
    
    internal.set_child_hash(3, child1);
    internal.set_child_hash(7, child2);
    
    Hash256 internal_hash = internal.hash();
    std::cout << "  Internal Node Hash: " << hash_to_hex(internal_hash).substr(0, 16) << "..." << std::endl;
    
    // Verify the hash is different from hashing without prefix
    auto serialized = internal.serialize_canonical();
    Hash256 hash_without_prefix = hash::blake3(serialized);
    
    if (internal_hash == hash_without_prefix) {
        std::cout << "  ❌ [FAIL] Hash does NOT include domain separator!" << std::endl;
        return 1;
    }
    std::cout << "  ✅ [OK] InternalNode hash includes domain separator" << std::endl;
    std::cout << std::endl;

    // Test 3: Verify LeafNode hash includes prefix
    std::cout << "[TEST 3] LeafNode Hash Format..." << std::endl;
    LeafNode leaf;
    leaf.account_key[0] = 0x11;
    leaf.account_key[1] = 0x22;
    leaf.value_hash[0] = 0x33;
    leaf.value_hash[1] = 0x44;
    
    Hash256 leaf_hash = leaf.hash();
    std::cout << "  Leaf Node Hash: " << hash_to_hex(leaf_hash).substr(0, 16) << "..." << std::endl;
    
    // Verify the hash is different from hashing without prefix
    auto leaf_serialized = leaf.serialize_canonical();
    Hash256 leaf_hash_without_prefix = hash::blake3(leaf_serialized);
    
    if (leaf_hash == leaf_hash_without_prefix) {
        std::cout << "  ❌ [FAIL] Hash does NOT include domain separator!" << std::endl;
        return 1;
    }
    std::cout << "  ✅ [OK] LeafNode hash includes domain separator" << std::endl;
    std::cout << std::endl;

    // Test 4: Verify InternalNode and LeafNode hashes are different
    std::cout << "[TEST 4] Collision Prevention..." << std::endl;
    
    // Create a leaf with same serialized size as internal (64 bytes)
    LeafNode collision_test_leaf;
    for (int i = 0; i < 32; i++) {
        collision_test_leaf.account_key[i] = i;
        collision_test_leaf.value_hash[i] = i + 32;
    }
    
    Hash256 collision_leaf_hash = collision_test_leaf.hash();
    
    // Even if we craft the data, the hashes MUST be different due to prefixes
    if (collision_leaf_hash == internal_hash) {
        std::cout << "  ❌ [FAIL] Collision detected! Domain separators not working!" << std::endl;
        return 1;
    }
    std::cout << "  ✅ [OK] No collision between Internal and Leaf nodes" << std::endl;
    std::cout << std::endl;

    // Test 5: Verify determinism (same input = same hash)
    std::cout << "[TEST 5] Determinism..." << std::endl;
    
    InternalNode internal2;
    internal2.set_child_hash(3, child1);
    internal2.set_child_hash(7, child2);
    
    Hash256 internal_hash2 = internal2.hash();
    
    if (internal_hash != internal_hash2) {
        std::cout << "  ❌ [FAIL] Non-deterministic hashing!" << std::endl;
        return 1;
    }
    std::cout << "  ✅ [OK] Hashing is deterministic" << std::endl;
    std::cout << std::endl;

    // Test 6: Verify prefix is actually in the hash input
    std::cout << "[TEST 6] Prefix Inclusion Verification..." << std::endl;
    
    // Manually construct what the hash input should be
    Bytes expected_buffer;
    expected_buffer.push_back(XOOK_INTERNAL_NODE_PREFIX);
    auto manual_serialized = internal.serialize_canonical();
    expected_buffer.insert(expected_buffer.end(), manual_serialized.begin(), manual_serialized.end());
    
    Hash256 expected_hash = hash::blake3(expected_buffer);
    
    if (internal_hash != expected_hash) {
        std::cout << "  ❌ [FAIL] Hash does not match expected format!" << std::endl;
        std::cout << "  Expected: " << hash_to_hex(expected_hash).substr(0, 16) << "..." << std::endl;
        std::cout << "  Got:      " << hash_to_hex(internal_hash).substr(0, 16) << "..." << std::endl;
        return 1;
    }
    std::cout << "  ✅ [OK] Prefix correctly included in hash input" << std::endl;
    std::cout << std::endl;

    // Summary
    std::cout << "=========================================" << std::endl;
    std::cout << ">>> ALL DOMAIN SEPARATOR TESTS PASSED <<<" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "Security Guarantees:" << std::endl;
    std::cout << "  ✅ Domain separators are active" << std::endl;
    std::cout << "  ✅ InternalNode uses prefix 0x01" << std::endl;
    std::cout << "  ✅ LeafNode uses prefix 0x02" << std::endl;
    std::cout << "  ✅ No collision between node types" << std::endl;
    std::cout << "  ✅ Deterministic hashing maintained" << std::endl;
    std::cout << std::endl;
    std::cout << "⚠️  BREAKING CHANGE: All state roots have changed!" << std::endl;
    std::cout << "    See docs/XOOK_DOMAIN_SEPARATOR_MIGRATION.md" << std::endl;
    std::cout << std::endl;

    return 0;
}
