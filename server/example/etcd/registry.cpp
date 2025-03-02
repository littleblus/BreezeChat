#include <gflags/gflags.h>
#include <brpc/server.h>

#include "common/etcd.hpp"
#include "common/channel.hpp"
#include "etcd/main.pb.h"

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");
DEFINE_string(service_name, "", "服务名称");
DEFINE_string(etcd_address, "", "etcd地址");
DEFINE_int32(ttl, 3, "ttl");
DEFINE_int32(listen_port, 7070, "Rpc监听端口");

class EchoServiceImpl : public example::EchoService {
public:
    EchoServiceImpl() {}
    virtual ~EchoServiceImpl() {}

    void Echo(google::protobuf::RpcController* cntl_base,
        const example::EchoRequest* request,
        example::EchoResponse* response,
        google::protobuf::Closure* done) {
        brpc::ClosureGuard done_guard(done);
        std::cout << "服务端收到消息:" << request->message() << std::endl;

        std::string str = request->message() + "--这是echo响应";
        response->set_message(str);
    }
};

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("config.conf", "", true);
    init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    logging::LoggingSettings settings;
    settings.logging_dest = logging::LOG_TO_NONE;
    logging::InitLogging(settings);
    brpc::Server server;
    EchoServiceImpl echo_service;
    if (-1 == server.AddService(&echo_service, brpc::SERVER_DOESNT_OWN_SERVICE)) {
        std::cout << "添加服务失败" << std::endl;
        return -1;
    }
    brpc::ServerOptions options;
    options.idle_timeout_sec = -1; // 禁用空闲超时
    options.num_threads = 1; // 单线程
    if (-1 == server.Start(FLAGS_listen_port, &options)) {
        std::cout << "启动失败" << std::endl;
        return -1;
    }

    Registry reg(FLAGS_service_name, FLAGS_etcd_address, FLAGS_ttl);
    reg.registry("instance1", "127.0.0.1:7070");
    server.RunUntilAskedToQuit();

    return 0;
}
