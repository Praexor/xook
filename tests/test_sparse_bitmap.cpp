// =========================================================
// FILE: tests/xook/test_sparse_bitmap.cpp
// PURPOSE: Unit tests for SparseBitmap functionality
// =========================================================

#include "../../src/xook/sparse_bitmap.hpp"
#include <iostream>
#include <cassert>

using namespace glofica::xook;

void test_basic_operations() {
    std::cout << "Testing basic operations..." << std::endl;
    
    SparseBitmap bitmap;
    
    // Test empty
    assert(bitmap.empty());
    assert(bitmap.total_children() == 0);
    
    // Test set and exists
    bitmap.set(3);
    assert(bitmap.exists(3));
    assert(!bitmap.exists(0));
    assert(bitmap.total_children() == 1);
    
    bitmap.set(7);
    bitmap.set(15);
    assert(bitmap.exists(7));
    assert(bitmap.exists(15));
    assert(bitmap.total_children() == 3);
    
    std::cout << "✅ Basic operations PASS" << std::endl;
}

void test_index_mapping() {
    std::cout << "Testing POPCNT index mapping..." << std::endl;
    
    SparseBitmap bitmap;
    bitmap.set(3);
    bitmap.set(7);
    bitmap.set(15);
    
    // Verify indices are sequential
    assert(bitmap.get_index(3) == 0);   // First child
    assert(bitmap.get_index(7) == 1);   // Second child
    assert(bitmap.get_index(15) == 2);  // Third child
    
    std::cout << "✅ Index mapping PASS" << std::endl;
}

void test_all_positions() {
    std::cout << "Testing all 16 positions..." << std::endl;
    
    SparseBitmap bitmap;
    
    // Set all positions
    for (uint8_t i = 0; i < 16; ++i) {
        bitmap.set(i);
    }
    
    // Verify all exist
    for (uint8_t i = 0; i < 16; ++i) {
        assert(bitmap.exists(i));
        assert(bitmap.get_index(i) == i);  // Sequential when all set
    }
    
    assert(bitmap.total_children() == 16);
    assert(bitmap.raw_mask() == 0xFFFF);
    
    std::cout << "✅ All positions PASS" << std::endl;
}

void test_serialization() {
    std::cout << "Testing serialization..." << std::endl;
    
    SparseBitmap bitmap;
    bitmap.set(0);
    bitmap.set(5);
    bitmap.set(10);
    bitmap.set(15);
    
    uint16_t mask = bitmap.raw_mask();
    
    // Reconstruct
    SparseBitmap bitmap2(mask);
    
    assert(bitmap2.exists(0));
    assert(bitmap2.exists(5));
    assert(bitmap2.exists(10));
    assert(bitmap2.exists(15));
    assert(bitmap2.total_children() == 4);
    assert(bitmap2.raw_mask() == mask);
    
    std::cout << "✅ Serialization PASS" << std::endl;
}

void test_memory_size() {
    std::cout << "Testing memory size..." << std::endl;
    
    SparseBitmap bitmap;
    size_t size = sizeof(bitmap);
    
    assert(size == 2);  // Must be exactly 2 bytes
    
    std::cout << "✅ Memory size is 2 bytes PASS" << std::endl;
}

int main() {
    std::cout << "=== SparseBitmap Unit Tests ===" << std::endl;
    std::cout << std::endl;
    
    try {
        test_basic_operations();
        test_index_mapping();
        test_all_positions();
        test_serialization();
        test_memory_size();
        
        std::cout << std::endl;
        std::cout << "=== All SparseBitmap Tests PASSED ===" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    }
}
