#include "MessageDispatcher.h"
#include "ThreadPool.h"
#include "MessageBufferPool.h"
#include <common/base/Logging.h>
#include <iostream>
#include <typeinfo>

MessageDispatcher::MessageDispatcher(size_t threadPoolSize)
    : messageQueue_(1024),  // 环形缓冲区大小
      threadPool_(std::make_unique<ThreadPool>(threadPoolSize)),
      bufferPool_(std::make_unique<MessageBufferPool>()) {
    
    defaultHandler_ = [](const common::net::TcpConnectionPtr&, 
                        const std::shared_ptr<MessagePb::NetMessage>& msg) {
        LOG_WARN << "No handler registered for message - service: " 
                 << msg->header().service_id()
                 << ", command: " << msg->header().command_id();
    };
}

MessageDispatcher::~MessageDispatcher() {
    stop();
}

void MessageDispatcher::start() {
    running_ = true;
    threadPool_->start();
}

void MessageDispatcher::stop() {
    running_ = false;
    threadPool_->stop();
}

void MessageDispatcher::registerHandler(uint32_t serviceId, 
                                      uint32_t commandId, 
                                      MessageHandler handler) {
    handlers_.emplace(std::make_pair(serviceId, commandId), handler);
}

void MessageDispatcher::dispatch(const common::net::TcpConnectionPtr& conn, 
                               const std::shared_ptr<MessagePb::NetMessage>& msg) {
    if (!running_) return;
    
    // 使用对象池分配任务内存
    MessageTask task;
    task.conn = conn;
    task.msg = bufferPool_->allocate();
    task.msg->CopyFrom(*msg);
    
    // 非阻塞方式推入队列
    while (!messageQueue_.push(task)) {
        LOG_WARN << "Message queue full, waiting...";
        std::this_thread::yield();
    }
    
    // 提交处理任务到线程池
    threadPool_->enqueue([this]() {
        processMessages();
    });
}

void MessageDispatcher::processMessages() {
    MessageTask task;
    while (running_ && messageQueue_.pop(task)) {
        auto key = std::make_pair(task.msg->header().service_id(), 
            task.msg->header().command_id());

         if (handlers_.contains(key)) {
            handlers_.visit(key, [&](const auto& pair) {
                try {
                   pair.second(task.conn, task.msg);
                } catch (const std::exception& e) {
                    LOG_ERROR << "Exception in message handler: " << e.what();
                }
            });
        } else {
            LOG_INFO << "not find message register type";
            defaultHandler_(task.conn, task.msg);
        }
        
        // 归还消息对象到池
        bufferPool_->release(task.msg);
    }
}

void MessageDispatcher::setDefaultHandler(MessageHandler handler) {
    defaultHandler_ = handler;
}