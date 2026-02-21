#pragma once

#include "letter.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scrabble {

// The delimiter character ◇ (U+25C7) encoded as UTF-8.
// On MSVC, compile with /utf-8 so string literals are treated as UTF-8.
inline const std::string DELIMITER = "\xE2\x97\x87";

class State;

struct Arc {
  std::shared_ptr<State> destination_state;

  Arc();
  explicit Arc(std::shared_ptr<State> dest);
};

class State {
public:
  std::unordered_map<std::string, Arc> arcs;
  std::unordered_set<char> letters_that_make_a_word;

  State() = default;

  // Add an arc for `letter`. If arc doesn't exist, creates it pointing to
  // `dest` (or a new State if dest is null). Always returns the arc's
  // destination state.
  std::shared_ptr<State> add_arc(const std::string &letter,
                                 std::shared_ptr<State> dest = nullptr);

  // Add an arc for `letter` and record `ending_letter` in the destination's
  // letters_that_make_a_word set. Returns the destination state.
  std::shared_ptr<State> add_ending_arc(const std::string &letter,
                                        char ending_letter);

  void add_ending_letter(char letter);

  // Returns the next state for `letter`, or nullptr if no such arc exists.
  std::shared_ptr<State> get_next_state(const std::string &letter) const;
};

class Gaddag {
public:
  std::shared_ptr<State> root;

  Gaddag();

  // Build GADDAG from an in-memory word list (no caching).
  void build_from_words(const std::vector<std::string> &words);

  // Build GADDAG from a wordlist file. Tries binary cache first when
  // use_cache=true. Cache is stored in temp/gaddag_<stem>.bin.
  void build_from_file(const std::string &wordlist_path, bool use_cache = true);

  // Add a single word to the GADDAG.
  void add_word(const std::string &word);

  // Returns all letter keys present at the root (excludes DELIMITER).
  std::unordered_set<Letter> get_dictionary_letters() const;

private:
  std::string cache_path_for(const std::string &wordlist_path) const;
  bool load_cache(const std::string &cache_path);
  void save_cache(const std::string &cache_path) const;
};

} // namespace scrabble
