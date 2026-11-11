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
# Optional Closed-Source Core Extension (Open-Core Hook)
# -------------------------------------------------------------
option(AXIOM_ENABLE_PRIVATE_CORE "Enable closed-source private core if present" ON)

set(AXIOM_PRIVATE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/private")
if(AXIOM_ENABLE_PRIVATE_CORE AND EXISTS "${AXIOM_PRIVATE_DIR}/CMakeLists.txt")
    message(STATUS "[Axiom] Closed-source private core detected: enabling axiom-private")
    add_compile_definitions(AXIOM_HAS_PRIVATE_CORE=1)
    include_directories(${AXIOM_PRIVATE_DIR}/include)
endif()

# -------------------------------------------------------------
# Axiom Subsystems: Common, Frontend (Public)
# -------------------------------------------------------------
add_subdirectory(common)
add_subdirectory(frontend)

# -------------------------------------------------------------
# Axiom Core Composite Target (Aggregates subsystems)
# -------------------------------------------------------------
add_library(axiom-core INTERFACE)

target_link_libraries(axiom-core INTERFACE
    axiom-common
    axiom-frontend
)

if(AXIOM_ENABLE_PRIVATE_CORE AND EXISTS "${AXIOM_PRIVATE_DIR}/CMakeLists.txt")
    add_subdirectory(private)
    target_compile_definitions(axiom-core INTERFACE AXIOM_HAS_PRIVATE_CORE=1)
    target_link_libraries(axiom-core INTERFACE axiom-private)
endif()

# -------------------------------------------------------------
# Compiler CLI Driver
# -------------------------------------------------------------
add_subdirectory(driver)
