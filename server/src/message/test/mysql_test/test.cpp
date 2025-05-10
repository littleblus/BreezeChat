#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "message.hxx"
#include "message-odb.hxx"
#include "data_mysql.hpp"

// 全局声明指针，后续在 main 中初始化
blus::MessageTable* g_message_table = nullptr;
std::shared_ptr<odb::database> g_db = nullptr;

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");
DEFINE_string(mysql_user, "root", "mysql服务器登录用户名");
DEFINE_string(mysql_pswd, "", "mysql服务器登录密码");

class MessageTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!g_db) g_db = blus::ODBFactory::create(FLAGS_mysql_user, FLAGS_mysql_pswd, "test", "localhost", "/var/run/mysqld/mysqld.sock");
        if (!g_message_table) g_message_table = new blus::MessageTable(g_db);
    }
    void TearDown() override {
        if (g_message_table) {
            delete g_message_table;
            g_message_table = nullptr;
        }
    }
};

TEST_F(MessageTableTest, insert) {
    blus::Message m1{ "1", "user1", "session1", 0, boost::posix_time::ptime(boost::posix_time::time_from_string("2023-10-01 12:00:00")) };
    blus::Message m2{ "2", "user2", "session1", 0, boost::posix_time::ptime(boost::posix_time::time_from_string("2023-10-02 12:00:00")) };
    blus::Message m3{ "3", "user3", "session1", 0, boost::posix_time::ptime(boost::posix_time::time_from_string("2023-10-03 12:00:00")) };

    blus::Message m4{ "4", "user4", "session2", 0, boost::posix_time::ptime(boost::posix_time::time_from_string("2023-10-04 12:00:00")) };
    blus::Message m5{ "5", "user5", "session2", 0, boost::posix_time::ptime(boost::posix_time::time_from_string("2023-10-05 12:00:00")) };
    EXPECT_TRUE(g_message_table->insert(m1));
    EXPECT_TRUE(g_message_table->insert(m2));
    EXPECT_TRUE(g_message_table->insert(m3));
    EXPECT_TRUE(g_message_table->insert(m4));
    EXPECT_TRUE(g_message_table->insert(m5));

    // 查询验证
    auto messages = g_message_table->get_recent("session1", 3);
    EXPECT_EQ(messages.size(), 3);
    EXPECT_EQ(messages[0].message_id(), "3");
    EXPECT_EQ(messages[1].message_id(), "2");
    EXPECT_EQ(messages[2].message_id(), "1");
    messages = g_message_table->get_recent("session2", 3);
    EXPECT_EQ(messages.size(), 2);
    EXPECT_EQ(messages[0].message_id(), "5");
    EXPECT_EQ(messages[1].message_id(), "4");
    messages = g_message_table->get_recent("session3", 3);
    EXPECT_EQ(messages.size(), 0);
    // 范围查询
    auto start1 = boost::posix_time::time_from_string("2023-10-01 00:00:00");
    auto end1 = boost::posix_time::time_from_string("2023-10-03 23:59:59");
    messages = g_message_table->get_range("session1", start1, end1);
    EXPECT_EQ(messages.size(), 3);
    auto start2 = boost::posix_time::time_from_string("2023-10-01 13:00:00");
    auto end2 = boost::posix_time::time_from_string("2023-10-04 13:00:00");
    messages = g_message_table->get_range("session2", start2, end2);
    EXPECT_EQ(messages.size(), 1);
    EXPECT_EQ(messages[0].message_id(), "4");
    auto start3 = boost::posix_time::time_from_string("2023-10-09 00:00:00");
    auto end3 = boost::posix_time::time_from_string("2023-10-10 23:59:59");
    messages = g_message_table->get_range("session2", start3, end3);
    EXPECT_EQ(messages.size(), 0);
}

TEST_F(MessageTableTest, remove) {
    auto messages = g_message_table->get_recent("session1", 3);
    EXPECT_EQ(messages.size(), 3);
    EXPECT_TRUE(g_message_table->remove("session1"));
    messages = g_message_table->get_recent("session1", 3);
    EXPECT_EQ(messages.size(), 0);

    messages = g_message_table->get_recent("session2", 3);
    EXPECT_EQ(messages.size(), 2);
    EXPECT_TRUE(g_message_table->remove("session2"));
    messages = g_message_table->get_recent("session2", 3);
    EXPECT_EQ(messages.size(), 0);
}

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("message.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    return RUN_ALL_TESTS();
}