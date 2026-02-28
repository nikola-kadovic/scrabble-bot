#pragma once

#include "letter.hpp"
#include "point.hpp"

#include <vector>

namespace scrabble {

struct Move {
  Point start;                     // top-left corner of the word span
  Point end;                       // bottom-right corner of the word span
  std::vector<Letter> letters;     // actual letter at each position (A-Z, never BLANK)
  std::vector<bool> is_blank;      // true if that position was played from a blank tile
  int score;
  // Direction inferred: start.row == end.row → horizontal; start.col == end.col → vertical

  Move(Point s, Point e, std::vector<Letter> l, std::vector<bool> blank_mask, int sc)
      : start(s), end(e), letters(std::move(l)),
        is_blank(std::move(blank_mask)), score(sc) {}
};

} // namespace scrabble
