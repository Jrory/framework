
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.



#include "../Poller.h"
#include "PollPoller.h"
#include "EPollPoller.h"

#include <stdlib.h>

using namespace common::net;

Poller* Poller::newDefaultPoller(EventLoop* loop)
{
  if (::getenv("MCOMMON_USE_POLL"))
  {
    return new PollPoller(loop);
  }
  else
  {
    return new EPollPoller(loop);
  }
}
