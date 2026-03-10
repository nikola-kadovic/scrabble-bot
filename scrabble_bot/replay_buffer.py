"""
AlphaZero-style replay buffer storing (obs, π, z) tuples.

  obs          — board observation at the time of the move
  mcts_policy  — normalised MCTS visit-count distribution π
  value_target — game outcome z filled in retrospectively at episode end
  n_moves      — number of valid moves (for policy loss masking)
"""

import numpy as np

from scrabble_bot._cpp_ext import MAX_MOVES_PER_ENV, OBS_DIM


class ReplayBuffer:
    """Ring buffer storing AlphaZero training transitions.

    Args:
        capacity:  Maximum number of transitions to store.
        max_moves: Width of the policy vector (default: MAX_MOVES_PER_ENV).
    """

    def __init__(self, capacity: int = 50_000, max_moves: int = MAX_MOVES_PER_ENV):
        self.capacity = capacity
        self.max_moves = max_moves

        self.obs = np.zeros((capacity, OBS_DIM), dtype=np.float32)
        self.mcts_policy = np.zeros((capacity, max_moves), dtype=np.float32)
        self.value_target = np.zeros(capacity, dtype=np.float32)
        self.n_moves = np.zeros(capacity, dtype=np.int32)

        self._size = 0
        self._ptr = 0

    # ------------------------------------------------------------------
    # Writing

    def push(
        self,
        obs: np.ndarray,
        mcts_policy: np.ndarray,
        value_target: float,
        n_moves: int,
    ) -> None:
        """Push a single transition into the buffer."""
        idx = self._ptr
        self.obs[idx] = obs
        policy_len = min(len(mcts_policy), self.max_moves)
        self.mcts_policy[idx, :policy_len] = mcts_policy[:policy_len]
        if policy_len < self.max_moves:
            self.mcts_policy[idx, policy_len:] = 0.0
        self.value_target[idx] = value_target
        self.n_moves[idx] = n_moves
        self._ptr = (self._ptr + 1) % self.capacity
        self._size = min(self._size + 1, self.capacity)

    def push_batch(
        self,
        obs: np.ndarray,
        mcts_policy: np.ndarray,
        value_target: np.ndarray,
        n_moves: np.ndarray,
    ) -> None:
        """Push a batch of transitions (vectorised).

        Args:
            obs:          [B, OBS_DIM] float32
            mcts_policy:  [B, max_moves] float32
            value_target: [B] float32
            n_moves:      [B] int32
        """
        B = len(obs)
        for i in range(B):
            self.push(obs[i], mcts_policy[i], float(value_target[i]), int(n_moves[i]))

    # ------------------------------------------------------------------
    # Reading

    def sample(self, batch_size: int) -> dict:
        """Uniformly sample a batch of transitions.

        Returns a dict with keys: obs, mcts_policy, value_target, n_moves.
        """
        if batch_size > self._size:
            raise ValueError(
                f"Requested {batch_size} samples but buffer only has {self._size}."
            )
        idx = np.random.choice(self._size, size=batch_size, replace=False)
        return dict(
            obs=self.obs[idx],
            mcts_policy=self.mcts_policy[idx],
            value_target=self.value_target[idx],
            n_moves=self.n_moves[idx],
        )

    def __len__(self) -> int:
        return self._size
