cmake_minimum_required(VERSION 3.10)
project(chatroom)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# proto文件
set(proto_path ${CMAKE_SOURCE_DIR}/server/proto)
file(GLOB proto_files RELATIVE ${proto_path} "${proto_path}/*.proto")

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/proto)

foreach(proto_file ${proto_files})
    string(REGEX REPLACE "\.proto$" ".pb.cc" proto_src "${proto_file}")
    string(REGEX REPLACE "\.proto$" ".pb.h" proto_hdr "${proto_file}")
    execute_process(
        COMMAND protoc -I=${proto_path} --experimental_allow_proto3_optional --cpp_out=${CMAKE_BINARY_DIR}/proto ${proto_path}/${proto_file}
        RESULT_VARIABLE protoc_result
        OUTPUT_VARIABLE protoc_output
        ERROR_VARIABLE protoc_error
    )
    if(NOT protoc_result EQUAL 0)
        message(FATAL_ERROR "Failed to generate ${proto_file}: ${protoc_error}")
    endif()
    list(APPEND proto_srcs ${CMAKE_BINARY_DIR}/proto/${proto_src})
    list(APPEND proto_hdrs ${CMAKE_BINARY_DIR}/proto/${proto_hdr})
endforeach()

# 公共 ODB 输出目录
set(ODB_OUT_DIR ${CMAKE_BINARY_DIR}/odb)
file(MAKE_DIRECTORY ${ODB_OUT_DIR})

# 所有要生成 .hxx 的文件
set(ODB_INPUT_DIR ${CMAKE_SOURCE_DIR}/server/odb)
set(ODB_FILES
    chat_session_member.hxx
    user.hxx
)

# 收集所有生成文件
set(ALL_ODB_SRCS  )
set(ALL_ODB_HDRS  )
set(ALL_ODB_IXXS  )
set(ALL_ODB_SQLS  )
foreach(f ${ODB_FILES})
  string(REGEX REPLACE "\.hxx$" "-odb.cxx"  _cxx ${f})
  string(REGEX REPLACE "\.hxx$" "-odb.hxx"  _hdr ${f})
  string(REGEX REPLACE "\.hxx$" "-odb.ixx"  _ixx ${f})
  string(REGEX REPLACE "\.hxx$" ".sql"      _sql ${f})
  add_custom_command(
    OUTPUT ${ODB_OUT_DIR}/${_cxx}
           ${ODB_OUT_DIR}/${_hdr}
           ${ODB_OUT_DIR}/${_ixx}
           ${ODB_OUT_DIR}/${_sql}
    COMMAND odb -d mysql --std c++17
            --generate-query --generate-schema
            -o ${ODB_OUT_DIR}
            ${ODB_INPUT_DIR}/${f}
    DEPENDS ${ODB_INPUT_DIR}/${f}
    COMMENT "Generating ODB files for ${f}"
  )
  list(APPEND ALL_ODB_SRCS ${ODB_OUT_DIR}/${_cxx})
  list(APPEND ALL_ODB_HDRS ${ODB_OUT_DIR}/${_hdr})
  list(APPEND ALL_ODB_HDRS ${ODB_OUT_DIR}/${_ixx})
  list(APPEND ALL_ODB_SQLS ${ODB_OUT_DIR}/${_sql})
endforeach()

# 把生成的文件打包成一个静态库
add_library(odb_gen STATIC ${ALL_ODB_SRCS})
target_include_directories(odb_gen PUBLIC ${ODB_INPUT_DIR} ${ODB_OUT_DIR})

# 包含子项目
add_subdirectory(server/src/speech)
add_subdirectory(server/src/file)
add_subdirectory(server/src/user)
add_subdirectory(server/src/transmit)

# 添加可执行文件
add_executable(test test.cpp)

# 链接依赖库
target_link_libraries(test
    PRIVATE
    pthread
    jsoncpp
    fmt
)

# 包含头文件目录
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/server/src/common
    ${CMAKE_SOURCE_DIR}/server/src/third/include
)

# 将项目目录的config.conf文件复制到build目录
# configure_file(${CMAKE_SOURCE_DIR}/config.conf ${CMAKE_BINARY_DIR}/config.conf COPYONLY)
