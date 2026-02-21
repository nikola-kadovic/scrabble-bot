#pragma once

#include <string>
#include <vector>

namespace scrabble {

struct Move {
  int starting_index;
  int ending_index;
  std::vector<std::string> letters; // letters placed on board, in order
  int score;

  Move(int si, int ei, std::vector<std::string> l, int s)
      : starting_index(si), ending_index(ei), letters(std::move(l)), score(s) {}
};

} // namespace scrabble
