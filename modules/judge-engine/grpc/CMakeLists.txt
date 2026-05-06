# gRPC server target. Built only when LQC_JUDGE_GRPC=ON and the find_package
# calls in the parent CMakeLists succeeded.

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/judge.proto)
set(PROTO_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_SRCS ${PROTO_GEN_DIR}/judge.pb.cc)
set(PROTO_HDRS ${PROTO_GEN_DIR}/judge.pb.h)
set(GRPC_SRCS  ${PROTO_GEN_DIR}/judge.grpc.pb.cc)
set(GRPC_HDRS  ${PROTO_GEN_DIR}/judge.grpc.pb.h)

find_program(PROTOC_BIN protoc REQUIRED)
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin REQUIRED)

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
    COMMAND ${PROTOC_BIN}
        --proto_path=${CMAKE_CURRENT_SOURCE_DIR}
        --cpp_out=${PROTO_GEN_DIR}
        --grpc_out=${PROTO_GEN_DIR}
        --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
        ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    VERBATIM
)

add_executable(judge_engine_grpc
    server.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

target_include_directories(judge_engine_grpc PRIVATE
    ${PROTO_GEN_DIR}
    ${CMAKE_SOURCE_DIR}/modules/judge-engine/include
    ${CMAKE_SOURCE_DIR}/shared/include
)

target_link_libraries(judge_engine_grpc PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    lqc_shared
)

# Reuse runner+sandbox sources from the CLI target.
target_sources(judge_engine_grpc PRIVATE
    ${CMAKE_SOURCE_DIR}/modules/judge-engine/src/runner.cpp
    ${CMAKE_SOURCE_DIR}/modules/judge-engine/src/sandbox.cpp
)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(judge_engine_grpc PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/judge-engine/src/sandbox_linux.cpp
    )
endif()
