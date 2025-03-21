#include "etcd.hpp"
#include "channel.hpp"
#include "speech.pb.h"
#include <gflags/gflags.h>
#include <iostream>
#include <fstream>
#include <filesystem>

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");
DEFINE_string(speech_service_name, "", "语音微服务名称");
DEFINE_string(etcd_address, "", "etcd地址");

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("speech.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    // 构造Rpc信道管理对象
    auto sm = std::make_shared<blus::ServiceManager>();
    auto put_cb = [sm](const std::string& name, const std::string& ip) {
        sm->online(name, ip);
        };
    auto del_cb = [sm](const std::string& name, const std::string& ip) {
        sm->offline(name, ip);
        };
    sm->declare(FLAGS_speech_service_name);

    // 构造服务发现对象
    auto dis = std::make_shared<blus::Discovery>(FLAGS_speech_service_name, FLAGS_etcd_address, put_cb, del_cb);

    // 获取speech服务的地址
    auto addr = sm->get(FLAGS_speech_service_name);
    if (!addr) {
        std::cout << "获取服务地址失败" << std::endl;
        return 1;
    }

    blus::SpeechService_Stub stub(addr.get());
    auto cntl = std::make_shared<brpc::Controller>();
    auto resp = std::make_shared<blus::SpeechRecognitionRsp>();
    blus::SpeechRecognitionReq req;
    req.set_request_id("1");
    std::ifstream ifs("/home/lwj/project/BreezeChat/server/src/speech/test/zh.wav", std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    req.set_speech_content(content);
    ifs.close();

    stub.SpeechRecognition(cntl.get(), &req, resp.get(), nullptr);
    if (cntl->Failed()) {
        std::cout << "调用失败:" << cntl->ErrorText() << std::endl;
        return 1;
    }
    std::cout << resp->request_id() << "收到响应:" << resp->recognition_result() << std::endl;
    sm->getServiceChannel(FLAGS_speech_service_name)->request_completed(addr);

    return 0;
}
