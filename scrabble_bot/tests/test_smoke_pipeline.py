"""
End-to-end smoke tests for the full RL pipeline:
  VectorizedEnv → TestingValuationFunction → ScrabbleVecEnv → ReplayBuffer
"""

import numpy as np
import pytest

from scrabble_bot._cpp_ext import (
    MAX_MOVES_PER_ENV,
    OBS_DIM,
    Gaddag,
    ScrabbleEnv,
)
from scrabble_bot.replay_buffer import ReplayBuffer
from scrabble_bot.vec_env import ScrabbleVecEnv, TestingValuationFunction


@pytest.fixture(scope="module")
def gaddag():
    g = Gaddag()
    g.build_from_file("dictionary/nwl_2023.txt", use_cache=True)
    return g


# ── Full pipeline smoke test ──────────────────────────────────────────────────

def test_smoke_50_steps_no_crash(gaddag):
    N = 4
    env = ScrabbleVecEnv(gaddag, num_envs=N, seed=0)
    valuation = TestingValuationFunction()
    buf = ReplayBuffer(capacity=1000)

    for _ in range(50):
        result = env.step_with_inference(valuation)
        buf.push_batch(
            result["obs"],
            result["values"][:, None] * np.ones((N, MAX_MOVES_PER_ENV), dtype=np.float32),
            result["values"],
            env.vec_env.num_moves,
        )

    assert len(buf) == 50 * N


def test_smoke_replay_buffer_sample_shapes(gaddag):
    N = 4
    env = ScrabbleVecEnv(gaddag, num_envs=N, seed=1)
    valuation = TestingValuationFunction()
    buf = ReplayBuffer(capacity=1000)

    for _ in range(50):
        result = env.step_with_inference(valuation)
        buf.push_batch(
            result["obs"],
            result["values"][:, None] * np.ones((N, MAX_MOVES_PER_ENV), dtype=np.float32),
            result["values"],
            env.vec_env.num_moves,
        )

    batch = buf.sample(16)
    assert batch["obs"].shape == (16, OBS_DIM)
    assert batch["mcts_policy"].shape == (16, MAX_MOVES_PER_ENV)
    assert batch["value_target"].shape == (16,)


def test_smoke_testing_valuation_function_output(gaddag):
    N = 4
    env = ScrabbleVecEnv(gaddag, num_envs=N, seed=2)
    valuation = TestingValuationFunction()

    obs = env.vec_env.observations
    coords = env.vec_env.move_coords
    letters = env.vec_env.move_letters
    scores = env.vec_env.move_scores
    n = env.vec_env.num_moves

    logits, values = valuation(obs, coords, letters, scores, n)

    assert logits.shape == (N, MAX_MOVES_PER_ENV), f"Expected ({N},{MAX_MOVES_PER_ENV})"
    assert values.shape == (N,), f"Expected ({N},)"
    assert np.all(np.isfinite(logits)), "All logits should be finite"
    assert np.all(np.isfinite(values)), "All values should be finite"

    # Policy logits should match scores cast to float32
    np.testing.assert_array_equal(logits, scores.astype(np.float32))


def test_smoke_scrabble_env_standalone_mcts_usage(gaddag):
    env = ScrabbleEnv(gaddag, seed=7)
    env.reset()

    moves = env.get_valid_moves()
    assert len(moves) >= 1, "Should have at least the pass move"

    # Clone for MCTS
    clone = ScrabbleEnv(env)
    result = clone.step(MAX_MOVES_PER_ENV)  # pass
    assert isinstance(result.reward, float), "StepResult.reward should be float"
    assert isinstance(result.done, bool), "StepResult.done should be bool"

    # Original should be unchanged
    orig_moves = env.get_valid_moves()
    assert len(orig_moves) >= 1
    assert env.consecutive_passes == 0, "Original env should not be affected by clone step"
