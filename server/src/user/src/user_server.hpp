#pragma once
#include <brpc/server.h>
#include <butil/logging.h>
#include <filesystem>
#include <regex>
#include <random>

#include "base.pb.h"
#include "user.pb.h"
#include "file.pb.h"

#include "utils.hpp"
#include "etcd.hpp"
#include "data_es.hpp"
#include "data_mysql.hpp"
#include "data_redis.hpp"
#include "email.hpp"
#include "channel.hpp"
#include "llm.hpp"

namespace blus {
    class UserServiceImpl : public UserService {
    public:
        UserServiceImpl(const std::shared_ptr<elasticlient::Client>& es,
            const std::shared_ptr<odb::database>& mysql,
            const std::shared_ptr<sw::redis::Redis>& redis,
            const EmailSender::Ptr& email,
            const std::string& file_service_name,
            const ServiceManager::Ptr& sm,
            const Discovery::Ptr& discovery,
            const TextClassifier::Ptr& text_classifier)
            : _es(es), _mysql(mysql), _redis(redis)
            , _es_user(std::make_shared<ESUser>(_es))
            , _user_table(std::make_shared<UserTable>(_mysql))
            , _session(std::make_shared<Session>(_redis))
            , _status(std::make_shared<Status>(_redis))
            , _verify_code(std::make_shared<VerifyCode>(_redis))
            , _email(email)
            , _file_service_name(file_service_name)
            , _service_manager(sm)
            , _discovery(discovery)
            , _text_classifier(text_classifier) {
            if (!_es_user->createIndex()) {
                LOG_ERROR("创建es索引失败");
                exit(EXIT_FAILURE);
            }
        }
        ~UserServiceImpl() {}

        enum class nickname_status {
            NICKNAME_OK,
            NICKNAME_EXIST,
            NICKNAME_STYLE_ERROR, // 昵称不符合格式要求
            NICKNAME_INVALID, // 昵称敏感（模型检测不通过）
        };

        nickname_status check_nickname(const std::string& nickname) {
            if (nickname.length() > 32) {
                return nickname_status::NICKNAME_STYLE_ERROR;
            }
            std::string result;
            try {
                result = _text_classifier->classify(nickname);
            }
            catch (const std::exception& e) {
                LOG_ERROR("模型请求失败: {}", e.what());
                return nickname_status::NICKNAME_INVALID;
            }
            if (result == "不合规") {
                return nickname_status::NICKNAME_INVALID;
            }
            // 检测昵称对应用户是否存在
            auto user = _user_table->select_by_nickname(nickname);
            if (user) {
                return nickname_status::NICKNAME_EXIST;
            }
            return nickname_status::NICKNAME_OK;
        }

        enum class password_status {
            PASSWORD_OK,
            PASSWORD_TOO_SHORT,
            PASSWORD_TOO_LONG,
            PASSWORD_STYLE_ERROR,
        };

        password_status check_password(const std::string& password) {
            if (password.length() < 8) {
                return password_status::PASSWORD_TOO_SHORT;
            }
            if (password.length() > 32) {
                return password_status::PASSWORD_TOO_LONG;
            }
            // 至少一个字母和数字, 长度8-32, 不允许空格
            std::regex re(R"(^(?=.{8,32}$)(?=.*[A-Za-z])(?=.*\d)\S+$)");
            if (!std::regex_match(password, re)) {
                return password_status::PASSWORD_STYLE_ERROR;
            }
            return password_status::PASSWORD_OK;
        }

        bool check_email(const std::string& email) {
            // 总长不长于64, 必须有@
            if (email.length() > 64) {
                return false;
            }
            std::regex re(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
            return std::regex_match(email, re);
        }

        void UserRegister(google::protobuf::RpcController* controller,
            const UserRegisterReq* request,
            UserRegisterRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            const std::string& nickname = request->nickname();
            switch (check_nickname(nickname)) {
            case nickname_status::NICKNAME_OK:
                // 昵称正常，继续后续注册处理
                break;
            case nickname_status::NICKNAME_EXIST:
                response->set_errmsg("昵称已存在");
                response->set_success(false);
                return;
            case nickname_status::NICKNAME_STYLE_ERROR:
                response->set_errmsg("昵称格式错误");
                response->set_success(false);
                return;
            case nickname_status::NICKNAME_INVALID:
                response->set_errmsg("昵称敏感");
                response->set_success(false);
                return;
            default:
                LOG_CRITICAL("未知昵称状态");
                exit(EXIT_FAILURE);
            }
            const std::string& password = request->password();
            std::string sha256_password;
            switch (check_password(password)) {
            case password_status::PASSWORD_OK:
                // 密码正常，继续后续注册处理
                sha256_password = hashPassword(password);
                break;
            case password_status::PASSWORD_TOO_SHORT:
                response->set_errmsg("密码过短");
                response->set_success(false);
                return;
            case password_status::PASSWORD_TOO_LONG:
                response->set_errmsg("密码过长");
                response->set_success(false);
                return;
            case password_status::PASSWORD_STYLE_ERROR:
                response->set_errmsg("密码格式错误, 至少一个字母和数字, 长度8-32, 允许字母、数字和特殊字符, 不允许空格");
                response->set_success(false);
                return;
            default:
                LOG_CRITICAL("未知密码状态");
                exit(EXIT_FAILURE);
            }
            std::string uid = uuid();
            User user{ uid, nickname, sha256_password };
            if (!_user_table->insert(user)) {
                LOG_ERROR("{} - mysql数据库插入失败", request->request_id());
                response->set_errmsg("注册失败");
                response->set_success(false);
                return;
            }
            if (!_es_user->append(uid, "", nickname, "", "")) {
                LOG_ERROR("{} - es数据库插入失败", request->request_id());
                response->set_errmsg("注册失败");
                response->set_success(false);
                // mysql插入成功，es插入失败，删除mysql数据
                _user_table->remove(uid);
                return;
            }
            response->set_success(true);
        }

        void UserLogin(google::protobuf::RpcController* controller,
            const UserLoginReq* request,
            UserLoginRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            const std::string& nickname = request->nickname();
            const std::string& password = request->password();
            const std::string& sha256_password = hashPassword(password);
            auto user = _user_table->select_by_nickname(nickname);
            if (!user) {
                response->set_errmsg("用户不存在");
                response->set_success(false);
                return;
            }
            if (user->password() != sha256_password) {
                response->set_errmsg("密码错误");
                response->set_success(false);
                return;
            }
            if (_status->exists(user->user_id())) {
                response->set_errmsg("用户已在其它地方登录");
                response->set_success(false);
                return;
            }
            // 生成登录会话ID, 保存ID 维持登陆状态
            std::string ssid = uuid();
            _session->append(ssid, user->user_id());
            _status->append(user->user_id());
            response->set_login_session_id(ssid);
            response->set_success(true);
        }

        void GetEmailVerifyCode(google::protobuf::RpcController* controller,
            const EmailVerifyCodeReq* request,
            EmailVerifyCodeRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            const std::string& email = request->email();
            if (!check_email(email)) {
                response->set_errmsg("邮箱格式错误");
                response->set_success(false);
                return;
            }
            std::uniform_int_distribution<int> dist(0, 999999);
            int randomNum = dist(_rng);
            std::ostringstream oss;
            oss << std::setw(6) << std::setfill('0') << randomNum;
            std::string output;
            if (!_email->sendVerifyCode(email, oss.str(), &output)) {
                LOG_ERROR("{}-{} 邮件发送失败", request->request_id(), email);
                response->set_errmsg("邮件发送失败");
                response->set_success(false);
                return;
            }
            std::string cid = uuid();
            // 10分钟内有效
            if (!_verify_code->append(cid, oss.str(), 600)) {
                LOG_ERROR("{} - redis数据库插入失败", request->request_id());
                response->set_errmsg("验证码存储失败");
                response->set_success(false);
                return;
            }
            response->set_verify_code_id(cid);
            response->set_success(true);
        }

        void EmailRegister(google::protobuf::RpcController* controller,
            const EmailRegisterReq* request,
            EmailRegisterRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            const std::string& email = request->email();
            const std::string& verify_code_id = request->verify_code_id();
            const std::string& verify_code = request->verify_code();
            // 检测邮箱格式
            if (!check_email(email)) {
                response->set_errmsg("邮箱格式错误");
                response->set_success(false);
                return;
            }
            // 检测验证码是否正确
            std::string redis_code = _verify_code->code(verify_code_id).value();
            if (redis_code != verify_code) {
                response->set_errmsg("验证码错误");
                response->set_success(false);
                return;
            }
            // 判断邮箱是否已被注册过
            if (_user_table->select_by_email(email)) {
                response->set_errmsg("邮箱已被注册");
                response->set_success(false);
                return;
            }
            // mysql插入用户
            std::string uid = uuid();
            User user{ uid, email };
            if (!_user_table->insert(user)) {
                LOG_ERROR("{} - mysql数据库插入失败", request->request_id());
                response->set_errmsg("注册失败");
                response->set_success(false);
                return;
            }
            // es插入用户
            if (!_es_user->append(uid, email, user.nickname(), "", "")) {
                LOG_ERROR("{} - es数据库插入失败", request->request_id());
                response->set_errmsg("注册失败");
                response->set_success(false);
                // mysql插入成功，es插入失败，删除mysql数据
                _user_table->remove(uid);
                return;
            }
            response->set_success(true);
            // 删除验证码
            _verify_code->remove(verify_code_id);
        }

        void EmailLogin(google::protobuf::RpcController* controller,
            const EmailLoginReq* request,
            EmailLoginRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            const std::string& email = request->email();
            const std::string& verify_code_id = request->verify_code_id();
            const std::string& verify_code = request->verify_code();
            if (!check_email(email)) {
                response->set_errmsg("邮箱格式错误");
                response->set_success(false);
                return;
            }
            auto user = _user_table->select_by_email(email);
            if (!user) {
                response->set_errmsg("用户不存在");
                response->set_success(false);
                return;
            }
            // 检测验证码是否正确
            auto redis_code = _verify_code->code(verify_code_id);
            if (redis_code != verify_code) {
                response->set_errmsg("验证码错误");
                response->set_success(false);
                return;
            }
            // 删除验证码
            _verify_code->remove(verify_code_id);
            // 检测用户是否登录
            if (_status->exists(user->user_id())) {
                response->set_errmsg("用户已在其它地方登录");
                response->set_success(false);
                return;
            }
            // 生成登录会话ID, 保存ID 维持登陆状态
            std::string ssid = uuid();
            _session->append(ssid, user->user_id());
            _status->append(user->user_id());
            response->set_login_session_id(ssid);
            response->set_success(true);
        }

        // -------- 用户已登录 --------
        void GetUserInfo(google::protobuf::RpcController* controller,
            const GetUserInfoReq* request,
            GetUserInfoRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            const std::string& uid = request->user_id();
            auto user = _user_table->select_by_uid(uid);
            if (!user) {
                LOG_ERROR("{}-{} mysql数据库查询失败: 未找到用户信息", request->request_id(), uid);
                response->set_errmsg("用户不存在");
                response->set_success(false);
                return;
            }
            // 调用file服务获取头像
            UserInfo* user_info = response->mutable_user_info();
            user_info->set_user_id(user->user_id());
            user_info->set_nickname(user->nickname());
            user_info->set_description(user->description());
            user_info->set_email(user->email());
            const auto& avatar_id = user->avatar_id();
            if (!avatar_id.empty()) {
                auto channel = _service_manager->get(_file_service_name);
                if (!channel) {
                    LOG_ERROR("{}-{} 获取file服务失败", request->request_id(), uid);
                    response->set_errmsg("获取file服务失败");
                    response->set_success(false);
                    return;
                }
                FileService_Stub stub(channel.get());
                GetSingleFileReq file_request;
                file_request.set_request_id(request->request_id());
                file_request.set_file_id(avatar_id);
                GetSingleFileRsp file_response;
                brpc::Controller cntl;
                stub.GetSingleFile(&cntl, &file_request, &file_response, nullptr);
                if (cntl.Failed() || !file_response.success()) {
                    LOG_ERROR("{}-{} file服务查询失败: {}", request->request_id(), uid, cntl.ErrorText());
                    response->set_errmsg("获取头像失败");
                    response->set_success(false);
                    return;
                }
                user_info->set_avatar(file_response.file_data().file_content());
            }
            response->set_success(true);
        }

        void GetMultiUserInfo(google::protobuf::RpcController* controller,
            const GetMultiUserInfoReq* request,
            GetMultiUserInfoRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            // 获取请求中的用户ID（可能包含重复项）
            const auto& user_id_list = request->users_id();
            std::vector<std::string> original_ids;
            original_ids.reserve(user_id_list.size());
            for (int i = 0; i < user_id_list.size(); ++i) {
                original_ids.push_back(user_id_list.Get(i));
            }
            // 去重
            std::vector<std::string> dedup_ids;
            std::unordered_set<std::string> seen;
            for (const auto& id : original_ids) {
                if (seen.insert(id).second) {
                    dedup_ids.push_back(id);
                }
            }
            // 查询去重后的用户信息
            auto users = _user_table->select_multi_users(dedup_ids);
            if (users.size() != dedup_ids.size()) {
                LOG_ERROR("{} - mysql数据库查询失败: 批量查找结果与要求不一致, 请求去重数量{}, 结果数量{}",
                    request->request_id(), dedup_ids.size(), users.size());
                response->set_errmsg("用户不存在");
                response->set_success(false);
                return;
            }
            // 构造用户映射
            std::unordered_map<std::string, std::shared_ptr<User>> user_map;
            for (const auto& user : users) {
                user_map[user->user_id()] = user;
            }
            auto* out_map = response->mutable_users_info();
            for (const auto& [k, v] : user_map) {
                UserInfo info;
                info.set_user_id(v->user_id());
                info.set_nickname(v->nickname());
                info.set_description(v->description());
                info.set_email(v->email());
                (*out_map)[k] = info;
            }
            // 收集所有需要查询头像的 avatar_id（去重）
            std::unordered_set<std::string> avatar_set;
            for (const auto& [_, v] : user_map) {
                if (!v->avatar_id().empty()) {
                    avatar_set.insert(v->avatar_id());
                }
            }
            if (!avatar_set.empty()) {
                auto channel = _service_manager->get(_file_service_name);
                if (!channel) {
                    LOG_ERROR("{} - 获取file服务失败", request->request_id());
                    response->set_errmsg("获取file服务失败");
                    response->set_success(false);
                    return;
                }
                FileService_Stub stub(channel.get());
                GetMultiFileReq file_request;
                file_request.set_request_id(request->request_id());
                for (const auto& aid : avatar_set) {
                    file_request.add_file_id_list(aid);
                }
                GetMultiFileRsp file_response;
                brpc::Controller cntl;
                stub.GetMultiFile(&cntl, &file_request, &file_response, nullptr);
                if (cntl.Failed() || !file_response.success()) {
                    LOG_ERROR("{} - file服务查询失败: {}", request->request_id(), cntl.ErrorText());
                    response->set_errmsg("获取头像失败");
                    response->set_success(false);
                    return;
                }
                const auto& file_map = file_response.file_data();
                // 更新 map 中各用户的头像字段
                for (auto& [k, v] : *out_map) {
                    auto it = user_map.find(k);
                    if (it != user_map.end() && !it->second->avatar_id().empty()) {
                        v.set_avatar(file_map.at(it->second->avatar_id()).file_content());
                    }
                }
            }
            response->set_success(true);
        }

        void SetUserAvatar(google::protobuf::RpcController* controller,
            const SetUserAvatarReq* request,
            SetUserAvatarRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            const std::string& uid = request->user_id();
            const std::string& avatar = request->avatar();
            auto user = _user_table->select_by_uid(uid);
            auto old_avatar_id = user->avatar_id();
            if (!user) {
                LOG_ERROR("{}-{} mysql数据库查询失败: 未找到用户信息", request->request_id(), uid);
                response->set_errmsg("用户不存在");
                response->set_success(false);
                return;
            }
            // 调用file服务上传头像
            auto channel = _service_manager->get(_file_service_name);
            if (!channel) {
                LOG_ERROR("{} - 获取file服务失败", request->request_id());
                response->set_errmsg("获取file服务失败");
                response->set_success(false);
                return;
            }
            FileService_Stub stub(channel.get());
            PutSingleFileReq file_request;
            file_request.set_request_id(request->request_id());
            file_request.mutable_file_data()->set_file_content(avatar);
            file_request.mutable_file_data()->set_file_name("avatar_" + uid + ".jpg");
            file_request.mutable_file_data()->set_file_size(avatar.size());
            blus::PutSingleFileRsp file_response;
            brpc::Controller cntl;
            stub.PutSingleFile(&cntl, &file_request, &file_response, nullptr);
            if (cntl.Failed() || !file_response.success()) {
                LOG_ERROR("{}-{} file服务上传失败: {}", request->request_id(), uid, cntl.ErrorText());
                response->set_errmsg("头像上传失败");
                response->set_success(false);
                return;
            }
            const auto& avatar_id = file_response.file_info().file_id();
            // 更新es用户头像
            if (!_es_user->append(user->user_id(), user->email(), user->nickname(), user->description(), avatar_id)) {
                LOG_ERROR("{}-{} es数据库更新失败: 更新用户头像失败", request->request_id(), uid);
                response->set_errmsg("头像更新失败");
                response->set_success(false);
                return;
            }
            // 更新mysql用户头像
            user->avatar_id(avatar_id);
            if (!_user_table->update(user)) {
                LOG_ERROR("{}-{} mysql数据库更新失败: 更新用户头像失败", request->request_id(), uid);
                response->set_errmsg("头像更新失败");
                response->set_success(false);
                // es更新成功，mysql更新失败，还原es数据
                if (!_es_user->append(user->user_id(), user->email(), user->nickname(), user->description(), old_avatar_id)) {
                    LOG_CRITICAL("{}-{} es数据库恢复头像失败", request->request_id(), uid);
                }
                return;
            }
            response->set_success(true);
        }

        void SetUserNickname(google::protobuf::RpcController* controller,
            const SetUserNicknameReq* request,
            SetUserNicknameRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            const std::string& uid = request->user_id();
            const std::string& nickname = request->nickname();
            switch (check_nickname(nickname)) {
            case nickname_status::NICKNAME_OK:
                // 昵称正常，继续后续更新处理
                break;
            case nickname_status::NICKNAME_EXIST:
                response->set_errmsg("昵称已存在");
                response->set_success(false);
                return;
            case nickname_status::NICKNAME_STYLE_ERROR:
                response->set_errmsg("昵称格式错误");
                response->set_success(false);
                return;
            case nickname_status::NICKNAME_INVALID:
                response->set_errmsg("昵称敏感");
                response->set_success(false);
                return;
            default:
                LOG_CRITICAL("未知昵称状态");
                exit(EXIT_FAILURE);
            }
            auto user = _user_table->select_by_uid(uid);
            auto old_nickname = user->nickname();
            if (!user) {
                LOG_ERROR("{}-{} mysql数据库查询失败: 未找到用户信息", request->request_id(), uid);
                response->set_errmsg("用户不存在");
                response->set_success(false);
                return;
            }
            // 更新es用户昵称
            if (!_es_user->append(user->user_id(), user->email(), nickname, user->description(), user->avatar_id())) {
                LOG_ERROR("{}-{} es数据库更新失败: 更新用户昵称失败", request->request_id(), uid);
                response->set_errmsg("昵称更新失败");
                response->set_success(false);
                return;
            }
            // 更新mysql用户昵称
            user->nickname(nickname);
            if (!_user_table->update(user)) {
                LOG_ERROR("{}-{} mysql数据库更新失败: 更新用户昵称失败", request->request_id(), uid);
                response->set_errmsg("昵称更新失败");
                response->set_success(false);
                // es更新成功，mysql更新失败，恢复es数据
                if (!_es_user->append(user->user_id(), user->email(), old_nickname, user->description(), user->avatar_id())) {
                    LOG_CRITICAL("{}-{} es数据库恢复昵称失败", request->request_id(), uid);
                }
                return;
            }
            response->set_success(true);
        }

        void SetUserDescription(google::protobuf::RpcController* controller,
            const SetUserDescriptionReq* request,
            SetUserDescriptionRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            const std::string& uid = request->user_id();
            const std::string& description = request->description();
            if (description.length() > 256) {
                response->set_errmsg("签名过长");
                response->set_success(false);
                return;
            }
            // 检测签名是否敏感
            std::string result;
            try {
                result = _text_classifier->classify(description);
            }
            catch (const std::exception& e) {
                LOG_ERROR("模型请求失败: {}", e.what());
                response->set_errmsg("签名敏感");
                response->set_success(false);
                return;
            }
            if (result == "不合规") {
                response->set_errmsg("签名敏感");
                response->set_success(false);
                return;
            }
            // 检测用户是否存在
            auto user = _user_table->select_by_uid(uid);
            auto old_description = user->description();
            if (!user) {
                LOG_ERROR("{}-{} mysql数据库查询失败: 未找到用户信息", request->request_id(), uid);
                response->set_errmsg("用户不存在");
                response->set_success(false);
                return;
            }
            // 更新es用户签名
            if (!_es_user->append(user->user_id(), user->email(), user->nickname(), description, user->avatar_id())) {
                LOG_ERROR("{}-{} es数据库更新失败: 更新用户签名失败", request->request_id(), uid);
                response->set_errmsg("签名更新失败");
                response->set_success(false);
                return;
            }
            // 更新mysql用户签名
            user->description(description);
            if (!_user_table->update(user)) {
                LOG_ERROR("{}-{} mysql数据库更新失败: 更新用户签名失败", request->request_id(), uid);
                response->set_errmsg("签名更新失败");
                response->set_success(false);
                // es更新成功，mysql更新失败，恢复es数据
                if (!_es_user->append(user->user_id(), user->email(), user->nickname(), old_description, user->avatar_id())) {
                    LOG_CRITICAL("{}-{} es数据库恢复签名失败", request->request_id(), uid);
                }
                return;
            }
            response->set_success(true);
        }

        void SetUserEmail(google::protobuf::RpcController* controller,
            const SetUserEmailReq* request,
            SetUserEmailRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            const std::string& uid = request->user_id();
            const std::string& email = request->email();
            // 检测邮箱格式
            if (!check_email(email)) {
                response->set_errmsg("邮箱格式错误");
                response->set_success(false);
                return;
            }
            const std::string& verify_code_id = request->email_verify_code_id();
            const std::string& verify_code = request->email_verify_code();
            // 检测验证码是否正确
            auto redis_code = _verify_code->code(verify_code_id);
            if (redis_code != verify_code) {
                response->set_errmsg("验证码错误");
                response->set_success(false);
                return;
            }
            // 删除验证码
            _verify_code->remove(verify_code_id);
            auto user = _user_table->select_by_uid(uid);
            auto old_email = user->email();
            if (!user) {
                LOG_ERROR("{}-{} mysql数据库查询失败: 未找到用户信息", request->request_id(), uid);
                response->set_errmsg("用户不存在");
                response->set_success(false);
                return;
            }
            // 更新es用户邮箱
            if (!_es_user->append(user->user_id(), email, user->nickname(), user->description(), user->avatar_id())) {
                LOG_ERROR("{}-{} es数据库更新失败: 更新用户邮箱失败", request->request_id(), uid);
                response->set_errmsg("邮箱更新失败");
                response->set_success(false);
                return;
            }
            // 更新mysql用户邮箱
            user->email(email);
            if (!_user_table->update(user)) {
                LOG_ERROR("{}-{} mysql数据库更新失败: 更新用户邮箱失败", request->request_id(), uid);
                response->set_errmsg("邮箱更新失败");
                response->set_success(false);
                // es更新成功，mysql更新失败，恢复es数据
                if (!_es_user->append(user->user_id(), old_email, user->nickname(), user->description(), user->avatar_id())) {
                    LOG_CRITICAL("{}-{} es数据库恢复邮箱失败", request->request_id(), uid);
                }
                return;
            }
            response->set_success(true);
        }
    private:
        std::shared_ptr<elasticlient::Client> _es;
        std::shared_ptr<odb::database> _mysql;
        std::shared_ptr<sw::redis::Redis> _redis;
        ESUser::Ptr _es_user;
        UserTable::Ptr _user_table;
        Session::Ptr _session;
        Status::Ptr _status;
        VerifyCode::Ptr _verify_code;
        EmailSender::Ptr _email;
        std::string _file_service_name;
        ServiceManager::Ptr _service_manager;
        Discovery::Ptr _discovery;
        std::mt19937 _rng{ std::random_device{}() };
        TextClassifier::Ptr _text_classifier;
    };

    class UserServer {
    public:
        using Ptr = std::shared_ptr<UserServer>;
        UserServer(const Discovery::Ptr& dis, const Registry::Ptr& reg, const std::shared_ptr<brpc::Server>& server)
            : _discovery(dis), _reg(reg), _server(server) {
        }
        ~UserServer() {}

        void start() {
            _server->RunUntilAskedToQuit();
        }
    private:
        Discovery::Ptr _discovery;
        Registry::Ptr _reg;
        std::shared_ptr<brpc::Server> _server;
    };

    class UserServerBuilder {
    public:
        UserServerBuilder(const std::string& file_service_name) : _file_service_name(file_service_name) {}

        // 设置es客户端
        bool make_es(const std::vector<std::string>& host_list) {
            _es = ESFactory::create(host_list);
            return true;
        }

        // 设置mysql客户端
        bool make_mysql(const std::string& user,
            const std::string& pswd,
            const std::string& db,
            const std::string& host,
            const std::string& sock_path = "",
            int conn_pool_count = 10,
            int port = 3306,
            const std::string& cset = "utf8") {
            _mysql = ODBFactory::create(user, pswd, db, host, sock_path, conn_pool_count, port, cset);
            return true;
        }

        // 设置redis客户端
        bool make_redis(int db = 0, // 数据库号
            const std::string& host = "localhost",
            int port = 6379,
            bool keep_alive = true) {
            _redis = RedisFactory::create(db, host, port, keep_alive);
            return true;
        }

        // 设置邮件服务
        bool make_email(const std::string& from,
            const std::string& smtp,
            const std::string& username,
            const std::string& password,
            const std::string& content_type = "html") {
            _email = std::make_shared<EmailSender>(from, smtp, username, password, content_type);
            return true;
        }

        // 设置etcd服务(包括discovery和registry)
        bool make_etcd(const std::string& etcd_addr,
            const std::string& user_service_name,
            const std::string& instance_name,
            const std::string& service_ip,
            int32_t service_port,
            int etcd_timeout) {
            _service_manager = std::make_shared<ServiceManager>();
            _service_manager->declare(_file_service_name);
            _discovery = std::make_shared<Discovery>(_file_service_name, etcd_addr,
                [this](const std::string& name, const std::string& ip) {
                    _service_manager->online(name, ip);
                },
                [this](const std::string& name, const std::string& ip) {
                    _service_manager->offline(name, ip);
                });
            _reg = make_shared<Registry>(user_service_name, etcd_addr, etcd_timeout);
            _reg->registry(instance_name, service_ip + ":" + to_string(service_port));
            return true;
        }

        // 设置rpc服务
        bool make_rpc(int32_t listen_port, uint8_t thread_num, int rpc_timeout, const std::string& classifier_ip, int32_t classifier_port, const std::string& classifier_service_name) {
            _server = make_shared<brpc::Server>();

            auto service = new UserServiceImpl(_es, _mysql, _redis, _email, _file_service_name, _service_manager, _discovery,
                std::make_shared<TextClassifier>(classifier_ip, classifier_port, classifier_service_name));
            int ret = _server->AddService(service, brpc::SERVER_OWNS_SERVICE);
            if (ret != 0) {
                LOG_ERROR("UserServer添加服务失败");
                return false;
            }

            brpc::ServerOptions options;
            options.idle_timeout_sec = rpc_timeout;
            options.num_threads = thread_num;

            ret = _server->Start(listen_port, &options);
            if (ret != 0) {
                LOG_ERROR("UserServer启动失败");
                return false;
            }
            return true;
        }

        UserServer::Ptr build() {
            if (!_es) {
                LOG_ERROR("es服务未设置");
                return nullptr;
            }
            if (!_mysql) {
                LOG_ERROR("mysql服务未设置");
                return nullptr;
            }
            if (!_redis) {
                LOG_ERROR("redis服务未设置");
                return nullptr;
            }
            if (!_discovery || !_service_manager || !_reg) {
                LOG_ERROR("etcd服务未设置");
                return nullptr;
            }
            if (!_server) {
                LOG_ERROR("rpc服务未设置");
                return nullptr;
            }
            return make_shared<UserServer>(_discovery, _reg, _server);
        }
    private:
        Registry::Ptr _reg;
        std::shared_ptr<elasticlient::Client> _es;
        std::shared_ptr<odb::database> _mysql;
        std::shared_ptr<sw::redis::Redis> _redis;
        EmailSender::Ptr _email;
        ServiceManager::Ptr _service_manager;
        std::string _file_service_name;
        Discovery::Ptr _discovery;
        std::shared_ptr<brpc::Server> _server;
    };
}