#pragma once
#include <brpc/server.h>
#include <butil/logging.h>

#include "base.pb.h"
#include "file.pb.h"

#include "utils.hpp"
#include "etcd.hpp"

namespace blus {
    class FileServiceImpl : public FileService {
    public:
        FileServiceImpl() {}
        ~FileServiceImpl() {}

        void GetSingleFile(google::protobuf::RpcController* controller,
            const GetSingleFileReq* request,
            GetSingleFileRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            std::string fid = request->file_id();
            std::string body;
            if (readFile(fid, body)) {
                response->set_success(true);
                response->mutable_file_data()->set_file_id(fid);
                response->mutable_file_data()->set_file_content(body);
            }
            else {
                response->set_success(false);
                response->set_errmsg("读取文件数据失败");
            }
        }

        void GetMultiFile(google::protobuf::RpcController* controller,
            const GetMultiFileReq* request,
            GetMultiFileRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            for (int i = 0; i < request->file_id_list_size(); i++) {
                std::string fid = request->file_id_list(i);
                std::string body;
                if (readFile(fid, body)) {
                    FileDownloadData data;
                    data.set_file_id(fid);
                    data.set_file_content(body);
                    response->mutable_file_data()->insert({ fid, data });
                }
                else {
                    response->set_success(false);
                    response->set_errmsg("读取文件数据失败");
                    response->mutable_file_data()->clear();
                    return;
                }
            }
            response->set_success(true);
        }

        void PutSingleFile(google::protobuf::RpcController* controller,
            const PutSingleFileReq* request,
            PutSingleFileRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            std::string fid = uuid();
            if (writeFile(fid, request->file_data().file_content())) {
                response->set_success(true);
                response->mutable_file_info()->set_file_id(fid);
                response->mutable_file_info()->set_file_name(request->file_data().file_name());
                response->mutable_file_info()->set_file_size(request->file_data().file_size());
            }
            else {
                response->set_success(false);
                response->set_errmsg("写入文件数据失败");
            }
        }

        void PutMultiFile(google::protobuf::RpcController* controller,
            const PutMultiFileReq* request,
            PutMultiFileRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            response->set_request_id(request->request_id());
            std::string fid = uuid();
            for (int i = 0; i < request->file_data_size(); i++) {
                std::string fid = uuid();
                if (writeFile(fid, request->file_data(i).file_content())) {
                    auto file_info = response->add_file_info();
                    file_info->set_file_id(fid);
                    file_info->set_file_name(request->file_data(i).file_name());
                    file_info->set_file_size(request->file_data(i).file_size());
                }
                else {
                    response->set_success(false);
                    response->set_errmsg("写入文件数据失败");
                    response->mutable_file_info()->Clear();
                    return;
                }
            }
            response->set_success(true);
        }
    };

    class FileServer {
    public:
        using Ptr = std::shared_ptr<FileServer>;
        FileServer(const Registry::Ptr& reg, const std::shared_ptr<brpc::Server>& server)
            : _reg(reg), _server(server) {
        }
        ~FileServer() {}

        void start() {
            _server->RunUntilAskedToQuit();
        }
    private:
        Registry::Ptr _reg;
        std::shared_ptr<brpc::Server> _server;
    };

    class FileServerBuilder {
    public:
        // 设置etcd服务
        bool make_etcd(const std::string& etcd_addr,
            const std::string& service_name,
            const std::string& instance_name,
            const std::string& service_ip,
            int32_t service_port,
            int etcd_timeout) {
            _reg = make_shared<Registry>(service_name, etcd_addr, etcd_timeout);
            _reg->registry(instance_name, service_ip + ":" + to_string(service_port));
            return true;
        }

        // 设置rpc服务
        bool make_rpc(int32_t listen_port, uint8_t thread_num, int rpc_timeout) {
            _server = make_shared<brpc::Server>();

            auto service = new FileServiceImpl();
            int ret = _server->AddService(service, brpc::SERVER_OWNS_SERVICE);
            if (ret != 0) {
                LOG_ERROR("FileServer添加服务失败");
                return false;
            }

            brpc::ServerOptions options;
            options.idle_timeout_sec = rpc_timeout;
            options.num_threads = thread_num;

            ret = _server->Start(listen_port, &options);
            if (ret != 0) {
                LOG_ERROR("FileServer启动失败");
                return false;
            }
            return true;
        }

        FileServer::Ptr build() {
            if (!_reg) {
                LOG_ERROR("etcd服务未设置");
                return nullptr;
            }
            if (!_server) {
                LOG_ERROR("rpc服务未设置");
                return nullptr;
            }
            return make_shared<FileServer>(_reg, _server);
        }
    private:
        Registry::Ptr _reg;
        std::shared_ptr<brpc::Server> _server;
    };
}