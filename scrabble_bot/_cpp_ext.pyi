"""
Stub file for the _cpp_ext pybind11 extension module.
Provides type information for Pyright in basic mode.
"""

from __future__ import annotations
from typing import Optional, overload

import numpy as np

# ── Constants ─────────────────────────────────────────────────────────────────

DELIMETER: str  # Python-spelling alias (with typo, matching original code)
DELIMITER: str
BOARD_ROWS: int
BOARD_COLS: int
LETTER_SCORES: dict[Letter, int]
OBS_DIM: int
MAX_MOVES_PER_ENV: int

# ── Letter ────────────────────────────────────────────────────────────────────

class Letter:
    A: Letter
    B: Letter
    C: Letter
    D: Letter
    E: Letter
    F: Letter
    G: Letter
    H: Letter
    I: Letter
    J: Letter
    K: Letter
    L: Letter
    M: Letter
    N: Letter
    O: Letter
    P: Letter
    Q: Letter
    R: Letter
    S: Letter
    T: Letter
    U: Letter
    V: Letter
    W: Letter
    X: Letter
    Y: Letter
    Z: Letter
    BLANK: Letter
    def __str__(self) -> str: ...
    def __repr__(self) -> str: ...

A = Letter.A
B = Letter.B
C = Letter.C
D = Letter.D
E = Letter.E
F = Letter.F
G = Letter.G
H = Letter.H
I = Letter.I
J = Letter.J
K = Letter.K
L = Letter.L
M = Letter.M
N = Letter.N
O = Letter.O
P = Letter.P
Q = Letter.Q
R = Letter.R
S = Letter.S
T = Letter.T
U = Letter.U
V = Letter.V
W = Letter.W
X = Letter.X
Y = Letter.Y
Z = Letter.Z
BLANK = Letter.BLANK

# ── SquareType ────────────────────────────────────────────────────────────────

class SquareType:
    DEFAULT: SquareType
    DOUBLE_WORD: SquareType
    TRIPLE_WORD: SquareType
    DOUBLE_LETTER: SquareType
    TRIPLE_LETTER: SquareType
    def __str__(self) -> str: ...

DEFAULT = SquareType.DEFAULT
DOUBLE_WORD = SquareType.DOUBLE_WORD
TRIPLE_WORD = SquareType.TRIPLE_WORD
DOUBLE_LETTER = SquareType.DOUBLE_LETTER
TRIPLE_LETTER = SquareType.TRIPLE_LETTER

# ── Point ─────────────────────────────────────────────────────────────────────

class Point:
    row: int
    col: int
    def __init__(self, row: int, col: int) -> None: ...
    def __repr__(self) -> str: ...

# ── Move ──────────────────────────────────────────────────────────────────────

class Move:
    start: Point  # (row, col) of word start
    end: Point  # (row, col) of word end
    letters: list[Letter]  # actual letter at each position (A-Z, never BLANK)
    is_blank: list[bool]  # true if that position was played from a blank tile
    score: int
    def __init__(
        self,
        start: Point,
        end: Point,
        letters: list[Letter],
        is_blank: list[bool],
        score: int,
    ) -> None: ...

# ── GADDAG ────────────────────────────────────────────────────────────────────

class Arc:
    destination_state: State
    def __init__(
        self,
        letters_that_make_a_word: object = None,
        destination_state: Optional[State] = None,
    ) -> None: ...

class State:
    arcs: dict[str, Arc]
    letters_that_make_a_word: set[str]
    def __init__(self) -> None: ...
    def add_arc(
        self, letter: str, destination_state: Optional[State] = None
    ) -> State: ...
    def add_ending_arc(self, letter: str, ending_letter: str) -> State: ...
    def add_ending_letter(self, letter: str) -> None: ...
    def get_next_state(self, letter: str) -> Optional[State]: ...

class Gaddag:
    root: State
    def __init__(self) -> None: ...
    def build_from_words(self, words: list[str]) -> None: ...
    def build_from_file(self, wordlist_path: str, use_cache: bool = True) -> None: ...
    def add_word(self, word: str) -> None: ...
    def get_dictionary_letters(self) -> set[Letter]: ...

# ── Board ─────────────────────────────────────────────────────────────────────

class Board:
    @property
    def board(self) -> list[list[Letter]]: ...
    @property
    def square_types(self) -> dict[tuple[int, int], SquareType]: ...
    @property
    def _anchor_points(self) -> set[tuple[int, int]]: ...
    @property
    def _horizontal_cross_checks(self) -> list[list[set[Letter]]]: ...
    @property
    def _vertical_cross_checks(self) -> list[list[set[Letter]]]: ...
    def __init__(self, dictionary: Gaddag) -> None: ...
    @overload
    def place_word(
        self, word: list[Letter], start: tuple[int, int], end: tuple[int, int]
    ) -> int: ...
    @overload
    def place_word(self, move: Move) -> int: ...
    def calculate_score(
        self, word: list[Letter], starting_point: tuple[int, int], vertical: bool
    ) -> int: ...
    def validate_board(self) -> list[str]: ...
    def get_all_valid_moves(self, rack: list[Letter]) -> list[Move]: ...
    def __str__(self) -> str: ...

# ── Helpers ───────────────────────────────────────────────────────────────────

def get_letter_score(letter: Letter) -> int: ...
def get_all_letters() -> set[Letter]: ...
def letter_to_char(letter: Letter) -> str: ...
def char_to_letter(c: str) -> Letter: ...
def in_bounds(point: tuple[int, int]) -> bool: ...

# ── RL environment ─────────────────────────────────────────────────────────────

class StepResult:
    reward: float
    done: bool
    def __repr__(self) -> str: ...

class ScrabbleEnv:
    def __init__(self, dict: Gaddag, seed: int = 0) -> None: ...
    @overload
    def __init__(self, other: ScrabbleEnv) -> None: ...  # copy constructor
    def reset(self) -> None: ...
    def step(self, action_idx: int) -> StepResult: ...
    def get_valid_moves(self) -> list[Move]: ...
    @property
    def done(self) -> bool: ...
    @property
    def current_player(self) -> int: ...
    @property
    def consecutive_passes(self) -> int: ...
    @property
    def bag_remaining(self) -> int: ...
    @property
    def player_scores(self) -> tuple[int, int]: ...
    @property
    def rack(self) -> list[Letter]: ...

class VectorizedEnv:
    def __init__(self, dict: Gaddag, num_envs: int, seed: int = 0) -> None: ...
    def reset_all(self) -> None: ...
    def step_all(self) -> None: ...
    @property
    def num_envs(self) -> int: ...
    @property
    def observations(self) -> np.ndarray: ...   # [N, OBS_DIM] float32
    @property
    def actions(self) -> np.ndarray: ...        # [N] int32  (writable)
    @property
    def rewards(self) -> np.ndarray: ...        # [N] float32
    @property
    def dones(self) -> np.ndarray: ...          # [N] uint8
    @property
    def num_moves(self) -> np.ndarray: ...      # [N] int32
    @property
    def move_coords(self) -> np.ndarray: ...    # [N, MAX_MOVES_PER_ENV, 4] int8
    @property
    def move_letters(self) -> np.ndarray: ...   # [N, MAX_MOVES_PER_ENV, 7] uint8
    @property
    def move_is_blank(self) -> np.ndarray: ...  # [N, MAX_MOVES_PER_ENV, 7] uint8
    @property
    def move_scores(self) -> np.ndarray: ...    # [N, MAX_MOVES_PER_ENV] int16
