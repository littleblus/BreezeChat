#include <iostream>
#include <thread>

#include <brpc/channel.h>

#include "etcd/main.pb.h"

void callback(std::shared_ptr<brpc::Controller> cntl,
    std::shared_ptr<example::EchoResponse> resp) {
    std::cout << "客户端收到响应:" << resp->message() << std::endl;
}

int main(int argc, char* argv[]) {
    brpc::ChannelOptions options;
    options.timeout_ms = -1;
    options.connect_timeout_ms = -1;
    options.max_retry = 3;

    brpc::Channel channel;
    if (-1 == channel.Init("127.0.0.1:8080", &options)) {
        std::cout << "初始化失败" << std::endl;
        return -1;
    }

    example::EchoService_Stub stub(&channel);
    example::EchoRequest req;
    req.set_message("这是请求");

    auto cntl = std::make_shared<brpc::Controller>();
    auto resp = std::make_shared<example::EchoResponse>();
    auto closure = brpc::NewCallback(callback, cntl, resp);
    stub.Echo(cntl.get(), &req, resp.get(), closure);
    std::cout << "异步调用完成" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // stub.Echo(cntl, &req, resp, nullptr);
    // if (cntl->Failed()) {
    //     std::cout << "调用失败:" << cntl->ErrorText() << std::endl;
    //     return -1;
    // }
    // std::cout << "客户端收到响应:" << resp->message() << std::endl;
    // delete cntl;
    // delete resp;

    return 0;
}