#include "WrapTcpServer.h"
#include <common/net/TcpServer.h>
#include <common/net/EventLoop.h>
#include <iostream>

class ServerApp {
public:
    ServerApp() 
        : loop_(),
          server_(&loop_, common::net::InetAddress(8888), "WrapTcpServer") {
        
        // 可以在这里注册更多的消息处理器
    }
    
    void start() {
        server_.start();
        loop_.loop();
    }

private:
    common::net::EventLoop loop_;
    WrapTcpServer server_;
};

int main() {
    common::Logger::setLogLevel(common::Logger::INFO);
    
    try {
        ServerApp app;
        app.start();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}