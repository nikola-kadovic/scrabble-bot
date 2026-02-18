import os

from scrabble_bot._cpp_ext import Board, Gaddag, Letter, char_to_letter


def main():
    gaddag = Gaddag()
    gaddag.build_from_file(
        wordlist_path=os.path.join("dictionary", "nwl_2023.txt"), use_cache=True
    )

    board = Board(gaddag)

    # First move: HELLO horizontally through center (7, 7)
    board.place_word(
        [char_to_letter(c) for c in "HELLO"],
        (7, 5),  # so HELLO spans cols 5–9, row 7, crossing center (7,7)
        vertical=False,
    )


    # Second move: BE vertically, crossing the E in HELLO
    board.place_word(
        [Letter.B, Letter.E],
        (6, 6),
        vertical=True,
    )

    # Third move: ALE vertically at col 8, crossing the L in HELLO
    board.place_word(
        [Letter.A, Letter.L, Letter.E],
        (6, 8),
        vertical=True,
    )

    print(board)
    print("Done.")


if __name__ == "__main__":
    main()
