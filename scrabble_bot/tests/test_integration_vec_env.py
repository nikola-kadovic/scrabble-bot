"""
Integration tests for ScrabbleEnv and VectorizedEnv C++ bindings.
"""

import numpy as np
import pytest

from scrabble_bot._cpp_ext import (
    MAX_MOVES_PER_ENV,
    OBS_DIM,
    Gaddag,
    ScrabbleEnv,
    VectorizedEnv,
)


@pytest.fixture(scope="module")
def gaddag():
    g = Gaddag()
    g.build_from_file("dictionary/nwl_2023.txt", use_cache=True)
    return g


@pytest.fixture(scope="module")
def vec_env(gaddag):
    N = 4
    ve = VectorizedEnv(gaddag, N, seed=0)
    ve.reset_all()
    return ve


# ── Buffer shapes and dtypes ─────────────────────────────────────────────────

def test_observation_shape_and_dtype(vec_env):
    N = vec_env.num_envs
    obs = vec_env.observations
    assert obs.shape == (N, OBS_DIM), f"Expected ({N}, {OBS_DIM}), got {obs.shape}"
    assert obs.dtype == np.float32

    coords = vec_env.move_coords
    assert coords.shape == (N, MAX_MOVES_PER_ENV, 4)
    assert coords.dtype == np.int8

    letters = vec_env.move_letters
    assert letters.shape == (N, MAX_MOVES_PER_ENV, 7)
    assert letters.dtype == np.uint8

    is_blank = vec_env.move_is_blank
    assert is_blank.shape == (N, MAX_MOVES_PER_ENV, 7)
    assert is_blank.dtype == np.uint8

    scores = vec_env.move_scores
    assert scores.shape == (N, MAX_MOVES_PER_ENV)
    assert scores.dtype == np.int16

    n_moves = vec_env.num_moves
    assert n_moves.shape == (N,)
    assert n_moves.dtype == np.int32

    actions = vec_env.actions
    assert actions.shape == (N,)
    assert actions.dtype == np.int32


def test_observation_values_in_range(gaddag):
    N = 4
    ve = VectorizedEnv(gaddag, N, seed=1)
    ve.reset_all()

    obs = ve.observations
    # All observations should be in [0.0, 1.0]
    assert np.all(obs >= 0.0), "Observation has values below 0"
    assert np.all(obs <= 1.0), "Observation has values above 1"

    # Empty board after reset: channel 26 (empty) == 1.0 for all 225 squares
    board_block = obs[:, :6075].reshape(N, 15, 15, 27)
    empty_ch = board_block[:, :, :, 26]
    assert np.all(empty_ch == 1.0), "Empty board should have ch26==1.0 everywhere"

    letter_chs = board_block[:, :, :, :26]
    assert np.all(letter_chs == 0.0), "Empty board should have ch0-25==0.0 everywhere"


# ── Valid-move counts ─────────────────────────────────────────────────────────

def test_num_moves_at_least_one(vec_env):
    N = vec_env.num_envs
    n = vec_env.num_moves
    assert np.all(n >= 1), "Every env should have at least the pass move"

    # Last move of each env is the pass: coords (-1,-1,-1,-1)
    coords = vec_env.move_coords
    for i in range(N):
        last = int(n[i]) - 1
        assert coords[i, last, 0] == -1, f"Env {i}: last move start_row should be -1 (pass)"


# ── Zero-copy write to action buffer ─────────────────────────────────────────

def test_zero_copy_action_write(gaddag):
    N = 4
    ve = VectorizedEnv(gaddag, N, seed=2)
    ve.reset_all()

    # Write pass action (large index treated as pass in C++)
    actions = ve.actions
    actions[:] = MAX_MOVES_PER_ENV

    ve.step_all()

    # Rewards should be finite after a valid step
    rewards = ve.rewards
    assert np.all(np.isfinite(rewards)), "Rewards should be finite after step_all()"


# ── Clone independence ────────────────────────────────────────────────────────

def test_scrabble_env_clone_independence(gaddag):
    original = ScrabbleEnv(gaddag, seed=42)
    original.reset()

    # Copy constructor — for MCTS cloning
    clone = ScrabbleEnv(original)

    # Step the clone (pass)
    clone.step(MAX_MOVES_PER_ENV)

    # Original should be unaffected
    assert original.consecutive_passes == 0, "Original should not be affected by clone step"
    assert original.current_player == 0
    assert clone.consecutive_passes == 1
    assert clone.current_player == 1


# ── Episode completion ────────────────────────────────────────────────────────

def test_episode_completion(gaddag):
    N = 4
    ve = VectorizedEnv(gaddag, N, seed=3)
    ve.reset_all()

    # Force done via 6 passes for all envs
    for _ in range(6):
        np.copyto(ve.actions, np.full(N, MAX_MOVES_PER_ENV, dtype=np.int32))
        ve.step_all()

    # After 6 passes, all envs should have been reset (dones triggered, then reset)
    # After the auto-reset, dones become 0 again on next step
    np.copyto(ve.actions, np.full(N, MAX_MOVES_PER_ENV, dtype=np.int32))
    ve.step_all()
    dones = ve.dones
    # Envs were auto-reset and are now in fresh state, so done should be 0
    assert np.all(dones == 0), "After auto-reset, done should be 0"


def test_rewards_finite(gaddag):
    N = 4
    ve = VectorizedEnv(gaddag, N, seed=4)
    ve.reset_all()

    for _ in range(100):
        # Use pass action for simplicity
        np.copyto(ve.actions, np.zeros(N, dtype=np.int32))
        ve.step_all()
        assert np.all(np.isfinite(ve.rewards)), "All rewards should be finite"
