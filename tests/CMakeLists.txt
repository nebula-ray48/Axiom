include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(axiom-test unit/test_compiler.cpp)

target_link_libraries(axiom-test PRIVATE axiom-core GTest::gtest_main)

set_axiom_warnings(axiom-test)

include(GoogleTest)
gtest_discover_tests(axiom-test)