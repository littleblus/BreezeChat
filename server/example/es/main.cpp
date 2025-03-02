#include <elasticlient/client.h>
#include <cpr/response.h>
#include <iostream>

int main() {
    // 构造es客户端
    elasticlient::Client client({ "http://localhost:9200" });
    // 发起搜索请求
    try {
        auto resp = client.search("/user", "_doc", R"({"query": {"match_all": {}}})");
        // 打印响应状态码和正文
        std::cout << resp.status_code << std::endl;
        std::cout << resp.text << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}