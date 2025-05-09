#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <iostream>

#include "base.pb.h"
#include "user.pb.h"
#include "etcd.hpp"
#include "channel.hpp"
#include "utils.hpp"
#include "email.hpp"

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");
DEFINE_string(file_service_name, "fileService", "文件微服务名称");
DEFINE_string(user_service_name, "userService", "用户微服务名称");
DEFINE_string(etcd_address, "127.0.0.1:2379", "etcd地址");

blus::ChannelPtr addr;

namespace blus {
    class MockEmailSender : public EmailSender {
    public:
        MockEmailSender() : EmailSender("", "", "", "") {}

        MOCK_METHOD(bool, send, (const std::string& to, const std::string& subject, const std::string& message, std::string* output), (override));
        MOCK_METHOD(bool, sendVerifyCode, (const std::string& to, const std::string& code, std::string* output), (override));
    };
}

void init_channel() {
    // 构造Rpc信道管理对象
    auto sm = std::make_shared<blus::ServiceManager>();
    auto put_cb = [sm](const std::string& name, const std::string& ip) {
        sm->online(name, ip);
        };
    auto del_cb = [sm](const std::string& name, const std::string& ip) {
        sm->offline(name, ip);
        };
    sm->declare(FLAGS_user_service_name);

    // 构造服务发现对象
    auto dis = std::make_shared<blus::Discovery>(FLAGS_user_service_name, FLAGS_etcd_address, put_cb, del_cb);

    // 获取user服务的地址
    addr = sm->get(FLAGS_user_service_name);
    if (!addr) {
        std::cout << "获取服务地址失败" << std::endl;
        exit(1);
    }
}

// Test suite for user service.
class UserServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!addr) {
            init_channel();
        }
    }
};

std::string user_id;
std::string login_ssid;

TEST_F(UserServiceTest, UserRegister_Valid) {
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::UserRegisterReq request;
    blus::UserRegisterRsp response;

    std::string id = blus::uuid();
    request.set_request_id(id);
    request.set_nickname("validUser");
    request.set_password("Password123");

    stub.UserRegister(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_TRUE(response.success());
    if (!response.success()) {
        std::cout << "Error: " << response.errmsg() << std::endl;
        return;
    }
    EXPECT_EQ(response.request_id(), id);
    std::cout << "请输入测试用户validUser对应的user_id:";
    std::cin >> user_id;
}

TEST_F(UserServiceTest, UserRegister_Invalid_Nickname_sensitive) {
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::UserRegisterReq request;
    blus::UserRegisterRsp response;

    std::string id = blus::uuid();
    request.set_request_id(id);
    request.set_nickname("河南人爱偷井盖");
    request.set_password("Password123");

    stub.UserRegister(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_FALSE(response.success());
    EXPECT_EQ(response.request_id(), id);
    EXPECT_EQ(response.errmsg(), "昵称敏感");
}

TEST_F(UserServiceTest, UserRegister_Invalid_Nickname_long) {
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::UserRegisterReq request;
    blus::UserRegisterRsp response;

    std::string id = blus::uuid();
    request.set_request_id(id);
    request.set_nickname(std::string(100, 'a')); // 超过最大长度
    request.set_password("Password123");

    stub.UserRegister(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_FALSE(response.success());
    EXPECT_EQ(response.request_id(), id);
    EXPECT_EQ(response.errmsg(), "昵称格式错误");
}

TEST_F(UserServiceTest, UserRegister_Invalid_Nickname_already_exist) {
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::UserRegisterReq request;
    blus::UserRegisterRsp response;

    std::string id = blus::uuid();
    request.set_request_id(id);
    request.set_nickname("validUser"); // 超过最大长度
    request.set_password("Password123");

    stub.UserRegister(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_FALSE(response.success());
    EXPECT_EQ(response.request_id(), id);
    EXPECT_EQ(response.errmsg(), "昵称已存在");
}

TEST_F(UserServiceTest, UserLogin) {
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::UserLoginReq request;
    blus::UserLoginRsp response;

    std::string id = blus::uuid();
    request.set_request_id(id);
    request.set_nickname("validUser");
    request.set_password("Password123");

    stub.UserLogin(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_TRUE(response.success());
    if (!response.success()) {
        std::cout << "Error: " << response.errmsg() << std::endl;
        return;
    }
    EXPECT_EQ(response.request_id(), id);
    login_ssid = response.login_session_id();
}

TEST_F(UserServiceTest, SetUserAvatar) {
    std::string avatar_data = "测试头像数据";
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::SetUserAvatarReq request;
    blus::SetUserAvatarRsp response;

    std::string id = blus::uuid();
    request.set_request_id(id);
    request.set_user_id(user_id);
    request.set_session_id(login_ssid);
    request.set_avatar(avatar_data);
    stub.SetUserAvatar(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_TRUE(response.success());
    if (!response.success()) {
        std::cout << "Error: " << response.errmsg() << std::endl;
    }
    EXPECT_EQ(response.request_id(), id);
}

TEST_F(UserServiceTest, SetUserNickname) {
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::SetUserNicknameReq request;
    blus::SetUserNicknameRsp response;

    std::string id = blus::uuid();
    request.set_request_id(id);
    request.set_user_id(user_id);
    request.set_session_id(login_ssid);
    request.set_nickname("NewNickname");
    stub.SetUserNickname(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_TRUE(response.success());
    if (!response.success()) {
        std::cout << "Error: " << response.errmsg() << std::endl;
    }
    EXPECT_EQ(response.request_id(), id);
}

TEST_F(UserServiceTest, SetUserDescription) {
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::SetUserDescriptionReq request;
    blus::SetUserDescriptionRsp response;

    std::string id = blus::uuid();
    request.set_request_id(id);
    request.set_user_id(user_id);
    request.set_session_id(login_ssid);
    request.set_description("我就是我，不一样的烟火");
    stub.SetUserDescription(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_TRUE(response.success());
    if (!response.success()) {
        std::cout << "Error: " << response.errmsg() << std::endl;
    }
    EXPECT_EQ(response.request_id(), id);
}

TEST_F(UserServiceTest, GetUserInfo) {
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::GetUserInfoReq request;
    blus::GetUserInfoRsp response;

    std::string id = blus::uuid();
    request.set_request_id(id);
    request.set_user_id(user_id);
    request.set_session_id(login_ssid);
    stub.GetUserInfo(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_TRUE(response.success());
    if (!response.success()) {
        std::cout << "Error: " << response.errmsg() << std::endl;
        return;
    }
    EXPECT_EQ(response.request_id(), id);
    // 检查返回的用户信息
    auto& user_info = response.user_info();
    EXPECT_EQ(user_info.user_id(), user_id);
    EXPECT_EQ(user_info.nickname(), "NewNickname");
    EXPECT_EQ(user_info.description(), "我就是我，不一样的烟火");
    EXPECT_EQ(user_info.avatar(), "测试头像数据");
}

TEST_F(UserServiceTest, GetMultiUserInfo) {
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::UserRegisterReq reg_request;
    blus::UserRegisterRsp reg_response;
    // 注册一个新用户
    std::string new_request_id = blus::uuid();
    reg_request.set_request_id(new_request_id);
    reg_request.set_nickname("NewUser2");
    reg_request.set_password("Password456");
    stub.UserRegister(&cntl, &reg_request, &reg_response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_TRUE(reg_response.success());
    if (!reg_response.success()) {
        std::cout << "Error: " << reg_response.errmsg() << std::endl;
        return;
    }
    EXPECT_EQ(reg_response.request_id(), new_request_id);
    std::cout << "请输入测试用户NewUser2对应的user_id:";
    std::string new_user_id;
    std::cin >> new_user_id;
    cntl.Reset();

    blus::GetMultiUserInfoReq request;
    blus::GetMultiUserInfoRsp response;

    std::string id = blus::uuid();
    request.set_request_id(id);
    request.add_users_id(user_id);
    request.add_users_id(new_user_id);
    stub.GetMultiUserInfo(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_TRUE(response.success());
    if (!response.success()) {
        std::cout << "Error: " << response.errmsg() << std::endl;
        return;
    }
    EXPECT_EQ(response.request_id(), id);
    // 检查返回的用户信息
    auto& user_info = *response.mutable_users_info();
    const auto& user1 = user_info[user_id];
    // std::cout << "获取user1信息成功" << std::endl;
    const auto& user2 = user_info[new_user_id];
    // std::cout << "获取user2信息成功" << std::endl;
    EXPECT_EQ(user1.user_id(), user_id);
    EXPECT_EQ(user1.nickname(), "NewNickname");
    EXPECT_EQ(user1.description(), "我就是我，不一样的烟火");
    EXPECT_EQ(user1.avatar(), "测试头像数据");
    EXPECT_EQ(user2.user_id(), new_user_id);
    EXPECT_EQ(user2.nickname(), "NewUser2");
}

std::string get_code(const std::string& email) {
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::EmailVerifyCodeReq request;
    blus::EmailVerifyCodeRsp response;
    std::string id = blus::uuid();

    request.set_request_id(id);
    request.set_email(email);
    stub.GetEmailVerifyCode(&cntl, &request, &response, nullptr);
    if (cntl.Failed()) {
        std::cout << "Error: " << cntl.ErrorText() << std::endl;
        return "";
    }
    if (!response.success()) {
        std::cout << "Error: " << response.errmsg() << std::endl;
        return "";
    }
    return response.verify_code_id();
}

std::string email_user_id;

TEST_F(UserServiceTest, EmailRegister) {
    auto vcid = get_code("test@abc.com");
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::EmailRegisterReq request;
    blus::EmailRegisterRsp response;

    std::string id = blus::uuid();
    request.set_request_id(id);
    request.set_email("test@abc.com");
    request.set_verify_code_id(vcid);
    request.set_verify_code("123456");
    stub.EmailRegister(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_TRUE(response.success());
    if (!response.success()) {
        std::cout << "Error: " << response.errmsg() << std::endl;
        return;
    }
    EXPECT_EQ(response.request_id(), id);
    std::cout << "请输入邮箱注册用户对应的user_id:";
    std::cin >> email_user_id;
}

std::string emain_login_ssid;

TEST_F(UserServiceTest, EmailLogin) {
    auto vcid = get_code("test@abc.com");
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::EmailLoginReq request;
    blus::EmailLoginRsp response;

    std::string id = blus::uuid();
    request.set_request_id(id);
    request.set_email("test@abc.com");
    request.set_verify_code_id(vcid);
    request.set_verify_code("123456");
    stub.EmailLogin(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_TRUE(response.success());
    if (!response.success()) {
        std::cout << "Error: " << response.errmsg() << std::endl;
        return;
    }
    EXPECT_EQ(response.request_id(), id);
    emain_login_ssid = response.login_session_id();
}

TEST_F(UserServiceTest, SetUserEmail) {
    auto vcid = get_code("test@abc.com");
    blus::UserService_Stub stub(addr.get());
    brpc::Controller cntl;

    blus::SetUserEmailReq request;
    blus::SetUserEmailRsp response;

    std::string id = blus::uuid();
    request.set_request_id(id);
    request.set_user_id(email_user_id);
    request.set_session_id(emain_login_ssid);
    request.set_email("newemail@abc.com");
    request.set_email_verify_code_id(vcid);
    request.set_email_verify_code("123456");
    stub.SetUserEmail(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    EXPECT_TRUE(response.success());
    if (!response.success()) {
        std::cout << "Error: " << response.errmsg() << std::endl;
        return;
    }
    EXPECT_EQ(response.request_id(), id);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("user.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));
    return RUN_ALL_TESTS();
}