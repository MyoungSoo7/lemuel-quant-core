add_executable(judge_engine
    src/main.cpp
    src/sandbox.cpp
    src/runner.cpp
)

target_include_directories(judge_engine PRIVATE include)
target_link_libraries(judge_engine PRIVATE lqc_shared)

if(UNIX AND NOT APPLE)
    find_library(SECCOMP_LIB seccomp)
    if(SECCOMP_LIB)
        target_link_libraries(judge_engine PRIVATE ${SECCOMP_LIB})
        target_compile_definitions(judge_engine PRIVATE LQC_HAS_SECCOMP=1)
    endif()
endif()
