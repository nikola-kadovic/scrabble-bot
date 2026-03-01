#pragma once

#include "gaddag.hpp"
#include "letter.hpp"
#include "move.hpp"
#include "point.hpp"

#include <array>
#include <memory>
#include <string>
#include <unordered_set>
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

using CrossChecks =
    std::array<std::array<CrossCheckSet, BOARD_COLS>, BOARD_ROWS>;

class Board {
public:
  std::array<std::array<Letter, BOARD_COLS>, BOARD_ROWS> board;
  std::array<std::array<SquareType, BOARD_COLS>, BOARD_ROWS> square_types;

  CrossChecks horizontal_cross_checks; // letters valid for horizontal words
                                       // (used when placing vertically)
  CrossChecks vertical_cross_checks;   // letters valid for vertical words (used
                                       // when placing horizontally)

  std::unordered_set<Point, PointHash> anchor_points;

  explicit Board(std::shared_ptr<Gaddag> dictionary);

  // Place a word (list of Letters). Direction inferred: start.row == end.row →
  // horizontal; start.col == end.col → vertical. Throws std::invalid_argument
  // on invalid placement. Returns the points earned from placing the word.
  int place_word(const std::vector<Letter> &word, Point start, Point end);

  // Convenience: apply a pre-built Move directly.
  int place_word(const Move &move);

  // Compute the score for placing word at (row, col), horizontal or vertical,
  // without modifying the board. Assumes the placement is valid.
  int calculate_score(const std::vector<Letter> &word, int row, int col,
                      bool vertical) const;

  std::string to_string() const;

  // Debugging: scans every row and column for contiguous tile runs and returns
  // the string representation of any runs that are not valid dictionary words.
  // An empty result means the board is fully valid.
  std::vector<std::string> validate_board() const;

  // Returns all valid moves for the given rack as a list of Move objects.
  std::vector<Move> get_all_valid_moves(const std::vector<Letter> &rack) const;

private:
  std::shared_ptr<Gaddag> dict_;
  bool first_move_;

  // Tile entry used during move generation
  struct TileEntry {
    Letter gaddag_letter; // A-Z (never BLANK; blanks store their display letter)
    bool is_blank;        // true = was played as a blank tile (scores 0)
    bool is_new;          // true = placed from rack; false = existing board tile

    Letter score_letter() const {
      return is_blank ? Letter::BLANK : gaddag_letter;
    }

    std::string letters_str() const {
      if (is_blank) {
        char c = letter_to_char(gaddag_letter);
        return std::string(1,
                           static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
      }
      return letter_to_key(gaddag_letter);
    }
  };

  void build_square_types();

  int place_word_horizontally(const std::vector<Letter> &word, int row,
                              int col);
  int place_word_vertically(const std::vector<Letter> &word, int row, int col);

  void update_horizontal_cross_checks(int row, int col);
  void update_vertical_cross_checks(int row, int col);

  bool letter_makes_word_horizontally(int row, int col, Letter letter) const;
  bool letter_makes_word_vertically(int row, int col, Letter letter) const;

  void update_anchor_points(int r1, int c1, int r2, int c2);

  static bool in_bounds(int row, int col) noexcept {
    return row >= 0 && row < BOARD_ROWS && col >= 0 && col < BOARD_COLS;
  }

  // Returns true if `word` exists in the dictionary via GADDAG traversal.
  bool is_word_valid(const std::vector<Letter> &word) const;

  // ── Move generation helpers ──────────────────────────────────────────────

  // Returns the cross-check set for position (r,c) given move direction.
  // Horizontal moves check vertical_cross_checks; vertical check horizontal.
  CrossCheckSet get_cross_checks_for(int r, int c, bool vertical) const;

  // Record a completed move into results.
  void record_move(const std::vector<TileEntry> &left_part,
                   const std::vector<TileEntry> &right_part, int anchor_r,
                   int anchor_c, bool vertical,
                   std::vector<Move> &results) const;

  // Extend the word to the right from position (r,c).
  void extend_right(int r, int c, bool vertical, std::shared_ptr<State> state,
                    const std::vector<Letter> &rack,
                    std::vector<TileEntry> &left_part,
                    std::vector<TileEntry> &right_part, int anchor_r,
                    int anchor_c, std::vector<Move> &results) const;

  // Extend the word to the left from position (r,c).
  void extend_left(int r, int c, bool vertical, std::shared_ptr<State> state,
                   const std::vector<Letter> &rack, int limit,
                   std::vector<TileEntry> &left_part, int anchor_r,
                   int anchor_c, std::vector<Move> &results) const;

  // Generate all valid moves for a single anchor point.
  void generate_for_anchor(int r, int c, bool vertical,
                            const std::vector<Letter> &rack,
                            std::vector<Move> &results) const;
};

} // namespace scrabble
