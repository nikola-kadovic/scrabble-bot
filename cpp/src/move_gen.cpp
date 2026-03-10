#include <omp.h>

#include <unordered_set>
#include <vector>

#include "scrabble/board.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/letter.hpp"
#include "scrabble/point.hpp"

namespace scrabble {

// ── File-local helpers ────────────────────────────────────────────────────────

namespace {

// Returns (gaddag_letter, is_blank) pairs to try for one rack tile.
// For a normal tile: just {tile, false}.
// For a blank:      {A,true}, {B,true}, ..., {Z,true}.
std::vector<std::pair<Letter, bool>> candidates_for(Letter rack_tile) {
  if (rack_tile != Letter::BLANK) {
    return {{rack_tile, false}};
  }
  std::vector<std::pair<Letter, bool>> result;
  result.reserve(26);
  for (int i = 0; i < 26; i++) {
    result.push_back({static_cast<Letter>(i), true});
  }
  return result;
}

// Remove one occurrence of `tile` from rack.
std::vector<Letter> rack_minus(const std::vector<Letter>& rack, Letter tile) {
  std::vector<Letter> result;
  result.reserve(!rack.empty() ? rack.size() - 1 : 0);
  bool removed = false;
  for (Letter l : rack) {
    if (!removed && l == tile) {
      removed = true;
      continue;
    }
    result.push_back(l);
  }
  return result;
}

// Returns deduplicated rack tiles (avoid processing the same tile type twice).
std::vector<Letter> unique_rack_tiles(const std::vector<Letter>& rack) {
  std::unordered_set<Letter> seen;
  std::vector<Letter> result;
  for (Letter l : rack) {
    if (seen.insert(l).second) {
      result.push_back(l);
    }
  }
  return result;
}

}  // namespace

// ── Board private method implementations ──────────────────────────────────────

CrossCheckSet Board::get_cross_checks_for(int r, int c, bool vertical) const {
  // When generating horizontal moves, we need vertical cross-checks (a tile
  // placed horizontally must form valid vertical words with its neighbors).
  // Symmetrically, vertical moves use horizontal cross-checks.
  return vertical ? horizontal_cross_checks[r][c] : vertical_cross_checks[r][c];
}

void Board::record_move(const std::vector<TileEntry>& left_part,
                        const std::vector<TileEntry>& right_part, int anchor_r, int anchor_c,
                        bool vertical, std::vector<Move>& results) const {
  int dr = vertical ? 1 : 0;
  int dc = vertical ? 0 : 1;
  int num_left = static_cast<int>(left_part.size());
  int total = num_left + static_cast<int>(right_part.size());

  // Full word in order = reverse(left_part) + right_part
  // word_letters: actual played letters (A-Z, never BLANK) stored in Move
  // score_letters: uses BLANK for blank tiles so scoring gives 0 for blanks
  std::vector<Letter> word_letters, score_letters;
  std::vector<bool> is_blank_vec;
  word_letters.reserve(total);
  score_letters.reserve(total);
  is_blank_vec.reserve(total);

  for (int i = num_left - 1; i >= 0; i--) {
    const auto& t = left_part[i];
    word_letters.push_back(t.gaddag_letter);
    score_letters.push_back(t.score_letter());
    is_blank_vec.push_back(t.is_blank);
  }
  for (const auto& t : right_part) {
    word_letters.push_back(t.gaddag_letter);
    score_letters.push_back(t.score_letter());
    is_blank_vec.push_back(t.is_blank);
  }

  int word_start_r = anchor_r - dr * (num_left - 1);
  int word_start_c = anchor_c - dc * (num_left - 1);
  int word_end_r = word_start_r + dr * (total - 1);
  int word_end_c = word_start_c + dc * (total - 1);

  int score = calculate_score(score_letters, word_start_r, word_start_c, vertical);

  results.push_back(Move{Point{word_start_r, word_start_c}, Point{word_end_r, word_end_c},
                         std::move(word_letters), std::move(is_blank_vec), score});
}

void Board::extend_right(int r, int c, bool vertical, const State* state,
                         const std::vector<Letter>& rack, std::vector<TileEntry>& left_part,
                         std::vector<TileEntry>& right_part, int anchor_r, int anchor_c,
                         std::vector<Move>& results) const {
  if (!in_bounds(r, c)) return;

  int dr = vertical ? 1 : 0;
  int dc = vertical ? 0 : 1;
  int next_r = r + dr, next_c = c + dc;
  bool next_empty = !in_bounds(next_r, next_c) || board[next_r][next_c] == Letter::BLANK;

  if (board[r][c] != Letter::BLANK) {
    // Existing tile – must consume it
    Letter L = board[r][c];
    right_part.push_back({L, false, false});

    // Check word end BEFORE navigating
    if (ltmaw_has(state->letters_that_make_a_word, L) && next_empty) {
      record_move(left_part, right_part, anchor_r, anchor_c, vertical, results);
    }

    const auto *next_state = state->get_next_state(static_cast<int>(L));
    if (next_state) {
      extend_right(next_r, next_c, vertical, next_state, rack, left_part, right_part, anchor_r,
                   anchor_c, results);
    }
    right_part.pop_back();
  } else {
    // Empty square – try rack tiles
    const auto& cc = get_cross_checks_for(r, c, vertical);
    for (Letter rack_tile : unique_rack_tiles(rack)) {
      for (auto [gaddag_L, is_blank] : candidates_for(rack_tile)) {
        if (!cross_check_has(cc, gaddag_L)) continue;

        right_part.push_back({gaddag_L, is_blank, true});

        // Check word end BEFORE navigating
        if (ltmaw_has(state->letters_that_make_a_word, gaddag_L) && next_empty) {
          record_move(left_part, right_part, anchor_r, anchor_c, vertical, results);
        }

        const auto *next_state = state->get_next_state(static_cast<int>(gaddag_L));
        if (next_state) {
          auto remaining = rack_minus(rack, rack_tile);
          extend_right(next_r, next_c, vertical, next_state, remaining, left_part, right_part,
                       anchor_r, anchor_c, results);
        }

        right_part.pop_back();
      }
    }
  }
}

void Board::extend_left(int r, int c, bool vertical, const State* state,
                        const std::vector<Letter>& rack, int limit,
                        std::vector<TileEntry>& left_part, int anchor_r, int anchor_c,
                        std::vector<Move>& results) const {
  int dr = vertical ? 1 : 0;
  int dc = vertical ? 0 : 1;

  // Option A: stop here, cross delimiter, extend right
  const auto *delim_state = state->get_next_state(DELIMITER_ARC_INDEX);
  if (delim_state) {
    std::vector<TileEntry> right_part;
    extend_right(anchor_r + dr, anchor_c + dc, vertical, delim_state, rack, left_part, right_part,
                 anchor_r, anchor_c, results);
  }

  // Option B: extend one more step left (if limit allows and square is empty)
  if (limit > 0 && in_bounds(r, c) && board[r][c] == Letter::BLANK) {
    const auto& cc = get_cross_checks_for(r, c, vertical);
    for (Letter rack_tile : unique_rack_tiles(rack)) {
      for (auto [gaddag_L, is_blank] : candidates_for(rack_tile)) {
        if (!cross_check_has(cc, gaddag_L)) continue;

        const auto *next_state = state->get_next_state(static_cast<int>(gaddag_L));
        if (!next_state) continue;

        auto remaining = rack_minus(rack, rack_tile);
        left_part.push_back({gaddag_L, is_blank, true});
        extend_left(r - dr, c - dc, vertical, next_state, remaining, limit - 1, left_part, anchor_r,
                    anchor_c, results);
        left_part.pop_back();
      }
    }
  }
}

void Board::generate_for_anchor(int r, int c, bool vertical, const std::vector<Letter>& rack,
                                std::vector<Move>& results) const {
  int dr = vertical ? 1 : 0;
  int dc = vertical ? 0 : 1;
  int back_r = r - dr;
  int back_c = c - dc;

  const auto& cc_anchor = get_cross_checks_for(r, c, vertical);

  if (in_bounds(back_r, back_c) && board[back_r][back_c] != Letter::BLANK) {
    // ── Case 1: existing tiles immediately behind the anchor ─────────────────
    // The anchor gets a new tile from rack; then we consume existing tiles
    // going left in GADDAG order; then cross delimiter; then extend right.
    for (Letter rack_tile : unique_rack_tiles(rack)) {
      for (auto [gaddag_L, is_blank] : candidates_for(rack_tile)) {
        if (!cross_check_has(cc_anchor, gaddag_L)) continue;

        const auto *state = dict_->root->get_next_state(static_cast<int>(gaddag_L));
        if (!state) continue;

        std::vector<TileEntry> left_part;
        left_part.push_back({gaddag_L, is_blank, true});  // anchor tile (new)

        int pos_r = back_r, pos_c = back_c;
        bool ok = true;

        // Is the square immediately to the right of the anchor empty?
        // If not, a word ending at the anchor would abut an existing tile and
        // form a longer (potentially invalid) run.
        bool right_of_anchor_empty =
            !in_bounds(r + dr, c + dc) || board[r + dr][c + dc] == Letter::BLANK;

        while (in_bounds(pos_r, pos_c) && board[pos_r][pos_c] != Letter::BLANK) {
          Letter tile = board[pos_r][pos_c];

          // Check if this is the leftmost occupied tile (nothing further left).
          // If it's in LTMAW, the word can end here (no right extension).
          int further_r = pos_r - dr, further_c = pos_c - dc;
          bool is_leftmost =
              !in_bounds(further_r, further_c) || board[further_r][further_c] == Letter::BLANK;

          if (is_leftmost && right_of_anchor_empty &&
              ltmaw_has(state->letters_that_make_a_word, tile)) {
            // Record a move that ends at this leftmost tile (no right part).
            left_part.push_back({tile, false, false});
            std::vector<TileEntry> empty_right;
            record_move(left_part, empty_right, r, c, vertical, results);
            left_part.pop_back();
          }

          // Navigate through this tile for potential right-extension paths.
          state = state->get_next_state(static_cast<int>(tile));
          if (!state) {
            ok = false;
            break;
          }
          left_part.push_back({tile, false, false});
          pos_r -= dr;
          pos_c -= dc;
        }

        if (!ok) continue;  // navigation failed; no delimiter-crossing path possible

        // All existing left tiles consumed with valid state – cross delimiter
        // and extend right for words that have a suffix after the existing tiles.
        const auto *delim_state = state->get_next_state(DELIMITER_ARC_INDEX);
        if (!delim_state) continue;

        auto remaining = rack_minus(rack, rack_tile);
        std::vector<TileEntry> right_part;
        extend_right(r + dr, c + dc, vertical, delim_state, remaining, left_part, right_part, r, c,
                     results);
      }
    }
  } else {
    // ── Case 2: free left extension ──────────────────────────────────────────
    // Compute how many empty squares exist to the left before hitting an anchor
    // point or a non-empty square (this bounds how far left we can extend).
    int left_limit = 0;
    {
      int pos_r = back_r, pos_c = back_c;
      while (in_bounds(pos_r, pos_c) && board[pos_r][pos_c] == Letter::BLANK &&
             !anchor_points[pos_r][pos_c]) {
        left_limit++;
        pos_r -= dr;
        pos_c -= dc;
      }
    }

    for (Letter rack_tile : unique_rack_tiles(rack)) {
      for (auto [gaddag_L, is_blank] : candidates_for(rack_tile)) {
        if (!cross_check_has(cc_anchor, gaddag_L)) continue;

        const auto *state = dict_->root->get_next_state(static_cast<int>(gaddag_L));
        if (!state) continue;

        auto remaining = rack_minus(rack, rack_tile);
        std::vector<TileEntry> left_part;
        left_part.push_back({gaddag_L, is_blank, true});  // anchor tile (new)
        extend_left(back_r, back_c, vertical, state, remaining, left_limit, left_part, r, c,
                    results);
      }
    }
  }
}

std::vector<Move> Board::get_all_valid_moves(const std::vector<Letter>& rack) const {
  std::vector<std::tuple<int, int, bool>> work;
  for (int r = 0; r < BOARD_ROWS; r++)
    for (int c = 0; c < BOARD_COLS; c++)
      if (anchor_points[r][c]) {
        work.emplace_back(r, c, false);
        work.emplace_back(r, c, true);
      }

  std::vector<std::vector<Move>> slots(work.size());

  // Guard against nested parallelism: when called from within VectorizedEnv::step_all()
  // the outer loop already occupies OpenMP threads; run serially in that case.
  if (omp_in_parallel()) {
    for (int i = 0; i < (int)work.size(); i++) {
      auto [r, c, vertical] = work[i];
      generate_for_anchor(r, c, vertical, rack, slots[i]);
    }
  } else {
#pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < (int)work.size(); i++) {
      auto [r, c, vertical] = work[i];
      generate_for_anchor(r, c, vertical, rack, slots[i]);
    }
  }

  std::vector<Move> results;
  for (auto& slot : slots) results.insert(results.end(), slot.begin(), slot.end());
  return results;
}

}  // namespace scrabble
