#include "llm.hpp"
#include <iostream>

int main() {
    blus::Asr asr("localhost", 5000, "asr");
    std::cout << asr.recognize(R"(/home/lwj/project/BreezeChat/server/asr/zh.wav)") << std::endl;

    return 0;
}