# Axiom専用の警告設定を行う関数
function(set_axiom_warnings target_name)
    set(CLANG_WARNINGS
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wcast-align
            -Wold-style-cast
    )

    set(GCC_WARNINGS
            ${CLANG_WARNINGS}
            -Wduplicated-cond
            -Wlogical-op
    )

    set(MSVC_WARNINGS
            /W4
            /permissive-
    )

    if(MSVC)
        set(PROJECT_WARNINGS ${MSVC_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
        set(PROJECT_WARNINGS ${CLANG_WARNINGS})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(PROJECT_WARNINGS ${GCC_WARNINGS})
    endif()

    target_compile_options(${target_name} PRIVATE ${PROJECT_WARNINGS})
endfunction()