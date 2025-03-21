cmake_minimum_required(VERSION 3.10)
project(speech_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# proto文件
set(proto_path ${CMAKE_SOURCE_DIR}/server/proto)
set(proto_files speech.proto)

foreach(proto_file ${proto_files})
    string(REGEX REPLACE "\.proto$" ".pb.cc" proto_src "${proto_file}")
    string(REGEX REPLACE "\.proto$" ".pb.h" proto_hdr "${proto_file}")
    if (NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${proto_src})
        message(STATUS "Generate ${proto_src} ${proto_hdr}")
        execute_process(
            COMMAND protoc -I=${proto_path} --experimental_allow_proto3_optional --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${proto_path}/${proto_file}
        )
    endif()
    list(APPEND proto_srcs ${CMAKE_CURRENT_BINARY_DIR}/${proto_src})
    list(APPEND proto_hdrs ${CMAKE_CURRENT_BINARY_DIR}/${proto_hdr})
endforeach()

# 添加可执行文件
add_executable(speech_server src/speech_server.cpp ${proto_srcs})
add_executable(speech_client test/speech_client.cpp ${proto_srcs})

# 链接依赖库
target_link_libraries(speech_server
    PRIVATE
    gflags
    spdlog
    fmt
    etcd-cpp-api
    cpprest
    jsoncpp
    brpc
    ssl
    crypto
    protobuf
    leveldb
    pthread
)

target_link_libraries(speech_client
    PRIVATE
    gflags
    spdlog
    fmt
    etcd-cpp-api
    cpprest
    jsoncpp
    brpc
    ssl
    crypto
    protobuf
    leveldb
    pthread
)

# 包含头文件目录
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../third/include
)

# 将项目目录的config.conf文件复制到build目录
configure_file(${CMAKE_SOURCE_DIR}/server/conf/speech.conf ${CMAKE_CURRENT_BINARY_DIR}/speech.conf COPYONLY)
