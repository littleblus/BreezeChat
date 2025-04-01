cmake_minimum_required(VERSION 3.10)
project(file_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# proto文件
set(proto_files base.proto file.proto)
foreach(proto_file ${proto_files})
    string(REGEX REPLACE "\.proto$" ".pb.cc" proto_src "${proto_file}")
    string(REGEX REPLACE "\.proto$" ".pb.h" proto_hdr "${proto_file}")
    list(APPEND proto_srcs ${CMAKE_BINARY_DIR}/proto/${proto_src})
    list(APPEND proto_hdrs ${CMAKE_BINARY_DIR}/proto/${proto_hdr})
endforeach()

# 添加可执行文件
add_executable(file_server src/file_server.cpp ${proto_srcs})

# 链接依赖库
target_link_libraries(file_server
    PRIVATE
    gflags
    spdlog
    fmt
    etcd-cpp-api
    cpprest
    jsoncpp
    brpc
    /usr/local/openssl-3.0.16/lib64/libssl.so.3
    /usr/local/openssl-3.0.16/lib64/libcrypto.so.3
    protobuf
    leveldb
    pthread
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_executable(file_client test/file_client.cpp ${proto_srcs})

    target_link_libraries(file_client
        PRIVATE
        gflags
        gtest
        spdlog
        fmt
        etcd-cpp-api
        cpprest
        jsoncpp
        brpc
        /usr/local/openssl-3.0.16/lib64/libssl.so.3
        /usr/local/openssl-3.0.16/lib64/libcrypto.so.3
        protobuf
        leveldb
        pthread
    )
endif()

# 包含头文件目录
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/proto
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../third/include
)

# 将项目目录的config.conf文件复制到build目录
configure_file(${CMAKE_SOURCE_DIR}/server/conf/file.conf ${CMAKE_CURRENT_BINARY_DIR}/file.conf COPYONLY)
