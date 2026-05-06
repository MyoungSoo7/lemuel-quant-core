set(JUDGE_SOURCES
    src/main.cpp
    src/sandbox.cpp
    src/runner.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND JUDGE_SOURCES src/sandbox_linux.cpp)
endif()

add_executable(judge_engine ${JUDGE_SOURCES})

target_include_directories(judge_engine PRIVATE include)
target_link_libraries(judge_engine PRIVATE lqc_shared)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_library(SECCOMP_LIB seccomp)
    if(SECCOMP_LIB)
        target_link_libraries(judge_engine PRIVATE ${SECCOMP_LIB})
        target_compile_definitions(judge_engine PRIVATE LQC_HAS_SECCOMP=1)
    endif()
endif()

option(LQC_JUDGE_GRPC "Build judge-engine gRPC server mode" OFF)
if(LQC_JUDGE_GRPC)
    find_package(Protobuf CONFIG QUIET)
    find_package(gRPC CONFIG QUIET)
    if(Protobuf_FOUND AND gRPC_FOUND)
        add_subdirectory(grpc)
    else()
        message(WARNING "LQC_JUDGE_GRPC requested but Protobuf/gRPC not found; skipping")
    endif()
endif()
