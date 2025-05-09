#include "user_server.hpp"
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

DEFINE_int32(redis_db, 0, "Redis数据库号");
DEFINE_string(redis_host, "localhost", "Redis主机名");
DEFINE_int32(redis_port, 6379, "Redis端口号");
DEFINE_bool(redis_keep_alive, true, "Redis连接保持活跃");

DEFINE_string(email_from, "", "邮件发送者");
DEFINE_string(email_smtp, "", "邮件服务器地址");
DEFINE_string(email_username, "", "登录用户名");
DEFINE_string(email_password, "", "登录授权码");
DEFINE_string(email_content_type, "html", "邮件内容类型");

DEFINE_string(etcd_address, "", "etcd注册中心地址");
DEFINE_string(file_service_name, "", "文件管理服务名称");
DEFINE_string(user_service_name, "", "用户服务名称");
DEFINE_string(instance_name, "", "实例名称");
DEFINE_string(service_ip, "", "实例供外部访问的ip");
DEFINE_int32(service_port, 0, "服务端口");
DEFINE_int32(etcd_timeout, 3, "ttl");

DEFINE_int32(listen_port, 7070, "Rpc监听端口");
DEFINE_int32(rpc_timeout, -1, "Rpc超时时间");
DEFINE_int32(rpc_threads, 1, "Rpc线程数");

DEFINE_string(llm_ip, "", "llm服务ip");
DEFINE_int32(llm_port, 0, "llm服务端口");
DEFINE_string(classifier_service_name, "", "llm分类服务名称");


int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("user.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    blus::UserServerBuilder builder{ FLAGS_file_service_name };
    builder.make_es({ FLAGS_es_url });
    builder.make_mysql(FLAGS_mysql_user, FLAGS_mysql_pswd, FLAGS_mysql_db, FLAGS_mysql_host, FLAGS_mysql_socket, FLAGS_mysql_conn_pool_count, FLAGS_mysql_port, "utf8");
    builder.make_redis(FLAGS_redis_db, FLAGS_redis_host, FLAGS_redis_port, FLAGS_redis_keep_alive);
    builder.make_email(FLAGS_email_from, FLAGS_email_smtp, FLAGS_email_username, FLAGS_email_password, FLAGS_email_content_type);
    builder.make_etcd(FLAGS_etcd_address, FLAGS_user_service_name, FLAGS_instance_name, FLAGS_service_ip, FLAGS_service_port, FLAGS_etcd_timeout);
    builder.make_rpc(FLAGS_listen_port, FLAGS_rpc_threads, FLAGS_rpc_timeout, FLAGS_llm_ip, FLAGS_llm_port, FLAGS_classifier_service_name);
    auto server = builder.build();
    if (server) {
        server->start();
    }
    else {
        LOG_ERROR("FileServer创建失败");
    }

    return 0;
}