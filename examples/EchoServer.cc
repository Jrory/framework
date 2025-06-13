#include "../common/net/TcpServer.h"

#include "../common/base/AsyncLogging.h"
#include "../common/base/Logging.h"
#include "../common/base/Thread.h"
#include "../common/net/EventLoop.h"
#include "../common/net/InetAddress.h"

#include <utility>

#include <stdio.h>
#include <unistd.h>

#define LOG_PATH "./logs"

using namespace common;
using namespace common::net;

int numThreads = 0;

class EchoServer
{
 public:
  EchoServer(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      server_(loop, listenAddr, "EchoServer")
  {
    server_.setConnectionCallback(
        std::bind(&EchoServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&EchoServer::onMessage, this, _1, _2, _3));
    server_.setThreadNum(numThreads);
  }

  void start()
  {
    server_.start();
  }
  // void stop();

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_TRACE << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
    LOG_INFO << conn->getTcpInfoString();

    conn->send("hello\n");
  }

  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
  {
    string msg(buf->retrieveAllAsString());
    LOG_INFO << conn->name() << " recv " << msg.size() << " bytes at " << time.toString();
    if (msg == "exit\n")
    {
      conn->send("bye\n");
      conn->shutdown();
    }
    if (msg == "quit\n")
    {
      loop_->quit();
    }
    conn->send(msg);
  }

  EventLoop* loop_;
  TcpServer server_;
};

int main(int argc, char* argv[])
{
  if(chdir(LOG_PATH) != 0) {
    fprintf(stderr, "ERROR: Cannot switch to log dir %s\n", LOG_PATH);
    return 1;
  }
  
  static common::AsyncLogging asyncLog("EchoServer", 500 * 1024 * 1024);
  asyncLog.start();

  common::Logger::setOutput([](const char* msg, int len) {
      asyncLog.append(msg, len);
  });

  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  LOG_INFO << "sizeof TcpConnection = " << sizeof(TcpConnection);
  if (argc > 1)
  {
    numThreads = atoi(argv[1]);
  }
  bool ipv6 = argc > 2;
  EventLoop loop;
  InetAddress listenAddr(2000, false, ipv6);
  EchoServer server(&loop, listenAddr);

  server.start();

  loop.loop();
}

