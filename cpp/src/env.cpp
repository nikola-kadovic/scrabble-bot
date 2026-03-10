#include "scrabble/env.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace scrabble {

// Standard 100-tile Scrabble bag distribution.
static constexpr std::pair<Letter, int> BAG_DIST[27] = {
    {Letter::A, 9},   {Letter::B, 2},  {Letter::C, 2},  {Letter::D, 4},
    {Letter::E, 12},  {Letter::F, 2},  {Letter::G, 3},  {Letter::H, 2},
    {Letter::I, 9},   {Letter::J, 1},  {Letter::K, 1},  {Letter::L, 4},
    {Letter::M, 2},   {Letter::N, 6},  {Letter::O, 8},  {Letter::P, 2},
    {Letter::Q, 1},   {Letter::R, 6},  {Letter::S, 4},  {Letter::T, 6},
    {Letter::U, 4},   {Letter::V, 2},  {Letter::W, 2},  {Letter::X, 1},
    {Letter::Y, 2},   {Letter::Z, 1},  {Letter::BLANK, 2},
};

ScrabbleEnv::ScrabbleEnv(std::shared_ptr<Gaddag> dict, uint32_t seed)
    : dict_(std::move(dict)), board_(dict_), rng_(seed) {
  init_bag();
  std::shuffle(bag_.begin(), bag_.end(), rng_);
  draw_tiles(0, 7);
  draw_tiles(1, 7);
}

void ScrabbleEnv::init_bag() {
  int idx = 0;
  for (auto [letter, count] : BAG_DIST) {
    for (int i = 0; i < count; i++) bag_[idx++] = letter;
  }
}

void ScrabbleEnv::draw_tiles(int player, int count) {
  for (int i = 0; i < count; i++) {
    racks_[player].push_back(bag_[--bag_end_]);
  }
}

void ScrabbleEnv::reset() {
  board_ = Board(dict_);
  bag_end_ = 100;
  init_bag();
  std::shuffle(bag_.begin(), bag_.end(), rng_);
  racks_[0].clear();
  racks_[1].clear();
  draw_tiles(0, 7);
  draw_tiles(1, 7);
  current_player_ = 0;
  scores_ = {0, 0};
  done_ = false;
  consecutive_passes_ = 0;
  moves_dirty_ = true;
  cached_moves_.clear();
}

void ScrabbleEnv::ensure_moves_valid() const {
  if (!moves_dirty_) return;
  if (done_) {
    cached_moves_.clear();
  } else {
    cached_moves_ = board_.get_all_valid_moves(racks_[current_player_]);
  }
  moves_dirty_ = false;
}

void ScrabbleEnv::fill_observation(float* buf) const noexcept {
  // ── Board encoding: 15×15×27 = 6075 floats ─────────────────────────────────
  for (int r = 0; r < BOARD_ROWS; r++) {
    for (int c = 0; c < BOARD_COLS; c++) {
      float* sq = buf + static_cast<ptrdiff_t>(r * BOARD_COLS + c) * 27;  // NOLINT
      std::fill(sq, sq + 27, 0.0f);
      Letter l = board_.board[r][c];
      if (l == Letter::BLANK) {
        sq[26] = 1.0f;  // empty square
      } else {
        sq[static_cast<uint8_t>(l)] = 1.0f;
      }
    }
  }
  // ── Rack encoding: 27 floats at offset 6075 ──────────────────────────────
  float* rack_buf = buf + 6075;
  std::fill(rack_buf, rack_buf + 27, 0.0f);
  for (Letter l : racks_[current_player_]) {
    rack_buf[static_cast<uint8_t>(l)] += 1.0f / 7.0f;
  }
  // ── Metadata: 4 floats at offset 6102 ────────────────────────────────────
  float* meta = buf + 6102;
  meta[0] = static_cast<float>(scores_[0]) / 500.0f;
  meta[1] = static_cast<float>(scores_[1]) / 500.0f;
  meta[2] = static_cast<float>(current_player_);
  meta[3] = static_cast<float>(consecutive_passes_) / 6.0f;
}

int ScrabbleEnv::get_valid_moves(int8_t* coords, uint8_t* letters, uint8_t* is_blank,
                                  int16_t* scores, int max_n) const {
  if (max_n <= 0) return 0;
  ensure_moves_valid();

  int n = static_cast<int>(cached_moves_.size());
  // +1 for the pass move; capped at max_n
  int count = std::min(n + 1, max_n);
  int num_regular = count - 1;

  for (int j = 0; j < num_regular; j++) {
    const Move& move = cached_moves_[j];

    // Coordinates
    coords[j * 4 + 0] = static_cast<int8_t>(move.start.row);
    coords[j * 4 + 1] = static_cast<int8_t>(move.start.col);
    coords[j * 4 + 2] = static_cast<int8_t>(move.end.row);
    coords[j * 4 + 3] = static_cast<int8_t>(move.end.col);

    // Score (clamped to int16_t range)
    scores[j] = static_cast<int16_t>(std::min(move.score, static_cast<int>(INT16_MAX)));

    // Letters: only NEW tiles (positions currently empty on the board)
    bool vertical = (move.start.col == move.end.col && move.start.row != move.end.row);
    int dr = vertical ? 1 : 0;
    int dc = vertical ? 0 : 1;
    int word_len = static_cast<int>(move.letters.size());

    int new_idx = 0;
    for (int k = 0; k < word_len && new_idx < 7; k++) {
      int r = move.start.row + k * dr;
      int c = move.start.col + k * dc;
      if (board_.board[r][c] == Letter::BLANK) {
        letters[j * 7 + new_idx] = static_cast<uint8_t>(move.letters[k]);
        is_blank[j * 7 + new_idx] = move.is_blank[k] ? 1u : 0u;
        new_idx++;
      }
    }
    // Pad unused slots with 26 (BLANK sentinel)
    for (int k = new_idx; k < 7; k++) {
      letters[j * 7 + k] = 26u;
      is_blank[j * 7 + k] = 0u;
    }
  }

  // Pass move (always last): coords = (-1,-1,-1,-1), letters = 26-filled, score = 0
  int last = count - 1;
  coords[last * 4 + 0] = -1;
  coords[last * 4 + 1] = -1;
  coords[last * 4 + 2] = -1;
  coords[last * 4 + 3] = -1;
  scores[last] = 0;
  for (int k = 0; k < 7; k++) {
    letters[last * 7 + k] = 26u;
    is_blank[last * 7 + k] = 0u;
  }

  return count;
}

std::vector<Move> ScrabbleEnv::get_all_valid_moves_py() const {
  ensure_moves_valid();
  std::vector<Move> result = cached_moves_;
  // Pass sentinel: start=end=(-1,-1), empty letters, score=0
  result.emplace_back(Point{-1, -1}, Point{-1, -1}, std::vector<Letter>{},
                      std::vector<bool>{}, 0);
  return result;
}

ScrabbleEnv::StepResult ScrabbleEnv::step(int action_idx) {
  ensure_moves_valid();

  int n = static_cast<int>(cached_moves_.size());
  bool is_pass = (action_idx >= n);  // index n (or beyond) means pass

  float reward = 0.0f;

  if (is_pass) {
    consecutive_passes_++;
    if (consecutive_passes_ >= 6) {
      done_ = true;
    }
  } else {
    consecutive_passes_ = 0;
    const Move& move = cached_moves_[action_idx];

    // Determine which positions are new tiles BEFORE placing
    bool vertical = (move.start.col == move.end.col && move.start.row != move.end.row);
    int dr = vertical ? 1 : 0;
    int dc = vertical ? 0 : 1;
    int word_len = static_cast<int>(move.letters.size());

    std::vector<Letter> tiles_to_remove;
    tiles_to_remove.reserve(7);
    for (int k = 0; k < word_len; k++) {
      int r = move.start.row + k * dr;
      int c = move.start.col + k * dc;
      if (board_.board[r][c] == Letter::BLANK) {
        // New tile: remove blank if played as blank, otherwise remove the letter
        tiles_to_remove.push_back(move.is_blank[k] ? Letter::BLANK : move.letters[k]);
      }
    }

    // Place the move and update score
    int score_gained = board_.place_word(move);
    scores_[current_player_] += score_gained;
    reward = static_cast<float>(score_gained) / 500.0f;

    // Remove consumed tiles from rack
    auto& rack = racks_[current_player_];
    for (Letter tile : tiles_to_remove) {
      auto it = std::find(rack.begin(), rack.end(), tile);
      if (it != rack.end()) rack.erase(it);
    }

    // Draw replacement tiles from bag
    int to_draw = std::min(7 - static_cast<int>(rack.size()), bag_end_);
    draw_tiles(current_player_, to_draw);

    // Game over when a player empties their rack and the bag is empty
    if (rack.empty() && bag_end_ == 0) {
      done_ = true;
    }
  }

  // Switch player and invalidate move cache
  current_player_ = 1 - current_player_;
  moves_dirty_ = true;

  return {reward, done_};
}

}  // namespace scrabble
