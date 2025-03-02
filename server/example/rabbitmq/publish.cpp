#include "common/rabbitmq.hpp"
#include <gflags/gflags.h>

DEFINE_string(rabbitmq_host, "127.0.0.1:5672", "RabbitMQ 服务器地址, 带端口");
DEFINE_string(rabbitmq_user, "root", "RabbitMQ 用户名");
DEFINE_string(rabbitmq_password, "", "RabbitMQ 密码");

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");


int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    RabbitMQ client(FLAGS_rabbitmq_user, FLAGS_rabbitmq_password, FLAGS_rabbitmq_host);
    client.declareComponents("exchange", "queue");
    for (int i = 0; i < 10; i++) {
        client.publish("exchange", "queue", "hello world" + std::to_string(i));
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));

    return 0;
}