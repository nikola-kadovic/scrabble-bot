# Scrabble Bot

A Scrabble-playing AI built on a fast C++ game engine and an AlphaZero-style reinforcement learning pipeline.

## Overview

The design follows a clean two-layer split:

- **C++ core** — all game logic: dictionary, board, move generation, environments, and MCTS tree search. Compiled to a Python extension module (`_cpp_ext`) via pybind11.
- **Python RL layer** — self-play orchestration, replay buffer, and the training loop. The neural network inference function is a caller-supplied callable, keeping the training code framework-agnostic.

This separation keeps the hot path (move generation, MCTS simulations) in native code while leaving training logic flexible.

```
┌─────────────────────────────────────────────────────────────────┐
│  Python RL/Training Layer  (scrabble_bot/)                      │
│  MCTSAgent · SelfPlayTrainer · ReplayBuffer · ScrabbleVecEnv    │
└──────────────────────────────┬──────────────────────────────────┘
                               │  pybind11
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│  C++ Core  (_cpp_ext)                                           │
│  Gaddag · Board · MoveGen · ScrabbleEnv · VectorizedEnv         │
│  MCTSTree · VectorizedMCTS                                      │
└─────────────────────────────────────────────────────────────────┘
```

---

## Game Engine (C++ Core)

### GADDAG

The dictionary is stored as a [GADDAG](https://en.wikipedia.org/wiki/GADDAG) — a directed acyclic word graph that encodes every word in both the forward and reverse directions, separated by a delimiter character (`◇`, U+25C7). This structure makes it possible to extend a partial word in *either* direction during move generation, which is essential for placing tiles next to letters already on the board.

**Implementation:** `cpp/include/scrabble/gaddag.hpp` · `cpp/src/gaddag.cpp`

- State nodes store 28 arcs (A–Z, BLANK, DELIMITER)
- Each state carries a `letters_that_make_a_word` bitmask (uint32_t, bits 0–25 = A–Z) to detect valid word endpoints without a separate traversal
- Arena-allocated: states live in a `vector<unique_ptr<State>>`; all internal traversal uses raw pointers (zero atomic refcount ops on the hot path)
- Binary cache at `temp/gaddag_<wordlist-stem>.bin` for fast startup (building from the NWL word list takes a few seconds; loading the cache is instant)

Word list used: NWL 2023 (`dictionary/nwl_2023.txt`).

See: [Gordon 1994 — A faster Scrabble move generation algorithm](https://users.cs.northwestern.edu/~robby/uc-courses/22001-2008-winter/faster-scrabble-gordon.pdf)

---

### Board

**Implementation:** `cpp/include/scrabble/board.hpp` · `cpp/src/board.cpp`

A 15×15 board with coordinates `(row, col)` and center at `(7, 7)`.

The board tracks two structures that drive move generation:

- **Anchor points** — empty squares adjacent to at least one placed tile. The move generator is seeded from anchor points; generating from every square would be wasteful. The center square `(7, 7)` is the initial anchor for the first move.
- **Cross-checks** — per-square bitmasks of letters that form valid words in the *perpendicular* direction. `horizontal_cross_checks[r][c]` lists letters valid for a horizontal placement (consulted during vertical move generation, and vice versa). These are recomputed after each `place_word()` call.

Square bonus types: `DOUBLE_LETTER`, `TRIPLE_LETTER`, `DOUBLE_WORD`, `TRIPLE_WORD`, `DEFAULT`.

---

### Move Generation

**Implementation:** `cpp/src/move_gen.cpp`

`Board::get_all_valid_moves(rack)` returns every legal move given a 7-tile rack. The algorithm is a backtracking search over the GADDAG, seeded from anchor points, in both horizontal and vertical orientations.

For each anchor:
1. **Case 1** — tiles already exist immediately to the left/above: consume them backward through the GADDAG, cross the DELIMITER, then extend right/below using rack tiles while consulting cross-checks.
2. **Case 2** — free space to the left/above: explore all left-extensions up to the nearest anchor or board edge, cross the DELIMITER at each valid prefix, then extend right.

Blanks are tried as each of the 26 letters. `calculate_score()` counts only newly placed tiles (pre-existing tiles contribute 0 to the turn score).

The outer anchor loop is parallelized with OpenMP. An `omp_in_parallel()` guard prevents nested parallelism when called from within a vectorized environment step.

See also (future parallel move-gen): [Daudjee & Keller 2006](https://cs.uwaterloo.ca/~kdaudjee/DaudjeeJPDC.pdf)

---

### ScrabbleEnv

**Implementation:** `cpp/include/scrabble/env.hpp` · `cpp/src/env.cpp`

A complete, copyable game state: board, both players' racks, scores, tile bag, and turn counter. Value semantics are intentional — each MCTS node holds a full copy of the game state at that point in the search tree, making tree search straightforward without snapshot/restore logic.

Interface: `reset()`, `step(action_idx)`, `get_valid_moves(...)`, `fill_observation(float*)`, `done`, `current_player`, `player_scores`.

---

### VectorizedEnv

**Implementation:** `cpp/include/scrabble/vec_env.hpp` · `cpp/src/vec_env.cpp`

Runs N independent game environments in parallel and exposes zero-copy numpy buffers for use with GPU inference. All buffer writes go directly into pre-allocated arrays; no Python-side copies needed.

Key constants (also exported to Python):
- `OBS_DIM = 6106` — observation vector size per environment
- `MAX_MOVES_PER_ENV = 4096` — maximum valid moves per position

**Observation layout** (6106 floats):
- `[0..6074]` — 15×15×27 one-hot board encoding (indices 0–25 = A–Z, 26 = empty)
- `[6075..6101]` — current player's rack (27 floats, normalized by 1/7)
- `[6102..6105]` — metadata: `score0/500`, `score1/500`, `current_player`, `passes/6`

**Shared buffers:** `obs_buf_`, `action_buf_`, `reward_buf_`, `done_buf_`, `move_coords_`, `move_letters_`, `move_is_blank_`, `move_scores_`, `num_moves_buf_`. The pass move is always the last entry in the valid-moves list (sentinel coords `(-1, -1, -1, -1)`, score 0).

---

## AlphaZero Training Pipeline (Python)

### MCTS

**C++ implementation:** `cpp/include/scrabble/mcts.hpp` · `cpp/src/mcts.cpp`  
**Vectorized:** `cpp/include/scrabble/vec_mcts.hpp` · `cpp/src/vec_mcts.cpp`

Each `MCTSNode` holds a copy of `ScrabbleEnv`, a visit count, accumulated value, and a prior probability from the neural network. Node selection uses the PUCT formula:

```
UCB(s, a) = Q(s, a) + c_puct · P(s, a) · √N(parent) / (1 + N(s, a))
```

Default constants: `c_puct = 1.5`, `dirichlet_alpha = 0.03`, `dirichlet_epsilon = 0.25`, `num_sims = 50`, `VALUE_NORM_SCALE = 200`.

**Value perspective:** The NN value head outputs a value in `[-1, +1]` from the current player's perspective. During backup, the value is negated at each level (Scrabble alternates players). Terminal values use `tanh((score_current - score_opponent) / 200)`, preserving score-differential signal rather than hard ±1.

**VectorizedMCTS** holds N `MCTSTree` instances and batches leaf selection across all trees into a single `[K, OBS_DIM]` inference call per simulation step. This is the mechanism that allows GPU batch inference over many parallel games.

---

### MCTSAgent

**Implementation:** `scrabble_bot/mcts.py`

Runs self-play episodes and produces training transitions `(obs, mcts_policy, z)`:

- **`run_episode(inference_fn)`** — one game, sequential MCTS per move
- **`run_episodes_vectorized(inference_fn, num_episodes)`** — N games in parallel, with a single NN call per simulation step across all active games. Games that finish are replaced until `num_episodes` complete.

The temperature schedule uses `temperature=1.0` for the first 15 plies (sample proportional to visit counts) and `temperature→0` after (argmax), consistent with AlphaZero.

See: [Batched inference for deep RL](https://arxiv.org/abs/1910.06591)

---

### ReplayBuffer

**Implementation:** `scrabble_bot/replay_buffer.py`

A fixed-capacity ring buffer storing AlphaZero-style training transitions:

| Field | Shape | Description |
|---|---|---|
| `obs` | `[capacity, OBS_DIM]` | Board observations |
| `mcts_policy` | `[capacity, MAX_MOVES_PER_ENV]` | MCTS visit-count distribution (normalized) |
| `value_target` | `[capacity]` | Game outcome from player's perspective |
| `n_moves` | `[capacity]` | Valid move count (for loss masking) |

Interface: `push(obs, policy, z, n_moves)`, `push_batch(...)`, `sample(batch_size)`.

---

### SelfPlayTrainer

**Implementation:** `scrabble_bot/train.py`

Orchestrates `MCTSAgent` + `ReplayBuffer`. `generate_data(num_episodes)` runs episodes and pushes transitions; `train_step(batch_size)` samples a batch. The caller is responsible for NN loss computation and weight updates — the trainer is framework-agnostic.

---

## Project Status

| Component | Status |
|---|---|
| GADDAG construction + binary caching | Done |
| Board (anchors, cross-checks, scoring) | Done |
| Move generation (backtracking, blanks, OpenMP) | Done |
| ScrabbleEnv (single game, pybind11 bindings) | Done |
| VectorizedEnv (N parallel envs, zero-copy buffers) | Done |
| MCTSTree + VectorizedMCTS | In Progress |
| MCTSAgent + SelfPlayTrainer | In Progress |
| ReplayBuffer | In Progress |
| Neural network (policy + value heads) | Not yet |
| Full training run | Not yet |

---

## Directory Layout

```
cpp/
  include/scrabble/
    gaddag.hpp          GADDAG trie + caching
    board.hpp           15×15 board, anchors, cross-checks
    letter.hpp          Letter enum, scores, cross-check helpers
    move.hpp            Move struct (start, end, letters, score)
    point.hpp           Point struct (row, col)
    env.hpp             ScrabbleEnv — single copyable game state
    vec_env.hpp         VectorizedEnv — N parallel games + numpy buffers
    mcts.hpp            MCTSNode + MCTSTree
    vec_mcts.hpp        VectorizedMCTS — batched leaf selection
  src/
    gaddag.cpp
    board.cpp
    move_gen.cpp        Backtracking move generation
    env.cpp
    vec_env.cpp         OpenMP-parallelized step_all()
    mcts.cpp
    vec_mcts.cpp
  bindings/
    bindings.cpp        pybind11 module (_cpp_ext)
  tests/
    test_mcts.cpp       Catch2 unit tests

scrabble_bot/
  __init__.py           Re-exports from _cpp_ext
  mcts.py               MCTSAgent (single + vectorized episodes)
  train.py              SelfPlayTrainer
  vec_env.py            ScrabbleVecEnv wrapper + TestingValuationFunction
  replay_buffer.py      AlphaZero replay buffer
  _cpp_ext.pyi          Pyright type stubs for C++ extension
  tests/                Python integration tests (pytest)

docs/
  mcts_spec.md          MCTS architecture specification

dictionary/
  nwl_2023.txt          NWL 2023 word list (not in repo)

temp/                   Runtime cache — gitignored
  gaddag_nwl_2023.bin   Binary GADDAG cache
```

---

## Build & Test

```bash
# Install Python deps + compile C++ extension
uv sync

# Force rebuild after editing .cpp files
uv sync --reinstall-package scrabble-bot

# Manual CMake iteration (faster for C++ changes)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# C++ unit tests (Catch2)
ctest --test-dir build --output-on-failure -C Release

# Python integration tests
uv run pytest scrabble_bot/tests/ -v

# C++ formatting / linting
make format        # format all C++ files in place
make format_check  # check formatting
make tidy          # run clang-tidy
```

**Note on Python versions:** `uv run python` uses Python 3.10 from `.venv`. Running `cmake --build` directly targets the system Python — always use `uv sync --reinstall-package scrabble-bot` to rebuild for the correct interpreter.

---

## References

- [Project notes (Google Doc)](https://docs.google.com/document/d/16_-B3DHRCVCkZi3WHuQVkJozqjU1rVUT7LSJb320K1w/edit)
- [Gordon 1994 — A faster Scrabble move generation algorithm](https://users.cs.northwestern.edu/~robby/uc-courses/22001-2008-winter/faster-scrabble-gordon.pdf) — the GADDAG data structure and backtracking move generation algorithm implemented here
- [Daudjee & Keller 2006 — Parallel Scrabble move generation](https://cs.uwaterloo.ca/~kdaudjee/DaudjeeJPDC.pdf) — future direction for parallelized move generation
- [Lerer et al. 2019 — Improving policies via search in cooperative partially observable games](https://arxiv.org/abs/1910.06591) — batched inference for deep RL
- [Stanford AA228 2019 — GADDAG + Q-Learning for Scrabble](https://web.stanford.edu/class/aa228/reports/2019/final41.pdf) — prior work demonstrating GADDAG effectiveness and Q-learning feasibility for Scrabble valuation
