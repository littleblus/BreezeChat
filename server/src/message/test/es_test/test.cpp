#include "data_es.hpp"
#include "logger.hpp"
#include <gflags/gflags.h>
#include <gtest/gtest.h>

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");
DEFINE_string(es_url, "http://localhost:9200/", "ES服务器地址");

std::shared_ptr<elasticlient::Client> es_client;
std::shared_ptr<blus::ESMessage> es_message;

TEST(ESMessage, CreateIndex) {
    EXPECT_TRUE(es_message->createIndex());
}

TEST(ESMessage, Insert) {
    EXPECT_TRUE(es_message->append("test_uid1", "test_message_id1", "test_chat_session_id1", boost::posix_time::second_clock::local_time(), "吃饭了吗？"));
    EXPECT_TRUE(es_message->append("test_uid2", "test_message_id2", "test_chat_session_id1", boost::posix_time::second_clock::local_time(), "吃的盖浇饭！"));
    EXPECT_TRUE(es_message->append("test_uid3", "test_message_id3", "test_chat_session_id2", boost::posix_time::second_clock::local_time(), "吃饭了吗？"));
    EXPECT_TRUE(es_message->append("test_uid4", "test_message_id4", "test_chat_session_id2", boost::posix_time::second_clock::local_time(), "吃的盖浇饭！"));
}

TEST(ESMessage, Search) {
    // 等待索引创建与数据 append 完成(ES索引创建是异步的)
    std::this_thread::sleep_for(std::chrono::seconds(2));

    try {
        auto result1 = es_message->search("盖浇饭", "test_chat_session_id1");
        EXPECT_EQ(result1.size(), 2) << "关键词 '盖浇饭' 在 test_chat_session_id1 下预期返回2条记录";
        std::cout << "盖浇饭搜索结果:" << std::endl;
        for (const auto& item : result1) {
            try {
                std::string ct = boost::posix_time::to_simple_string(item.create_time());
                std::cout << "user_id: " << item.user_id()
                    << ", message_id: " << item.message_id()
                    << ", session_id: " << item.session_id()
                    << ", create_time: " << ct
                    << ", content: " << item.content() << std::endl;
            }
            catch (const std::exception& ex) {
                std::cerr << "转换 create_time 异常: " << ex.what() << std::endl;
            }
        }

        auto result2 = es_message->search("盖浇", "test_chat_session_id2");
        EXPECT_EQ(result2.size(), 1) << "关键词 '盖浇' 在 test_chat_session_id2 下预期返回1条记录";
        std::cout << "盖浇搜索结果:" << std::endl;
        for (const auto& item : result2) {
            try {
                std::string ct = boost::posix_time::to_simple_string(item.create_time());
                std::cout << "user_id: " << item.user_id()
                    << ", message_id: " << item.message_id()
                    << ", session_id: " << item.session_id()
                    << ", create_time: " << ct
                    << ", content: " << item.content() << std::endl;
            }
            catch (const std::exception& ex) {
                std::cerr << "转换 create_time 异常: " << ex.what() << std::endl;
            }
        }
    }
    catch (const std::exception& ex) {
        FAIL() << "Search 测试发生异常: " << ex.what();
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("message.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    es_client = blus::ESFactory::create({ FLAGS_es_url });
    es_message = std::make_shared<blus::ESMessage>(es_client);

    return RUN_ALL_TESTS();
}