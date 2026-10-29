# -------------------------------------------------------------
# Axiom Backend Library: IR, Codegen, Machine Encoders, JIT (C++23)
# -------------------------------------------------------------
file(GLOB_RECURSE AXIOM_BACKEND_HEADERS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/axiom/*.h
)

file(GLOB_RECURSE AXIOM_BACKEND_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_library(axiom-backend STATIC
    ${AXIOM_BACKEND_HEADERS}
    ${AXIOM_BACKEND_SOURCES}
)

target_include_directories(axiom-backend PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(axiom-backend PUBLIC
    axiom-common
)

set_axiom_warnings(axiom-backend)
