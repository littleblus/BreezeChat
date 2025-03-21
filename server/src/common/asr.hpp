#pragma once
#include <json/json.h>
#include <json/reader.h>
#include "logger.hpp"
#include "httplib.h"

namespace blus {
    class Asr {
    public:
        using Ptr = std::shared_ptr<Asr>;
        Asr(const std::string& ip, int32_t port, const std::string& service_name)
            : client_("http://" + ip + ":" + std::to_string(port)), service_name_(service_name) {
        }

        std::string recognize(const std::string& audio_path) {
            // 发起一个POST请求
            // POST "http://ip:port/service_name"
            // Content-Type: application/json
            // {"path": "audio_path"}
            // 返回识别结果
            // 200 OK 400 文件不存在 500 服务器错误
            // {"text": "识别结果"}
            httplib::Headers headers = { {"Content-Type", "application/json"} };
            std::string body = "{\"path\": \"" + audio_path + "\"}";

            auto res = client_.Post("/" + service_name_, headers, body, "application/json");

            if (res) {
                if (res->status == 200) {
                    // 解析json
                    Json::CharReaderBuilder readerBuilder;
                    std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());
                    const char* begin = res->body.c_str();
                    const char* end = begin + res->body.length();
                    Json::Value json;
                    std::string errs;
                    bool parsingSuccessful = reader->parse(begin, end, &json, &errs);
                    if (!parsingSuccessful) {
                        throw std::runtime_error("解析JSON失败: " + errs);
                    }
                    return json["text"].asString();
                }
                else if (res->status == 400) {
                    LOG_ERROR("语音文件路径{}", audio_path);
                    throw std::runtime_error("语音文件不存在");
                }
                else if (res->status == 500) {
                    throw std::runtime_error("ASR服务内部错误");
                }
                else {
                    throw std::runtime_error("未知错误");
                }
            }
            else {
                throw std::runtime_error("ASR HTTP请求失败");
            }
        }

    private:
        httplib::Client client_;
        std::string service_name_;
    };
}