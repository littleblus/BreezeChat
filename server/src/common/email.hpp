#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <cstdio>
#include <memory>
#include <array>

#include "logger.hpp"

namespace blus {
    class EmailSender {
    public:
        using Ptr = std::shared_ptr<EmailSender>;
        /// @brief 邮件发送（http://caspian.dotconf.net/menu/Software/SendEmail/）
        /// @param from 发送邮箱地址
        /// @param smtp smtp服务器地址
        /// @param username 登录验证用户名（邮箱地址）
        /// @param password 登录验证授权码
        /// @param contentType 邮件内容类型（默认html）
        /// @param charset 字符集
        /// @param tls 是否使用tls加密（默认false）
        /// @note 需要安装sendEmail工具
        EmailSender(const std::string& from, const std::string& smtp,
            const std::string& username, const std::string& password,
            const std::string& contentType = "html",
            const std::string& charset = "utf-8", bool tls = false)
            : from(from), smtp(smtp), username(username), password(password),
            contentType(contentType), charset(charset), tls(tls) {
        }

        virtual bool send(const std::string& to, const std::string& subject, const std::string& message, std::string* output = nullptr) {
            std::stringstream command;
            command << "sendEmail -f " << from
                << " -t " << to
                << " -s " << smtp
                << " -u \"" << subject << "\""
                << " -o message-content-type=" << contentType
                << " -o message-charset=" << charset
                << " -o tls=" << (tls ? "yes" : "no")
                << " -xu " << username
                << " -xp " << password
                << " -m \"" << message << "\"";

            return executeCommand(command.str(), output);
        }

        virtual bool sendVerifyCode(const std::string& to, const std::string& code, std::string* output = nullptr) {
            return send(to, "BreezeChat验证码", R"(
                <!DOCTYPE html>
                <html lang="zh">
                <head>
                    <meta charset="UTF-8">
                    <meta name="viewport" content="width=device-width, initial-scale=1.0">
                    <title>验证码邮件</title>
                    <style>
                        body {
                            font-family: Arial, sans-serif;
                            background-color: #f4f4f4;
                            text-align: center;
                            padding: 50px;
                        }
                        .email-container {
                            background: #fff;
                            padding: 20px;
                            border-radius: 10px;
                            box-shadow: 0px 0px 10px 0px rgba(0, 0, 0, 0.1);
                            display: inline-block;
                            text-align: left;
                        }
                        .verification-code {
                            font-size: 24px;
                            font-weight: bold;
                            color: #d9534f;
                            margin: 20px 0;
                        }
                        .footer {
                            margin-top: 20px;
                            font-size: 12px;
                            color: #666;
                        }
                    </style>
                </head>
                <body>
                    <div class="email-container">
                        <h2>您的验证码</h2>
                        <p>您好，</p>
                        <p>感谢您的注册/登录请求。您的一次性验证码如下：</p>
                        <p class="verification-code">)" + code + R"(</p>
                        <p>请在10分钟内使用此验证码完成验证。如非本人操作，请忽略此邮件。</p>
                        <div class="footer">
                            <p>此邮件由系统自动发送，请勿回复。</p>
                            <p>如果您有任何问题，请联系我们的客服支持。</p>
                        </div>
                    </div>
                </body>
                </html>)"
                , output);
        }
    private:
        std::string from, smtp, username, password, contentType, charset;
        bool tls;

        bool executeCommand(const std::string& cmd, std::string* output = nullptr) {
            std::string result;
            std::array<char, 128> buffer;
            std::unique_ptr<::FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
            if (!pipe) {
                if (output) {
                    *output = "popen failed!";
                }
                LOG_ERROR("popen failed! command: {}", cmd);
                return false;
            }
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                result += buffer.data();
            }
            if (output) {
                *output = result;
            }
            // 如果错误，程序的退出代码就不是0
            int exitCode = pclose(pipe.release());
            if (exitCode != 0) {
                LOG_ERROR("email sent failed! command: {}, exit code: {}, output: {}", cmd, exitCode, result);
                return false;
            }
            return true;
        }
    };
} // namespace blus