#ifndef WRAPTCPSERVER_H
#define WRAPTCPSERVER_H

#include <common/net/TcpServer.h>
#include <common/net/EventLoop.h>
#include <common/base/Logging.h>
#include <common/net/TcpConnection.h>
#include <common/base/Timestamp.h>
#include <boost/noncopyable.hpp>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include "proto/message.pb.h"

class ConnectionHandler;
class MessageDispatcher;
class ConnectionManager;

class WrapTcpServer : boost::noncopyable {
public:
    WrapTcpServer(common::net::EventLoop* loop,
              const common::net::InetAddress& listenAddr,
              const std::string& name,
              uint32_t maxConnections = 10000,
              uint32_t maxMessageSize = 64 * 1024 * 1024);

    ~WrapTcpServer();

    void start();
    void stop();

private:
    friend class ConnectionHandler;
    
    void onConnection(const common::net::TcpConnectionPtr& conn);
    void onMessage(const common::net::TcpConnectionPtr& conn,
                   common::net::Buffer* buf,
                   common::Timestamp time);
    void onWriteComplete(const common::net::TcpConnectionPtr& conn);
    
    void handleHeartbeat(const common::net::TcpConnectionPtr& conn, 
                        const std::shared_ptr<MessagePb::NetMessage>& heartbeat);
    void handleLogin(const common::net::TcpConnectionPtr& conn, 
                    const std::shared_ptr<MessagePb::NetMessage>& ack);

    void checkConnectionsHealth();
    void sendHeartbeats();

    common::net::EventLoop* loop_;
    common::net::TcpServer server_;
    
    std::unique_ptr<ConnectionManager> connectionManager_;
    std::unique_ptr<MessageDispatcher> messageDispatcher_;
    
    uint32_t maxMessageSize_;
    uint32_t heartbeatInterval_; // 心跳间隔(秒)
    common::net::TimerId healthCheckTimer_;
    common::net::TimerId heartbeatTimer_;
    
};

#endif // WRAPTCPSERVER_H