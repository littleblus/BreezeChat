// 聊天会话成员表映射对象
#pragma once
#include <string>
#include <cstddef>
#include <odb/nullable.hxx>
#include <odb/core.hxx>
#include <memory>

namespace blus {
#pragma db object table("chat_session_member")
    class ChatSessionMember {
    public:
        ChatSessionMember() = default;

        ChatSessionMember(const std::string& session_id, const std::string& user_id) :
            _session_id(session_id), _user_id(user_id) {
        }

        std::string session_id() const {
            return _session_id;
        }
        void session_id(const std::string& session_id) {
            _session_id = session_id;
        }
        std::string user_id() const {
            return _user_id;
        }
        void user_id(const std::string& user_id) {
            _user_id = user_id;
        }
    private:
        friend class odb::access;
#pragma db id auto
        unsigned long _id;
#pragma db type("varchar(64)") index
        std::string _session_id;
#pragma db type("varchar(64)")
        std::string _user_id;
    };
} // namespace blus
