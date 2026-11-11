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

set_target_properties(axiom-tree-sitter PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

# Suppress compiler warnings for third-party / generated C sources
if(MSVC)
    target_compile_options(axiom-tree-sitter PRIVATE /std:c11 /w)
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
# Optional Closed-Source Core Extension (Open-Core Hook)
# -------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/private/CMakeLists.txt")
    message(STATUS "[Axiom] Closed-source private core detected: enabling axiom-private")
    add_subdirectory(private)
    target_compile_definitions(axiom-core INTERFACE AXIOM_HAS_PRIVATE_CORE=1)
    target_link_libraries(axiom-core INTERFACE axiom-private)
endif()

# -------------------------------------------------------------
# Compiler CLI Driver
# -------------------------------------------------------------
add_subdirectory(driver)
