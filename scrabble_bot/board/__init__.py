"""
Backward-compatibility re-exports for scrabble_bot.board.*
"""
from scrabble_bot._cpp_ext import (  # noqa: F401
    Letter,
    LETTER_SCORES,
    get_letter_score,
    get_all_letters,
    Move,
    Board,
    SquareType,
    BOARD_ROWS,
    BOARD_COLS,
    in_bounds,
)
