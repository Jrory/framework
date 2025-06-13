
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.



#include "Poller.h"

#include "Channel.h"

using namespace common;
using namespace common::net;

Poller::Poller(EventLoop* loop)
  : ownerLoop_(loop)
{
}

Poller::~Poller() = default;

bool Poller::hasChannel(Channel* channel) const
{
  assertInLoopThread();
  ChannelMap::const_iterator it = channels_.find(channel->fd());
  return it != channels_.end() && it->second == channel;
}

