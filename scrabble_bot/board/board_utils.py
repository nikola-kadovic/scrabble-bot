from scrabble_bot.board.letter import Letter


def get_all_letters() -> set[Letter]:
    """
    Returns all letters in the alphabet.
    """
    return set([
        Letter.A,
        Letter.B,
        Letter.C,
        Letter.D,
        Letter.E,
        Letter.F,
        Letter.G,
        Letter.H,
        Letter.I,
        Letter.J,
        Letter.K,
        Letter.L,
        Letter.M,
        Letter.N,
        Letter.O,
        Letter.P,
        Letter.Q,
        Letter.R,
        Letter.S,
        Letter.T,
        Letter.U,
        Letter.V,
        Letter.W,
        Letter.X,
        Letter.Y,
        Letter.Z,
        Letter.BLANK,
    ])
