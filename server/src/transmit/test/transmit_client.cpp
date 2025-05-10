#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <iostream>

#include "base.pb.h"
#include "user.pb.h"
#include "transmite.pb.h"
#include "etcd.hpp"
#include "channel.hpp"
#include "utils.hpp"

#include "chat_session_member.hxx"
#include "chat_session_member-odb.hxx"
#include "data_mysql.hpp"

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");
DEFINE_string(transmit_service_name, "", "转发微服务名称");
DEFINE_string(user_service_name, "", "用户微服务名称");
DEFINE_string(etcd_address, "127.0.0.1:2379", "etcd地址");

DEFINE_string(mysql_user, "root", "mysql服务器登录用户名");
DEFINE_string(mysql_pswd, "", "mysql服务器登录密码");

blus::ChannelPtr transmit_addr, user_addr;
std::shared_ptr<odb::database> g_db = nullptr;
blus::UserTable* g_user_table = nullptr;
blus::ChatSessionMemberTable* g_csm_table = nullptr;
blus::Discovery::Ptr transmit_dis, user_dis;

void init_channel() {
    // 构造Rpc信道管理对象
    auto sm = std::make_shared<blus::ServiceManager>();
    auto put_cb = [sm](const std::string& name, const std::string& ip) {
        sm->online(name, ip);
        };
    auto del_cb = [sm](const std::string& name, const std::string& ip) {
        sm->offline(name, ip);
        };
    sm->declare(FLAGS_transmit_service_name);
    sm->declare(FLAGS_user_service_name);

    // 构造服务发现对象
    transmit_dis = std::make_shared<blus::Discovery>(FLAGS_transmit_service_name, FLAGS_etcd_address, put_cb, del_cb);
    user_dis = std::make_shared<blus::Discovery>(FLAGS_user_service_name, FLAGS_etcd_address, put_cb, del_cb);

    // 获取transmit服务的地址
    transmit_addr = sm->get(FLAGS_transmit_service_name);
    if (!transmit_addr) {
        std::cout << "获取transmit服务地址失败" << std::endl;
        exit(1);
    }
    // 获取user服务的地址
    user_addr = sm->get(FLAGS_user_service_name);
    if (!user_addr) {
        std::cout << "获取user服务地址失败" << std::endl;
        exit(1);
    }
}

std::string user_id1;

// Test suite for user service.
class TransmitServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!transmit_addr || !user_addr) init_channel();
        if (!g_db) g_db = blus::ODBFactory::create(FLAGS_mysql_user, FLAGS_mysql_pswd, "test", "localhost", "/var/run/mysqld/mysqld.sock");
        if (!g_user_table) g_user_table = new blus::UserTable(g_db);
        if (!g_csm_table) g_csm_table = new blus::ChatSessionMemberTable(g_db);
    }
    void TearDown() override {
        // 清理测试中创建的聊天会话成员
        if (g_user_table) {
            g_user_table->remove(user_id1);
            delete g_user_table;
            g_user_table = nullptr;
        }
        if (g_csm_table) {
            g_csm_table->remove("test_session1");
            delete g_csm_table;
            g_csm_table = nullptr;
        }
        // 删除ES中创建的user（手动）
    }
};

TEST_F(TransmitServiceTest, GetTransmitTarget) {
    // 注册一个用户
    blus::UserService_Stub user_stub(user_addr.get());
    brpc::Controller cntl1;
    blus::UserRegisterReq user_request;
    blus::UserRegisterRsp user_response;

    std::string user_req_id = blus::uuid();
    user_request.set_request_id(user_req_id);
    user_request.set_nickname("validUser");
    user_request.set_password("Password123");

    user_stub.UserRegister(&cntl1, &user_request, &user_response, nullptr);
    ASSERT_FALSE(cntl1.Failed());
    EXPECT_TRUE(user_response.success());
    if (!user_response.success()) {
        std::cout << "Error: " << user_response.errmsg() << std::endl;
        return;
    }
    EXPECT_EQ(user_response.request_id(), user_req_id);
    std::cout << "请输入测试用户validUser对应的user_id:";
    std::cin >> user_id1;

    // 添加csm
    blus::ChatSessionMember csm1, csm2;
    csm1.session_id("test_session1");
    csm1.user_id("test_user1");
    csm2.session_id("test_session1");
    csm2.user_id("test_user2");
    // 列表添加
    std::vector<blus::ChatSessionMember> members;
    members.push_back(csm1);
    members.push_back(csm2);
    ASSERT_TRUE(g_csm_table->append(members));

    // 获取转发目标
    blus::MsgTransmitService_Stub transmit_stub(transmit_addr.get());
    brpc::Controller cntl2;
    blus::NewMessageReq transmit_req;
    blus::GetTransmitTargetRsp transmit_rsp;

    std::string trans_req_id = blus::uuid();
    transmit_req.set_request_id(trans_req_id);
    transmit_req.set_user_id(user_id1);
    transmit_req.set_chat_session_id("test_session1");
    transmit_req.mutable_message()->set_message_type(blus::MessageType::STRING);
    transmit_req.mutable_message()->mutable_string_message()->set_content("Hello, this is a test message.");
    transmit_stub.GetTransmitTarget(&cntl2, &transmit_req, &transmit_rsp, nullptr);
    EXPECT_FALSE(cntl2.Failed());
    if (cntl2.Failed()) {
        std::cout << "Error: " << cntl2.ErrorText() << std::endl;
        return;
    }
    EXPECT_TRUE(transmit_rsp.success());
    if (!transmit_rsp.success()) {
        std::cout << "Error: " << transmit_rsp.errmsg() << std::endl;
        return;
    }
    EXPECT_EQ(transmit_rsp.request_id(), trans_req_id);
    std::cout << "msg_id: " << transmit_rsp.message().message_id() << std::endl;
    std::cout << "chat_session_id: " << transmit_rsp.message().chat_session_id() << std::endl;
    std::cout << "timestamp: " << transmit_rsp.message().timestamp() << std::endl;
    std::cout << "sender: " << transmit_rsp.message().sender().nickname() << std::endl;
    std::cout << "message: " << transmit_rsp.message().message().string_message().content() << std::endl;
    std::cout << "target_id_list: ";
    for (const auto& id : transmit_rsp.target_id_list()) {
        std::cout << id << " ";
    }
    std::cout << std::endl;
    EXPECT_EQ(transmit_rsp.target_id_list_size(), 2);
    EXPECT_EQ(transmit_rsp.target_id_list(0), csm1.user_id());
    EXPECT_EQ(transmit_rsp.target_id_list(1), csm2.user_id());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("transmit.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));
    return RUN_ALL_TESTS();
}