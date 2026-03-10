"""
Vectorized Scrabble environment wrapper with inference bridge.

The C++ VectorizedEnv holds all game state and exposes zero-copy numpy
buffers. This module provides:
  - ScrabbleVecEnv: thin wrapper that bridges inference functions to C++ step.
  - TestingValuationFunction: heuristic placeholder until a real NN is trained.
  - masked_sample: sample from logits with per-env valid-move masking.
"""

import numpy as np

from scrabble_bot._cpp_ext import MAX_MOVES_PER_ENV, OBS_DIM, VectorizedEnv


def masked_sample(logits: np.ndarray, n: np.ndarray) -> np.ndarray:
    """Sample an action index for each env, masking indices >= n[i] as invalid.

    Args:
        logits: [N, MAX_MOVES] float32 — raw policy logits.
        n:      [N] int32 — number of valid moves per env.

    Returns:
        actions: [N] int32 — sampled action index in [0, n[i]-1].
    """
    N = logits.shape[0]
    max_moves = logits.shape[1]
    idx = np.arange(max_moves)
    mask = idx[None, :] < n[:, None]  # [N, MAX_MOVES] bool

    # Replace invalid logits with -inf, then softmax
    masked = np.where(mask, logits.astype(np.float64), -1e38)
    masked -= np.max(masked, axis=1, keepdims=True)
    probs = np.exp(masked)
    probs_sum = probs.sum(axis=1, keepdims=True)
    probs = probs / np.where(probs_sum > 0, probs_sum, 1.0)

    actions = np.empty(N, dtype=np.int32)
    for i in range(N):
        valid_n = int(n[i])
        actions[i] = np.random.choice(valid_n, p=probs[i, :valid_n] / probs[i, :valid_n].sum())
    return actions


class TestingValuationFunction:
    """Simple linear heuristic for testing the full pipeline.

    Policy: raw move scores as logits (higher Scrabble score = preferred).
    Value:  score differential from current player's perspective, extracted
            from the obs metadata block.

    No learned parameters. Replace with a neural network for real training.
    """

    def __call__(
        self,
        obs: np.ndarray,      # [N, OBS_DIM] float32
        coords: np.ndarray,   # [N, MAX_MOVES, 4] int8   — ignored here
        letters: np.ndarray,  # [N, MAX_MOVES, 7] uint8  — ignored here
        scores: np.ndarray,   # [N, MAX_MOVES] int16
        n: np.ndarray,        # [N] int32
    ) -> tuple[np.ndarray, np.ndarray]:
        # Policy logits: raw move scores (higher is better)
        policy_logits = scores.astype(np.float32)  # [N, MAX_MOVES]

        # Value: score differential from current player's perspective.
        # Obs metadata layout at offset 6102: [score0/500, score1/500, player, passes/6]
        meta = obs[:, 6102:6106]       # [N, 4]
        score0 = meta[:, 0]            # player 0 cumulative score (normalised)
        score1 = meta[:, 1]            # player 1 cumulative score (normalised)
        current_player = meta[:, 2]    # 0.0 or 1.0

        value = np.where(current_player == 0, score0 - score1, score1 - score0)  # [N]
        return policy_logits, value.astype(np.float32)


class ScrabbleVecEnv:
    """High-level wrapper around C++ VectorizedEnv with inference bridge.

    Usage::

        gaddag = Gaddag()
        gaddag.build_from_file("dictionary/nwl_2023.txt")
        env = ScrabbleVecEnv(gaddag, num_envs=16)
        valuation = TestingValuationFunction()

        for _ in range(1000):
            result = env.step_with_inference(valuation)
            # result: dict with obs, actions, rewards, dones, values
    """

    def __init__(self, gaddag, num_envs: int, seed: int = 42):
        self.vec_env = VectorizedEnv(gaddag, num_envs, seed)
        self.vec_env.reset_all()

    def step_with_inference(self, inference_fn) -> dict:
        """Run one environment step driven by the given inference function.

        The inference function receives zero-copy views into C++ memory and must
        return (policy_logits [N, MAX_MOVES], values [N]).

        The obs buffer is copied *before* step_all() overwrites it, so the
        returned ``obs`` reflects the state at which the action was taken.
        """
        obs = self.vec_env.observations    # [N, OBS_DIM] float32 — zero-copy view
        letters = self.vec_env.move_letters  # [N, MAX_MOVES, 7] uint8
        coords = self.vec_env.move_coords    # [N, MAX_MOVES, 4] int8
        scores = self.vec_env.move_scores    # [N, MAX_MOVES] int16
        n = self.vec_env.num_moves           # [N] int32

        policy_logits, values = inference_fn(obs, coords, letters, scores, n)
        actions = masked_sample(policy_logits, n)

        # Must copy obs before step_all() overwrites the buffer
        obs_copy = obs.copy()
        np.copyto(self.vec_env.actions, actions)

        self.vec_env.step_all()  # GIL released inside C++, OpenMP runs

        return dict(
            obs=obs_copy,
            actions=actions,
            rewards=self.vec_env.rewards.copy(),
            dones=self.vec_env.dones.copy(),
            values=values,
        )

    @property
    def num_envs(self) -> int:
        return self.vec_env.num_envs
