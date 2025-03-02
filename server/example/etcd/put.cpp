#include <etcd/Client.hpp>
#include <etcd/Response.hpp>
#include <etcd/KeepAlive.hpp>

int main(int argc, char* argv[]) {
    std::string etcd_address = "http://127.0.0.1:2379";
    etcd::Client client(etcd_address);
    auto keep_alive = client.leasekeepalive(10).get();
    auto lease_id = keep_alive->Lease();

    auto response1 = client.put("/service/user", "127.0.0.1:8080", lease_id).get();
    if (!response1.is_ok()) {
        std::cerr << "Failed to put key: " << response1.error_message() << std::endl;
        return 1;
    }

    auto response2 = client.put("/service/friend", "127.0.0.1:9090", lease_id).get();
    if (!response2.is_ok()) {
        std::cerr << "Failed to put key: " << response2.error_message() << std::endl;
        return 1;
    }

    return 0;
}