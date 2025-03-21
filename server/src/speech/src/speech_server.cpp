#include "speech_server.hpp"
#include "logger.hpp"
#include <gflags/gflags.h>
#include <filesystem>

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");

DEFINE_string(asr_server_ip, "", "asr服务ip");
DEFINE_string(asr_server_name, "", "asr服务名称");
DEFINE_int32(asr_server_port, 0, "asr服务端口");

DEFINE_string(etcd_address, "", "etcd注册中心地址");
DEFINE_string(speech_service_name, "", "服务名称");
DEFINE_string(instance_name, "", "实例名称");
DEFINE_string(service_ip, "", "实例供外部访问的ip");
DEFINE_int32(service_port, 0, "服务端口");
DEFINE_int32(etcd_timeout, 3, "ttl");

DEFINE_int32(listen_port, 7070, "Rpc监听端口");
DEFINE_int32(rpc_timeout, -1, "Rpc超时时间");
DEFINE_int32(rpc_threads, 1, "Rpc线程数");



int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("speech.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    blus::SpeechServerBuilder builder;
    builder.make_asr(FLAGS_asr_server_ip, FLAGS_asr_server_port, FLAGS_asr_server_name);
    builder.make_etcd(FLAGS_etcd_address, FLAGS_speech_service_name, FLAGS_instance_name, FLAGS_service_ip, FLAGS_service_port, FLAGS_etcd_timeout);
    builder.make_rpc(FLAGS_listen_port, FLAGS_rpc_threads, FLAGS_rpc_timeout);
    auto server = builder.build();
    if (server) {
        server->start();
    }
    else {
        LOG_ERROR("SpeechServer构建失败");
        exit(1);
    }

    return 0;
}