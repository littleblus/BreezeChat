#include "message_server.hpp"
#include "logger.hpp"
#include <gflags/gflags.h>

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");

DEFINE_string(es_url, "http://localhost:9200/", "ES服务器地址");

DEFINE_string(mysql_user, "root", "mysql服务器登录用户名");
DEFINE_string(mysql_pswd, "", "mysql服务器登录密码");
DEFINE_string(mysql_db, "BreezeChat", "mysql数据库名称");
DEFINE_string(mysql_host, "localhost", "mysql服务器地址");
DEFINE_int32(mysql_port, 3306, "mysql服务器端口");
DEFINE_int32(mysql_conn_pool_count, 10, "mysql连接池大小");
DEFINE_string(mysql_socket, "", "mysql socket路径");

DEFINE_string(etcd_address, "", "etcd注册中心地址");
DEFINE_string(file_service_name, "", "文件管理服务名称");
DEFINE_string(user_service_name, "", "用户管理服务名称");
DEFINE_string(message_service_name, "", "消息存储服务名称");
DEFINE_string(instance_name, "", "实例名称");
DEFINE_string(service_ip, "", "实例供外部访问的ip");
DEFINE_int32(service_port, 0, "服务端口");
DEFINE_int32(etcd_timeout, 3, "ttl");

DEFINE_string(rabbitmq_host, "127.0.0.1:5672", "RabbitMQ 服务器地址, 带端口");
DEFINE_string(rabbitmq_user, "root", "RabbitMQ 用户名");
DEFINE_string(rabbitmq_password, "", "RabbitMQ 密码");
DEFINE_string(rabbitmq_msg_exchange, "", "RabbitMQ 交换机名称");
DEFINE_string(rabbitmq_msg_queue, "", "RabbitMQ 队列名称");

DEFINE_int32(listen_port, 7070, "Rpc监听端口");
DEFINE_int32(rpc_timeout, -1, "Rpc超时时间");
DEFINE_int32(rpc_threads, 1, "Rpc线程数");

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("message.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    blus::MsgStorageServerBuilder builder{ FLAGS_file_service_name, FLAGS_user_service_name };
    builder.make_es({ FLAGS_es_url });
    builder.make_mysql(FLAGS_mysql_user, FLAGS_mysql_pswd, FLAGS_mysql_db, FLAGS_mysql_host, FLAGS_mysql_socket, FLAGS_mysql_conn_pool_count, FLAGS_mysql_port, "utf8");
    builder.make_etcd(FLAGS_etcd_address, FLAGS_message_service_name, FLAGS_instance_name, FLAGS_service_ip, FLAGS_service_port, FLAGS_etcd_timeout);
    builder.make_rabbitmq(FLAGS_rabbitmq_user, FLAGS_rabbitmq_password, FLAGS_rabbitmq_host, FLAGS_rabbitmq_msg_exchange, FLAGS_rabbitmq_msg_queue);
    builder.make_rpc(FLAGS_listen_port, FLAGS_rpc_threads, FLAGS_rpc_timeout);
    auto server = builder.build();
    if (server) {
        server->start();
    }
    else {
        LOG_ERROR("MsgStorageServer创建失败");
    }

    return 0;
}