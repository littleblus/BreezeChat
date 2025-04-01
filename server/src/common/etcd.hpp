#pragma once
#include <etcd/Client.hpp>
#include <etcd/Response.hpp>
#include <etcd/KeepAlive.hpp>
#include <etcd/Watcher.hpp>
#include "logger.hpp"

namespace blus {
    class Registry {
    public:
        using Ptr = std::shared_ptr<Registry>;
        Registry(const std::string& service_name, const std::string& service_address, int ttl = 10)
            : _service_name(service_name)
            , _service_address(service_address)
            , _ttl(ttl)
            , _client(new etcd::Client(_service_address))
            , _lease(_client->leasekeepalive(_ttl).get())
            , _lease_id(_lease->Lease()) {
        }

        ~Registry() {
            _lease->Cancel();
        }

        bool registry(const std::string& key, const std::string& value) {
            auto response = _client->put(_service_name + "/" + key, value, _lease_id).get();
            if (!response.is_ok()) {
                LOG_ERROR("REG PUT FAILED: {}", response.error_message());
                return false;
            }
            return true;
        }
    private:
        std::string _service_name;
        std::string _service_address;
        int _ttl;
        std::shared_ptr<etcd::Client> _client;
        std::shared_ptr<etcd::KeepAlive> _lease;
        uint64_t _lease_id;
    };


    class Discovery {
    public:
        using Ptr = std::shared_ptr<Discovery>;
        using callback_t = std::function<void(const std::string&, const std::string&)>;
        Discovery(const std::string& service_name, const std::string& service_address,
            const callback_t& put_cb, const callback_t& delete_cb)
            : _service_name(service_name)
            , _service_address(service_address)
            , _put_cb(put_cb)
            , _delete_cb(delete_cb)
            , _client(std::make_shared<etcd::Client>(_service_address))
            , _watcher(std::make_shared<etcd::Watcher>(*_client.get(), _service_name, [this](etcd::Response response) {
            this->watch_callback(response);
                }, true)) {
            auto response = _client->ls(_service_name).get();
            if (!response.is_ok()) {
                LOG_CRITICAL("DIS LS FAILED: {}", response.error_message());
                abort();
            }
            for (auto& v : response.values()) {
                _put_cb(v.key(), v.as_string());
            }
        }

        ~Discovery() {}
    private:
        void watch_callback(etcd::Response response) {
            if (!response.is_ok()) {
                LOG_ERROR("WATCH FAILED: {}", response.error_message());
                return;
            }
            for (const auto& ev : response.events()) {
                if (ev.event_type() == etcd::Event::EventType::PUT) {
                    _put_cb(ev.kv().key(), ev.kv().as_string());
                    LOG_DEBUG("WATCH PUT: {}, {}", ev.kv().key(), ev.kv().as_string());
                }
                else if (ev.event_type() == etcd::Event::EventType::DELETE_) {
                    _delete_cb(ev.prev_kv().key(), ev.prev_kv().as_string());
                    LOG_DEBUG("WATCH DELETE: {}, {}", ev.prev_kv().key(), ev.prev_kv().as_string());
                }
            }
        }

        std::string _service_name;
        std::string _service_address;
        callback_t _put_cb;
        callback_t _delete_cb;
        std::shared_ptr<etcd::Client> _client;
        std::shared_ptr<etcd::Watcher> _watcher;
    };
}
