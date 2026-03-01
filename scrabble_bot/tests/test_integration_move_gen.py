"""
Integration tests for move generation correctness.
Uses the real NWL wordlist (cached) so these are integration-level tests.
"""

import os

import pytest
from scrabble_bot._cpp_ext import Board, Gaddag, Letter, char_to_letter


WORDLIST = os.path.join("dictionary", "nwl_2023.txt")


@pytest.fixture(scope="module")
def gaddag():
    g = Gaddag()
    g.build_from_file(wordlist_path=WORDLIST, use_cache=True)
    return g


def make_hello_be_ale_board(gaddag) -> Board:
    """Board with HELLO (h, row 7, cols 5-9), BE (v, col 6, rows 6-7),
    ALE (v, col 8, rows 6-8)."""
    b = Board(gaddag)
    b.place_word([char_to_letter(c) for c in "HELLO"], (7, 5), (7, 9))
    b.place_word([Letter.B, Letter.E], (6, 6), (7, 6))
    b.place_word([Letter.A, Letter.L, Letter.E], (6, 8), (8, 8))
    return b


class TestMoveGenValidity:
    def test_all_generated_moves_are_valid(self, gaddag):
        """Every move returned by get_all_valid_moves must produce a board
        where validate_board() reports no invalid words."""
        board = make_hello_be_ale_board(gaddag)
        rack = [Letter.A, Letter.B, Letter.C, Letter.D, Letter.BLANK]
        moves = board.get_all_valid_moves(rack)

        assert len(moves) > 0, "Expected at least one valid move"

        for move in moves:
            b = make_hello_be_ale_board(gaddag)
            b.place_word(move)
            invalid = b.validate_board()
            assert invalid == [], (
                f"Move produced invalid word(s) {invalid}: "
                f"start=({move.start.row},{move.start.col}) "
                f"end=({move.end.row},{move.end.col}) "
                f"letters={move.letters} is_blank={move.is_blank}"
            )
    
    def test_first_moves_until_no_more_moves(self, gaddag):

        board = Board(gaddag)
        rack = [Letter.A, Letter.B, Letter.C, Letter.D, Letter.E, Letter.F, Letter.G]

        moves = board.get_all_valid_moves(rack)

        while len(moves) > 0:
            move = max(moves, key=lambda x: x.score)
            board.place_word(move)
            moves = board.get_all_valid_moves(rack)
        
        print(board)
        
        assert board.validate_board() == [], "Board should be valid"
