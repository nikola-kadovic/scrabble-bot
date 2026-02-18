#include <catch2/catch_test_macros.hpp>
#include "scrabble/letter.hpp"

using namespace scrabble;

TEST_CASE("letter_to_char returns correct characters", "[letter]") {
    CHECK(letter_to_char(Letter::A) == 'A');
    CHECK(letter_to_char(Letter::Z) == 'Z');
    CHECK(letter_to_char(Letter::BLANK) == '_');
    CHECK(letter_to_char(Letter::M) == 'M');
}

TEST_CASE("char_to_letter returns correct letters", "[letter]") {
    CHECK(char_to_letter('A') == Letter::A);
    CHECK(char_to_letter('Z') == Letter::Z);
    CHECK(char_to_letter('_') == Letter::BLANK);
}

TEST_CASE("letter_to_char and char_to_letter are inverses", "[letter]") {
    for (int i = 0; i < 26; i++) {
        Letter l = static_cast<Letter>(i);
        CHECK(char_to_letter(letter_to_char(l)) == l);
    }
    CHECK(char_to_letter(letter_to_char(Letter::BLANK)) == Letter::BLANK);
}

TEST_CASE("LETTER_SCORES has correct values", "[letter]") {
    CHECK(LETTER_SCORES[static_cast<int>(Letter::A)] == 1);
    CHECK(LETTER_SCORES[static_cast<int>(Letter::E)] == 1);
    CHECK(LETTER_SCORES[static_cast<int>(Letter::Q)] == 10);
    CHECK(LETTER_SCORES[static_cast<int>(Letter::Z)] == 10);
    CHECK(LETTER_SCORES[static_cast<int>(Letter::J)] == 8);
    CHECK(LETTER_SCORES[static_cast<int>(Letter::X)] == 8);
    CHECK(LETTER_SCORES[static_cast<int>(Letter::BLANK)] == 0);
    CHECK(get_letter_score(Letter::K) == 5);
}

TEST_CASE("get_all_letters returns 27 entries", "[letter]") {
    auto letters = get_all_letters();
    CHECK(letters.size() == 27);
    CHECK(letters.count(Letter::A) == 1);
    CHECK(letters.count(Letter::Z) == 1);
    CHECK(letters.count(Letter::BLANK) == 1);
}

TEST_CASE("Letter enum indices are 0-26", "[letter]") {
    CHECK(static_cast<uint8_t>(Letter::A)    == 0);
    CHECK(static_cast<uint8_t>(Letter::Z)    == 25);
    CHECK(static_cast<uint8_t>(Letter::BLANK) == 26);
}
