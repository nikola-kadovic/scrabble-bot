#pragma once

#include "gaddag.hpp"
#include "letter.hpp"
#include "move.hpp"

#include <array>
#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

namespace scrabble {

constexpr int BOARD_ROWS = 15;
constexpr int BOARD_COLS = 15;

enum class SquareType : uint8_t {
    DEFAULT = 0,
    DOUBLE_WORD,
    TRIPLE_WORD,
    DOUBLE_LETTER,
    TRIPLE_LETTER
};

struct Point {
    int row;
    int col;

    bool operator==(const Point& o) const noexcept {
        return row == o.row && col == o.col;
    }
};

struct PointHash {
    size_t operator()(const Point& p) const noexcept {
        return static_cast<size_t>(p.row) * 31 + static_cast<size_t>(p.col);
    }
};

using CrossChecks = std::array<std::array<std::unordered_set<Letter>, BOARD_COLS>, BOARD_ROWS>;

class Board {
public:
    std::array<std::array<Letter, BOARD_COLS>, BOARD_ROWS> board;
    std::array<std::array<SquareType, BOARD_COLS>, BOARD_ROWS> square_types;

    CrossChecks horizontal_cross_checks; // letters valid for horizontal words (used when placing vertically)
    CrossChecks vertical_cross_checks;   // letters valid for vertical words (used when placing horizontally)

    std::unordered_set<Point, PointHash> anchor_points;

    explicit Board(std::shared_ptr<Gaddag> dictionary);

    // Place a word (list of Letters) starting at (row,col), horizontal or vertical.
    // Throws std::invalid_argument on invalid placement.
    void place_word(const std::vector<Letter>& word,
                    std::pair<int, int> starting_point,
                    bool vertical);

    std::string to_string() const;

private:
    std::shared_ptr<Gaddag> dict_;
    bool first_move_;

    void build_square_types();

    void place_word_horizontally(const std::vector<Letter>& word, int row, int col);
    void place_word_vertically(const std::vector<Letter>& word, int row, int col);

    void update_horizontal_cross_checks(int row, int col);
    void update_vertical_cross_checks(int row, int col);

    bool letter_makes_word_horizontally(int row, int col, Letter letter) const;
    bool letter_makes_word_vertically(int row, int col, Letter letter) const;

    void update_anchor_points(int r1, int c1, int r2, int c2);

    static bool in_bounds(int row, int col) noexcept {
        return row >= 0 && row < BOARD_ROWS && col >= 0 && col < BOARD_COLS;
    }
};

} // namespace scrabble
