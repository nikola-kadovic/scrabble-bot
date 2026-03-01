#include "scrabble/gaddag.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace scrabble {

// ─── Arc
// ──────────────────────────────────────────────────────────────────────

Arc::Arc() : destination_state(std::make_shared<State>()) {}

// Non-owning wrap: the State is managed by the Gaddag arena; we use a
// no-op deleter so the shared_ptr never frees the object.
Arc::Arc(State* dest)
    : destination_state(dest ? std::shared_ptr<State>(dest, [](State*) {}) : nullptr) {}

// ─── State
// ────────────────────────────────────────────────────────────────────

namespace {

// Maps a string-based GADDAG key to its arc array index.
// Indices 0-25: Letters A-Z, 26: BLANK ('_'), 27: DELIMITER.
static int key_to_arc_index(const std::string& key) {
  if (key == DELIMITER) return DELIMITER_ARC_INDEX;
  char c = key[0];
  return (c == '_') ? 26 : (c - 'A');
}

}  // namespace

void State::add_ending_letter(char letter) {
  letters_that_make_a_word[static_cast<unsigned char>(letter - 'A')] = true;
}

const State* State::get_next_state(const std::string& letter) const {
  return get_next_state(key_to_arc_index(letter));
}

const State* State::get_next_state(int arc_index) const { return arcs[arc_index]; }

// ─── Gaddag
// ───────────────────────────────────────────────────────────────────

State* Gaddag::alloc_state() {
  states_.push_back(std::make_unique<State>());
  return states_.back().get();
}

State* Gaddag::ensure_arc(State* s, int arc_index) {
  if (!s->arcs[arc_index]) {
    s->arcs[arc_index] = alloc_state();
  }
  return s->arcs[arc_index];
}

State* Gaddag::ensure_arc(State* s, int arc_index, State* dest) {
  if (!s->arcs[arc_index]) {
    s->arcs[arc_index] = dest;
  }
  return s->arcs[arc_index];
}

Gaddag::Gaddag() { root = alloc_state(); }

void Gaddag::build_from_words(const std::vector<std::string>& words) {
  states_.clear();
  root = alloc_state();
  for (const auto& word : words) {
    add_word(word);
  }
}

void Gaddag::build_from_file(const std::string& wordlist_path, bool use_cache) {
  states_.clear();
  root = nullptr;

  if (use_cache) {
    std::string cp = cache_path_for(wordlist_path);
    if (load_cache(cp)) {
      return;
    }
  }

  root = alloc_state();

  // Load words from file
  std::ifstream file(wordlist_path);
  if (!file) {
    throw std::runtime_error("Cannot open wordlist: " + wordlist_path);
  }
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty()) continue;
    // Take first whitespace-delimited token
    std::istringstream iss(line);
    std::string word;
    iss >> word;
    if (!word.empty()) {
      add_word(word);
    }
  }

  if (use_cache) {
    std::string cp = cache_path_for(wordlist_path);
    try {
      save_cache(cp);
    } catch (const std::exception& e) {
      std::cerr << "Warning: Failed to save GADDAG cache: " << e.what() << "\n";
    }
  }
}

void Gaddag::add_word(const std::string& word) {
  // Replicates the Python add_word algorithm exactly.
  // word indices: 0 .. n-1, where n = word.size()
  int n = static_cast<int>(word.size());
  if (n < 2) {
    throw std::invalid_argument("Words must have at least 2 letters");
  }

  State* current = root;

  // Step 1: Add arcs for word[n-1 .. 2], then ending arc word[1] → word[0]
  // range(len(word)-1, 1, -1) == [n-1, n-2, ..., 2]
  for (int i = n - 1; i > 1; i--) {
    current = ensure_arc(current, word[i] - 'A');
  }
  {
    State* dest = ensure_arc(current, word[1] - 'A');
    dest->letters_that_make_a_word[word[0] - 'A'] = true;
  }

  // Step 2: Add arcs for word[n-2 .. 0] then ending arc DELIMITER → word[n-1]
  // range(len(word)-2, -1, -1) == [n-2, n-3, ..., 0]
  current = root;
  for (int i = n - 2; i >= 0; i--) {
    current = ensure_arc(current, word[i] - 'A');
  }
  {
    State* dest = ensure_arc(current, DELIMITER_ARC_INDEX);
    dest->letters_that_make_a_word[word[n - 1] - 'A'] = true;
    current = dest;
  }

  // Step 3: Minimization loop
  // range(len(word)-3, -1, -1) == [n-3, n-4, ..., 0]
  for (int m = n - 3; m >= 0; m--) {
    State* destination = current;
    current = root;
    for (int i = m; i >= 0; i--) {
      current = ensure_arc(current, word[i] - 'A');
    }
    current = ensure_arc(current, DELIMITER_ARC_INDEX);
    ensure_arc(current, word[m + 1] - 'A', destination);
  }
}

std::unordered_set<Letter> Gaddag::get_dictionary_letters() const {
  std::unordered_set<Letter> result;
  for (int i = 0; i < 26; i++) {
    if (root->arcs[i]) {
      result.insert(static_cast<Letter>(i));
    }
  }
  return result;
}

// ─── Binary cache helpers
// ─────────────────────────────────────────────────────

std::string Gaddag::cache_path_for(const std::string& wordlist_path) const {
  namespace fs = std::filesystem;
  fs::path p(wordlist_path);
  std::string stem = p.stem().string();
  fs::path cache_dir("temp");
  fs::create_directories(cache_dir);
  return (cache_dir / ("gaddag_" + stem + ".bin")).string();
}

namespace {

// Collect all reachable states in DFS pre-order, assigning each a unique ID.
void collect_states(State* s, std::unordered_map<State*, uint32_t>& ids,
                    std::vector<State*>& ordered) {
  if (ids.count(s)) return;
  uint32_t id = static_cast<uint32_t>(ordered.size());
  ids[s] = id;
  ordered.push_back(s);
  for (int i = 0; i < 28; i++) {
    if (s->arcs[i]) {
      collect_states(s->arcs[i], ids, ordered);
    }
  }
}

void write_u32(std::ostream& out, uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); }
void write_u8(std::ostream& out, uint8_t v) { out.write(reinterpret_cast<const char*>(&v), 1); }

uint32_t read_u32(std::istream& in) {
  uint32_t v = 0;
  in.read(reinterpret_cast<char*>(&v), 4);
  return v;
}
uint8_t read_u8(std::istream& in) {
  uint8_t v = 0;
  in.read(reinterpret_cast<char*>(&v), 1);
  return v;
}

}  // namespace

void Gaddag::save_cache(const std::string& cache_path) const {
  std::unordered_map<State*, uint32_t> ids;
  std::vector<State*> ordered;
  collect_states(root, ids, ordered);

  std::ofstream out(cache_path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("Cannot open cache file for writing: " + cache_path);
  }

  // Header
  const char magic[] = "SCRABBLE";
  out.write(magic, 8);
  write_u32(out, 2);  // version 2

  // Root ID
  write_u32(out, ids.at(root));

  // Number of states
  write_u32(out, static_cast<uint32_t>(ordered.size()));

  // Each state
  for (State* s : ordered) {
    // letters_that_make_a_word as a 26-bit bitmask
    uint32_t bitmask = 0;
    for (int i = 0; i < 26; i++) {
      if (s->letters_that_make_a_word[i]) bitmask |= (1u << i);
    }
    write_u32(out, bitmask);

    // arcs: count non-null entries, then write (arc_index, dest_id) pairs
    uint8_t num_arcs = 0;
    for (int i = 0; i < 28; i++) {
      if (s->arcs[i]) num_arcs++;
    }
    write_u8(out, num_arcs);
    for (int i = 0; i < 28; i++) {
      if (s->arcs[i]) {
        write_u8(out, static_cast<uint8_t>(i));
        write_u32(out, ids.at(s->arcs[i]));
      }
    }
  }
}

bool Gaddag::load_cache(const std::string& cache_path) {
  std::ifstream in(cache_path, std::ios::binary);
  if (!in) return false;

  // Header
  char magic[8] = {};
  in.read(magic, 8);
  if (std::strncmp(magic, "SCRABBLE", 8) != 0) return false;

  uint32_t version = read_u32(in);
  if (version != 2) return false;

  uint32_t root_id = read_u32(in);
  uint32_t num_states = read_u32(in);

  if (!in) return false;
  if (num_states > 10'000'000) return false;  // sanity check

  std::cerr << "Loading GADDAG from cache: " << cache_path << "\n";

  // Create all state objects via the arena
  std::vector<State*> ptrs(num_states);
  for (uint32_t i = 0; i < num_states; i++) {
    ptrs[i] = alloc_state();
  }

  // Fill in letters_that_make_a_word and arcs
  for (uint32_t i = 0; i < num_states; i++) {
    uint32_t bitmask = read_u32(in);
    for (int j = 0; j < 26; j++) {
      if ((bitmask >> j) & 1u) {
        ptrs[i]->letters_that_make_a_word[j] = true;
      }
    }
    uint8_t num_arcs = read_u8(in);
    for (uint8_t j = 0; j < num_arcs; j++) {
      uint8_t arc_index = read_u8(in);
      uint32_t dest_id = read_u32(in);
      if (dest_id >= num_states) return false;
      if (arc_index >= 28) return false;
      ptrs[i]->arcs[arc_index] = ptrs[dest_id];
    }
  }

  if (!in) return false;

  if (root_id >= num_states) return false;
  root = ptrs[root_id];
  return true;
}

}  // namespace scrabble
