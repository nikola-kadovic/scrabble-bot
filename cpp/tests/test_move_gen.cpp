#include <catch2/catch_test_macros.hpp>
#include "scrabble/board.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/letter.hpp"
#include "scrabble/move.hpp"
#include "scrabble/point.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace scrabble;

// ── Shared helpers ────────────────────────────────────────────────────────────

namespace {

std::shared_ptr<Gaddag> make_gaddag(std::vector<std::string> words) {
    auto g = std::make_shared<Gaddag>();
    g->build_from_words(words);
    return g;
}

// Check whether a specific word placement is present in the move list.
// word: uppercase full word string (e.g. "CATS")
// sr/sc: start row/col of the word span
// vert: true = vertical
bool move_exists(const std::vector<Move>& moves, const std::string& word,
                 int sr, int sc, bool vert) {
    for (const auto& m : moves) {
        bool m_vert = (m.start.col == m.end.col);
        if (m_vert != vert) continue;
        if (m.start.row != sr || m.start.col != sc) continue;
        std::string w;
        for (const auto& s : m.letters) w += s;
        if (w == word) return true;
    }
    return false;
}

// Convert a Move's letters (vector<string>) to a vector<Letter>.
// Lowercase letters represent blank tiles (converted to Letter::BLANK).
std::vector<Letter> move_to_letters(const Move& m) {
    std::vector<Letter> word;
    for (const auto& s : m.letters) {
        char c = s[0];
        if (std::islower(static_cast<unsigned char>(c))) {
            word.push_back(Letter::BLANK);
        } else {
            word.push_back(char_to_letter(c));
        }
    }
    return word;
}

// Core validation: for every move, place it on a copy of the board and assert
// validate_board() returns empty. Also verify the stored score matches
// calculate_score() on the original board.
void verify_all_moves(const Board& original, const std::vector<Move>& moves) {
    for (const auto& m : moves) {
        bool vert = (m.start.col == m.end.col);
        Board copy = original;
        std::vector<Letter> word = move_to_letters(m);
        copy.place_word(word, {m.start.row, m.start.col}, vert);
        auto invalid = copy.validate_board();
        CHECK(invalid.empty());
        int expected_score =
            original.calculate_score(word, m.start.row, m.start.col, vert);
        CHECK(m.score == expected_score);
    }
}

} // namespace

// ── Test 1: first move – all returned moves are valid ─────────────────────────

TEST_CASE("first move: generated moves are all valid", "[move_gen]") {
    auto g = make_gaddag({"CAT", "CATS", "ACT", "ACTS", "CAST", "SCAT"});
    Board board(g);
    std::vector<Letter> rack = {Letter::C, Letter::A, Letter::T, Letter::S};

    auto moves = board.get_all_valid_moves(rack);
    REQUIRE(!moves.empty());

    // Spot-check: CATS and CAST must appear starting at (7,7)
    CHECK(move_exists(moves, "CATS", 7, 7, false));
    CHECK(move_exists(moves, "CAST", 7, 7, false));

    // Every move must include (7,7) since it is the only anchor on an empty board.
    for (const auto& m : moves) {
        bool vert = (m.start.col == m.end.col);
        if (vert) {
            CHECK(m.start.col == 7);
            CHECK(m.start.row <= 7);
            CHECK(m.end.row >= 7);
        } else {
            CHECK(m.start.row == 7);
            CHECK(m.start.col <= 7);
            CHECK(m.end.col >= 7);
        }
    }

    verify_all_moves(board, moves);
}

// ── Test 2: extending an existing word ───────────────────────────────────────

TEST_CASE("extending existing word generates correct moves", "[move_gen]") {
    auto g = make_gaddag({"CAT", "CATS", "SCAT", "TA", "AT", "AS"});
    Board board(g);
    board.place_word({Letter::C, Letter::A, Letter::T}, {7, 7}, false);

    std::vector<Letter> rack = {Letter::S};
    auto moves = board.get_all_valid_moves(rack);
    REQUIRE(!moves.empty());

    // S appended to the right → CATS starting at (7,7)
    CHECK(move_exists(moves, "CATS", 7, 7, false));
    // S prepended to the left → SCAT starting at (7,6)
    CHECK(move_exists(moves, "SCAT", 7, 6, false));

    verify_all_moves(board, moves);
}

// ── Test 3: cross-word constraint filters invalid placements ─────────────────

TEST_CASE("cross-word constraint filters invalid placements", "[move_gen]") {
    // ACT vertical at (7,7): A@(7,7), C@(8,7), T@(9,7).
    // Placing T at (7,6) forms "TA" horizontally → valid (in dict).
    // Placing X or Q at (7,6) would form "XA" or "QA" → not in dict → filtered.
    auto g = make_gaddag({"ACT", "TA", "AX", "AT"});
    Board board(g);
    board.place_word({Letter::A, Letter::C, Letter::T}, {7, 7}, true);

    std::vector<Letter> rack = {Letter::T, Letter::X, Letter::Q};
    auto moves = board.get_all_valid_moves(rack);

    // "TA" at (7,6) horizontal must appear
    CHECK(move_exists(moves, "TA", 7, 6, false));

    // All returned moves must be valid (cross-check constraint works)
    verify_all_moves(board, moves);
}

// ── Test 4: score accuracy ────────────────────────────────────────────────────

TEST_CASE("generated move scores match calculate_score", "[move_gen]") {
    auto g = make_gaddag({"CAT", "CATS"});
    Board board(g);
    board.place_word({Letter::C, Letter::A, Letter::T}, {7, 7}, false);

    std::vector<Letter> rack = {Letter::S};
    auto moves = board.get_all_valid_moves(rack);

    auto it = std::find_if(moves.begin(), moves.end(), [](const Move& m) {
        std::string w;
        for (const auto& s : m.letters) w += s;
        return w == "CATS" && m.start.row == 7 && m.start.col == 7;
    });
    REQUIRE(it != moves.end());

    // After CAT is placed the center DWS is consumed.
    // Only S (new tile) at DEFAULT square (7,10) contributes: score = 1.
    CHECK(it->score == 1);

    // Also verify score matches calculate_score directly
    std::vector<Letter> cats_word = {Letter::C, Letter::A, Letter::T, Letter::S};
    int expected = board.calculate_score(cats_word, 7, 7, false);
    CHECK(it->score == expected);
}
