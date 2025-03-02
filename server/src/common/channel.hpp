#pragma once
#include <iostream>
#include <string>
#include <memory>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <brpc/channel.h>

#include "logger.hpp"

using ChannelPtr = std::shared_ptr<brpc::Channel>;

class ServiceChannel {
public:
    // 定义信道状态结构体
    struct ChannelStatus {
        ChannelPtr channel;
        int busy_level; // 忙碌程度，值越小越不忙碌

        ChannelStatus(ChannelPtr ch, int level) : channel(ch), busy_level(level) {}

        // 重载比较运算符，用于小根堆
        bool operator>(const ChannelStatus& other) const {
            return busy_level > other.busy_level;
        }
    };

    ServiceChannel(const std::string& service_name)
        : _service_name(service_name)
        , _channel_heap(std::greater<ChannelStatus>(), std::vector<ChannelStatus>()) {
    }

    // 添加信道
    void append(const std::string& ip_port) {
        brpc::ChannelOptions options;
        options.timeout_ms = -1;
        options.connect_timeout_ms = -1;
        options.max_retry = 3;

        ChannelPtr channel = std::make_shared<brpc::Channel>();
        if (channel->Init(ip_port.c_str(), &options) == 0) {
            std::lock_guard<std::mutex> lock(_mutex);
            ChannelStatus status(channel, 0);
            _channel_heap.push(status);
            _ip_port_map.emplace(ip_port, status);
        }
        else {
            LOG_ERROR("Failed to initialize channel for {}-{}", _service_name, ip_port);
        }
    }

    // 下线指定的信道
    void remove(const std::string& ip_port) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _ip_port_map.find(ip_port);
        if (it != _ip_port_map.end()) {
            std::priority_queue<ChannelStatus, std::vector<ChannelStatus>, std::greater<ChannelStatus>> temp_heap{ std::greater<ChannelStatus>(), std::vector<ChannelStatus>() };
            while (!_channel_heap.empty()) {
                auto status = _channel_heap.top();
                _channel_heap.pop();
                if (status.channel != it->second.channel) {
                    temp_heap.push(status);
                }
            }
            _channel_heap = std::move(temp_heap);
            _ip_port_map.erase(it); // 从映射中移除
        }
        else {
            LOG_WARN("Remove channel {}-{} not found", _service_name, ip_port);
        }
    }

    // 下线最不忙碌的信道
    void remove_least_busy() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_channel_heap.empty()) {
            return;
        }
        auto least_busy = _channel_heap.top();
        _channel_heap.pop();
        for (auto it = _ip_port_map.begin(); it != _ip_port_map.end(); ++it) {
            if (it->second.channel == least_busy.channel) {
                _ip_port_map.erase(it);
                break;
            }
        }
    }

    // 获取最不忙碌的信道
    ChannelPtr get() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_channel_heap.empty()) {
            LOG_WARN("没有可用的节点，服务名：{}", _service_name);
            return nullptr;
        }
        auto least_busy = _channel_heap.top();
        _channel_heap.pop();
        // 使用该信道前，将其忙碌程度加1
        least_busy.busy_level++;
        _channel_heap.push(least_busy);

        // // 更新映射中的忙碌程度
        // for (auto& pair : _ip_port_map) {
        //     if (pair.second.channel == least_busy.channel) {
        //         pair.second.busy_level = least_busy.busy_level;
        //         break;
        //     }
        // }

        return least_busy.channel;
    }

    // 请求完成后，更新信道的忙碌程度（客户端进行此调用）
    void request_completed(ChannelPtr channel) {
        std::lock_guard<std::mutex> lock(_mutex);
        std::priority_queue<ChannelStatus, std::vector<ChannelStatus>, std::greater<ChannelStatus>> temp_heap{ std::greater<ChannelStatus>(), std::vector<ChannelStatus>() };
        while (!_channel_heap.empty()) {
            auto status = _channel_heap.top();
            _channel_heap.pop();
            if (status.channel == channel && status.busy_level > 0) {
                status.busy_level--; // 忙碌程度减1
            }
            temp_heap.push(status);

            // // 更新映射中的忙碌程度
            // for (auto& pair : _ip_port_map) {
            //     if (pair.second.channel == status.channel) {
            //         pair.second.busy_level = status.busy_level;
            //         break;
            //     }
            // }
        }
        _channel_heap = std::move(temp_heap);
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _channel_heap.size();
    }

    // DEBUG
    void print() {
        std::lock_guard<std::mutex> lock(_mutex);
        std::cout << "ServiceChannel: " << _service_name << std::endl;
        std::cout << "ChannelHeap: ";
        auto temp_heap = _channel_heap;
        while (!temp_heap.empty()) {
            auto status = temp_heap.top();
            temp_heap.pop();
            std::cout << status.busy_level << " ";
        }
        std::cout << std::endl;
    }
private:
    std::mutex _mutex;
    std::string _service_name;
    std::priority_queue<ChannelStatus, std::vector<ChannelStatus>, std::greater<ChannelStatus>> _channel_heap;
    std::unordered_map<std::string, ChannelStatus> _ip_port_map; // 存储 ip_port 到 ChannelStatus 的映射
};

class ServiceManager {
public:
    using ptr = std::shared_ptr<ServiceChannel>;

    // 获取指定服务的节点
    ChannelPtr get(const std::string& service_name) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _services.find(service_name);
        if (it != _services.end()) {
            return it->second->get();
        }
        else {
            LOG_ERROR("没有能提供{}服务的节点", service_name);
            return nullptr;
        }
    }

    // 声明关注的服务
    void declare(const std::string& service_name) {
        std::lock_guard<std::mutex> lock(_mutex);
        _focus.insert(service_name);
    }

    // 取消关注的服务
    void undeclare(const std::string& service_name) {
        std::lock_guard<std::mutex> lock(_mutex);
        _focus.erase(service_name);
    }

    // 服务上线的回调函数
    void online(const std::string& service_instance, const std::string& ip_port) {
        std::unique_lock<std::mutex> lock(_mutex);
        auto service_name = getServiceName(service_instance);
        if (_focus.find(service_name) == _focus.end()) {
            LOG_DEBUG("添加服务节点时，服务{}未被关注", service_name);
            return;
        }
        auto it = _services.find(service_name);
        if (it == _services.end()) {
            auto channel = std::make_shared<ServiceChannel>(service_name);
            channel->append(ip_port);
            _services[service_name] = channel;
            lock.unlock();
        }
        else {
            it->second->append(ip_port);
        }
    }

    // 服务下线的回调函数
    void offline(const std::string& service_instance, const std::string& ip_port) {
        std::unique_lock<std::mutex> lock(_mutex);
        auto service_name = getServiceName(service_instance);
        if (_focus.find(service_name) == _focus.end()) {
            LOG_DEBUG("删除服务节点时，服务{}未被关注", service_name);
            return;
        }
        auto it = _services.find(service_name);
        if (it != _services.end()) {
            lock.unlock();
            it->second->remove(ip_port);
        }
        else {
            LOG_WARN("删除服务节点时，服务{}不存在", service_name);
        }
    }

    // 获取指定服务的信道管理对象
    ptr getServiceChannel(const std::string& service_name) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _services.find(service_name);
        if (it != _services.end()) {
            return it->second;
        }
        else {
            return nullptr;
        }
    }
private:
    std::string getServiceName(const std::string& service_instance) {
        auto pos = service_instance.find_last_of('/');
        if (pos != std::string::npos) {
            return service_instance.substr(0, pos);
        }
        return service_instance;
    }

    std::mutex _mutex;
    std::unordered_set<std::string> _focus; // 关注的服务
    std::unordered_map<std::string, ptr> _services;
};