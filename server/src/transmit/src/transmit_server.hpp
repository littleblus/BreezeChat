#pragma once
#include <brpc/server.h>
#include <butil/logging.h>
#include <filesystem>
#include <chrono>

#include "base.pb.h"
#include "user.pb.h"
#include "transmite.pb.h"

#include "etcd.hpp"
#include "channel.hpp"
#include "data_mysql.hpp"
#include "rabbitmq.hpp"
#include "logger.hpp"
#include "utils.hpp"

namespace blus {
    class MsgTransmitServiceImpl : public MsgTransmitService {
    public:
        MsgTransmitServiceImpl(const std::shared_ptr<odb::database>& mysql,
            const std::string& user_service_name,
            const ServiceManager::Ptr& sm,
            const Discovery::Ptr& discovery,
            const RabbitMQ::Ptr& rabbitmq,
            const std::string& exchange_name,
            const std::string& queue_name)
            : _user_service_name(user_service_name)
            , _service_manager(sm)
            , _csm_table(std::make_shared<ChatSessionMemberTable>(mysql))
            , _rabbitmq(rabbitmq)
            , _exchange_name(exchange_name)
            , _queue_name(queue_name) {
        }
        ~MsgTransmitServiceImpl() {}

        void GetTransmitTarget(google::protobuf::RpcController* controller,
            const NewMessageReq* request,
            GetTransmitTargetRsp* response,
            google::protobuf::Closure* done) override {
            // LOG_DEBUG("{}-{} 收到新消息请求", request->request_id(), request->user_id());
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            std::string uid = request->user_id();
            std::string chat_ssid = request->chat_session_id();
            const auto& content = request->message();
            auto channel = _service_manager->get(_user_service_name);
            if (!channel) {
                LOG_ERROR("{}-{} 获取user服务失败", request->request_id(), uid);
                response->set_errmsg("获取user服务失败");
                response->set_success(false);
                return;
            }
            blus::UserService_Stub stub(channel.get());
            brpc::Controller cntl;
            GetUserInfoReq req;
            GetUserInfoRsp rsp;
            req.set_request_id(request->request_id());
            req.set_user_id(uid);
            stub.GetUserInfo(&cntl, &req, &rsp, nullptr);
            if (cntl.Failed() || rsp.success() == false) {
                LOG_ERROR("{}-{} user服务调用失败: {}", request->request_id(), uid, cntl.ErrorText());
                response->set_errmsg("user服务调用失败");
                response->set_success(false);
                return;
            }
            // LOG_DEBUG("{}-{} user服务调用成功", request->request_id(), uid);
            MessageInfo message;
            message.set_message_id(uuid());
            message.set_chat_session_id(chat_ssid);
            message.set_timestamp(time(nullptr));
            message.mutable_sender()->CopyFrom(rsp.user_info());
            message.mutable_message()->CopyFrom(content);
            // 获取转发客户端用户列表
            auto targets = _csm_table->get_members(chat_ssid);
            // 消息持久化
            if (!_rabbitmq->publish(_exchange_name, _queue_name, message.SerializeAsString())) {
                LOG_ERROR("{}-{} 消息持久化失败", request->request_id(), uid);
                response->set_errmsg("消息持久化失败");
                response->set_success(false);
                return;
            }
            response->set_success(true);
            response->mutable_message()->CopyFrom(message);
            for (const auto& id : targets) {
                response->add_target_id_list(id);
            }
        }
    private:
        std::string _user_service_name;
        ServiceManager::Ptr _service_manager;
        ChatSessionMemberTable::Ptr _csm_table;
        RabbitMQ::Ptr _rabbitmq;
        std::string _exchange_name;
        std::string _queue_name;
    };

    class TransmitServer {
    public:
        using Ptr = std::shared_ptr<TransmitServer>;
        TransmitServer(const Discovery::Ptr& dis,
            const Registry::Ptr& reg,
            const std::shared_ptr<brpc::Server>& server)
            : _discovery(dis)
            , _reg(reg)
            , _server(server) {
        }
        ~TransmitServer() {}

        void start() {
            _server->RunUntilAskedToQuit();
        }

    private:
        Discovery::Ptr _discovery;
        Registry::Ptr _reg;
        std::shared_ptr<brpc::Server> _server;
    };

    class TransmitServerBuilder {
    public:
        TransmitServerBuilder(const std::string& user_service_name)
            : _user_service_name(user_service_name) {
        }
        ~TransmitServerBuilder() {}

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
            const std::string& transmit_service_name,
            const std::string& instance_name,
            const std::string& service_ip,
            int32_t service_port,
            int etcd_timeout) {
            _service_manager = std::make_shared<ServiceManager>();
            _service_manager->declare(_user_service_name);
            _discovery = std::make_shared<Discovery>(_user_service_name, etcd_addr,
                [this](const std::string& name, const std::string& ip) {
                    _service_manager->online(name, ip);
                },
                [this](const std::string& name, const std::string& ip) {
                    _service_manager->offline(name, ip);
                });
            _reg = make_shared<Registry>(transmit_service_name, etcd_addr, etcd_timeout);
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

            auto service = new MsgTransmitServiceImpl(_mysql, _user_service_name, _service_manager, _discovery, _rabbitmq,
                _exchange_name, _queue_name);
            int ret = _server->AddService(service, brpc::SERVER_OWNS_SERVICE);
            if (ret != 0) {
                LOG_ERROR("TransmitServer添加服务失败");
                return false;
            }

            brpc::ServerOptions options;
            options.idle_timeout_sec = rpc_timeout;
            options.num_threads = thread_num;

            ret = _server->Start(listen_port, &options);
            if (ret != 0) {
                LOG_ERROR("TransmitServer启动失败");
                return false;
            }
            return true;
        }


        TransmitServer::Ptr build() {
            if (!_mysql) {
                LOG_ERROR("mysql服务未设置");
                return nullptr;
            }
            if (!_discovery || !_service_manager || !_reg) {
                LOG_ERROR("etcd服务未设置");
                return nullptr;
            }
            if (!_rabbitmq) {
                LOG_ERROR("rabbitmq服务未设置");
                return nullptr;
            }
            if (!_server) {
                LOG_ERROR("rpc服务未设置");
                return nullptr;
            }
            return make_shared<TransmitServer>(_discovery, _reg, _server);
        }
    private:
        std::string _user_service_name;
        std::shared_ptr<odb::database> _mysql;
        Discovery::Ptr _discovery;
        Registry::Ptr _reg;
        RabbitMQ::Ptr _rabbitmq;
        std::string _exchange_name;
        std::string _queue_name;
        ServiceManager::Ptr _service_manager;
        std::shared_ptr<brpc::Server> _server;
    };
}