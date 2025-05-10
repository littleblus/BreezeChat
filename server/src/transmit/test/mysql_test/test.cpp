#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "chat_session_member.hxx"
#include "chat_session_member-odb.hxx"
#include "data_mysql.hpp"

// 全局声明指针，后续在 main 中初始化
blus::ChatSessionMemberTable* g_csm_table = nullptr;
std::shared_ptr<odb::database> g_db = nullptr;

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");
DEFINE_string(mysql_user, "root", "mysql服务器登录用户名");
DEFINE_string(mysql_pswd, "", "mysql服务器登录密码");

TEST(odb, append) {
    blus::ChatSessionMember csm1, csm2, csm3;
    csm1.session_id("test_session1");
    csm1.user_id("test_user1");
    EXPECT_TRUE(g_csm_table->append(csm1));
    csm2.session_id("test_session2");
    csm2.user_id("test_user2");
    csm3.session_id("test_session2");
    csm3.user_id("test_user3");
    // 列表添加
    std::vector<blus::ChatSessionMember> members;
    members.push_back(csm2);
    members.push_back(csm3);
    EXPECT_TRUE(g_csm_table->append(members));
}

TEST(odb, get) {
    auto vs = g_csm_table->get_members("test_session1");
    EXPECT_EQ(vs.size(), 1);
    EXPECT_EQ(vs[0], "test_user1");
    vs = g_csm_table->get_members("test_session2");
    EXPECT_EQ(vs.size(), 2);
    EXPECT_EQ(vs[0], "test_user2");
    EXPECT_EQ(vs[1], "test_user3");
    vs = g_csm_table->get_members("test_session3");
    EXPECT_EQ(vs.size(), 0);
}

TEST(odb, remove) {
    blus::ChatSessionMember csm1;
    csm1.session_id("test_session1");
    csm1.user_id("test_user1");
    EXPECT_TRUE(g_csm_table->remove(csm1));
    EXPECT_TRUE(g_csm_table->remove("test_session2"));
}

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("transmit.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    g_db = blus::ODBFactory::create(FLAGS_mysql_user, FLAGS_mysql_pswd, "test", "localhost", "/var/run/mysqld/mysqld.sock");
    g_csm_table = new blus::ChatSessionMemberTable(g_db);

    int ret = RUN_ALL_TESTS();

    delete g_csm_table;
    return ret;
}