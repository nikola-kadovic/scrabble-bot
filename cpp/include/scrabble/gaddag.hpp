#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "letter.hpp"

namespace scrabble {

// The delimiter character ◇ (U+25C7) encoded as UTF-8.
// On MSVC, compile with /utf-8 so string literals are treated as UTF-8.
constexpr const char* DELIMITER = "\xE2\x97\x87";

// Arc index for the DELIMITER arc in the State::arcs array.
// Indices 0-25: Letters A-Z, 26: BLANK, 27: DELIMITER.
constexpr int DELIMITER_ARC_INDEX = 27;

// Bitmask helpers for State::letters_that_make_a_word (one bit per letter A-Z + BLANK).
inline constexpr bool ltmaw_has(uint32_t mask, Letter l) noexcept {
  return (mask >> static_cast<uint8_t>(l)) & 1u;
}
inline constexpr uint32_t ltmaw_add(uint32_t mask, Letter l) noexcept {
  return mask | (1u << static_cast<uint8_t>(l));
}
inline constexpr bool ltmaw_has(uint32_t mask, int idx) noexcept { return (mask >> idx) & 1u; }
inline constexpr uint32_t ltmaw_add(uint32_t mask, int idx) noexcept { return mask | (1u << idx); }

class State;

struct Arc {
  std::shared_ptr<State> destination_state;

  Arc();
  // Non-owning: wraps a raw pointer from the arena with a no-op deleter.
  explicit Arc(State* dest);
};

class State {
 public:
  // Raw (non-owning) pointers — owned by Gaddag::states_ arena.
  std::array<State*, 28> arcs{};
  uint32_t letters_that_make_a_word = 0;
  // Keeps Python-created child states alive when add_arc is called from
  // Python bindings on a standalone State (not arena-managed).
  std::vector<std::shared_ptr<State>> python_children;

  State() = default;

  void add_ending_letter(char letter);

  // Returns the next state for `letter` string key, or nullptr if no such arc exists.
  const State* get_next_state(const std::string& letter) const;

  // Returns the next state for arc index (0-25=A-Z, 26=BLANK, 27=DELIMITER),
  // or nullptr if no such arc exists. Faster than the string overload.
  const State* get_next_state(int arc_index) const;
};

class Gaddag {
 public:
  State* root = nullptr;

  Gaddag();

  // Build GADDAG from an in-memory word list (no caching).
  void build_from_words(const std::vector<std::string>& words);

  // Build GADDAG from a wordlist file. Tries binary cache first when
  // use_cache=true. Cache is stored in temp/gaddag_<stem>.bin.
  void build_from_file(const std::string& wordlist_path, bool use_cache = true);

  // Add a single word to the GADDAG.
  void add_word(const std::string& word);

  // Returns all letter keys present at the root (excludes DELIMITER).
  std::unordered_set<Letter> get_dictionary_letters() const;

 private:
  std::vector<std::unique_ptr<State>> states_;

  State* alloc_state();

  // Get or create the arc at arc_index from s, allocating a new state if needed.
  State* ensure_arc(State* s, int arc_index);

  // Get or set the arc at arc_index from s to dest; if arc already exists,
  // leaves it unchanged. Always returns the arc's destination.
  State* ensure_arc(State* s, int arc_index, State* dest);

  std::string cache_path_for(const std::string& wordlist_path) const;
  bool load_cache(const std::string& cache_path);
  void save_cache(const std::string& cache_path) const;
};

}  // namespace scrabble
