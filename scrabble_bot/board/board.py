from enum import Enum
from typing import TypeAlias

from scrabble_bot.gaddag.gaddag import DELIMETER, Gaddag
from scrabble_bot.board.letter import Letter, get_all_letters
from scrabble_bot.board.move import Move


Point: TypeAlias = tuple[int, int]


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
    - The location of special squares(e.g. triple word score, double word score, triple letter score, double letter score)
    """

    ROWS = 15
    COLS = 15

    def __init__(self, dictionary: Gaddag):
        self.board: list[list[Letter]] = [
            [Letter.BLANK for _ in range(self.ROWS)] for _ in range(self.COLS)]

        self._dictionary = dictionary

        self.square_types: dict[Point, SquareType] = {}
        self._build_square_types()

        self._first_move = True

        # Represents points where we can start a move. Since we need at least one
        # letter added, this is usually empty adjacent squares to words that have
        # already been placed.
        self._anchor_points: list[Point] = []

        # Represents the set of letters that can be placed horizontally and
        # vertically at a particular point, while making valid words on the adjacent axis.
        # If there's no letters on the adjacent axis, the cross check includes all letters.
        # Horizontal cross checks are for creating vertical words, and vice versa.
        self._horizontal_cross_checks: list[list[set[Letter]]] = [
            [get_all_letters() for _ in range(self.ROWS)] for _ in range(self.COLS)]
        self._vertical_cross_checks: list[list[set[Letter]]] = [
            [get_all_letters() for _ in range(self.ROWS)] for _ in range(self.COLS)]

    def _build_square_types(self):
        """
        Builds the square types for the board. Follows the standard special square distribution for Scrabble, see:
        https: // simple.wikipedia.org / wiki / Scrabble  # /media/File:Scrabble_board_-_English.svg
        """
        # Start by setting all squares to default
        for row in range(self.ROWS):
            for col in range(self.COLS):
                self.square_types[(row, col)] = SquareType.DEFAULT

        # Build triple letter squares:
        for i in range(1, self.ROWS, 4):
            for j in range(1, self.COLS, 4):
                self.square_types[(i, j)] = SquareType.TRIPLE_LETTER

        # Build double word squares (top and bottom diagonals):
        for i in range(1, 5):
            self.square_types[(i, i)] = SquareType.DOUBLE_WORD
            self.square_types[(i, self.COLS - i - 1)] = SquareType.DOUBLE_WORD
            self.square_types[(self.ROWS - i - 1, i)] = SquareType.DOUBLE_WORD
            self.square_types[(self.ROWS - i - 1, self.COLS - i - 1)] = SquareType.DOUBLE_WORD

        # Build triple word squares:
        for i in range(0, self.ROWS, 7):
            for j in range(0, self.COLS, 7):
                self.square_types[(i, j)] = SquareType.TRIPLE_WORD
        self.square_types[(7, 7)] = SquareType.DEFAULT

        # Build double letter squares:
        for (qx, qy) in [(1, 1), (1, -1), (-1, 1), (-1, -1)]:
            # use the symmetry from each quadrant to build the squares
            self.square_types[(
                (qx * 0 + (self.ROWS - 1 if qx == -1 else 0)),
                (qy * 3 + (self.COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 3 + (self.ROWS - 1 if qx == -1 else 0)),
                (qy * 0 + (self.COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 2 + (self.ROWS - 1 if qx == -1 else 0)),
                (qy * 6 + (self.COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 6 + (self.ROWS - 1 if qx == -1 else 0)),
                (qy * 2 + (self.COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 6 + (self.ROWS - 1 if qx == -1 else 0)),
                (qy * 6 + (self.COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 3 + (self.ROWS - 1 if qx == -1 else 0)),
                (qy * 7 + (self.COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 7 + (self.ROWS - 1 if qx == -1 else 0)),
                (qy * 3 + (self.COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER

    def print_square_types(self):
        """
        Prints the square types for the board with colors, along with a legend:
        - Default: .
        - Double Word: w(yellow)
        - Triple Word: W(red)
        - Double Letter: l(light blue)
        - Triple Letter: L(dark blue)
        """
        # ANSI color codes
        RED = '\033[91m'  # Triple word
        YELLOW = '\033[93m'  # Double word
        LIGHT_BLUE = '\033[96m'  # Double letter
        DARK_BLUE = '\033[94m'  # Triple letter
        RESET = '\033[0m'  # Reset color

        def get_color(square_type: SquareType) -> str:
            """Get the color code for a square type"""
            if square_type == SquareType.TRIPLE_WORD:
                return RED
            elif square_type == SquareType.DOUBLE_WORD:
                return YELLOW
            elif square_type == SquareType.DOUBLE_LETTER:
                return LIGHT_BLUE
            elif square_type == SquareType.TRIPLE_LETTER:
                return DARK_BLUE
            else:
                return RESET

        print("Square Types:")
        print("-------------")
        for row in range(self.ROWS):
            for col in range(self.COLS):
                square_type = self.square_types[(row, col)]
                color = get_color(square_type)
                print(f"{color}{square_type}{RESET}", end=" ")
            print()
        print("-------------")
        print("Legend:")
        print(f"{YELLOW}w{RESET}: Double Word")
        print(f"{RED}W{RESET}: Triple Word")
        print(f"{LIGHT_BLUE}l{RESET}: Double Letter")
        print(f"{DARK_BLUE}L{RESET}: Triple Letter")

    def get_valid_moves(self) -> list[Move]:
        raise NotImplementedError("Not implemented")

    def place_word(self, word: list[Letter], starting_point: Point, vertical: bool) -> bool:
        """
        Places a word on the board. May raise a ValueError if the move is invalid.
        """

        if not (0 <= starting_point[0] < self.ROWS and 0 <= starting_point[1] < self.COLS):
            raise ValueError(f"Starting point {starting_point} is out of bounds")

        if vertical:
            return self._place_word_vertically(word, starting_point)
        else:
            return self._place_word_horizontally(word, starting_point)

    def _place_word_horizontally(self, word: list[Letter], sp: Point) -> bool:
        if sp[1] + len(word) > self.COLS:
            raise ValueError(f"Word {word} is too long to fit at starting point {sp}")

        if self._first_move and (sp[0] != 7 or not (sp[1] <= 7 <= sp[1] + len(word))):
            raise ValueError(
                f"First move must be on the center row, between columns 7 and {sp[1] + len(word)}")

        for i, letter in enumerate(word):
            x, y = sp[0], sp[1] + i
            if self.board[x][y] != Letter.BLANK and self.board[x][y] != letter:
                raise ValueError(
                    f"Letter {letter} at position {x, y} is already occupied by {self.board[x][y]}")

            self.board[x][y] = letter
            # Update the vertical cross checks for the space we just filled
            self._update_vertical_cross_checks((x, y))

            if i == 0 or i == len(word) - 1:
                self._update_horizontal_cross_checks((x, y))

        if self._first_move:
            self._first_move = False

        return True

    def _place_word_vertically(self, word: list[Letter], sp: Point) -> bool:
        if sp[0] + len(word) > self.ROWS:
            raise ValueError(f"Word {word} is too long to fit at starting point {sp}")

        if self._first_move and (sp[1] != 7 or not (sp[0] <= 7 <= sp[0] + len(word))):
            raise ValueError(
                f"First move must be on the center column, between rows 7 and {sp[0] + len(word)}")

        for i, letter in enumerate(word):
            x, y = sp[0] + i, sp[1]
            if self.board[x][y] != Letter.BLANK and self.board[x][y] != letter:
                raise ValueError(
                    f"Letter {letter} at position {x, y} is already occupied by {self.board[x][y]}")

            self.board[x][y] = letter
            # Update the horizontal cross checks for the space we just filled
            self._update_horizontal_cross_checks((x, y))

            if i == 0 or i == len(word) - 1:
                self._update_vertical_cross_checks((x, y))

            # Update the cross checks for the space we just filled - trivially, only
            # that letter can be placed there.
            self._vertical_cross_checks[x][y] = set([letter])
            self._horizontal_cross_checks[x][y] = set([letter])

        if self._first_move:
            self._first_move = False

        return True

    def _update_horizontal_cross_checks(self, point: Point):
        """
        Updates the horizontal cross checks of a point that was just filled.

        The following examples help illustrate the concept - not that we only consider the left neighbor in the examples, but the same logic applies to the right neighbor.

        ### Example 1:
        ```
        . . . . .
        . . A . .
        . x B . .
        . . S . .
        . . . . .
        ```

        - If we just filled the letter B, we need to find all the letters x that make xB a valid word.

        ### Example 2:
        . . . . .
        . . A . .
        . x B A R
        . . S . .
        . . . . .

        - If we just filled the letter B, we need to find all the letters x that make xBAR a valid word.

        ### Edge case:

        . . . . . . .
        . . . . . . .
        P A x A B L E
        . . . . . . .
        . . . . . . .

        - If we just filled out PA, we need to find all the letters x that make PAxABLE a valid word.
        """

        x, y = point
        hy1, hy2 = y - 1, y + 1

        if 0 <= hy1 < self.COLS:
            for letter in self._horizontal_cross_checks[x][hy1]:
                # Test to see if the word is valid by querying the dictionary
                first_letter = self._dictionary.root.get_next_state(str(letter))

                if not first_letter:
                    raise RuntimeError("This should never happen")

                # Add delimiter to start going right
                current_state = first_letter.get_next_state(DELIMETER)
                if not current_state:
                    self._horizontal_cross_checks[x][hy1].remove(letter)
                    continue

                idx = y

                while current_state and idx < self.COLS and self.board[x][idx] == Letter.BLANK:
                    current_state = current_state.get_next_state(str(self.board[x][idx]))

                    if not current_state:
                        self._horizontal_cross_checks[x][idx].remove(letter)
                        break

                    idx += 1

                if current_state and letter not in current_state.letters_that_make_a_word:
                    self._horizontal_cross_checks[x][idx].remove(letter)
                    continue

        if 0 <= hy2 < self.COLS:
            for letter in self._horizontal_cross_checks[x][hy2]:
                # Test to see if the word is valid by querying the dictionary
                first_letter = self._dictionary.root.get_next_state(str(letter))

                if not first_letter:
                    raise RuntimeError("This should never happen")

                idx = y

                while current_state and idx >= 0 and self.board[x][idx] == Letter.BLANK:
                    current_state = current_state.get_next_state(str(self.board[x][idx]))

                    if not current_state:
                        self._horizontal_cross_checks[x][idx].remove(letter)
                        break

                    idx -= 1

                if current_state and letter not in current_state.letters_that_make_a_word:
                    self._horizontal_cross_checks[x][idx].remove(letter)
                    continue

    def _update_vertical_cross_checks(self, point: Point):
        pass

    def __str__(self):
        return "\n".join(
            [" ".join([cell.value for cell in row]) for row in self.board])
