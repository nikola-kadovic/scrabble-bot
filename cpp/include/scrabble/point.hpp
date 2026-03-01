#pragma once

namespace scrabble {

struct Point {
  int row;
  int col;

  bool operator==(const Point& o) const noexcept { return row == o.row && col == o.col; }
};

}  // namespace scrabble
