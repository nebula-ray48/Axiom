cmake_minimum_required(VERSION 3.24)
project(Axiom LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CompilerWarnings.cmake)

include(FetchContent)
FetchContent_Declare(
    tree-sitter
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
    GIT_TAG master
)
FetchContent_MakeAvailable(tree-sitter)

enable_testing()

add_subdirectory(compiler)
add_subdirectory(tests)
# TODO lsp と stdlib は実装時に追加する

