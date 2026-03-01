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

TEST_CASE("Board center square is DOUBLE_WORD before first move", "[board]") {
    auto g = make_gaddag({});
    Board b(g);
    CHECK(b.square_types[7][7] == SquareType::DOUBLE_WORD);
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

TEST_CASE("Board anchor points seeded with center on init", "[board]") {
    auto g = make_gaddag({});
    Board b(g);
    // Constructor seeds {7,7} as the first-move anchor point.
    CHECK(b.anchor_points.size() == 1);
    CHECK(b.anchor_points.count(Point{7, 7}) == 1);
}

TEST_CASE("Board anchor points after horizontal word", "[board]") {
    auto g = make_gaddag({"ACT"});
    Board b(g);
    b.place_word({Letter::A, Letter::C, Letter::T}, Point{7, 7}, Point{7, 9});

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
    b.place_word({Letter::A, Letter::C, Letter::T}, Point{7, 7}, Point{9, 7});

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
    b.place_word({Letter::A, Letter::C, Letter::T}, Point{7, 7}, Point{9, 7});
    b.place_word({Letter::C, Letter::A, Letter::T}, Point{8, 7}, Point{8, 9});

    CHECK(b.anchor_points.size() == 10);
}

TEST_CASE("Board horizontal cross check: ACT vertical, T+A pair", "[board]") {
    auto g = make_gaddag({"ACT","ACTS","TA"});
    Board b(g);
    b.place_word({Letter::A, Letter::C, Letter::T}, Point{7, 7}, Point{9, 7});

    // Square to the left of 'A' (row 7, col 6) should allow only T
    CrossCheckSet cc_76 = b.horizontal_cross_checks[7][6];
    CHECK(cross_check_count(cc_76) == 1);
    CHECK(cross_check_has(cc_76, Letter::T));

    // Square to the right of 'A' (row 7, col 8) should be empty
    CHECK(b.horizontal_cross_checks[7][8] == CROSS_CHECK_NONE);

    // Square to the right of 'T' (row 9, col 8) should allow only A
    CrossCheckSet cc_98 = b.horizontal_cross_checks[9][8];
    CHECK(cross_check_count(cc_98) == 1);
    CHECK(cross_check_has(cc_98, Letter::A));

    // Square below the word (row 10, col 7): only S (ACTS)
    CrossCheckSet vc_10_7 = b.vertical_cross_checks[10][7];
    CHECK(cross_check_count(vc_10_7) == 1);
    CHECK(cross_check_has(vc_10_7, Letter::S));
}

TEST_CASE("Board horizontal cross check: PAYABLE example", "[board]") {
    auto g = make_gaddag({"PA","ABLE","PAYABLE","PARABLE"});
    Board b(g);

    // Place "PA" horizontally at (7,5)
    b.place_word({Letter::P, Letter::A}, Point{7, 5}, Point{7, 6});

    // Below P at (8,5): only A (from PA)
    CHECK(cross_check_count(b.vertical_cross_checks[8][5]) == 1);

    // Above A at (6,6): only P (from PA)
    CHECK(cross_check_count(b.vertical_cross_checks[6][6]) == 1);
    CHECK(cross_check_has(b.vertical_cross_checks[6][6], Letter::P));

    // Above P at (6,5): nothing
    CHECK(b.vertical_cross_checks[6][5] == CROSS_CHECK_NONE);
    // Below A at (8,6): nothing
    CHECK(b.vertical_cross_checks[8][6] == CROSS_CHECK_NONE);

    // Left of P at (7,4): nothing
    CHECK(b.horizontal_cross_checks[7][4] == CROSS_CHECK_NONE);
    // Right of A at (7,7): nothing
    CHECK(b.horizontal_cross_checks[7][7] == CROSS_CHECK_NONE);

    // Place "ABLE" horizontally at (7,8)
    b.place_word({Letter::A, Letter::B, Letter::L, Letter::E}, Point{7, 8}, Point{7, 11});

    // Gap at (7,7) should allow R and Y (for PARABLE / PAYABLE)
    CrossCheckSet gap = b.horizontal_cross_checks[7][7];
    CHECK(cross_check_count(gap) == 2);
    CHECK(cross_check_has(gap, Letter::R));
    CHECK(cross_check_has(gap, Letter::Y));
}

TEST_CASE("Board place_word throws on out of bounds", "[board]") {
    auto g = make_gaddag({});
    Board b(g);
    // Force first_move_ off by placing a valid word first
    b.place_word({Letter::A, Letter::B}, Point{7, 7}, Point{7, 8});
    // Now try to overflow
    CHECK_THROWS_AS(
        b.place_word({Letter::A, Letter::B, Letter::C}, Point{7, 14}, Point{7, 16}),
        std::invalid_argument);
}

// ── Scoring tests ──────────────────────────────────────────────────────────

// 1. Baseline: DWS at center doubles the word.
// RATE horizontal at (7,5): R(7,5)=DEFAULT, A(7,6)=DEFAULT, T(7,7)=DWS,
// E(7,8)=DEFAULT. word_score=4, multiplier=2 → 8.
TEST_CASE("Scoring: RATE horizontal, DWS at center", "[scoring]") {
    auto g = make_gaddag({});
    Board b(g);
    int score = b.place_word(
        {Letter::R, Letter::A, Letter::T, Letter::E}, Point{7, 5}, Point{7, 8});
    CHECK(score == 8);
}

// 2. DLS doubles one letter, DWS at center.
// TRACE horizontal at (7,3): T(7,3)=DLS(2), R(7,4)=1, A(7,5)=1, C(7,6)=3,
// E(7,7)=DWS. word_score=8, multiplier=2 → 16.
TEST_CASE("Scoring: TRACE horizontal, DLS + DWS at center", "[scoring]") {
    auto g = make_gaddag({});
    Board b(g);
    int score = b.place_word(
        {Letter::T, Letter::R, Letter::A, Letter::C, Letter::E}, Point{7, 3}, Point{7, 7});
    CHECK(score == 16);
}

// 3. TLS triples one letter on a second move (already-placed tile skipped).
// Setup: ACT horizontal at (7,5) → A(1)+C(3)+T(7,7,DWS→1) * 2 = 10.
// Move: MIA vertical at (5,5): M(5,5)=TLS(9), I(6,5)=1, A(7,5)=pre-placed.
// word_score=10, multiplier=1 → 10.
TEST_CASE("Scoring: MIA vertical, TLS + pre-placed tile skip", "[scoring]") {
    auto g = make_gaddag({});
    Board b(g);
    b.place_word({Letter::A, Letter::C, Letter::T}, Point{7, 5}, Point{7, 7}); // setup
    int score = b.place_word(
        {Letter::M, Letter::I, Letter::A}, Point{5, 5}, Point{7, 5});
    CHECK(score == 10);
}

// 4. DWS doubles whole word on a second move (already-placed tile skipped).
// Setup: ACE horizontal at (7,4) → 5.
// Move: YOGA vertical at (4,4): Y(4,4)=DWS(4,×2), O(5,4)=1, G(6,4)=2,
// A(7,4)=pre-placed. word_score=7, multiplier=2 → 14.
TEST_CASE("Scoring: YOGA vertical, DWS + pre-placed tile skip", "[scoring]") {
    auto g = make_gaddag({});
    Board b(g);
    b.place_word({Letter::A, Letter::C, Letter::E}, Point{7, 4}, Point{7, 6}); // setup
    int score = b.place_word(
        {Letter::Y, Letter::O, Letter::G, Letter::A}, Point{4, 4}, Point{7, 4});
    CHECK(score == 14);
}

// 5. TWS × DLS + bingo bonus.
// MIGRANT horizontal at (7,0): M(7,0)=TWS(3,×3), I(7,1)=1, G(7,2)=2,
// R(7,3)=DLS(2), A(7,4)=1, N(7,5)=1, T(7,6)=1. word_score=11, ×3 + 50 = 83.
TEST_CASE("Scoring: MIGRANT horizontal, TWS + DLS + bingo", "[scoring]") {
    auto g = make_gaddag({});
    Board b(g);
    int score = b.place_word(
        {Letter::M, Letter::I, Letter::G, Letter::R, Letter::A, Letter::N, Letter::T},
        Point{7, 0}, Point{7, 6});
    CHECK(score == 83);
}

// 6. Two DWS stack to ×4 + bingo.
// Setup: A vertical at (7,7) → 2.
// Move: DOUBLES horizontal at (4,4): D(4,4)=DWS(2,×2), O(1) U(1) B(3) L(1) E(1),
// S(4,10)=DWS(1,×4). word_score=10, ×4 + 50 = 90.
TEST_CASE("Scoring: DOUBLES horizontal, two DWS stack to x4 + bingo", "[scoring]") {
    auto g = make_gaddag({});
    Board b(g);
    b.place_word({Letter::A}, Point{7, 7}, Point{7, 7}); // setup: A at center (DWS)
    int score = b.place_word(
        {Letter::D, Letter::O, Letter::U, Letter::B, Letter::L, Letter::E, Letter::S},
        Point{4, 4}, Point{4, 10});
    CHECK(score == 90);
}

// 7. TWS × word + DLS on letter + bingo (vertical second move).
// Setup: A vertical at (7,7) → 2.
// Move: STOPPED vertical at (0,7): S(0,7)=TWS(1,×3), T(1,7)=1, O(2,7)=1,
// P(3,7)=DLS(6), P(4,7)=3, E(5,7)=1, D(6,7)=2. word_score=15, ×3 + 50 = 95.
TEST_CASE("Scoring: STOPPED vertical, TWS + DLS + bingo", "[scoring]") {
    auto g = make_gaddag({});
    Board b(g);
    b.place_word({Letter::A}, Point{7, 7}, Point{7, 7}); // setup: A at center (DWS)
    int score = b.place_word(
        {Letter::S, Letter::T, Letter::O, Letter::P, Letter::P, Letter::E, Letter::D},
        Point{0, 7}, Point{6, 7});
    CHECK(score == 95);
}

// 8. Bingo bonus with DWS at center (7-letter first move).
// MIGRATE horizontal at (7,4): M(3)+I(1)+G(2) = 6 DEFAULT; R(7,7)=DWS(1,×2);
// A(1)+T(1)+E(1) = 3 DEFAULT. word_score=10, ×2 + 50 = 70.
TEST_CASE("Scoring: MIGRATE horizontal, DWS bingo", "[scoring]") {
    auto g = make_gaddag({});
    Board b(g);
    int score = b.place_word(
        {Letter::M, Letter::I, Letter::G, Letter::R, Letter::A, Letter::T, Letter::E},
        Point{7, 4}, Point{7, 10});
    CHECK(score == 70);
}

// 9. Pre-placed letters score zero; only the new tile is counted.
// Setup: ACE horizontal at (7,5) → 10.
// Move: ACES horizontal at (7,5): A C E pre-placed (skip), S(7,8)=DEFAULT(1).
// word_score=1, multiplier=1 → 1.
TEST_CASE("Scoring: ACES horizontal, overlap scores only new tile", "[scoring]") {
    auto g = make_gaddag({});
    Board b(g);
    b.place_word({Letter::A, Letter::C, Letter::E}, Point{7, 5}, Point{7, 7}); // setup
    int score = b.place_word(
        {Letter::A, Letter::C, Letter::E, Letter::S}, Point{7, 5}, Point{7, 8});
    CHECK(score == 1);
}

// 10. Cross-word formed below a new horizontal tile.
// Move 1: AT vertical at (6,7): A(6,7)=DEFAULT(1), T(7,7)=DWS(1,×2) → 4.
// Move 2: SO horizontal at (5,6): S(5,6)=DEFAULT(1); O(5,7)=DEFAULT(1) +
// cross below: A(6,7)+T(7,7)=2, plus O contribution=1 → additional=3.
// word_score=2, multiplier=1, additional=3 → 5.
TEST_CASE("Scoring: SO horizontal, cross-word below from vertical tiles", "[scoring]") {
    auto g = make_gaddag({});
    Board b(g);
    b.place_word({Letter::A, Letter::T}, Point{6, 7}, Point{7, 7}); // setup: AT vertical
    int score = b.place_word(
        {Letter::S, Letter::O}, Point{5, 6}, Point{5, 7});
    CHECK(score == 5);
}

// 11. Cross-word on the right side of a new vertical tile (tests fixed bug).
// Move 1: X vertical at (7,7) → DWS → 16.
// Move 2: ACE horizontal at (6,8): A(6,8)=DLS(2), C(6,9)=3, E(6,10)=1 → 6.
// Move 3: BA vertical at (5,7): B(5,7)=DEFAULT(3); A(6,7)=DEFAULT(1) +
// cross right: A(6,8)+C(6,9)+E(6,10)=5, plus A contribution=1 → additional=6.
// word_score=4, multiplier=1, additional=6 → 10.
TEST_CASE("Scoring: BA vertical, cross-word on right side", "[scoring]") {
    auto g = make_gaddag({});
    Board b(g);
    b.place_word({Letter::X}, Point{7, 7}, Point{7, 7});                            // X at center
    b.place_word({Letter::A, Letter::C, Letter::E}, Point{6, 8}, Point{6, 10});     // ACE horizontal
    int score = b.place_word(
        {Letter::B, Letter::A}, Point{5, 7}, Point{6, 7});
    CHECK(score == 10);
}

// 12. Cross-word on the left side of a new vertical tile.
// Move 1: ACE horizontal at (7,4): A(7,4)=1, C(7,5)=3, E(7,6)=1 → 5.
// Move 2: BA vertical at (6,7): B(6,7)=DEFAULT(3); A(7,7)=DEFAULT(1) +
// cross left: E(7,6)+C(7,5)+A(7,4)=5, plus A contribution=1 → additional=6.
// word_score=4, multiplier=1, additional=6 → 10.
TEST_CASE("Scoring: BA vertical, cross-word on left side", "[scoring]") {
    auto g = make_gaddag({});
    Board b(g);
    b.place_word({Letter::A, Letter::C, Letter::E}, Point{7, 4}, Point{7, 6}); // ACE horizontal
    int score = b.place_word(
        {Letter::B, Letter::A}, Point{6, 7}, Point{7, 7});
    CHECK(score == 10);
}

// ── validate_board tests ───────────────────────────────────────────────────

TEST_CASE("validate_board: empty board returns no invalid words", "[validate_board]") {
    auto g = make_gaddag({"CAT"});
    Board b(g);
    CHECK(b.validate_board().empty());
}

TEST_CASE("validate_board: valid horizontal word", "[validate_board]") {
    auto g = make_gaddag({"CAT"});
    Board b(g);
    b.place_word({Letter::C, Letter::A, Letter::T}, Point{7, 7}, Point{7, 9});
    CHECK(b.validate_board().empty());
}

TEST_CASE("validate_board: valid vertical word", "[validate_board]") {
    auto g = make_gaddag({"CAT"});
    Board b(g);
    b.place_word({Letter::C, Letter::A, Letter::T}, Point{7, 7}, Point{9, 7});
    CHECK(b.validate_board().empty());
}

TEST_CASE("validate_board: valid crossing words", "[validate_board]") {
    // CAT horizontal: C@(7,7) A@(7,8) T@(7,9)
    // ACT vertical:   A@(5,9) C@(6,9) T@(7,9)  -- T is shared
    auto g = make_gaddag({"CAT", "ACT"});
    Board b(g);
    b.place_word({Letter::C, Letter::A, Letter::T}, Point{7, 7}, Point{7, 9});
    b.place_word({Letter::A, Letter::C, Letter::T}, Point{5, 9}, Point{7, 9});
    CHECK(b.validate_board().empty());
}

TEST_CASE("validate_board: detects invalid horizontal word", "[validate_board]") {
    // Bypass place_word to put a non-word sequence directly on the board
    auto g = make_gaddag({"CAT"});
    Board b(g);
    b.board[7][7] = Letter::X;
    b.board[7][8] = Letter::Q;
    b.board[7][9] = Letter::Z;
    auto invalid = b.validate_board();
    CHECK(invalid.size() == 1);
    CHECK(invalid[0] == "XQZ");
}

TEST_CASE("validate_board: detects invalid vertical word", "[validate_board]") {
    auto g = make_gaddag({"CAT"});
    Board b(g);
    b.board[5][3] = Letter::Z;
    b.board[6][3] = Letter::X;
    auto invalid = b.validate_board();
    CHECK(invalid.size() == 1);
    CHECK(invalid[0] == "ZX");
}

TEST_CASE("validate_board: mix of valid and invalid words", "[validate_board]") {
    // CAT (valid) via place_word + XQ (invalid) injected directly
    auto g = make_gaddag({"CAT"});
    Board b(g);
    b.place_word({Letter::C, Letter::A, Letter::T}, Point{7, 7}, Point{7, 9});
    b.board[0][0] = Letter::X;
    b.board[0][1] = Letter::Q;
    auto invalid = b.validate_board();
    CHECK(invalid.size() == 1);
    CHECK(invalid[0] == "XQ");
}
