#pragma once
#include "es.hpp"
#include "logger.hpp"
#include "user.hxx"
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
} // namespace blus
