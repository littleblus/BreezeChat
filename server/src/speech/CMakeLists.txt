cmake_minimum_required(VERSION 3.10)
project(speech_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 添加可执行文件
add_executable(speech_server src/speech_server.cpp)

# 链接依赖库
target_link_libraries(speech_server
    PRIVATE
    proto_objs
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
    add_executable(speech_client test/speech_client.cpp)

    target_link_libraries(speech_client
        PRIVATE
        proto_objs
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
endif()

# 包含头文件目录
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../third/include
)

# 将项目目录的config.conf文件复制到build目录
configure_file(${CMAKE_SOURCE_DIR}/server/conf/speech.conf ${CMAKE_CURRENT_BINARY_DIR}/speech.conf COPYONLY)
