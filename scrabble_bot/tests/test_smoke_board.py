"""
Python smoke tests for the Board pybind11 bindings.
These verify that the C++ extension is correctly wired to Python,
reproducing the key assertions from the original test_board.py.
"""

import pytest
from scrabble_bot._cpp_ext import (
    BOARD_COLS,
    BOARD_ROWS,
    LETTER_SCORES,
    Board,
    Gaddag,
    Letter,
    Move,
    SquareType,
)


def make_gaddag(*words: str) -> Gaddag:
    g = Gaddag()
    g.build_from_words(list(words))
    return g


# ── Letter ────────────────────────────────────────────────────────────────────


class TestLetter:
    def test_all_letters_exist(self):
        letters = list(Letter)
        assert len(letters) == 27  # A-Z + BLANK

    def test_str_representation(self):
        assert str(Letter.A) == "A"
        assert str(Letter.BLANK) == "_"

    def test_blank_value(self):
        assert str(Letter.BLANK) == "_"


class TestLetterScores:
    def test_all_letters_have_scores(self):
        for l in Letter:
            assert l in LETTER_SCORES

    def test_scores(self):
        assert LETTER_SCORES[Letter.A] == 1
        assert LETTER_SCORES[Letter.E] == 1
        assert LETTER_SCORES[Letter.Q] == 10
        assert LETTER_SCORES[Letter.Z] == 10
        assert LETTER_SCORES[Letter.BLANK] == 0
        assert LETTER_SCORES[Letter.J] == 8
        assert LETTER_SCORES[Letter.X] == 8


# ── SquareType ────────────────────────────────────────────────────────────────


class TestSquareType:
    def test_str_representations(self):
        assert str(SquareType.DEFAULT) == "."
        assert str(SquareType.DOUBLE_WORD) == "w"
        assert str(SquareType.TRIPLE_WORD) == "W"
        assert str(SquareType.DOUBLE_LETTER) == "l"
        assert str(SquareType.TRIPLE_LETTER) == "L"


# ── Move ─────────────────────────────────────────────────────────────────────


class TestMove:
    def test_move_initialization(self):
        m = Move(0, 5, ["A", "B", "C"], 10)
        assert m.starting_index == 0
        assert m.ending_index == 5
        assert m.letters == ["A", "B", "C"]
        assert m.score == 10


# ── Board ─────────────────────────────────────────────────────────────────────


class TestBoard:
    def test_board_initialization(self):
        g = make_gaddag()
        b = Board(g)
        assert BOARD_ROWS == 15
        assert BOARD_COLS == 15
        assert len(b.board) == 15
        assert len(b.board[0]) == 15

    def test_board_square_types_initialized(self):
        g = make_gaddag()
        b = Board(g)
        sq = b.square_types
        assert len(sq) == 225
        assert (0, 0) in sq
        assert (14, 14) in sq

    def test_center_square_is_default(self):
        g = make_gaddag()
        b = Board(g)
        assert b.square_types[(7, 7)] == SquareType.DEFAULT

    def test_triple_word_squares(self):
        g = make_gaddag()
        b = Board(g)
        sq = b.square_types
        assert sq[(0, 0)] == SquareType.TRIPLE_WORD
        assert sq[(0, 7)] == SquareType.TRIPLE_WORD
        assert sq[(7, 0)] == SquareType.TRIPLE_WORD
        assert sq[(14, 14)] == SquareType.TRIPLE_WORD

    def test_triple_letter_squares(self):
        g = make_gaddag()
        b = Board(g)
        sq = b.square_types
        assert sq[(1, 5)] == SquareType.TRIPLE_LETTER
        assert sq[(1, 9)] == SquareType.TRIPLE_LETTER
        assert sq[(5, 5)] == SquareType.TRIPLE_LETTER

    def test_all_squares_have_types(self):
        g = make_gaddag()
        b = Board(g)
        sq = b.square_types
        for row in range(BOARD_ROWS):
            for col in range(BOARD_COLS):
                assert (row, col) in sq
                assert isinstance(sq[(row, col)], SquareType)

    def test_anchor_points_initial_empty(self):
        g = make_gaddag()
        b = Board(g)
        assert len(b._anchor_points) == 0


class TestHorizontalCrossChecks:
    def test_cross_check_example_1(self):
        g = make_gaddag("ACT", "ACTS", "TA")
        b = Board(g)
        b.place_word([Letter.A, Letter.C, Letter.T], (7, 7), True)

        hcc = b._horizontal_cross_checks
        assert len(hcc[7][6]) == 1
        assert Letter.T in hcc[7][6]
        assert len(hcc[7][8]) == 0

        assert len(hcc[8][6]) == 0
        assert len(hcc[8][8]) == 0

        assert len(hcc[9][8]) == 1
        assert Letter.A in hcc[9][8]

        vcc = b._vertical_cross_checks
        assert len(vcc[10][7]) == 1
        assert Letter.S in vcc[10][7]

    def test_cross_check_example_2(self):
        g = make_gaddag("PA", "ABLE", "PAYABLE", "PARABLE")
        b = Board(g)

        b.place_word([Letter.P, Letter.A], (7, 5), False)

        vcc = b._vertical_cross_checks
        assert len(vcc[8][5]) == 1
        assert Letter.A in vcc[8][1]
        assert len(vcc[6][6]) == 1
        assert Letter.P in vcc[6][6]
        assert len(vcc[6][5]) == 0
        assert len(vcc[8][6]) == 0

        hcc = b._horizontal_cross_checks
        assert len(hcc[7][4]) == 0
        assert len(hcc[7][7]) == 0

        b.place_word([Letter.A, Letter.B, Letter.L, Letter.E], (7, 8), False)

        assert len(hcc[7][7]) == 2
        assert Letter.R in hcc[7][7]
        assert Letter.Y in hcc[7][7]

        assert len(vcc[6][8]) == 1
        assert Letter.P in vcc[6][8]


class TestAnchorPoints:
    def test_anchor_points_after_horizontal_word(self):
        g = make_gaddag("ACT")
        b = Board(g)
        b.place_word([Letter.A, Letter.C, Letter.T], (7, 7), False)

        anchors = b._anchor_points
        assert len(anchors) == 8
        expected = {(6, 7), (8, 7), (6, 8), (8, 8), (6, 9), (8, 9), (7, 6), (7, 10)}
        for a in expected:
            assert a in anchors

    def test_anchor_points_after_vertical_word(self):
        g = make_gaddag("ACT")
        b = Board(g)
        b.place_word([Letter.A, Letter.C, Letter.T], (7, 7), True)

        anchors = b._anchor_points
        assert len(anchors) == 8
        expected = {(6, 7), (10, 7), (7, 6), (8, 6), (9, 6), (7, 8), (8, 8), (9, 8)}
        for a in expected:
            assert a in anchors

    def test_anchor_points_after_multiple_words(self):
        g = make_gaddag("ACT", "CAT")
        b = Board(g)
        b.place_word([Letter.A, Letter.C, Letter.T], (7, 7), True)
        b.place_word([Letter.C, Letter.A, Letter.T], (8, 7), False)

        anchors = b._anchor_points
        assert len(anchors) == 10
        expected = {
            (7, 6),
            (8, 6),
            (9, 6),
            (10, 7),
            (9, 8),
            (9, 9),
            (8, 10),
            (7, 9),
            (7, 8),
            (6, 7),
        }
        for a in expected:
            assert a in anchors
