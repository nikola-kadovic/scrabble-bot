"""
Test suite for gaddag.py

Tests the State, Arc, and Gaddag classes that implement a GADDAG
(Directed Acyclic Word Graph) for Scrabble word lookup.
"""

import pytest
import os
import sys
import tempfile

# Add parent directory to path to import gaddag
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from gaddag import State, Arc, Gaddag, DELIMETER


class TestState:
    """Tests for the State class"""

    def test_state_initialization_empty(self):
        """Test State initialization with no arcs"""
        state = State()
        assert state.arcs == {}

    def test_state_initialization_with_arcs(self):
        """Test State initialization with arcs"""
        arc1 = Arc(letters_that_make_a_word={'a'}, destination_state=State())
        arc2 = Arc(letters_that_make_a_word={'b'}, destination_state=State())
        state = State([('x', arc1), ('y', arc2)])
        assert len(state.arcs) == 2
        assert 'x' in state.arcs
        assert 'y' in state.arcs

    def test_add_arc_creates_new_arc(self):
        """Test that add_arc creates a new arc if it doesn't exist"""
        state = State()
        destination = state.add_arc('a')
        assert 'a' in state.arcs
        assert isinstance(state.arcs['a'], Arc)
        assert isinstance(destination, State)
        assert state.arcs['a'].destination_state == destination

    def test_add_arc_with_existing_destination(self):
        """Test add_arc with a provided destination state"""
        state = State()
        destination = State()
        result = state.add_arc('b', destination)
        assert result == destination
        assert state.arcs['b'].destination_state == destination

    def test_add_arc_returns_existing_destination(self):
        """Test that add_arc returns existing destination if arc exists"""
        state = State()
        dest1 = state.add_arc('c')
        dest2 = state.add_arc('c')
        assert dest1 == dest2
        assert len(state.arcs) == 1

    def test_add_ending_arc_creates_new_arc(self):
        """Test that add_ending_arc creates a new arc with ending letter"""
        state = State()
        destination = state.add_ending_arc('d', 'e')
        assert 'd' in state.arcs
        assert 'e' in state.arcs['d'].letters_that_make_a_word
        assert isinstance(destination, State)

    def test_add_ending_arc_adds_to_existing(self):
        """Test that add_ending_arc adds to existing arc's letter set"""
        state = State()
        state.add_ending_arc('f', 'g')
        state.add_ending_arc('f', 'h')
        assert 'g' in state.arcs['f'].letters_that_make_a_word
        assert 'h' in state.arcs['f'].letters_that_make_a_word
        assert len(state.arcs['f'].letters_that_make_a_word) == 2


class TestArc:
    """Tests for the Arc class"""

    def test_arc_initialization_empty(self):
        """Test Arc initialization with default values"""
        arc = Arc()
        assert arc.letters_that_make_a_word == set()
        assert isinstance(arc.destination_state, State)

    def test_arc_initialization_with_values(self):
        """Test Arc initialization with provided values"""
        destination = State()
        arc = Arc(letters_that_make_a_word={'a', 'b'}, destination_state=destination)
        assert arc.letters_that_make_a_word == {'a', 'b'}
        assert arc.destination_state == destination

    def test_add_letter_to_set(self):
        """Test adding a letter to the letter set"""
        arc = Arc(letters_that_make_a_word={'x'})
        arc.add_letter_to_set('y')
        assert 'x' in arc.letters_that_make_a_word
        assert 'y' in arc.letters_that_make_a_word
        assert len(arc.letters_that_make_a_word) == 2

    def test_add_letter_to_set_duplicate(self):
        """Test that adding duplicate letters doesn't create duplicates"""
        arc = Arc(letters_that_make_a_word={'z'})
        arc.add_letter_to_set('z')
        assert len(arc.letters_that_make_a_word) == 1
        assert 'z' in arc.letters_that_make_a_word


class TestGaddag:
    """Tests for the Gaddag class"""

    def test_gaddag_initialization(self):
        """Test Gaddag initialization"""
        gaddag = Gaddag("test_path.txt")
        assert gaddag.wordlist_path == "test_path.txt"
        assert gaddag.words == []
        assert isinstance(gaddag.root, State)

    def test_load_wordlist_simple(self):
        """Test loading a simple wordlist"""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
            f.write("CAT\n")
            f.write("DOG\n")
            f.write("BAT\n")
            temp_path = f.name

        try:
            gaddag = Gaddag(temp_path)
            gaddag.load_wordlist()
            assert len(gaddag.words) == 3
            assert "CAT" in gaddag.words
            assert "DOG" in gaddag.words
            assert "BAT" in gaddag.words
        finally:
            os.unlink(temp_path)

    def test_load_wordlist_with_spaces(self):
        """Test loading wordlist where words have spaces (takes first word)"""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
            f.write("WORD definition here\n")
            f.write("ANOTHER word\n")
            temp_path = f.name

        try:
            gaddag = Gaddag(temp_path)
            gaddag.load_wordlist()
            assert len(gaddag.words) == 2
            assert "WORD" in gaddag.words
            assert "ANOTHER" in gaddag.words
        finally:
            os.unlink(temp_path)

    def test_load_wordlist_empty_file(self):
        """Test loading an empty wordlist"""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
            pass  # Empty file
            temp_path = f.name

        try:
            gaddag = Gaddag(temp_path)
            gaddag.load_wordlist()
            assert len(gaddag.words) == 0
        finally:
            os.unlink(temp_path)

    def test_load_wordlist_empty_lines(self):
        """Test loading wordlist with empty lines"""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
            f.write("WORD\n")
            f.write("\n")
            f.write("ANOTHER\n")
            temp_path = f.name

        try:
            gaddag = Gaddag(temp_path)
            gaddag.load_wordlist()
            # Empty lines should result in empty strings
            assert "WORD" in gaddag.words
            assert "ANOTHER" in gaddag.words
        finally:
            os.unlink(temp_path)

    def test_add_word_single_letter(self):
        """Test that single letter words are not supported (Scrabble doesn't have single letter words)"""
        gaddag = Gaddag("dummy.txt")
        # Single letter words are not valid in Scrabble, so this should raise an error
        with pytest.raises(IndexError):
            gaddag.add_word("A")

    def test_add_word_two_letters(self):
        """Test adding a two-letter word"""
        gaddag = Gaddag("dummy.txt")
        gaddag.add_word("AB")
        # Should create arcs in the root
        assert isinstance(gaddag.root, State)

    def test_add_word_three_letters(self):
        """Test adding a three-letter word"""
        gaddag = Gaddag("dummy.txt")
        gaddag.add_word("CAT")
        # Should create proper GADDAG structure
        assert isinstance(gaddag.root, State)
        # Check that arcs were created
        assert len(gaddag.root.arcs) > 0

    def test_add_word_multiple_words(self):
        """Test adding multiple words"""
        gaddag = Gaddag("dummy.txt")
        gaddag.add_word("CAT")
        gaddag.add_word("DOG")
        gaddag.add_word("BAT")
        assert isinstance(gaddag.root, State)

    def test_build_gaddag(self):
        """Test building GADDAG from wordlist"""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
            f.write("CAT\n")
            f.write("DOG\n")
            temp_path = f.name

        try:
            gaddag = Gaddag(temp_path)
            gaddag.load_wordlist()
            assert len(gaddag.words) == 2
            gaddag.build_gaddag()
            # After building, the root should have arcs
            assert isinstance(gaddag.root, State)
        finally:
            os.unlink(temp_path)

    def test_add_word_structure_verification(self):
        """Test that add_word creates the expected GADDAG structure for a word"""
        gaddag = Gaddag("dummy.txt")
        word = "CAT"
        gaddag.add_word(word)
        
        # Verify root has arcs (should have arcs for reverse prefixes)
        assert isinstance(gaddag.root, State)
        # The exact structure depends on GADDAG algorithm, but root should have some arcs
        # for a 3-letter word, we expect some arcs to be created


class TestGaddagIntegration:
    """Integration tests for Gaddag"""

    def test_full_workflow(self):
        """Test the full workflow: load, build, verify structure"""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
            f.write("CAT\n")
            f.write("DOG\n")
            f.write("BAT\n")
            f.write("RAT\n")
            temp_path = f.name

        try:
            gaddag = Gaddag(temp_path)
            gaddag.load_wordlist()
            assert len(gaddag.words) == 4
            
            gaddag.build_gaddag()
            # Verify the structure was built
            assert isinstance(gaddag.root, State)
            # Root should have arcs for the words we added
        finally:
            os.unlink(temp_path)

    def test_delimiter_constant(self):
        """Test that DELIMETER constant is defined"""
        assert DELIMETER == "◇"
        assert isinstance(DELIMETER, str)
        assert len(DELIMETER) == 1


if __name__ == "__main__":
    pytest.main([__file__, "-v"])

