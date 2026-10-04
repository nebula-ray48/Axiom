set(PARSER_SRC ${CMAKE_SOURCE_DIR}/grammar/tree-sitter-axiom/src/parser.c
        include/axiom/base/compiler.h
        include/axiom/base/registry.h
        src/syntax/analyzer.cpp
        include/axiom/syntax/analyzer.h
        include/axiom/sema/type_system.h
        include/axiom/sema/symbol_table.h
        src/sema/symbol_table.cpp
        include/axiom/sema/type_checker.h
        src/sema/type_checker.cpp)

add_library(axiom-core STATIC
    ${PARSER_SRC}
    src/base/compiler.cpp
    ${tree-sitter_SOURCE_DIR}/lib/src/lib.c
)

target_include_directories(axiom-core PUBLIC
    include
    ${tree-sitter_SOURCE_DIR}/lib/include
)

set_axiom_warnings(axiom-core)

add_subdirectory(driver)