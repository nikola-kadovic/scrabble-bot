"""
This contains the code that implements a specialized DAWG (Directed Acyclic Word Graph) that
is used as a lexicon and word generator for the game of Scrabble.

By recording each reversed prefix of words as a separate node, we can answer the following queries fast:

1. 

The implementation is based on the following paper by Gordon, the inventor of the GADDAG:
https://users.cs.northwestern.edu/~robby/uc-courses/22001-2008-winter/faster-scrabble-gordon.pdf
"""