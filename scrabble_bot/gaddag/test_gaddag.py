"""
Basic unit tests for gaddag.py using pytest
"""

import pytest
import os
import tempfile
import shutil
from pathlib import Path
from gaddag import State, Arc, Gaddag, DELIMETER


class TestState:
    """Tests for the State class"""

    def test_state_initialization_empty(self):
        """Test State initialization with no arcs"""
        state = State()
        assert state.arcs == {}
        assert state.letters_that_make_a_word == set()

    def test_state_initialization_with_arcs(self):
        """Test State initialization with arcs"""
        arc1 = Arc(destination_state=State())
        arc2 = Arc(destination_state=State())
        state = State([('x', arc1), ('y', arc2)])
        assert len(state.arcs) == 2
        assert 'x' in state.arcs
        assert 'y' in state.arcs
        assert state.arcs['x'] == arc1
        assert state.arcs['y'] == arc2

    def test_add_arc_creates_new_arc(self):
        """Test that add_arc creates a new arc if it doesn't exist"""
        state = State()
        destination = state.add_arc('a')
        assert 'a' in state.arcs
        assert isinstance(state.arcs['a'], Arc)
        assert isinstance(destination, State)
        assert state.arcs['a'].destination_state == destination

    def test_add_arc_with_provided_destination(self):
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
        """Test that add_ending_arc creates a new arc and adds ending letter"""
        state = State()
        destination = state.add_ending_arc('d', 'e')
        assert 'd' in state.arcs
        assert isinstance(destination, State)
        # The ending letter should be added to the destination state
        assert 'e' in destination.letters_that_make_a_word

    def test_add_ending_arc_adds_to_existing(self):
        """Test that add_ending_arc adds to existing arc's destination state"""
        state = State()
        dest1 = state.add_ending_arc('f', 'g')
        dest2 = state.add_ending_arc('f', 'h')
        assert dest1 == dest2  # Should return same destination
        assert 'g' in dest1.letters_that_make_a_word
        assert 'h' in dest1.letters_that_make_a_word
        assert len(dest1.letters_that_make_a_word) == 2

    def test_add_ending_letter(self):
        """Test that add_ending_letter adds letter to the set"""
        state = State()
        state.add_ending_letter('x')
        assert 'x' in state.letters_that_make_a_word
        state.add_ending_letter('y')
        assert 'y' in state.letters_that_make_a_word
        assert len(state.letters_that_make_a_word) == 2

    def test_add_ending_letter_duplicate(self):
        """Test that adding duplicate ending letters doesn't create duplicates"""
        state = State()
        state.add_ending_letter('z')
        state.add_ending_letter('z')
        assert len(state.letters_that_make_a_word) == 1
        assert 'z' in state.letters_that_make_a_word


class TestArc:
    """Tests for the Arc class"""

    def test_arc_initialization_empty(self):
        """Test Arc initialization with default values"""
        arc = Arc()
        assert isinstance(arc.destination_state, State)

    def test_arc_initialization_with_destination(self):
        """Test Arc initialization with provided destination state"""
        destination = State()
        arc = Arc(destination_state=destination)
        assert arc.destination_state == destination

    def test_arc_initialization_with_letters_parameter(self):
        """Test Arc initialization with letters_that_make_a_word parameter (even though not used)"""
        destination = State()
        arc = Arc(letters_that_make_a_word={'a', 'b'}, destination_state=destination)
        assert arc.destination_state == destination


class TestGaddag:
    """Tests for the Gaddag class"""

    def test_gaddag_initialization_with_words(self):
        """Test Gaddag initialization with words list"""
        words = ["CAT", "DOG"]
        gaddag = Gaddag(words=words)
        assert gaddag.words == words
        assert isinstance(gaddag.root, State)

    def test_gaddag_initialization_with_wordlist_path(self):
        """Test Gaddag initialization with wordlist path"""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
            f.write("CAT\n")
            f.write("DOG\n")
            temp_path = f.name

        try:
            gaddag = Gaddag(wordlist_path=temp_path)
            assert gaddag.wordlist_path == temp_path
            assert isinstance(gaddag.root, State)
            # Words should be loaded automatically
            assert len(gaddag.words) == 2
            assert "CAT" in gaddag.words
            assert "DOG" in gaddag.words
        finally:
            os.unlink(temp_path)

    def test_load_wordlist_simple(self):
        """Test loading a simple wordlist"""
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
            f.write("CAT\n")
            f.write("DOG\n")
            f.write("BAT\n")
            temp_path = f.name

        try:
            gaddag = Gaddag(words=[])
            gaddag.wordlist_path = temp_path
            gaddag.words = []  # Initialize words list
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
            gaddag = Gaddag(words=[])
            gaddag.wordlist_path = temp_path
            gaddag.words = []
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
            gaddag = Gaddag(words=[])
            gaddag.wordlist_path = temp_path
            gaddag.words = []
            gaddag.load_wordlist()
            assert len(gaddag.words) == 0
        finally:
            os.unlink(temp_path)

    def test_add_word_two_letters(self):
        """Test adding a two-letter word"""
        gaddag = Gaddag(words=[])
        gaddag.add_word("AB")
        # Should create arcs in the root
        assert isinstance(gaddag.root, State)
        # For a 2-letter word, the algorithm should create some structure
        assert len(gaddag.root.arcs) > 0

    def test_add_word_three_letters(self):
        """Test adding a three-letter word"""
        gaddag = Gaddag(words=[])
        gaddag.add_word("CAT")
        # Should create proper GADDAG structure
        assert isinstance(gaddag.root, State)
        # Check that arcs were created
        assert len(gaddag.root.arcs) > 0

    def test_add_word_multiple_words(self):
        """Test adding multiple words"""
        gaddag = Gaddag(words=[])
        gaddag.add_word("CAT")
        gaddag.add_word("DOG")
        gaddag.add_word("BAT")
        assert isinstance(gaddag.root, State)
        # Multiple words should create more arcs
        assert len(gaddag.root.arcs) > 0

    def test_add_word_single_letter_raises_error(self):
        """Test that single letter words raise an IndexError"""
        gaddag = Gaddag(words=[])
        # Single letter words cause IndexError due to range(len(word)-1, 1, -1) being empty
        with pytest.raises(IndexError):
            gaddag.add_word("A")

    def test_build_gaddag(self):
        """Test building GADDAG from wordlist"""
        words = ["CAT", "DOG", "BAT"]
        gaddag = Gaddag(words=words)
        assert len(gaddag.words) == 3
        gaddag.build_gaddag()
        # After building, the root should have arcs
        assert isinstance(gaddag.root, State)
        assert len(gaddag.root.arcs) > 0

    def test_delimiter_constant(self):
        """Test that DELIMETER constant is defined correctly"""
        assert DELIMETER == "◇"
        assert isinstance(DELIMETER, str)
        assert len(DELIMETER) == 1


class TestGaddagIntegration:
    """Integration tests for Gaddag"""

    def test_full_workflow(self):
        """Test the full workflow: initialize with words, build, verify structure"""
        words = ["CAT", "DOG", "BAT", "RAT"]
        gaddag = Gaddag(words=words)
        assert len(gaddag.words) == 4

        gaddag.build_gaddag()
        # Verify the structure was built
        assert isinstance(gaddag.root, State)
        # Root should have arcs for the words we added
        assert len(gaddag.root.arcs) > 0


class TestGaddagCaching:
    """Tests for Gaddag caching functionality"""

    def test_cache_not_created_with_words_list(self):
        """Test that cache is NOT created when using words list (only file wordlists are cached)"""
        import shutil
        from pathlib import Path

        # Clean up any existing cache
        cache_dir = Path("temp")
        if cache_dir.exists():
            shutil.rmtree(cache_dir)

        words = ["CAT", "DOG", "BAT"]

        # Build with words list - should NOT create cache
        gaddag1 = Gaddag(words=words)
        gaddag1.build_gaddag()

        # Verify cache file was NOT created
        cache_files = list(cache_dir.glob("gaddag_*.pkl"))
        assert len(cache_files) == 0

        # Clean up
        if cache_dir.exists():
            shutil.rmtree(cache_dir)

    def test_cache_save_and_load_with_wordlist_path(self):
        """Test that cache is saved and can be loaded when using wordlist_path"""
        import shutil
        from pathlib import Path

        # Clean up any existing cache
        cache_dir = Path("temp")
        if cache_dir.exists():
            shutil.rmtree(cache_dir)

        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
            f.write("CAT\n")
            f.write("DOG\n")
            f.write("BAT\n")
            temp_path = f.name
            temp_filename = Path(temp_path).stem

        try:
            # First build - should create cache
            gaddag1 = Gaddag(wordlist_path=temp_path)
            gaddag1.build_gaddag()
            root1_arcs_count = len(gaddag1.root.arcs)

            # Verify cache file was created with filename-based key
            expected_cache_file = cache_dir / f"gaddag_{temp_filename}.pkl"
            assert expected_cache_file.exists()

            # Second build - should load from cache
            gaddag2 = Gaddag(wordlist_path=temp_path)
            gaddag2.build_gaddag()
            root2_arcs_count = len(gaddag2.root.arcs)

            # Verify structure is the same
            assert root1_arcs_count == root2_arcs_count
        finally:
            os.unlink(temp_path)
            if cache_dir.exists():
                shutil.rmtree(cache_dir)

    def test_cache_different_filenames_creates_different_cache(self):
        """Test that different wordlist filenames create different cache files"""
        import shutil
        from pathlib import Path

        # Clean up any existing cache
        cache_dir = Path("temp")
        if cache_dir.exists():
            shutil.rmtree(cache_dir)

        # Create two different wordlist files
        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt', prefix='wordlist1_') as f1:
            f1.write("CAT\nDOG\n")
            temp_path1 = f1.name
            temp_filename1 = Path(temp_path1).stem

        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt', prefix='wordlist2_') as f2:
            f2.write("BAT\nRAT\n")
            temp_path2 = f2.name
            temp_filename2 = Path(temp_path2).stem

        try:
            # Build with first wordlist
            gaddag1 = Gaddag(wordlist_path=temp_path1)
            gaddag1.build_gaddag()

            # Build with second wordlist
            gaddag2 = Gaddag(wordlist_path=temp_path2)
            gaddag2.build_gaddag()

            # Should have two different cache files based on filenames
            expected_cache1 = cache_dir / f"gaddag_{temp_filename1}.pkl"
            expected_cache2 = cache_dir / f"gaddag_{temp_filename2}.pkl"
            assert expected_cache1.exists()
            assert expected_cache2.exists()
            assert expected_cache1 != expected_cache2
        finally:
            os.unlink(temp_path1)
            os.unlink(temp_path2)
            if cache_dir.exists():
                shutil.rmtree(cache_dir)

    def test_build_gaddag_without_cache(self):
        """Test that build_gaddag can be called with use_cache=False"""
        import shutil
        from pathlib import Path

        # Clean up any existing cache
        cache_dir = Path("temp")
        if cache_dir.exists():
            shutil.rmtree(cache_dir)

        with tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.txt') as f:
            f.write("CAT\nDOG\n")
            temp_path = f.name

        try:
            # Build with cache
            gaddag1 = Gaddag(wordlist_path=temp_path)
            gaddag1.build_gaddag(use_cache=True)
            root1_arcs_count = len(gaddag1.root.arcs)

            # Build without cache (should rebuild)
            gaddag2 = Gaddag(wordlist_path=temp_path)
            gaddag2.build_gaddag(use_cache=False)
            root2_arcs_count = len(gaddag2.root.arcs)

            # Both should have the same structure
            assert root1_arcs_count == root2_arcs_count
        finally:
            os.unlink(temp_path)
            if cache_dir.exists():
                shutil.rmtree(cache_dir)
