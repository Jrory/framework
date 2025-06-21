#include "ConnectionManager.h"
#include <common/base/Logging.h>
#include <algorithm>

using namespace common;
using namespace common::net;

ConnectionManager::ConnectionManager(uint32_t maxConnections)
    : maxConnections_(maxConnections) {
}

bool ConnectionManager::addConnection(const TcpConnectionPtr& conn, uint32_t groupId) {
    if (currentConnections_.load() >= maxConnections_) {
        return false;
    }
    
    auto info = std::make_shared<ConnectionInfo>();
    info->conn = conn;
    info->lastActiveTime = Timestamp::now();
    info->groupId = groupId;
    
    if (connections_.emplace(conn->name(), info)) {
        currentConnections_++;
        
        if (groupId != 0) {
            std::lock_guard<std::mutex> lock(groupMutex_);
            groupConnections_[groupId].push_back(conn->name());
        }
        return true;
    }
    return false;
}

void ConnectionManager::removeConnection(const TcpConnectionPtr& conn) {
    if (connections_.erase(conn->name())) {
        currentConnections_--;
        
        auto info = getConnectionInfo(conn->name());
        if (info && info->groupId != 0) {
            std::lock_guard<std::mutex> lock(groupMutex_);
            auto& group = groupConnections_[info->groupId];
            group.erase(std::remove(group.begin(), group.end(), conn->name()), group.end());
        }
    }
}

void ConnectionManager::updateConnectionActivity(const TcpConnectionPtr& conn) {
    auto info = getConnectionInfo(conn->name());
    if (info) {
        info->lastActiveTime = Timestamp::now();
    }
}

void ConnectionManager::authenticateConnection(const TcpConnectionPtr& conn, 
                                             const std::string& sessionId) {
    if (auto info = getConnectionInfo(conn->name())) {
        info->authenticated = true;
        info->sessionId = sessionId;
    }
}

size_t ConnectionManager::connectionCount() const {
    return currentConnections_.load();
}

size_t ConnectionManager::groupConnectionCount(uint32_t groupId) const {
    std::lock_guard<std::mutex> lock(groupMutex_);
    auto it = groupConnections_.find(groupId);
    return it != groupConnections_.end() ? it->second.size() : 0;
}

void ConnectionManager::checkConnectionsHealth(int timeoutSeconds) {
    Timestamp now = Timestamp::now();
    
    connections_.cvisit_all([&](const auto& pair) {
        double elapsed = timeDifference(now, pair.second->lastActiveTime);
        if (elapsed > timeoutSeconds) {
            LOG_WARN << "Closing inactive connection: " << pair.first
                     << ", last active " << elapsed << " seconds ago";
            pair.second->conn->shutdown();
        }
    });
}

void ConnectionManager::closeAllConnections() {
    connections_.cvisit_all([](const auto& pair) {
        pair.second->conn->shutdown();
    });
}

void ConnectionManager::closeGroupConnections(uint32_t groupId) {
    std::vector<std::string> toClose;
    {
        std::lock_guard<std::mutex> lock(groupMutex_);
        auto it = groupConnections_.find(groupId);
        if (it != groupConnections_.end()) {
            toClose = it->second;
        }
    }
    
    for (const auto& connName : toClose) {
        if (auto info = getConnectionInfo(connName)) {
            info->conn->shutdown();
        }
    }
}

void ConnectionManager::sendToAll(common::net::Buffer* buf) {
    connections_.cvisit_all([&](const auto& pair) {
        if (pair.second->conn->connected()) {
            pair.second->conn->send(buf);
        }
    });
}

void ConnectionManager::sendToGroup(uint32_t groupId, const std::string& data) {
    std::vector<std::string> groupConns;
    {
        std::lock_guard<std::mutex> lock(groupMutex_);
        auto it = groupConnections_.find(groupId);
        if (it != groupConnections_.end()) {
            groupConns = it->second;
        }
    }
    
    for (const auto& connName : groupConns) {
        if (auto info = getConnectionInfo(connName)) {
            if (info->conn->connected()) {
                info->conn->send(data);
            }
        }
    }
}

std::shared_ptr<ConnectionManager::ConnectionInfo> 
ConnectionManager::getConnectionInfo(const std::string& connName) {
    std::shared_ptr<ConnectionInfo> result;
    if (connections_.contains(connName)) {
        connections_.visit(connName, [&](const auto& pair) {
            result = pair.second;
        });
    }
    return result;
}