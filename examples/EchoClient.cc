#include "../common/net/TcpClient.h"

#include "../common/base/AsyncLogging.h"
#include "../common/base/Logging.h"
#include "../common/base/Thread.h"
#include "../common/net/EventLoop.h"
#include "../common/net/InetAddress.h"

#include <utility>

#include <stdio.h>
#include <unistd.h>

using namespace common;
using namespace common::net;

#define LOG_PATH "./logs"

int numThreads = 0;
class EchoClient;
std::vector<std::unique_ptr<EchoClient>> clients;
int current = 0;

class EchoClient : noncopyable
{
 public:
  EchoClient(EventLoop* loop, const InetAddress& listenAddr, const string& id)
    : loop_(loop),
      client_(loop, listenAddr, "EchoClient"+id)
  {
    client_.setConnectionCallback(
        std::bind(&EchoClient::onConnection, this, _1));
    client_.setMessageCallback(
        std::bind(&EchoClient::onMessage, this, _1, _2, _3));
    //client_.enableRetry();
  }

  void connect()
  {
    client_.connect();
  }
  // void stop();

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_TRACE << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");

    if (conn->connected())
    {
      ++current;
      if (implicit_cast<size_t>(current) < clients.size())
      {
        clients[current]->connect();
      }
      LOG_INFO << "*** connected " << current;
    }
    conn->send("world\n");
  }

  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
  {
    string msg(buf->retrieveAllAsString());
    LOG_INFO << conn->name() << " recv " << msg.size() << " bytes at " << time.toString();
    if (msg == "quit\n")
    {
      conn->send("bye\n");
      conn->shutdown();
    }
    else if (msg == "shutdown\n")
    {
      loop_->quit();
    }
    else
    {
      conn->send(msg);
    }
  }

  EventLoop* loop_;
  TcpClient client_;
};

int main(int argc, char* argv[])
{
  if(chdir(LOG_PATH) != 0) {
    fprintf(stderr, "ERROR: Cannot switch to log dir %s\n", LOG_PATH);
    return 1;
  }
  
  static common::AsyncLogging asyncLog("EchoClient", 500 * 1024 * 1024);
  asyncLog.start();

  common::Logger::setOutput([](const char* msg, int len) {
      asyncLog.append(msg, len);
  });

  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  if (argc > 1)
  {
    EventLoop loop;
    bool ipv6 = argc > 3;
    InetAddress serverAddr(argv[1], 2000, ipv6);

    int n = 1;
    if (argc > 2)
    {
      n = atoi(argv[2]);
    }

    clients.reserve(n);
    for (int i = 0; i < n; ++i)
    {
      char buf[32];
      snprintf(buf, sizeof buf, "%d", i+1);
      clients.emplace_back(new EchoClient(&loop, serverAddr, buf));
    }

    clients[current]->connect();
    loop.loop();
  }
  else
  {
    printf("Usage: %s host_ip [current#]\n", argv[0]);
  }
}

