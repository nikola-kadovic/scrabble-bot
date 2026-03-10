#include "scrabble/vec_env.hpp"

#ifdef _OPENMP
#include <omp.h>
#endif

namespace scrabble {

VectorizedEnv::VectorizedEnv(const std::shared_ptr<Gaddag>& dict, int num_envs, uint32_t seed)
    : num_envs_(num_envs) {
#ifdef _OPENMP
  // Prevent nested parallelism: inner get_all_valid_moves uses omp_in_parallel() guard.
  omp_set_max_active_levels(1);
#endif
  envs_.reserve(num_envs);
  for (int i = 0; i < num_envs; i++) {
    envs_.emplace_back(dict, seed + static_cast<uint32_t>(i));
  }

  // Allocate all shared buffers
  obs_buf_.assign(static_cast<size_t>(num_envs) * OBS_DIM, 0.0f);
  action_buf_.assign(num_envs, 0);
  reward_buf_.assign(num_envs, 0.0f);
  done_buf_.assign(num_envs, 0u);
  num_moves_buf_.assign(num_envs, 0);

  const size_t env_moves = static_cast<size_t>(num_envs) * MAX_MOVES_PER_ENV;
  move_coords_.assign(env_moves * 4, 0);
  move_letters_.assign(env_moves * 7, 26u);
  move_is_blank_.assign(env_moves * 7, 0u);
  move_scores_.assign(env_moves, 0);
}

void VectorizedEnv::reset_all() {
  for (int i = 0; i < num_envs_; i++) {
    envs_[i].reset();
    envs_[i].fill_observation(obs_buf_.data() + static_cast<size_t>(i) * OBS_DIM);

    const size_t base = static_cast<size_t>(i) * MAX_MOVES_PER_ENV;
    num_moves_buf_[i] = envs_[i].get_valid_moves(
        move_coords_.data()   + base * 4,
        move_letters_.data()  + base * 7,
        move_is_blank_.data() + base * 7,
        move_scores_.data()   + base,
        MAX_MOVES_PER_ENV);
  }
}

void VectorizedEnv::step_all() {
#pragma omp parallel for schedule(dynamic, 1)
  for (int i = 0; i < num_envs_; i++) {
    auto r = envs_[i].step(action_buf_[i]);
    reward_buf_[i] = r.reward;
    done_buf_[i]   = r.done ? 1u : 0u;
    if (r.done) envs_[i].reset();

    envs_[i].fill_observation(obs_buf_.data() + static_cast<size_t>(i) * OBS_DIM);

    const size_t base = static_cast<size_t>(i) * MAX_MOVES_PER_ENV;
    num_moves_buf_[i] = envs_[i].get_valid_moves(
        move_coords_.data()   + base * 4,
        move_letters_.data()  + base * 7,
        move_is_blank_.data() + base * 7,
        move_scores_.data()   + base,
        MAX_MOVES_PER_ENV);
  }
}

}  // namespace scrabble
