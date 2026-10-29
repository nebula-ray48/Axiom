# -------------------------------------------------------------
# Axiom Common / Base Foundation Library (C++23)
# -------------------------------------------------------------
file(GLOB_RECURSE AXIOM_COMMON_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/axiom/*.h
)

file(GLOB_RECURSE AXIOM_COMMON_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(axiom-common STATIC
    ${AXIOM_COMMON_HEADERS}
    ${AXIOM_COMMON_SOURCES}
)

target_include_directories(axiom-common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(axiom-common PUBLIC
    axiom-tree-sitter
)

set_axiom_warnings(axiom-common)
