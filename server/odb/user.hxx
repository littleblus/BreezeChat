#pragma once
#include <string>
#include <cstddef>
#include <odb/nullable.hxx>
#include <odb/core.hxx>
#include <memory>

namespace blus {
#pragma db object table("users")
    class User {
    public:
        using Ptr = std::shared_ptr<User>;
        User() {}
        User(const std::string& user_id, const std::string& nickname, const std::string& password)
            : _user_id(user_id), _nickname(nickname), _password(password) {
        }
        User(const std::string& user_id, const std::string& email)
            : _user_id(user_id), _email(email), _nickname("BreezeChatUser_" + user_id) {
        }

        unsigned long id() const { return _id; }
        std::string user_id() const { return _user_id; }
        void user_id(const std::string& user_id) { _user_id = user_id; }

        std::string nickname() const {
                return _nickname ? *_nickname : std::string{};
        }
        void nickname(const std::string& nickname) { _nickname = nickname; }

        std::string description() const {
                return _description ? *_description : std::string{};
        }
        void description(const std::string& description) { _description = description; }

        std::string password() const {
                return _password ? *_password : std::string{};
        }
        void password(const std::string& password) { _password = password; }

        std::string email() const {
                return _email ? *_email : std::string{};
        }
        void email(const std::string& email) { _email = email; }

        std::string avatar_id() const {
                return _avatar_id ? *_avatar_id : std::string{};
        }
        void avatar_id(const std::string& avatar_id) { _avatar_id = avatar_id; }

    private:
        friend class odb::access;

#pragma db id auto
        unsigned long _id;
#pragma db type("varchar(64)") index unique
        std::string _user_id;
#pragma db type("varchar(32)") index unique
        odb::nullable<std::string> _nickname;
#pragma db type("text")
        odb::nullable<std::string> _description;
#pragma db type("char(64)")
        odb::nullable<std::string> _password;
#pragma db type("varchar(64)") index unique
        odb::nullable<std::string> _email;
#pragma db type("char(16)")
        odb::nullable<std::string> _avatar_id;
    };
} // namespace blus
