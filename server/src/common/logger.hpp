#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <iostream>
#include <string>

std::shared_ptr<spdlog::logger> g_logger;

#define LOG_TRACE(format, ...) g_logger->trace(std::string("[{}:{}]") + format, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) g_logger->debug(std::string("[{}:{}]") + format, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO(format, ...) g_logger->info(std::string("[{}:{}]") + format, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_WARN(format, ...) g_logger->warn(std::string("[{}:{}]") + format, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) g_logger->error(std::string("[{}:{}]") + format, __FILE__, __LINE__, ##__VA_ARGS__)
#define LOG_CRITICAL(format, ...) g_logger->critical(std::string("[{}:{}]") + format, __FILE__, __LINE__, ##__VA_ARGS__)

// 初始化日志器
// 参数:
// - log_file: 日志文件路径（默认输出到控制台）
// - level: 日志等级（默认trace）
void init_logger(const std::string& log_file = "", spdlog::level::level_enum level = spdlog::level::level_enum::trace) {
    spdlog::flush_every(std::chrono::seconds(1));

    if (log_file.empty()) {
        // 输出到控制台
        g_logger = spdlog::stdout_color_mt("console");
    }
    else {
        // 输出到文件
        g_logger = spdlog::basic_logger_mt("file", log_file);
    }

    // 设置日志等级
    g_logger->set_level(level);
    // 设置日志刷新策略
    g_logger->flush_on(level);
    // 设置日志输出格式
    g_logger->set_pattern("[%n][%H:%M:%S][%t][%l] %v");
}
