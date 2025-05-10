// 消息映射对象
#pragma once
#include <string>
#include <cstddef>
#include <odb/nullable.hxx>
#include <odb/core.hxx>
#include <memory>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace blus {
#pragma db object table("message")
    class Message {
    public:
        Message() = default;
        Message(const std::string& message_id, const std::string& user_id,
            const std::string& session_id, unsigned char message_type,
            boost::posix_time::ptime create_time)
            : _message_id(message_id), _user_id(user_id), _session_id(session_id),
            _message_type(message_type), _create_time(create_time) {
        }

        std::string message_id() const { return _message_id; }
        void message_id(const std::string& mid) { _message_id = mid; }
        std::string user_id() const { return _user_id; }
        void user_id(const std::string& uid) { _user_id = uid; }
        std::string session_id() const { return _session_id; }
        void session_id(const std::string& sid) { _session_id = sid; }
        unsigned char message_type() const { return _message_type; }
        void message_type(unsigned char type) { _message_type = type; }
        boost::posix_time::ptime create_time() const { return _create_time; }
        void create_time(boost::posix_time::ptime time) { _create_time = time; }
        std::string content() const { return _content ? *_content : ""; }
        void content(const std::string& content) { _content = content; }
        std::string file_id() const { return _file_id ? *_file_id : ""; }
        void file_id(const std::string& file_id) { _file_id = file_id; }
        std::string file_name() const { return _file_name ? *_file_name : ""; }
        void file_name(const std::string& file_name) { _file_name = file_name; }
        unsigned int file_size() const { return _file_size ? *_file_size : 0; }
        void file_size(unsigned int size) { _file_size = size; }
    private:
        friend class odb::access;
#pragma db id auto
        unsigned long _id;
#pragma db type("varchar(64)") index unique
        std::string _message_id;
#pragma db type("varchar(64)")
        std::string _user_id;
#pragma db type("varchar(64)") index
        std::string _session_id;
        unsigned char _message_type; // 0: text, 1: image, 2: file, 3: audio
#pragma db type("timestamp")
        boost::posix_time::ptime _create_time;

#pragma db type("text")
        odb::nullable<std::string> _content;
#pragma db type("varchar(64)")
        odb::nullable<std::string> _file_id;
#pragma db type("varchar(128)")
        odb::nullable<std::string> _file_name;
        odb::nullable<unsigned int> _file_size;
    };
} // namespace blus
