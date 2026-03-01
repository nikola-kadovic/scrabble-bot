# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Commands

```bash
# Install dependencies and compile the C++ extension
uv sync

# Manual CMake iteration (for faster C++ iteration)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# C++ tests (Catch2)
ctest --test-dir build --output-on-failure -C Release
# Or run directly on Windows:
.\build\cpp\tests\Release\scrabble_tests.exe --reporter console -v

# Python integration tests
uv run pytest scrabble_bot/tests/ -v
uv run pytest scrabble_bot/tests/test_integration_board.py -v
uv run pytest scrabble_bot/tests/test_integration_gaddag.py -v
```

Formatting is handled by autopep8 (max line length 100, aggressive level 1).
Type checking uses Pyright in basic mode (stubs in `scrabble_bot/_cpp_ext.pyi`).

C++ formatting and static analysis use clang-format and clang-tidy (config in `.clang-format`
and `.clang-tidy`). Run via Makefile targets:

```bash
make format        # format all C++ files in place
make format_check  # check formatting (non-zero exit if diff found)
make tidy          # run clang-tidy static analysis (builds first)
```

## Architecture

This is a Scrabble-playing AI bot. The game logic lives in a C++ core compiled to a
Python extension module (`_cpp_ext`) via pybind11. Python is reserved for agent/RL code.

### Build system
`scikit-build-core` wraps CMake so `uv sync` compiles the extension automatically.
`FetchContent` downloads pybind11 v2.13.6 and Catch2 v3.7.1 at configure time.

### Directory layout
```
cpp/
  include/scrabble/   <- public C++ headers
  src/                <- implementation (.cpp)
  bindings/           <- pybind11 PYBIND11_MODULE(_cpp_ext, m)
  tests/              <- Catch2 unit tests (scrabble_tests executable)
scrabble_bot/
  _cpp_ext.pyi        <- hand-written stub for Pyright
  __init__.py         <- re-exports from _cpp_ext
  board/__init__.py   <- backward-compat re-exports
  gaddag/__init__.py  <- backward-compat re-exports
  tests/              <- Python integration tests (pytest)
temp/                 <- runtime cache (.bin files, gitignored)
```

### GADDAG (`cpp/include/scrabble/gaddag.hpp` + `cpp/src/gaddag.cpp`)
Implements the GADDAG data structure for fast move generation. The delimiter character
`U+25C7` (UTF-8: `\xE2\x97\x87`) separates the reversed-prefix portion from the
suffix portion of each encoded word. The cache is stored as a binary file
(`temp/gaddag_<wordlist-stem>.bin`). Old `.pkl` files from the previous Python
implementation are harmless and can be deleted manually.

### Board (`cpp/include/scrabble/board.hpp` + `cpp/src/board.cpp`)
15x15 board with `SquareType` bonuses. Coordinates are `(row, col)`; center is `(7, 7)`.
Tracks:
- **Anchor points**: empty squares adjacent to placed tiles.
- **Cross checks**: `horizontal_cross_checks[r][c]` stores letters valid for horizontal
  words (consulted when generating *vertical* moves); `vertical_cross_checks` is the
  symmetric counterpart.

### Letter + Move (`cpp/include/scrabble/letter.hpp` + `move.hpp`)
`enum class Letter : uint8_t { A=0 ... Z=25, BLANK=26 }` with `LETTER_SCORES` array.
`struct Move { int starting_index, ending_index; vector<string> letters; int score; }`.

### Move Generation (in progress)
The backtracking algorithm using GADDAG traversal seeded from anchor points is
not yet implemented (`get_all_valid_moves`).
- Based on https://users.cs.northwestern.edu/~robby/uc-courses/22001-2008-winter/faster-scrabble-gordon.pdf
- See "The move generation algorithm" section
- This is a backtracking algorithm. Given a list of the letters you have, for each anchor point, find all traversals of the GADDAG that work on the board.
- For each result, we should also have the score you'd gain by placing that word.
- We return a list of all those possible moves, their scores, and the remaining letters you have.
- This should be exposed in python bindings.
- For now, let's just focus on correctness of the algorithm. But in the future, we can implement parallelized backtracking for a significant speedup.
- For the future, not now: https://cs.uwaterloo.ca/~kdaudjee/DaudjeeJPDC.pdf

### Data Flow
```
Wordlist file -> Gaddag::build_from_file() -> binary-cached DAG
Board state -> anchor points + cross checks -> backtracking search -> valid moves
```
