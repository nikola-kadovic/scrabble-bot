
from dataclasses import dataclass


@dataclass
class Move:
    """
    A move contains:
    - The starting index
    - The ending index
    - The letters placed on the board, in order
    - The final score of the move
    """

    def __init__(self, starting_index: int, ending_index: int, letters: list[str], score: int):
        self.starting_index = starting_index
        self.ending_index = ending_index
        self.letters = letters
        self.score = score

