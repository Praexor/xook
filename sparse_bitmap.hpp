// =========================================================
// FILE: src/xook/sparse_bitmap.hpp
// PURPOSE: XOOK Innovation - Hardware-Accelerated Navigation
// AUTHOR: German Malave (Original Work)
// =========================================================

#pragma once
#include <cstdint>
#include <bit>  // C++20 for std::popcount

namespace glofica::xook {

/// @brief SparseBitmap - XOOK's core innovation for memory optimization
/// 
/// Reduces InternalNode memory from 640 bytes to ~100 bytes using:
/// - 16-bit bitmap (2 bytes) instead of array of optionals
/// - POPCNT CPU instruction for O(1) index mapping
/// - Dense vector storage for actual hashes
/// 
/// Example:
///   Children at nibbles: 3, 7, 15
///   Bitmap: 0b1000000010001000 (bits 3, 7, 15 set)
///   Vector: [hash_3, hash_7, hash_15] (only 3 hashes stored)
///   
///   get_index(7) -> POPCNT(0b0000000010001000) = 1 (second position)
class SparseBitmap {
private:
    uint16_t mask_ = 0;  // 16 bits for 16 possible children (nibbles 0-15)

public:
    constexpr SparseBitmap() = default;
    explicit constexpr SparseBitmap(uint16_t mask) : mask_(mask) {}

    /// @brief Check if a child exists at the given nibble
    /// @param nibble Position to check (0-15)
    /// @return true if child exists
    [[nodiscard]] constexpr bool exists(uint8_t nibble) const noexcept {
        return (mask_ >> nibble) & 1;
    }

    /// @brief Calculate physical index in dense vector using POPCNT
    /// 
    /// This is the XOOK innovation: Hardware instruction counts set bits
    /// before the nibble position, giving us the dense vector index.
    /// 
    /// @param nibble Logical position (0-15)
    /// @return Physical index in child_hashes vector
    [[nodiscard]] uint8_t get_index(uint8_t nibble) const noexcept {
        // Create mask with all bits before nibble set
        uint16_t mask_before = (1 << nibble) - 1;
        
        // Count set bits using POPCNT instruction
        return static_cast<uint8_t>(std::popcount(
            static_cast<uint16_t>(mask_ & mask_before)
        ));
    }

    /// @brief Activate a bit (mark child as existing)
    /// @param nibble Position to activate (0-15)
    void set(uint8_t nibble) noexcept { 
        mask_ |= (1 << nibble); 
    }

    /// @brief Get raw bitmap for serialization
    /// @return 16-bit mask
    [[nodiscard]] uint16_t raw_mask() const noexcept { 
        return mask_; 
    }

    /// @brief Count total number of children
    /// @return Number of set bits (0-16)
    [[nodiscard]] size_t total_children() const noexcept { 
        return std::popcount(mask_); 
    }

    /// @brief Clear all bits
    void clear() noexcept {
        mask_ = 0;
    }

    /// @brief Check if empty
    [[nodiscard]] bool empty() const noexcept {
        return mask_ == 0;
    }
};

} // namespace glofica::xook
