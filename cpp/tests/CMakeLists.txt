add_executable(scrabble_tests
    test_letter.cpp
    test_move.cpp
    test_gaddag.cpp
    test_board.cpp
)

target_link_libraries(scrabble_tests PRIVATE scrabble_core Catch2::Catch2WithMain)

if (MSVC)
    target_compile_options(scrabble_tests PRIVATE /utf-8)
endif()

include(CTest)
include(Catch)
catch_discover_tests(scrabble_tests)
