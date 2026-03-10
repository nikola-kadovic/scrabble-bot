#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "scrabble/board.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/letter.hpp"
#include "scrabble/move.hpp"

namespace scrabble {

constexpr int OBS_DIM = 6106;
constexpr int MAX_MOVES_PER_ENV = 4096;

// Single Scrabble game environment. Copyable (Board is a value type) for MCTS cloning.
class ScrabbleEnv {
 public:
  struct StepResult {
    float reward;
    bool done;
  };

  explicit ScrabbleEnv(std::shared_ptr<Gaddag> dict, uint32_t seed = 0);
  ScrabbleEnv(const ScrabbleEnv&) = default;
  ScrabbleEnv& operator=(const ScrabbleEnv&) = default;

  // Reset to a fresh game (re-shuffles bag, draws new racks, clears board).
  void reset();

  // Write OBS_DIM floats to buf:
  //   [0..6074]   board 15×15×27 one-hot  (ch26 == empty)
  //   [6075..6101] rack 27 floats          (count/7.0 per letter)
  //   [6102..6105] metadata               (score0/500, score1/500, player, passes/6)
  void fill_observation(float* buf) const noexcept;

  // Write move data into pre-allocated caller-owned arrays.
  // Pass move is always last: coords=(-1,-1,-1,-1), letters=26-filled, score=0.
  // Returns number of entries written (including pass).
  int get_valid_moves(int8_t* coords,    // [max_n × 4]  start_r,c end_r,c
                      uint8_t* letters,  // [max_n × 7]  new-tile letters (26=unused)
                      uint8_t* is_blank, // [max_n × 7]  1 if blank tile
                      int16_t* scores,   // [max_n]
                      int max_n) const;

  // Python-friendly: returns list of Move objects with pass as last sentinel.
  std::vector<Move> get_all_valid_moves_py() const;

  StepResult step(int action_idx);

  // Accessors (for tests and Python bindings)
  bool done() const { return done_; }
  int current_player() const { return current_player_; }
  int consecutive_passes() const { return consecutive_passes_; }
  const std::array<int, 2>& player_scores() const { return scores_; }
  const std::vector<Letter>& rack(int player) const { return racks_[player]; }
  const Board& board() const { return board_; }
  int bag_remaining() const { return bag_end_; }

 private:
  std::shared_ptr<Gaddag> dict_;
  Board board_;
  std::mt19937 rng_;
  std::array<Letter, 100> bag_{};
  int bag_end_{100};  // bag_[0..bag_end_-1] are remaining tiles

  std::array<std::vector<Letter>, 2> racks_;
  int current_player_{0};
  std::array<int, 2> scores_{0, 0};
  bool done_{false};
  int consecutive_passes_{0};

  mutable std::vector<Move> cached_moves_;
  mutable bool moves_dirty_{true};

  void init_bag();
  void draw_tiles(int player, int count);
  void ensure_moves_valid() const;
};

}  // namespace scrabble
