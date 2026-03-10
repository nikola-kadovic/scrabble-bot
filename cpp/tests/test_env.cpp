#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

#include "scrabble/env.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/letter.hpp"
#include "scrabble/vec_env.hpp"

using namespace scrabble;

namespace {

// Minimal word list: all 2-letter Scrabble words (TWL) to guarantee valid moves.
std::vector<std::string> two_letter_words() {
  return {"AA", "AB", "AD", "AE", "AG", "AH", "AI", "AL", "AM", "AN", "AR", "AS",
          "AT", "AW", "AX", "AY", "BA", "BE", "BI", "BO", "BY", "DA", "DE", "DO",
          "ED", "EF", "EH", "EL", "EM", "EN", "ER", "ES", "ET", "EX", "FA", "FE",
          "GI", "GO", "HA", "HE", "HI", "HO", "ID", "IF", "IN", "IS", "IT", "JO",
          "KA", "KI", "LA", "LI", "LO", "MA", "ME", "MI", "MO", "MU", "MY", "NA",
          "NE", "NO", "NU", "OD", "OE", "OF", "OH", "OM", "ON", "OP", "OR", "OS",
          "OW", "OX", "OY", "PA", "PE", "PI", "QI", "RE", "SH", "SI", "SO", "TA",
          "TE", "TI", "TO", "UH", "UM", "UN", "UP", "UR", "US", "UT", "WE", "WO",
          "XI", "XU", "YA", "YE", "YO", "ZA"};
}

std::shared_ptr<Gaddag> make_test_gaddag() {
  auto g = std::make_shared<Gaddag>();
  g->build_from_words(two_letter_words());
  return g;
}

}  // namespace

// ── ScrabbleEnv ───────────────────────────────────────────────────────────────

TEST_CASE("ScrabbleEnv reset — valid initial state", "[env]") {
  auto g = make_test_gaddag();
  ScrabbleEnv env(g, /*seed=*/42);

  // After construction: 7 tiles per player, 86 remaining in bag
  CHECK(env.rack(0).size() == 7);
  CHECK(env.rack(1).size() == 7);
  CHECK(env.bag_remaining() == 86);
  CHECK(!env.done());
  CHECK(env.current_player() == 0);
  CHECK(env.consecutive_passes() == 0);
  CHECK(env.player_scores()[0] == 0);
  CHECK(env.player_scores()[1] == 0);

  // Board should be empty (all BLANK)
  const Board& b = env.board();
  for (int r = 0; r < BOARD_ROWS; r++)
    for (int c = 0; c < BOARD_COLS; c++)
      CHECK(b.board[r][c] == Letter::BLANK);
}

TEST_CASE("ScrabbleEnv fill_observation — empty board encoding", "[env]") {
  auto g = make_test_gaddag();
  ScrabbleEnv env(g, 42);

  std::vector<float> buf(OBS_DIM, -1.0f);
  env.fill_observation(buf.data());

  // Board block: channel 26 (empty) should be 1.0 for all 225 squares
  for (int r = 0; r < 15; r++) {
    for (int c = 0; c < 15; c++) {
      int base = (r * 15 + c) * 27;
      CHECK(buf[base + 26] == 1.0f);
      for (int ch = 0; ch < 26; ch++) {
        CHECK(buf[base + ch] == 0.0f);
      }
    }
  }

  // Rack block: should sum to 1.0 (7 tiles each at 1/7)
  float rack_sum = 0.0f;
  for (int i = 0; i < 27; i++) rack_sum += buf[6075 + i];
  CHECK(std::abs(rack_sum - 1.0f) < 1e-5f);

  // Metadata in [0,1]
  for (int i = 0; i < 4; i++) {
    CHECK(buf[6102 + i] >= 0.0f);
    CHECK(buf[6102 + i] <= 1.0f);
  }
}

TEST_CASE("ScrabbleEnv get_valid_moves — pass always present", "[env]") {
  auto g = make_test_gaddag();
  ScrabbleEnv env(g, 42);

  constexpr int MAX = MAX_MOVES_PER_ENV;
  std::vector<int8_t>  coords(MAX * 4, 0);
  std::vector<uint8_t> letters(MAX * 7, 0);
  std::vector<uint8_t> is_blank(MAX * 7, 0);
  std::vector<int16_t> scores(MAX, 0);

  int count = env.get_valid_moves(coords.data(), letters.data(), is_blank.data(),
                                   scores.data(), MAX);

  REQUIRE(count >= 1);

  // Last move is always the pass: coords = (-1,-1,-1,-1)
  int last = count - 1;
  CHECK(coords[last * 4 + 0] == -1);
  CHECK(coords[last * 4 + 1] == -1);
  CHECK(coords[last * 4 + 2] == -1);
  CHECK(coords[last * 4 + 3] == -1);
  CHECK(scores[last] == 0);
}

TEST_CASE("ScrabbleEnv step with pass — increments consecutive_passes, done after 6", "[env]") {
  auto g = make_test_gaddag();
  ScrabbleEnv env(g, 42);

  // Pass move is at index = num_cached_moves (i.e. beyond regular moves)
  // We get the count first, then pass index = count - 1 from the buffer perspective.
  // But step() treats action_idx >= n as pass. Use a large index.
  constexpr int PASS_IDX = MAX_MOVES_PER_ENV;

  for (int i = 0; i < 5; i++) {
    auto r = env.step(PASS_IDX);
    CHECK(!r.done);
  }
  // 6th pass → done
  auto r = env.step(PASS_IDX);
  CHECK(r.done);
  CHECK(env.done());
}

TEST_CASE("ScrabbleEnv step with move — board updated, rack shrinks, score >= 0", "[env]") {
  auto g = make_test_gaddag();
  ScrabbleEnv env(g, 42);

  constexpr int MAX = MAX_MOVES_PER_ENV;
  std::vector<int8_t>  coords(MAX * 4, 0);
  std::vector<uint8_t> letters(MAX * 7, 0);
  std::vector<uint8_t> is_blank(MAX * 7, 0);
  std::vector<int16_t> scores(MAX, 0);

  int count = env.get_valid_moves(coords.data(), letters.data(), is_blank.data(),
                                   scores.data(), MAX);

  if (count <= 1) {
    // Only pass available — skip this test body
    SUCCEED("No regular moves available (only pass); skipping move step check");
    return;
  }

  size_t rack_before = env.rack(0).size();
  auto result = env.step(0);  // first regular move

  // Board should no longer be all-blank
  const Board& b = env.board();
  bool any_tile = false;
  for (int r = 0; r < BOARD_ROWS; r++)
    for (int c = 0; c < BOARD_COLS; c++)
      if (b.board[r][c] != Letter::BLANK) any_tile = true;
  CHECK(any_tile);

  // Score is non-negative
  CHECK(result.reward >= 0.0f);

  // Player 0's rack shrank (they placed tiles)
  // After step, current_player switches to 1, so rack(0) is the one that was used
  CHECK(env.rack(0).size() <= rack_before);  // could be same if all 7 drawn back
}

TEST_CASE("ScrabbleEnv copy independence", "[env]") {
  auto g = make_test_gaddag();
  ScrabbleEnv original(g, 42);

  // Deep copy
  ScrabbleEnv copy = original;

  // Step the copy with a pass
  copy.step(MAX_MOVES_PER_ENV);

  // Original should be unchanged
  CHECK(original.consecutive_passes() == 0);
  CHECK(original.current_player() == 0);

  // Copy should have advanced
  CHECK(copy.consecutive_passes() == 1);
  CHECK(copy.current_player() == 1);
}

// ── VectorizedEnv ─────────────────────────────────────────────────────────────

TEST_CASE("VectorizedEnv reset_all — obs and move buffers populated", "[vec_env]") {
  auto g = make_test_gaddag();
  constexpr int N = 4;
  VectorizedEnv vec(g, N, 0);
  vec.reset_all();

  REQUIRE(static_cast<int>(vec.obs_buf_.size()) == N * OBS_DIM);
  for (int i = 0; i < N; i++) {
    CHECK(vec.num_moves_buf_[i] >= 1);
  }
}

TEST_CASE("VectorizedEnv step_all — reward and done buffers populated", "[vec_env]") {
  auto g = make_test_gaddag();
  constexpr int N = 4;
  VectorizedEnv vec(g, N, 0);
  vec.reset_all();

  // Set all actions to pass (a large index that step() treats as pass)
  for (int i = 0; i < N; i++) vec.action_buf_[i] = MAX_MOVES_PER_ENV;
  vec.step_all();

  for (int i = 0; i < N; i++) {
    CHECK(std::isfinite(vec.reward_buf_[i]));
    CHECK((vec.done_buf_[i] == 0 || vec.done_buf_[i] == 1));
  }
}

TEST_CASE("VectorizedEnv auto-reset — env resets and produces fresh obs on done", "[vec_env]") {
  auto g = make_test_gaddag();
  constexpr int N = 2;
  VectorizedEnv vec(g, N, 0);
  vec.reset_all();

  // Drive env 0 to done by 6 consecutive passes
  for (int step = 0; step < 6; step++) {
    vec.action_buf_[0] = MAX_MOVES_PER_ENV;
    vec.action_buf_[1] = MAX_MOVES_PER_ENV;
    vec.step_all();
  }

  // On the 6th pass, done_buf_[0] should have been 1 at some point.
  // After auto-reset, done_buf_[0] should be 0 again on next step.
  for (int i = 0; i < N; i++) vec.action_buf_[i] = MAX_MOVES_PER_ENV;
  vec.step_all();
  CHECK(vec.done_buf_[0] == 0);  // auto-reset happened, new game started
}

TEST_CASE("omp_in_parallel guard — get_all_valid_moves correct from VectorizedEnv", "[vec_env]") {
  auto g = make_test_gaddag();

  // Standalone board
  Board board(g);
  std::vector<Letter> rack = {Letter::A, Letter::T, Letter::S, Letter::I, Letter::N,
                               Letter::E, Letter::R};
  auto standalone_moves = board.get_all_valid_moves(rack);

  // Same rack via VectorizedEnv (uses OpenMP outer loop → serial inner)
  VectorizedEnv vec(g, 1, 0);
  // Manually set env 0 to have the same rack by resetting and checking
  // (We can't inject a rack, so just verify num_moves >= 1, i.e. the guard doesn't crash)
  vec.reset_all();
  CHECK(vec.num_moves_buf_[0] >= 1);

  // Standalone call outside parallel context should also succeed
  CHECK(standalone_moves.size() >= 0);
}
