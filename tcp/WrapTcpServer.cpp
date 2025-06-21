#include "WrapTcpServer.h"
#include "MessageCodec.h"
#include "ConnectionManager.h"
#include "MessageDispatcher.h"
#include <google/protobuf/message.h>
#include <zlib.h>
#include <functional>
#include <iomanip>  
#include <sstream> 

using namespace common;
using namespace common::net;

namespace {
    const int kHeaderLength = 32; // 头部固定长度
    const int kDefaultHeartbeatInterval = 30; // 默认心跳间隔30秒
    const int kHealthCheckInterval = 60; // 健康检查间隔60秒
}

// 添加hexDump辅助函数
static std::string hexDump(const void* data, size_t size) {
    std::ostringstream oss;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') 
            << static_cast<int>(p[i]) << " ";
    }
    return oss.str();
}

WrapTcpServer::WrapTcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const std::string& name,
                     uint32_t maxConnections,
                     uint32_t maxMessageSize)
    : loop_(loop),
      server_(loop, listenAddr, name),
      connectionManager_(new ConnectionManager(maxConnections)),
      messageDispatcher_(new MessageDispatcher()),
      maxMessageSize_(maxMessageSize),
      heartbeatInterval_(kDefaultHeartbeatInterval){
    
    // 注册消息处理器
    messageDispatcher_->registerHandler(0, 0, 
        std::bind(&WrapTcpServer::handleHeartbeat, this, _1, _2));
    messageDispatcher_->registerHandler(0, 1, 
        std::bind(&WrapTcpServer::handleLogin, this, _1, _2));
    messageDispatcher_->start();
    
    server_.setConnectionCallback(
        std::bind(&WrapTcpServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&WrapTcpServer::onMessage, this, _1, _2, _3));
    server_.setWriteCompleteCallback(
        std::bind(&WrapTcpServer::onWriteComplete, this, _1));
}

WrapTcpServer::~WrapTcpServer() {
    stop();
}

void WrapTcpServer::start() {
    LOG_INFO << "TCP server [" << server_.name() 
             << "] starts listening on " << server_.ipPort();
    
    // 启动健康检查定时器
    healthCheckTimer_ = loop_->runEvery(kHealthCheckInterval, 
        std::bind(&WrapTcpServer::checkConnectionsHealth, this));
    
    // 启动心跳定时器
    heartbeatTimer_ = loop_->runEvery(heartbeatInterval_, 
        std::bind(&WrapTcpServer::sendHeartbeats, this));
    
    server_.start();
}

void WrapTcpServer::stop() {
    if (messageDispatcher_) {
        messageDispatcher_->stop();
    }
    if (healthCheckTimer_.isValid()) {
        loop_->cancel(healthCheckTimer_);
    }
    if (heartbeatTimer_.isValid()) {
        loop_->cancel(heartbeatTimer_);
    }
    if (EventLoop* loop = server_.getLoop()) {
         loop->runInLoop([loop](){ 
            loop->quit();
        });
    }
}

void WrapTcpServer::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        if (!connectionManager_->addConnection(conn)) {
            LOG_WARN << "Max connections reached, rejecting " 
                     << conn->peerAddress().toIpPort();
            conn->shutdown();
            return;
        }
        
        LOG_INFO << "New connection: " << conn->peerAddress().toIpPort() << ", conn name: "<< conn->name()
                 << ", total connections: " << connectionManager_->connectionCount();
    } else {
        connectionManager_->removeConnection(conn);
        LOG_INFO << "Connection closed: " << conn->peerAddress().toIpPort()
                 << ", remaining connections: " << connectionManager_->connectionCount();
    }
}

void WrapTcpServer::onMessage(const TcpConnectionPtr& conn,
                          Buffer* buf,
                          Timestamp time) {
    connectionManager_->updateConnectionActivity(conn);
    
    while (buf->readableBytes() >= MessageCodec::kMinMessageLen + MessageCodec::kHeaderLen) {
        std::shared_ptr<google::protobuf::Message> message;
        int ret = MessageCodec::decode(buf, message);   
        if (0 != ret) {
            LOG_ERROR << "decode message fail connection will close ret: "<< ret;
            connectionManager_->removeConnection(conn);
            conn->shutdown();
            break;
        }

        auto netMessage = std::dynamic_pointer_cast<MessagePb::NetMessage>(message);
        if (netMessage) {
            messageDispatcher_->dispatch(conn, netMessage);
        }
        break;
    }
}

void WrapTcpServer::onWriteComplete(const TcpConnectionPtr& conn) {
    LOG_DEBUG << "Write complete: " << conn->name();
}

void WrapTcpServer::handleHeartbeat(const common::net::TcpConnectionPtr& conn, 
                                    const std::shared_ptr<MessagePb::NetMessage>& msg) {
    uint32_t sequence = msg->header().sequence();
    MessagePb::Heartbeat heartbeat;
    if (heartbeat.ParseFromString(msg->body().content())) {
        LOG_INFO << "Heartbeat received from " << conn->peerAddress().toIpPort() << ", connName: "<< conn->name()
                  << ", interval: " << heartbeat.ping_interval();
        
        // 更新心跳间隔
        if (heartbeat.ping_interval() > 0) {
            heartbeatInterval_ = heartbeat.ping_interval();
        }
        
        if (msg->header().flags() <= 0 ) {
            return;
        }

        // 回复心跳
        heartbeat.set_send_time(Timestamp::now().microSecondsSinceEpoch() / 1000);
        
        MessagePb::NetMessage reply;
        reply.mutable_header()->set_service_id(1);
        reply.mutable_header()->set_command_id(0);
        reply.mutable_header()->set_sequence(sequence);
        reply.mutable_header()->set_flags(0x0);
        reply.mutable_body()->set_content(heartbeat.SerializeAsString());
        
        common::net::Buffer buf;
        MessageCodec::encode(&buf, reply);
        conn->send(&buf);
    }
}

void WrapTcpServer::handleLogin(const common::net::TcpConnectionPtr& conn, 
                            const std::shared_ptr<MessagePb::NetMessage>& msg) {
    LOG_INFO << "WrapTcpServer::handleLogin from client msg:"<< msg->ShortDebugString();

    MessagePb::LoginResponse loginRsp;
    loginRsp.set_result_code(0);
    loginRsp.set_result_msg("test login");
    
    MessagePb::NetMessage reply;
    reply.mutable_header()->set_service_id(1);
    reply.mutable_header()->set_command_id(1);
    reply.mutable_header()->set_sequence(msg->header().sequence());
    reply.mutable_body()->set_content(loginRsp.SerializeAsString());
    
    common::net::Buffer buf;
    MessageCodec::encode(&buf, reply);
    conn->send(&buf);
}

void WrapTcpServer::checkConnectionsHealth() {
    LOG_INFO << "Performing health check on connections";
    connectionManager_->checkConnectionsHealth(heartbeatInterval_ * 3); // 允许错过3次心跳
}

void WrapTcpServer::sendHeartbeats() {
    MessagePb::Heartbeat heartbeat;
    heartbeat.set_send_time(Timestamp::now().microSecondsSinceEpoch() / 1000);
    heartbeat.set_ping_interval(heartbeatInterval_);
    
    MessagePb::NetMessage message;
    message.mutable_header()->set_service_id(1);
    message.mutable_header()->set_command_id(0);
    message.mutable_header()->set_flags(0x1); // 需要ACK
    message.mutable_header()->set_body_length(heartbeat.SerializeAsString().length());
    message.mutable_body()->set_content(heartbeat.SerializeAsString());
    
    common::net::Buffer buf;
    MessageCodec::encode(&buf, message);

    // 向所有连接发送心跳
    connectionManager_->sendToAll(&buf);
}