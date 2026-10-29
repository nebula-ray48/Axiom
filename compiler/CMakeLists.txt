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
# Axiom Subsystems: Common, Frontend (Public), Backend (Core)
# -------------------------------------------------------------
add_subdirectory(common)
add_subdirectory(frontend)
add_subdirectory(backend)

# -------------------------------------------------------------
# Axiom Core Composite Target (Aggregates subsystems)
# -------------------------------------------------------------
add_library(axiom-core INTERFACE)

target_link_libraries(axiom-core INTERFACE
    axiom-common
    axiom-frontend
    axiom-backend
)

# -------------------------------------------------------------
# Compiler CLI Driver
# -------------------------------------------------------------
add_subdirectory(driver)
