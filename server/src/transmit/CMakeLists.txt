cmake_minimum_required(VERSION 3.10)
project(transmit_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 添加可执行文件
add_executable(transmit_server src/transmit_server.cpp)

# 链接依赖库
target_link_libraries(transmit_server
    PRIVATE
    odb_gen
    proto_objs
    gflags
    spdlog
    fmt
    amqpcpp
    ev
    etcd-cpp-api
    cpprest
    brpc
    /usr/local/openssl-3.0.16/lib64/libssl.so.3
    /usr/local/openssl-3.0.16/lib64/libcrypto.so.3
    protobuf
    leveldb
    pthread
    odb-mysql
    odb
)


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_executable(transmit_mysql_test test/mysql_test/test.cpp)
    add_executable(transmit_client test/transmit_client.cpp)

    target_link_libraries(transmit_mysql_test
        PRIVATE
        odb_gen
        proto_objs
        gflags
        gtest
        spdlog
        fmt
        jsoncpp
        protobuf
        pthread
        odb-mysql
        odb
    )

    target_link_libraries(transmit_client
        PRIVATE
        odb_gen
        proto_objs
        gflags
        gtest
        spdlog
        fmt
        amqpcpp
        ev
        etcd-cpp-api
        cpprest
        brpc
        /usr/local/openssl-3.0.16/lib64/libssl.so.3
        /usr/local/openssl-3.0.16/lib64/libcrypto.so.3
        protobuf
        leveldb
        pthread
        odb-mysql
        odb
    )
endif()

# 包含头文件目录
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../third/include
)

# 将项目目录的transmit.conf文件复制到build目录
configure_file(${CMAKE_SOURCE_DIR}/server/conf/transmit.conf ${CMAKE_CURRENT_BINARY_DIR}/transmit.conf COPYONLY)
