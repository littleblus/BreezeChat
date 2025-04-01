#pragma once
#include <sw/redis++/redis.h>
#include <gflags/gflags.h>
#include <iostream>
#include <memory>

namespace blus {
    class RedisFactory {
    public:
        static std::shared_ptr<sw::redis::Redis> create(
            int db = 0, // 数据库号
            const std::string& host = "localhost",
            int port = 6379,
            bool keep_alive = true) {
            sw::redis::ConnectionOptions opts;
            opts.host = host;
            opts.port = port;
            opts.db = db;
            opts.keep_alive = keep_alive;
            return std::make_shared<sw::redis::Redis>(opts);
        }
    };

    class Session {
    public:
        using Ptr = std::shared_ptr<Session>;
        Session(const std::shared_ptr<sw::redis::Redis>& redis)
            : _redis(redis) {
        }
        bool append(const std::string& ssid, const std::string& uid) {
            return _redis->set(ssid, uid);
        }
        bool remove(const std::string& ssid) {
            return _redis->del(ssid) ? true : false;
        }
        sw::redis::OptionalString uid(const std::string& ssid) {
            return _redis->get(ssid);
        }
    private:
        std::shared_ptr<sw::redis::Redis> _redis;
    };

    class Status {
    public:
        using Ptr = std::shared_ptr<Status>;
        Status(const std::shared_ptr<sw::redis::Redis>& redis)
            : _redis(redis) {
        }
        bool append(const std::string& uid) {
            return _redis->set(uid, "1");
        }
        bool remove(const std::string& uid) {
            return _redis->del(uid) ? true : false;
        }
        bool exists(const std::string& uid) {
            auto val = _redis->get(uid);
            if (val) {
                return *val == "1";
            }
            return false;
        }
    private:
        std::shared_ptr<sw::redis::Redis> _redis;
    };

    class VerifyCode {
    public:
        using Ptr = std::shared_ptr<VerifyCode>;
        VerifyCode(const std::shared_ptr<sw::redis::Redis>& redis)
            : _redis(redis) {
        }
        bool append(const std::string& cid, const std::string& code, int expire_seconds = 60) {
            return _redis->set(cid, code, std::chrono::seconds(expire_seconds));
        }
        bool remove(const std::string& cid) {
            return _redis->del(cid) ? true : false;
        }
        sw::redis::OptionalString code(const std::string& cid) {
            return _redis->get(cid);
        }
    private:
        std::shared_ptr<sw::redis::Redis> _redis;
    };
} // namespace blus
