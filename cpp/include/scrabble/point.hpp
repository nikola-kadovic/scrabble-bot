#pragma once

#include <cstddef>

namespace scrabble {

struct Point {
  int row;
  int col;

  bool operator==(const Point &o) const noexcept {
    return row == o.row && col == o.col;
  }
};

struct PointHash {
  size_t operator()(const Point &p) const noexcept {
    return static_cast<size_t>(p.row) * 31 + static_cast<size_t>(p.col);
  }
};

} // namespace scrabble
