from enum import Enum


class Letter(Enum):
    A = "A"
    B = "B"
    C = "C"
    D = "D"
    E = "E"
    F = "F"
    G = "G"
    H = "H"
    I = "I"
    J = "J"
    K = "K"
    L = "L"
    M = "M"
    N = "N"
    O = "O"
    P = "P"
    Q = "Q"
    R = "R"
    S = "S"
    T = "T"
    U = "U"
    V = "V"
    W = "W"
    X = "X"
    Y = "Y"
    Z = "Z"
    BLANK = "_"

    def __str__(self):
        return self.value


# Taken from: https://en.wikipedia.org/wiki/Scrabble_letter_distributions
LETTER_SCORES = {
    Letter.A: 1,
    Letter.B: 3,
    Letter.C: 3,
    Letter.D: 2,
    Letter.E: 1,
    Letter.F: 4,
    Letter.G: 2,
    Letter.H: 4,
    Letter.I: 1,
    Letter.J: 8,
    Letter.K: 5,
    Letter.L: 1,
    Letter.M: 3,
    Letter.N: 1,
    Letter.O: 1,
    Letter.P: 3,
    Letter.Q: 10,
    Letter.R: 1,
    Letter.S: 1,
    Letter.T: 1,
    Letter.U: 1,
    Letter.V: 4,
    Letter.W: 4,
    Letter.X: 8,
    Letter.Y: 4,
    Letter.Z: 10,
    Letter.BLANK: 0,
}