#pragma once
#include <ev.h>
#include <amqpcpp.h>
#include <amqpcpp/libev.h>
#include <openssl/ssl.h>
#include <openssl/opensslv.h>
#include "common/logger.hpp"

namespace blus {
    class RabbitMQ {
    public:
        using MessageCallback = std::function<void(const std::string&)>;
        RabbitMQ(const std::string& user,
            const std::string& password,
            const std::string& host) {
            _loop = EV_DEFAULT;
            _handler = std::make_unique<AMQP::LibEvHandler>(_loop);
            std::string url = "amqp://" + user + ":" + password + "@" + host + "/";
            _connection = std::make_unique<AMQP::TcpConnection>(_handler.get(), AMQP::Address(url));
            _channel = std::make_unique<AMQP::TcpChannel>(_connection.get());

            _thread = std::thread([this]() {
                ev_run(_loop, 0);
                });
        }
        ~RabbitMQ() {
            struct ev_async watcher;
            ev_async_init(&watcher, watcher_callback);
            ev_async_start(_loop, &watcher);
            ev_async_send(_loop, &watcher);
            _thread.join();
        }

        void declareComponents(const std::string& exchange,
            const std::string& queue,
            std::string routingKey = "__same__",
            AMQP::ExchangeType exchange_type = AMQP::ExchangeType::direct) {
            if (routingKey == "__same__") {
                routingKey = queue;
            }
            _channel->declareExchange(exchange, exchange_type).onError([exchange](const char* message) {
                LOG_ERROR("声明交换机{}失败: {}", exchange, message);
                exit(1);
                });
            _channel->declareQueue(queue).onError([queue](const char* message) {
                LOG_ERROR("声明队列{}失败: {}", queue, message);
                exit(1);
                });
            _channel->bindQueue(exchange, queue, routingKey).onError([exchange, queue](const char* message) {
                LOG_ERROR("绑定队列{}-{}失败: {}", exchange, queue, message);
                exit(1);
                });
        }
        bool publish(const std::string& exchange,
            const std::string& routingKey,
            const std::string& message) {
            bool ret = _channel->publish(exchange, routingKey, message);
            if (!ret) {
                LOG_ERROR("{}发布消息失败", exchange);
            }
            return ret;
        }
        void consume(const std::string& queue,
            const MessageCallback& callback) {
            _channel->consume(queue, 0).onReceived([this, callback](const AMQP::Message& message,
                uint64_t deliveryTag,
                bool redelivered) {
                    callback(std::string(message.body(), message.bodySize()));
                    _channel->ack(deliveryTag);
                })
                .onError([](const char* message) {
                LOG_ERROR("消费消息失败: {}", message);
                exit(1);
                    });
        }
    private:
        static void watcher_callback(struct ev_loop* loop, struct ev_async* w, int32_t revents) {
            ev_break(loop, EVBREAK_ALL);
        }

        std::unique_ptr<AMQP::LibEvHandler> _handler;
        std::unique_ptr<AMQP::TcpConnection> _connection;
        std::unique_ptr<AMQP::TcpChannel> _channel;
        struct ev_loop* _loop;
        std::thread _thread;
    };
}
