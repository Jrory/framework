// MessageBufferPool.h
#ifndef MESSAGE_BUFFER_POOL_H
#define MESSAGE_BUFFER_POOL_H

#include <memory>
#include <vector>
#include <mutex>
#include "proto/message.pb.h"

class MessageBufferPool {
public:
    MessageBufferPool(size_t initialSize = 1024);
    
    std::shared_ptr<MessagePb::NetMessage> allocate();
    void release(const std::shared_ptr<MessagePb::NetMessage>& msg);
    
    size_t capacity() const;
    size_t available() const;

private:
    struct MessageDeleter {
        MessageBufferPool* pool;
        
        void operator()(MessagePb::NetMessage* msg) {
            pool->releaseToPool(msg);
        }
    };
    
    void releaseToPool(MessagePb::NetMessage* msg);
    MessagePb::NetMessage* createMessage();
    
    mutable std::mutex mutex_;
    std::vector<MessagePb::NetMessage*> pool_;
    size_t initialSize_;
    size_t created_{0};
};

#endif // MESSAGE_BUFFER_POOL_H
