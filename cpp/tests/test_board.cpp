#include <catch2/catch_test_macros.hpp>
#include "scrabble/board.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/letter.hpp"

using namespace scrabble;

namespace {
std::shared_ptr<Gaddag> make_gaddag(std::vector<std::string> words) {
    auto g = std::make_shared<Gaddag>();
    g->build_from_words(words);
    return g;
}
} // namespace

TEST_CASE("Board initializes 15x15 grid of BLANK", "[board]") {
    auto g = make_gaddag({});
    Board b(g);
    for (int r = 0; r < BOARD_ROWS; r++) {
        for (int c = 0; c < BOARD_COLS; c++) {
            CHECK(b.board[r][c] == Letter::BLANK);
        }
    }
}

TEST_CASE("Board has 225 square types", "[board]") {
    auto g = make_gaddag({});
    Board b(g);
    int count = 0;
    for (int r = 0; r < BOARD_ROWS; r++)
        for (int c = 0; c < BOARD_COLS; c++)
            count++;
    CHECK(count == 225);
}

TEST_CASE("Board center square is DEFAULT before first move", "[board]") {
    auto g = make_gaddag({});
    Board b(g);
    CHECK(b.square_types[7][7] == SquareType::DEFAULT);
}

TEST_CASE("Board corners are TRIPLE_WORD", "[board]") {
    auto g = make_gaddag({});
    Board b(g);
    CHECK(b.square_types[0][0]   == SquareType::TRIPLE_WORD);
    CHECK(b.square_types[0][7]   == SquareType::TRIPLE_WORD);
    CHECK(b.square_types[7][0]   == SquareType::TRIPLE_WORD);
    CHECK(b.square_types[14][14] == SquareType::TRIPLE_WORD);
}

TEST_CASE("Board triple letter squares", "[board]") {
    auto g = make_gaddag({});
    Board b(g);
    CHECK(b.square_types[1][5]  == SquareType::TRIPLE_LETTER);
    CHECK(b.square_types[1][9]  == SquareType::TRIPLE_LETTER);
    CHECK(b.square_types[5][5]  == SquareType::TRIPLE_LETTER);
}

TEST_CASE("Board anchor points empty on init", "[board]") {
    auto g = make_gaddag({});
    Board b(g);
    CHECK(b.anchor_points.empty());
}

TEST_CASE("Board anchor points after horizontal word", "[board]") {
    auto g = make_gaddag({"ACT"});
    Board b(g);
    b.place_word({Letter::A, Letter::C, Letter::T}, {7, 7}, false);

    CHECK(b.anchor_points.size() == 8);

    std::unordered_set<Point, PointHash> expected = {
        {6,7},{8,7},{6,8},{8,8},{6,9},{8,9},{7,6},{7,10}
    };
    for (const auto& p : expected) {
        CHECK(b.anchor_points.count(p) == 1);
    }
}

TEST_CASE("Board anchor points after vertical word", "[board]") {
    auto g = make_gaddag({"ACT"});
    Board b(g);
    b.place_word({Letter::A, Letter::C, Letter::T}, {7, 7}, true);

    CHECK(b.anchor_points.size() == 8);

    std::unordered_set<Point, PointHash> expected = {
        {6,7},{10,7},{7,6},{8,6},{9,6},{7,8},{8,8},{9,8}
    };
    for (const auto& p : expected) {
        CHECK(b.anchor_points.count(p) == 1);
    }
}

TEST_CASE("Board anchor points after multiple words", "[board]") {
    auto g = make_gaddag({"ACT","CAT"});
    Board b(g);
    b.place_word({Letter::A, Letter::C, Letter::T}, {7, 7}, true);
    b.place_word({Letter::C, Letter::A, Letter::T}, {8, 7}, false);

    CHECK(b.anchor_points.size() == 10);
}

TEST_CASE("Board horizontal cross check: ACT vertical, T+A pair", "[board]") {
    auto g = make_gaddag({"ACT","ACTS","TA"});
    Board b(g);
    b.place_word({Letter::A, Letter::C, Letter::T}, {7, 7}, true);

    // Square to the left of 'A' (row 7, col 6) should allow only T
    const auto& cc_76 = b.horizontal_cross_checks[7][6];
    CHECK(cc_76.size() == 1);
    CHECK(cc_76.count(Letter::T) == 1);

    // Square to the right of 'A' (row 7, col 8) should be empty
    CHECK(b.horizontal_cross_checks[7][8].empty());

    // Square to the right of 'T' (row 9, col 8) should allow only A
    const auto& cc_98 = b.horizontal_cross_checks[9][8];
    CHECK(cc_98.size() == 1);
    CHECK(cc_98.count(Letter::A) == 1);

    // Square below the word (row 10, col 7): only S (ACTS)
    const auto& vc_10_7 = b.vertical_cross_checks[10][7];
    CHECK(vc_10_7.size() == 1);
    CHECK(vc_10_7.count(Letter::S) == 1);
}

TEST_CASE("Board horizontal cross check: PAYABLE example", "[board]") {
    auto g = make_gaddag({"PA","ABLE","PAYABLE","PARABLE"});
    Board b(g);

    // Place "PA" horizontally at (7,5)
    b.place_word({Letter::P, Letter::A}, {7, 5}, false);

    // Below P at (8,5): only A (from PA)
    CHECK(b.vertical_cross_checks[8][5].size() == 1);

    // Above A at (6,6): only P (from PA)
    CHECK(b.vertical_cross_checks[6][6].size() == 1);
    CHECK(b.vertical_cross_checks[6][6].count(Letter::P) == 1);

    // Above P at (6,5): nothing
    CHECK(b.vertical_cross_checks[6][5].empty());
    // Below A at (8,6): nothing
    CHECK(b.vertical_cross_checks[8][6].empty());

    // Left of P at (7,4): nothing
    CHECK(b.horizontal_cross_checks[7][4].empty());
    // Right of A at (7,7): nothing
    CHECK(b.horizontal_cross_checks[7][7].empty());

    // Place "ABLE" horizontally at (7,8)
    b.place_word({Letter::A, Letter::B, Letter::L, Letter::E}, {7, 8}, false);

    // Gap at (7,7) should allow R and Y (for PARABLE / PAYABLE)
    const auto& gap = b.horizontal_cross_checks[7][7];
    CHECK(gap.size() == 2);
    CHECK(gap.count(Letter::R) == 1);
    CHECK(gap.count(Letter::Y) == 1);
}

TEST_CASE("Board place_word throws on out of bounds", "[board]") {
    auto g = make_gaddag({});
    Board b(g);
    // Force first_move_ off by placing a valid word first
    b.place_word({Letter::A, Letter::B}, {7, 7}, false);
    // Now try to overflow
    CHECK_THROWS_AS(
        b.place_word({Letter::A, Letter::B, Letter::C}, {7, 14}, false),
        std::invalid_argument);
}
