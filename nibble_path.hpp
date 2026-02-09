// =========================================================
// FILE: src/state/jmt/nibble_path.hpp
// PURPOSE: Deterministic key encoding for Aptos JMT Port
// PHASE: 1 (Week 1-2)
// =========================================================

#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>
#include <compare>
#include <stdexcept>
#include <string>
#include <sstream>
#include <iomanip>

namespace glofica::xook {

/// @brief NibblePath - Deterministic key encoding
/// 
/// Stores a path in the JMT as a sequence of nibbles (4-bit values).
/// Packed 2 nibbles per byte for memory efficiency.
/// 
/// CRITICAL: This replaces std::vector<bool> which has non-deterministic
/// iteration order. NibblePath guarantees identical serialization across
/// all platforms and compilers.
class NibblePath {
private:
    std::vector<uint8_t> bytes_;  // Packed nibbles (2 per byte)
    size_t num_nibbles_;           // Actual number of nibbles
    
public:
    NibblePath() : num_nibbles_(0) {}
    
    /// @brief Construct from raw binary key
    static NibblePath from_binary(const std::vector<uint8_t>& key) {
        NibblePath path;
        path.bytes_ = key;
        path.num_nibbles_ = key.size() * 2;
        return path;
    }

    /// @brief Construct from packed bytes and nibble count
    static NibblePath from_bytes(const std::vector<uint8_t>& bytes, size_t num_nibbles) {
        NibblePath path;
        size_t expected_size = (num_nibbles + 1) / 2;
        if (bytes.size() > expected_size) {
            path.bytes_.assign(bytes.begin(), bytes.begin() + expected_size);
        } else {
            path.bytes_ = bytes;
        }
        path.num_nibbles_ = num_nibbles;
        
        // DETERMINISM FIX: Ensure the unused padding nibble in the last byte is always 0x0
        if (num_nibbles % 2 != 0 && !path.bytes_.empty()) {
            path.bytes_.back() &= 0xF0;
        }
        return path;
    }
    
    /// @brief Get nibble at index (0-15 value)
    [[nodiscard]] uint8_t get_nibble(size_t index) const {
        if (index >= num_nibbles_) {
            throw std::out_of_range("Nibble index out of bounds");
        }
        
        uint8_t byte = bytes_[index / 2];
        return (index % 2 == 0) ? (byte >> 4) : (byte & 0x0F);
    }
    
    /// @brief Push nibble (for incremental path construction)
    void push(uint8_t nibble) {
        if (nibble > 0x0F) {
            throw std::invalid_argument("Nibble must be 4 bits (0-15)");
        }
        
        if (num_nibbles_ % 2 == 0) {
            // Even position: high nibble of new byte
            bytes_.push_back(nibble << 4);
        } else {
            // Odd position: low nibble of existing byte
            bytes_.back() |= (nibble & 0x0F);
        }
        num_nibbles_++;
    }
    
    /// @brief Pop nibble (for backtracking)
    void pop() {
        if (num_nibbles_ == 0) return;
        
        // FIXED: Reversed logic in XOOK port
        // If count is even (2, 4...), last nibble was in the LOW bits (index 1, 3...).
        // If count is odd (1, 3...), last nibble was in the HIGH bits (index 0, 2...).
        if (num_nibbles_ % 2 == 0) {
            // Removing low nibble: clear the bits but keep the byte
            bytes_.back() &= 0xF0;
        } else {
            // Removing high nibble: this was the first nibble of the byte, so pop it
            bytes_.pop_back();
        }
        num_nibbles_--;
    }
    
    /// @brief Get number of nibbles
    [[nodiscard]] size_t size() const { return num_nibbles_; }
    
    /// @brief Check if empty
    [[nodiscard]] bool empty() const { return num_nibbles_ == 0; }
    
    /// @brief Get underlying bytes (for serialization)
    [[nodiscard]] const std::vector<uint8_t>& bytes() const { return bytes_; }
    
    /// @brief C++20 three-way comparison (deterministic ordering)
    auto operator<=>(const NibblePath& other) const {
        // 1. Compare length first
        if (auto cmp = num_nibbles_ <=> other.num_nibbles_; cmp != 0) {
            return cmp;
        }
        // 2. Compare bytes lexicographically
        return bytes_ <=> other.bytes_;
    }
    
    bool operator==(const NibblePath& other) const = default;
    
    /// @brief Convert to hex string (for debugging)
    [[nodiscard]] std::string to_hex() const {
        std::ostringstream oss;
        for (size_t i = 0; i < num_nibbles_; ++i) {
            oss << std::hex << static_cast<int>(get_nibble(i));
        }
        return oss.str();
    }
};

} // namespace glofica::xook
