"""
This contains the code that implements a specialized DAWG (Directed Acyclic Word Graph) that
is used as a lexicon and word generator for the game of Scrabble.

By recording each reversed prefix of words as a separate node, we 
effectively create a state machine that helps us answer the following question:

- Given a prefix, what are all the words that can be formed by adding letters to the prefix?

The implementation is based on the following paper by Gordon, the inventor of the GADDAG:
https://users.cs.northwestern.edu/~robby/uc-courses/22001-2008-winter/faster-scrabble-gordon.pdf
"""

from __future__ import annotations
from typing import Optional

DELIMETER = "◇"

class State: 
    """
    A state specifies the arcs leaving it and their associated letters.
    """
    def __init__(self, arcs: list[tuple[str, Arc]] = []):
        self.arcs:dict[str, Arc] = dict(arcs)
    
    def add_arc(self, letter: str, destination_state: "Optional[State]" = None) -> State:
        if destination_state is None:
            destination_state = State()

        if letter not in self.arcs:
            self.arcs[letter] = Arc(letters_that_make_a_word=set(), destination_state=destination_state)

        return self.arcs[letter].destination_state 

    def add_ending_arc(self, letter: str, ending_letter) -> State:
        if letter not in self.arcs:
            self.arcs[letter] = Arc(
                letters_that_make_a_word=set(ending_letter), destination_state=State())
        else:
            self.arcs[letter].add_letter_to_set(ending_letter)

        return self.arcs[letter].destination_state 

class Arc:
    """
    An arc specifies the destination state and the set of letters that can be added to the current 
    prefix to make a word.
    """
    def __init__(self, letters_that_make_a_word: set[str] = set(), destination_state: State = State()):
        self.letters_that_make_a_word = letters_that_make_a_word
        self.destination_state = destination_state
    
    def add_letter_to_set(self, letter: str):
        self.letters_that_make_a_word.add(letter)

class Gaddag:
    """
    A GADDAG is a reverse-prefix directed acyclic word graph that is used as a lexicon and word generator for the game of Scrabble.
    """
    def __init__(self, wordlist_path: str):
        self.wordlist_path = wordlist_path
        self.words = []
        self.root = State()

    def load_wordlist(self):
        with open(self.wordlist_path, 'r') as file:
            for idx, line in file:
                try:
                    word = line.strip().split(' ')[0]
                except IndexError:
                    print(f"Error: line {idx} in {self.wordlist_path} is not in an expected format")
                    continue
                self.words.append(word)
    
    def build_gaddag(self):
        for word in self.words:
            self.add_word(word)


    def add_word(self, word: str):
        current_state = self.root
        # Add arcs for word[n..1] 
        for i in range(len(word)-1, 2, -1):
            current_state = current_state.add_arc(word[i])
        current_state.add_ending_arc(word[1], word[0])

        current_state = self.root
        # Add arcs for word[n-1..1]◇word[n]
        for i in range(len(word)-2, 0, -1):
            current_state = current_state.add_arc(word[i])
        current_state = current_state.add_ending_arc(DELIMETER, word[-1])

        # Add all other reverse prefixes word[i..0]◇ rev(word[i+1..n]), i = n-2, n-3, ..., 2
        for m in range(len(word)-2, 0, -1):
            destination = current_state
            current_state = self.root
            for i in range(m-1, 0, -1):
                current_state = current_state.add_arc(word[i])
            current_state = current_state.add_arc(DELIMETER)
            current_state.add_arc(word[m], destination)
