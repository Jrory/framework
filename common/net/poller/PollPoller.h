
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.


//
// This is an internal header file, you should not include this.

#ifndef MCOMMON_NET_POLLER_POLLPOLLER_H
#define MCOMMON_NET_POLLER_POLLPOLLER_H

#include "../Poller.h"

#include <vector>

struct pollfd;

namespace common
{
namespace net
{

///
/// IO Multiplexing with poll(2).
///
class PollPoller : public Poller
{
 public:

  PollPoller(EventLoop* loop);
  ~PollPoller() override;

  Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
  void updateChannel(Channel* channel) override;
  void removeChannel(Channel* channel) override;

 private:
  void fillActiveChannels(int numEvents,
                          ChannelList* activeChannels) const;

  typedef std::vector<struct pollfd> PollFdList;
  PollFdList pollfds_;
};

}  // namespace net
}  // namespace common
#endif  // MCOMMON_NET_POLLER_POLLPOLLER_H
