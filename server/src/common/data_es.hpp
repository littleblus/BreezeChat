#pragma once
#include "es.hpp"
#include "logger.hpp"
#include "user.hxx"
#include "message.hxx"
#include <vector>

namespace blus {
    class ESFactory {
    public:
        static std::shared_ptr<elasticlient::Client> create(const std::vector<std::string>& host_list) {
            return std::make_shared<elasticlient::Client>(host_list);
        }
    };

    class ESUser {
    public:
        using Ptr = std::shared_ptr<ESUser>;
        ESUser(const std::shared_ptr<elasticlient::Client>& client)
            : _client(client) {
        }
        bool createIndex() {
            auto ret = ESIndex(_client, "user")
                .append("user_id", "keyword", "standard", true)
                .append("email", "keyword", "standard", true)
                .append("nickname")
                .append("description", "text", "standard", false)
                .append("avatar_id", "keyword", "standard", false)
                .create();
            if (!ret) {
                LOG_ERROR("用户索引创建失败");
            }
            return ret;
        }
        bool append(
            const std::string& uid,
            const std::string& email,
            const std::string& nickname,
            const std::string& description,
            const std::string& avatar_id) {
            auto ret = ESInsert(_client, "user")
                .append("user_id", uid)
                .append("email", email)
                .append("nickname", nickname)
                .append("description", description)
                .append("avatar_id", avatar_id)
                .insert(uid);
            if (!ret) {
                LOG_ERROR("用户索引插入失败");
            }
            return ret;
        }
        bool remove(const std::string& uid) {
            auto ret = ESRemove(_client, "user").remove(uid);
            if (!ret) {
                LOG_ERROR("用户索引删除失败");
            }
            return ret;
        }
        std::vector<User> search(const std::string& key, const std::vector<std::string>& exclude_uid_list = {}) {
            auto es = ESSearch(_client, "user")
                .append_should_match("email.keyword", key)
                .append_should_match("user_id.keyword", key)
                .append_should_match("nickname", key);
            if (!exclude_uid_list.empty()) {
                es.append_must_not_terms("user_id.keyword", exclude_uid_list);
            }
            auto users = es.search();
            std::vector<User> result;
            if (users.isArray() == false) {
                LOG_ERROR("用户索引搜索失败");
                return result;
            }
            for (const auto& item : users) {
                User user;
                user.user_id(item["_source"]["user_id"].asString());
                user.email(item["_source"]["email"].asString());
                user.nickname(item["_source"]["nickname"].asString());
                user.description(item["_source"]["description"].asString());
                user.avatar_id(item["_source"]["avatar_id"].asString());
                result.push_back(user);
            }
            return result;
        }
        // TODO: 通配符查询
    private:
        std::shared_ptr<elasticlient::Client> _client;
    };

    class ESMessage {
    public:
        using Ptr = std::shared_ptr<ESMessage>;
        ESMessage(const std::shared_ptr<elasticlient::Client>& client)
            : _client(client) {
        }
        bool createIndex() {
            auto ret = ESIndex(_client, "message")
                .append("user_id", "keyword", "standard", true)
                .append("message_id", "keyword", "standard", false)
                .append("chat_session_id", "keyword", "standard", false)
                .append("create_time", "date", "standard", false)
                .append("content")
                .create();
            if (!ret) {
                LOG_ERROR("消息索引创建失败");
            }
            return ret;
        }
        bool append(const std::string& user_id,
            const std::string& message_id,
            const std::string& chat_session_id,
            const boost::posix_time::ptime& create_time,
            const std::string& content) {
            auto ret = ESInsert(_client, "message")
                .append("user_id", user_id)
                .append("message_id", message_id)
                .append("chat_session_id", chat_session_id)
                // JSON没有日期数据类型, 所以在ES中, 日期可以是
                // 包含格式化日期的字符串, "2018-10-01", 或"2018/10/01 12:10:30".
                // 代表时间毫秒数的长整型数字.
                // 代表时间秒数的整数. 
                .append("create_time", boost::posix_time::to_iso_extended_string(create_time))
                .append("content", content)
                .insert(message_id);
            if (!ret) {
                LOG_ERROR("消息索引插入失败");
            }
            return ret;
        }
        bool remove(const std::string& message_id) {
            auto ret = ESRemove(_client, "message").remove(message_id);
            if (!ret) {
                LOG_ERROR("消息索引删除失败");
            }
            return ret;
        }
        std::vector<Message> search(const std::string& key, const std::string& chat_session_id) {
            auto messages = ESSearch(_client, "message")
                .append_must_term("chat_session_id.keyword", chat_session_id)
                .append_must_match("content", key)
                .search();
            std::vector<Message> result;
            if (messages.isArray() == false) {
                LOG_ERROR("消息索引搜索失败");
                return result;
            }
            for (const auto& item : messages) {
                Message message;
                message.user_id(item["_source"]["user_id"].asString());
                message.message_id(item["_source"]["message_id"].asString());
                message.session_id(item["_source"]["chat_session_id"].asString());
                message.create_time(
                    boost::posix_time::from_iso_extended_string(item["_source"]["create_time"].asString()));
                message.content(item["_source"]["content"].asString());
                result.push_back(message);
            }
            return result;
        }
    private:
        std::shared_ptr<elasticlient::Client> _client;
    };
} // namespace blus
