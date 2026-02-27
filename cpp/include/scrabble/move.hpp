#pragma once

#include "point.hpp"

#include <string>
#include <vector>

namespace scrabble {

struct Move {
  Point start;                      // top-left corner of the word span
  Point end;                        // bottom-right corner of the word span
  std::vector<std::string> letters; // all letters of the full word in order
  int score;
  // Direction inferred: start.row == end.row → horizontal; start.col == end.col → vertical

  Move(Point s, Point e, std::vector<std::string> l, int sc)
      : start(s), end(e), letters(std::move(l)), score(sc) {}
};

} // namespace scrabble
