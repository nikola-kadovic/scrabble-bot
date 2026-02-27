#include "scrabble/board.hpp"
#include "scrabble/gaddag.hpp"
#include "scrabble/letter.hpp"
#include "scrabble/move.hpp"
#include "scrabble/point.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace scrabble;

PYBIND11_MODULE(_cpp_ext, m) {
  m.doc() = "C++ core for scrabble-bot";

  // ── Constants ──────────────────────────────────────────────────────────────
  m.attr("DELIMETER") = DELIMITER; // expose with the Python typo spelling
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
      .def("__str__",
           [](Letter l) { return std::string(1, letter_to_char(l)); })
      .def("__repr__",
           [](Letter l) { return std::string("Letter.") + letter_to_char(l); });

  // LETTER_SCORES dict (Letter → int)
  py::dict letter_scores;
  for (int i = 0; i <= 26; i++) {
    Letter l = static_cast<Letter>(i);
    letter_scores[py::cast(l)] = LETTER_SCORES[i];
  }
  m.attr("LETTER_SCORES") = letter_scores;

  m.def("get_letter_score", &get_letter_score, py::arg("letter"));
  m.def("get_all_letters", &get_all_letters);
  m.def("letter_to_char",
        [](Letter l) { return std::string(1, letter_to_char(l)); });
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
      .def("__repr__", [](const Point &p) {
        return "Point(" + std::to_string(p.row) + ", " +
               std::to_string(p.col) + ")";
      });

  py::class_<Move>(m, "Move")
      .def(py::init<Point, Point, std::vector<Letter>, int>(),
           py::arg("start"), py::arg("end"), py::arg("letters"),
           py::arg("score"))
      .def_readwrite("start", &Move::start)
      .def_readwrite("end", &Move::end)
      .def_readwrite("letters", &Move::letters)
      .def_readwrite("score", &Move::score)
      .def("__repr__", [](const Move &mv) {
        return "Move(start=(" + std::to_string(mv.start.row) + "," +
               std::to_string(mv.start.col) + "), end=(" +
               std::to_string(mv.end.row) + "," +
               std::to_string(mv.end.col) +
               "), score=" + std::to_string(mv.score) + ")";
      });

  // ── State ──────────────────────────────────────────────────────────────────
  // Bind with shared_ptr holder so Gaddag.root (a shared_ptr<State>) works
  // seamlessly.
  py::class_<State, std::shared_ptr<State>>(m, "State")
      .def(py::init<>())
      .def_readwrite("arcs", &State::arcs)
      .def_readwrite("letters_that_make_a_word",
                     &State::letters_that_make_a_word)
      .def(
          "add_arc",
          [](State &self, const std::string &letter, py::object dest) {
            std::shared_ptr<State> dest_ptr = nullptr;
            if (!dest.is_none()) {
              dest_ptr = dest.cast<std::shared_ptr<State>>();
            }
            return self.add_arc(letter, dest_ptr);
          },
          py::arg("letter"), py::arg("destination_state") = py::none())
      .def(
          "add_ending_arc",
          [](State &self, const std::string &letter,
             const std::string &ending_letter) {
            if (ending_letter.empty())
              throw std::invalid_argument("ending_letter must not be empty");
            return self.add_ending_arc(letter, ending_letter[0]);
          },
          py::arg("letter"), py::arg("ending_letter"))
      .def(
          "add_ending_letter",
          [](State &self, const std::string &letter) {
            if (letter.empty())
              throw std::invalid_argument("letter must not be empty");
            self.add_ending_letter(letter[0]);
          },
          py::arg("letter"))
      .def(
          "get_next_state",
          [](const State &self, const std::string &letter) -> py::object {
            auto next = self.get_next_state(letter);
            if (!next)
              return py::none();
            return py::cast(next);
          },
          py::arg("letter"));

  // ── Arc ────────────────────────────────────────────────────────────────────
  py::class_<Arc>(m, "Arc")
      .def(py::init<>())
      .def(py::init(
               [](py::object /*letters_that_make_a_word*/, py::object dest) {
                 if (dest.is_none())
                   return Arc{};
                 return Arc(dest.cast<std::shared_ptr<State>>());
               }),
           py::arg("letters_that_make_a_word") = py::none(),
           py::arg("destination_state") = py::none())
      .def_readwrite("destination_state", &Arc::destination_state);

  // ── Gaddag ─────────────────────────────────────────────────────────────────
  py::class_<Gaddag, std::shared_ptr<Gaddag>>(m, "Gaddag")
      .def(py::init<>())
      .def_readwrite("root", &Gaddag::root)
      .def("build_from_words", &Gaddag::build_from_words, py::arg("words"))
      .def("build_from_file", &Gaddag::build_from_file,
           py::arg("wordlist_path"), py::arg("use_cache") = true)
      .def("add_word", &Gaddag::add_word, py::arg("word"))
      .def("get_dictionary_letters", &Gaddag::get_dictionary_letters);

  // ── Board ──────────────────────────────────────────────────────────────────
  py::class_<Board>(m, "Board")
      .def(py::init<std::shared_ptr<Gaddag>>(), py::arg("dictionary"))
      // board: expose as list[list[Letter]]
      .def_property_readonly("board",
                             [](const Board &b) {
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
                             [](const Board &b) {
                               py::dict d;
                               for (int r = 0; r < BOARD_ROWS; r++) {
                                 for (int c = 0; c < BOARD_COLS; c++) {
                                   d[py::make_tuple(r, c)] =
                                       b.square_types[r][c];
                                 }
                               }
                               return d;
                             })
      // _anchor_points: expose as set[tuple[int,int]]
      .def_property_readonly("_anchor_points",
                             [](const Board &b) {
                               py::set s;
                               for (const auto &p : b.anchor_points) {
                                 s.add(py::make_tuple(p.row, p.col));
                               }
                               return s;
                             })
      // _horizontal_cross_checks: list[list[set[Letter]]]
      .def_property_readonly("_horizontal_cross_checks",
                             [](const Board &b) {
                               py::list rows;
                               for (int r = 0; r < BOARD_ROWS; r++) {
                                 py::list row;
                                 for (int c = 0; c < BOARD_COLS; c++) {
                                   py::set s;
                                   for (Letter l :
                                        b.horizontal_cross_checks[r][c]) {
                                     s.add(l);
                                   }
                                   row.append(s);
                                 }
                                 rows.append(row);
                               }
                               return rows;
                             })
      // _vertical_cross_checks: list[list[set[Letter]]]
      .def_property_readonly("_vertical_cross_checks",
                             [](const Board &b) {
                               py::list rows;
                               for (int r = 0; r < BOARD_ROWS; r++) {
                                 py::list row;
                                 for (int c = 0; c < BOARD_COLS; c++) {
                                   py::set s;
                                   for (Letter l :
                                        b.vertical_cross_checks[r][c]) {
                                     s.add(l);
                                   }
                                   row.append(s);
                                 }
                                 rows.append(row);
                               }
                               return rows;
                             })
      .def(
          "place_word",
          [](Board &b, const std::vector<Letter> &word,
             py::tuple starting_point, bool vertical) {
            int row = starting_point[0].cast<int>();
            int col = starting_point[1].cast<int>();
            int score = b.place_word(word, {row, col}, vertical);
            return score;
          },
          py::arg("word"), py::arg("starting_point"), py::arg("vertical"))
      .def(
          "calculate_score",
          [](const Board &b, const std::vector<Letter> &word,
             py::tuple starting_point, bool vertical) {
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
          [](const Board &b, const std::vector<Letter> &rack) {
            return b.get_all_valid_moves(rack);
          },
          py::arg("rack"),
          "Returns all valid moves for the given rack as a list of Move "
          "objects.");

  // ── Free helpers ───────────────────────────────────────────────────────────
  m.def(
      "in_bounds",
      [](py::tuple point) {
        int r = point[0].cast<int>();
        int c = point[1].cast<int>();
        return r >= 0 && r < BOARD_ROWS && c >= 0 && c < BOARD_COLS;
      },
      py::arg("point"));
}
