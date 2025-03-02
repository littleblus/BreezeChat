#include <etcd/Client.hpp>
#include <etcd/Response.hpp>
#include <etcd/Watcher.hpp>

void watch_callback(etcd::Response response) {
    if (!response.is_ok()) {
        std::cerr << "Failed to watch key: " << response.error_message() << std::endl;
    }
    for (const auto& ev : response.events()) {
        if (ev.event_type() == etcd::Event::EventType::PUT) {
            std::cout << "PUT: " << ev.kv().key() << ", Now Value: " << ev.kv().as_string() << std::endl;
            std::cout << "Prev Value: " << ev.prev_kv().as_string() << std::endl;
        }
        else if (ev.event_type() == etcd::Event::EventType::DELETE_) {
            std::cout << "DELETE: " << ev.prev_kv().key() << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    std::string etcd_address = "http://127.0.0.1:2379";
    etcd::Client client(etcd_address);

    auto response = client.ls("/test").get();

    if (!response.is_ok()) {
        std::cerr << "Failed to get key: " << response.error_message() << std::endl;
        return 1;
    }

    for (auto& v : response.values()) {
        std::cout << "Key: " << v.key() << ", Value: " << v.as_string() << std::endl;
    }

    etcd::Watcher watcher(client, "/service", watch_callback, true);
    watcher.Wait();

    return 0;
}