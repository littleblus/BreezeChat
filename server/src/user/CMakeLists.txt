cmake_minimum_required(VERSION 3.10)
project(user_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# proto文件
set(proto_files base.proto user.proto file.proto)
foreach(proto_file ${proto_files})
    string(REGEX REPLACE "\.proto$" ".pb.cc" proto_src "${proto_file}")
    string(REGEX REPLACE "\.proto$" ".pb.h" proto_hdr "${proto_file}")
    list(APPEND proto_srcs ${CMAKE_BINARY_DIR}/proto/${proto_src})
    list(APPEND proto_hdrs ${CMAKE_BINARY_DIR}/proto/${proto_hdr})
endforeach()

# odb头文件
set(odb_path ${CMAKE_SOURCE_DIR}/server/odb)
set(odb_files user.hxx)

foreach(odb_file ${odb_files})
    string(REGEX REPLACE "\.hxx$" "-odb.cxx" odb_cxx "${odb_file}")
    string(REGEX REPLACE "\.hxx$" "-odb.ixx" odb_ixx "${odb_file}")
    string(REGEX REPLACE "\.hxx$" "-odb.hxx" odb_hdr "${odb_file}")
    string(REGEX REPLACE "\.hxx$" ".sql" odb_sql "${odb_file}")
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${odb_cxx}
               ${CMAKE_CURRENT_BINARY_DIR}/${odb_ixx}
               ${CMAKE_CURRENT_BINARY_DIR}/${odb_hdr}
               ${CMAKE_CURRENT_BINARY_DIR}/${odb_sql}
        COMMAND odb -d mysql --std c++17 --generate-query --generate-schema -o ${CMAKE_CURRENT_BINARY_DIR} ${odb_path}/${odb_file}
        DEPENDS ${odb_path}/${odb_file}
        COMMENT "Generating ODB files for ${odb_file}"
    )
    list(APPEND odb_srcs ${CMAKE_CURRENT_BINARY_DIR}/${odb_cxx})
    list(APPEND odb_hdrs ${CMAKE_CURRENT_BINARY_DIR}/${odb_ixx})
    list(APPEND odb_hdrs ${CMAKE_CURRENT_BINARY_DIR}/${odb_hdr})
    list(APPEND odb_sqls ${CMAKE_CURRENT_BINARY_DIR}/${odb_sql})
endforeach()

# 添加可执行文件
add_executable(user_server src/user_server.cpp ${proto_srcs} ${odb_srcs})

# 链接依赖库
target_link_libraries(user_server
    PRIVATE
    gflags
    spdlog
    fmt
    hiredis
    redis++
    cpr
    elasticlient
    etcd-cpp-api
    cpprest
    jsoncpp
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
    add_executable(mysql_test test/mysql_test/test.cpp ${proto_srcs} ${odb_srcs})
    add_executable(redis_test test/redis_test/test.cpp ${proto_srcs})
    add_executable(es_test test/es_test/test.cpp ${proto_srcs})
    add_executable(user_client test/user_client.cpp ${proto_srcs} ${odb_srcs})

    target_link_libraries(mysql_test
        PRIVATE
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

    target_link_libraries(redis_test
        PRIVATE
        gflags
        gtest
        hiredis
        redis++
        spdlog
        fmt
        jsoncpp
        protobuf
        pthread
    )

    target_link_libraries(es_test
        PRIVATE
        gflags
        gtest
        spdlog
        fmt
        cpr
        elasticlient
        jsoncpp
        protobuf
        pthread
    )

    target_link_libraries(user_client
        PRIVATE
        gflags
        gtest
        gmock
        spdlog
        fmt
        hiredis
        redis++
        cpr
        elasticlient
        etcd-cpp-api
        cpprest
        jsoncpp
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
    ${CMAKE_BINARY_DIR}/proto
    ${CMAKE_SOURCE_DIR}/server/odb
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../third/include
)

# 将项目目录的user.conf文件复制到build目录
configure_file(${CMAKE_SOURCE_DIR}/server/conf/user.conf ${CMAKE_CURRENT_BINARY_DIR}/user.conf COPYONLY)
