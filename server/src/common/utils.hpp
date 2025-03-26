#pragma once
#include <iostream>
#include <random>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>

#include "logger.hpp"

namespace blus {
    std::string uuid() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> distrib(0, 255);

        std::stringstream ss;
        for (int i = 0; i < 6; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << distrib(gen);
        }

        static std::atomic<short> idx = 0;
        short tmp = idx.fetch_add(1);
        ss << std::hex << std::setw(4) << std::setfill('0') << tmp;

        return ss.str();
    }

    bool readFile(const std::filesystem::path& filepath, std::string& body) {
        std::ifstream ifs(filepath, std::ios::in | std::ios::binary);
        if (!ifs) {
            LOG_ERROR("打开文件失败: {}", filepath.string());
            ifs.close();
            return false;
        }

        ifs.seekg(0, std::ios::end);
        size_t size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        body.resize(size);
        ifs.read(&body[0], size);
        if (!ifs.good()) {
            LOG_ERROR("读取文件失败: {}", filepath.string());
            ifs.close();
            return false;
        }
        ifs.close();
        return true;
    }

    bool writeFile(const std::filesystem::path& filepath, const std::string& body) {
        if (!std::filesystem::exists(filepath.parent_path())) {
            std::filesystem::create_directories(filepath.parent_path());
        }
        std::ofstream ofs(filepath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!ofs) {
            LOG_ERROR("打开文件失败: {}", filepath.string());
            ofs.close();
            return false;
        }

        ofs.write(body.data(), body.size());
        if (!ofs.good()) {
            LOG_ERROR("写入文件失败: {}", filepath.string());
            ofs.close();
            return false;
        }
        ofs.close();
        return true;
    }
} // namespace blus
