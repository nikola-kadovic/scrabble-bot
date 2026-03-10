#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "scrabble/board.hpp"
#include "scrabble/env.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/letter.hpp"
#include "scrabble/move.hpp"
#include "scrabble/point.hpp"
#include "scrabble/vec_env.hpp"

namespace py = pybind11;
using namespace scrabble;

PYBIND11_MODULE(_cpp_ext, m) {
  m.doc() = "C++ core for scrabble-bot";

  // ── Constants ──────────────────────────────────────────────────────────────
  m.attr("DELIMETER") = DELIMITER;  // expose with the Python typo spelling
  m.attr("DELIMITER") = DELIMITER;
  m.attr("BOARD_ROWS") = BOARD_ROWS;
  m.attr("BOARD_COLS") = BOARD_COLS;

  // ── Letter enum ────────────────────────────────────────────────────────────
  py::enum_<Letter>(m, "Letter")
      .value("A", Letter::A)
      .value("B", Letter::B)
      .value("C", Letter::C)
      .value("D", Letter::D)
      .value("E", Letter::E)
      .value("F", Letter::F)
      .value("G", Letter::G)
      .value("H", Letter::H)
      .value("I", Letter::I)
      .value("J", Letter::J)
      .value("K", Letter::K)
      .value("L", Letter::L)
      .value("M", Letter::M)
      .value("N", Letter::N)
      .value("O", Letter::O)
      .value("P", Letter::P)
      .value("Q", Letter::Q)
      .value("R", Letter::R)
      .value("S", Letter::S)
      .value("T", Letter::T)
      .value("U", Letter::U)
      .value("V", Letter::V)
      .value("W", Letter::W)
      .value("X", Letter::X)
      .value("Y", Letter::Y)
      .value("Z", Letter::Z)
      .value("BLANK", Letter::BLANK)
      .export_values()
      .def("__str__", [](Letter l) { return std::string(1, letter_to_char(l)); })
      .def("__repr__", [](Letter l) { return std::string("Letter.") + letter_to_char(l); });

  // LETTER_SCORES dict (Letter → int)
  py::dict letter_scores;
  for (int i = 0; i <= 26; i++) {
    Letter l = static_cast<Letter>(i);
    letter_scores[py::cast(l)] = LETTER_SCORES[i];
  }
  m.attr("LETTER_SCORES") = letter_scores;

  m.def("get_letter_score", &get_letter_score, py::arg("letter"));
  m.def("get_all_letters", &get_all_letters);
  m.def("letter_to_char", [](Letter l) { return std::string(1, letter_to_char(l)); });
  m.def("char_to_letter", &char_to_letter, py::arg("c"));

  // ── SquareType enum ────────────────────────────────────────────────────────
  py::enum_<SquareType>(m, "SquareType")
      .value("DEFAULT", SquareType::DEFAULT)
      .value("DOUBLE_WORD", SquareType::DOUBLE_WORD)
      .value("TRIPLE_WORD", SquareType::TRIPLE_WORD)
      .value("DOUBLE_LETTER", SquareType::DOUBLE_LETTER)
      .value("TRIPLE_LETTER", SquareType::TRIPLE_LETTER)
      .export_values()
      .def("__str__", [](SquareType t) -> std::string {
        switch (t) {
          case SquareType::DEFAULT:
            return ".";
          case SquareType::DOUBLE_WORD:
            return "w";
          case SquareType::TRIPLE_WORD:
            return "W";
          case SquareType::DOUBLE_LETTER:
            return "l";
          case SquareType::TRIPLE_LETTER:
            return "L";
          default:
            return "?";
        }
      });

  // ── Move ───────────────────────────────────────────────────────────────────
  py::class_<Point>(m, "Point")
      .def(py::init<int, int>())
      .def_readwrite("row", &Point::row)
      .def_readwrite("col", &Point::col)
      .def("__repr__", [](const Point& p) {
        return "Point(" + std::to_string(p.row) + ", " + std::to_string(p.col) + ")";
      });

  py::class_<Move>(m, "Move")
      .def(py::init<Point, Point, std::vector<Letter>, std::vector<bool>, int>(), py::arg("start"),
           py::arg("end"), py::arg("letters"), py::arg("is_blank"), py::arg("score"))
      .def_readwrite("start", &Move::start)
      .def_readwrite("end", &Move::end)
      .def_readwrite("letters", &Move::letters)
      .def_readwrite("is_blank", &Move::is_blank)
      .def_readwrite("score", &Move::score)
      .def("__repr__", [](const Move& mv) {
        return "Move(start=(" + std::to_string(mv.start.row) + "," + std::to_string(mv.start.col) +
               "), end=(" + std::to_string(mv.end.row) + "," + std::to_string(mv.end.col) +
               "), score=" + std::to_string(mv.score) + ")";
      });

  // ── State ──────────────────────────────────────────────────────────────────
  // Bind with shared_ptr holder for Python lifetime management.
  // C++ traversal uses raw State* in arcs[]; Python_children keeps arena-less
  // States alive when add_arc is called on a standalone Python State object.
  py::class_<State, std::shared_ptr<State>>(m, "State")
      .def(py::init<>())
      .def_property_readonly("arcs",
                             [](const State& self) {
                               py::dict d;
                               for (int i = 0; i < 26; i++) {
                                 if (self.arcs[i]) {
                                   std::string key(1, static_cast<char>('A' + i));
                                   d[py::str(key)] = Arc(self.arcs[i]);
                                 }
                               }
                               if (self.arcs[26]) {
                                 d[py::str("_")] = Arc(self.arcs[26]);
                               }
                               if (self.arcs[DELIMITER_ARC_INDEX]) {
                                 d[py::str(DELIMITER)] = Arc(self.arcs[DELIMITER_ARC_INDEX]);
                               }
                               return d;
                             })
      .def_property_readonly("letters_that_make_a_word",
                             [](const State& self) {
                               py::set s;
                               for (int i = 0; i < 26; i++) {
                                 if (ltmaw_has(self.letters_that_make_a_word, i)) {
                                   s.add(py::str(std::string(1, static_cast<char>('A' + i))));
                                 }
                               }
                               return s;
                             })
      .def(
          "add_arc",
          [](State& self, const std::string& letter, const py::object& dest) -> State* {
            // Compute arc index from the letter key.
            int idx;
            if (letter == DELIMITER) {
              idx = DELIMITER_ARC_INDEX;
            } else {
              char c = letter[0];
              idx = (c == '_') ? 26 : (c - 'A');
            }
            if (!self.arcs[idx]) {
              std::shared_ptr<State> child;
              if (!dest.is_none()) {
                child = dest.cast<std::shared_ptr<State>>();
              } else {
                child = std::make_shared<State>();
              }
              // Keep child alive as long as this State is alive.
              self.python_children.push_back(child);
              self.arcs[idx] = child.get();
            }
            return self.arcs[idx];
          },
          py::arg("letter"), py::arg("destination_state") = py::none(),
          py::return_value_policy::reference)
      .def(
          "add_ending_arc",
          [](State& self, const std::string& letter, const std::string& ending_letter) -> State* {
            if (ending_letter.empty())
              throw std::invalid_argument("ending_letter must not be empty");
            int idx;
            if (letter == DELIMITER) {
              idx = DELIMITER_ARC_INDEX;
            } else {
              char c = letter[0];
              idx = (c == '_') ? 26 : (c - 'A');
            }
            if (!self.arcs[idx]) {
              auto child = std::make_shared<State>();
              self.python_children.push_back(child);
              self.arcs[idx] = child.get();
            }
            self.arcs[idx]->letters_that_make_a_word =
                ltmaw_add(self.arcs[idx]->letters_that_make_a_word, ending_letter[0] - 'A');
            return self.arcs[idx];
          },
          py::arg("letter"), py::arg("ending_letter"), py::return_value_policy::reference)
      .def(
          "add_ending_letter",
          [](State& self, const std::string& letter) {
            if (letter.empty()) throw std::invalid_argument("letter must not be empty");
            self.add_ending_letter(letter[0]);
          },
          py::arg("letter"))
      .def(
          "get_next_state",
          [](const State& self, const std::string& letter) -> py::object {
            const State* next = self.get_next_state(letter);
            if (!next) return py::none();
            return py::cast(next, py::return_value_policy::reference);
          },
          py::arg("letter"));

  // ── Arc ────────────────────────────────────────────────────────────────────
  py::class_<Arc>(m, "Arc")
      .def(py::init<>())
      .def(py::init([](const py::object& /*letters_that_make_a_word*/, const py::object& dest) {
             if (dest.is_none()) return Arc{};
             return Arc(dest.cast<State*>());
           }),
           py::arg("letters_that_make_a_word") = py::none(),
           py::arg("destination_state") = py::none())
      .def_readwrite("destination_state", &Arc::destination_state);

  // ── Gaddag ─────────────────────────────────────────────────────────────────
  py::class_<Gaddag, std::shared_ptr<Gaddag>>(m, "Gaddag")
      .def(py::init<>())
      .def_readwrite("root", &Gaddag::root)
      .def("build_from_words", &Gaddag::build_from_words, py::arg("words"))
      .def("build_from_file", &Gaddag::build_from_file, py::arg("wordlist_path"),
           py::arg("use_cache") = true)
      .def("add_word", &Gaddag::add_word, py::arg("word"))
      .def("get_dictionary_letters", &Gaddag::get_dictionary_letters);

  // ── Board ──────────────────────────────────────────────────────────────────
  py::class_<Board>(m, "Board")
      .def(py::init<std::shared_ptr<Gaddag>>(), py::arg("dictionary"))
      // board: expose as list[list[Letter]]
      .def_property_readonly("board",
                             [](const Board& b) {
                               py::list rows;
                               for (int r = 0; r < BOARD_ROWS; r++) {
                                 py::list row;
                                 for (int c = 0; c < BOARD_COLS; c++) {
                                   row.append(b.board[r][c]);
                                 }
                                 rows.append(row);
                               }
                               return rows;
                             })
      // square_types: expose as dict[(int,int), SquareType]
      .def_property_readonly("square_types",
                             [](const Board& b) {
                               py::dict d;
                               for (int r = 0; r < BOARD_ROWS; r++) {
                                 for (int c = 0; c < BOARD_COLS; c++) {
                                   d[py::make_tuple(r, c)] = b.square_types[r][c];
                                 }
                               }
                               return d;
                             })
      // _anchor_points: expose as set[tuple[int,int]]
      .def_property_readonly("_anchor_points",
                             [](const Board& b) {
                               py::set s;
                               for (int r = 0; r < BOARD_ROWS; r++)
                                 for (int c = 0; c < BOARD_COLS; c++)
                                   if (b.anchor_points[r][c]) s.add(py::make_tuple(r, c));
                               return s;
                             })
      // _horizontal_cross_checks: list[list[set[Letter]]]
      .def_property_readonly("_horizontal_cross_checks",
                             [](const Board& b) {
                               py::list rows;
                               for (int r = 0; r < BOARD_ROWS; r++) {
                                 py::list row;
                                 for (int c = 0; c < BOARD_COLS; c++) {
                                   py::set s;
                                   CrossCheckSet mask = b.horizontal_cross_checks[r][c];
                                   for (int i = 0; i < 26; i++) {
                                     if ((mask >> i) & 1u) s.add(static_cast<Letter>(i));
                                   }
                                   row.append(s);
                                 }
                                 rows.append(row);
                               }
                               return rows;
                             })
      // _vertical_cross_checks: list[list[set[Letter]]]
      .def_property_readonly("_vertical_cross_checks",
                             [](const Board& b) {
                               py::list rows;
                               for (int r = 0; r < BOARD_ROWS; r++) {
                                 py::list row;
                                 for (int c = 0; c < BOARD_COLS; c++) {
                                   py::set s;
                                   CrossCheckSet mask = b.vertical_cross_checks[r][c];
                                   for (int i = 0; i < 26; i++) {
                                     if ((mask >> i) & 1u) s.add(static_cast<Letter>(i));
                                   }
                                   row.append(s);
                                 }
                                 rows.append(row);
                               }
                               return rows;
                             })
      .def(
          "place_word",
          [](Board& b, const std::vector<Letter>& word, const py::tuple& start,
             const py::tuple& end) {
            return b.place_word(word, Point{start[0].cast<int>(), start[1].cast<int>()},
                                Point{end[0].cast<int>(), end[1].cast<int>()});
          },
          py::arg("word"), py::arg("start"), py::arg("end"))
      .def(
          "place_word", [](Board& b, const Move& move) { return b.place_word(move); },
          py::arg("move"))
      .def(
          "calculate_score",
          [](const Board& b, const std::vector<Letter>& word, const py::tuple& starting_point,
             bool vertical) {
            int row = starting_point[0].cast<int>();
            int col = starting_point[1].cast<int>();
            return b.calculate_score(word, row, col, vertical);
          },
          py::arg("word"), py::arg("starting_point"), py::arg("vertical"))
      .def("__str__", &Board::to_string)
      .def("validate_board", &Board::validate_board,
           "Scan every row and column for contiguous tile runs and return a "
           "list of invalid word strings. Empty list means the board is fully "
           "valid.")
      .def(
          "get_all_valid_moves",
          [](const Board& b, const std::vector<Letter>& rack) {
            return b.get_all_valid_moves(rack);
          },
          py::arg("rack"),
          "Returns all valid moves for the given rack as a list of Move "
          "objects.");

  // ── Free helpers ───────────────────────────────────────────────────────────
  m.def(
      "in_bounds",
      [](const py::tuple& point) {
        int r = point[0].cast<int>();
        int c = point[1].cast<int>();
        return r >= 0 && r < BOARD_ROWS && c >= 0 && c < BOARD_COLS;
      },
      py::arg("point"));

  // ── RL environment constants ───────────────────────────────────────────────
  m.attr("OBS_DIM") = OBS_DIM;
  m.attr("MAX_MOVES_PER_ENV") = MAX_MOVES_PER_ENV;

  // ── ScrabbleEnv ────────────────────────────────────────────────────────────
  py::class_<ScrabbleEnv::StepResult>(m, "StepResult")
      .def_readonly("reward", &ScrabbleEnv::StepResult::reward)
      .def_readonly("done", &ScrabbleEnv::StepResult::done)
      .def("__repr__", [](const ScrabbleEnv::StepResult& r) {
        return "StepResult(reward=" + std::to_string(r.reward) +
               ", done=" + (r.done ? "True" : "False") + ")";
      });

  py::class_<ScrabbleEnv>(m, "ScrabbleEnv")
      .def(py::init<std::shared_ptr<Gaddag>, uint32_t>(),
           py::arg("dict"), py::arg("seed") = 0u)
      .def(py::init<const ScrabbleEnv&>())  // copy constructor — for MCTS node cloning
      .def("reset", &ScrabbleEnv::reset)
      .def("step", &ScrabbleEnv::step, py::arg("action_idx"))
      .def("get_valid_moves", &ScrabbleEnv::get_all_valid_moves_py,
           "Returns list of Move objects (including pass sentinel as last element).")
      .def_property_readonly("done", &ScrabbleEnv::done)
      .def_property_readonly("current_player", &ScrabbleEnv::current_player)
      .def_property_readonly("consecutive_passes", &ScrabbleEnv::consecutive_passes)
      .def_property_readonly("bag_remaining", &ScrabbleEnv::bag_remaining)
      .def_property_readonly(
          "player_scores",
          [](const ScrabbleEnv& e) {
            return py::make_tuple(e.player_scores()[0], e.player_scores()[1]);
          })
      .def_property_readonly(
          "rack",
          [](const ScrabbleEnv& e) {
            // Returns current player's rack as a list of Letter
            py::list r;
            for (Letter l : e.rack(e.current_player())) r.append(l);
            return r;
          });

  // ── VectorizedEnv ─────────────────────────────────────────────────────────
  // Helper: expose a flat C++ vector as a typed numpy array with explicit shape.
  // The array holds a reference to `self` (the VectorizedEnv) to prevent GC.
  auto as_array = [](auto& self, auto& vec, std::vector<ssize_t> shape) {
    using T = typename std::remove_reference_t<decltype(vec)>::value_type;
    std::vector<ssize_t> strides(shape.size());
    strides.back() = sizeof(T);
    for (int i = static_cast<int>(shape.size()) - 2; i >= 0; i--)
      strides[i] = strides[i + 1] * shape[i + 1];
    return py::array_t<T>(shape, strides, vec.data(), py::cast(self));
  };

  py::class_<VectorizedEnv>(m, "VectorizedEnv")
      .def(py::init<const std::shared_ptr<Gaddag>&, int, uint32_t>(),
           py::arg("dict"), py::arg("num_envs"), py::arg("seed") = 0u)
      .def("reset_all", &VectorizedEnv::reset_all)
      .def("step_all", &VectorizedEnv::step_all,
           py::call_guard<py::gil_scoped_release>())
      .def_property_readonly("num_envs", &VectorizedEnv::num_envs)
      .def_property_readonly("observations",
                             [as_array](VectorizedEnv& s) {
                               return as_array(s, s.obs_buf_,
                                              {s.num_envs(), OBS_DIM});
                             })
      .def_property_readonly("actions",
                             [as_array](VectorizedEnv& s) {
                               return as_array(s, s.action_buf_, {s.num_envs()});
                             })
      .def_property_readonly("rewards",
                             [as_array](VectorizedEnv& s) {
                               return as_array(s, s.reward_buf_, {s.num_envs()});
                             })
      .def_property_readonly("dones",
                             [as_array](VectorizedEnv& s) {
                               return as_array(s, s.done_buf_, {s.num_envs()});
                             })
      .def_property_readonly("num_moves",
                             [as_array](VectorizedEnv& s) {
                               return as_array(s, s.num_moves_buf_, {s.num_envs()});
                             })
      .def_property_readonly("move_coords",
                             [as_array](VectorizedEnv& s) {
                               return as_array(s, s.move_coords_,
                                              {s.num_envs(), MAX_MOVES_PER_ENV, 4});
                             })
      .def_property_readonly("move_letters",
                             [as_array](VectorizedEnv& s) {
                               return as_array(s, s.move_letters_,
                                              {s.num_envs(), MAX_MOVES_PER_ENV, 7});
                             })
      .def_property_readonly("move_is_blank",
                             [as_array](VectorizedEnv& s) {
                               return as_array(s, s.move_is_blank_,
                                              {s.num_envs(), MAX_MOVES_PER_ENV, 7});
                             })
      .def_property_readonly("move_scores",
                             [as_array](VectorizedEnv& s) {
                               return as_array(s, s.move_scores_,
                                              {s.num_envs(), MAX_MOVES_PER_ENV});
                             });
}
