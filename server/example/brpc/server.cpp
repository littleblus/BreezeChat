#include <iostream>

#include <brpc/server.h>
#include <butil/logging.h>

#include "main.pb.h"

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
    if (-1 == server.Start(8080, &options)) {
        std::cout << "启动失败" << std::endl;
        return -1;
    }
    server.RunUntilAskedToQuit();

    return 0;
}
