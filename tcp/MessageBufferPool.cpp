// MessageBufferPool.cpp
#include "MessageBufferPool.h"

MessageBufferPool::MessageBufferPool(size_t initialSize)
    : initialSize_(initialSize) {
    pool_.reserve(initialSize);
    for (size_t i = 0; i < initialSize; ++i) {
        pool_.push_back(createMessage());
    }
}

std::shared_ptr<MessagePb::NetMessage> MessageBufferPool::allocate() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (pool_.empty()) {
        for (size_t i = 0; i < initialSize_ / 2; ++i) {
            pool_.push_back(createMessage());
        }
    }
    
    MessagePb::NetMessage* msg = pool_.back();
    pool_.pop_back();
    
    return std::shared_ptr<MessagePb::NetMessage>(msg, MessageDeleter{this});
}

void MessageBufferPool::release(const std::shared_ptr<MessagePb::NetMessage>& msg) {
    // 通过shared_ptr的deleter自动处理
}

void MessageBufferPool::releaseToPool(MessagePb::NetMessage* msg) {
    std::unique_lock<std::mutex> lock(mutex_);
    msg->Clear();
    pool_.push_back(msg);
}

MessagePb::NetMessage* MessageBufferPool::createMessage() {
    created_++;
    return new MessagePb::NetMessage();
}

size_t MessageBufferPool::capacity() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return pool_.capacity();
}

size_t MessageBufferPool::available() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return pool_.size();
}