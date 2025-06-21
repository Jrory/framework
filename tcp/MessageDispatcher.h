#ifndef MESSAGE_DISPATCHER_H
#define MESSAGE_DISPATCHER_H

#include <common/net/TcpConnection.h>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include "proto/message.pb.h"

class ThreadPool;
class MessageBufferPool;

class MessageDispatcher {
public:
    using MessageHandler = std::function<void(const common::net::TcpConnectionPtr&, 
                                           const std::shared_ptr<MessagePb::NetMessage>&)>;
    
    MessageDispatcher(size_t threadPoolSize = std::thread::hardware_concurrency());
    ~MessageDispatcher();
    
    void registerHandler(uint32_t serviceId, uint32_t commandId, MessageHandler handler);
    void dispatch(const common::net::TcpConnectionPtr& conn, 
                 const std::shared_ptr<MessagePb::NetMessage>& msg);
    
    void setDefaultHandler(MessageHandler handler);
    void start();
    void stop();

private:
    void processMessages();
    
    using HandlerKey = std::pair<uint32_t, uint32_t>;
    struct HandlerKeyHash {
        size_t operator()(const HandlerKey& key) const {
            return (static_cast<size_t>(key.first) << 32) | key.second;
        }
    };
    
    boost::concurrent_flat_map<HandlerKey, MessageHandler, HandlerKeyHash> handlers_;
    MessageHandler defaultHandler_;
    
    struct MessageTask {
        common::net::TcpConnectionPtr conn;
        std::shared_ptr<MessagePb::NetMessage> msg;
    };
    
    boost::lockfree::spsc_queue<MessageTask> messageQueue_;
    std::unique_ptr<ThreadPool> threadPool_;
    std::unique_ptr<MessageBufferPool> bufferPool_;
    std::atomic<bool> running_{false};
};

#endif // MESSAGE_DISPATCHER_H