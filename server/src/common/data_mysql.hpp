#pragma once
#include <string>
#include <memory>
#include <cstdlib>
#include <iostream>
#include <odb/mysql/database.hxx>
#include <odb/database.hxx>

#include "user.hxx"
#include "user-odb.hxx"
#include "chat_session_member.hxx"
#include "chat_session_member-odb.hxx"
#include "logger.hpp"

namespace blus {
    class ODBFactory {
    public:
        static std::shared_ptr<odb::core::database> create(
            const std::string& user,
            const std::string& pswd,
            const std::string& db,
            const std::string& host,
            const std::string& sock_path = "",
            int conn_pool_count = 10,
            int port = 3306,
            const std::string& cset = "utf8") {
            std::unique_ptr<odb::mysql::connection_pool_factory> cpf(
                new odb::mysql::connection_pool_factory(conn_pool_count, 0));
            return std::make_shared<odb::mysql::database>(user, pswd,
                db, host, port, sock_path, cset, 0, std::move(cpf));
        }
    };

    class UserTable {
    public:
        using Ptr = std::shared_ptr<UserTable>;
        UserTable() {}
        UserTable(const std::shared_ptr<odb::database>& db) : _db(db) {}

        bool insert(const std::shared_ptr<User>& user) {
            try {
                odb::transaction trans(_db->begin());
                _db->persist(*user);
                trans.commit();
                return true;
            }
            catch (const std::exception& e) {
                LOG_ERROR("新增用户失败{}: {}", user->nickname(), e.what());
                return false;
            }
        }
        bool insert(const User& user) {
            auto user_ptr = std::make_shared<User>(user);
            return insert(user_ptr);
        }
        bool update(const std::shared_ptr<User>& user) {
            try {
                odb::transaction trans(_db->begin());
                _db->update(*user);
                trans.commit();
                return true;
            }
            catch (const std::exception& e) {
                LOG_ERROR("更新用户失败{}: {}", user->nickname(), e.what());
                return false;
            }
        }
        bool update(const User& user) {
            auto user_ptr = std::make_shared<User>(user);
            return update(user_ptr);
        }
        bool remove(const std::string& user_id) {
            try {
                odb::transaction trans(_db->begin());
                using query = odb::query<User>;
                // 使用 erase_query 删除满足条件的记录
                auto count = _db->erase_query<User>(query::user_id == user_id);
                trans.commit();
                return (count > 0);
            }
            catch (const std::exception& e) {
                LOG_ERROR("删除用户失败 {}: {}", user_id, e.what());
                return false;
            }
        }
        bool clear() {
            try {
                odb::transaction trans(_db->begin());
                // 直接执行 SQL TRUNCATE，相当于清空整个表
                _db->execute("TRUNCATE TABLE users");
                trans.commit();
                return true;
            }
            catch (const std::exception& e) {
                LOG_ERROR("清空用户表失败: {}", e.what());
                return false;
            }
        }
        std::shared_ptr<User> select_by_uid(const std::string& user_id) {
            std::shared_ptr<User> res;
            try {
                odb::transaction trans(_db->begin());
                using query = odb::query<User>;
                using result = odb::result<User>;
                res.reset(_db->query_one<User>(query::user_id == user_id));
                trans.commit();
                return res;
            }
            catch (const std::exception& e) {
                LOG_ERROR("查询用户失败 user_id: {}, {}", user_id, e.what());
            }
            return nullptr;
        }
        std::shared_ptr<User> select_by_nickname(const std::string& nickname) {
            std::shared_ptr<User> res;
            try {
                odb::transaction trans(_db->begin());
                using query = odb::query<User>;
                using result = odb::result<User>;
                res.reset(_db->query_one<User>(query::nickname == nickname));
                trans.commit();
                return res;
            }
            catch (const std::exception& e) {
                LOG_ERROR("查询用户失败 nickname: {}, {}", nickname, e.what());
            }
            return nullptr;
        }
        std::shared_ptr<User> select_by_email(const std::string& email) {
            std::shared_ptr<User> res;
            try {
                odb::transaction trans(_db->begin());
                using query = odb::query<User>;
                using result = odb::result<User>;
                res.reset(_db->query_one<User>(query::email == email));
                trans.commit();
                return res;
            }
            catch (const std::exception& e) {
                LOG_ERROR("查询用户失败 email: {}, {}", email, e.what());
            }
            return nullptr;
        }
        std::vector<std::shared_ptr<User>> select_multi_users(const std::vector<std::string>& user_ids) {
            try {
                odb::transaction trans(_db->begin());
                using query = odb::query<User>;
                using result = odb::result<User>;
                std::string condition;
                condition += "user_id in (";
                for (const auto& id : user_ids) {
                    condition += "'" + id + "',";
                }
                condition.pop_back();
                condition += ")";

                result r = _db->query<User>(condition);
                std::vector<std::shared_ptr<User>> users;
                for (const auto& user : r) {
                    users.push_back(std::make_shared<User>(user));
                }
                trans.commit();
                return users;
            }
            catch (const std::exception& e) {
                std::string ids;
                for (const auto& id : user_ids) {
                    ids += id + ",";
                }
                ids.pop_back();
                LOG_ERROR("查询多用户失败 user_ids: {}, {}", ids, e.what());
            }
            return std::vector<std::shared_ptr<User>>();
        }
    private:
        std::shared_ptr<odb::database> _db;
    };

    class ChatSessionMemberTable {
    public:
        using Ptr = std::shared_ptr<ChatSessionMemberTable>;
        ChatSessionMemberTable(const std::shared_ptr<odb::database>& db) : _db(db) {}

        bool append(const std::shared_ptr<ChatSessionMember>& member) {
            try {
                odb::transaction trans(_db->begin());
                _db->persist(*member);
                trans.commit();
                return true;
            }
            catch (const std::exception& e) {
                LOG_ERROR("新增单会话成员失败{}-{}: {}", member->session_id(), member->user_id(), e.what());
                return false;
            }
        }
        bool append(const ChatSessionMember& member) {
            auto member_ptr = std::make_shared<ChatSessionMember>(member);
            return append(member_ptr);
        }
        bool append(const std::vector<std::shared_ptr<ChatSessionMember>>& members) {
            if (members.empty()) {
                return false;
            }
            try {
                odb::transaction trans(_db->begin());
                for (const auto& member : members) {
                    _db->persist(*member);
                }
                trans.commit();
                return true;
            }
            catch (const std::exception& e) {
                LOG_ERROR("新增多会话成员（共{}个）失败{}: {}", members.size(), members[0]->session_id(), e.what());
                return false;
            }
        }
        bool append(const std::vector<ChatSessionMember>& members) {
            if (members.empty()) {
                return false;
            }
            std::vector<std::shared_ptr<ChatSessionMember>> ptrs;
            ptrs.reserve(members.size());
            for (const auto& m : members) {
                ptrs.push_back(std::make_shared<ChatSessionMember>(m));
            }
            return append(ptrs);
        }
        bool remove(const ChatSessionMember& member) {
            try {
                odb::transaction trans(_db->begin());
                using query = odb::query<ChatSessionMember>;
                auto count = _db->erase_query<ChatSessionMember>(query::session_id == member.session_id() && query::user_id == member.user_id());
                trans.commit();
                return (count > 0);
            }
            catch (const std::exception& e) {
                LOG_ERROR("删除单会话成员失败{}-{}: {}", member.session_id(), member.user_id(), e.what());
                return false;
            }
        }
        bool remove(const std::string& session_id) {
            try {
                odb::transaction trans(_db->begin());
                using query = odb::query<ChatSessionMember>;
                using result = odb::result<ChatSessionMember>;
                _db->erase_query<ChatSessionMember>(query::session_id == session_id);
                trans.commit();
                return true;
            }
            catch (const std::exception& e) {
                LOG_ERROR("删除会话所有成员失败{}: {}", session_id, e.what());
                return false;
            }
        }
        bool clear() {
            try {
                odb::transaction trans(_db->begin());
                // 直接执行 SQL TRUNCATE，相当于清空整个表
                _db->execute("TRUNCATE TABLE chat_session_member");
                trans.commit();
                return true;
            }
            catch (const std::exception& e) {
                LOG_ERROR("清空会话成员表失败: {}", e.what());
                return false;
            }
        }
        std::vector<std::string> get_members(const std::string& session_id) {
            std::vector<std::string> members;
            try {
                odb::transaction trans(_db->begin());
                using query = odb::query<ChatSessionMember>;
                using result = odb::result<ChatSessionMember>;

                result r = _db->query<ChatSessionMember>(query::session_id == session_id);
                std::vector<std::shared_ptr<User>> users;
                for (const auto& member : r) {
                    members.push_back(member.user_id());
                }
                trans.commit();
            }
            catch (const std::exception& e) {
                LOG_ERROR("查询会话成员失败 session_id: {}, {}", session_id, e.what());
            }
            return members;
        }
    private:
        std::shared_ptr<odb::database> _db;

    };
} // namespace blus
