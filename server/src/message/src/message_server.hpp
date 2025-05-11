#pragma once
#include <brpc/server.h>
#include <butil/logging.h>
#include <filesystem>

#include "base.pb.h"
#include "file.pb.h"
#include "user.pb.h"
#include "message.pb.h"

#include "utils.hpp"
#include "etcd.hpp"
#include "data_es.hpp"
#include "data_mysql.hpp"
#include "rabbitmq.hpp"
#include "channel.hpp"
#include "logger.hpp"

namespace blus {
    class MsgStorageServiceImpl : public MsgStorageService {
    public:
        MsgStorageServiceImpl(const std::shared_ptr<elasticlient::Client>& es,
            const std::shared_ptr<odb::database>& mysql,
            const std::string& file_service_name,
            const std::string& user_service_name,
            const ServiceManager::Ptr& sm)
            : _es(es), _mysql(mysql)
            , _es_message(std::make_shared<ESMessage>(_es))
            , _message_table(std::make_shared<MessageTable>(_mysql))
            , _file_service_name(file_service_name)
            , _user_service_name(user_service_name)
            , _service_manager(sm) {
        }
        ~MsgStorageServiceImpl() {}

        void GetHistoryMsg(google::protobuf::RpcController* controller,
            const GetHistoryMsgReq* request,
            GetHistoryMsgRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            const auto& chat_session_id = request->chat_session_id();
            auto start = boost::posix_time::from_time_t(request->start_time());
            auto end = boost::posix_time::from_time_t(request->over_time());
            auto msg_list = _message_table->get_range(chat_session_id, start, end);
            // 调用file服务批量获取文件内容
            std::vector<std::string> file_ids;
            for (const auto& msg : msg_list) {
                if (msg.file_id().empty()) {
                    continue;
                }
                file_ids.push_back(msg.file_id());
            }
            auto file_data = _get_files(request->request_id(), file_ids);
            if (file_data.size() != file_ids.size()) {
                LOG_ERROR("获取文件内容失败");
                response->set_success(false);
                response->set_errmsg("获取文件内容失败");
                return;
            }
            // 调用user服务批量获取用户信息
            std::vector<std::string> user_ids;
            for (const auto& msg : msg_list) {
                user_ids.push_back(msg.user_id());
            }
            auto user_info = _get_users(request->request_id(), user_ids);
            if (user_info.size() != user_ids.size()) {
                LOG_ERROR("获取用户信息失败");
                response->set_success(false);
                response->set_errmsg("获取用户信息失败");
                return;
            }
            // 组装返回数据
            for (const auto& msg : msg_list) {
                auto message_info = response->add_msg_list();
                message_info->set_message_id(msg.message_id());
                message_info->set_chat_session_id(msg.session_id());
                message_info->set_timestamp(boost::posix_time::to_time_t(msg.create_time()));
                message_info->mutable_sender()->CopyFrom(user_info[msg.user_id()]);
                switch (msg.message_type()) {
                case MessageType::STRING:
                    message_info->mutable_message()->set_message_type(MessageType::STRING);
                    message_info->mutable_message()->mutable_string_message()->set_content(msg.content());
                    break;
                case MessageType::FILE:
                    message_info->mutable_message()->set_message_type(MessageType::FILE);
                    message_info->mutable_message()->mutable_file_message()->set_file_name(msg.file_name());
                    message_info->mutable_message()->mutable_file_message()->set_file_size(msg.file_size());
                    message_info->mutable_message()->mutable_file_message()->set_file_id(msg.file_id());
                    message_info->mutable_message()->mutable_file_message()->set_file_contents(file_data[msg.file_id()].file_content());
                    break;
                case MessageType::IMAGE:
                    message_info->mutable_message()->set_message_type(MessageType::IMAGE);
                    message_info->mutable_message()->mutable_image_message()->set_file_id(msg.file_id());
                    message_info->mutable_message()->mutable_image_message()->set_image_content(file_data[msg.file_id()].file_content());
                    break;
                case MessageType::SPEECH:
                    message_info->mutable_message()->set_message_type(MessageType::SPEECH);
                    message_info->mutable_message()->mutable_speech_message()->set_file_id(msg.file_id());
                    message_info->mutable_message()->mutable_speech_message()->set_file_contents(file_data[msg.file_id()].file_content());
                    break;
                default:
                    LOG_CRITICAL("未知消息类型{}", msg.message_type());
                    abort();
                }
            }
            response->set_success(true);
        }

        void GetRecentMsg(google::protobuf::RpcController* controller,
            const GetRecentMsgReq* request,
            GetRecentMsgRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            const auto& chat_session_id = request->chat_session_id();
            auto msg_list = _message_table->get_recent(chat_session_id, request->msg_count());
            // 调用file服务批量获取文件内容
            std::vector<std::string> file_ids;
            for (const auto& msg : msg_list) {
                if (msg.file_id().empty()) {
                    continue;
                }
                file_ids.push_back(msg.file_id());
            }
            auto file_data = _get_files(request->request_id(), file_ids);
            if (file_data.size() != file_ids.size()) {
                LOG_ERROR("获取文件内容失败");
                response->set_success(false);
                response->set_errmsg("获取文件内容失败");
                return;
            }
            // 调用user服务批量获取用户信息
            std::vector<std::string> user_ids;
            for (const auto& msg : msg_list) {
                user_ids.push_back(msg.user_id());
            }
            auto user_info = _get_users(request->request_id(), user_ids);
            if (user_info.size() != user_ids.size()) {
                LOG_ERROR("获取用户信息失败");
                response->set_success(false);
                response->set_errmsg("获取用户信息失败");
                return;
            }
            // 组装返回数据
            for (const auto& msg : msg_list) {
                auto message_info = response->add_msg_list();
                message_info->set_message_id(msg.message_id());
                message_info->set_chat_session_id(msg.session_id());
                message_info->set_timestamp(boost::posix_time::to_time_t(msg.create_time()));
                message_info->mutable_sender()->CopyFrom(user_info[msg.user_id()]);
                switch (msg.message_type()) {
                case MessageType::STRING:
                    message_info->mutable_message()->set_message_type(MessageType::STRING);
                    message_info->mutable_message()->mutable_string_message()->set_content(msg.content());
                    break;
                case MessageType::FILE:
                    message_info->mutable_message()->set_message_type(MessageType::FILE);
                    message_info->mutable_message()->mutable_file_message()->set_file_name(msg.file_name());
                    message_info->mutable_message()->mutable_file_message()->set_file_size(msg.file_size());
                    message_info->mutable_message()->mutable_file_message()->set_file_id(msg.file_id());
                    message_info->mutable_message()->mutable_file_message()->set_file_contents(file_data[msg.file_id()].file_content());
                    break;
                case MessageType::IMAGE:
                    message_info->mutable_message()->set_message_type(MessageType::IMAGE);
                    message_info->mutable_message()->mutable_image_message()->set_file_id(msg.file_id());
                    message_info->mutable_message()->mutable_image_message()->set_image_content(file_data[msg.file_id()].file_content());
                    break;
                case MessageType::SPEECH:
                    message_info->mutable_message()->set_message_type(MessageType::SPEECH);
                    message_info->mutable_message()->mutable_speech_message()->set_file_id(msg.file_id());
                    message_info->mutable_message()->mutable_speech_message()->set_file_contents(file_data[msg.file_id()].file_content());
                    break;
                default:
                    LOG_CRITICAL("未知消息类型{}", msg.message_type());
                    abort();
                }
            }
            response->set_success(true);
        }

        void MsgSearch(google::protobuf::RpcController* controller,
            const MsgSearchReq* request,
            MsgSearchRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            const auto& chat_session_id = request->chat_session_id();
            auto msg_list = _es_message->search(request->search_key(), chat_session_id);
            // 调用user服务批量获取用户信息
            std::vector<std::string> user_ids;
            for (const auto& msg : msg_list) {
                user_ids.push_back(msg.user_id());
            }
            auto user_info = _get_users(request->request_id(), user_ids);
            if (user_info.size() != user_ids.size()) {
                LOG_ERROR("获取用户信息失败");
                response->set_success(false);
                response->set_errmsg("获取用户信息失败");
                return;
            }
            // 组装返回数据
            for (const auto& msg : msg_list) {
                auto message_info = response->add_msg_list();
                message_info->set_message_id(msg.message_id());
                message_info->set_chat_session_id(msg.session_id());
                message_info->set_timestamp(boost::posix_time::to_time_t(msg.create_time()));
                message_info->mutable_sender()->CopyFrom(user_info[msg.user_id()]);
                message_info->mutable_message()->set_message_type(MessageType::STRING);
                message_info->mutable_message()->mutable_string_message()->set_content(msg.content());
            }
            response->set_success(true);
        }

        // RabbitMQ消息回调函数 
        void onMessage(const std::string& message) {
            MessageInfo msg;
            if (!msg.ParseFromString(message)) {
                LOG_ERROR("RabbitMQ消息反系列化失败");
                return;
            }
            auto put_file = [this](const std::string& file_name, const std::string& data, std::string& file_id) {
                auto channel = _service_manager->get(_file_service_name);
                if (!channel) {
                    LOG_ERROR("没有可用的文件服务节点");
                    return false;
                }
                FileService_Stub stub(channel.get());
                brpc::Controller cntl;
                PutSingleFileReq req;
                PutSingleFileRsp rsp;
                req.set_request_id(uuid());
                req.mutable_file_data()->set_file_name(file_name);
                req.mutable_file_data()->set_file_content(data);
                req.mutable_file_data()->set_file_size(data.size());
                stub.PutSingleFile(&cntl, &req, &rsp, nullptr);
                if (cntl.Failed() || rsp.success() == false) {
                    LOG_ERROR("PutSingleFile RPC失败{}: {}", req.request_id(), cntl.Failed() ? cntl.ErrorText() : rsp.errmsg());
                    return false;
                }
                file_id = rsp.file_info().file_id();
                return true;
                };
            Message sql_msg{ msg.message_id(),
                msg.sender().user_id(),
                msg.chat_session_id(),
                static_cast<unsigned char>(msg.message().message_type()),
                boost::posix_time::from_time_t(msg.timestamp()) };
            std::string file_id;
            switch (msg.message().message_type()) {
            case MessageType::STRING:
                // 插入es
                if (!_es_message->append(msg.sender().user_id(),
                    msg.message_id(),
                    msg.chat_session_id(),
                    boost::posix_time::from_time_t(msg.timestamp()),
                    msg.message().string_message().content())) {
                    LOG_ERROR("持久化消息插入es失败{}", msg.message_id());
                    return;
                }
                break;
            case MessageType::FILE:
                put_file(msg.message().file_message().file_name(),
                    msg.message().file_message().file_contents(),
                    file_id);
                sql_msg.file_name(msg.message().file_message().file_name());
                sql_msg.file_size(msg.message().file_message().file_size());
                break;
            case MessageType::IMAGE:
                put_file("",
                    msg.message().image_message().image_content(),
                    file_id);
                break;
            case MessageType::SPEECH:
                put_file("",
                    msg.message().speech_message().file_contents(),
                    file_id);
                break;
            default:
                LOG_CRITICAL("未知消息类型{}", msg.message().message_type());
                abort();
            }
            if (msg.message().message_type() == MessageType::STRING) sql_msg.content(msg.message().string_message().content());
            else sql_msg.file_id(file_id);
            // 插入mysql
            if (!_message_table->insert(sql_msg)) {
                LOG_ERROR("持久化消息插入mysql失败{}", msg.message_id());
                // 如果是文本消息，则删除es中的索引
                if (msg.message().message_type() == MessageType::STRING &&
                    !_es_message->remove(msg.message_id())) {
                    LOG_CRITICAL("持久化消息保持一致性失败{}", msg.message_id());
                }
            }
        }
    private:
        google::protobuf::Map<std::string, FileDownloadData> _get_files(const std::string& request_id, const std::vector<std::string>& file_ids) {
            auto channel = _service_manager->get(_file_service_name);
            if (!channel) {
                LOG_ERROR("没有可用的文件服务节点");
                return {};
            }
            FileService_Stub stub(channel.get());
            brpc::Controller cntl;
            GetMultiFileReq req;
            GetMultiFileRsp rsp;
            req.set_request_id(request_id);
            for (const auto& file_id : file_ids) {
                req.add_file_id_list(file_id);
            }
            stub.GetMultiFile(&cntl, &req, &rsp, nullptr);
            if (cntl.Failed() || rsp.success() == false) {
                LOG_ERROR("GetFile RPC失败{}: {}", request_id, cntl.Failed() ? cntl.ErrorText() : rsp.errmsg());
                return {};
            }
            return rsp.file_data();
        }

        google::protobuf::Map<std::string, UserInfo> _get_users(const std::string& request_id, const std::vector<std::string>& user_ids) {
            auto channel = _service_manager->get(_user_service_name);
            if (!channel) {
                LOG_ERROR("没有可用的用户服务节点");
                return {};
            }
            UserService_Stub stub(channel.get());
            brpc::Controller cntl;
            GetMultiUserInfoReq req;
            GetMultiUserInfoRsp rsp;
            req.set_request_id(request_id);
            for (const auto& user_id : user_ids) {
                req.add_users_id(user_id);
            }
            stub.GetMultiUserInfo(&cntl, &req, &rsp, nullptr);
            if (cntl.Failed() || rsp.success() == false) {
                LOG_ERROR("GetUserInfo RPC失败{}: {}", request_id, cntl.Failed() ? cntl.ErrorText() : rsp.errmsg());
                return {};
            }
            return rsp.users_info();
        }

        std::shared_ptr<elasticlient::Client> _es;
        std::shared_ptr<odb::database> _mysql;
        ESMessage::Ptr _es_message;
        MessageTable::Ptr _message_table;
        std::string _file_service_name;
        std::string _user_service_name;
        ServiceManager::Ptr _service_manager;
    };

    class MsgStorageServer {
    public:
        using Ptr = std::shared_ptr<MsgStorageServer>;
        MsgStorageServer(const Discovery::Ptr& file_dis, const Discovery::Ptr& user_dis, const Registry::Ptr& reg, const std::shared_ptr<brpc::Server>& server)
            : _file_dis(file_dis), _user_dis(user_dis), _reg(reg), _server(server) {
        }
        ~MsgStorageServer() {}

        void start() {
            _server->RunUntilAskedToQuit();
        }
    private:
        Discovery::Ptr _file_dis, _user_dis;
        Registry::Ptr _reg;
        std::shared_ptr<brpc::Server> _server;
    };

    class MsgStorageServerBuilder {
    public:
        MsgStorageServerBuilder(const std::string& file_service_name, const std::string& user_service_name)
            : _file_service_name(file_service_name)
            , _user_service_name(user_service_name) {
        }

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

        // 设置etcd服务(包括discovery和registry)
        bool make_etcd(const std::string& etcd_addr,
            const std::string& message_service_name,
            const std::string& instance_name,
            const std::string& service_ip,
            int32_t service_port,
            int etcd_timeout) {
            _service_manager = std::make_shared<ServiceManager>();
            _service_manager->declare(_file_service_name);
            _service_manager->declare(_user_service_name);
            _file_dis = std::make_shared<Discovery>(_file_service_name, etcd_addr,
                [this](const std::string& name, const std::string& ip) {
                    _service_manager->online(name, ip);
                },
                [this](const std::string& name, const std::string& ip) {
                    _service_manager->offline(name, ip);
                });
            _user_dis = std::make_shared<Discovery>(_user_service_name, etcd_addr,
                [this](const std::string& name, const std::string& ip) {
                    _service_manager->online(name, ip);
                },
                [this](const std::string& name, const std::string& ip) {
                    _service_manager->offline(name, ip);
                });
            _reg = make_shared<Registry>(message_service_name, etcd_addr, etcd_timeout);
            _reg->registry(instance_name, service_ip + ":" + to_string(service_port));
            return true;
        }

        // 设置rabbitmq服务
        bool make_rabbitmq(const std::string& user, const std::string& password, const std::string& host,
            const std::string& exchange, const std::string& queue) {
            _rabbitmq = std::make_shared<RabbitMQ>(user, password, host);
            _rabbitmq->declareComponents(exchange, queue);
            _exchange_name = exchange;
            _queue_name = queue;
            return true;
        }

        // 设置rpc服务
        bool make_rpc(int32_t listen_port, uint8_t thread_num, int rpc_timeout) {
            _server = make_shared<brpc::Server>();

            auto service = new MsgStorageServiceImpl(_es, _mysql, _file_service_name, _user_service_name, _service_manager);
            int ret = _server->AddService(service, brpc::SERVER_OWNS_SERVICE);
            if (ret != 0) {
                LOG_ERROR("MsgStorageServer添加服务失败");
                return false;
            }

            brpc::ServerOptions options;
            options.idle_timeout_sec = rpc_timeout;
            options.num_threads = thread_num;

            ret = _server->Start(listen_port, &options);
            if (ret != 0) {
                LOG_ERROR("MsgStorageServer启动失败");
                return false;
            }
            _rabbitmq->consume(_queue_name, [service](const std::string& message) {
                service->onMessage(message);
                });
            return true;
        }

        MsgStorageServer::Ptr build() {
            if (!_es) {
                LOG_ERROR("es服务未设置");
                return nullptr;
            }
            if (!_mysql) {
                LOG_ERROR("mysql服务未设置");
                return nullptr;
            }
            if (!_rabbitmq) {
                LOG_ERROR("rabbitmq服务未设置");
                return nullptr;
            }
            if (!_file_dis || !_user_dis || !_service_manager || !_reg) {
                LOG_ERROR("etcd服务未设置");
                return nullptr;
            }
            if (!_server) {
                LOG_ERROR("rpc服务未设置");
                return nullptr;
            }
            return make_shared<MsgStorageServer>(_file_dis, _user_dis, _reg, _server);
        }
    private:
        Registry::Ptr _reg;
        std::shared_ptr<elasticlient::Client> _es;
        std::shared_ptr<odb::database> _mysql;
        RabbitMQ::Ptr _rabbitmq;
        ServiceManager::Ptr _service_manager;
        std::string _file_service_name;
        std::string _user_service_name;
        std::string _exchange_name;
        std::string _queue_name;
        Discovery::Ptr _file_dis, _user_dis;
        std::shared_ptr<brpc::Server> _server;
    };
}