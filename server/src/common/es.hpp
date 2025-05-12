#pragma once
#include <elasticlient/client.h>
#include <cpr/cpr.h>
#include <json/json.h>
#include <json/reader.h>
#include <json/writer.h>
#include <iostream>
#include <string>
#include <memory>
#include "logger.hpp"

namespace blus {
    bool Serialize(const Json::Value& value, std::string& str) {
        Json::StreamWriterBuilder builder;
        builder["commentStyle"] = "None"; // 不添加注释
        builder["indentation"] = ""; // 不添加缩进
        builder["emitUTF8"] = true; // 使用 UTF-8
        str = Json::writeString(builder, value);
        return true;
    }

    bool Deserialize(const std::string& str, Json::Value& value) {
        Json::CharReaderBuilder readerBuilder;
        std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());

        const char* begin = str.c_str();
        const char* end = begin + str.length();
        std::string errs;

        // 解析 JSON 字符串
        bool parsingSuccessful = reader->parse(begin, end, &value, &errs);
        if (!parsingSuccessful) {
            LOG_ERROR("解析JSON失败: {}", errs);
            return false;
        }

        return true;
    }

    class ESIndex {
    public:
        ESIndex(const std::shared_ptr<elasticlient::Client>& client,
            const std::string& name, const std::string& type = "_doc")
            : _name(name), _type(type), _client(client) {
            Json::Value settings;
            Json::Value analysis;
            Json::Value analyzer;
            Json::Value ik;
            ik["tokenizer"] = "ik_max_word";
            analyzer["ik"] = ik;
            analysis["analyzer"] = analyzer;
            settings["analysis"] = analysis;
            _index["settings"] = settings;

            // std::string str;
            // Serialize(_root, str);
            // std::cout << str << std::endl;
        }

        // 检查索引是否存在
        bool exists() {
            try {
                // 发送 HEAD 请求到索引路径
                auto resp = _client->performRequest(elasticlient::Client::HTTPMethod::HEAD, _name, "");

                // 根据状态码判断索引是否存在
                return resp.status_code == 200;
            }
            catch (...) {
                // 处理连接异常，例如所有节点都失败的情况
                LOG_ERROR("检查索引是否存在失败{}", _name);
                return false;
            }
        }

        ESIndex& append(const std::string& key,
            const std::string& type = "text",
            const std::string& analyzer = "ik_max_word",
            bool enabled = true) {
            Json::Value fields;
            fields["type"] = type;
            fields["analyzer"] = analyzer;
            if (enabled == false) fields["enabled"] = enabled;
            _properties[key] = fields;
            return *this;
        }
        bool create(const std::string& index_id = "default_index_id") {
            Json::Value mappings;
            mappings["dynamic"] = true;
            mappings["properties"] = _properties;
            _index["mappings"] = mappings;

            std::string body;
            if (!Serialize(_index, body)) {
                LOG_ERROR("ESIndex::create()序列化失败");
                return false;
            }

            try {
                auto resp = _client->index(_name, _type, index_id, body);
                if (resp.status_code < 200 || resp.status_code >= 300) {
                    LOG_ERROR("创建ES索引失败{}-{}", _name, resp.status_code);
                    LOG_ERROR("请求正文: {}", body);
                    return false;
                }
            }
            catch (const std::exception& e) {
                LOG_ERROR("创建ES索引失败{}-{}", _name, e.what());
                LOG_ERROR("请求正文: {}", body);
                return false;
            }
            return true;
        }
        bool remove() {
            try {
                // 发送 DELETE 请求到索引路径
                auto resp = _client->performRequest(elasticlient::Client::HTTPMethod::DELETE, _name, "");

                // 根据状态码判断索引是否删除
                return resp.status_code == 200;
            }
            catch (...) {
                // 处理连接异常，例如所有节点都失败的情况
                LOG_ERROR("检查索引是否存在失败{}", _name);
                return false;
            }
        }
    private:
        std::string _name;
        std::string _type;
        Json::Value _index;
        Json::Value _properties;
        std::shared_ptr<elasticlient::Client> _client;
    };

    class ESInsert {
    public:
        ESInsert(const std::shared_ptr<elasticlient::Client>& client,
            const std::string& name, const std::string& type = "_doc")
            : _name(name), _type(type), _client(client) {
        }

        ESInsert& append(const std::string& key, const std::string& value) {
            _item[key] = value;
            return *this;
        }
        bool insert(const std::string& id = "") {
            std::string body;
            if (!Serialize(_item, body)) {
                LOG_ERROR("ESInsert::insert()序列化失败");
                return false;
            }

            try {
                auto resp = _client->index(_name, _type, id, body);
                if (resp.status_code < 200 || resp.status_code >= 300) {
                    LOG_ERROR("新增ES数据失败{}, 请求正文: {}", resp.status_code, body);
                    return false;
                }
            }
            catch (const std::exception& e) {
                LOG_ERROR("新增ES数据失败{}, 请求正文: {}", e.what(), body);
                return false;
            }
            clear();
            return true;
        }
        void clear() {
            _item.clear();
        }
    private:
        Json::Value _item;
        std::string _name;
        std::string _type;
        std::shared_ptr<elasticlient::Client> _client;
    };

    class ESRemove {
    public:
        ESRemove(const std::shared_ptr<elasticlient::Client>& client,
            const std::string& name, const std::string& type = "_doc")
            : _name(name), _type(type), _client(client) {
        }

        bool remove(const std::string& id) {
            try {
                auto resp = _client->remove(_name, _type, id);
                if (resp.status_code < 200 || resp.status_code >= 300) {
                    LOG_ERROR("删除ES数据失败{}, id: {}", resp.status_code, id);
                    return false;
                }
            }
            catch (const std::exception& e) {
                LOG_ERROR("删除ES数据失败{}, id: {}", e.what(), id);
                return false;
            }
            return true;
        }
    private:
        std::string _name;
        std::string _type;
        std::shared_ptr<elasticlient::Client> _client;
    };

    class ESSearch {
    public:
        ESSearch(const std::shared_ptr<elasticlient::Client>& client,
            const std::string& name, const std::string& type = "_doc")
            : _name(name), _type(type), _client(client) {
        }

        ESSearch& append_must_not_terms(const std::string& key, const std::vector<std::string>& value) {
            Json::Value fields;
            for (const auto& v : value) {
                fields[key].append(v);
            }
            Json::Value terms;
            terms["terms"] = fields;
            _must_not.append(terms);
            return *this;
        }
        ESSearch& append_should_match(const std::string& key, const std::string& value) {
            Json::Value field;
            field[key] = value;
            Json::Value match;
            match["match"] = field;
            _should.append(match);
            return *this;
        }
        ESSearch& append_must_term(const std::string& key, const std::string& value) {
            Json::Value field;
            field[key] = value;
            Json::Value term;
            term["term"] = field;
            _must.append(term);
            return *this;
        }
        ESSearch& append_must_match(const std::string& key, const std::string& value) {
            Json::Value field;
            field[key] = value;
            Json::Value match;
            match["match"] = field;
            _must.append(match);
            return *this;
        }
        Json::Value search() {
            Json::Value condition;
            if (!_must_not.empty()) condition["must_not"] = _must_not;
            if (!_should.empty()) condition["should"] = _should;
            if (!_must.empty()) condition["must"] = _must;
            Json::Value root;
            root["query"]["bool"] = condition;
            std::string body;
            if (!Serialize(root, body)) {
                LOG_ERROR("ESSearch::search()序列化失败");
                return Json::Value();
            }

            cpr::Response resp;
            try {
                resp = _client->search(_name, _type, body);
                if (resp.status_code < 200 || resp.status_code >= 300) {
                    LOG_ERROR("搜索ES数据失败{}, 请求正文: {}", resp.status_code, body);
                    return Json::Value();
                }
            }
            catch (const std::exception& e) {
                LOG_ERROR("搜索ES数据失败{}, 请求正文: {}", e.what(), body);
                return Json::Value();
            }
            Json::Value result;
            if (!Deserialize(resp.text, result)) {
                LOG_ERROR("ESSearch::search()反序列化失败");
                return Json::Value();
            }
            clear();
            return result["hits"]["hits"];
        }
        void clear() {
            _must_not.clear();
            _should.clear();
        }
    private:
        Json::Value _must_not;
        Json::Value _should;
        Json::Value _must;
        std::string _name;
        std::string _type;
        std::shared_ptr<elasticlient::Client> _client;
    };
}
