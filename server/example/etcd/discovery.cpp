#include "common/etcd.hpp"
#include "common/channel.hpp"
#include "main.pb.h"
#include <gflags/gflags.h>

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");
DEFINE_string(service_name, "", "服务名称");
DEFINE_string(etcd_address, "", "etcd地址");

int main(int argc, char** argv) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("config.conf", "", true);
    init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    // 构造Rpc信道管理对象
    auto sm = std::make_shared<ServiceManager>();
    auto put_cb = [sm](const std::string& name, const std::string& ip) {
        sm->online(name, ip);
        };
    auto del_cb = [sm](const std::string& name, const std::string& ip) {
        sm->offline(name, ip);
        };
    sm->declare("echo");

    // 构造服务发现对象
    auto dis = std::make_shared<Discovery>(FLAGS_service_name, FLAGS_etcd_address, put_cb, del_cb);

    // 发起echo调用
    for (;;) {
        // 获取echo服务的地址
        auto addr = sm->get("echo");
        if (!addr) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        example::EchoService_Stub stub(addr.get());
        auto cntl = std::make_shared<brpc::Controller>();
        auto resp = std::make_shared<example::EchoResponse>();
        example::EchoRequest req;
        req.set_message("这是请求");
        stub.Echo(cntl.get(), &req, resp.get(), nullptr);
        if (cntl->Failed()) {
            std::cout << "调用失败:" << cntl->ErrorText() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        std::cout << "dis收到响应:" << resp->message() << std::endl;
        sm->getServiceChannel("echo")->request_completed(addr);

        sm->getServiceChannel("echo")->print();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
