#pragma once
#include <json/json.h>
#include <json/reader.h>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include "logger.hpp"
#include "httplib.h"

namespace blus {
    class Asr {
    public:
        using Ptr = std::shared_ptr<Asr>;
        Asr(const std::string& ip, int32_t port, const std::string& service_name)
            : client_("http://" + ip + ":" + std::to_string(port)), service_name_(service_name) {
        }

        std::string recognize(const std::filesystem::path& audio_path) {
            // 发起一个POST请求
            // POST "http://ip:port/service_name"
            // Content-Type: application/json
            // {"path": "audio_path"}
            // 返回识别结果
            // 200 OK 400 文件不存在 500 服务器错误
            // {"text": "识别结果"}
            httplib::Headers headers = { {"Content-Type", "application/json"} };
            std::string body = "{\"path\": \"" + audio_path.string() + "\"}";

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
                    LOG_ERROR("语音文件路径{}", audio_path.string());
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

    class TextClassifier {
    public:
        using Ptr = std::shared_ptr<TextClassifier>;
        // 构造时指定 LLM 服务所在的 IP、端口以及服务名称（如 "api/generate"）
        TextClassifier(const std::string& ip, int32_t port, const std::string& service_name)
            : client_("http://" + ip + ":" + std::to_string(port)), service_name_(service_name) {
        }

        // classify 方法：发送请求获取分类结果
        std::string classify(const std::string& text) {
            // 构造 JSON 请求体：只包含文本字段
            Json::Value root;
            root["text"] = text;
            Json::StreamWriterBuilder writer;
            std::string body = Json::writeString(writer, root);

            httplib::Headers headers = { {"Content-Type", "application/json"} };
            std::string url = "/" + service_name_;
            auto res = client_.Post(url.c_str(), headers, body, "application/json");

            if (res) {
                if (res->status == 200) {
                    Json::CharReaderBuilder readerBuilder;
                    std::unique_ptr<Json::CharReader> jsonReader(readerBuilder.newCharReader());
                    Json::Value jsonResponse;
                    std::string errs;
                    const char* begin = res->body.c_str();
                    const char* end = begin + res->body.size();
                    if (!jsonReader->parse(begin, end, &jsonResponse, &errs)) {
                        throw std::runtime_error("解析JSON失败: " + errs);
                    }
                    // 直接返回服务端返回的 classification
                    return jsonResponse.get("classification", "").asString();
                }
                else {
                    throw std::runtime_error("HTTP请求失败, 状态码: " + std::to_string(res->status));
                }
            }
            else {
                throw std::runtime_error("LLM HTTP请求失败");
            }
        }

    private:
        httplib::Client client_;
        std::string service_name_;
    };

} // namespace blus