cmake_minimum_required(VERSION 3.10)
project(chatroom)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含子项目
add_subdirectory(server/src/speech)

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
