#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "etcd.hpp"
#include "channel.hpp"
#include "base.pb.h"
#include "file.pb.h"
#include "utils.hpp"

DEFINE_string(log_file, "", "日志文件路径, 默认输出到控制台");
DEFINE_int32(log_level, 0, "日志等级, 0: trace, 1: debug, 2: info, 3: warn, 4: error, 5: critical");
DEFINE_string(file_service_name, "", "文件微服务名称");
DEFINE_string(etcd_address, "", "etcd地址");

blus::ChannelPtr addr;
std::string single_file_id;

TEST(put_test, single_file) {
    std::string body;
    ASSERT_TRUE(blus::readFile("/home/lwj/project/BreezeChat/server/src/file/test/一拳超人.png", body));
    blus::FileService_Stub stub(addr.get());
    blus::PutSingleFileReq request;
    request.set_request_id("111");
    request.mutable_file_data()->set_file_content(body);
    request.mutable_file_data()->set_file_name("一拳超人.png");
    request.mutable_file_data()->set_file_size(body.size());
    blus::PutSingleFileRsp response;
    brpc::Controller cntl;
    stub.PutSingleFile(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    ASSERT_TRUE(response.success());
    ASSERT_EQ(response.file_info().file_name(), "一拳超人.png");
    ASSERT_EQ(response.file_info().file_size(), body.size());
    ASSERT_EQ(response.request_id(), "111");
    LOG_DEBUG("文件ID: {}", response.file_info().file_id());
    single_file_id = response.file_info().file_id();
    ASSERT_FALSE(single_file_id.empty());
}

TEST(get_test, single_file) {
    blus::FileService_Stub stub(addr.get());
    blus::GetSingleFileReq request;
    request.set_request_id("222");
    request.set_file_id(single_file_id);
    blus::GetSingleFileRsp response;
    brpc::Controller cntl;
    stub.GetSingleFile(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    ASSERT_TRUE(response.success());
    ASSERT_EQ(response.request_id(), "222");
    ASSERT_EQ(response.file_data().file_id(), single_file_id);
    std::string body;
    ASSERT_TRUE(blus::readFile("/home/lwj/project/BreezeChat/server/src/file/test/一拳超人.png", body));
    ASSERT_EQ(response.file_data().file_content(), body);
}

std::string multi_file_id[2];

TEST(put_test, multi_file) {
    std::string body1;
    ASSERT_TRUE(blus::readFile("/home/lwj/project/BreezeChat/server/src/file/test/吴亦凡 - JULY.flac", body1));
    std::string body2;
    ASSERT_TRUE(blus::readFile("/home/lwj/project/BreezeChat/server/src/file/test/吴亦凡 - November Rain.flac", body2));

    blus::FileService_Stub stub(addr.get());
    blus::PutMultiFileReq request;
    request.set_request_id("333");

    auto file_data = request.add_file_data();
    file_data->set_file_content(body1);
    file_data->set_file_name("吴亦凡 - JULY.flac");
    file_data->set_file_size(body1.size());

    file_data = request.add_file_data();
    file_data->set_file_content(body2);
    file_data->set_file_name("吴亦凡 - November Rain.flac");
    file_data->set_file_size(body2.size());
    blus::PutMultiFileRsp response;
    brpc::Controller cntl;
    stub.PutMultiFile(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    ASSERT_TRUE(response.success());
    ASSERT_EQ(response.file_info_size(), 2);
    ASSERT_EQ(response.request_id(), "333");
    ASSERT_EQ(response.file_info(0).file_name(), "吴亦凡 - JULY.flac");
    ASSERT_EQ(response.file_info(0).file_size(), body1.size());
    ASSERT_EQ(response.file_info(1).file_name(), "吴亦凡 - November Rain.flac");
    ASSERT_EQ(response.file_info(1).file_size(), body2.size());
    LOG_DEBUG("文件ID1: {}", response.file_info(0).file_id());
    LOG_DEBUG("文件ID2: {}", response.file_info(1).file_id());
    multi_file_id[0] = response.file_info(0).file_id();
    multi_file_id[1] = response.file_info(1).file_id();
    ASSERT_FALSE(multi_file_id[0].empty());
    ASSERT_FALSE(multi_file_id[1].empty());
}

TEST(get_test, multi_file) {
    blus::FileService_Stub stub(addr.get());
    blus::GetMultiFileReq request;
    request.set_request_id("444");
    request.add_file_id_list(multi_file_id[0]);
    request.add_file_id_list(multi_file_id[1]);
    blus::GetMultiFileRsp response;
    brpc::Controller cntl;
    stub.GetMultiFile(&cntl, &request, &response, nullptr);
    ASSERT_FALSE(cntl.Failed());
    ASSERT_TRUE(response.success());
    ASSERT_EQ(response.request_id(), "444");
    ASSERT_EQ(response.file_data_size(), 2);
    std::string body1;
    ASSERT_TRUE(blus::readFile("/home/lwj/project/BreezeChat/server/src/file/test/吴亦凡 - JULY.flac", body1));
    std::string body2;
    ASSERT_TRUE(blus::readFile("/home/lwj/project/BreezeChat/server/src/file/test/吴亦凡 - November Rain.flac", body2));
    auto it = response.file_data().find(multi_file_id[0]);
    ASSERT_NE(it, response.file_data().end());
    ASSERT_EQ(it->second.file_content(), body1);
    it = response.file_data().find(multi_file_id[1]);
    ASSERT_NE(it, response.file_data().end());
    ASSERT_EQ(it->second.file_content(), body2);
}

int main(int argc, char* argv[]) {
    testing::InitGoogleTest(&argc, argv);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("file.conf", "", true);
    blus::init_logger(FLAGS_log_file, static_cast<spdlog::level::level_enum>(FLAGS_log_level));

    // 构造Rpc信道管理对象
    auto sm = std::make_shared<blus::ServiceManager>();
    auto put_cb = [sm](const std::string& name, const std::string& ip) {
        sm->online(name, ip);
        };
    auto del_cb = [sm](const std::string& name, const std::string& ip) {
        sm->offline(name, ip);
        };
    sm->declare(FLAGS_file_service_name);

    // 构造服务发现对象
    auto dis = std::make_shared<blus::Discovery>(FLAGS_file_service_name, FLAGS_etcd_address, put_cb, del_cb);

    // 获取file服务的地址
    addr = sm->get(FLAGS_file_service_name);
    if (!addr) {
        std::cout << "获取服务地址失败" << std::endl;
        return 1;
    }

    return RUN_ALL_TESTS();
}
