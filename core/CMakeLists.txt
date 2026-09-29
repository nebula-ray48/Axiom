set(PARSER_SRC ${CMAKE_SOURCE_DIR}/tree-sitter-axiom/src/parser.c
        include/axiom/compiler.h
        include/axiom/registry.h
        src/analyzer.cpp
        include/axiom/analyzer.h
        include/axiom/type_system.h
        include/axiom/symbol_table.h
        src/symbol_table.cpp
        include/axiom/type_checker.h
        src/type_checker.cpp)

add_library(axiom-core STATIC
    ${PARSER_SRC}
    src/compiler.cpp
    ${tree-sitter_SOURCE_DIR}/lib/src/lib.c
)

target_include_directories(axiom-core PUBLIC
    include
    ${tree-sitter_SOURCE_DIR}/lib/include
)

set_axiom_warnings(axiom-core)