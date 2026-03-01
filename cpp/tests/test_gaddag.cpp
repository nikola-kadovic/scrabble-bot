#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

#include "scrabble/gaddag.hpp"

using namespace scrabble;

namespace fs = std::filesystem;

// ── Helpers for querying the new array-based State fields ────────────────────

static bool arcs_empty(const State& s) {
  for (const auto& p : s.arcs)
    if (p) return false;
  return true;
}

static size_t arcs_size(const State& s) {
  size_t n = 0;
  for (const auto& p : s.arcs)
    if (p) n++;
  return n;
}

static bool ltmaw_has(const State& s, char c) {
  return s.letters_that_make_a_word[static_cast<unsigned char>(c - 'A')];
}

static size_t ltmaw_size(const State& s) {
  size_t n = 0;
  for (bool b : s.letters_that_make_a_word)
    if (b) n++;
  return n;
}

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_CASE("DELIMITER is the correct UTF-8 sequence", "[gaddag]") {
  CHECK(std::string_view{DELIMITER}.size() == 3);  // 3-byte UTF-8 for U+25C7
  CHECK(static_cast<unsigned char>(DELIMITER[0]) == 0xE2);
  CHECK(static_cast<unsigned char>(DELIMITER[1]) == 0x97);
  CHECK(static_cast<unsigned char>(DELIMITER[2]) == 0x87);
}

TEST_CASE("State initializes empty", "[gaddag][state]") {
  State s;
  CHECK(arcs_empty(s));
  CHECK(ltmaw_size(s) == 0);
}

TEST_CASE("GADDAG arc to traversal state is non-null", "[gaddag][state]") {
  // After build_from_words({"AB"}), root has arcs for both 'A' (step 2) and
  // 'B' (step 1). Verify at least the 'A' arc was created.
  Gaddag g;
  g.build_from_words({"AB"});
  CHECK(g.root->arcs[static_cast<int>(Letter::A)] != nullptr);
  CHECK(g.root->get_next_state(static_cast<int>(Letter::A)) != nullptr);
}

TEST_CASE("GADDAG arc creation is idempotent", "[gaddag][state]") {
  // Both "AB" and "AC" traverse root->A in step 2; ensure_arc is called
  // twice for 'A' but the pointer must be the same state.
  Gaddag g;
  g.build_from_words({"AB", "AC"});
  const State* s_A = g.root->get_next_state(static_cast<int>(Letter::A));
  REQUIRE(s_A != nullptr);
  CHECK(g.root->arcs[static_cast<int>(Letter::A)] == s_A);  // stable pointer
}

TEST_CASE("GADDAG minimization reuses delimiter state across words", "[gaddag][state]") {
  // "ABCD" and "ABCE" share the path root->C->B->A->DELIM.
  // That DELIM state should have both 'D' and 'E' in letters_that_make_a_word.
  Gaddag g;
  g.build_from_words({"ABCD", "ABCE"});
  const State* s_C = g.root->get_next_state(static_cast<int>(Letter::C));
  REQUIRE(s_C != nullptr);
  const State* s_CB = s_C->get_next_state(static_cast<int>(Letter::B));
  REQUIRE(s_CB != nullptr);
  const State* s_CBA = s_CB->get_next_state(static_cast<int>(Letter::A));
  REQUIRE(s_CBA != nullptr);
  const State* s_delim = s_CBA->get_next_state(DELIMITER_ARC_INDEX);
  REQUIRE(s_delim != nullptr);
  // Both 'D' and 'E' must appear in the same (shared) delimiter state.
  CHECK(ltmaw_has(*s_delim, 'D'));
  CHECK(ltmaw_has(*s_delim, 'E'));
}

TEST_CASE("GADDAG ending letter recorded in LTMAW", "[gaddag][state]") {
  // For word "DE" (n=2), step 2 produces root->D->DELIM with 'E' in LTMAW.
  Gaddag g;
  g.build_from_words({"DE"});
  const State* s_D = g.root->get_next_state(static_cast<int>(Letter::D));
  REQUIRE(s_D != nullptr);
  const State* s_delim = s_D->get_next_state(DELIMITER_ARC_INDEX);
  REQUIRE(s_delim != nullptr);
  CHECK(ltmaw_has(*s_delim, 'E'));
}

TEST_CASE("State::add_ending_letter inserts to set", "[gaddag][state]") {
  State s;
  s.add_ending_letter('X');
  s.add_ending_letter('X');  // duplicate
  s.add_ending_letter('Y');
  CHECK(ltmaw_size(s) == 2);
  CHECK(ltmaw_has(s, 'X'));
  CHECK(ltmaw_has(s, 'Y'));
}

TEST_CASE("State::get_next_state returns nullptr for missing arc", "[gaddag][state]") {
  State s;
  CHECK(s.get_next_state("Z") == nullptr);
}

TEST_CASE("State::get_next_state returns correct destination", "[gaddag][state]") {
  // Both overloads (string and int) must return the same raw pointer.
  Gaddag g;
  g.build_from_words({"AB"});
  const State* via_str = g.root->get_next_state("A");
  const State* via_int = g.root->get_next_state(static_cast<int>(Letter::A));
  REQUIRE(via_str != nullptr);
  CHECK(via_str == via_int);
}

TEST_CASE("Gaddag builds from words list", "[gaddag]") {
  Gaddag g;
  g.build_from_words({"CAT", "DOG"});
  CHECK(g.root != nullptr);
  CHECK(!arcs_empty(*g.root));
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
  CHECK(ltmaw_has(*s_delim, 'B'));
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
  size_t n1 = arcs_size(*g1.root);

  CHECK(fs::exists(expected_cache));

  // Second build: loads from cache
  Gaddag g2;
  g2.build_from_file(wl.string(), true);
  size_t n2 = arcs_size(*g2.root);

  CHECK(n1 == n2);

  // Cleanup
  fs::remove(expected_cache);
  fs::remove_all(tmp_dir);
}
