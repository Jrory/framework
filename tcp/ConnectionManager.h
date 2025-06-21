#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <common/net/TcpConnection.h>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include "proto/message.pb.h"

class ConnectionManager {
public:
    struct ConnectionInfo {
        common::net::TcpConnectionPtr conn;
        common::Timestamp lastActiveTime;
        std::atomic<uint32_t> unackCount{0};
        std::atomic<bool> authenticated{false};
        std::string sessionId;
        uint32_t groupId{0};  // 连接分组ID
    };

    using ConnectionMap = boost::concurrent_flat_map<std::string, std::shared_ptr<ConnectionInfo>>;
    
    explicit ConnectionManager(uint32_t maxConnections);
    
    bool addConnection(const common::net::TcpConnectionPtr& conn, uint32_t groupId = 0);
    void removeConnection(const common::net::TcpConnectionPtr& conn);
    void updateConnectionActivity(const common::net::TcpConnectionPtr& conn);
    void authenticateConnection(const common::net::TcpConnectionPtr& conn, 
                              const std::string& sessionId);
    
    size_t connectionCount() const;
    size_t groupConnectionCount(uint32_t groupId) const;
    
    void checkConnectionsHealth(int timeoutSeconds);
    void closeAllConnections();
    void closeGroupConnections(uint32_t groupId);
    
    void sendToAll(common::net::Buffer* buf);
    void sendToGroup(uint32_t groupId, const std::string& data);
    
    std::shared_ptr<ConnectionInfo> getConnectionInfo(const std::string& connName);

private:
    mutable std::mutex groupMutex_;
    std::unordered_map<uint32_t, std::vector<std::string>> groupConnections_;
    ConnectionMap connections_;
    std::atomic<uint32_t> currentConnections_{0};
    uint32_t maxConnections_;
};

#endif // CONNECTION_MANAGER_H