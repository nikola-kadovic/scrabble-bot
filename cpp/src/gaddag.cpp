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

// ─── Arc ──────────────────────────────────────────────────────────────────────

Arc::Arc() : destination_state(std::make_shared<State>()) {}
Arc::Arc(std::shared_ptr<State> dest) : destination_state(std::move(dest)) {}

// ─── State ────────────────────────────────────────────────────────────────────

std::shared_ptr<State> State::add_arc(const std::string& letter,
                                       std::shared_ptr<State> dest) {
    if (!dest) {
        dest = std::make_shared<State>();
    }
    // Only insert if arc doesn't already exist; always return existing destination.
    if (arcs.find(letter) == arcs.end()) {
        arcs.emplace(letter, Arc(dest));
    }
    return arcs.at(letter).destination_state;
}

std::shared_ptr<State> State::add_ending_arc(const std::string& letter,
                                               char ending_letter) {
    if (arcs.find(letter) == arcs.end()) {
        arcs.emplace(letter, Arc(std::make_shared<State>()));
    }
    arcs.at(letter).destination_state->add_ending_letter(ending_letter);
    return arcs.at(letter).destination_state;
}

void State::add_ending_letter(char letter) {
    letters_that_make_a_word.insert(letter);
}

std::shared_ptr<State> State::get_next_state(const std::string& letter) const {
    auto it = arcs.find(letter);
    if (it == arcs.end()) return nullptr;
    return it->second.destination_state;
}

// ─── Gaddag ───────────────────────────────────────────────────────────────────

Gaddag::Gaddag() : root(std::make_shared<State>()) {}

void Gaddag::build_from_words(const std::vector<std::string>& words) {
    root = std::make_shared<State>();
    for (const auto& word : words) {
        add_word(word);
    }
}

void Gaddag::build_from_file(const std::string& wordlist_path, bool use_cache) {
    root = std::make_shared<State>();

    if (use_cache) {
        std::string cp = cache_path_for(wordlist_path);
        if (load_cache(cp)) {
            return;
        }
    }

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

    std::shared_ptr<State> current = root;

    // Step 1: Add arcs for word[n-1 .. 2], then ending arc word[1] → word[0]
    // range(len(word)-1, 1, -1) == [n-1, n-2, ..., 2]
    for (int i = n - 1; i > 1; i--) {
        current = current->add_arc(std::string(1, word[i]));
    }
    current->add_ending_arc(std::string(1, word[1]), word[0]);

    // Step 2: Add arcs for word[n-2 .. 0] then ending arc DELIMITER → word[n-1]
    // range(len(word)-2, -1, -1) == [n-2, n-3, ..., 0]
    current = root;
    for (int i = n - 2; i >= 0; i--) {
        current = current->add_arc(std::string(1, word[i]));
    }
    current = current->add_ending_arc(DELIMITER, word[n - 1]);

    // Step 3: Minimization loop
    // range(len(word)-3, -1, -1) == [n-3, n-4, ..., 0]
    for (int m = n - 3; m >= 0; m--) {
        std::shared_ptr<State> destination = current;
        current = root;
        for (int i = m; i >= 0; i--) {
            current = current->add_arc(std::string(1, word[i]));
        }
        current = current->add_arc(DELIMITER);
        current->add_arc(std::string(1, word[m + 1]), destination);
    }
}

std::unordered_set<Letter> Gaddag::get_dictionary_letters() const {
    std::unordered_set<Letter> result;
    for (const auto& [key, arc] : root->arcs) {
        if (key != DELIMITER) {
            // key is a single ASCII char
            result.insert(char_to_letter(key[0]));
        }
    }
    return result;
}

// ─── Binary cache helpers ─────────────────────────────────────────────────────

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
void collect_states(State* s,
                    std::unordered_map<State*, uint32_t>& ids,
                    std::vector<State*>& ordered) {
    if (ids.count(s)) return;
    uint32_t id = static_cast<uint32_t>(ordered.size());
    ids[s] = id;
    ordered.push_back(s);
    for (auto& [key, arc] : s->arcs) {
        collect_states(arc.destination_state.get(), ids, ordered);
    }
}

void write_u32(std::ostream& out, uint32_t v) {
    out.write(reinterpret_cast<const char*>(&v), 4);
}
void write_u16(std::ostream& out, uint16_t v) {
    out.write(reinterpret_cast<const char*>(&v), 2);
}
void write_u8(std::ostream& out, uint8_t v) {
    out.write(reinterpret_cast<const char*>(&v), 1);
}

uint32_t read_u32(std::istream& in) {
    uint32_t v = 0;
    in.read(reinterpret_cast<char*>(&v), 4);
    return v;
}
uint16_t read_u16(std::istream& in) {
    uint16_t v = 0;
    in.read(reinterpret_cast<char*>(&v), 2);
    return v;
}
uint8_t read_u8(std::istream& in) {
    uint8_t v = 0;
    in.read(reinterpret_cast<char*>(&v), 1);
    return v;
}

} // namespace

void Gaddag::save_cache(const std::string& cache_path) const {
    std::unordered_map<State*, uint32_t> ids;
    std::vector<State*> ordered;
    collect_states(root.get(), ids, ordered);

    std::ofstream out(cache_path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Cannot open cache file for writing: " + cache_path);
    }

    // Header
    const char magic[] = "SCRABBLE";
    out.write(magic, 8);
    write_u32(out, 1); // version

    // Root ID
    write_u32(out, ids.at(root.get()));

    // Number of states
    write_u32(out, static_cast<uint32_t>(ordered.size()));

    // Each state
    for (State* s : ordered) {
        // letters_that_make_a_word
        write_u32(out, static_cast<uint32_t>(s->letters_that_make_a_word.size()));
        for (char c : s->letters_that_make_a_word) {
            write_u8(out, static_cast<uint8_t>(c));
        }
        // arcs
        write_u32(out, static_cast<uint32_t>(s->arcs.size()));
        for (const auto& [key, arc] : s->arcs) {
            uint16_t key_len = static_cast<uint16_t>(key.size());
            write_u16(out, key_len);
            out.write(key.data(), key_len);
            write_u32(out, ids.at(arc.destination_state.get()));
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
    if (version != 1) return false;

    uint32_t root_id = read_u32(in);
    uint32_t num_states = read_u32(in);

    if (!in) return false;
    if (num_states > 10'000'000) return false; // sanity check

    std::cerr << "Loading GADDAG from cache: " << cache_path << "\n";

    // Create all state objects
    std::vector<std::shared_ptr<State>> states(num_states);
    for (auto& sp : states) {
        sp = std::make_shared<State>();
    }

    // Fill in letters_that_make_a_word and arcs
    for (uint32_t i = 0; i < num_states; i++) {
        uint32_t num_letters = read_u32(in);
        for (uint32_t j = 0; j < num_letters; j++) {
            uint8_t c = read_u8(in);
            states[i]->letters_that_make_a_word.insert(static_cast<char>(c));
        }
        uint32_t num_arcs = read_u32(in);
        for (uint32_t j = 0; j < num_arcs; j++) {
            uint16_t key_len = read_u16(in);
            std::string key(key_len, '\0');
            in.read(key.data(), key_len);
            uint32_t dest_id = read_u32(in);
            if (dest_id >= num_states) return false;
            states[i]->arcs.emplace(key, Arc(states[dest_id]));
        }
    }

    if (!in) return false;

    if (root_id >= num_states) return false;
    root = states[root_id];
    return true;
}

} // namespace scrabble
