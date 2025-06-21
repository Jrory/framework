#ifndef WRAPTCPCLIENT_H
#define WRAPTCPCLIENT_H

#include <common/net/TcpClient.h>
#include <common/net/EventLoop.h>
#include <common/base/Logging.h>
#include <common/net/TcpConnection.h>
#include <common/base/Timestamp.h>
#include <boost/noncopyable.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <atomic>
#include "proto/message.pb.h"

class MessageDispatcher;

class WrapTcpClient : boost::noncopyable {
public:
    using MessageCallback = std::function<void(const std::shared_ptr<MessagePb::NetMessage>&)>;
    using ConnectionCallback = std::function<void(bool)>;
    
    WrapTcpClient(common::net::EventLoop* loop,
              const common::net::InetAddress& serverAddr,
              const std::string& name);
    
    ~WrapTcpClient();
    
    void connect();
    void disconnect();
    void reconnect();
    
    void send(uint32_t cmd, uint32_t srvId, const google::protobuf::Message& message, bool needAck = false);
    void sendHeartbeat();
    void sendLoginRequest();
    
    void setMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }
    
    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }
    
    bool isConnected() const {
        return connection_ && connection_->connected();
    }
    
    void setAutoReconnect(bool autoReconnect, int interval = 5) {
        autoReconnect_ = autoReconnect;
        reconnectInterval_ = interval;
    }

private:
    void onConnection(const common::net::TcpConnectionPtr& conn);
    void onMessage(const common::net::TcpConnectionPtr& conn,
                   common::net::Buffer* buf,
                   common::Timestamp time);
    void onWriteComplete(const common::net::TcpConnectionPtr& conn);
    
    void handleHeartbeat(const common::net::TcpConnectionPtr& conn, 
                        const std::shared_ptr<MessagePb::NetMessage>& heartbeat);
    void handleLogin(const common::net::TcpConnectionPtr& conn, 
                    const std::shared_ptr<MessagePb::NetMessage>& ack);
    
    void checkResendQueue();
    void resendMessage(const MessagePb::NetMessage& msg);
    
    common::net::EventLoop* loop_;
    common::net::TcpClient client_;
    common::net::TcpConnectionPtr connection_;
    
    MessageCallback messageCallback_;
    ConnectionCallback connectionCallback_;
    std::unique_ptr<MessageDispatcher> messageDispatcher_;

    // 心跳相关
    uint32_t heartbeatInterval_;
    common::net::TimerId heartbeatTimer_;
    
    // 重传相关
    std::mutex resendMutex_;
    std::queue<MessagePb::NetMessage> resendQueue_;
    common::net::TimerId resendTimer_;
    
    // 自动重连
    bool autoReconnect_;
    int reconnectInterval_;
    common::net::TimerId reconnectTimer_;
};

#endif // WRAPTCPCLIENT_H