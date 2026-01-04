from dataclasses import dataclass
from gaddag import Gaddag
from enum import Enum


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


class SquareType(Enum):
    """
    Enum for the type of square on the board.
    """

    DEFAULT = "."
    DOUBLE_WORD = "w"
    TRIPLE_WORD = "W"
    DOUBLE_LETTER = "l"
    TRIPLE_LETTER = "L"

    def __str__(self):
        return self.value


class Board:
    """
    The board class contains:
    - board state
    - common functions for interacting with the board.
    - The location of special squares (e.g. triple word score, double word score, triple letter score, double letter score)
    """

    def __init__(self, rows: int, cols: int):
        self.rows = rows
        self.cols = cols
        self.board = [[None for _ in range(rows)] for _ in range(cols)]
        self.square_types: dict[tuple[int, int], SquareType] = {}
        self.build_square_types()

    def build_square_types(self):
        """
        Builds the square types for the board. We matched the square type distribution in the original game of Scrabble, see:
        https://simple.wikipedia.org/wiki/Scrabble#/media/File:Scrabble_board_-_English.svg
        """

        # Build double word squares (top and bottom diagonals):
        for i in range(1, 5):
            self.square_types[(i, i)] = SquareType.DOUBLE_WORD
            self.square_types[(i, self.cols - i - 1)] = SquareType.DOUBLE_WORD
            self.square_types[(self.rows - i - 1, i)] = SquareType.DOUBLE_WORD
            self.square_types[(self.rows - i - 1, self.cols - i - 1)] = SquareType.DOUBLE_WORD

        # Build triple letter squares:
        for i in range(1, self.rows, 4):
            for j in range(1, self.cols, 4):
                self.square_types[(i, j)] = SquareType.TRIPLE_LETTER

        # Build triple word squares:
        for i in range(0, self.rows, 7):
            for j in range(0, self.cols, 7):
                self.square_types[(i, j)] = SquareType.TRIPLE_WORD
        self.square_types[(7, 7)] = SquareType.DEFAULT

        # Build double letter squares:
        for (qx, qy) in [(1, 1), (1, -1), (-1, 1), (-1, -1)]:
            # use the symmetry from each quadrant to build the squares
            self.square_types[(
                (qx * 0 + self.rows if qx == -1 else 0),
                (qy * 3 + self.cols if qy == -1 else 0))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 3 + self.rows if qx == -1 else 0),
                (qy * 0 + self.cols if qy == -1 else 0))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 2 + self.rows if qx == -1 else 0),
                (qy * 6 + self.cols if qy == -1 else 0))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 6 + self.rows if qx == -1 else 0),
                (qy * 2 + self.cols if qy == -1 else 0))] = SquareType.DOUBLE_LETTER

    def print_square_types(self):
        """
        Prints the square types for the board, along with a legend:
        - Default: .
        - Double Word: w
        - Triple Word: W
        - Double Letter: l
        - Triple Letter: L
        """

        print("Square Types:")
        print("-------------")
        for row in range(self.rows):
            for col in range(self.cols):
                print(self.square_types[(row, col)], end=" ")
            print()
        print("-------------")
        print("Legend:")
        print("w: Double Word")
        print("W: Triple Word")
        print("l: Double Letter")
        print("L: Triple Letter")

    def get_valid_moves(self, gaddag: Gaddag) -> list[Move]:
        raise NotImplementedError("Not implemented")

    def __str__(self):
        return "\n".join(
            [" ".join([cell.letter if cell is not None else "." for cell in row]) for row in self.board])
