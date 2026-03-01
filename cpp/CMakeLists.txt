add_library(scrabble_core STATIC
    src/letter.cpp
    src/gaddag.cpp
    src/board.cpp
    src/move_gen.cpp
)

target_include_directories(scrabble_core PUBLIC include)
target_compile_features(scrabble_core PUBLIC cxx_std_20)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/libomp")
endif()

find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(scrabble_core PUBLIC OpenMP::OpenMP_CXX)
endif()

# On Windows with MSVC, add /utf-8 flag so the ◇ delimiter literal is handled correctly
if (MSVC)
    target_compile_options(scrabble_core PUBLIC /utf-8)
endif()

python_add_library(_cpp_ext MODULE bindings/bindings.cpp WITH_SOABI)
target_link_libraries(_cpp_ext PRIVATE scrabble_core pybind11::headers)
set_target_properties(_cpp_ext PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/scrabble_bot"
)

set_target_properties(scrabble_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (MSVC)
    target_compile_options(_cpp_ext PRIVATE /utf-8)
endif()

install(TARGETS _cpp_ext DESTINATION ${SKBUILD_PLATLIB_DIR}/scrabble_bot)

add_subdirectory(tests)
