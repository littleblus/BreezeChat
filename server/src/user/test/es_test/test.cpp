#include "data_es.hpp"
#include "logger.hpp"
#include <gflags/gflags.h>
#include <gtest/gtest.h>

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");
DEFINE_string(es_url, "http://localhost:9200/", "ES服务器地址");

std::shared_ptr<elasticlient::Client> es_client;
std::shared_ptr<blus::ESUser> es_user;

TEST(ESUser, CreateIndex) {
    EXPECT_TRUE(es_user->createIndex());
}

TEST(ESUser, Insert) {
    EXPECT_TRUE(es_user->append("test_uid", "12345678901@gmail.com", "test_nickname", "test_description", "test_avatar_id"));
    EXPECT_TRUE(es_user->append("test_uid2", "12345678902@gmail.com", "test_nickname2", "test_description2", "test_avatar_id2"));
}

TEST(ESUser, Search) {
    // 等待索引创建与数据append完成(ES索引创建是异步的)
    // 实际使用中可以使用ES的API来检查索引状态
    std::this_thread::sleep_for(std::chrono::seconds(2));
    // 如果users没数据就提前结束，避免段错误
    auto users = es_user->search("test_uid2");
    EXPECT_EQ(users.size(), 1);
    if (users.empty()) {
        LOG_ERROR("Test with test_uid2");
        FAIL();
    }
    EXPECT_EQ(users[0].user_id(), "test_uid2");
    EXPECT_EQ(users[0].email(), "12345678902@gmail.com");
    EXPECT_EQ(users[0].nickname(), "test_nickname2");
    EXPECT_EQ(users[0].description(), "test_description2");
    EXPECT_EQ(users[0].avatar_id(), "test_avatar_id2");
    // Test with exclude_uid_list
    users = es_user->search("test_nickname2", { "test_uid2" });
    EXPECT_EQ(users.size(), 0);
    // Test with empty exclude_uid_list
    users = es_user->search("test_nickname", {});
    EXPECT_EQ(users.size(), 1);
    if (users.empty()) {
        LOG_ERROR("Test with empty exclude_uid_list");
        FAIL();
    }
    EXPECT_EQ(users[0].user_id(), "test_uid");
    EXPECT_EQ(users[0].email(), "12345678901@gmail.com");
    EXPECT_EQ(users[0].nickname(), "test_nickname");
    EXPECT_EQ(users[0].description(), "test_description");
    EXPECT_EQ(users[0].avatar_id(), "test_avatar_id");
    // Test email
    users = es_user->search("1234567890@gmail.com");
    EXPECT_EQ(users.size(), 0);
    users = es_user->search("12345678901@gmail.com");
    EXPECT_EQ(users.size(), 1);
    if (users.empty()) {
        LOG_ERROR("Test email");
        FAIL();
    }
    EXPECT_EQ(users[0].user_id(), "test_uid");
    EXPECT_EQ(users[0].email(), "12345678901@gmail.com");
    EXPECT_EQ(users[0].nickname(), "test_nickname");
    EXPECT_EQ(users[0].description(), "test_description");
    EXPECT_EQ(users[0].avatar_id(), "test_avatar_id");
}

int main(int argc, char** argv) {
    // Initialize gflags
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("user.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    // Initialize gtest
    ::testing::InitGoogleTest(&argc, argv);

    es_client = blus::ESFactory::create({ FLAGS_es_url });
    es_user = std::make_shared<blus::ESUser>(es_client);

    // Run all tests
    return RUN_ALL_TESTS();
}