#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "scrabble/gaddag.hpp"

using namespace scrabble;

namespace fs = std::filesystem;

TEST_CASE("DELIMITER is the correct UTF-8 sequence", "[gaddag]") {
  CHECK(std::string_view{DELIMITER}.size() == 3);  // 3-byte UTF-8 for U+25C7
  CHECK(static_cast<unsigned char>(DELIMITER[0]) == 0xE2);
  CHECK(static_cast<unsigned char>(DELIMITER[1]) == 0x97);
  CHECK(static_cast<unsigned char>(DELIMITER[2]) == 0x87);
}

TEST_CASE("State initializes empty", "[gaddag][state]") {
  State s;
  CHECK(s.arcs.empty());
  CHECK(s.letters_that_make_a_word.empty());
}

TEST_CASE("State::add_arc creates arc and returns destination", "[gaddag][state]") {
  State s;
  auto dest = s.add_arc("A");
  CHECK(s.arcs.count("A") == 1);
  CHECK(dest != nullptr);
  CHECK(s.arcs.at("A").destination_state == dest);
}

TEST_CASE("State::add_arc is idempotent (returns same destination)", "[gaddag][state]") {
  State s;
  auto d1 = s.add_arc("B");
  auto d2 = s.add_arc("B");
  CHECK(d1 == d2);
  CHECK(s.arcs.size() == 1);
}

TEST_CASE("State::add_arc with provided destination shares state", "[gaddag][state]") {
  State s;
  auto shared = std::make_shared<State>();
  auto result = s.add_arc("C", shared);
  CHECK(result == shared);
  CHECK(s.arcs.at("C").destination_state == shared);
}

TEST_CASE("State::add_ending_arc records ending letter", "[gaddag][state]") {
  State s;
  auto dest = s.add_ending_arc("D", 'E');
  CHECK(dest != nullptr);
  CHECK(dest->letters_that_make_a_word.count('E') == 1);
}

TEST_CASE("State::add_ending_letter inserts to set", "[gaddag][state]") {
  State s;
  s.add_ending_letter('X');
  s.add_ending_letter('X');  // duplicate
  s.add_ending_letter('Y');
  CHECK(s.letters_that_make_a_word.size() == 2);
  CHECK(s.letters_that_make_a_word.count('X') == 1);
  CHECK(s.letters_that_make_a_word.count('Y') == 1);
}

TEST_CASE("State::get_next_state returns nullptr for missing arc", "[gaddag][state]") {
  State s;
  CHECK(s.get_next_state("Z") == nullptr);
}

TEST_CASE("State::get_next_state returns correct destination", "[gaddag][state]") {
  State s;
  auto dest = s.add_arc("A");
  CHECK(s.get_next_state("A") == dest);
}

TEST_CASE("Gaddag builds from words list", "[gaddag]") {
  Gaddag g;
  g.build_from_words({"CAT", "DOG"});
  CHECK(g.root != nullptr);
  CHECK(!g.root->arcs.empty());
}

TEST_CASE("Gaddag get_dictionary_letters excludes DELIMITER", "[gaddag]") {
  Gaddag g;
  g.build_from_words({"CAT", "DOG"});
  auto letters = g.get_dictionary_letters();
  // DELIMITER should not be in the returned set
  for (Letter l : letters) {
    CHECK(letter_to_char(l) != '\xE2');  // not the DELIMITER first byte
  }
  // C, A, T, D, O, G should all appear
  CHECK(letters.count(Letter::C) == 1);
  CHECK(letters.count(Letter::D) == 1);
}

TEST_CASE("Gaddag traversal finds expected word structure", "[gaddag]") {
  // For word "AB": root --A--> s_A, s_A has arc ◇ with 'B' in letters_that_make_a_word
  Gaddag g;
  g.build_from_words({"AB"});
  auto s_A = g.root->get_next_state("A");
  REQUIRE(s_A != nullptr);
  auto s_delim = s_A->get_next_state(DELIMITER);
  REQUIRE(s_delim != nullptr);
  CHECK(s_delim->letters_that_make_a_word.count('B') == 1);
}

TEST_CASE("Gaddag add_word throws on single letter", "[gaddag]") {
  Gaddag g;
  CHECK_THROWS_AS(g.add_word("A"), std::invalid_argument);
}

TEST_CASE("Gaddag binary cache round-trip", "[gaddag][cache]") {
  // Use a uniquely-named wordlist so the cache key is distinct
  fs::path tmp_dir = fs::temp_directory_path() / "scrabble_test_cache";
  fs::create_directories(tmp_dir);

  fs::path wl = tmp_dir / "scrabble_smoke_wl.txt";
  {
    std::ofstream f(wl);
    f << "CAT\nDOG\nBAT\n";
  }

  // The cache is written to temp/ relative to CWD
  fs::path expected_cache = fs::path("temp") / "gaddag_scrabble_smoke_wl.bin";

  // Remove stale cache if present
  fs::remove(expected_cache);

  // First build: no cache → builds from file, then saves cache
  Gaddag g1;
  g1.build_from_file(wl.string(), true);
  size_t n1 = g1.root->arcs.size();

  CHECK(fs::exists(expected_cache));

  // Second build: loads from cache
  Gaddag g2;
  g2.build_from_file(wl.string(), true);
  size_t n2 = g2.root->arcs.size();

  CHECK(n1 == n2);

  // Cleanup
  fs::remove(expected_cache);
  fs::remove_all(tmp_dir);
}
