add_executable(axc src/main.cpp)

target_link_libraries(axc PRIVATE axiom-core)

set_axiom_warnings(axc)