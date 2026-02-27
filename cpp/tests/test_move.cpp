#include <catch2/catch_test_macros.hpp>
#include "scrabble/move.hpp"
#include "scrabble/point.hpp"

using namespace scrabble;

TEST_CASE("Move can be constructed", "[move]") {
    Move mv(Point{0, 0}, Point{0, 5}, {"A", "B", "C"}, 10);
    CHECK(mv.start.row == 0);
    CHECK(mv.start.col == 0);
    CHECK(mv.end.row   == 0);
    CHECK(mv.end.col   == 5);
    CHECK(mv.letters   == std::vector<std::string>{"A", "B", "C"});
    CHECK(mv.score     == 10);
}

TEST_CASE("Move fields are mutable", "[move]") {
    Move mv(Point{1, 1}, Point{1, 2}, {}, 0);
    mv.score = 42;
    CHECK(mv.score == 42);
}

TEST_CASE("Move direction inferred from start/end", "[move]") {
    // Horizontal: same row
    Move horiz(Point{7, 5}, Point{7, 9}, {"A", "B", "C", "D", "E"}, 5);
    CHECK(horiz.start.row == horiz.end.row);

    // Vertical: same col
    Move vert(Point{3, 7}, Point{7, 7}, {"A", "B", "C", "D", "E"}, 5);
    CHECK(vert.start.col == vert.end.col);
}
