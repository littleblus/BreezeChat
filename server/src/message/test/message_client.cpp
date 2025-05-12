#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <iostream>

#include "base.pb.h"
#include "user.pb.h"
#include "transmite.pb.h"
#include "etcd.hpp"
#include "channel.hpp"
#include "utils.hpp"
#include "data_mysql.hpp"
#include "data_es.hpp"
#include "message.pb.h"

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");
DEFINE_string(user_service_name, "", "用户管理服务名称");
DEFINE_string(transmit_service_name, "", "消息转发服务名称");
DEFINE_string(message_service_name, "", "消息存储服务名称");
DEFINE_string(etcd_address, "127.0.0.1:2379", "etcd地址");

DEFINE_string(mysql_user, "root", "mysql服务器登录用户名");
DEFINE_string(mysql_pswd, "", "mysql服务器登录密码");

DEFINE_string(es_url, "http://localhost:9200/", "ES服务器地址");

blus::Discovery::Ptr message_dis, transmit_dis, user_dis;
blus::ChannelPtr message_addr, transmit_addr, user_addr;
std::shared_ptr<odb::database> g_db = nullptr;
blus::UserTable* g_user_table = nullptr;
blus::ChatSessionMemberTable* g_csm_table = nullptr;
blus::MessageTable* g_message_table = nullptr;
std::shared_ptr<elasticlient::Client> g_es = nullptr;
std::shared_ptr<blus::ESUser> g_user_es = nullptr;
std::shared_ptr<blus::ESMessage> g_message_es = nullptr;
std::string user_id1, user_id2;
std::string message_id1, message_id2;

void init_channel() {
    // 构造Rpc信道管理对象
    auto sm = std::make_shared<blus::ServiceManager>();
    auto put_cb = [sm](const std::string& name, const std::string& ip) {
        sm->online(name, ip);
        };
    auto del_cb = [sm](const std::string& name, const std::string& ip) {
        sm->offline(name, ip);
        };
    sm->declare(FLAGS_message_service_name);
    sm->declare(FLAGS_transmit_service_name);
    sm->declare(FLAGS_user_service_name);

    // 构造服务发现对象
    message_dis = std::make_shared<blus::Discovery>(FLAGS_message_service_name, FLAGS_etcd_address, put_cb, del_cb);
    transmit_dis = std::make_shared<blus::Discovery>(FLAGS_transmit_service_name, FLAGS_etcd_address, put_cb, del_cb);
    user_dis = std::make_shared<blus::Discovery>(FLAGS_user_service_name, FLAGS_etcd_address, put_cb, del_cb);

    // 获取服务地址
    message_addr = sm->get(FLAGS_message_service_name);
    if (!message_addr) {
        LOG_ERROR("获取消息存储服务地址失败");
        abort();
    }
    transmit_addr = sm->get(FLAGS_transmit_service_name);
    if (!transmit_addr) {
        LOG_ERROR("获取消息转发服务地址失败");
        abort();
    }
    user_addr = sm->get(FLAGS_user_service_name);
    if (!user_addr) {
        LOG_ERROR("获取用户管理服务地址失败");
        abort();
    }
}

TEST(MessageServiceTest, GetHistoryMsg) {
    // 调用消息存储服务的 GetHistoryMsg 接口并验证返回结果
    blus::MsgStorageService_Stub stub(message_addr.get());
    brpc::Controller cntl;
    blus::GetHistoryMsgReq req;
    blus::GetHistoryMsgRsp rsp;
    req.set_request_id("test_history");
    req.set_chat_session_id("test_session1");
    req.set_start_time(time(nullptr) - 3600);
    req.set_over_time(time(nullptr) + 10);
    stub.GetHistoryMsg(&cntl, &req, &rsp, nullptr);
    EXPECT_TRUE(rsp.success());
    EXPECT_EQ(rsp.msg_list_size(), 5);
}

TEST(MessageServiceTest, GetRecentMsg) {
    // 调用消息存储服务的 GetRecentMsg 接口并验证返回结果
    blus::MsgStorageService_Stub stub(message_addr.get());
    brpc::Controller cntl;
    blus::GetRecentMsgReq req;
    blus::GetRecentMsgRsp rsp;
    req.set_request_id("test_recent");
    req.set_chat_session_id("test_session1");
    req.set_msg_count(3);
    req.set_cur_time(time(nullptr));
    stub.GetRecentMsg(&cntl, &req, &rsp, nullptr);
    EXPECT_TRUE(rsp.success());
    EXPECT_EQ(rsp.msg_list_size(), 3);
}

TEST(MessageServiceTest, MsgSearch) {
    // 调用消息存储服务的 MsgSearch 接口并验证返回结果
    blus::MsgStorageService_Stub stub(message_addr.get());
    brpc::Controller cntl;
    blus::MsgSearchReq req;
    blus::MsgSearchRsp rsp;
    req.set_request_id("test_search");
    req.set_chat_session_id("test_session1");
    req.set_search_key("吗");
    stub.MsgSearch(&cntl, &req, &rsp, nullptr);
    EXPECT_TRUE(rsp.success());
    EXPECT_EQ(rsp.msg_list_size(), 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("message.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    if (!message_addr || !transmit_addr || !user_addr) init_channel();
    // 初始化数据库连接
    if (!g_db) g_db = blus::ODBFactory::create(FLAGS_mysql_user, FLAGS_mysql_pswd, "test", "localhost", "/var/run/mysqld/mysqld.sock");
    if (!g_user_table) g_user_table = new blus::UserTable(g_db);
    if (!g_csm_table) g_csm_table = new blus::ChatSessionMemberTable(g_db);
    if (!g_message_table) g_message_table = new blus::MessageTable(g_db);
    if (!g_es) g_es = blus::ESFactory::create({ FLAGS_es_url });
    if (!g_user_es) g_user_es = std::make_shared<blus::ESUser>(g_es);
    if (!g_message_es) g_message_es = std::make_shared<blus::ESMessage>(g_es);
    // 构造测试数据 user表添加对应的用户信息(使用user服务), chat_session_member表添加对应的会话成员信息(mysql直接添加)
    // 然后插入message数据(使用transmit服务)
    auto add_user = [](const std::string& nickname, const std::string& password) -> std::string {
        blus::UserService_Stub user_stub(user_addr.get());
        brpc::Controller cntl;
        blus::UserRegisterReq user_request;
        blus::UserRegisterRsp user_response;

        std::string user_req_id = blus::uuid();
        user_request.set_request_id(user_req_id);
        user_request.set_nickname(nickname);
        user_request.set_password(password);

        user_stub.UserRegister(&cntl, &user_request, &user_response, nullptr);
        if (cntl.Failed() || !user_response.success()) {
            LOG_CRITICAL("Error: {}", cntl.Failed() ? cntl.ErrorText() : user_response.errmsg());
            abort();
        }
        if (user_response.request_id() != user_req_id) {
            LOG_CRITICAL("Error: user_response.request_id() != user_req_id");
            abort();
        }
        // 调用user表查询user_id并返回
        auto user = g_user_table->select_by_nickname(nickname);
        if (!user) {
            LOG_CRITICAL("Error: new user {} not found", nickname);
            abort();
        }
        return user->user_id();
        };
    auto add_csm = [](const std::string& session_id, const std::vector<std::string>& user_ids) {
        std::vector<std::shared_ptr<blus::ChatSessionMember>> members;
        for (const auto& user_id : user_ids) {
            auto member = std::make_shared<blus::ChatSessionMember>();
            member->session_id(session_id);
            member->user_id(user_id);
            members.push_back(member);
        }
        if (!g_csm_table->append(members)) {
            LOG_CRITICAL("Error: add chat session member failed, session_id: {}", session_id);
            abort();
        }
        };
    auto string_message = [](const std::string& user_id, const std::string& chat_session_id, const std::string& content) {
        blus::MsgTransmitService_Stub transmit_stub(transmit_addr.get());
        brpc::Controller cntl;
        blus::NewMessageReq transmit_req;
        blus::GetTransmitTargetRsp transmit_rsp;

        transmit_req.set_request_id(blus::uuid());
        transmit_req.set_user_id(user_id);
        transmit_req.set_chat_session_id(chat_session_id);
        transmit_req.mutable_message()->set_message_type(blus::MessageType::STRING);
        transmit_req.mutable_message()->mutable_string_message()->set_content(content);
        transmit_stub.GetTransmitTarget(&cntl, &transmit_req, &transmit_rsp, nullptr);
        if (cntl.Failed() || !transmit_rsp.success()) {
            LOG_CRITICAL("Error: {}", cntl.Failed() ? cntl.ErrorText() : transmit_rsp.errmsg());
            abort();
        }
        };
    auto image_message = [](const std::string& user_id, const std::string& chat_session_id,
        const std::string& file_id, const std::string& content) {
            blus::MsgTransmitService_Stub transmit_stub(transmit_addr.get());
            brpc::Controller cntl;
            blus::NewMessageReq transmit_req;
            blus::GetTransmitTargetRsp transmit_rsp;

            transmit_req.set_request_id(blus::uuid());
            transmit_req.set_user_id(user_id);
            transmit_req.set_chat_session_id(chat_session_id);
            transmit_req.mutable_message()->set_message_type(blus::MessageType::IMAGE);
            transmit_req.mutable_message()->mutable_image_message()->set_image_content(content);
            transmit_req.mutable_message()->mutable_image_message()->set_file_id(file_id);
            transmit_stub.GetTransmitTarget(&cntl, &transmit_req, &transmit_rsp, nullptr);
            if (cntl.Failed() || !transmit_rsp.success()) {
                LOG_CRITICAL("Error: {}", cntl.Failed() ? cntl.ErrorText() : transmit_rsp.errmsg());
                abort();
            }
        };
    auto speech_message = [](const std::string& user_id, const std::string& chat_session_id,
        const std::string& file_id, const std::string& content) {
            blus::MsgTransmitService_Stub transmit_stub(transmit_addr.get());
            brpc::Controller cntl;
            blus::NewMessageReq transmit_req;
            blus::GetTransmitTargetRsp transmit_rsp;

            transmit_req.set_request_id(blus::uuid());
            transmit_req.set_user_id(user_id);
            transmit_req.set_chat_session_id(chat_session_id);
            transmit_req.mutable_message()->set_message_type(blus::MessageType::SPEECH);
            transmit_req.mutable_message()->mutable_speech_message()->set_file_contents(content);
            transmit_req.mutable_message()->mutable_speech_message()->set_file_id(file_id);
            transmit_stub.GetTransmitTarget(&cntl, &transmit_req, &transmit_rsp, nullptr);
            if (cntl.Failed() || !transmit_rsp.success()) {
                LOG_CRITICAL("Error: {}", cntl.Failed() ? cntl.ErrorText() : transmit_rsp.errmsg());
                abort();
            }
        };
    auto file_message = [](const std::string& user_id, const std::string& chat_session_id,
        const std::string& file_id, const std::string& file_name, unsigned long file_size, const std::string& content) {
            blus::MsgTransmitService_Stub transmit_stub(transmit_addr.get());
            brpc::Controller cntl;
            blus::NewMessageReq transmit_req;
            blus::GetTransmitTargetRsp transmit_rsp;

            transmit_req.set_request_id(blus::uuid());
            transmit_req.set_user_id(user_id);
            transmit_req.set_chat_session_id(chat_session_id);
            transmit_req.mutable_message()->set_message_type(blus::MessageType::FILE);
            transmit_req.mutable_message()->mutable_file_message()->set_file_contents(content);
            transmit_req.mutable_message()->mutable_file_message()->set_file_id(file_id);
            transmit_req.mutable_message()->mutable_file_message()->set_file_name(file_name);
            transmit_req.mutable_message()->mutable_file_message()->set_file_size(file_size);
            transmit_stub.GetTransmitTarget(&cntl, &transmit_req, &transmit_rsp, nullptr);
            if (cntl.Failed() || !transmit_rsp.success()) {
                LOG_CRITICAL("Error: {}", cntl.Failed() ? cntl.ErrorText() : transmit_rsp.errmsg());
                abort();
            }
        };

    user_id1 = add_user("test_user1", "Password123");
    user_id2 = add_user("test_user2", "Password456");
    add_csm("test_session1", { user_id1, user_id2 });
    string_message(user_id1, "test_session1", "吃饭了吗？");
    string_message(user_id2, "test_session1", "吃的盖浇饭！");
    image_message(user_id1, "test_session1", "image_file_id", "可可爱爱的头像数据");
    speech_message(user_id1, "test_session1", "speech_file_id", "动动听听的语音数据");
    file_message(user_id1, "test_session1", "file_file_id", "长春必吃榜", 1024, "美味盖浇饭大全");

    // 睡眠一段时间，等待索引创建完成
    std::this_thread::sleep_for(std::chrono::seconds(5));

    int ret = RUN_ALL_TESTS();

    // 清理测试中创建的聊天会话成员
    if (g_user_table) {
        g_user_table->clear();
        delete g_user_table;
        g_user_table = nullptr;
    }
    if (g_csm_table) {
        g_csm_table->clear();
        delete g_csm_table;
        g_csm_table = nullptr;
    }
    if (g_message_table) {
        g_message_table->clear();
        delete g_message_table;
        g_message_table = nullptr;
    }
    // 删除ES中创建的user, message
    if (g_user_es) {
        g_user_es->deleteIndex();
        g_user_es.reset();
    }
    if (g_message_es) {
        g_message_es->deleteIndex();
        g_message_es.reset();
    }

    return ret;
}