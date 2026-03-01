#include "scrabble/board.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

namespace scrabble {

Board::Board(std::shared_ptr<Gaddag> dictionary)
    : dict_(std::move(dictionary)), first_move_(true) {
  // Initialize board with BLANK
  for (auto &row : board) {
    row.fill(Letter::BLANK);
  }

  build_square_types();

  // Initialize all cross-check sets with all letters that appear in the GADDAG
  auto dict_letters = dict_->get_dictionary_letters();
  for (int r = 0; r < BOARD_ROWS; r++) {
    for (int c = 0; c < BOARD_COLS; c++) {
      horizontal_cross_checks[r][c] = dict_letters;
      vertical_cross_checks[r][c] = dict_letters;
    }
  }

  // Seed the center square as the initial anchor point for the first move.
  // update_anchor_points() called after place_word() will remove {7,7} (now
  // occupied) and populate real neighbors.
  anchor_points.insert(Point{7, 7});
}

// ─── Square type initialization
// ─────────────────────────────────────────────── Follows the standard Scrabble
// board layout. Reference:
// https://simple.wikipedia.org/wiki/Scrabble#/media/File:Scrabble_board_-_English.svg

void Board::build_square_types() {
  // Default everything
  for (int r = 0; r < BOARD_ROWS; r++) {
    for (int c = 0; c < BOARD_COLS; c++) {
      square_types[r][c] = SquareType::DEFAULT;
    }
  }

  // Triple letter squares (every 4 squares, starting at 1)
  for (int i = 1; i < BOARD_ROWS; i += 4) {
    for (int j = 1; j < BOARD_COLS; j += 4) {
      square_types[i][j] = SquareType::TRIPLE_LETTER;
    }
  }

  // Double word squares (diagonals, rows 1-4)
  for (int i = 1; i <= 4; i++) {
    square_types[i][i] = SquareType::DOUBLE_WORD;
    square_types[i][BOARD_COLS - i - 1] = SquareType::DOUBLE_WORD;
    square_types[BOARD_ROWS - i - 1][i] = SquareType::DOUBLE_WORD;
    square_types[BOARD_ROWS - i - 1][BOARD_COLS - i - 1] =
        SquareType::DOUBLE_WORD;
  }

  // Triple word squares (every 7 squares from 0)
  for (int i = 0; i < BOARD_ROWS; i += 7) {
    for (int j = 0; j < BOARD_COLS; j += 7) {
      square_types[i][j] = SquareType::TRIPLE_WORD;
    }
  }
  // Restore center to DOUBLE_WORD (the TWS loop above would have overridden
  // it); place_word will clear it to DEFAULT after the first move.
  square_types[7][7] = SquareType::DOUBLE_WORD;

  // Double letter squares (4 quadrant symmetry from paper positions)
  const int dx[] = {1, 1, -1, -1};
  const int dy[] = {1, -1, 1, -1};

  // Offsets of DLS in one quadrant relative to corner (0,0)
  // Each pair (qx, qy) drives a quadrant
  // The Python code uses an offset-based approach; replicate it exactly:
  for (int q = 0; q < 4; q++) {
    int qx = dx[q];
    int qy = dy[q];

    auto cell = [&](int ox, int oy) -> std::pair<int, int> {
      int r = qx * ox + (qx == -1 ? BOARD_ROWS - 1 : 0);
      int c = qy * oy + (qy == -1 ? BOARD_COLS - 1 : 0);
      return {r, c};
    };

    // Python: (qx*0 + base_r, qy*3 + base_c)
    auto [r0, c0] = cell(0, 3);
    square_types[r0][c0] = SquareType::DOUBLE_LETTER;
    auto [r1, c1] = cell(3, 0);
    square_types[r1][c1] = SquareType::DOUBLE_LETTER;
    auto [r2, c2] = cell(2, 6);
    square_types[r2][c2] = SquareType::DOUBLE_LETTER;
    auto [r3, c3] = cell(6, 2);
    square_types[r3][c3] = SquareType::DOUBLE_LETTER;
    auto [r4, c4] = cell(6, 6);
    square_types[r4][c4] = SquareType::DOUBLE_LETTER;
    auto [r5, c5] = cell(3, 7);
    square_types[r5][c5] = SquareType::DOUBLE_LETTER;
    auto [r6, c6] = cell(7, 3);
    square_types[r6][c6] = SquareType::DOUBLE_LETTER;
  }
}

// ─── place_word (public)
// ───────────────────────────────────────────────────────

int Board::place_word(const std::vector<Letter> &word, Point start, Point end) {
  if (!in_bounds(start.row, start.col))
    throw std::invalid_argument("Starting point is out of bounds");

  bool vertical = (start.col == end.col);
  int score = vertical ? place_word_vertically(word, start.row, start.col)
                       : place_word_horizontally(word, start.row, start.col);

  if (first_move_) {
    first_move_ = false;
    square_types[7][7] = SquareType::DEFAULT;
  }

  return score;
}

int Board::place_word(const Move &move) {
  return place_word(move.letters, move.start, move.end);
}

// ─── calculate_score
// ─────────────────────────────────────────────────────────

int Board::calculate_score(const std::vector<Letter> &word, int sp_row,
                           int sp_col, bool vertical) const {
  int len = static_cast<int>(word.size());
  int word_score = 0;
  int additional_words_score = 0;
  int word_multiplier = 1;

  int row_step = vertical ? 1 : 0;
  int col_step = vertical ? 0 : 1;
  // cross directions: negative (above/left) and positive (below/right)
  int cdr1 = vertical ? 0 : -1, cdc1 = vertical ? -1 : 0;
  int cdr2 = vertical ? 0 : 1, cdc2 = vertical ? 1 : 0;

  for (int i = 0; i < len; i++) {
    int r = sp_row + i * row_step;
    int c = sp_col + i * col_step;

    if (board[r][c] != Letter::BLANK)
      continue; // pre-existing tile, no bonus

    // Main word: apply letter/word bonus for this new tile
    switch (square_types[r][c]) {
    case SquareType::DOUBLE_LETTER:
      word_score += get_letter_score(word[i]) * 2;
      break;
    case SquareType::TRIPLE_LETTER:
      word_score += get_letter_score(word[i]) * 3;
      break;
    case SquareType::DOUBLE_WORD:
      word_score += get_letter_score(word[i]);
      word_multiplier *= 2;
      break;
    case SquareType::TRIPLE_WORD:
      word_score += get_letter_score(word[i]);
      word_multiplier *= 3;
      break;
    case SquareType::DEFAULT:
      word_score += get_letter_score(word[i]);
      break;
    default:
      throw std::runtime_error("square type unrecognized");
    }

    // Cross word in negative direction (above / left)
    if (in_bounds(r + cdr1, c + cdc1) &&
        board[r + cdr1][c + cdc1] != Letter::BLANK) {
      int aws = 0;
      for (int y = r + cdr1, x = c + cdc1;
           in_bounds(y, x) && board[y][x] != Letter::BLANK;
           y += cdr1, x += cdc1)
        aws += get_letter_score(board[y][x]);
      switch (square_types[r][c]) {
      case SquareType::DOUBLE_LETTER:
        aws += get_letter_score(word[i]) * 2;
        break;
      case SquareType::TRIPLE_LETTER:
        aws += get_letter_score(word[i]) * 3;
        break;
      case SquareType::DOUBLE_WORD:
        aws += get_letter_score(word[i]);
        aws *= 2;
        break;
      case SquareType::TRIPLE_WORD:
        aws += get_letter_score(word[i]);
        aws *= 3;
        break;
      case SquareType::DEFAULT:
        aws += get_letter_score(word[i]);
        break;
      default:
        throw std::runtime_error("square type unrecognized");
      }
      additional_words_score += aws;
    }

    // Cross word in positive direction (below / right)
    if (in_bounds(r + cdr2, c + cdc2) &&
        board[r + cdr2][c + cdc2] != Letter::BLANK) {
      int aws = 0;
      for (int y = r + cdr2, x = c + cdc2;
           in_bounds(y, x) && board[y][x] != Letter::BLANK;
           y += cdr2, x += cdc2)
        aws += get_letter_score(board[y][x]);
      switch (square_types[r][c]) {
      case SquareType::DOUBLE_LETTER:
        aws += get_letter_score(word[i]) * 2;
        break;
      case SquareType::TRIPLE_LETTER:
        aws += get_letter_score(word[i]) * 3;
        break;
      case SquareType::DOUBLE_WORD:
        aws += get_letter_score(word[i]);
        aws *= 2;
        break;
      case SquareType::TRIPLE_WORD:
        aws += get_letter_score(word[i]);
        aws *= 3;
        break;
      case SquareType::DEFAULT:
        aws += get_letter_score(word[i]);
        break;
      default:
        throw std::runtime_error("square type unrecognized");
      }
      additional_words_score += aws;
    }
  }

  return (word_multiplier * word_score) + additional_words_score +
         (len == 7 ? 50 : 0);
}

// ─── place_word_horizontally
// ──────────────────────────────────────────────────

int Board::place_word_horizontally(const std::vector<Letter> &word, int sp_row,
                                   int sp_col) {
  int len = static_cast<int>(word.size());
  if (sp_col + len > BOARD_COLS) {
    throw std::invalid_argument(
        "Word too long to fit horizontally at given position");
  }

  if (first_move_) {
    // Mirrors Python: sp[0] != 7 or not (sp[1] <= 7 <= sp[1] + len(word))
    // Note: sp[1] + len is the exclusive end, so this matches Python's
    // behaviour exactly.
    bool ok = (sp_row == 7) && (sp_col <= 7) && (7 <= sp_col + len);
    if (!ok) {
      throw std::invalid_argument(
          "First move must be on row 7 and cover the center column 7");
    }
  }

  // Conflict check
  for (int i = 0; i < len; i++) {
    int r = sp_row;
    int c = sp_col + i;
    if (board[r][c] != Letter::BLANK && board[r][c] != word[i]) {
      throw std::invalid_argument(
          "Square already occupied by a different letter");
    }
  }

  int final_score = calculate_score(word, sp_row, sp_col, /*vertical=*/false);

  // Place letters
  for (int i = 0; i < len; i++)
    board[sp_row][sp_col + i] = word[i];

  // Update cross-checks (board already has new letters placed)
  for (int i = 0; i < len; i++) {
    int c = sp_col + i;
    // Walk up to first blank above this column tile
    {
      int rr = sp_row - 1;
      while (in_bounds(rr, c) && board[rr][c] != Letter::BLANK)
        rr--;
      if (in_bounds(rr, c))
        update_vertical_cross_checks(rr, c);
    }
    // Walk down to first blank below this column tile
    {
      int rr = sp_row + 1;
      while (in_bounds(rr, c) && board[rr][c] != Letter::BLANK)
        rr++;
      if (in_bounds(rr, c))
        update_vertical_cross_checks(rr, c);
    }
  }
  // Walk left past any pre-existing tiles to the first blank
  {
    int c = sp_col - 1;
    while (in_bounds(sp_row, c) && board[sp_row][c] != Letter::BLANK)
      c--;
    if (in_bounds(sp_row, c))
      update_horizontal_cross_checks(sp_row, c);
  }
  // Walk right past the word (and any pre-existing tiles) to the first blank
  {
    int c = sp_col + len;
    while (in_bounds(sp_row, c) && board[sp_row][c] != Letter::BLANK)
      c++;
    if (in_bounds(sp_row, c))
      update_horizontal_cross_checks(sp_row, c);
  }

  update_anchor_points(sp_row, sp_col, sp_row, sp_col + len - 1);

  return final_score;
}

// ─── place_word_vertically
// ────────────────────────────────────────────────────

int Board::place_word_vertically(const std::vector<Letter> &word, int sp_row,
                                 int sp_col) {
  int len = static_cast<int>(word.size());
  if (sp_row + len > BOARD_ROWS) {
    throw std::invalid_argument(
        "Word too long to fit vertically at given position");
  }

  if (first_move_) {
    // Mirrors Python: sp[1] != 7 or not (sp[0] <= 7 <= sp[0] + len(word))
    bool ok = (sp_col == 7) && (sp_row <= 7) && (7 <= sp_row + len);
    if (!ok) {
      throw std::invalid_argument(
          "First move must be on column 7 and cover the center row 7");
    }
  }

  // Conflict check
  for (int i = 0; i < len; i++) {
    int r = sp_row + i;
    int c = sp_col;
    if (board[r][c] != Letter::BLANK && board[r][c] != word[i]) {
      throw std::invalid_argument(
          "Square already occupied by a different letter");
    }
  }

  int final_score = calculate_score(word, sp_row, sp_col, /*vertical=*/true);

  // Place letters
  for (int i = 0; i < len; i++)
    board[sp_row + i][sp_col] = word[i];

  // Update cross-checks (board already has new letters placed)
  for (int i = 0; i < len; i++) {
    int r = sp_row + i;
    // Walk left to first blank left of this row tile
    {
      int cc = sp_col - 1;
      while (in_bounds(r, cc) && board[r][cc] != Letter::BLANK)
        cc--;
      if (in_bounds(r, cc))
        update_horizontal_cross_checks(r, cc);
    }
    // Walk right to first blank right of this row tile
    {
      int cc = sp_col + 1;
      while (in_bounds(r, cc) && board[r][cc] != Letter::BLANK)
        cc++;
      if (in_bounds(r, cc))
        update_horizontal_cross_checks(r, cc);
    }
  }
  // Walk up past any pre-existing tiles to the first blank
  {
    int r = sp_row - 1;
    while (in_bounds(r, sp_col) && board[r][sp_col] != Letter::BLANK)
      r--;
    if (in_bounds(r, sp_col))
      update_vertical_cross_checks(r, sp_col);
  }
  // Walk down past the word (and any pre-existing tiles) to the first blank
  {
    int r = sp_row + len;
    while (in_bounds(r, sp_col) && board[r][sp_col] != Letter::BLANK)
      r++;
    if (in_bounds(r, sp_col))
      update_vertical_cross_checks(r, sp_col);
  }

  update_anchor_points(sp_row, sp_col, sp_row + len - 1, sp_col);

  return final_score;
}

// ─── Cross-check update helpers
// ───────────────────────────────────────────────

void Board::update_horizontal_cross_checks(int row, int col) {
  auto &checks = horizontal_cross_checks[row][col];
  for (Letter letter : dict_->get_dictionary_letters()) {
    if (letter_makes_word_horizontally(row, col, letter)) {
      checks.insert(letter);
    } else {
      checks.erase(letter);
    }
  }
}

void Board::update_vertical_cross_checks(int row, int col) {
  auto &checks = vertical_cross_checks[row][col];
  for (Letter letter : dict_->get_dictionary_letters()) {
    if (letter_makes_word_vertically(row, col, letter)) {
      checks.insert(letter);
    } else {
      checks.erase(letter);
    }
  }
}

// ─── letter_makes_word_horizontally ──────────────────────────────────────────
//
// Checks whether placing `letter` at (row, col) forms a valid horizontal word
// with the letters already on the board to its left and/or right.
//
// Mirrors the Python _letter_makes_a_word_horizontally method exactly.

bool Board::letter_makes_word_horizontally(int row, int col,
                                           Letter letter) const {
  bool has_left = (col > 0) && (board[row][col - 1] != Letter::BLANK);
  bool has_right =
      (col < BOARD_COLS - 1) && (board[row][col + 1] != Letter::BLANK);

  // Start traversal from root using the placed letter
  std::string letter_key = letter_to_key(letter);
  std::shared_ptr<State> state = dict_->root->get_next_state(letter_key);
  if (!state) {
    // Should not happen for any letter present in the dictionary
    return false;
  }

  int idx = col; // tracks last position examined (for the final check)

  if (has_left) {
    for (idx = col - 1; idx >= 0; idx--) {
      if (idx == 0 || board[row][idx - 1] == Letter::BLANK) {
        // Don't advance; `idx` is the leftmost letter — check it last
        break;
      }
      auto next = state->get_next_state(letter_to_key(board[row][idx]));
      if (!next)
        return false;
      state = next;
    }
    if (!state)
      return false;
  }

  if (has_left && has_right) {
    // Process the leftmost letter before crossing the delimiter
    auto next = state->get_next_state(letter_to_key(board[row][idx]));
    if (!next)
      return false;
    state = next;
  }

  if (!state)
    return false;

  if (has_right) {
    auto next = state->get_next_state(DELIMITER);
    if (!next)
      return false;
    state = next;

    for (idx = col + 1; idx < BOARD_COLS; idx++) {
      if (idx == BOARD_COLS - 1 || board[row][idx + 1] == Letter::BLANK) {
        // Don't advance; `idx` is the rightmost letter — check it last
        break;
      }
      auto next2 = state->get_next_state(letter_to_key(board[row][idx]));
      if (!next2)
        return false;
      state = next2;
    }
  }

  if (!has_left && !has_right)
    return false;

  char last_char = letter_to_char(board[row][idx]);
  return state && state->letters_that_make_a_word.count(last_char) > 0;
}

// ─── letter_makes_word_vertically ────────────────────────────────────────────
//
// Symmetric counterpart for the vertical axis.

bool Board::letter_makes_word_vertically(int row, int col,
                                         Letter letter) const {
  bool has_up = (row > 0) && (board[row - 1][col] != Letter::BLANK);
  bool has_down =
      (row < BOARD_ROWS - 1) && (board[row + 1][col] != Letter::BLANK);

  std::string letter_key = letter_to_key(letter);
  std::shared_ptr<State> state = dict_->root->get_next_state(letter_key);
  if (!state)
    return false;

  int idx = row;

  if (has_up) {
    for (idx = row - 1; idx >= 0; idx--) {
      if (idx == 0 || board[idx - 1][col] == Letter::BLANK) {
        break;
      }
      auto next = state->get_next_state(letter_to_key(board[idx][col]));
      if (!next)
        return false;
      state = next;
    }
    if (!state)
      return false;
  }

  if (has_up && has_down) {
    auto next = state->get_next_state(letter_to_key(board[idx][col]));
    if (!next)
      return false;
    state = next;
  }

  if (!state)
    return false;

  if (has_down) {
    auto next = state->get_next_state(DELIMITER);
    if (!next)
      return false;
    state = next;

    for (idx = row + 1; idx < BOARD_ROWS; idx++) {
      if (idx == BOARD_ROWS - 1 || board[idx + 1][col] == Letter::BLANK) {
        break;
      }
      auto next2 = state->get_next_state(letter_to_key(board[idx][col]));
      if (!next2)
        return false;
      state = next2;
    }
  }

  if (!has_up && !has_down)
    return false;

  char last_char = letter_to_char(board[idx][col]);
  return state && state->letters_that_make_a_word.count(last_char) > 0;
}

// ─── update_anchor_points ────────────────────────────────────────────────────

void Board::update_anchor_points(int r1, int c1, int r2, int c2) {
  bool vertical = (c1 == c2);

  if (vertical) {
    for (int r = r1; r <= r2; r++) {
      anchor_points.erase({r, c1});
      // Horizontal neighbors
      if (in_bounds(r, c1 - 1) && board[r][c1 - 1] == Letter::BLANK) {
        anchor_points.insert({r, c1 - 1});
      }
      if (in_bounds(r, c1 + 1) && board[r][c1 + 1] == Letter::BLANK) {
        anchor_points.insert({r, c1 + 1});
      }
    }
    // Vertical endpoints
    if (in_bounds(r1 - 1, c1) && board[r1 - 1][c1] == Letter::BLANK) {
      anchor_points.insert({r1 - 1, c1});
    }
    if (in_bounds(r2 + 1, c2) && board[r2 + 1][c2] == Letter::BLANK) {
      anchor_points.insert({r2 + 1, c2});
    }
  } else {
    for (int c = c1; c <= c2; c++) {
      anchor_points.erase({r1, c});
      // Vertical neighbors
      if (in_bounds(r1 - 1, c) && board[r1 - 1][c] == Letter::BLANK) {
        anchor_points.insert({r1 - 1, c});
      }
      if (in_bounds(r1 + 1, c) && board[r1 + 1][c] == Letter::BLANK) {
        anchor_points.insert({r1 + 1, c});
      }
    }
    // Horizontal endpoints
    if (in_bounds(r1, c1 - 1) && board[r1][c1 - 1] == Letter::BLANK) {
      anchor_points.insert({r1, c1 - 1});
    }
    if (in_bounds(r2, c2 + 1) && board[r2][c2 + 1] == Letter::BLANK) {
      anchor_points.insert({r2, c2 + 1});
    }
  }
}

// ─── is_word_valid
// ───────────────────────────────────────────────────────────────
//
// Traverses the C◇... GADDAG path: root → word[0] → DELIMITER → word[1] →
// ... → word[n-2], then checks word[n-1] in letters_that_make_a_word.

bool Board::is_word_valid(const std::vector<Letter> &word) const {
  int n = static_cast<int>(word.size());
  if (n < 2)
    return false;

  auto state = dict_->root->get_next_state(letter_to_key(word[0]));
  if (!state)
    return false;

  state = state->get_next_state(DELIMITER);
  if (!state)
    return false;

  for (int i = 1; i <= n - 2; i++) {
    state = state->get_next_state(letter_to_key(word[i]));
    if (!state)
      return false;
  }

  char last = letter_to_char(word[n - 1]);
  return state->letters_that_make_a_word.count(last) > 0;
}

// ─── validate_board
// ──────────────────────────────────────────────────────────────
//
// Scans every row and column for contiguous tile runs of length >= 2 and
// returns the string representation of any that are not valid dictionary words.

std::vector<std::string> Board::validate_board() const {
  std::vector<std::string> invalid_words;

  auto check_run = [&](const std::vector<Letter> &run) {
    if (run.size() < 2)
      return;
    if (!is_word_valid(run)) {
      std::string s;
      s.reserve(run.size());
      for (Letter l : run)
        s += letter_to_char(l);
      invalid_words.push_back(std::move(s));
    }
  };

  // Scan rows
  for (int r = 0; r < BOARD_ROWS; r++) {
    std::vector<Letter> run;
    for (int c = 0; c < BOARD_COLS; c++) {
      if (board[r][c] != Letter::BLANK) {
        run.push_back(board[r][c]);
      } else {
        check_run(run);
        run.clear();
      }
    }
    check_run(run);
  }

  // Scan columns
  for (int c = 0; c < BOARD_COLS; c++) {
    std::vector<Letter> run;
    for (int r = 0; r < BOARD_ROWS; r++) {
      if (board[r][c] != Letter::BLANK) {
        run.push_back(board[r][c]);
      } else {
        check_run(run);
        run.clear();
      }
    }
    check_run(run);
  }

  return invalid_words;
}

// ─── to_string
// ────────────────────────────────────────────────────────────────

std::string Board::to_string() const {
  // ANSI color codes
  const std::string RED = "\033[91m";        // Triple word
  const std::string YELLOW = "\033[93m";     // Double word
  const std::string LIGHT_BLUE = "\033[96m"; // Double letter
  const std::string DARK_BLUE = "\033[94m";  // Triple letter
  const std::string GRAY = "\033[90m";       // Default empty
  const std::string GREEN = "\033[92m";      // Anchor points
  const std::string BOLD = "\033[1m";        // Placed letters
  const std::string RESET = "\033[0m";

  auto square_color = [&](SquareType t) -> const std::string & {
    switch (t) {
    case SquareType::TRIPLE_WORD:
      return RED;
    case SquareType::DOUBLE_WORD:
      return YELLOW;
    case SquareType::DOUBLE_LETTER:
      return LIGHT_BLUE;
    case SquareType::TRIPLE_LETTER:
      return DARK_BLUE;
    default:
      return GRAY;
    }
  };

  auto square_char = [](SquareType t) -> char {
    switch (t) {
    case SquareType::TRIPLE_WORD:
      return 'W';
    case SquareType::DOUBLE_WORD:
      return 'w';
    case SquareType::DOUBLE_LETTER:
      return 'l';
    case SquareType::TRIPLE_LETTER:
      return 'L';
    default:
      return '.';
    }
  };

  std::ostringstream out;

  // Header: column numbers
  out << "   ";
  for (int c = 0; c < BOARD_COLS; c++) {
    char buf[4];
    std::snprintf(buf, sizeof(buf), "%2d", c);
    out << buf;
    if (c < BOARD_COLS - 1)
      out << ' ';
  }
  out << '\n';

  // Separator line
  out << "   " << std::string(BOARD_COLS * 3 - 1, '-') << '\n';

  // Board rows
  for (int r = 0; r < BOARD_ROWS; r++) {
    char row_label[4];
    std::snprintf(row_label, sizeof(row_label), "%2d", r);
    out << row_label << '|';

    for (int c = 0; c < BOARD_COLS; c++) {
      Letter l = board[r][c];
      out << ' ';
      if (l != Letter::BLANK) {
        out << BOLD << letter_to_char(l) << RESET;
      } else if (anchor_points.count(Point{r, c})) {
        out << GREEN << '.' << RESET;
      } else {
        out << square_color(square_types[r][c])
            << square_char(square_types[r][c]) << RESET;
      }
      out << ' ';
    }

    out << '|' << row_label << '\n';
  }

  // Footer separator and column numbers
  out << "   " << std::string(BOARD_COLS * 3 - 1, '-') << '\n';
  out << "   ";
  for (int c = 0; c < BOARD_COLS; c++) {
    char buf[4];
    std::snprintf(buf, sizeof(buf), "%2d", c);
    out << buf;
    if (c < BOARD_COLS - 1)
      out << ' ';
  }
  out << '\n';

  return out.str();
}

} // namespace scrabble
