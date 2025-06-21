#include "WrapTcpClient.h"
#include <common/net/EventLoop.h>
#include <common/base/Thread.h>
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include "proto/message.pb.h"

using namespace std::placeholders;

class ClientApp {
public:
    ClientApp(const std::string& ip, uint16_t port)
        : loop_(),
        client_(&loop_, common::net::InetAddress(ip, port), "WrapTcpClient") {
    }
    
    void start() {
        client_.connect();
        loop_.loop();
    }

private:    
    common::net::EventLoop loop_;
    WrapTcpClient client_;
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <ip> <port>" << std::endl;
        return 1;
    }
    
    common::Logger::setLogLevel(common::Logger::INFO);
    
    try {
        ClientApp app(argv[1], static_cast<uint16_t>(atoi(argv[2])));
        app.start();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}