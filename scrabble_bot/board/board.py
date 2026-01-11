from enum import Enum
from typing import TypeAlias

from scrabble_bot.gaddag.gaddag import DELIMETER, Gaddag, State
from scrabble_bot.board.letter import Letter, get_all_letters
from scrabble_bot.board.move import Move

# The nubmer of rows and columns on the board
BOARD_ROWS = 15
BOARD_COLS = 15

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

    def __init__(self, dictionary: Gaddag):
        self.board: list[list[Letter]] = [
            [Letter.BLANK for _ in range(BOARD_ROWS)] for _ in range(BOARD_COLS)]

        self._dict = dictionary

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
            [self._dict.starting_letters() for _ in range(BOARD_ROWS)] for _ in range(BOARD_COLS)]
        self._vertical_cross_checks: list[list[set[Letter]]] = [
            [self._dict.starting_letters() for _ in range(BOARD_ROWS)] for _ in range(BOARD_COLS)]

    def _build_square_types(self):
        """
        Builds the square types for the board. Follows the standard special square distribution for Scrabble, see:
        https: // simple.wikipedia.org / wiki / Scrabble  # /media/File:Scrabble_board_-_English.svg
        """
        # Start by setting all squares to default
        for row in range(BOARD_ROWS):
            for col in range(BOARD_COLS):
                self.square_types[(row, col)] = SquareType.DEFAULT

        # Build triple letter squares:
        for i in range(1, BOARD_ROWS, 4):
            for j in range(1, BOARD_COLS, 4):
                self.square_types[(i, j)] = SquareType.TRIPLE_LETTER

        # Build double word squares (top and bottom diagonals):
        for i in range(1, 5):
            self.square_types[(i, i)] = SquareType.DOUBLE_WORD
            self.square_types[(i, BOARD_COLS - i - 1)] = SquareType.DOUBLE_WORD
            self.square_types[(BOARD_ROWS - i - 1, i)] = SquareType.DOUBLE_WORD
            self.square_types[(BOARD_ROWS - i - 1, BOARD_COLS - i - 1)] = SquareType.DOUBLE_WORD

        # Build triple word squares:
        for i in range(0, BOARD_ROWS, 7):
            for j in range(0, BOARD_COLS, 7):
                self.square_types[(i, j)] = SquareType.TRIPLE_WORD
        self.square_types[(7, 7)] = SquareType.DEFAULT

        # Build double letter squares:
        for (qx, qy) in [(1, 1), (1, -1), (-1, 1), (-1, -1)]:
            # use the symmetry from each quadrant to build the squares
            self.square_types[(
                (qx * 0 + (BOARD_ROWS - 1 if qx == -1 else 0)),
                (qy * 3 + (BOARD_COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 3 + (BOARD_ROWS - 1 if qx == -1 else 0)),
                (qy * 0 + (BOARD_COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 2 + (BOARD_ROWS - 1 if qx == -1 else 0)),
                (qy * 6 + (BOARD_COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 6 + (BOARD_ROWS - 1 if qx == -1 else 0)),
                (qy * 2 + (BOARD_COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 6 + (BOARD_ROWS - 1 if qx == -1 else 0)),
                (qy * 6 + (BOARD_COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 3 + (BOARD_ROWS - 1 if qx == -1 else 0)),
                (qy * 7 + (BOARD_COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER
            self.square_types[(
                (qx * 7 + (BOARD_ROWS - 1 if qx == -1 else 0)),
                (qy * 3 + (BOARD_COLS - 1 if qy == -1 else 0)))] = SquareType.DOUBLE_LETTER

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
        for row in range(BOARD_ROWS):
            for col in range(BOARD_COLS):
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

        x, y = starting_point

        if not (0 <= x < BOARD_ROWS and 0 <= y < BOARD_COLS):
            raise ValueError(f"Starting point {starting_point} is out of bounds")

        if vertical:
            return self._place_word_vertically(word, starting_point)
        else:
            return self._place_word_horizontally(word, starting_point)

    def _place_word_horizontally(self, word: list[Letter], sp: Point) -> bool:
        if sp[1] + len(word) > BOARD_COLS:
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

            # Update cross checks for the space we just filled
            if in_bounds((x - 1, y)):
                self._update_vertical_cross_checks((x - 1, y))

            if in_bounds((x + 1, y)):
                self._update_vertical_cross_checks((x + 1, y))

            if i == 0 and in_bounds((x, y - 1)):
                self._update_horizontal_cross_checks((x, y - 1))

            if i == len(word) - 1 and in_bounds((x, y + 1)):
                self._update_horizontal_cross_checks((x, y + 1))

        if self._first_move:
            self._first_move = False

        return True

    def _place_word_vertically(self, word: list[Letter], sp: Point) -> bool:
        if sp[0] + len(word) > BOARD_ROWS:
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

            # Update cross checks for the space we just filled
            if in_bounds((x, y - 1)):
                self._update_horizontal_cross_checks((x, y - 1))

            if in_bounds((x, y + 1)):
                self._update_horizontal_cross_checks((x, y + 1))

            if i == 0 and in_bounds((x - 1, y)):
                self._update_vertical_cross_checks((x - 1, y))

            if i == len(word) - 1 and in_bounds((x + 1, y)):
                self._update_vertical_cross_checks((x + 1, y))

            if self._first_move:
                self._first_move = False

        return True

    def _update_horizontal_cross_checks(self, point: Point):
        """
        Updates the horizontal cross checks for a given point.
        A cross check is a set of letters that can be placed at a given point, while making valid words on the adjacent axis.
        If there's no letters on the adjacent axis, the cross check includes all letters.
        Horizontal cross checks are for creating vertical words, and vice versa.
        """
        x, y = point
        to_remove = []

        for letter in self._horizontal_cross_checks[x][y]:
            if not self._letter_makes_a_word_horizontally(point, letter):
                to_remove.append(letter)

        # Remove the letters that are not valid from the cross checks
        for letter in to_remove:
            self._horizontal_cross_checks[x][y].remove(letter)

    def _letter_makes_a_word_horizontally(self, point: Point, letter: Letter) -> bool:
        """
        Checks if a letter makes a word horizontally at a given point.
        """
        x, y = point
        letters_to_left = True if y > 0 and self.board[x][y - 1] != Letter.BLANK else False
        letters_to_right = True if y < BOARD_ROWS - \
            1 and self.board[x][y + 1] != Letter.BLANK else False

        state = self._dict.root.get_next_state(str(letter))

        if not state:
            raise RuntimeError("This should never happen")

        if letters_to_left:
            for idx in range(y - 1, -1, -1):
                if not state:
                    return False
                if idx == 0 or self.board[x][idx - 1] == Letter.BLANK:
                    break
                state = state.get_next_state(str(self.board[x][idx]))

            if not state:
                return False

        if letters_to_right:
            state = state.get_next_state(DELIMETER)

            for idx in range(y + 1, BOARD_COLS):
                if not state:
                    return False
                if idx == BOARD_COLS - 1 or self.board[x][idx + 1] == Letter.BLANK:
                    break
                state = state.get_next_state(str(self.board[x][idx]))

        return state is not None and (str(self.board[x][idx]) in state.letters_that_make_a_word)

    def _update_vertical_cross_checks(self, point: Point):
        """
        Updates the vertical cross checks for a given point.
        A cross check is a set of letters that can be placed at a given point, while making valid words on the adjacent axis.
        If there's no letters on the adjacent axis, the cross check includes all letters.
        Vertical cross checks are for creating horizontal words, and vice versa.
        """
        x, y = point
        to_remove = []

        for letter in self._vertical_cross_checks[x][y]:
            if not self._letter_makes_a_word_vertically(point, letter):
                to_remove.append(letter)

        # Remove the letters that are not valid from the cross checks
        for letter in to_remove:
            self._vertical_cross_checks[x][y].remove(letter)

    def _letter_makes_a_word_vertically(self, point: Point, letter: Letter) -> bool:
        """
        Checks if a letter makes a word vertically at a given point.
        """
        x, y = point
        letters_up = True if x > 0 and self.board[x - 1][y] != Letter.BLANK else False
        letters_down = True if x < BOARD_ROWS - \
            1 and self.board[x + 1][y] != Letter.BLANK else False

        state = self._dict.root.get_next_state(str(letter))

        if not state:
            raise RuntimeError("This should never happen")

        if letters_up:
            for idx in range(x - 1, -1, -1):
                if not state:
                    return False
                if idx == 0 or self.board[idx - 1][y] == Letter.BLANK:
                    break
                state = state.get_next_state(str(self.board[idx][y]))

            if not state:
                return False

        if letters_down:
            state = state.get_next_state(DELIMETER)

            for idx in range(x + 1, BOARD_ROWS):
                if not state:
                    return False
                if idx == BOARD_ROWS - 1 or self.board[idx + 1][y] == Letter.BLANK:
                    break
                state = state.get_next_state(str(self.board[idx][y]))

        return state is not None and (str(self.board[idx][y]) in state.letters_that_make_a_word)

    def __str__(self):
        return "\n".join(
            [" ".join([cell.value for cell in row]) for row in self.board])


def in_bounds(point: Point) -> bool:
    x, y = point
    return 0 <= x < BOARD_ROWS and 0 <= y < BOARD_COLS
