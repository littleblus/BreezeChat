#include "logger.hpp"
#include <gflags/gflags.h>

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");

int main(int argc, char** argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));
    LOG_TRACE("Hello, {}!", "World");

    return 0;
}