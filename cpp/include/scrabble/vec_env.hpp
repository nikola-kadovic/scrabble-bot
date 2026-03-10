#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "scrabble/env.hpp"
#include "scrabble/gaddag.hpp"

namespace scrabble {

// N parallel Scrabble game environments sharing zero-copy numpy-compatible buffers.
class VectorizedEnv {
 public:
  VectorizedEnv(const std::shared_ptr<Gaddag>& dict, int num_envs, uint32_t seed = 0);

  // Reset all environments and populate all buffers.
  void reset_all();

  // For each env: read action_buf_[i], step, auto-reset on done, fill obs and move buffers.
  // Outer loop parallelised with OpenMP; inner get_all_valid_moves uses serial branch
  // (omp_in_parallel() guard in move_gen.cpp prevents thread oversubscription).
  void step_all();

  int num_envs() const { return num_envs_; }

  // Public typed buffer members — exposed directly as numpy arrays via pybind11 buffer protocol.
  // Python reads/writes these zero-copy.
  std::vector<float>   obs_buf_;        // [N × OBS_DIM]
  std::vector<int32_t> action_buf_;     // [N]               Python writes before step_all()
  std::vector<float>   reward_buf_;     // [N]
  std::vector<uint8_t> done_buf_;       // [N]               0 or 1
  std::vector<int32_t> num_moves_buf_;  // [N]

  std::vector<int8_t>  move_coords_;    // [N × MAX_MOVES_PER_ENV × 4]   start_r,c end_r,c
  std::vector<uint8_t> move_letters_;   // [N × MAX_MOVES_PER_ENV × 7]   26=unused slot
  std::vector<uint8_t> move_is_blank_;  // [N × MAX_MOVES_PER_ENV × 7]
  std::vector<int16_t> move_scores_;    // [N × MAX_MOVES_PER_ENV]

 private:
  int num_envs_;
  std::vector<ScrabbleEnv> envs_;
};

}  // namespace scrabble
