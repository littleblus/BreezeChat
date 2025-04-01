#include "data_redis.hpp"
#include "logger.hpp"
#include <gflags/gflags.h>
#include <gtest/gtest.h>

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");
DEFINE_int32(redis_db, 0, "Redis数据库号");
DEFINE_string(redis_host, "localhost", "Redis主机名");
DEFINE_int32(redis_port, 6379, "Redis端口号");
DEFINE_bool(redis_keep_alive, true, "Redis连接保持活跃");

std::shared_ptr<sw::redis::Redis> redis;

TEST(redis, session) {
    // append
    blus::Session session(redis);
    session.append("test_session", "test_user");
    auto uid = session.uid("test_session");
    EXPECT_EQ(uid.value(), "test_user");
    // remove
    session.remove("test_session");
    uid = session.uid("test_session");
    EXPECT_FALSE(uid.has_value());
}

TEST(redis, status) {
    // append
    blus::Status status(redis);
    status.append("test_user");
    EXPECT_TRUE(status.exists("test_user"));
    // remove
    status.remove("test_user");
    EXPECT_FALSE(status.exists("test_user"));
}

TEST(redis, verifycode) {
    // append
    blus::VerifyCode verifycode(redis);
    verifycode.append("test_user", "123456");
    auto code = verifycode.code("test_user");
    EXPECT_EQ(code.value(), "123456");
    // remove
    verifycode.remove("test_user");
    code = verifycode.code("test_user");
    EXPECT_FALSE(code.has_value());
    // expire
    verifycode.append("test_user", "123456", 2);
    code = verifycode.code("test_user");
    EXPECT_EQ(code.value(), "123456");
    std::this_thread::sleep_for(std::chrono::seconds(3));
    code = verifycode.code("test_user");
    EXPECT_FALSE(code.has_value());
}


int main(int argc, char** argv) {
    // Initialize gflags
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("user.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    // Initialize gtest
    ::testing::InitGoogleTest(&argc, argv);

    redis = blus::RedisFactory::create(FLAGS_redis_db, FLAGS_redis_host, FLAGS_redis_port, FLAGS_redis_keep_alive);

    // Run all tests
    return RUN_ALL_TESTS();
}