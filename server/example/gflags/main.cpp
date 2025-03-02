#include <gflags/gflags.h>
#include <iostream>

DEFINE_int32(port, 8080, "port to listen");

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    gflags::ReadFromFlagsFile("config.conf", "", true);
    std::cout << "port: " << FLAGS_port << std::endl;

    return 0;
}