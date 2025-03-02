#include <sw/redis++/redis.h>
#include <gflags/gflags.h>
#include <iostream>
#include <thread>

DEFINE_string(redis_host, "localhost", "redis host");
DEFINE_int32(redis_port, 6379, "redis port");
DEFINE_int32(redis_db, 0, "redis db");
DEFINE_bool(keep_alive, true, "是否进行长连接保活");

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    // gflags::ReadFromFlagsFile("config.conf", "", true);
    // 1. 实例化 Redis 对象
    sw::redis::ConnectionOptions opts;
    opts.host = FLAGS_redis_host;
    opts.port = FLAGS_redis_port;
    opts.db = FLAGS_redis_db;
    opts.keep_alive = FLAGS_keep_alive;

    sw::redis::Redis redis(opts);
    // 2. 添加、删除、获取字符串键值对
    redis.set("name1", "zhangsan");
    redis.set("name2", "lisi");
    redis.set("name3", "wangwu");

    redis.del("name1");

    auto name1 = redis.get("name1");
    if (name1) {
        std::cout << "name1: " << *name1 << std::endl;
    }
    auto name2 = redis.get("name2");
    if (name2) {
        std::cout << "name2: " << *name2 << std::endl;
    }
    auto name3 = redis.get("name3");
    if (name3) {
        std::cout << "name3: " << *name3 << std::endl;
    }
    // 3. 控制数据有效时间
    redis.set("name3", "haha", std::chrono::milliseconds(1000));
    auto name3_1 = redis.get("name3");
    if (name3_1) {
        std::cout << "name3_1: " << *name3_1 << std::endl;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    auto name3_2 = redis.get("name3");
    if (name3_2) {
        std::cout << "name3_2: " << *name3_2 << std::endl;
    }
    // 4. 列表操作：右插左获取
    redis.rpush("list1", { "a", "b", "c" });
    std::vector<std::string> list1;
    redis.lrange("list1", 0, -1, std::back_inserter(list1));
    for (const auto& item : list1) {
        std::cout << "list1: " << item << std::endl;
    }

    return 0;
}