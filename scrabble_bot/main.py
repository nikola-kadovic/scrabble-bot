import os
import random

from scrabble_bot._cpp_ext import Board, Gaddag, Letter, char_to_letter


def main():
    gaddag = Gaddag()
    gaddag.build_from_file(
        wordlist_path=os.path.join("dictionary", "nwl_2023.txt"), use_cache=True
    )

    board = Board(gaddag)

    # First move: HELLO horizontally through center (7, 7)
    score1 = board.place_word(
        [char_to_letter(c) for c in "HELLO"],
        (7, 5),  # so HELLO spans cols 5–9, row 7, crossing center (7,7)
        (7, 9),
    )

    print(f"score 1: {score1}")


    # Second move: BE vertically, crossing the E in HELLO
    score2 = board.place_word(
        [Letter.B, Letter.E],
        (6, 6),
        (7, 6),
    )

    print(f"score 2: {score2}")

    # Third move: ALE vertically at col 8, crossing the L in HELLO
    score3 = board.place_word(
        [Letter.A, Letter.L, Letter.E],
        (6, 8),
        (8, 8),
    )

    print(f"score 3: {score3}")

    print(board)
    print("Done.")


    actions = board.get_all_valid_moves([Letter.A, Letter.B, Letter.C, Letter.D, Letter.BLANK])

    a = random.choice(actions)
    board.place_word(a)

    print(board)


if __name__ == "__main__":
    main()
