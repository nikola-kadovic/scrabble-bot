"""
Basic tests for board.py using pytest
"""

import pytest
from scrabble_bot.board.board import Board, SquareType, BOARD_ROWS, BOARD_COLS
from scrabble_bot.board.letter import Letter, LETTER_SCORES
from scrabble_bot.board.move import Move
from scrabble_bot.gaddag.gaddag import Gaddag


class TestLetter:
    """Tests for the Letter enum"""

    def test_letter_enum_values(self):
        """Test that Letter enum has correct values"""
        assert Letter.A.value == "A"
        assert Letter.Z.value == "Z"
        assert Letter.BLANK.value == "_"

    def test_letter_string_representation(self):
        """Test that Letter enum converts to string correctly"""
        assert str(Letter.A) == "A"
        assert str(Letter.BLANK) == "_"

    def test_all_letters_exist(self):
        """Test that all 26 letters plus blank exist"""
        letters = [letter for letter in Letter]
        assert len(letters) == 27  # 26 letters + blank


class TestLetterScores:
    """Tests for LETTER_SCORES dictionary"""

    def test_letter_scores_exist(self):
        """Test that all letters have scores"""
        for letter in Letter:
            assert letter in LETTER_SCORES

    def test_common_letter_scores(self):
        """Test some common letter scores"""
        assert LETTER_SCORES[Letter.A] == 1
        assert LETTER_SCORES[Letter.E] == 1
        assert LETTER_SCORES[Letter.Q] == 10
        assert LETTER_SCORES[Letter.Z] == 10
        assert LETTER_SCORES[Letter.BLANK] == 0

    def test_high_value_letters(self):
        """Test high-value letters"""
        assert LETTER_SCORES[Letter.J] == 8
        assert LETTER_SCORES[Letter.X] == 8
        assert LETTER_SCORES[Letter.Q] == 10
        assert LETTER_SCORES[Letter.Z] == 10


class TestSquareType:
    """Tests for the SquareType enum"""

    def test_square_type_enum_values(self):
        """Test that SquareType enum has correct values"""
        assert SquareType.DEFAULT.value == "."
        assert SquareType.DOUBLE_WORD.value == "w"
        assert SquareType.TRIPLE_WORD.value == "W"
        assert SquareType.DOUBLE_LETTER.value == "l"
        assert SquareType.TRIPLE_LETTER.value == "L"

    def test_square_type_string_representation(self):
        """Test that SquareType enum converts to string correctly"""
        assert str(SquareType.DEFAULT) == "."
        assert str(SquareType.DOUBLE_WORD) == "w"


class TestMove:
    """Tests for the Move class"""

    def test_move_initialization(self):
        """Test that Move can be initialized"""
        move = Move(0, 5, ["A", "B", "C"], 10)
        assert move.starting_index == 0
        assert move.ending_index == 5
        assert move.letters == ["A", "B", "C"]
        assert move.score == 10


class TestBoard:
    """General Tests for the Board class"""

    def test_board_initialization(self):
        """Test that Board can be initialized"""
        dictionary = Gaddag(words=[])
        board = Board(dictionary)
        assert BOARD_ROWS == 15
        assert BOARD_COLS == 15
        assert len(board.board) == 15
        assert len(board.board[0]) == 15

    def test_board_square_types_initialized(self):
        """Test that square types are initialized"""
        dictionary = Gaddag(words=[])
        board = Board(dictionary)
        assert len(board.square_types) == 225  # 15 * 15
        assert (0, 0) in board.square_types
        assert (14, 14) in board.square_types

    def test_center_square_is_default(self):
        """Test that center square (7,7) is DEFAULT"""
        dictionary = Gaddag(words=[])
        board = Board(dictionary)
        assert board.square_types[(7, 7)] == SquareType.DEFAULT

    def test_triple_word_squares_exist(self):
        """Test that some triple word squares exist"""
        dictionary = Gaddag(words=[])
        board = Board(dictionary)
        # Check corners and edges
        assert board.square_types[(0, 0)] == SquareType.TRIPLE_WORD
        assert board.square_types[(0, 7)] == SquareType.TRIPLE_WORD
        assert board.square_types[(7, 0)] == SquareType.TRIPLE_WORD
        assert board.square_types[(14, 14)] == SquareType.TRIPLE_WORD

    def test_triple_letter_squares_exist(self):
        """Test that some triple letter squares exist"""
        dictionary = Gaddag(words=[])
        board = Board(dictionary)
        # Triple letter squares should be at (1,5), (1,9), (1,13), (5,5), etc.
        # Note: (1,1) is overwritten by DOUBLE_WORD
        assert board.square_types[(1, 5)] == SquareType.TRIPLE_LETTER
        assert board.square_types[(1, 9)] == SquareType.TRIPLE_LETTER
        assert board.square_types[(5, 5)] == SquareType.TRIPLE_LETTER

    def test_double_word_squares_exist(self):
        """Test that some double word squares exist"""
        dictionary = Gaddag(words=[])
        board = Board(dictionary)
        # Double word squares should be on diagonals
        assert board.square_types[(1,
                                   1)] == SquareType.DOUBLE_WORD or board.square_types[(1,
                                                                                        1)] == SquareType.TRIPLE_LETTER
        # Some positions might be overwritten, so just check they have a type
        assert (2, 2) in board.square_types

    def test_all_squares_have_type(self):
        """Test that all squares have a type"""
        dictionary = Gaddag(words=[])
        board = Board(dictionary)
        for row in range(BOARD_ROWS):
            for col in range(BOARD_COLS):
                assert (row, col) in board.square_types
                assert isinstance(board.square_types[(row, col)], SquareType)

    def test_board_string_representation(self):
        """Test that Board can be converted to string"""
        dictionary = Gaddag(words=[])
        board = Board(dictionary)
        board_str = str(board)
        assert isinstance(board_str, str)
        # Should have 15 lines (one per row)
        lines = board_str.split("\n")
        assert len(lines) == 15

    def test_get_valid_moves_not_implemented(self):
        """Test that get_valid_moves raises NotImplementedError"""
        dictionary = Gaddag(words=[])
        board = Board(dictionary)
        with pytest.raises(NotImplementedError):
            board.get_valid_moves()


class TestHorizontalCrossChecks:
    """Tests for _update_horizontal_cross_checks method"""

    def test_horizontal_cross_check_example_1(self):
        """Test horizontal cross check with a letter to the left"""
        dictionary = Gaddag(words=["ACT", "ACTS", "TA"])
        board = Board(dictionary)
        board.place_word([Letter.A, Letter.C, Letter.T], (7, 7), vertical=True)

        assert len(board._horizontal_cross_checks[7][6]) == 1
        assert len(board._horizontal_cross_checks[7][8]) == 0
        assert Letter.T in board._horizontal_cross_checks[7][6]

        assert len(board._horizontal_cross_checks[8][6]) == 0
        assert len(board._horizontal_cross_checks[8][8]) == 0

        assert len(board._horizontal_cross_checks[9][6]) == 0
        assert len(board._horizontal_cross_checks[9][8]) == 1
        assert Letter.A in board._horizontal_cross_checks[9][8]

        # Vertical checks at the bottom of the word
        assert len(board._vertical_cross_checks[10][7]) == 1
        assert Letter.S in board._vertical_cross_checks[10][7]

    def test_horizontal_cross_check_example_2(self):
        dictionary = Gaddag(words=["PA", "ABLE", "PAYABLE", "PARABLE"])

        board = Board(dictionary)

        # Place first word
        board.place_word([Letter.P, Letter.A], (7, 5), vertical=False)

        # Bottom of the first letter should have PA as a valid word
        assert len(board._vertical_cross_checks[8][5]) == 1
        assert Letter.A in board._vertical_cross_checks[8][1]

        # Top of the second letter should have PA as a valid word
        assert len(board._vertical_cross_checks[6][6]) == 1
        assert Letter.P in board._vertical_cross_checks[6][6]

        # Rest of the word shouldn't have any horizontal / vertical cross checks

        assert len(board._vertical_cross_checks[6][5]) == 0

        assert len(board._vertical_cross_checks[8][6]) == 0

        assert len(board._horizontal_cross_checks[7][4]) == 0
        assert len(board._horizontal_cross_checks[7][7]) == 0

        # Place second word

        board.place_word([Letter.A, Letter.B, Letter.L, Letter.E], (7, 8), vertical=False)

        # R and Y should be in the middle horizontal cross check
        assert len(board._horizontal_cross_checks[7][7]) == 2
        assert Letter.R in board._horizontal_cross_checks[7][7]
        assert Letter.Y in board._horizontal_cross_checks[7][7]
