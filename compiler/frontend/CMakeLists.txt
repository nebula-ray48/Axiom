# -------------------------------------------------------------
# Axiom Frontend Library: Syntax, Parsing, Type Checking (C++23)
# -------------------------------------------------------------
file(GLOB_RECURSE AXIOM_FRONTEND_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/axiom/*.h
)

file(GLOB_RECURSE AXIOM_FRONTEND_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(axiom-frontend STATIC
    ${AXIOM_FRONTEND_HEADERS}
    ${AXIOM_FRONTEND_SOURCES}
)

target_include_directories(axiom-frontend PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(axiom-frontend PUBLIC
    axiom-common
    axiom-tree-sitter
)

set_axiom_warnings(axiom-frontend)

