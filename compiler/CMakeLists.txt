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
set(AXIOM_CORE_HEADERS
    include/axiom/base/compiler.h
    include/axiom/base/diagnostic.h
    include/axiom/base/registry.h
    include/axiom/syntax/analyzer.h
    include/axiom/sema/type_system.h
    include/axiom/sema/symbol_table.h
    include/axiom/sema/type_checker.h
    include/axiom/ir/ir.h
    include/axiom/ir/ir_builder.h
    include/axiom/ir/ir_printer.h
    include/axiom/ir/ir_verifier.h
    include/axiom/codegen/c99_codegen.h
)

set(AXIOM_CORE_SOURCES
    src/base/compiler.cpp
    src/syntax/analyzer.cpp
    src/sema/symbol_table.cpp
    src/sema/type_checker.cpp
    src/ir/ir_builder.cpp
    src/ir/ir_printer.cpp
    src/ir/ir_verifier.cpp
    src/codegen/c99_codegen.cpp
)

add_library(axiom-core STATIC
    ${AXIOM_CORE_HEADERS}
    ${AXIOM_CORE_SOURCES}
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