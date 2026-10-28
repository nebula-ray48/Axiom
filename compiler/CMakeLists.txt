# -------------------------------------------------------------
# Third-party: Tree-sitter C runtime & generated Axiom parser
# -------------------------------------------------------------
add_library(axiom-tree-sitter STATIC
    ${tree-sitter_SOURCE_DIR}/lib/src/lib.c
    ${CMAKE_SOURCE_DIR}/grammar/tree-sitter-axiom/src/parser.c
)

target_include_directories(axiom-tree-sitter SYSTEM PUBLIC
    ${tree-sitter_SOURCE_DIR}/lib/include
    ${CMAKE_SOURCE_DIR}/grammar/tree-sitter-axiom/src
)

# Suppress compiler warnings for third-party / generated C sources
if(MSVC)
    target_compile_options(axiom-tree-sitter PRIVATE /w)
else()
    target_compile_options(axiom-tree-sitter PRIVATE -w)
endif()

# -------------------------------------------------------------
# Axiom Core Compiler Library (C++23)
# -------------------------------------------------------------
file(GLOB_RECURSE AXIOM_CORE_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/axiom/*.h
)

file(GLOB_RECURSE AXIOM_CORE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(axiom-core STATIC
    ${AXIOM_CORE_HEADERS}
    ${AXIOM_CORE_SOURCES}
        include/axiom/target/arm64.h
        src/target/arm64.cpp
        include/axiom/jit/jit_arena.h
        src/jit/jit_arena.cpp
)

target_include_directories(axiom-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(axiom-core PUBLIC
    axiom-tree-sitter
)

set_axiom_warnings(axiom-core)

add_subdirectory(driver)
