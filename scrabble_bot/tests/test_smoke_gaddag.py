"""
Python smoke tests for the GADDAG pybind11 bindings.
These verify that the C++ extension is correctly wired to Python.
"""

import os
import shutil
import tempfile

import pytest
from scrabble_bot._cpp_ext import (
    DELIMETER,
    DELIMITER,
    Arc,
    Gaddag,
    Letter,
    State,
)


class TestDelimiter:
    def test_delimiter_value(self):
        assert DELIMETER == "◇"
        assert DELIMITER == "◇"
        assert len(DELIMETER) == 3  # 3-byte UTF-8

    def test_delimiter_aliases_match(self):
        assert DELIMETER == DELIMITER


class TestState:
    def test_state_initializes_empty(self):
        s = State()
        assert s.arcs == {}
        assert s.letters_that_make_a_word == set()

    def test_add_arc_creates_arc(self):
        s = State()
        dest = s.add_arc("A")
        assert "A" in s.arcs
        assert isinstance(dest, State)

    def test_add_arc_idempotent(self):
        s = State()
        d1 = s.add_arc("B")
        d2 = s.add_arc("B")
        assert d1 is d2
        assert len(s.arcs) == 1

    def test_add_arc_with_destination(self):
        s = State()
        target = State()
        result = s.add_arc("C", target)
        assert result is target

    def test_add_ending_arc(self):
        s = State()
        dest = s.add_ending_arc("D", "E")
        assert isinstance(dest, State)
        assert "E" in dest.letters_that_make_a_word

    def test_add_ending_arc_existing(self):
        s = State()
        d1 = s.add_ending_arc("F", "G")
        d2 = s.add_ending_arc("F", "H")
        assert d1 is d2
        assert "G" in d1.letters_that_make_a_word
        assert "H" in d1.letters_that_make_a_word

    def test_add_ending_letter(self):
        s = State()
        s.add_ending_letter("X")
        s.add_ending_letter("X")  # duplicate
        s.add_ending_letter("Y")
        assert len(s.letters_that_make_a_word) == 2

    def test_get_next_state_missing(self):
        s = State()
        result = s.get_next_state("Z")
        assert result is None

    def test_get_next_state_found(self):
        s = State()
        dest = s.add_arc("A")
        result = s.get_next_state("A")
        assert result is dest


class TestArc:
    def test_arc_default_init(self):
        a = Arc()
        assert isinstance(a.destination_state, State)

    def test_arc_with_destination(self):
        target = State()
        a = Arc(destination_state=target)
        assert a.destination_state is target


class TestGaddag:
    def test_gaddag_init(self):
        g = Gaddag()
        assert isinstance(g.root, State)

    def test_build_from_words(self):
        g = Gaddag()
        g.build_from_words(["CAT", "DOG"])
        assert len(g.root.arcs) > 0

    def test_get_dictionary_letters(self):
        g = Gaddag()
        g.build_from_words(["CAT", "DOG"])
        letters = g.get_dictionary_letters()
        assert isinstance(letters, set)
        assert Letter.C in letters
        assert Letter.D in letters
        # DELIMITER should not appear as a Letter
        for l in letters:
            assert isinstance(l, Letter)

    def test_add_word_single_raises(self):
        g = Gaddag()
        with pytest.raises(Exception):
            g.add_word("A")

    def test_build_from_file(self):
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
            f.write("CAT\nDOG\nBAT\n")
            tmp = f.name
        try:
            g = Gaddag()
            g.build_from_file(tmp, use_cache=False)
            assert len(g.root.arcs) > 0
        finally:
            os.unlink(tmp)

    def test_cache_save_and_load(self):
        cache_dir = "temp_test_cache"
        # Build a small wordlist file
        with tempfile.NamedTemporaryFile(mode='w', delete=False,
                                         suffix='.txt', prefix='wl_smoke_') as f:
            f.write("CAT\nDOG\n")
            tmp = f.name

        try:
            g1 = Gaddag()
            g1.build_from_file(tmp, use_cache=True)
            n1 = len(g1.root.arcs)

            g2 = Gaddag()
            g2.build_from_file(tmp, use_cache=True)
            n2 = len(g2.root.arcs)

            assert n1 == n2
        finally:
            os.unlink(tmp)
            if os.path.exists("temp"):
                shutil.rmtree("temp")
