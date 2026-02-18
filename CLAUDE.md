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

# Python smoke tests
uv run pytest scrabble_bot/tests/ -v
uv run pytest scrabble_bot/tests/test_smoke_board.py -v
uv run pytest scrabble_bot/tests/test_smoke_gaddag.py -v
```

Formatting is handled by autopep8 (max line length 100, aggressive level 1).
Type checking uses Pyright in basic mode (stubs in `scrabble_bot/_cpp_ext.pyi`).

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
  tests/              <- Python smoke tests (pytest)
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

### Data Flow
```
Wordlist file -> Gaddag::build_from_file() -> binary-cached DAG
Board state -> anchor points + cross checks -> backtracking search -> valid moves
```
