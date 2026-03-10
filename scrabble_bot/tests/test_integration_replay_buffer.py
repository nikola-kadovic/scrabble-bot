"""
Integration tests for ReplayBuffer.
"""

import numpy as np
import pytest

from scrabble_bot._cpp_ext import MAX_MOVES_PER_ENV, OBS_DIM
from scrabble_bot.replay_buffer import ReplayBuffer


def make_obs(B: int) -> np.ndarray:
    return np.random.rand(B, OBS_DIM).astype(np.float32)


def make_policy(B: int, max_moves: int = MAX_MOVES_PER_ENV) -> np.ndarray:
    p = np.random.rand(B, max_moves).astype(np.float32)
    p /= p.sum(axis=1, keepdims=True)
    return p


def make_values(B: int) -> np.ndarray:
    return (np.random.rand(B) * 2 - 1).astype(np.float32)


def make_n_moves(B: int, max_n: int = 100) -> np.ndarray:
    return np.random.randint(1, max_n + 1, size=B, dtype=np.int32)


# ── push_batch and sample ────────────────────────────────────────────────────

def test_push_and_sample_shapes():
    buf = ReplayBuffer(capacity=100)
    B = 10
    obs = make_obs(B)
    policy = make_policy(B)
    values = make_values(B)
    n = make_n_moves(B)

    buf.push_batch(obs, policy, values, n)
    assert len(buf) == B

    batch = buf.sample(5)
    assert batch["obs"].shape == (5, OBS_DIM)
    assert batch["mcts_policy"].shape == (5, MAX_MOVES_PER_ENV)
    assert batch["value_target"].shape == (5,)
    assert batch["n_moves"].shape == (5,)


# ── Circular overwrite ───────────────────────────────────────────────────────

def test_circular_overwrite():
    capacity = 50
    buf = ReplayBuffer(capacity=capacity)
    B = capacity + 10

    obs = make_obs(B)
    policy = make_policy(B)
    values = make_values(B)
    n = make_n_moves(B)

    buf.push_batch(obs, policy, values, n)

    # Buffer should not exceed capacity
    assert len(buf) == capacity

    # The 10 newest entries should be present; check a few value_targets
    # (ptr wraps: positions capacity..capacity+9 overwrite positions 0..9)
    recent_values = values[capacity:]  # last 10 entries
    for i, v in enumerate(recent_values):
        ptr_pos = i % capacity
        assert abs(buf.value_target[ptr_pos] - v) < 1e-5, \
            f"Position {ptr_pos}: expected {v}, got {buf.value_target[ptr_pos]}"


# ── Partial fill ─────────────────────────────────────────────────────────────

def test_sample_after_partial_fill():
    capacity = 100
    B = 30
    buf = ReplayBuffer(capacity=capacity)

    buf.push_batch(make_obs(B), make_policy(B), make_values(B), make_n_moves(B))
    assert len(buf) == B

    # sample up to B should work
    batch = buf.sample(B)
    assert batch["obs"].shape == (B, OBS_DIM)


def test_sample_too_large_raises():
    buf = ReplayBuffer(capacity=100)
    buf.push_batch(make_obs(10), make_policy(10), make_values(10), make_n_moves(10))

    with pytest.raises(ValueError):
        buf.sample(50)


# ── Stored values match inputs ────────────────────────────────────────────────

def test_value_target_stored_correctly():
    buf = ReplayBuffer(capacity=10)
    values = np.array([0.1, -0.5, 0.9, 0.0, -1.0], dtype=np.float32)
    obs = make_obs(len(values))
    policy = make_policy(len(values))
    n = make_n_moves(len(values))

    buf.push_batch(obs, policy, values, n)

    batch = buf.sample(len(values))
    # All stored values should appear in the sample (order may differ)
    sampled = sorted(batch["value_target"].tolist())
    expected = sorted(values.tolist())
    for s, e in zip(sampled, expected):
        assert abs(s - e) < 1e-5, f"Value mismatch: {s} vs {e}"


def test_mcts_policy_stored_correctly():
    buf = ReplayBuffer(capacity=10)
    B = 3
    # Use clearly distinct policies so matching is unambiguous
    policy = np.zeros((B, MAX_MOVES_PER_ENV), dtype=np.float32)
    for i in range(B):
        policy[i, i * 100] = 1.0  # each row has a unique spike

    obs = make_obs(B)
    values = make_values(B)
    n = make_n_moves(B)

    buf.push_batch(obs, policy, values, n)

    batch = buf.sample(B)
    # Sort both by argmax of each row; spikes are at 0, 100, 200 — guaranteed distinct
    batch_order = np.argsort(np.argmax(batch["mcts_policy"], axis=1))
    policy_order = np.argsort(np.argmax(policy, axis=1))

    np.testing.assert_allclose(
        batch["mcts_policy"][batch_order], policy[policy_order], atol=1e-5
    )
