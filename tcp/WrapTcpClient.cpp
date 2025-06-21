#include "WrapTcpClient.h"
#include "MessageCodec.h"
#include "MessageDispatcher.h"
#include <google/protobuf/message.h>
#include <zlib.h>
#include <functional>

using namespace common;
using namespace common::net;

namespace {
    const int kHeaderLength = 32; // 头部固定长度
    const int kDefaultHeartbeatInterval = 30; // 默认心跳间隔30秒
    const int kResendCheckInterval = 1; // 重传检查间隔1秒
}

WrapTcpClient::WrapTcpClient(EventLoop* loop,
                     const InetAddress& serverAddr,
                     const std::string& name)
    : loop_(loop),
      client_(loop, serverAddr, name),
      heartbeatInterval_(kDefaultHeartbeatInterval),
      messageDispatcher_(new MessageDispatcher()),
      autoReconnect_(false),
      reconnectInterval_(5) {
    
    // 注册消息处理器
    messageDispatcher_->registerHandler(1, 0, 
        std::bind(&WrapTcpClient::handleHeartbeat, this, _1, _2));
    messageDispatcher_->registerHandler(1, 1, 
        std::bind(&WrapTcpClient::handleLogin, this, _1, _2));
    messageDispatcher_->start();

    client_.setConnectionCallback(
        std::bind(&WrapTcpClient::onConnection, this, _1));
    client_.setMessageCallback(
        std::bind(&WrapTcpClient::onMessage, this, _1, _2, _3));
    client_.setWriteCompleteCallback(
        std::bind(&WrapTcpClient::onWriteComplete, this, _1));
}

WrapTcpClient::~WrapTcpClient() {
    disconnect();
}

void WrapTcpClient::connect() {
    client_.connect();
}

void WrapTcpClient::disconnect() {
    if (heartbeatTimer_.isValid()) {
        loop_->cancel(heartbeatTimer_);
    }
    if (resendTimer_.isValid()) {
        loop_->cancel(resendTimer_);
    }
    if (reconnectTimer_.isValid()) {
        loop_->cancel(reconnectTimer_);
    }
    if (messageDispatcher_) {
        messageDispatcher_->stop();
    }
    if (connection_) {
        connection_->shutdown();
    }
}

void WrapTcpClient::reconnect() {
    if (!autoReconnect_) return;
    
    LOG_INFO << "Will reconnect to server in " << reconnectInterval_ << " seconds";
    reconnectTimer_ = loop_->runAfter(reconnectInterval_, 
        std::bind(&WrapTcpClient::connect, this));
}

void WrapTcpClient::send(uint32_t cmd, uint32_t srvId, const google::protobuf::Message& message, bool needAck) {
    if (!connection_ || !connection_->connected()) {
        LOG_WARN << "Connection is not established, message not sent";
        return;
    }

    MessagePb::NetMessage msgToSend;
    msgToSend.mutable_body()->set_content(message.SerializeAsString());

    msgToSend.mutable_header()->set_service_id(srvId); // 认证服务
    msgToSend.mutable_header()->set_command_id(cmd); // 登录命令
    msgToSend.mutable_header()->set_flags(needAck ? 0x1 : 0x0);    // 需要ACK
    msgToSend.mutable_header()->set_body_length(msgToSend.body().SerializeAsString().size());

    common::net::Buffer buf;
    MessageCodec::encode(&buf, msgToSend);
    connection_->send(&buf);
    
}

void WrapTcpClient::sendHeartbeat() {
    if (!isConnected()) return;
    
    MessagePb::Heartbeat heartbeat;
    heartbeat.set_send_time(Timestamp::now().microSecondsSinceEpoch() / 1000);
    heartbeat.set_ping_interval(heartbeatInterval_);
        
    send(0, 0, heartbeat, true);
}

void WrapTcpClient::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        connection_ = conn;
        LOG_INFO << "Connected to server " << conn->peerAddress().toIpPort();
        
        // 启动心跳定时器
        heartbeatTimer_ = loop_->runEvery(heartbeatInterval_, 
            std::bind(&WrapTcpClient::sendHeartbeat, this));
        
        // 启动重传检查定时器
        resendTimer_ = loop_->runEvery(kResendCheckInterval, 
            std::bind(&WrapTcpClient::checkResendQueue, this));

        sendLoginRequest();

    } else {
        LOG_INFO << "Disconnected from server";
        connection_.reset();
        
        if (heartbeatTimer_.isValid()) {
            loop_->cancel(heartbeatTimer_);
        }
        if (resendTimer_.isValid()) {
            loop_->cancel(resendTimer_);
        }
        reconnect();
    }
}

void WrapTcpClient::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) {
    while (buf->readableBytes() >= MessageCodec::kMinMessageLen + MessageCodec::kHeaderLen) {
        std::shared_ptr<google::protobuf::Message> message;
        int ret = MessageCodec::decode(buf, message);   
        if (0 != ret) {
            LOG_ERROR << "decode message fail connection will close ret: "<< ret;
            connection_->shutdown();
            break;
        }

        auto netMessage = std::dynamic_pointer_cast<MessagePb::NetMessage>(message);
        if (netMessage) {
            messageDispatcher_->dispatch(conn, netMessage);
        }
        break;
    }
}

void WrapTcpClient::onWriteComplete(const TcpConnectionPtr& conn) {
    LOG_DEBUG << "Write complete: " << conn->name();
}

void WrapTcpClient::handleHeartbeat(const common::net::TcpConnectionPtr& conn, const std::shared_ptr<MessagePb::NetMessage>& msg) {
    MessagePb::Heartbeat heartbeat;
    if (heartbeat.ParseFromString(msg->body().content())) {
        LOG_INFO << "Heartbeat received, interval: " << heartbeat.ping_interval();
        
        // 更新心跳间隔
        if (heartbeat.ping_interval() > 0 && heartbeatInterval_ != heartbeat.ping_interval() ) {
            heartbeatInterval_ = heartbeat.ping_interval();
            loop_->cancel(heartbeatTimer_);
            heartbeatTimer_ = loop_->runEvery(heartbeatInterval_, 
                std::bind(&WrapTcpClient::sendHeartbeat, this));
        }
        
        if (msg->header().flags() <= 0 ) {
            return;
        }
        
        // 回复心跳
        heartbeat.set_send_time(Timestamp::now().microSecondsSinceEpoch() / 1000);
        send(0, 0, heartbeat);
    }
}

void WrapTcpClient::handleLogin(const common::net::TcpConnectionPtr& conn, const std::shared_ptr<MessagePb::NetMessage>& msg) {
    LOG_INFO << "WrapTcpClient::handleLogin from server msg:"<< msg->ShortDebugString();
}

void WrapTcpClient::checkResendQueue() {
    // std::lock_guard<std::mutex> lock(resendMutex_);
    // if (!resendQueue_.empty()) {
    //     const auto& msg = resendQueue_.front();
    //     if (Timestamp::now().microSecondsSinceEpoch() / 1000000 - 
    //         msg.header().timestamp() / 1000 > 3) { // 超过3秒未收到ACK
    //         LOG_WARN << "Resending message with sequence " << msg.header().sequence();
    //         resendMessage(msg);
    //     }
    // }
}

void WrapTcpClient::resendMessage(const MessagePb::NetMessage& msg) {
    // if (!isConnected()) return;
    
    // std::string data;
    // msg.SerializeToString(&data);
    // connection_->send(data);
}

void WrapTcpClient::sendLoginRequest() {
    MessagePb::LoginRequest request;
    request.set_username("admin");
    request.set_password("123456");
    request.set_client_version("1.0.0");
    request.set_device_id("device_001");
    send(1, 0, request, true);
}