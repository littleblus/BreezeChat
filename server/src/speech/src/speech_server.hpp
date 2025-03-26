#pragma once
#include <brpc/server.h>
#include <butil/logging.h>
#include <filesystem>

#include "asr.hpp"
#include "etcd.hpp" // 服务注册
#include "logger.hpp"
#include "speech.pb.h" // protobuf

namespace blus {
    class SpeechServiceImpl : public SpeechService {
    public:
        SpeechServiceImpl(const Asr::Ptr& asr) : _asr(asr) {}
        ~SpeechServiceImpl() {}

        void SpeechRecognition(google::protobuf::RpcController* controller,
            const SpeechRecognitionReq* request,
            SpeechRecognitionRsp* response,
            google::protobuf::Closure* done) override {
            brpc::ClosureGuard rpc_guard(done);
            std::string data = request->speech_content();
            // 保存请求中的音频数据到文件
            std::filesystem::path tmpDir = std::filesystem::absolute("tmp");
            if (!std::filesystem::exists(tmpDir)) {
                std::filesystem::create_directories(tmpDir);
            }
            std::filesystem::path audio_path = tmpDir / (std::to_string(time(nullptr)) + ".wav");
            std::ofstream ofs(audio_path, std::ios::binary);
            ofs.write(data.data(), data.size());
            ofs.close();
            // 识别音频文件
            response->set_request_id(request->request_id());
            std::string text;
            try {
                text = _asr->recognize(audio_path);
            }
            catch (const std::exception& e) {
                LOG_ERROR("{}识别音频文件失败: {}", request->request_id(), e.what());
                response->set_success(false);
                response->set_errmsg(e.what());
                // 不删除音频文件，方便调试
                return;
            }
            // 删除音频文件
            std::remove(audio_path.c_str());
            // 返回识别结果
            response->set_success(true);
            response->set_recognition_result(text);
        }
    private:
        Asr::Ptr _asr;
    };

    class SpeechServer {
    public:
        using Ptr = std::shared_ptr<SpeechServer>;
        SpeechServer(const Asr::Ptr& asr, const Registry::Ptr& reg, const std::shared_ptr<brpc::Server>& server)
            : _asr(asr), _reg(reg), _server(server) {
        }

        void start() {
            _server->RunUntilAskedToQuit();
        }

        ~SpeechServer() {
        }

    private:
        Asr::Ptr _asr;
        Registry::Ptr _reg;
        std::shared_ptr<brpc::Server> _server;
    };

    class SpeechServerBuilder {
    public:
        // 设置asr服务
        bool make_asr(const std::string& asr_server_ip, int32_t asr_server_port, const std::string& asr_server_name) {
            _asr = make_shared<Asr>(asr_server_ip, asr_server_port, asr_server_name);
            return true;
        }

        // 设置etcd服务
        bool make_etcd(const std::string& etcd_addr,
            const std::string& service_name,
            const std::string& instance_name,
            const std::string& service_ip,
            int32_t service_port,
            int etcd_timeout) {
            _reg = make_shared<Registry>(service_name, etcd_addr, etcd_timeout);
            _reg->registry(instance_name, service_ip + ":" + to_string(service_port));
            return true;
        }

        // 设置rpc服务
        bool make_rpc(int32_t listen_port, uint8_t thread_num, int rpc_timeout) {
            _server = make_shared<brpc::Server>();

            if (!_asr) {
                LOG_ERROR("asr服务未设置");
                return false;
            }
            auto service = new SpeechServiceImpl(_asr);
            int ret = _server->AddService(service, brpc::SERVER_OWNS_SERVICE);
            if (ret != 0) {
                LOG_ERROR("SpeechServer添加服务失败");
                return false;
            }

            brpc::ServerOptions options;
            options.idle_timeout_sec = rpc_timeout;
            options.num_threads = thread_num;

            ret = _server->Start(listen_port, &options);
            if (ret != 0) {
                LOG_ERROR("SpeechServer启动失败");
                return false;
            }
            return true;
        }

        SpeechServer::Ptr build() {
            if (!_asr) {
                LOG_ERROR("asr服务未设置");
                return nullptr;
            }
            if (!_reg) {
                LOG_ERROR("etcd服务未设置");
                return nullptr;
            }
            if (!_server) {
                LOG_ERROR("rpc服务未设置");
                return nullptr;
            }
            return make_shared<SpeechServer>(_asr, _reg, _server);
        }
    private:
        Asr::Ptr _asr;
        Registry::Ptr _reg;
        std::shared_ptr<brpc::Server> _server;
    };
}