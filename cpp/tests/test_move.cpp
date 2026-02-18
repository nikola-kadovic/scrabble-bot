#include <catch2/catch_test_macros.hpp>
#include "scrabble/move.hpp"

using namespace scrabble;

TEST_CASE("Move can be constructed", "[move]") {
    Move mv(0, 5, {"A", "B", "C"}, 10);
    CHECK(mv.starting_index == 0);
    CHECK(mv.ending_index   == 5);
    CHECK(mv.letters        == std::vector<std::string>{"A", "B", "C"});
    CHECK(mv.score          == 10);
}

TEST_CASE("Move fields are mutable", "[move]") {
    Move mv(1, 2, {}, 0);
    mv.score = 42;
    CHECK(mv.score == 42);
}
