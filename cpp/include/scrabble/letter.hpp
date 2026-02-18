#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_set>

namespace scrabble {

enum class Letter : uint8_t {
    A = 0, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    BLANK = 26
};

// Scrabble letter scores (indexed by Letter cast to uint8_t)
// Source: https://en.wikipedia.org/wiki/Scrabble_letter_distributions
inline constexpr std::array<int, 27> LETTER_SCORES = {
    1,  // A
    3,  // B
    3,  // C
    2,  // D
    1,  // E
    4,  // F
    2,  // G
    4,  // H
    1,  // I
    8,  // J
    5,  // K
    1,  // L
    3,  // M
    1,  // N
    1,  // O
    3,  // P
    10, // Q
    1,  // R
    1,  // S
    1,  // T
    1,  // U
    4,  // V
    4,  // W
    8,  // X
    4,  // Y
    10, // Z
    0   // BLANK
};

constexpr char letter_to_char(Letter l) noexcept {
    if (l == Letter::BLANK) return '_';
    return static_cast<char>('A' + static_cast<uint8_t>(l));
}

constexpr Letter char_to_letter(char c) noexcept {
    if (c == '_') return Letter::BLANK;
    return static_cast<Letter>(c - 'A');
}

// Returns the single-char string key used in GADDAG arc maps
inline std::string letter_to_key(Letter l) {
    return std::string(1, letter_to_char(l));
}

constexpr int get_letter_score(Letter l) noexcept {
    return LETTER_SCORES[static_cast<uint8_t>(l)];
}

} // namespace scrabble

// Hash specialization must appear before any unordered container uses Letter
template <>
struct std::hash<scrabble::Letter> {
    size_t operator()(scrabble::Letter l) const noexcept {
        return static_cast<size_t>(l);
    }
};

namespace scrabble {

inline std::unordered_set<Letter> get_all_letters() {
    std::unordered_set<Letter> result;
    for (int i = 0; i <= 26; i++) {
        result.insert(static_cast<Letter>(i));
    }
    return result;
}

} // namespace scrabble
