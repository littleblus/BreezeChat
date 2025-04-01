#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "user.hxx"
#include "user-odb.hxx"
#include "data_mysql.hpp"

// 全局声明指针，后续在 main 中初始化
blus::UserTable* g_user_table = nullptr;
std::shared_ptr<odb::database> g_db = nullptr;

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");
DEFINE_string(mysql_user, "root", "mysql服务器登录用户名");
DEFINE_string(mysql_pswd, "", "mysql服务器登录密码");

TEST(odb, insert) {
    auto user1 = std::make_shared<blus::User>("uid1", "nickname1", "123456");
    g_user_table->insert(user1);

    auto user2 = std::make_shared<blus::User>("uid2", "12344445555@gmail.com");
    g_user_table->insert(user2);

    auto r1 = g_user_table->select_by_uid(user1->user_id());
    auto r2 = g_user_table->select_by_uid(user2->user_id());
    ASSERT_NE(r1, nullptr);
    ASSERT_NE(r2, nullptr);
    ASSERT_EQ(r1->user_id(), user1->user_id());
    ASSERT_EQ(r1->nickname(), user1->nickname());
    ASSERT_EQ(r1->password(), user1->password());
    ASSERT_EQ(r2->user_id(), user2->user_id());
    ASSERT_EQ(r2->email(), user2->email());
}

TEST(odb, update) {
    auto user1 = std::make_shared<blus::User>("uid1", "12300001111@gmail.com");
    g_user_table->update(user1);

    auto user2 = std::make_shared<blus::User>("uid2", "Musk", "88888888");
    g_user_table->update(user2);

    auto r1 = g_user_table->select_by_uid(user1->user_id());
    auto r2 = g_user_table->select_by_uid(user2->user_id());
    ASSERT_NE(r1, nullptr);
    ASSERT_NE(r2, nullptr);
    ASSERT_EQ(r1->user_id(), user1->user_id());
    ASSERT_EQ(r1->nickname(), user1->nickname());
    ASSERT_EQ(r1->password(), user1->password());
    ASSERT_EQ(r2->user_id(), user2->user_id());
    ASSERT_EQ(r2->email(), user2->email());
}

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("user.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    g_db = blus::ODBFactory::create(FLAGS_mysql_user, FLAGS_mysql_pswd, "test", "localhost", "/var/run/mysqld/mysqld.sock");
    g_user_table = new blus::UserTable(g_db);

    int ret = RUN_ALL_TESTS();

    delete g_user_table;
    return ret;
}